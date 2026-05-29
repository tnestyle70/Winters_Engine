#include "Network/IOCPCore.h"

#include "Game/GameRoom.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"

#include <iostream>
#include <ws2tcpip.h>

extern CGameRoom* g_pRoom;

namespace
{
    void LogWinsockError(const char* pOperation)
    {
        std::cerr << "[IOCPCore] " << (pOperation ? pOperation : "winsock")
            << " failed wsa=" << WSAGetLastError() << "\n";
    }

    void LogWin32Error(const char* pOperation)
    {
        std::cerr << "[IOCPCore] " << (pOperation ? pOperation : "win32")
            << " failed gle=" << GetLastError() << "\n";
    }
}

std::unique_ptr<CIOCPCore> CIOCPCore::Create(u16_t port, u32_t workerCount)
{
    return std::unique_ptr<CIOCPCore>(new CIOCPCore(port, workerCount));
}

CIOCPCore::CIOCPCore(u16_t port, u32_t workerCount)
    : m_port(port), m_workerCount(workerCount ? workerCount : 1)
{
}

CIOCPCore::~CIOCPCore()
{
    Shutdown();
}

bool CIOCPCore::Start()
{
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, m_workerCount);
    if (!m_hIOCP)
    {
        LogWin32Error("CreateIoCompletionPort(root)");
        return false;
    }

    m_listenSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (m_listenSocket == INVALID_SOCKET)
    {
        LogWinsockError("WSASocketW(listen)");
        return false;
    }

    BOOL exclusive = TRUE;
    if (::setsockopt(
        m_listenSocket,
        SOL_SOCKET,
        SO_EXCLUSIVEADDRUSE,
        reinterpret_cast<const char*>(&exclusive),
        sizeof(exclusive)) == SOCKET_ERROR)
    {
        LogWinsockError("setsockopt(SO_EXCLUSIVEADDRUSE)");
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = ::htons(m_port);

    if (::bind(m_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        LogWinsockError("bind(0.0.0.0)");
        return false;
    }

    if (::listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        LogWinsockError("listen()");
        return false;
    }

    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_listenSocket), m_hIOCP, 0, 0))
    {
        LogWin32Error("CreateIoCompletionPort(listen)");
        return false;
    }

    GUID guidAcceptEx = WSAID_ACCEPTEX;
    GUID guidGetAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    DWORD bytes = 0;

    if (WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidAcceptEx, sizeof(guidAcceptEx), &m_pfnAcceptEx, sizeof(m_pfnAcceptEx),
        &bytes, nullptr, nullptr) == SOCKET_ERROR)
    {
        LogWinsockError("WSAIoctl(AcceptEx)");
        return false;
    }

    if (WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidGetAddrs, sizeof(guidGetAddrs), &m_pfnGetAcceptExSockaddrs,
        sizeof(m_pfnGetAcceptExSockaddrs), &bytes, nullptr, nullptr) == SOCKET_ERROR)
    {
        LogWinsockError("WSAIoctl(GetAcceptExSockaddrs)");
        return false;
    }

    if (!m_pfnAcceptEx || !m_pfnGetAcceptExSockaddrs)
    {
        std::cerr << "[IOCPCore] AcceptEx extension pointers missing\n";
        return false;
    }

    m_bRunning = true;
    for (u32_t i = 0; i < m_workerCount; ++i)
        m_workers.emplace_back(&CIOCPCore::WorkerLoop, this, i);

    for (u32_t i = 0; i < 4; ++i)
    {
        auto* ctx = new IOContext();
        ctx->op = eIOOp::Accept;
        if (!PostAccept(m_listenSocket, ctx))
            delete ctx;
    }

    std::cout << "[IOCPCore] listening on port " << m_port
        << " (workers=" << m_workerCount << ")\n";
    return true;
}

void CIOCPCore::Shutdown()
{
    const bool wasRunning = m_bRunning.exchange(false);

    if (wasRunning && m_hIOCP)
    {
        for (u32_t i = 0; i < m_workerCount; ++i)
            PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr);
    }

    if (m_listenSocket != INVALID_SOCKET)
    {
        ::closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    for (auto& worker : m_workers)
    {
        if (worker.joinable())
            worker.join();
    }
    m_workers.clear();

    if (m_hIOCP)
    {
        CloseHandle(m_hIOCP);
        m_hIOCP = nullptr;
    }
}

