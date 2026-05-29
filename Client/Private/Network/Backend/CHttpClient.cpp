#include "Network/Backend/CHttpClient.h"
#include <winhttp.h>
#include <future>

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
	pInstance->ParseURL(baseURL, pInstance->m_Host, pInstance->m_Port, pInstance->m_BasePath);

	return pInstance;
}

//URL Parsing http://localhost:8081 -> host, port, basepath
void CHttpClient::ParseURL(const string& url, wstring& host, uint16_t& port, wstring& basePath)
{
	string temp = url;
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
		port = 80;
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
void CHttpClient::AsyncGet(const string& path, HttpCallback callback)
{
	auto self = this;
	string pathCopy = path;
	//별도의 쓰레드에서 비동기 요청, 게임이 멈추면 안 되기 때문에 별도의 쓰레드에서 따로 돌림!
	async(launch::async, [self, pathCopy, callback]() {
		HttpResponse resp = self->DoRequest("GET", pathCopy, "");
		lock_guard<mutex> lock(self->m_CallbackMutex);
		self->m_PendingCallbacks.push([callback, resp]() {callback(resp); });
	});
}

void CHttpClient::AsyncPost(const string & path, const string & jsonBody, HttpCallback callback)
{
	auto self = this;
	string pathCopy = path;
	string bodyCopy = jsonBody;
	//별도의 쓰레드에서 요청!, 요청 응답 받을 때까지 게임이 끊기면 안 되기 때문이다!!
	async(launch::async, [self, pathCopy, bodyCopy, callback]() {
		HttpResponse resp = self->DoRequest("POST", pathCopy, bodyCopy);
		lock_guard<mutex> lock(self->m_CallbackMutex);
		self->m_PendingCallbacks.push([callback, resp]() {callback(resp); });
		});
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
//핵심 : WinHTTP 요청!
HttpResponse CHttpClient::DoRequest(const string & method, const string & path, const string & body)
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
	HINTERNET hConnect = WinHttpConnect(hSession, m_Host.c_str(), m_Port, 0);
	if (!hConnect)
	{
		response.error = "WinHttpConnect failed";
		WinHttpCloseHandle(hSession);
		return response;
	}

	//3. 요청 생성
	wstring fullPath = m_BasePath + ToWide(path);
	wstring wMethod = ToWide(method);

	HINTERNET hRequest = WinHttpOpenRequest(hConnect, wMethod.c_str(),
		fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
	if (!hRequest)
	{
		response.error = "WinHttpOpenRequest failed";
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		return response;
	}

	//4. 헤더 추가
	wstring headers = L"Content-Type: application/json\r\n";
	if (!m_AuthToken.empty())
		headers += L"Authorization: Bearer " + ToWide(m_AuthToken) + L"\r\n";

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
