# Phase 04a v2 — D-1 Sub-plan: Server Transport (TCP IOCP)

**작성일**: 2026-04-30
**상위 문서**: `04a_MVP_2CLIENT_TCP_DEMO_v2.md` §4
**범위**: D-1A~D-1J Server 측 본격 — IOCPCore/FrameParser/Session/Manager/Dispatcher/GameRoom/SnapshotBuilder/main + Hello packet
**합격**: 1 client connect → CommandBatch send → server tick → full snapshot broadcast → 5분 jitter < 5ms

**한 줄**: **TCP IOCP transport-aware 6개 (IOCPCore/FrameParser/Session/Manager/Dispatcher) + transport-neutral GameRoom/SnapshotBuilder/CommandExecutor 본격 박제. UDP M1 마이그 시 transport-aware 만 갈아끼움.**

---

## 1. D-1A — Server.vcxproj Engine project reference (1h)

### 1.1 추가 XML

`Server/Include/Server.vcxproj` 의 `<ItemGroup>` (`ProjectReference` 그룹) 추가:

```xml
<ItemGroup>
  <ProjectReference Include="..\..\Engine\Include\Engine.vcxproj">
    <Project>{ENGINE_PROJECT_GUID_HERE}</Project>
    <Private>true</Private>
    <ReferenceOutputAssembly>true</ReferenceOutputAssembly>
    <CopyLocalSatelliteAssemblies>true</CopyLocalSatelliteAssemblies>
    <LinkLibraryDependencies>true</LinkLibraryDependencies>
    <UseLibraryDependencyInputs>false</UseLibraryDependencyInputs>
  </ProjectReference>
</ItemGroup>
```

(GUID 는 Engine.vcxproj 안 `<ProjectGuid>` 값 복사)

### 1.2 합격
- `#include "ECS/World.h"` + `world.CreateEntity()` 컴파일/링크 OK

---

## 2. D-1B — CIOCPCore 본격 (4h)

### 2.1 `Server/Public/Network/IOCPCore.h` (수정 — typo + acceptSocket)

```cpp
#pragma once
#include "WintersTypes.h"
#include <WinSock2.h>
#include <MSWSock.h>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

enum class eIOOp : u8_t { Accept, Recv, Send };

struct IOContext
{
    OVERLAPPED overlapped{};
    WSABUF     wsaBuf{};
    char       buffer[8192]{};
    eIOOp      op = eIOOp::Recv;
    u32_t      sessionId = 0;
    SOCKET     acceptSocket = INVALID_SOCKET;   // ★ AcceptEx 수명 보장
    char       acceptBuffer[2 * (sizeof(sockaddr_in) + 16)]{};   // AcceptEx addr 버퍼
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
    u16_t  m_port;
    u32_t  m_workerCount;

    std::vector<std::thread> m_workers;
    std::thread              m_acceptThread;
    std::atomic<bool>        m_bRunning{ false };

    LPFN_ACCEPTEX m_pfnAcceptEx = nullptr;
    LPFN_GETACCEPTEXSOCKADDRS m_pfnGetAcceptExSockaddrs = nullptr;
};
```

### 2.2 `Server/Private/Network/IOCPCore.cpp` (전문 ~250줄)

