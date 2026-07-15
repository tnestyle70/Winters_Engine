#include "Network/Session.h"

#include "Network/PacketDispatcher.h"
#include "Network/ServerSessionHub.h"

#include <iostream>

namespace
{
    constexpr size_t kMaxTcpSendQueuePackets = 256u;
    constexpr size_t kMaxTcpSendQueueBytes = 8u * 1024u * 1024u;
}

std::shared_ptr<CSession> CSession::Create(SOCKET socket, u32_t sessionId)
{
    return std::shared_ptr<CSession>(new CSession(socket, sessionId));
}

CSession::CSession(SOCKET socket, u32_t sessionId)
    : m_socket(socket), m_sessionId(sessionId)
{
    m_recvContext.op = eIOOp::Recv;
    m_recvContext.sessionId = sessionId;
    m_sendContext.op = eIOOp::Send;
    m_sendContext.sessionId = sessionId;
}

CSession::~CSession()
{
    OnDisconnect();
}

bool CSession::TryAcceptSequence(u32_t seq, bool& bSuspicious)
{
    return CServerSessionHub::Instance().TryAcceptCommandSequence(
        m_sessionId,
        seq,
        bSuspicious);
}

void CSession::FlagSuspicious()
{
    CServerSessionHub::Instance().FlagSuspicious(m_sessionId);
}

bool CSession::IsSuspicious() const
{
    return CServerSessionHub::Instance().IsSuspicious(m_sessionId);
}

bool CSession::PostInitialRecv()
{
    return PostRecv();
}

bool CSession::PostRecv()
{
    if (m_bClosing.load(std::memory_order_acquire))
        return false;
    const SOCKET socket = m_socket.load(std::memory_order_acquire);
    if (socket == INVALID_SOCKET)
        return false;

    m_recvContext.wsaBuf.buf = m_recvContext.buffer;
    m_recvContext.wsaBuf.len = static_cast<ULONG>(sizeof(m_recvContext.buffer));
    ZeroMemory(&m_recvContext.overlapped, sizeof(OVERLAPPED));

    DWORD flags = 0;
    DWORD bytes = 0;

    AddPendingIo();
    const int result = WSARecv(socket, &m_recvContext.wsaBuf, 1,
        &bytes, &flags, &m_recvContext.overlapped, nullptr);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        CompletePendingIo();
        return false;
    }

    return true;
}

bool CSession::OnRecvComplete(const u8_t* bytes, u32_t len)
{
    CompletePendingIo();

    if (len == 0 || m_bClosing.load(std::memory_order_relaxed))
        return false;

    m_recvParser.Append(bytes, len);
    CPacketDispatcher::Instance().DrainFrames(m_sessionId, m_recvParser);

    return !m_bClosing.load(std::memory_order_relaxed) && PostRecv();
}

bool CSession::Send(std::vector<u8_t> packet)
{
    if (m_bClosing.load(std::memory_order_acquire) ||
        packet.empty())
    {
        return false;
    }

    std::lock_guard lk(m_sendMutex);
    if (m_sendQueue.size() >= kMaxTcpSendQueuePackets ||
        packet.size() > kMaxTcpSendQueueBytes - m_sendQueueBytes)
    {
        return false;
    }

    m_sendQueueBytes += packet.size();
    m_sendQueue.push_back(std::move(packet));
    if (m_bSendPending)
        return true;

    m_bSendPending = true;
    m_sendOffset = 0;
    if (PostFrontSendLocked())
        return true;

    m_sendQueueBytes -= m_sendQueue.front().size();
    m_sendQueue.pop_front();
    m_sendOffset = 0;
    m_bSendPending = false;
    return false;
}

bool CSession::PostFrontSendLocked()
{
    if (m_bClosing.load(std::memory_order_acquire) ||
        m_sendQueue.empty() ||
        m_sendOffset >= m_sendQueue.front().size())
    {
        return false;
    }

    auto& front = m_sendQueue.front();
    const SOCKET socket = m_socket.load(std::memory_order_acquire);
    if (socket == INVALID_SOCKET)
        return false;
    const size_t remaining = front.size() - m_sendOffset;
    m_sendContext.wsaBuf.buf = reinterpret_cast<char*>(
        front.data() + m_sendOffset);
    m_sendContext.wsaBuf.len = static_cast<ULONG>(remaining);
    ZeroMemory(&m_sendContext.overlapped, sizeof(OVERLAPPED));

    DWORD sent = 0;
    AddPendingIo();
    const int result = WSASend(socket, &m_sendContext.wsaBuf, 1,
        &sent, 0, &m_sendContext.overlapped, nullptr);
    if (result == SOCKET_ERROR)
    {
        const int sendError = WSAGetLastError();
        if (sendError != WSA_IO_PENDING)
        {
            CompletePendingIo();
            std::cerr << "[Session] send post failed sid=" << m_sessionId
                      << " wsa=" << sendError << '\n';
            return false;
        }
    }
    return true;
}

bool CSession::OnSendComplete(u32_t bytes)
{
    CompletePendingIo();

    std::lock_guard lk(m_sendMutex);
    if (!m_bSendPending || m_sendQueue.empty())
        return false;

    const size_t remaining =
        m_sendQueue.front().size() - m_sendOffset;
    if (bytes == 0u || bytes > remaining)
    {
        m_bSendPending = false;
        return false;
    }

    m_sendOffset += bytes;
    if (m_sendOffset < m_sendQueue.front().size())
    {
        if (PostFrontSendLocked())
            return true;
        m_bSendPending = false;
        return false;
    }

    m_sendQueueBytes -= m_sendQueue.front().size();
    m_sendQueue.pop_front();
    m_sendOffset = 0;
    if (m_sendQueue.empty())
    {
        m_bSendPending = false;
        return true;
    }

    if (PostFrontSendLocked())
        return true;
    m_bSendPending = false;
    return false;
}

void CSession::OnDisconnect()
{
    if (m_bClosing.exchange(true, std::memory_order_acq_rel))
        return;

    const SOCKET socket = m_socket.exchange(
        INVALID_SOCKET,
        std::memory_order_acq_rel);
    if (socket != INVALID_SOCKET)
        closesocket(socket);
}
