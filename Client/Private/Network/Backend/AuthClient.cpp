#include "Network/Backend/AuthClient.h"
#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

using namespace Client;
using json = nlohmann::json;

unique_ptr<CAuthClient> CAuthClient::Create(const string& baseURL)
{
	auto pInstance = unique_ptr<CAuthClient>(new CAuthClient());
	pInstance->m_pHttp = CHttpClient::Create(baseURL);
	return pInstance;
}

void CAuthClient::Register(const string& username, const string& email, 
	const string& password, AuthCallback callback)
{
	string body = "{\"username\":\"" + username
		+ "\",\"email\":\"" + email
		+ "\",\"password\":\"" + password + "\"}";

	m_pHttp->AsyncPost("/auth/register", body, [this, callback](const HttpResponse& resp) {
		AuthResult result = ParseAuthResponse(resp);
		if (result.success)
		{
			m_AccessToken = result.accessToken;
			m_RefreshToken = result.refreshToken;
			m_pHttp->SetAuthToken(m_AccessToken);
		}
		callback(result);
	});
}

void CAuthClient::Login(const string & email, const string & password, 
	AuthCallback callback)
{
	string body = "{\"email\":\"" + email + "\",\"password\":\"" + password + "\"}";

	m_pHttp->AsyncPost("/auth/login", body, [this, callback](const HttpResponse& resp) {
		AuthResult result = ParseAuthResponse(resp);
		if (result.success)
		{
			m_AccessToken = result.accessToken;
			m_RefreshToken = result.refreshToken;
			m_pHttp->SetAuthToken(m_AccessToken);
		}
		callback(result);
	});
}

void CAuthClient::LoginByID(const string& loginID, AuthCallback callback)
{
	json bodyJson;
	bodyJson["login_id"] = loginID;

	m_pHttp->AsyncPost("/auth/id/login", bodyJson.dump(), [this, callback](const HttpResponse& resp) {
		AuthResult result = ParseAuthResponse(resp);
		if (result.success)
		{
			m_AccessToken = result.accessToken;
			m_RefreshToken = result.refreshToken;
			m_pHttp->SetAuthToken(m_AccessToken);
		}
		callback(result);
	});
}

void CAuthClient::RegisterByID(const string& loginID, AuthCallback callback)
{
	json bodyJson;
	bodyJson["login_id"] = loginID;

	m_pHttp->AsyncPost("/auth/id/register", bodyJson.dump(), [this, callback](const HttpResponse& resp) {
		AuthResult result = ParseAuthResponse(resp);
		if (result.success)
		{
			m_AccessToken = result.accessToken;
			m_RefreshToken = result.refreshToken;
			m_pHttp->SetAuthToken(m_AccessToken);
		}
		callback(result);
	});
}

void CAuthClient::Refresh(AuthCallback callback)
{
	string body = "{\"refresh_token\":\"" + m_RefreshToken + "\"}";

	m_pHttp->AsyncPost("/auth/refresh", body, [this, callback](const HttpResponse& resp) {
		AuthResult result = ParseAuthResponse(resp);
		if (result.success)
		{
			m_AccessToken = result.accessToken;
			m_RefreshToken = result.refreshToken;
			m_pHttp->SetAuthToken(m_AccessToken);
		}
		callback(result);
		});
}

void CAuthClient::Logout()
{
	m_AccessToken.clear();
	m_RefreshToken.clear();
	m_pHttp->SetAuthToken("");
}

void CAuthClient::ProcessCallbacks()
{
	m_pHttp->ProcessCallbacks();
}

AuthResult CAuthClient::ParseAuthResponse(const HttpResponse& resp)
{
	AuthResult result;
	result.statusCode = resp.statusCode;
	try
	{
		auto j = json::parse(resp.body);
		if (!resp.success || !j.value("success", false))
		{
			result.error = j.value("error", "unknown error");
			return result;
		}
		auto data = j["data"];
		result.success = true;
		result.userID = data.value("user_id", "");
		result.displayName = data.value("display_name", "");
		result.accessToken = data.value("access_token", "");
		result.refreshToken = data.value("refresh_token", "");
		result.expiresAt = data.value("expires_at", (i64_t)0);
	}
	catch (const json::exception& e)
	{
		result.error = string("JSON parse error: ") + e.what();
	}
	return result;
}