```cpp
#include "Network/IOCPCore.h"
#include "Network/Session_Manager.h"
#include "Network/Session.h"

#include <iostream>
#include <ws2tcpip.h>

std::unique_ptr<CIOCPCore> CIOCPCore::Create(u16_t port, u32_t workerCount)
{
    return std::unique_ptr<CIOCPCore>(new CIOCPCore(port, workerCount));
}

CIOCPCore::CIOCPCore(u16_t port, u32_t workerCount)
    : m_port(port), m_workerCount(workerCount)
{
}

CIOCPCore::~CIOCPCore()
{
    Shutdown();
}

bool CIOCPCore::Start()
{
    // 1. IOCP 핸들 생성
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, m_workerCount);
    if (!m_hIOCP) return false;

    // 2. listen socket 생성
    m_listenSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (m_listenSocket == INVALID_SOCKET) return false;

    BOOL on = TRUE;
    setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    setsockopt(m_listenSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&on, sizeof(on));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);
    if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
        return false;
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR)
        return false;

    // 3. listen socket 을 IOCP 에 등록
    if (!CreateIoCompletionPort((HANDLE)m_listenSocket, m_hIOCP, 0, 0))
        return false;

    // 4. AcceptEx / GetAcceptExSockaddrs 함수 포인터 획득
    GUID guidAcceptEx = WSAID_ACCEPTEX;
    GUID guidGetAddrs = WSAID_GETACCEPTEXSOCKADDRS;
    DWORD bytes = 0;
    WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidAcceptEx, sizeof(guidAcceptEx), &m_pfnAcceptEx, sizeof(m_pfnAcceptEx),
        &bytes, nullptr, nullptr);
    WSAIoctl(m_listenSocket, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidGetAddrs, sizeof(guidGetAddrs), &m_pfnGetAcceptExSockaddrs, sizeof(m_pfnGetAcceptExSockaddrs),
        &bytes, nullptr, nullptr);

    // 5. worker thread 시작
    m_bRunning = true;
    for (u32_t i = 0; i < m_workerCount; ++i)
        m_workers.emplace_back(&CIOCPCore::WorkerLoop, this, i);

    // 6. accept 미리 N 개 예약 (back pressure)
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
    if (!m_bRunning.exchange(false)) return;

    // worker 깨우기
    for (u32_t i = 0; i < m_workerCount; ++i)
        PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr);

    if (m_listenSocket != INVALID_SOCKET)
    {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    for (auto& t : m_workers)
        if (t.joinable()) t.join();
    m_workers.clear();

    if (m_hIOCP)
    {
        CloseHandle(m_hIOCP);
        m_hIOCP = nullptr;
    }
}

bool CIOCPCore::PostAccept(SOCKET listenSocket, IOContext* ctx)
{
    ctx->acceptSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (ctx->acceptSocket == INVALID_SOCKET) return false;

    DWORD bytes = 0;
    BOOL ok = m_pfnAcceptEx(listenSocket, ctx->acceptSocket,
        ctx->acceptBuffer, 0,
        sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16,
        &bytes, &ctx->overlapped);

    if (!ok && WSAGetLastError() != ERROR_IO_PENDING)
    {
        closesocket(ctx->acceptSocket);
        ctx->acceptSocket = INVALID_SOCKET;
        return false;
    }
    return true;
}

bool CIOCPCore::BindIOCP(SOCKET socket, u32_t sessionId)
{
    return CreateIoCompletionPort((HANDLE)socket, m_hIOCP,
        (ULONG_PTR)sessionId, 0) != nullptr;
}

void CIOCPCore::WorkerLoop(u32_t workerId)
{
    (void)workerId;

    while (m_bRunning)
    {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* pOverlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(m_hIOCP, &bytes, &key, &pOverlapped, INFINITE);

        // shutdown signal
        if (pOverlapped == nullptr)
        {
            if (!m_bRunning) break;
            continue;
        }

        IOContext* ctx = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);

        if (!ok)
        {
            // 실패 — session 정리 후 ctx 회수
            if (ctx->op != eIOOp::Accept)
                CSession_Manager::Get()->OnDisconnect(ctx->sessionId);

            if (ctx->op == eIOOp::Accept && ctx->acceptSocket != INVALID_SOCKET)
                closesocket(ctx->acceptSocket);

            delete ctx;
            continue;
        }

        switch (ctx->op)
        {
            case eIOOp::Accept:
            {
                // SO_UPDATE_ACCEPT_CONTEXT — listen socket 의 옵션을 accepted socket 으로 복사
                setsockopt(ctx->acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                    (char*)&m_listenSocket, sizeof(m_listenSocket));

                auto session = CSession_Manager::Get()->OnAccept(ctx->acceptSocket, sockaddr_in{});
                if (session)
                {
                    if (BindIOCP(session->GetSocket(), session->GetSessionId()))
                        session->PostInitialRecv();
                    else
                        CSession_Manager::Get()->OnDisconnect(session->GetSessionId());
                }
                else
                {
                    closesocket(ctx->acceptSocket);
                }

                ctx->acceptSocket = INVALID_SOCKET;

                // 다음 accept 예약 (재사용)
                if (!PostAccept(m_listenSocket, ctx))
                    delete ctx;
                break;
            }
            case eIOOp::Recv:
            {
                if (bytes == 0)
                {
                    CSession_Manager::Get()->OnDisconnect(ctx->sessionId);
                    break;
                }
                CSession_Manager::Get()->OnRecvComplete(ctx->sessionId,
                    reinterpret_cast<const u8_t*>(ctx->buffer), bytes);
                // 다음 PostRecv 는 Session 내부에서 예약
                break;
            }
            case eIOOp::Send:
            {
                CSession_Manager::Get()->OnSendComplete(ctx->sessionId, bytes);
                break;
            }
        }
    }
}

void CIOCPCore::AcceptLoop()
{
    // PostAccept 가 IOCP 안에서 자동 재예약되므로 별도 thread 불필요.
    // 본 함수는 향후 별도 정책 (rate limit 등) 시 활성화.
}
```

---

## 3. D-1C — CFrameParser TryPop (2h)

### 3.1 `Server/Public/Network/FrameParser.h` (전문)

```cpp
#pragma once
#include "Shared/Network/PacketEnvelope.h"
#include "WintersTypes.h"
#include <vector>

enum class eFrameParseResult : u8_t
{
    NeedMore = 0,
    Complete = 1,
    Invalid  = 2,
};

struct ParsedFrameOwned
{
    ePacketType       type = ePacketType::None;
    u32_t             sequence = 0;
    std::vector<u8_t> payload;
};

class CFrameParser
{
public:
    void              Append(const u8_t* bytes, u32_t len);
    eFrameParseResult TryPop(ParsedFrameOwned& outFrame);
    u32_t             BufferedBytes() const { return static_cast<u32_t>(m_Buffer.size()); }
    void              Clear();

private:
    static constexpr u32_t kMaxPayloadBytes = 64 * 1024;
    static constexpr u32_t kMaxBufferBytes  = 256 * 1024;

    std::vector<u8_t> m_Buffer;
};
```

### 3.2 `Server/Private/Network/FrameParser.cpp` (전문)

```cpp
#include "Network/FrameParser.h"

#include <cstring>

void CFrameParser::Append(const u8_t* bytes, u32_t len)
{
    if (bytes == nullptr || len == 0) return;
    m_Buffer.insert(m_Buffer.end(), bytes, bytes + len);
    if (m_Buffer.size() > kMaxBufferBytes)
        Clear();
}

eFrameParseResult CFrameParser::TryPop(ParsedFrameOwned& outFrame)
{
    outFrame = {};

    if (m_Buffer.size() < sizeof(PacketHeader))
        return eFrameParseResult::NeedMore;

    PacketHeader hdr{};
    std::memcpy(&hdr, m_Buffer.data(), sizeof(PacketHeader));

    if (hdr.magic != kPacketMagic || hdr.version != kPacketVersion)
    {
        Clear();
        return eFrameParseResult::Invalid;
    }
    if (hdr.payloadSize > kMaxPayloadBytes)
    {
        Clear();
        return eFrameParseResult::Invalid;
    }

    const u32_t totalSize = sizeof(PacketHeader) + hdr.payloadSize;
    if (m_Buffer.size() < totalSize)
        return eFrameParseResult::NeedMore;

    outFrame.type = static_cast<ePacketType>(hdr.type);
    outFrame.sequence = hdr.sequence;
    outFrame.payload.assign(
        m_Buffer.data() + sizeof(PacketHeader),
        m_Buffer.data() + totalSize);

    m_Buffer.erase(m_Buffer.begin(), m_Buffer.begin() + totalSize);
    return eFrameParseResult::Complete;
}

void CFrameParser::Clear()
{
    m_Buffer.clear();
}
```

---

## 4. D-1D — CSession 본격 (5h)

### 4.1 `Server/Public/Network/Session.h` (전문)

