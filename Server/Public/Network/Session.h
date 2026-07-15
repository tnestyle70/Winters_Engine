#pragma once

#include "ECS/Entity.h"
#include "Network/FrameParser.h"
#include "Network/IOCPCore.h"
#include "WintersTypes.h"

#include <WinSock2.h>
#include <atomic>
#include <cstddef>
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
    // False means the IOCP owner must perform the idempotent manager-level
    // disconnect so Hub, GameRoom, and dispatcher ownership are retired too.
    bool OnRecvComplete(const u8_t* bytes, u32_t len);
    bool OnSendComplete(u32_t bytes);
    void OnDisconnect();
    bool Send(std::vector<u8_t> packet);

    u32_t GetSessionId() const { return m_sessionId; }
    SOCKET GetSocket() const
    {
        return m_socket.load(std::memory_order_acquire);
    }
    EntityID GetControlledEntity() const { return m_controlledEntity; }
    void SetControlledEntity(EntityID entity) { m_controlledEntity = entity; }

    bool TryAcceptSequence(u32_t seq, bool& bSuspicious);

    void FlagSuspicious();
    bool IsSuspicious() const;

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
    bool PostFrontSendLocked();

    std::atomic<SOCKET> m_socket{ INVALID_SOCKET };
    u32_t m_sessionId = 0;
    EntityID m_controlledEntity = NULL_ENTITY;

    std::atomic<u32_t> m_pendingIoCount{ 0 };
    std::atomic<bool> m_bClosing{ false };

    CFrameParser m_recvParser;

    IOContext m_recvContext{};
    IOContext m_sendContext{};

    std::mutex m_sendMutex;
    std::deque<std::vector<u8_t>> m_sendQueue;
    size_t m_sendQueueBytes = 0;
    size_t m_sendOffset = 0;
    bool m_bSendPending = false;
};
