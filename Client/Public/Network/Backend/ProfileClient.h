#pragma once
#include "Defines.h"
#include "Network/Backend/CHttpClient.h"

NS_BEGIN(Client)

struct ProfileData
{
	string userId;
	string username;
	i32_t mmr = 0;
	i32_t rank = 0;
	i32_t wins = 0;
	i32_t losses = 0;
	i32_t winRate = 0;
	i32_t kills = 0;
	i32_t deaths = 0;
	i32_t assists = 0;
	f32_t kda = 0.f;
	string error;
};

struct MatchRecord
{
	string  matchId;
	string  result;
	i32_t   kills = 0;
	i32_t   deaths = 0;
	i32_t   assists = 0;
	i32_t   mmrChange = 0;
};

using ProfileCallback = function<void(const ProfileData&)>;
using HistoryCallback = function<void(const vector<MatchRecord>&)>;

class CProfileClient
{
private:
	CProfileClient() = default;
public:
	~CProfileClient() = default;
	
	void SetAuthToken(const string& token);
	void GetProfile(const string& userId, ProfileCallback callback);
	void GetHistory(const string& userId, HistoryCallback callback);
	// JWT claims 기반 자기 프로필/전적 (/profile/me, /profile/me/history) — user id를 보내지 않는다.
	void GetMyProfile(ProfileCallback callback);
	void GetMyHistory(HistoryCallback callback);
	void ProcessCallbacks();

	static unique_ptr<CProfileClient> Create(const string& baseURL);

private:
	unique_ptr<CHttpClient> m_pHttp;
};

NS_END