```cpp
#pragma once
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "Network/IOCPCore.h"
#include "Network/FrameParser.h"
#include <WinSock2.h>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_set>
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

    u32_t    GetSessionId() const             { return m_sessionId; }
    SOCKET   GetSocket() const                { return m_socket; }
    EntityID GetControlledEntity() const      { return m_controlledEntity; }
    void     SetControlledEntity(EntityID e)  { m_controlledEntity = e; }

    bool TryAcceptSequence(u32_t seq, bool& bSuspicious);

    void FlagSuspicious()       { ++m_suspicionCount; }
    bool IsSuspicious() const   { return m_suspicionCount > 5; }

    void AddPendingIo()         { m_pendingIoCount.fetch_add(1, std::memory_order_relaxed); }
    void CompletePendingIo()    { m_pendingIoCount.fetch_sub(1, std::memory_order_relaxed); }
    bool CanDestroy() const     { return m_bClosing && m_pendingIoCount.load() == 0; }

    CFrameParser& GetRecvParser() { return m_recvParser; }

private:
    CSession(SOCKET socket, u32_t sessionId);

    SOCKET   m_socket = INVALID_SOCKET;
    u32_t    m_sessionId = 0;
    EntityID m_controlledEntity = NULL_ENTITY;

    mutable std::mutex m_seqMutex;
    u32_t              m_lastProcessedSeq = 0;
    u32_t              m_suspicionCount = 0;

    std::atomic<u32_t> m_pendingIoCount{ 0 };
    std::atomic<bool>  m_bClosing{ false };

    CFrameParser m_recvParser;

    IOContext m_recvContext{};
    IOContext m_sendContext{};
    char      m_recvBuffer[8192]{};

    std::mutex                    m_sendMutex;
    std::deque<std::vector<u8_t>> m_sendQueue;
    bool                          m_bSendPending = false;
};
```

### 4.2 `Server/Private/Network/Session.cpp` (전문 — 핵심부)

```cpp
#include "Network/Session.h"
#include "Network/Session_Manager.h"
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

    if (seq <= m_lastProcessedSeq) return false;
    if (seq > m_lastProcessedSeq + 60)
    {
        bSuspicious = true;
        return false;
    }
    m_lastProcessedSeq = seq;
    return true;
}

bool CSession::PostInitialRecv() { return PostRecv(); }

bool CSession::PostRecv()
{
    if (m_bClosing || m_socket == INVALID_SOCKET) return false;

    m_recvContext.wsaBuf.buf = m_recvBuffer;
    m_recvContext.wsaBuf.len = static_cast<ULONG>(sizeof(m_recvBuffer));
    ZeroMemory(&m_recvContext.overlapped, sizeof(OVERLAPPED));

    DWORD flags = 0;
    DWORD bytes = 0;
    AddPendingIo();
    int result = WSARecv(m_socket, &m_recvContext.wsaBuf, 1,
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
    if (len == 0) { OnDisconnect(); return; }

    m_recvParser.Append(bytes, len);
    CPacketDispatcher::Instance().DrainFrames(m_sessionId, m_recvParser);

    if (!m_bClosing)
        if (!PostRecv()) OnDisconnect();
}

bool CSession::Send(std::vector<u8_t> packet)
{
    if (m_bClosing || m_socket == INVALID_SOCKET || packet.empty()) return false;

    std::lock_guard lk(m_sendMutex);
    m_sendQueue.push_back(std::move(packet));
    if (m_bSendPending) return true;

    m_bSendPending = true;
    auto& front = m_sendQueue.front();
    m_sendContext.wsaBuf.buf = reinterpret_cast<char*>(front.data());
    m_sendContext.wsaBuf.len = static_cast<ULONG>(front.size());
    ZeroMemory(&m_sendContext.overlapped, sizeof(OVERLAPPED));

    DWORD sent = 0;
    AddPendingIo();
    int result = WSASend(m_socket, &m_sendContext.wsaBuf, 1,
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
    if (!m_sendQueue.empty()) m_sendQueue.pop_front();
    if (m_sendQueue.empty()) { m_bSendPending = false; return; }

    auto& front = m_sendQueue.front();
    m_sendContext.wsaBuf.buf = reinterpret_cast<char*>(front.data());
    m_sendContext.wsaBuf.len = static_cast<ULONG>(front.size());
    ZeroMemory(&m_sendContext.overlapped, sizeof(OVERLAPPED));

    DWORD sent = 0;
    AddPendingIo();
    int result = WSASend(m_socket, &m_sendContext.wsaBuf, 1,
        &sent, 0, &m_sendContext.overlapped, nullptr);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        CompletePendingIo();
        m_bSendPending = false;
    }
}

void CSession::OnDisconnect()
{
    m_bClosing = true;
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}
```

---

## 5. D-1E — CSession_Manager (3h)

### 5.1 `Server/Public/Network/Session_Manager.h` (전문)

```cpp
#pragma once
#include "Network/Session.h"
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <vector>

class CSession_Manager
{
public:
    static CSession_Manager* Get();

    std::shared_ptr<CSession> OnAccept(SOCKET clientSocket, const sockaddr_in& addr);
    void OnDisconnect(u32_t sessionId);
    void OnRecvComplete(u32_t sessionId, const u8_t* bytes, u32_t len);
    void OnSendComplete(u32_t sessionId, u32_t bytes);

    std::shared_ptr<CSession> Find(u32_t sessionId);
    void   ForEach(const std::function<void(CSession&)>& fn);
    size_t Count() const;

private:
    CSession_Manager() = default;
    CSession_Manager(const CSession_Manager&) = delete;
    CSession_Manager& operator=(const CSession_Manager&) = delete;

    void ReapClosingSessions();

    std::unordered_map<u32_t, std::shared_ptr<CSession>> m_sessions;       // allowlist (sim 외 lookup)
    std::vector<std::shared_ptr<CSession>>               m_closingSessions;
    std::atomic<u32_t>                                   m_nextSessionId{ 1 };
    mutable std::mutex                                   m_mutex;
};
```

### 5.2 `Server/Private/Network/Session_Manager.cpp` (전문 — 핵심)

