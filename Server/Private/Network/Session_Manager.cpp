#include "Network/Session_Manager.h"

#include "Game/GameRoom.h"
#include "Network/PacketDispatcher.h"

#include <algorithm>
#include <iostream>

extern CGameRoom* g_pRoom;

CSession_Manager* CSession_Manager::Get()
{
    static CSession_Manager s_inst;
    return &s_inst;
}

std::shared_ptr<CSession> CSession_Manager::OnAccept(SOCKET clientSocket,
    const sockaddr_in& addr)
{
    (void)addr;

    const u32_t newSid = m_nextSessionId.fetch_add(1, std::memory_order_relaxed);
    auto session = CSession::Create(clientSocket, newSid);
    if (!session)
    {
        closesocket(clientSocket);
        return nullptr;
    }

    {
        std::lock_guard lk(m_mutex);
        m_sessions[newSid] = session;
    }

    std::cout << "[SM] OnAccept sid=" << newSid << "\n";
    return session;
}

void CSession_Manager::OnDisconnect(u32_t sessionId)
{
    std::shared_ptr<CSession> session;
    {
        std::lock_guard lk(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end())
            return;

        session = std::move(it->second);
        m_sessions.erase(it);
        m_closingSessions.push_back(session);
    }

    if (g_pRoom)
        g_pRoom->OnSessionLeave(sessionId);
    CPacketDispatcher::Instance().UnrouteSession(sessionId);

    std::cout << "[SM] OnDisconnect sid=" << sessionId << "\n";
    session->OnDisconnect();
    ReapClosingSessions();
}

void CSession_Manager::OnIoDisconnect(u32_t sessionId)
{
    auto pSession = Find(sessionId);
    if (pSession)
        pSession->CompletePendingIo();

    OnDisconnect(sessionId);
    ReapClosingSessions();
}

void CSession_Manager::OnRecvComplete(u32_t sessionId, const u8_t* bytes, u32_t len)
{
    auto pSession = Find(sessionId);
    if (pSession)
        pSession->OnRecvComplete(bytes, len);
    ReapClosingSessions();
}

void CSession_Manager::OnSendComplete(u32_t sessionId, u32_t bytes)
{
    auto pSession = Find(sessionId);
    if (pSession)
        pSession->OnSendComplete(bytes);
    ReapClosingSessions();
}

std::shared_ptr<CSession> CSession_Manager::Find(u32_t sessionId)
{
    std::lock_guard lk(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end())
        return it->second;

    for (auto& closing : m_closingSessions)
    {
        if (closing && closing->GetSessionId() == sessionId)
            return closing;
    }

    return nullptr;
}

void CSession_Manager::ForEach(const std::function<void(CSession&)>& fn)
{
    std::vector<u32_t> ids;
    {
        std::lock_guard lk(m_mutex);
        ids.reserve(m_sessions.size());
        for (const auto& [sid, session] : m_sessions)
        {
            (void)session;
            ids.push_back(sid);
        }
    }

    std::sort(ids.begin(), ids.end());

    for (u32_t sid : ids)
    {
        auto pSession = Find(sid);
        if (pSession)
            fn(*pSession);
    }
}

size_t CSession_Manager::Count() const
{
    std::lock_guard lk(m_mutex);
    return m_sessions.size();
}

void CSession_Manager::ReapClosingSessions()
{
    std::lock_guard lk(m_mutex);
    m_closingSessions.erase(
        std::remove_if(m_closingSessions.begin(), m_closingSessions.end(),
            [](const std::shared_ptr<CSession>& session)
            {
                return !session || session->CanDestroy();
            }),
        m_closingSessions.end());
}
