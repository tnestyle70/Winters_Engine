#include "Network/Session.h"

#include "Network/PacketDispatcher.h"

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
    std::lock_guard lk(m_seqMutex);
    bSuspicious = false;

    if (seq <= m_lastProcessedSeq)
        return false;

    if (seq > m_lastProcessedSeq + 60)
    {
        bSuspicious = true;
        return false;
    }

    m_lastProcessedSeq = seq;
    return true;
}

bool CSession::PostInitialRecv()
{
    return PostRecv();
}

bool CSession::PostRecv()
{
    if (m_bClosing.load(std::memory_order_relaxed) || m_socket == INVALID_SOCKET)
        return false;

    m_recvContext.wsaBuf.buf = m_recvContext.buffer;
    m_recvContext.wsaBuf.len = static_cast<ULONG>(sizeof(m_recvContext.buffer));
    ZeroMemory(&m_recvContext.overlapped, sizeof(OVERLAPPED));

    DWORD flags = 0;
    DWORD bytes = 0;

    AddPendingIo();
    const int result = WSARecv(m_socket, &m_recvContext.wsaBuf, 1,
        &bytes, &flags, &m_recvContext.overlapped, nullptr);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        CompletePendingIo();
        return false;
    }

    return true;
}

void CSession::OnRecvComplete(const u8_t* bytes, u32_t len)
{
    CompletePendingIo();

    if (len == 0)
    {
        OnDisconnect();
        return;
    }

    m_recvParser.Append(bytes, len);
    CPacketDispatcher::Instance().DrainFrames(m_sessionId, m_recvParser);

    if (!m_bClosing.load(std::memory_order_relaxed) && !PostRecv())
        OnDisconnect();
}

bool CSession::Send(std::vector<u8_t> packet)
{
    if (m_bClosing.load(std::memory_order_relaxed) ||
        m_socket == INVALID_SOCKET ||
        packet.empty())
    {
        return false;
    }

    std::lock_guard lk(m_sendMutex);
    m_sendQueue.push_back(std::move(packet));
    if (m_bSendPending)
        return true;

    m_bSendPending = true;

    auto& front = m_sendQueue.front();
    m_sendContext.wsaBuf.buf = reinterpret_cast<char*>(front.data());
    m_sendContext.wsaBuf.len = static_cast<ULONG>(front.size());
    ZeroMemory(&m_sendContext.overlapped, sizeof(OVERLAPPED));

    DWORD sent = 0;
    AddPendingIo();
    const int result = WSASend(m_socket, &m_sendContext.wsaBuf, 1,
        &sent, 0, &m_sendContext.overlapped, nullptr);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        CompletePendingIo();
        m_sendQueue.pop_back();
        m_bSendPending = false;
        return false;
    }

    return true;
}

void CSession::OnSendComplete(u32_t bytes)
{
    (void)bytes;
    CompletePendingIo();

    std::lock_guard lk(m_sendMutex);
    if (!m_sendQueue.empty())
        m_sendQueue.pop_front();

    if (m_sendQueue.empty())
    {
        m_bSendPending = false;
        return;
    }

    auto& front = m_sendQueue.front();
    m_sendContext.wsaBuf.buf = reinterpret_cast<char*>(front.data());
    m_sendContext.wsaBuf.len = static_cast<ULONG>(front.size());
    ZeroMemory(&m_sendContext.overlapped, sizeof(OVERLAPPED));

    DWORD sent = 0;
    AddPendingIo();
    const int result = WSASend(m_socket, &m_sendContext.wsaBuf, 1,
        &sent, 0, &m_sendContext.overlapped, nullptr);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        CompletePendingIo();
        m_bSendPending = false;
    }
}

void CSession::OnDisconnect()
{
    m_bClosing.store(true, std::memory_order_relaxed);

    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}