```cpp
#include "Network/Session_Manager.h"
#include "Game/GameRoom.h"   // OnSessionJoin/Leave 호출용
#include "Network/PacketDispatcher.h"
#include <algorithm>

CSession_Manager* CSession_Manager::Get()
{
    static CSession_Manager s_inst;
    return &s_inst;
}

std::shared_ptr<CSession> CSession_Manager::OnAccept(SOCKET clientSocket, const sockaddr_in& addr)
{
    (void)addr;
    const u32_t newSid = m_nextSessionId.fetch_add(1, std::memory_order_relaxed);

    auto session = CSession::Create(clientSocket, newSid);
    if (!session) { closesocket(clientSocket); return nullptr; }

    {
        std::lock_guard lk(m_mutex);
        m_sessions[newSid] = session;
    }

    // ★ Hello → Room join 은 PacketDispatcher 또는 GameRoom 측에서 처리 (D-1J)
    return session;
}

void CSession_Manager::OnDisconnect(u32_t sessionId)
{
    std::shared_ptr<CSession> session;
    {
        std::lock_guard lk(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return;
        session = std::move(it->second);
        m_sessions.erase(it);
        m_closingSessions.push_back(session);
    }
    session->OnDisconnect();
    ReapClosingSessions();
}

void CSession_Manager::OnRecvComplete(u32_t sessionId, const u8_t* bytes, u32_t len)
{
    auto pSession = Find(sessionId);
    if (pSession) pSession->OnRecvComplete(bytes, len);
    ReapClosingSessions();
}

void CSession_Manager::OnSendComplete(u32_t sessionId, u32_t bytes)
{
    auto pSession = Find(sessionId);
    if (pSession) pSession->OnSendComplete(bytes);
    ReapClosingSessions();
}

std::shared_ptr<CSession> CSession_Manager::Find(u32_t sessionId)
{
    std::lock_guard lk(m_mutex);
    auto it = m_sessions.find(sessionId);
    if (it != m_sessions.end()) return it->second;
    for (auto& closing : m_closingSessions)
        if (closing && closing->GetSessionId() == sessionId) return closing;
    return nullptr;
}

void CSession_Manager::ForEach(const std::function<void(CSession&)>& fn)
{
    std::vector<u32_t> ids;
    {
        std::lock_guard lk(m_mutex);
        ids.reserve(m_sessions.size());
        for (auto& [sid, _] : m_sessions) ids.push_back(sid);
    }
    std::sort(ids.begin(), ids.end());   // ★ 결정성

    for (u32_t sid : ids)
    {
        auto pSession = Find(sid);
        if (pSession) fn(*pSession);
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
            [](const std::shared_ptr<CSession>& s) { return !s || s->CanDestroy(); }),
        m_closingSessions.end());
}
```

---

## 6. D-1F — CPacketDispatcher (3h)

### 6.1 `Server/Public/Network/PacketDispatcher.h` (전문)

```cpp
#pragma once
#include "WintersTypes.h"
#include "Network/FrameParser.h"
#include "Shared/Network/PacketEnvelope.h"
#include <unordered_map>
#include <mutex>

class CGameRoom;   // forward

class CPacketDispatcher
{
public:
    static CPacketDispatcher& Instance();

    void DrainFrames(u32_t sessionId, CFrameParser& parser);
    void DispatchCommandBatch(u32_t sessionId, const ParsedFrameOwned& frame);
    void DispatchHello(u32_t sessionId, const ParsedFrameOwned& frame);

    void RegisterRoom(u32_t roomId, CGameRoom* pRoom);
    void RouteSession(u32_t sessionId, u32_t roomId);

private:
    CPacketDispatcher() = default;
    CPacketDispatcher(const CPacketDispatcher&) = delete;
    CPacketDispatcher& operator=(const CPacketDispatcher&) = delete;

    std::mutex                          m_mutex;
    std::unordered_map<u32_t, CGameRoom*> m_rooms;
    std::unordered_map<u32_t, u32_t>      m_sessionToRoom;
};
```

### 6.2 `Server/Private/Network/PacketDispatcher.cpp` (전문)

```cpp
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Game/GameRoom.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
#include <flatbuffers/flatbuffers.h>

CPacketDispatcher& CPacketDispatcher::Instance()
{
    static CPacketDispatcher s_inst;
    return s_inst;
}

void CPacketDispatcher::RegisterRoom(u32_t roomId, CGameRoom* pRoom)
{
    std::lock_guard lk(m_mutex);
    m_rooms[roomId] = pRoom;
}

void CPacketDispatcher::RouteSession(u32_t sessionId, u32_t roomId)
{
    std::lock_guard lk(m_mutex);
    m_sessionToRoom[sessionId] = roomId;
}

void CPacketDispatcher::DrainFrames(u32_t sessionId, CFrameParser& parser)
{
    ParsedFrameOwned frame{};
    for (;;)
    {
        const eFrameParseResult result = parser.TryPop(frame);
        if (result == eFrameParseResult::NeedMore) return;
        if (result == eFrameParseResult::Invalid)
        {
            CSession_Manager::Get()->OnDisconnect(sessionId);
            return;
        }

        switch (frame.type)
        {
            case ePacketType::CommandBatch:
                DispatchCommandBatch(sessionId, frame);
                break;
            case ePacketType::Hello:
                DispatchHello(sessionId, frame);
                break;
            case ePacketType::Heartbeat:
                // no-op
                break;
            default:
                if (auto pSession = CSession_Manager::Get()->Find(sessionId))
                    pSession->FlagSuspicious();
                break;
        }
    }
}

void CPacketDispatcher::DispatchCommandBatch(u32_t sessionId, const ParsedFrameOwned& frame)
{
    flatbuffers::Verifier verifier(frame.payload.data(), frame.payload.size());
    if (!Shared::Schema::VerifyCommandBatchBuffer(verifier))
    {
        if (auto pSession = CSession_Manager::Get()->Find(sessionId))
            pSession->FlagSuspicious();
        return;
    }

    const auto* batch = Shared::Schema::GetCommandBatch(frame.payload.data());

    CGameRoom* pRoom = nullptr;
    {
        std::lock_guard lk(m_mutex);
        auto rit = m_sessionToRoom.find(sessionId);
        if (rit == m_sessionToRoom.end()) return;
        auto pit = m_rooms.find(rit->second);
        if (pit == m_rooms.end()) return;
        pRoom = pit->second;
    }
    if (pRoom) pRoom->OnCommandBatch(sessionId, batch);
}

void CPacketDispatcher::DispatchHello(u32_t sessionId, const ParsedFrameOwned& frame)
{
    // ★ Hello 는 Client → Server 가 아님. Server → Client 일방 송신 (D-1J).
    //   여기서는 Hello 수신 시 단순 무시 (혹은 ack 처리 — 향후)
    (void)sessionId;
    (void)frame;
}
```

