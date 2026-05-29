#pragma once

#include "ECS/Entity.h"
#include "Network/FrameParser.h"
#include "Network/IOCPCore.h"
#include "WintersTypes.h"

#include <WinSock2.h>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

class CSession : public std::enable_shared_from_this<CSession>
{
public:
    static std::shared_ptr<CSession> Create(SOCKET socket, u32_t sessionId);
    ~CSession();

    bool PostInitialRecv();
    bool PostRecv();
    void OnRecvComplete(const u8_t* bytes, u32_t len);
    void OnSendComplete(u32_t bytes);
    void OnDisconnect();
    bool Send(std::vector<u8_t> packet);

    u32_t GetSessionId() const { return m_sessionId; }
    SOCKET GetSocket() const { return m_socket; }
    EntityID GetControlledEntity() const { return m_controlledEntity; }
    void SetControlledEntity(EntityID entity) { m_controlledEntity = entity; }

    bool TryAcceptSequence(u32_t seq, bool& bSuspicious);

    void FlagSuspicious() { ++m_suspicionCount; }
    bool IsSuspicious() const { return m_suspicionCount > 5; }

    void AddPendingIo()
    {
        m_pendingIoCount.fetch_add(1, std::memory_order_relaxed);
    }

    void CompletePendingIo()
    {
        m_pendingIoCount.fetch_sub(1, std::memory_order_relaxed);
    }

    bool CanDestroy() const
    {
        return m_bClosing.load(std::memory_order_relaxed) &&
            m_pendingIoCount.load(std::memory_order_relaxed) == 0;
    }

    CFrameParser& GetRecvParser() { return m_recvParser; }

private:
    CSession(SOCKET socket, u32_t sessionId);

    SOCKET m_socket = INVALID_SOCKET;
    u32_t m_sessionId = 0;
    EntityID m_controlledEntity = NULL_ENTITY;

    mutable std::mutex m_seqMutex;
    u32_t m_lastProcessedSeq = 0;
    u32_t m_suspicionCount = 0;

    std::atomic<u32_t> m_pendingIoCount{ 0 };
    std::atomic<bool> m_bClosing{ false };

    CFrameParser m_recvParser;

    IOContext m_recvContext{};
    IOContext m_sendContext{};

    std::mutex m_sendMutex;
    std::deque<std::vector<u8_t>> m_sendQueue;
    bool m_bSendPending = false;
};
