#pragma once

#include "WintersTypes.h"

#include <WinSock2.h>
#include <MSWSock.h>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

enum class eIOOp : u8_t
{
    Accept,
    Recv,
    Send
};

struct IOContext
{
    OVERLAPPED overlapped{};
    WSABUF wsaBuf{};
    char buffer[8192]{};
    eIOOp op = eIOOp::Recv;
    u32_t sessionId = 0;
    SOCKET acceptSocket = INVALID_SOCKET;
    char acceptBuffer[2 * (sizeof(sockaddr_in) + 16)]{};
};

class CIOCPCore final
{
public:
    static std::unique_ptr<CIOCPCore> Create(u16_t port, u32_t workerCount);
    ~CIOCPCore();

    bool Start();
    void Shutdown();

    HANDLE GetCompletionPort() const { return m_hIOCP; }

private:
    CIOCPCore(u16_t port, u32_t workerCount);

    void WorkerLoop(u32_t workerId);
    void AcceptLoop();

    bool PostAccept(SOCKET listenSocket, IOContext* ctx);
    bool BindIOCP(SOCKET socket, u32_t sessionId);

    HANDLE m_hIOCP = nullptr;
    SOCKET m_listenSocket = INVALID_SOCKET;
    u16_t m_port = 0;
    u32_t m_workerCount = 0;

    std::vector<std::thread> m_workers;
    std::thread m_acceptThread;
    std::atomic<bool> m_bRunning{ false };

    LPFN_ACCEPTEX m_pfnAcceptEx = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS m_pfnGetAcceptExSockaddrs = nullptr;
};