---

## 7. D-1G — CGameRoom 본격 (6h, ★ 핵심)

### 7.1 `Server/Public/Game/GameRoom.h` (전문)

```cpp
#pragma once
#include "WintersTypes.h"
#include "ECS/World.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/EntityIdMap.h"
#include "Shared/GameSim/DeterministicRng.h"
#include "Shared/GameSim/DeterministicTime.h"
#include "Shared/GameSim/Systems/ICommandExecutor.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_map>

namespace flatbuffers { class DetachedBuffer; }
namespace Shared::Schema { struct CommandBatch; }

class CSession;
class CSnapshotBuilder;

struct PendingCommand
{
    u32_t           sessionId = 0;
    u32_t           sequenceNum = 0;
    GameCommandWire wire{};
    u64_t           acceptedTick = 0;
    u64_t           recvTimeMs = 0;
};

class CGameRoom final
{
public:
    static std::unique_ptr<CGameRoom> Create(u32_t roomId);
    ~CGameRoom();

    void Start();
    void Stop();

    // Worker thread → Tick 큐 push
    void OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch);

    // Session 관리
    EntityID OnSessionJoin(u32_t sessionId);
    void     OnSessionLeave(u32_t sessionId);

    u32_t GetRoomId() const                   { return m_roomId; }
    u64_t GetCurrentTickIndex() const         { return m_visibleTickIndex.load(std::memory_order_relaxed); }
    bool  IsRunning() const                   { return m_bRunning; }

private:
    CGameRoom(u32_t roomId);

    void TickThread();
    void Tick();

    void Phase_DrainCommands(TickContext& tc);
    void Phase_ExecuteCommands(TickContext& tc);
    void Phase_SimulationSystems(TickContext& tc);
    void Phase_BroadcastSnapshot(TickContext& tc);

    EntityID SpawnChampion(u32_t sessionId);

    u32_t              m_roomId;
    std::atomic<bool>  m_bRunning{ false };
    std::thread        m_tickThread;

    CWorld             m_world;
    EntityIdMap        m_entityMap;
    DeterministicRng   m_rng{ 0xC0FFEEull };
    u64_t              m_tickIndex = 0;
    std::atomic<u64_t> m_visibleTickIndex{ 0 };

    std::unique_ptr<ICommandExecutor> m_pExecutor;
    std::unique_ptr<CSnapshotBuilder> m_pSnapBuilder;

    std::mutex                  m_pendingMutex;
    std::vector<PendingCommand> m_pendingCommands;
    std::vector<GameCommand>    m_pendingExecCommands;

    std::vector<u32_t>                  m_sessionIds;          // sorted (결정성)
    std::unordered_map<u32_t, EntityID> m_sessionToEntity;     // allowlist (sim 외 lookup)
};
```

### 7.2 `Server/Private/Game/GameRoom.cpp` (전문 — 핵심)

