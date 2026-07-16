#include "Network/Backend/MatchClient.h"
#pragma push_macro("new")
#undef new
#include "Network/Backend/json.hpp"
#pragma pop_macro("new")

using json = nlohmann::json;

unique_ptr<CMatchClient> CMatchClient::Create(const string& baseURL)
{
    auto pInstance = unique_ptr<CMatchClient>(new CMatchClient());
    pInstance->m_pHttp = CHttpClient::Create(baseURL);
    return pInstance;
}

void CMatchClient::SetAuthToken(const string& token)
{
	m_pHttp->SetAuthToken(token);
}

void CMatchClient::JoinQueue(MatchCallback callback)
{
    m_pHttp->AsyncPost("/matchmaking/join", "{}", [this, callback](const HttpResponse& resp) {
        callback(ParseResponse(resp));
        });
}

void CMatchClient::PollStatus(MatchCallback callback)
{
    m_pHttp->AsyncGet("/matchmaking/status", [this, callback](const HttpResponse& resp) {
        callback(ParseResponse(resp));
        });
}

void CMatchClient::LeaveQueue(MatchCallback callback)
{
    m_pHttp->AsyncPost("/matchmaking/leave", "{}", [this, callback](const HttpResponse& resp) {
        callback(ParseResponse(resp));
        });
}

void CMatchClient::ProcessCallbacks()
{
    m_pHttp->ProcessCallbacks();
}

MatchStatus CMatchClient::ParseResponse(const HttpResponse & resp)
{
    MatchStatus result;
    try
    {
        auto j = json::parse(resp.body);
        if (!resp.success)
        {
            result.error = j.value("error", "unknown error");
            return result;
        }
        auto data = j["data"];
        result.status = data.value("status", "");
        result.matchId = data.value("match_id", "");
        result.gameSessionId = data.value("game_session_id", "");
        result.host = data.value("host", "");
        result.port = data.value("port", 0);
        result.transport = data.value("transport", "");
        result.playerTicket = data.value("player_ticket", "");
        result.expiresAt = data.value("expires_at", 0ll);
    }
    catch (const json::exception& e)
    {
        result.error = string("JSON parse error : ") + e.what();
    }
    return result;
}
