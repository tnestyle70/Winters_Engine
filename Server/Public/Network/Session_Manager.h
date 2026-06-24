#pragma once

#include "Network/Session.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

class CSession_Manager
{
public:
    static CSession_Manager* Get();

    std::shared_ptr<CSession> OnAccept(SOCKET clientSocket, const sockaddr_in& addr);
    void OnIoDisconnect(u32_t sessionId);
    void OnDisconnect(u32_t sessionId);
    void OnRecvComplete(u32_t sessionId, const u8_t* bytes, u32_t len);
    void OnSendComplete(u32_t sessionId, u32_t bytes);

    std::shared_ptr<CSession> Find(u32_t sessionId);
    void ForEach(const std::function<void(CSession&)>& fn);
    size_t Count() const;

private:
    CSession_Manager() = default;
    CSession_Manager(const CSession_Manager&) = delete;
    CSession_Manager& operator=(const CSession_Manager&) = delete;

    void ReapClosingSessions();

    std::unordered_map<u32_t, std::shared_ptr<CSession>> m_sessions;
    std::vector<std::shared_ptr<CSession>> m_closingSessions;
    std::atomic<u32_t> m_nextSessionId{ 1 };
    mutable std::mutex m_mutex;
};