```cpp
#include "Game/GameRoom.h"
#include "Game/SnapshotBuilder.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/Network/PacketEnvelope.h"
#include "Shared/Schemas/Generated/cpp/Command_generated.h"
#include "Shared/GameSim/Systems/StatSystem.h"
#include "Shared/GameSim/Systems/BuffSystem.h"
#include "Shared/GameSim/Systems/MoveSystem.h"
#include "Shared/GameSim/Systems/SkillCooldownSystem.h"
#include "Shared/GameSim/Systems/DamageQueueSystem.h"
#include "Shared/GameSim/Systems/DeathSystem.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "ECS/Components/TransformComponent.h"

#include <algorithm>
#include <chrono>

std::unique_ptr<CGameRoom> CGameRoom::Create(u32_t roomId)
{
    auto room = std::unique_ptr<CGameRoom>(new CGameRoom(roomId));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    return room;
}

CGameRoom::CGameRoom(u32_t roomId) : m_roomId(roomId) {}

CGameRoom::~CGameRoom() { Stop(); }

void CGameRoom::Start()
{
    if (m_bRunning.exchange(true)) return;
    m_tickThread = std::thread(&CGameRoom::TickThread, this);
}

void CGameRoom::Stop()
{
    if (!m_bRunning.exchange(false)) return;
    if (m_tickThread.joinable()) m_tickThread.join();
}

void CGameRoom::TickThread()
{
    using clock = std::chrono::steady_clock;   // ★ wall-clock 은 sim logic 에 미주입
    auto next = clock::now();
    const auto period = std::chrono::microseconds(33333);   // 30 Hz

    while (m_bRunning)
    {
        Tick();
        next += period;
        std::this_thread::sleep_until(next);
    }
}

void CGameRoom::Tick()
{
    ++m_tickIndex;
    m_visibleTickIndex.store(m_tickIndex, std::memory_order_relaxed);

    TickContext tc{
        m_tickIndex,
        DeterministicTime::kFixedDt,
        DeterministicTime::TickToSec(m_tickIndex),
        &m_rng,
        &m_entityMap,
        NULL_ENTITY
    };

    Phase_DrainCommands(tc);
    Phase_ExecuteCommands(tc);
    Phase_SimulationSystems(tc);
    Phase_BroadcastSnapshot(tc);
}

void CGameRoom::Phase_DrainCommands(TickContext& tc)
{
    std::vector<PendingCommand> drained;
    {
        std::lock_guard lk(m_pendingMutex);
        drained.swap(m_pendingCommands);
    }

    // ★ 결정성 — stable_sort + tie-breaker (acceptedTick, sessionId, sequenceNum)
    std::stable_sort(drained.begin(), drained.end(),
        [](const PendingCommand& a, const PendingCommand& b) {
            if (a.acceptedTick != b.acceptedTick) return a.acceptedTick < b.acceptedTick;
            if (a.sessionId    != b.sessionId)    return a.sessionId    < b.sessionId;
            return a.sequenceNum < b.sequenceNum;
        });

    for (auto& pc : drained)
    {
        auto it = m_sessionToEntity.find(pc.sessionId);
        if (it == m_sessionToEntity.end() || it->second == NULL_ENTITY) continue;

        GameCommand cmd = BuildServerCommand(pc.wire, pc.sessionId, it->second, m_entityMap);
        cmd.issuedAtTick = tc.tickIndex;
        m_pendingExecCommands.push_back(cmd);
    }
}

void CGameRoom::Phase_ExecuteCommands(TickContext& tc)
{
    for (const auto& cmd : m_pendingExecCommands)
        m_pExecutor->ExecuteCommand(m_world, tc, cmd);
    m_pendingExecCommands.clear();
}

void CGameRoom::Phase_SimulationSystems(TickContext& tc)
{
    // ★ DAG 고정 순서
    CStatSystem::Execute(m_world, tc);
    CBuffSystem::Execute(m_world, tc);
    CSkillCooldownSystem::Execute(m_world, tc);
    CMoveSystem::Execute(m_world, tc);
    CDamageQueueSystem::Execute(m_world, tc);   // Layer 1 = no-op
    CDeathSystem::Execute(m_world, tc);
}

void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession) continue;

        auto it = m_sessionToEntity.find(sid);
        if (it == m_sessionToEntity.end() || it->second == NULL_ENTITY) continue;

        auto buf = m_pSnapBuilder->Build(
            m_world, m_entityMap,
            tc.tickIndex, m_rng.GetState(),
            /*lastAckedSeq=*/0,
            m_entityMap.ToNet(it->second));

        auto wrapped = WrapEnvelope(
            ePacketType::Snapshot, static_cast<u32_t>(tc.tickIndex),
            buf.data(), static_cast<u32_t>(buf.size()));
        pSession->Send(std::move(wrapped));
    }
}

void CGameRoom::OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch)
{
    if (!batch || !batch->commands()) return;

    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (!pSession) return;

    const u64_t acceptedTick = GetCurrentTickIndex();
    const u64_t recvMs = static_cast<u64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::lock_guard lk(m_pendingMutex);
    for (const auto* cmd : *batch->commands())
    {
        const u32_t seq = cmd->sequenceNum();
        bool bSuspicious = false;
        if (!pSession->TryAcceptSequence(seq, bSuspicious))
        {
            if (bSuspicious) pSession->FlagSuspicious();
            continue;
        }

        PendingCommand pc{};
        pc.sessionId        = sessionId;
        pc.sequenceNum      = seq;
        pc.wire.kind        = static_cast<eCommandKind>(cmd->kind());
        pc.wire.clientTick  = cmd->clientTick();
        pc.wire.sequenceNum = seq;
        pc.wire.slot        = cmd->slot();
        pc.wire.targetNet   = cmd->targetNet();
        if (auto* g = cmd->groundPos()) pc.wire.groundPos = { g->x(), g->y(), g->z() };
        if (auto* d = cmd->direction()) pc.wire.direction = { d->x(), d->y(), d->z() };
        pc.wire.itemId       = cmd->itemId();
        pc.acceptedTick      = acceptedTick;
        pc.recvTimeMs        = recvMs;
        m_pendingCommands.push_back(std::move(pc));
    }
}

EntityID CGameRoom::OnSessionJoin(u32_t sessionId)
{
    EntityID e = SpawnChampion(sessionId);
    NetEntityId netId = m_entityMap.IssueNew(e);

    m_sessionToEntity[sessionId] = e;
    if (std::find(m_sessionIds.begin(), m_sessionIds.end(), sessionId) == m_sessionIds.end())
        m_sessionIds.push_back(sessionId);
    std::sort(m_sessionIds.begin(), m_sessionIds.end());

    (void)netId;
    return e;
}

void CGameRoom::OnSessionLeave(u32_t sessionId)
{
    auto it = m_sessionToEntity.find(sessionId);
    if (it != m_sessionToEntity.end())
    {
        // ★ entity 삭제는 Layer 2 에서 (현재는 entity 유지 + bIsDead)
        m_sessionToEntity.erase(it);
    }
    m_sessionIds.erase(
        std::remove(m_sessionIds.begin(), m_sessionIds.end(), sessionId),
        m_sessionIds.end());
}

EntityID CGameRoom::SpawnChampion(u32_t sessionId)
{
    EntityID e = m_world.CreateEntity();

    // 기본 컴포넌트 — Layer 1 minimal
    TransformComponent tf{};
    tf.SetPosition({ 27.f + (f32_t)(sessionId * 2.f), 1.f, 0.f });
    m_world.AddComponent<TransformComponent>(e, tf);

    StatComponent stat{};
    stat.hpMax     = 600.f;
    stat.manaMax   = 300.f;
    stat.moveSpeed = 5.f;
    m_world.AddComponent<StatComponent>(e, stat);

    // ★ Codex 보정 (D-0 P1-1 일관) — 실제 필드명 fCurrent / fMaximum / bIsDead
    HealthComponent hp{};
    hp.fCurrent = 600.f;
    hp.fMaximum = 600.f;
    m_world.AddComponent<HealthComponent>(e, hp);

    SkillStateComponent sk{};
    m_world.AddComponent<SkillStateComponent>(e, sk);

    SkillRankComponent rank{};
    m_world.AddComponent<SkillRankComponent>(e, rank);

    ChampionComponent champ{};
    champ.id   = eChampion::EZREAL;   // Layer 1 — 디폴트 이즈리얼
    champ.team = (sessionId % 2 == 0) ? eTeam::Blue : eTeam::Red;
    m_world.AddComponent<ChampionComponent>(e, champ);

    return e;
}
```

---

## 8. D-1H — CSnapshotBuilder (2h)

### 8.1 `Server/Public/Game/SnapshotBuilder.h` (전문, ★ 신규 — header 분리)

