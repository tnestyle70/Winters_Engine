#include "Network/Backend/CHttpClient.h"
#include <winhttp.h>
#include <future>
#include <chrono>

#pragma comment(lib, "winhttp.lib")

//string -> wstring
static wstring ToWide(const string& str)
{
	if (str.empty())
		return {};

	int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
	wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &result[0], size);
	return result;
}

//Factory Pattern
unique_ptr<CHttpClient> CHttpClient::Create(const string& baseURL)
{
	auto pInstance = unique_ptr<CHttpClient>(new CHttpClient());
	pInstance->m_BaseURL = baseURL;
	pInstance->ParseURL(
		baseURL,
		pInstance->m_Host,
		pInstance->m_Port,
		pInstance->m_BasePath,
		pInstance->m_bSecure);

	return pInstance;
}

//URL Parsing http://localhost:8081 -> host, port, basepath
void CHttpClient::ParseURL(
	const string& url,
	wstring& host,
	uint16_t& port,
	wstring& basePath,
	bool_t& secure)
{
	string temp = url;
	secure = temp.rfind("https://", 0u) == 0u;
	//프로토콜 제거
	size_t protoEnd = temp.find("://");
	//npos -> size_t의 최대값을 의미
	if (protoEnd != string::npos)
		temp = temp.substr(protoEnd + 3);
	//basePath 분리 - localhost: <- 이 부분!!
	size_t pathStart = temp.find('/');
	string hostPort;
	// / 위치를 찾았을 경우 
	if (pathStart != string::npos)
	{
		basePath = ToWide(temp.substr(pathStart));
		hostPort = temp.substr(0, pathStart);
	}
	else
	{
		basePath = L"";
		hostPort = temp;
	}
	//host:port 분리
	size_t colonPos = hostPort.find(':');
	if (colonPos != string::npos)
	{
		host = ToWide(hostPort.substr(0, colonPos));
		port = (uint16_t)stoi(hostPort.substr(colonPos + 1));
	}
	else
	{
		host = ToWide(hostPort);
		//포트가 없을 경우 기본값 80으로 세팅!
		port = secure ? 443 : 80;
	}
}


void CHttpClient::SetAuthToken(const string& token)
{
	m_AuthToken = token;
}

HttpResponse CHttpClient::Get(const string & path)
{
	return DoRequest("GET", path, "");
}

HttpResponse CHttpClient::Post(const string& path, const string& jsonBody)
{
	return DoRequest("POST", path, jsonBody);
}

HttpResponse CHttpClient::Delete(const string& path)
{
	return DoRequest("DELETE", path, "");
}

//비동기 요청
//과거 버그: async(launch::async, ...)의 반환 future를 버리면 임시 future 소멸자가
//작업 완료까지 대기해 사실상 동기 호출이 됐다 (gotcha 2026-07-09 async lifetime).
//지금은 m_PendingRequests가 future를 소유하고, 소멸자가 전부 드레인한다.
void CHttpClient::AsyncGet(const string& path, HttpCallback callback)
{
	LaunchAsyncRequest("GET", path, "", callback);
}

void CHttpClient::AsyncPost(const string & path, const string & jsonBody, HttpCallback callback)
{
	LaunchAsyncRequest("POST", path, jsonBody, callback);
}

void CHttpClient::AsyncDelete(const string& path, HttpCallback callback)
{
	LaunchAsyncRequest("DELETE", path, "", callback);
}

CHttpClient::~CHttpClient()
{
	//진행 중인 요청 lambda가 this(m_CallbackMutex/m_PendingCallbacks)를 만지므로
	//전부 끝날 때까지 대기한다. 블로킹은 파괴 시점에만 발생한다.
	vector<future<void>> pending;
	{
		lock_guard<mutex> lock(m_RequestMutex);
		pending.swap(m_PendingRequests);
	}
	for (auto& task : pending)
	{
		if (task.valid())
			task.wait();
	}
}

CHttpClient::RequestSnapshot CHttpClient::MakeRequestSnapshot() const
{
	RequestSnapshot snapshot;
	snapshot.host = m_Host;
	snapshot.port = m_Port;
	snapshot.basePath = m_BasePath;
	snapshot.authToken = m_AuthToken;
	snapshot.secure = m_bSecure;
	return snapshot;
}

