#pragma once
#include "Defines.h"
#include <queue>
#include <mutex>

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
	~CHttpClient() = default;

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

	HttpResponse DoRequest(const string& method, const string& path, const string& body);
	void ParseURL(const string& url, wstring& host, uint16_t& port, wstring& basePath);

	string m_BaseURL;
	wstring m_Host;
	uint16_t m_Port = 80;
	wstring m_BasePath;
	string m_AuthToken;
	mutex m_CallbackMutex;
	queue<function<void()>> m_PendingCallbacks;
};

NS_END