```cpp
#pragma once
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/EntityIdMap.h"
#include <flatbuffers/flatbuffers.h>
#include <memory>

class CWorld;

class CSnapshotBuilder final
{
public:
    static std::unique_ptr<CSnapshotBuilder> Create();

    // ★ Codex C3 — non-const CWorld&
    flatbuffers::DetachedBuffer Build(
        CWorld& world,
        const EntityIdMap& entityMap,
        u64_t serverTick,
        u64_t rngState,
        u32_t lastAckedSeq,
        NetEntityId yourNetId);

private:
    CSnapshotBuilder() = default;
};
```

### 8.2 `Server/Private/Game/SnapshotBuilder.cpp` (전문)

```cpp
#include "Game/SnapshotBuilder.h"

#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

#include <algorithm>
#include <vector>

std::unique_ptr<CSnapshotBuilder> CSnapshotBuilder::Create()
{
    return std::unique_ptr<CSnapshotBuilder>(new CSnapshotBuilder());
}

flatbuffers::DetachedBuffer CSnapshotBuilder::Build(
    CWorld& world,
    const EntityIdMap& entityMap,
    u64_t serverTick,
    u64_t rngState,
    u32_t lastAckedSeq,
    NetEntityId yourNetId)
{
    flatbuffers::FlatBufferBuilder fbb(2048);

    // ★ 결정성 — sorted EntityID 순회 후 NetEntityId 오름차순 정렬
    auto entities = DeterministicEntityIterator<TransformComponent>::CollectSorted(world);

    struct Tmp { NetEntityId net; EntityID e; };
    std::vector<Tmp> sorted;
    sorted.reserve(entities.size());
    for (EntityID e : entities)
    {
        NetEntityId net = entityMap.ToNet(e);
        if (net == NULL_NET_ENTITY) continue;
        sorted.push_back({ net, e });
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const Tmp& a, const Tmp& b) { return a.net < b.net; });

    // EntitySnapshot 직렬화
    std::vector<flatbuffers::Offset<Shared::Schema::EntitySnapshot>> esVec;
    esVec.reserve(sorted.size());

    for (const auto& [netId, e] : sorted)
    {
        const auto& tf = world.GetComponent<TransformComponent>(e);
        // ★ Codex 보정 (D-0 P1-3 일관) — phase chain 에 CTransformSystem 미포함이라
        //   GetWorldPosition() stale. root entity 가정으로 local 직접 사용.
        const Vec3 pos = tf.GetLocalPosition();

        f32_t hp = 0.f, mana = 0.f, moveSpeed = 0.f;
        u8_t championId = 0, team = 0, level = 1;
        if (world.HasComponent<HealthComponent>(e))
            hp = world.GetComponent<HealthComponent>(e).fCurrent;   // ★ Codex P1-1
        if (world.HasComponent<StatComponent>(e))
        {
            const auto& s = world.GetComponent<StatComponent>(e);
            mana = s.manaMax;
            moveSpeed = s.moveSpeed;
        }
        if (world.HasComponent<ChampionComponent>(e))
        {
            const auto& c = world.GetComponent<ChampionComponent>(e);
            championId = static_cast<u8_t>(c.id);
            team = static_cast<u8_t>(c.team);
        }

        // skillCooldowns / skillRanks — Layer 2 에서 추가, Layer 1 은 빈 배열
        std::vector<float> cdVec;
        std::vector<uint8_t> rkVec;

        auto cdOffset = fbb.CreateVector(cdVec);
        auto rkOffset = fbb.CreateVector(rkVec);

        esVec.push_back(Shared::Schema::CreateEntitySnapshot(
            fbb,
            netId,
            championId, team, level,
            hp, mana,
            pos.x, pos.y, pos.z,
            /*yaw=*/0.f,
            moveSpeed,
            /*animId=*/0, /*animPhaseFrame=*/0,
            cdOffset, rkOffset,
            /*buffMask=*/0, /*statHash=*/0));
    }

    auto entitiesOffset = fbb.CreateVector(esVec);

    auto snap = Shared::Schema::CreateSnapshot(
        fbb,
        serverTick, /*serverTimeMs=*/0,
        rngState,
        entitiesOffset,
        lastAckedSeq,
        yourNetId,
        /*deltaBaseTick=*/0);

    fbb.Finish(snap);
    return fbb.Release();
}
```

---

## 9. D-1I — main.cpp bootstrap (1h)

### 9.1 `Server/Private/main.cpp` (전문 — placeholder 폐기)

```cpp
#include "Network/IOCPCore.h"
#include "Network/Session_Manager.h"
#include "Network/PacketDispatcher.h"
#include "Game/GameRoom.h"

#include <iostream>
#include <string>
#include <WinSock2.h>

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cerr << "[ERROR] WSAStartup failed\n";
        return 1;
    }

    auto core = CIOCPCore::Create(9000, 4);
    if (!core || !core->Start())
    {
        std::cerr << "[ERROR] IOCPCore start failed\n";
        WSACleanup();
        return 2;
    }

    auto room = CGameRoom::Create(1);
    CPacketDispatcher::Instance().RegisterRoom(1, room.get());

    // ★ MVP 단순화 — 모든 session 을 자동으로 room=1 에 라우팅
    //   본격은 Hello 응답 시 PacketDispatcher::RouteSession 호출
    room->Start();

    std::cout << "[Server] WintersServer v0.2 running on port 9000.\n";
    std::cout << "[Server] Press 'q' + Enter to quit.\n";

    std::string line;
    while (std::getline(std::cin, line))
        if (line == "q" || line == "Q") break;

    room->Stop();
    core->Shutdown();
    WSACleanup();
    return 0;
}
```

---

## 10. D-1J — Hello packet (2h)

### 10.1 `Shared/Schemas/Hello.fbs` (★ 신규)

```fbs
namespace Shared.Schema;

table Hello {
    sessionId:uint;
    yourNetId:uint;
    serverTick:ulong;
    serverTimeMs:ulong;
    championId:ubyte;       // Layer 1 — server 자동 할당 (Ezreal=0)
    team:ubyte;             // 0=Blue, 1=Red
}

root_type Hello;
```