void CHttpClient::PruneCompletedRequests()
{
	lock_guard<mutex> lock(m_RequestMutex);
	for (size_t i = m_PendingRequests.size(); i > 0; --i)
	{
		auto& task = m_PendingRequests[i - 1];
		if (!task.valid() ||
			task.wait_for(chrono::seconds(0)) == future_status::ready)
		{
			m_PendingRequests.erase(m_PendingRequests.begin() + (i - 1));
		}
	}
}

void CHttpClient::LaunchAsyncRequest(string method, string path, string body, HttpCallback callback)
{
	PruneCompletedRequests();

	const RequestSnapshot snapshot = MakeRequestSnapshot();
	future<void> task = async(launch::async,
		[this, snapshot, method, path, body, callback]() {
			HttpResponse resp = DoRequestWith(snapshot, method, path, body);
			lock_guard<mutex> lock(m_CallbackMutex);
			m_PendingCallbacks.push([callback, resp]() { callback(resp); });
		});

	lock_guard<mutex> lock(m_RequestMutex);
	m_PendingRequests.push_back(std::move(task));
}
//메인 스레드에서 콜백 실행
void CHttpClient::ProcessCallbacks()
{
	queue<function<void()>> pending;
	{
		lock_guard<mutex> lock(m_CallbackMutex);
		swap(pending, m_PendingCallbacks);
	}
	while (!pending.empty())
	{
		pending.front()();
		pending.pop();
	}
}
HttpResponse CHttpClient::DoRequest(const string & method, const string & path, const string & body)
{
	return DoRequestWith(MakeRequestSnapshot(), method, path, body);
}

//핵심 : WinHTTP 요청! worker 스레드에서 실행되므로 멤버 대신 snapshot 복사본만 읽는다(static으로 강제).
HttpResponse CHttpClient::DoRequestWith(
	const RequestSnapshot& snapshot,
	const string& method,
	const string& path,
	const string& body)
{
	HttpResponse response;
	//1. 세션 열기
	HINTERNET hSession = WinHttpOpen(L"Winters/Client/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession)
	{
		response.error = "WinHttpOpen failed";
		return response;
	}

	//2. 서버 연결
	HINTERNET hConnect = WinHttpConnect(hSession, snapshot.host.c_str(), snapshot.port, 0);
	if (!hConnect)
	{
		response.error = "WinHttpConnect failed";
		WinHttpCloseHandle(hSession);
		return response;
	}

	//3. 요청 생성
	wstring fullPath = snapshot.basePath + ToWide(path);
	wstring wMethod = ToWide(method);

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(),
		fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
		snapshot.secure ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest)
	{
		response.error = "WinHttpOpenRequest failed";
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return response;
	}

	//4. 헤더 추가
	wstring headers = L"Content-Type: application/json\r\n";
	if (!snapshot.authToken.empty())
		headers += L"Authorization: Bearer " + ToWide(snapshot.authToken) + L"\r\n";

	WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

	//5. 요청 전송
	BOOL bResult = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
		(DWORD)body.size(), (DWORD)body.size(), 0);
	//요청 못 받았을 경우 error 메세지 보내고 핸들 닫기!
	if (!bResult)
	{
		response.error = "WinHttpSendRequest failed: " + to_string(GetLastError());
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return response;
	}

	//6. 응답 수신
	bResult = WinHttpReceiveResponse(hRequest, nullptr);
	if (!bResult)
	{
		response.error = "WinHttpReceiveResponse failed";
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return response;
	}

	//7. 상태 코드
	DWORD statusCode = 0;
	DWORD size = sizeof(statusCode);
	WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX);
	response.statusCode = (i32_t)statusCode;

	//8. 바디 읽기
	string responseBody;
	DWORD bytesAvailable = 0;
	do
	{
		bytesAvailable = 0;
		WinHttpQueryDataAvailable(hRequest, &bytesAvailable);
		if (bytesAvailable > 0)
		{
			vector<char> buffer(bytesAvailable + 1, 0);
			DWORD bytesRead = 0;
			WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead);
			responseBody.append(buffer.data(), bytesRead);
		}
	} while (bytesAvailable > 0);

	response.body = responseBody;
	response.success = (statusCode >= 200 && statusCode < 300);

	//9. 정리
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	return response;
}
