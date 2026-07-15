#pragma once
#include "Defines.h"
#include <queue>
#include <mutex>
#include <future>
#include <vector>

NS_BEGIN(Client)

struct HttpResponse
{
	i32_t   statusCode = 0;
	string  body;
	bool_t  success    = false;
	string  error;
};

using HttpCallback = function<void(const HttpResponse&)>;

class CHttpClient
{
public:
	// 진행 중인 async 요청이 전부 끝날 때까지 대기 후 파괴된다.
	// 호출 시점 블로킹(gotcha 2026-07-09 async lifetime)을 파괴 시점으로 한정하는 계약.
	~CHttpClient();

	static unique_ptr<CHttpClient> Create(const string& baseURL);

	void SetAuthToken(const string& token);

	HttpResponse Get(const string& path);
	HttpResponse Post(const string& path, const string& jsonBody);
	HttpResponse Delete(const string& path);

	void AsyncGet(const string& path, HttpCallback callback);
	void AsyncPost(const string& path, const string& jsonBody, HttpCallback callback);

	void ProcessCallbacks();

private:
	CHttpClient() = default;

	// worker 스레드는 멤버 대신 호출 시점 복사본만 읽는다 (SetAuthToken과의 race 차단).
	struct RequestSnapshot
	{
		wstring host;
		uint16_t port = 80;
		wstring basePath;
		string authToken;
	};

	RequestSnapshot MakeRequestSnapshot() const;
	static HttpResponse DoRequestWith(
		const RequestSnapshot& snapshot,
		const string& method,
		const string& path,
		const string& body);
	HttpResponse DoRequest(const string& method, const string& path, const string& body);
	void LaunchAsyncRequest(string method, string path, string body, HttpCallback callback);
	void PruneCompletedRequests();
	void ParseURL(const string& url, wstring& host, uint16_t& port, wstring& basePath);

	string m_BaseURL;
	wstring m_Host;
	uint16_t m_Port = 80;
	wstring m_BasePath;
	string m_AuthToken;
	mutex m_CallbackMutex;
	queue<function<void()>> m_PendingCallbacks;
	mutex m_RequestMutex;
	vector<future<void>> m_PendingRequests;
};

NS_END