bool CIOCPCore::PostAccept(SOCKET listenSocket, IOContext* ctx)
{
    if (!ctx || !m_pfnAcceptEx)
        return false;

    ZeroMemory(&ctx->overlapped, sizeof(ctx->overlapped));
    ctx->op = eIOOp::Accept;
    ctx->sessionId = 0;

    ctx->acceptSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (ctx->acceptSocket == INVALID_SOCKET)
    {
        LogWinsockError("WSASocketW(accept)");
        return false;
    }

    DWORD bytes = 0;
    const BOOL ok = m_pfnAcceptEx(listenSocket, ctx->acceptSocket,
        ctx->acceptBuffer, 0,
        sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
        &bytes, &ctx->overlapped);

    const int acceptError = WSAGetLastError();
    if (!ok && acceptError != ERROR_IO_PENDING)
    {
        std::cerr << "[IOCPCore] AcceptEx failed wsa=" << acceptError << "\n";
        ::closesocket(ctx->acceptSocket);
        ctx->acceptSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

bool CIOCPCore::BindIOCP(SOCKET socket, u32_t sessionId)
{
    return CreateIoCompletionPort(reinterpret_cast<HANDLE>(socket), m_hIOCP,
        static_cast<ULONG_PTR>(sessionId), 0) != nullptr;
}

void CIOCPCore::WorkerLoop(u32_t workerId)
{
    (void)workerId;

    while (m_bRunning)
    {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* pOverlapped = nullptr;

        const BOOL ok = GetQueuedCompletionStatus(m_hIOCP, &bytes, &key, &pOverlapped, INFINITE);

        if (pOverlapped == nullptr)
        {
            if (!m_bRunning)
                break;
            continue;
        }

        IOContext* ctx = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);

        if (!ok)
        {
            if (ctx->op != eIOOp::Accept)
                CSession_Manager::Get()->OnDisconnect(ctx->sessionId);

            if (ctx->op == eIOOp::Accept && ctx->acceptSocket != INVALID_SOCKET)
                ::closesocket(ctx->acceptSocket);

            delete ctx;
            continue;
        }

        switch (ctx->op)
        {
        case eIOOp::Accept:
        {
            ::setsockopt(ctx->acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                reinterpret_cast<char*>(&m_listenSocket), sizeof(m_listenSocket));

            BOOL on = TRUE;
            ::setsockopt(ctx->acceptSocket, IPPROTO_TCP, TCP_NODELAY,
                reinterpret_cast<const char*>(&on), sizeof(on));

            auto session = CSession_Manager::Get()->OnAccept(ctx->acceptSocket, sockaddr_in{});
            if (session)
            {
                if (BindIOCP(session->GetSocket(), session->GetSessionId()))
                {
                    std::cout << "[IOCP] Accept sid=" << session->GetSessionId() << "\n";
                    session->PostInitialRecv();

                    if (g_pRoom)
                    {
                        const EntityID controlled = g_pRoom->OnSessionJoin(session->GetSessionId());
                        session->SetControlledEntity(controlled);
                    }
                }
                else
                {
                    CSession_Manager::Get()->OnDisconnect(session->GetSessionId());
                }
            }
            else
            {
                ::closesocket(ctx->acceptSocket);
            }

            ctx->acceptSocket = INVALID_SOCKET;
            if (m_bRunning && !PostAccept(m_listenSocket, ctx))
                delete ctx;
            break;
        }
        case eIOOp::Recv:
            if (bytes == 0)
            {
                CSession_Manager::Get()->OnDisconnect(ctx->sessionId);
                break;
            }

            CSession_Manager::Get()->OnRecvComplete(ctx->sessionId,
                reinterpret_cast<const u8_t*>(ctx->buffer), bytes);
            break;
        case eIOOp::Send:
            CSession_Manager::Get()->OnSendComplete(ctx->sessionId, bytes);
            break;
        default:
            break;
        }
    }
}

void CIOCPCore::AcceptLoop()
{
}
