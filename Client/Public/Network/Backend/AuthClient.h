#pragma once
#include "Defines.h"
#include "Network/Backend/CHttpClient.h"

NS_BEGIN(Client)

struct AuthResult
{
	bool_t  success      = false;
	i32_t   statusCode   = 0;
	string  userID;
	string  displayName;
	string  accessToken;
	string  refreshToken;
	i64_t   expiresAt    = 0;
	string  error;
};

using AuthCallback = function<void(const AuthResult&)>;

class CAuthClient
{
private:
	CAuthClient() = default;
public:
	~CAuthClient() = default;

	static unique_ptr<CAuthClient> Create(const string& baseURL);

	void Register(const string& username, const string& email,
		const string& password, AuthCallback callback);
	void Login(const string& email, const string& password, AuthCallback callback);
	// 패스워드 없는 로컬 아이디 계정 (/auth/id/*): 404 = 미가입(회원 가입 유도), 409 = 이미 가입됨.
	void LoginByID(const string& loginID, AuthCallback callback);
	void RegisterByID(const string& loginID, AuthCallback callback);
	void Refresh(AuthCallback callback);
	void Logout();
	void ProcessCallbacks();

	const string& GetAccessToken() const { return m_AccessToken; }
	bool_t IsLoggedIn() const { return !m_AccessToken.empty(); }
private:
	AuthResult ParseAuthResponse(const HttpResponse& resp);

	unique_ptr<CHttpClient> m_pHttp;
	string m_AccessToken;
	string m_RefreshToken;
};

NS_END
