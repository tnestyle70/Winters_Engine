#include "Network/Backend/ProfileClient.h"
#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

using namespace Client;
using json = nlohmann::json;

void CProfileClient::SetAuthToken(const string & token)
{
	m_pHttp->SetAuthToken(token);
}

void CProfileClient::GetProfile(const string & userId, ProfileCallback callback)
{
	m_pHttp->AsyncGet("/profile/" + userId, [callback](const HttpResponse& resp) {
		ProfileData profile;
		try
		{
			auto j = json::parse(resp.body);
			if (!resp.success)
			{
				profile.error = j.value("error", "");
				callback(profile);
				return;
			}
			//backend에서 정보 받아와서 테이블 별로 데이터 파싱!
			auto data = j["data"];
			profile.userId = data.value("user_id", "");
			profile.username = data.value("username", "");
			profile.mmr = data.value("mmr", 0);
			profile.rank = data.value("rank", 0);
			profile.wins = data.value("wins", 0);
			profile.losses = data.value("losses", 0);
			profile.winRate = data.value("win_rate", 0);
			profile.kills = data.value("kills", 0);
			profile.deaths = data.value("deaths", 0);
			profile.assists = data.value("assists", 0);
			profile.kda = data.value("kda", 0.f);
		}
		catch (const json::exception& e) { profile.error = e.what(); }
		callback(profile);
		});
}

void CProfileClient::GetHistory(const string & userId, HistoryCallback callback)
{
	m_pHttp->AsyncGet("/profile/" + userId + "/history", [callback](const HttpResponse& resp) {
		vector<MatchRecord> records;
		try
		{
			auto j = json::parse(resp.body);
			if (!resp.success || !j.contains("data"))
			{
				callback(records);
				return;
			}
			for (const auto& item : j["data"])
			{
				MatchRecord rec;
				rec.matchId = item.value("match_id", "");
				rec.result = item.value("result", "");
				rec.kills = item.value("kills", 0);
				rec.deaths = item.value("deaths", 0);
				rec.assists = item.value("assists", 0);
				rec.mmrChange = item.value("mmr_change", 0);
				records.push_back(rec);
			}
		}
		catch (...) {}
		callback(records);
		});
}

void CProfileClient::GetMyProfile(ProfileCallback callback)
{
	m_pHttp->AsyncGet("/profile/me", [callback](const HttpResponse& resp) {
		ProfileData profile;
		try
		{
			auto j = json::parse(resp.body);
			if (!resp.success)
			{
				profile.error = j.value("error", "");
				callback(profile);
				return;
			}
			auto data = j["data"];
			profile.userId = data.value("user_id", "");
			profile.username = data.value("username", "");
			profile.mmr = data.value("mmr", 0);
			profile.rank = data.value("rank", 0);
			profile.wins = data.value("wins", 0);
			profile.losses = data.value("losses", 0);
			profile.kills = data.value("kills", 0);
			profile.deaths = data.value("deaths", 0);
			profile.assists = data.value("assists", 0);
		}
		catch (const json::exception& e) { profile.error = e.what(); }
		callback(profile);
		});
}

void CProfileClient::GetMyHistory(HistoryCallback callback)
{
	m_pHttp->AsyncGet("/profile/me/history", [callback](const HttpResponse& resp) {
		vector<MatchRecord> records;
		try
		{
			auto j = json::parse(resp.body);
			if (!resp.success || !j.contains("data"))
			{
				callback(records);
				return;
			}
			for (const auto& item : j["data"])
			{
				MatchRecord rec;
				rec.matchId = item.value("match_id", "");
				rec.result = item.value("result", "");
				rec.kills = item.value("kills", 0);
				rec.deaths = item.value("deaths", 0);
				rec.assists = item.value("assists", 0);
				rec.mmrChange = item.value("mmr_change", 0);
				records.push_back(rec);
			}
		}
		catch (...) {}
		callback(records);
		});
}

void CProfileClient::ProcessCallbacks()
{
	m_pHttp->ProcessCallbacks();
}

unique_ptr<CProfileClient> CProfileClient::Create(const string & baseURL)
{
	auto pInstance = unique_ptr<CProfileClient>(new CProfileClient());
	pInstance->m_pHttp = CHttpClient::Create(baseURL);
	return pInstance;
}
