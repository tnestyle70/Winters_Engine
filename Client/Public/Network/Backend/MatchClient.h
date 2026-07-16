#pragma once
#include "Defines.h"
#include "Network/Backend/CHttpClient.h"

NS_BEGIN(Client)

struct MatchStatus
{
    string  status;     // "queued", "matched"
    string  matchId;
    string  gameSessionId;
    string  host;
    i32_t   port = 0;
    string  transport;
    string  playerTicket;
    i64_t   expiresAt = 0;
    string  error;
};

using MatchCallback = function<void(const MatchStatus&)>;

class CMatchClient
{
public:
    ~CMatchClient() = default;

    static unique_ptr<CMatchClient> Create(const string& baseURL);

    void    SetAuthToken(const string& token);
    void    JoinQueue(MatchCallback callback);
    void    PollStatus(MatchCallback callback);
    void    LeaveQueue(MatchCallback callback);
    void    ProcessCallbacks();

private:
    CMatchClient() = default;
    MatchStatus ParseResponse(const HttpResponse& resp);

    unique_ptr<CHttpClient> m_pHttp;
};

NS_END