빌드 절차: `Shared/Schemas/run_codegen.bat` → `Hello_generated.h` 산출.

### 10.2 GameRoom 측 Hello 송신 (D-1G `OnSessionJoin` 직후 호출)

`Server/Private/Game/GameRoom.cpp` 의 `OnSessionJoin` 에 추가:

```cpp
EntityID CGameRoom::OnSessionJoin(u32_t sessionId)
{
    // ... (기존 spawn 코드)

    // ★ D-1J — Hello 송신
    auto pSession = CSession_Manager::Get()->Find(sessionId);
    if (pSession)
    {
        // ★ Codex 보정 — championId 0 은 eChampion::NONE. SpawnChampion 의
        //   `champ.id = eChampion::EZREAL` (=12) 와 일관되게 enum 캐스트 사용.
        flatbuffers::FlatBufferBuilder fbb(128);
        auto hello = Shared::Schema::CreateHello(
            fbb,
            sessionId,
            netId,
            m_tickIndex, 0,
            static_cast<u8_t>(eChampion::EZREAL),   // ★ SpawnChampion 와 1:1 매칭
            (sessionId % 2 == 0) ? 0 : 1);          // team (Blue=0, Red=1)
        fbb.Finish(hello);
        auto buf = fbb.Release();

        auto wrapped = WrapEnvelope(
            ePacketType::Hello, /*sequence=*/0,
            buf.data(), static_cast<u32_t>(buf.size()));
        pSession->Send(std::move(wrapped));

        // ★ 자동 라우팅
        CPacketDispatcher::Instance().RouteSession(sessionId, m_roomId);
    }

    return e;
}
```

### 10.3 IOCPCore 의 Accept completion 직후 자동 join

`IOCPCore::WorkerLoop` 의 `eIOOp::Accept` 분기에서 `BindIOCP` 성공 후 `room->OnSessionJoin(sid)` 호출:

```cpp
case eIOOp::Accept:
{
    setsockopt(ctx->acceptSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
        (char*)&m_listenSocket, sizeof(m_listenSocket));

    auto session = CSession_Manager::Get()->OnAccept(ctx->acceptSocket, sockaddr_in{});
    if (session)
    {
        if (BindIOCP(session->GetSocket(), session->GetSessionId()))
        {
            session->PostInitialRecv();

            // ★ D-1J — 자동 room join (MVP 는 단일 room)
            extern CGameRoom* g_pRoom;   // main.cpp 에서 set
            if (g_pRoom)
                g_pRoom->OnSessionJoin(session->GetSessionId());
        }
        else
        {
            CSession_Manager::Get()->OnDisconnect(session->GetSessionId());
        }
    }
    // ... (다음 PostAccept)
    break;
}
```

`main.cpp` 에 전역 `CGameRoom* g_pRoom = room.get();` 선언.

(★ 향후 multi-room 시 `CMatchRouter` 가 sessionId → roomId 결정)

---

## 11. 합격 게이트 (D-1 전체)

| Phase | 합격 조건 |
|---|---|
| **D-1A** | Engine project reference 추가 → `world.CreateEntity()` 컴파일 OK |
| **D-1B** | localhost telnet connect/disconnect 반복 → leak 0 |
| **D-1C** | 1 byte 분할 송신 → frame 1개 복원, sticky 2 envelope → 2 frame |
| **D-1D** | recv → DispatchHello/CommandBatch 호출 도달 |
| **D-1E** | 동시 connect 2개 → sessionId 중복 0 |
| **D-1F** | dummy CommandBatch FlatBuffers verify → GameRoom queue 도달 |
| **D-1G** | 30Hz tick 5분 jitter < 5ms / Move command → Transform 갱신 |
| **D-1H** | 5v5 = 10 entity Layer 1 snapshot < 800B |
| **D-1I** | `WintersServer.exe` Q 로 정상 종료 |
| **D-1J** | Client connect 0.5s 내 Hello 도착 + 본인 NetEntityId 인지 |

---

## 12. 위험

| # | 위험 | 완화 |
|---|---|---|
| **R1** | `m_pfnAcceptEx` 미초기화 | `WSAIoctl` 실패 시 즉시 abort |
| **R2** | AcceptEx accepted socket 누수 | IOContext::acceptSocket 항상 close 또는 BindIOCP 통과 |
| **R3** | `CSession::Send` 가 send queue race | mutex + bSendPending |
| **R4** | `GetCurrentTickIndex` 가 worker thread 에서 stale 읽음 | atomic mirror (memory_order_relaxed 충분) |
| **R5** | Hello 송신 시점에 session->Send 가 아직 PostRecv 전 | OnAccept → BindIOCP → PostInitialRecv → OnSessionJoin → Hello Send 순서 강제 |

---

## 13. 한 줄

**D-1 = Server transport (TCP IOCP) 본격 박제. IOCPCore (AcceptEx 수명 + worker loop) + FrameParser (TryPop + Invalid) + Session (recv/send/queue + sequence guard) + Manager + Dispatcher (CommandBatch verify + Room route) + GameRoom (30Hz tick + stable_sort drain + Phase chain + Hello 자동 송신) + SnapshotBuilder (Layer 1 entity 직렬화) + main.cpp bootstrap. 파일 13개. ~27h. 합격 = 1 client connect → Hello 받음 → CommandBatch send → snapshot loop.**

---

## 부록 A — Codex 1차 검토 반영 매핑 (D-0 일관)

| # | Findings | 위치 | 적용 patch |
|---|---|---|---|
| P1-1 | `HealthComponent.current` 필드명 오류 | SnapshotBuilder.cpp §8.2 + GameRoom.cpp §7.2 SpawnChampion | `current/maxValue → fCurrent/fMaximum` |
| P1-3 | `GetWorldPosition()` stale | SnapshotBuilder.cpp §8.2 | `GetLocalPosition()` 사용 (root entity 가정) |
| Hello 일관 | championId=0 은 eChampion::NONE | GameRoom.cpp §10.2 OnSessionJoin | `static_cast<u8_t>(eChampion::EZREAL)` 사용 |
