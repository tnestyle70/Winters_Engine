# Phase Sim-10 v2 — M1 Sub-plan: UDP Transport (TCP → UDP migration)

> [!IMPORTANT]
> **Historical M1 design.** 아래의 “UDP 미착수 / reliability·fragment 없음” 전제는 더 이상 현행 상태가 아니다. 2026-07-13 현재 UDP v3 vertical slice는 40 B header, 16 B fragment header, 1,200 B datagram, reliable-ordered/unreliable-sequenced lanes, bounded fragmentation/reassembly, cookie handshake, Server IOCP와 Client facade까지 구현·검증됐다. 다만 기본 transport는 여전히 TCP이고 production UDP cutover는 인증·암호화, congestion/pacing, snapshot diet/AOI, WAN soak gate를 남긴 opt-in 상태다. 현행 기준은 [canonical implementation plan](../2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)를 따른다.

**작성일**: 2026-04-30
**상위 문서**: `10_UDP_LOL_NETSTACK_MASTER_v2.md` §8 M1
**전제**: `04a_v2_D0~D3` (TCP MVP) 합격 완료 — Server IOCP + Client TCP + Hello/CommandBatch/Snapshot loop 동작 검증
**범위**: TCP IOCP → UDP IOCP 전환. **단일 client 1 packet hello + CommandBatch + full snapshot loop**. Reliability 는 M2.
**합격**: localhost 1 client UDP connect → CommandBatch → server tick → full snapshot. 손실 0. 5분 jitter < 5ms. UDP RTT < 1ms localhost.

**한 줄**: **TCP 의 byte stream / FrameParser / TCP_NODELAY 의존을 UDP 의 datagram / WSARecvFrom / sourceAddr 기반 session lookup 으로 교체. Reliability X (M2 별도). transport-aware 는 5개 파일만 swap, GameRoom/SnapshotBuilder/Executor 는 그대로.**

---

## 0. M1 사이클 위치

```
04a v2 D-0 → D-1 → D-2 → D-3 (TCP MVP, 완료)
                    ↓
              ┌──── M1 (본 sub-plan) ────┐
              │  TCP → UDP transport swap │
              └────────────┬──────────────┘
                          ↓
                M2 (Reliability + Fragment)
                          ↓
                M3 (Delta + AOI)
                          ↓
                M4 (Lag Compensation)
                          ↓
                M5 (Client Prediction)
```

M1 = transport 만 갈아끼우는 사이클. **재사용**: `GameRoom`, `SnapshotBuilder`, `CommandExecutor`, `PacketEnvelope`, `Hello/CommandBatch/Snapshot.fbs` 모두 그대로. **교체**: 5 transport-aware 파일.

---

## 1. TCP → UDP 의 본질적 차이

| 측면 | TCP (04a v2) | UDP (M1) |
|---|---|---|
| **Socket** | `SOCK_STREAM` + `IPPROTO_TCP` | `SOCK_DGRAM` + `IPPROTO_UDP` |
| **Connection** | `connect()` 후 stream | connectionless. `bind()` 후 sourceAddr 로 식별 |
| **Send** | `WSASend` byte stream | `WSASendTo(sockaddr_in)` 명시 dest |
| **Recv** | `WSARecv` byte stream + FrameParser | `WSARecvFrom` 1 datagram = 1 frame |
| **Frame boundary** | TCP byte stream — FrameParser 가 magic+payloadSize 로 구분 | UDP datagram = frame 1개 (단 < MTU 1200B) |
| **Sticky packet** | 발생 가능 — FrameParser 가 흡수 | 발생 X (datagram 자체가 boundary) |
| **순서 보장** | TCP 가 자동 | X — sequenceNum 으로 client 가 verify (M2 ReliableOrdered) |
| **재전송** | TCP 가 자동 | X — M2 에서 RTO + ack |
| **Session lookup** | accept 시 socket per client | sourceAddr (`ip:port`) 가 key |
| **Hello / handshake** | accept 시 자동 | client 가 Hello 패킷 첫 송신 → server 가 sourceAddr 등록 |

**핵심**: M1 은 **순서 보장 / 재전송 X** — 단순 datagram echo. 손실 시 다음 snapshot 으로 복구. **Move 명령 손실 = 다음 우클릭 시 다시 보냄** (M2 까지 수용).

---

## 2. 신규 / 변경 파일 목록

### 2.1 Server (transport-aware 교체)

| 기존 (TCP) | 신규 (UDP) | 변경 강도 |
|---|---|---|
| `Server/Public/Network/IOCPCore.h/.cpp` | `UdpCore.h/.cpp` (신규) | 전면 재작성 |
| `Server/Public/Network/Session.h/.cpp` | `UdpSession.h/.cpp` (신규) | 전면 재작성 (sourceAddr 기반) |
| `Server/Public/Network/Session_Manager.h/.cpp` | `UdpSession_Manager.h/.cpp` (신규) | 거의 동일 (sourceAddr key 만 차이) |
| `Server/Public/Network/PacketDispatcher.h/.cpp` | `UdpPacketDispatcher.h/.cpp` (신규) | 거의 동일 (HandleAccept 제거) |
| `Server/Public/Network/FrameParser.h/.cpp` | **삭제** (UDP datagram = frame) | 제거 |
| `Server/Private/main.cpp` | 수정 — `CIOCPCore` → `CUdpCore` |  |
| `Server/Public/Game/GameRoom.h/.cpp` | **★ Codex P1-1 보정**: 기존 GameRoom.cpp 가 `CSession_Manager / CSession / CPacketDispatcher` 직접 사용 → UDP 등가물 (`CUdpSession_Manager` 등) 로 caller 교체. UDP Hello 경로에서 **명시적으로 `room->OnSessionJoin(sid)` 호출 필수** (TCP 의 IOCPCore Accept 대체). §8.2 + §11.2 참조. | 부분 수정 |
| `Server/Public/Game/SnapshotBuilder.h/.cpp` | **변경 X** |  |

★ **Codex P1-1 핵심**: TCP 에선 `IOCPCore::WorkerLoop` 의 `eIOOp::Accept` 분기가 `g_pRoom->OnSessionJoin(sid)` 호출 → champion spawn + NetEntityId 발급 + Hello 송신. UDP 는 accept 개념 X → **`UdpPacketDispatcher::DispatchHello` 가 자동 `OnSessionJoin` 호출** 해야 m_sessionToEntity 매핑 / snapshot broadcast 가 연결됨. 누락 시 client 가 Hello 보내도 server 가 응답 안 함.

### 2.2 Client (transport-aware 교체)

| 기존 (TCP) | 신규 (UDP) | 변경 강도 |
|---|---|---|
| `Client/Public/Network/Client/ClientNetwork.h/.cpp` | `UdpClientNetwork.h/.cpp` (신규) 또는 `ClientNetwork.cpp` 재작성 | 전면 재작성 |
| `Client/Public/Network/Client/CommandSerializer.h/.cpp` | **변경 X** — `BuildCommandBatch` 그대로 |  |
| `Client/Public/Network/Client/SnapshotApplier.h/.cpp` | **변경 X** |  |
| `Client/Private/Scene/Scene_InGame.cpp` | **포인터 캐시 type 만 변경** (CClientNetwork → CUdpClientNetwork 또는 동일 name 유지) |  |

### 2.3 Shared (transport-neutral — 변경 X)

- `Shared/Network/PacketEnvelope.h` — magic / version / type / payloadSize (16B header) 그대로
- `Shared/Schemas/*.fbs` — Hello / CommandBatch / Snapshot 그대로
- `Shared/GameSim/*` — 전부 transport 무관

---

## 3. `Server/Public/Network/UdpCore.h` (★ 신규, 전문)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")

// ─────────────────────────────────────────────────────────────────
//  CUdpCore  |  UDP IOCP transport
//
//  TCP 의 IOCPCore 와 다른 점:
//    1. socket = SOCK_DGRAM + IPPROTO_UDP (1개 socket 으로 모든 client 처리)
//    2. accept X — client 가 Hello 패킷 보낼 때 sourceAddr 로 자동 session 발급
//    3. WSARecvFrom — fromAddr 채워짐. sourceAddr 기반 session lookup
//    4. WSASendTo — toAddr 명시
//    5. 1 datagram = 1 envelope (MTU 1200B 한도)
//
//  M2 fragment 진입 전까지 payload > 1200B 는 reject.
// ─────────────────────────────────────────────────────────────────

enum class eUdpIOOp : u8_t
{
    Recv = 0,
    Send,
};

struct UdpIOContext
{
    OVERLAPPED      overlapped{};
    WSABUF          wsaBuf{};
    char            buffer[1500]{};   // MTU 한도
    eUdpIOOp        op = eUdpIOOp::Recv;
    sockaddr_in     fromAddr{};       // RecvFrom 결과 source
    int             fromAddrLen = sizeof(sockaddr_in);
};

class CUdpCore final
{
public:
    static std::unique_ptr<CUdpCore> Create(u16_t port, u32_t workerCount);
    ~CUdpCore();

    CUdpCore(const CUdpCore&) = delete;
    CUdpCore& operator=(const CUdpCore&) = delete;

    bool_t Start();
    void   Shutdown();

    // ★ transport-aware send. SessionManager 가 호출.
    bool_t SendTo(const sockaddr_in& dst, const u8_t* data, u32_t len);

    HANDLE GetCompletionPort() const { return m_hIOCP; }

private:
    CUdpCore(u16_t port, u32_t workerCount);

    bool_t PostRecvFrom();
    void   WorkerLoop(u32_t workerId);

    HANDLE  m_hIOCP        = nullptr;
    SOCKET  m_socket       = INVALID_SOCKET;
    u16_t   m_port         = 0;
    u32_t   m_workerCount  = 0;

    std::vector<std::thread> m_workers;
    std::atomic<bool_t>      m_bRunning{ false };

    // PostRecvFrom 용 풀 (back pressure — N개 동시 in-flight)
    static constexpr u32_t kRecvPoolSize = 16;
    std::vector<std::unique_ptr<UdpIOContext>> m_recvPool;
};
```

---

## 4. `Server/Private/Network/UdpCore.cpp` (★ 신규, 전문 — 핵심부)

### 4.1 Create / Start

```cpp
#include "Network/UdpCore.h"
#include "Network/UdpSession_Manager.h"

#include <iostream>

std::unique_ptr<CUdpCore> CUdpCore::Create(u16_t port, u32_t workerCount)
{
    return std::unique_ptr<CUdpCore>(new CUdpCore(port, workerCount));
}

CUdpCore::CUdpCore(u16_t port, u32_t workerCount)
    : m_port(port), m_workerCount(workerCount)
{
}

CUdpCore::~CUdpCore()
{
    Shutdown();
}

bool_t CUdpCore::Start()
{
    // 1. IOCP
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, m_workerCount);
    if (!m_hIOCP) return false;

    // 2. UDP socket
    m_socket = WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (m_socket == INVALID_SOCKET) return false;

    // ★ UDP_CONNRESET disable (Windows quirk — ICMP unreachable 시 socket 죽음 방지)
    BOOL bNewBehavior = FALSE;
    DWORD bytesReturned = 0;
    WSAIoctl(m_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
             nullptr, 0, &bytesReturned, nullptr, nullptr);

    // 3. bind
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(m_port);
    if (bind(m_socket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
        return false;

    // 4. socket → IOCP 등록
    if (!CreateIoCompletionPort((HANDLE)m_socket, m_hIOCP, 0, 0))
        return false;

    // 5. recv pool 채우기
    m_bRunning = true;
    for (u32_t i = 0; i < kRecvPoolSize; ++i)
    {
        m_recvPool.push_back(std::unique_ptr<UdpIOContext>(new UdpIOContext()));
    }

    // 6. workers
    for (u32_t i = 0; i < m_workerCount; ++i)
        m_workers.emplace_back(&CUdpCore::WorkerLoop, this, i);

    // 7. 초기 PostRecvFrom 풀 채우기
    for (u32_t i = 0; i < kRecvPoolSize; ++i)
        if (!PostRecvFrom())
            break;

    std::cout << "[UdpCore] listening UDP on port " << m_port
              << " (workers=" << m_workerCount << ")\n";
    return true;
}

void CUdpCore::Shutdown()
{
    if (!m_bRunning.exchange(false)) return;

    // worker 깨우기
    for (u32_t i = 0; i < m_workerCount; ++i)
        PostQueuedCompletionStatus(m_hIOCP, 0, 0, nullptr);

    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
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
```

### 4.2 PostRecvFrom + WorkerLoop

```cpp
bool_t CUdpCore::PostRecvFrom()
{
    // 풀에서 free context 확보 — 단순 round-robin 또는 free list
    // (M1 은 단순 — 매번 새로 alloc 해도 OK, M2 에서 풀링 강화)
    UdpIOContext* ctx = new UdpIOContext();
    ctx->op = eUdpIOOp::Recv;
    ctx->wsaBuf.buf = ctx->buffer;
    ctx->wsaBuf.len = static_cast<ULONG>(sizeof(ctx->buffer));

    DWORD flags = 0;
    DWORD bytes = 0;
    int result = WSARecvFrom(
        m_socket,
        &ctx->wsaBuf, 1,
        &bytes, &flags,
        (sockaddr*)&ctx->fromAddr, &ctx->fromAddrLen,
        &ctx->overlapped, nullptr);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        delete ctx;
        return false;
    }
    return true;
}

void CUdpCore::WorkerLoop(u32_t workerId)
{
    (void)workerId;

    while (m_bRunning)
    {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* pOverlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(m_hIOCP, &bytes, &key, &pOverlapped, INFINITE);

        if (pOverlapped == nullptr)
        {
            if (!m_bRunning) break;
            continue;
        }

        UdpIOContext* ctx = CONTAINING_RECORD(pOverlapped, UdpIOContext, overlapped);

        if (!ok)
        {
            delete ctx;
            // 다음 recv 재예약
            if (m_bRunning) PostRecvFrom();
            continue;
        }

        switch (ctx->op)
        {
            case eUdpIOOp::Recv:
            {
                if (bytes > 0)
                {
                    // ★ sourceAddr + payload 를 SessionManager 로 전달
                    CUdpSession_Manager::Get()->OnRecv(
                        ctx->fromAddr,
                        reinterpret_cast<const u8_t*>(ctx->buffer), bytes);
                }
                delete ctx;
                // 다음 recv 재예약
                if (m_bRunning) PostRecvFrom();
                break;
            }
            case eUdpIOOp::Send:
            {
                // SendTo 완료 — context 단순 삭제
                delete ctx;
                break;
            }
        }
    }
}
```

### 4.3 SendTo

```cpp
bool_t CUdpCore::SendTo(const sockaddr_in& dst, const u8_t* data, u32_t len)
{
    if (m_socket == INVALID_SOCKET || !data || len == 0) return false;
    if (len > 1200) return false;   // M1 = MTU 한도. M2 fragment 별도.

    // overlapped send context — caller 에 push 후 worker 가 회수
    UdpIOContext* ctx = new UdpIOContext();
    ctx->op = eUdpIOOp::Send;
    std::memcpy(ctx->buffer, data, len);
    ctx->wsaBuf.buf = ctx->buffer;
    ctx->wsaBuf.len = len;

    DWORD bytes = 0;
    int result = WSASendTo(
        m_socket,
        &ctx->wsaBuf, 1,
        &bytes, 0,
        (const sockaddr*)&dst, sizeof(dst),
        &ctx->overlapped, nullptr);

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
    {
        delete ctx;
        return false;
    }
    return true;
}
```

---

## 5. `Server/Public/Network/UdpSession.h` (★ 신규, 핵심부)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

#include <WinSock2.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

class CUdpSession final
{
public:
    static std::shared_ptr<CUdpSession> Create(u32_t sessionId, const sockaddr_in& addr);
    ~CUdpSession() = default;

    CUdpSession(const CUdpSession&) = delete;
    CUdpSession& operator=(const CUdpSession&) = delete;

    u32_t              GetSessionId() const          { return m_sessionId; }
    const sockaddr_in& GetSourceAddr() const         { return m_sourceAddr; }
    EntityID           GetControlledEntity() const   { return m_controlledEntity; }
    void               SetControlledEntity(EntityID e) { m_controlledEntity = e; }

    // sequence guard — TCP 와 동일
    bool_t TryAcceptSequence(u32_t seq, bool_t& bSuspicious);

    // suspicion
    void   FlagSuspicious()     { ++m_suspicionCount; }
    bool_t IsSuspicious() const { return m_suspicionCount > 5; }

    // ★ idle 검증 — UDP 는 connection 개념 없음. 30초 무수신 시 timeout
    void  TouchLastRecv()
    {
        m_lastRecvMs.store(NowMs(), std::memory_order_relaxed);
    }
    bool_t IsExpired(u64_t timeoutMs) const
    {
        return (NowMs() - m_lastRecvMs.load(std::memory_order_relaxed)) > timeoutMs;
    }

private:
    CUdpSession(u32_t sessionId, const sockaddr_in& addr);

    static u64_t NowMs()
    {
        return static_cast<u64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }

    u32_t       m_sessionId          = 0;
    sockaddr_in m_sourceAddr         = {};
    EntityID    m_controlledEntity   = NULL_ENTITY;

    mutable std::mutex m_seqMutex;
    u32_t              m_lastProcessedSeq = 0;
    u32_t              m_suspicionCount   = 0;

    std::atomic<u64_t> m_lastRecvMs{ 0 };
};
```

---

## 6. `Server/Public/Network/UdpSession_Manager.h` (★ 신규)

```cpp
#pragma once
#include "Network/UdpSession.h"

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────
//  CUdpSession_Manager
//
//  TCP 와 다른 점:
//    - sourceAddr 기반 lookup (string key = ip:port)
//    - Hello 패킷 첫 수신 시 자동 session 발급 (TCP 의 OnAccept 대체)
//    - Idle timeout (30초 무수신)
// ─────────────────────────────────────────────────────────────────

class CUdpCore;

class CUdpSession_Manager
{
public:
    static CUdpSession_Manager* Get();

    void SetUdpCore(CUdpCore* pCore) { m_pCore = pCore; }

    // 단일 진입점 — UdpCore 의 worker 가 호출
    void OnRecv(const sockaddr_in& fromAddr, const u8_t* bytes, u32_t len);

    // session find by id (sim 외 lookup)
    std::shared_ptr<CUdpSession> Find(u32_t sessionId);

    // SendTo wrapping
    bool_t SendTo(u32_t sessionId, const u8_t* data, u32_t len);
    bool_t SendTo(const sockaddr_in& addr, const u8_t* data, u32_t len);

    // foreach (sorted by sessionId — 결정성)
    void ForEach(const std::function<void(CUdpSession&)>& fn);

    // periodic — main thread (또는 별도 timer thread) 호출. expired session 정리.
    void TickExpire(u64_t timeoutMs = 30'000);

private:
    CUdpSession_Manager() = default;
    CUdpSession_Manager(const CUdpSession_Manager&) = delete;

    static std::string AddrKey(const sockaddr_in& addr);

    CUdpCore* m_pCore = nullptr;

    std::mutex                                                    m_mutex;
    std::unordered_map<std::string, std::shared_ptr<CUdpSession>> m_byAddr;       // sourceAddr → session
    std::unordered_map<u32_t, std::shared_ptr<CUdpSession>>       m_bySessionId;  // id → session
    std::atomic<u32_t>                                            m_nextSessionId{ 1 };
};
```

---

## 7. `Server/Private/Network/UdpSession_Manager.cpp` (★ 핵심부)

### 7.1 OnRecv — Hello 자동 session 발급

```cpp
void CUdpSession_Manager::OnRecv(const sockaddr_in& fromAddr, const u8_t* bytes, u32_t len)
{
    if (!bytes || len < sizeof(PacketHeader)) return;

    // 1. Envelope verify
    PacketHeader hdr{};
    std::memcpy(&hdr, bytes, sizeof(PacketHeader));
    if (hdr.magic != kPacketMagic || hdr.version != kPacketVersion ||
        hdr.payloadSize > kMaxPacketPayloadSize ||
        sizeof(PacketHeader) + hdr.payloadSize != len)
    {
        // 잘못된 envelope → drop. (M2 부터 suspicion ++)
        return;
    }

    const ePacketType type = static_cast<ePacketType>(hdr.type);
    const u8_t* payload = bytes + sizeof(PacketHeader);
    const u32_t payloadSize = hdr.payloadSize;

    // 2. Session lookup or create
    std::shared_ptr<CUdpSession> session;
    std::string key = AddrKey(fromAddr);
    {
        std::lock_guard lk(m_mutex);
        auto it = m_byAddr.find(key);
        if (it != m_byAddr.end())
        {
            session = it->second;
        }
        else if (type == ePacketType::Hello)
        {
            // ★ Hello 첫 수신 시 자동 session 발급 (TCP 의 OnAccept 대체)
            const u32_t newSid = m_nextSessionId.fetch_add(1, std::memory_order_relaxed);
            session = CUdpSession::Create(newSid, fromAddr);
            m_byAddr[key] = session;
            m_bySessionId[newSid] = session;
        }
        else
        {
            // Unknown source + non-Hello → drop
            return;
        }
    }

    session->TouchLastRecv();

    // 3. Dispatch
    CUdpPacketDispatcher::Instance().Dispatch(
        session->GetSessionId(), type, hdr.sequence, payload, payloadSize);
}
```

### 7.2 SendTo + ForEach + AddrKey

```cpp
bool_t CUdpSession_Manager::SendTo(u32_t sessionId, const u8_t* data, u32_t len)
{
    auto session = Find(sessionId);
    if (!session || !m_pCore) return false;
    return m_pCore->SendTo(session->GetSourceAddr(), data, len);
}

bool_t CUdpSession_Manager::SendTo(const sockaddr_in& addr, const u8_t* data, u32_t len)
{
    if (!m_pCore) return false;
    return m_pCore->SendTo(addr, data, len);
}

void CUdpSession_Manager::ForEach(const std::function<void(CUdpSession&)>& fn)
{
    std::vector<u32_t> ids;
    {
        std::lock_guard lk(m_mutex);
        ids.reserve(m_bySessionId.size());
        for (auto& [sid, _] : m_bySessionId) ids.push_back(sid);
    }
    std::sort(ids.begin(), ids.end());   // ★ 결정성

    for (u32_t sid : ids)
        if (auto pSession = Find(sid)) fn(*pSession);
}

std::string CUdpSession_Manager::AddrKey(const sockaddr_in& addr)
{
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
    char buf[64];
    sprintf_s(buf, "%s:%u", ip, ntohs(addr.sin_port));
    return std::string(buf);
}

void CUdpSession_Manager::TickExpire(u64_t timeoutMs)
{
    std::vector<std::string> toErase;
    {
        std::lock_guard lk(m_mutex);
        for (auto& [key, sess] : m_byAddr)
        {
            if (sess && sess->IsExpired(timeoutMs))
                toErase.push_back(key);
        }

        for (const auto& key : toErase)
        {
            auto it = m_byAddr.find(key);
            if (it != m_byAddr.end())
            {
                m_bySessionId.erase(it->second->GetSessionId());
                m_byAddr.erase(it);
            }
        }
    }
}
```

---

## 8. `Server/Public/Network/UdpPacketDispatcher.h` (★ 신규, 거의 TCP 와 동일)

```cpp
#pragma once
#include "WintersTypes.h"
#include "Shared/Network/PacketEnvelope.h"

#include <unordered_map>
#include <mutex>

class CGameRoom;

class CUdpPacketDispatcher
{
public:
    static CUdpPacketDispatcher& Instance();

    // ★ TCP 와 다른 점: Sequence 가 envelope 에 박혀있어 별도 frame parsing X.
    //   payload 는 datagram 내부의 payload 부분만 (envelope 제거됨).
    void Dispatch(u32_t sessionId, ePacketType type, u32_t sequence,
                  const u8_t* payload, u32_t payloadSize);

    void RegisterRoom(u32_t roomId, CGameRoom* pRoom);
    void RouteSession(u32_t sessionId, u32_t roomId);

private:
    CUdpPacketDispatcher() = default;

    void DispatchHello(u32_t sessionId, const u8_t* payload, u32_t len);
    void DispatchCommandBatch(u32_t sessionId, const u8_t* payload, u32_t len);

    std::mutex                            m_mutex;
    std::unordered_map<u32_t, CGameRoom*> m_rooms;
    std::unordered_map<u32_t, u32_t>      m_sessionToRoom;
};
```

`UdpPacketDispatcher.cpp` 구현은 TCP 의 `PacketDispatcher.cpp` 와 거의 동일. **차이점**:
- `DrainFrames` / `FrameParser` 호출 X (datagram = frame 1개)
- `DispatchHello` 시 자동 RouteSession 호출 (TCP 와 동일 로직)
- **★ Codex P1-1 — `DispatchHello` 가 GameRoom::OnSessionJoin 명시 호출** (TCP 의 IOCPCore Accept 대체)

### 8.3 `DispatchHello` 본격 (★ Codex P1-1 — GameRoom 연결)

```cpp
void CUdpPacketDispatcher::DispatchHello(u32_t sessionId, const u8_t* payload, u32_t len)
{
    (void)payload;
    (void)len;
    // M1 의 Hello = 빈 envelope. payload 검증 X (M2 부터 client 정보 직렬화).

    // 1. 본 sessionId 가 이미 room 에 routed 됐으면 skip (재전송 Hello 흡수)
    {
        std::lock_guard lk(m_mutex);
        if (m_sessionToRoom.find(sessionId) != m_sessionToRoom.end())
            return;
    }

    // 2. MVP — 단일 room 에 자동 라우팅
    CGameRoom* pRoom = nullptr;
    u32_t roomId = 0;
    {
        std::lock_guard lk(m_mutex);
        if (m_rooms.empty()) return;
        roomId = m_rooms.begin()->first;
        pRoom  = m_rooms.begin()->second;
    }
    if (!pRoom) return;

    // 3. ★ Codex P1-1 — RouteSession + OnSessionJoin 명시 호출
    //    TCP 의 IOCPCore::WorkerLoop 의 eIOOp::Accept 분기와 등가
    {
        std::lock_guard lk(m_mutex);
        m_sessionToRoom[sessionId] = roomId;
    }

    // SpawnChampion + NetEntityId 발급 + Hello 응답 broadcast 수행
    pRoom->OnSessionJoin(sessionId);
}
```

★ 이 한 곳이 누락되면 client Hello → server session 만 발급 → champion spawn X → snapshot broadcast 0 entity → maphack 의심 회귀.

---

## 9. `Server/Private/main.cpp` 변경

**수정 전 (TCP)**:
```cpp
auto core = CIOCPCore::Create(9000, 4);
```

**수정 후 (UDP)**:
```cpp
WSADATA wsa{};
if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { /* fail */ }

auto room = CGameRoom::Create(1);
g_pRoom = room.get();
CUdpPacketDispatcher::Instance().RegisterRoom(1, room.get());
room->Start();

auto core = CUdpCore::Create(9000, 4);
if (!core || !core->Start()) { /* fail */ }

CUdpSession_Manager::Get()->SetUdpCore(core.get());

std::cout << "[Server] WintersServer v0.3 UDP listening on port 9000.\n";
// ... (기존 stdin loop + hp 디버그 명령 그대로)

// 별도 thread 로 idle session 정리
std::thread expireThread([&]() {
    while (g_pRoom) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        CUdpSession_Manager::Get()->TickExpire(30'000);
    }
});

// shutdown 시 thread join
```

---

## 10. `Client/Public/Network/Client/UdpClientNetwork.h` (★ 신규)

```cpp
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

#include "Defines.h"
#include "Shared/Network/PacketEnvelope.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <tuple>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

class CUdpClientNetwork final
{
public:
    static std::unique_ptr<CUdpClientNetwork> Create();
    ~CUdpClientNetwork();

    CUdpClientNetwork(const CUdpClientNetwork&) = delete;
    CUdpClientNetwork& operator=(const CUdpClientNetwork&) = delete;

    bool_t Connect(const char* host, u16_t port);
    void   Disconnect();

    bool_t Send(std::vector<u8_t> packet);

    using FrameCallback = std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;
    void  SetFrameCallback(FrameCallback fn);
    void  PumpReceivedFrames();

    bool_t IsConnected() const { return m_bConnected.load(std::memory_order_relaxed); }

    u32_t GetMyNetEntityId() const { return m_myNetId; }
    void  SetMyNetEntityId(u32_t id) { m_myNetId = id; }
    u32_t GetMySessionId() const { return m_mySessionId; }
    void  SetMySessionId(u32_t sid) { m_mySessionId = sid; }

private:
    CUdpClientNetwork() = default;

    void RecvThread();
    bool_t SendHelloIfNeeded();   // 첫 send 직전에 Hello 자동 송신

    SOCKET             m_socket = INVALID_SOCKET;
    sockaddr_in        m_serverAddr{};
    std::thread        m_recvThread;
    std::atomic<bool_t> m_bRunning{ false };
    std::atomic<bool_t> m_bConnected{ false };
    std::atomic<bool_t> m_bHelloSent{ false };

    std::mutex                                                            m_pendingMutex;
    std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>>        m_pendingFrames;

    FrameCallback m_callback;
    u32_t         m_myNetId = 0;
    u32_t         m_mySessionId = 0;
};
```

---

## 11. `Client/Private/Network/Client/UdpClientNetwork.cpp` (★ 핵심부)

### 11.1 Connect — UDP 는 connect 자체가 단순 destination set

```cpp
bool_t CUdpClientNetwork::Connect(const char* host, u16_t port)
{
    if (m_bConnected) return true;

    m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (m_socket == INVALID_SOCKET) return false;

    // ★ UDP_CONNRESET disable (Windows ICMP unreachable 시 socket 죽음 방지)
    BOOL bNewBehavior = FALSE;
    DWORD bytesReturned = 0;
    WSAIoctl(m_socket, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior),
             nullptr, 0, &bytesReturned, nullptr, nullptr);

    m_serverAddr = {};
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &m_serverAddr.sin_addr) != 1)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    m_bRunning   = true;
    m_bConnected = true;
    m_bHelloSent = false;
    m_recvThread = std::thread(&CUdpClientNetwork::RecvThread, this);

    OutputDebugStringA("[UdpClient] connected (UDP)\n");
    return true;
}
```

### 11.2 Send — Hello 자동 송신 + sendto

```cpp
bool_t CUdpClientNetwork::Send(std::vector<u8_t> packet)
{
    if (!m_bConnected || m_socket == INVALID_SOCKET || packet.empty()) return false;

    // ★ 첫 send 직전에 Hello 패킷 자동 송신 (TCP 의 connect 대체)
    if (!m_bHelloSent.load(std::memory_order_acquire))
    {
        if (!SendHelloIfNeeded()) return false;
    }

    int sent = sendto(m_socket,
        reinterpret_cast<const char*>(packet.data()),
        static_cast<int>(packet.size()),
        0,
        (const sockaddr*)&m_serverAddr, sizeof(m_serverAddr));

    if (sent == SOCKET_ERROR || sent != static_cast<int>(packet.size()))
    {
        OutputDebugStringA("[UdpClient] sendto failed\n");
        return false;
    }
    return true;
}

bool_t CUdpClientNetwork::SendHelloIfNeeded()
{
    if (m_bHelloSent.exchange(true, std::memory_order_acq_rel)) return true;

    // 빈 Hello payload — server 가 sourceAddr 만 보고 session 발급. 본격 Hello 데이터는 server 가 broadcast.
    PacketHeader hdr{};
    hdr.type        = static_cast<u16_t>(ePacketType::Hello);
    hdr.payloadSize = 0;
    hdr.sequence    = 0;

    int sent = sendto(m_socket,
        reinterpret_cast<const char*>(&hdr), sizeof(hdr),
        0,
        (const sockaddr*)&m_serverAddr, sizeof(m_serverAddr));

    return sent == sizeof(hdr);
}
```

### 11.3 RecvThread — recvfrom + datagram = frame 직접 emit

```cpp
void CUdpClientNetwork::RecvThread()
{
    char buf[1500]{};

    while (m_bRunning)
    {
        sockaddr_in fromAddr{};
        int fromLen = sizeof(fromAddr);

        int n = recvfrom(m_socket, buf, sizeof(buf), 0,
                         (sockaddr*)&fromAddr, &fromLen);

        if (n <= 0)
        {
            if (m_bRunning && WSAGetLastError() != WSAEINTR)
            {
                OutputDebugStringA("[UdpClient] recvfrom failed\n");
                m_bConnected = false;
            }
            break;
        }

        // datagram = 1 frame. envelope 직접 파싱.
        if (n < static_cast<int>(sizeof(PacketHeader))) continue;

        PacketHeader hdr{};
        std::memcpy(&hdr, buf, sizeof(PacketHeader));
        if (hdr.magic != kPacketMagic || hdr.version != kPacketVersion ||
            sizeof(PacketHeader) + hdr.payloadSize != static_cast<u32_t>(n))
        {
            // M2 부터 suspicion / metric. M1 은 단순 drop.
            continue;
        }

        std::vector<u8_t> payload(
            reinterpret_cast<u8_t*>(buf) + sizeof(PacketHeader),
            reinterpret_cast<u8_t*>(buf) + n);

        {
            std::lock_guard lk(m_pendingMutex);
            m_pendingFrames.emplace_back(
                static_cast<ePacketType>(hdr.type), hdr.sequence, std::move(payload));
        }
    }
}
```

`PumpReceivedFrames`, `SetFrameCallback`, `Disconnect` 는 TCP 의 `ClientNetwork.cpp` 와 동일 (그대로 복사).

---

## 11.5 GameRoom caller 변경 (★ Codex P1-1 — TCP Session → UDP Session)

기존 `Server/Private/Game/GameRoom.cpp` 가 TCP 의 `CSession_Manager / CSession / CPacketDispatcher` 직접 사용 → UDP 등가물로 caller 교체.

### 11.5.1 변경 부분 — `Phase_BroadcastSnapshot`

**수정 전 (TCP, 04a v2 D-1G)**:
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CSession_Manager::Get()->Find(sid);
        if (!pSession) continue;
        // ... build snapshot ...
        auto wrapped = WrapEnvelope(...);
        pSession->Send(std::move(wrapped));   // TCP CSession::Send
    }
}
```

**수정 후 (UDP, M1)**:
```cpp
void CGameRoom::Phase_BroadcastSnapshot(TickContext& tc)
{
    for (u32_t sid : m_sessionIds)
    {
        auto pSession = CUdpSession_Manager::Get()->Find(sid);
        if (!pSession) continue;
        // ... build snapshot (동일) ...
        auto wrapped = WrapEnvelope(...);
        // ★ UDP 는 SessionManager 의 SendTo wrapping 사용 (sourceAddr resolve)
        CUdpSession_Manager::Get()->SendTo(sid, wrapped.data(),
            static_cast<u32_t>(wrapped.size()));
    }
}
```

### 11.5.2 변경 부분 — `OnSessionJoin` 의 Hello 송신

기존 D-1J 의 Hello 송신 코드:
```cpp
auto pSession = CSession_Manager::Get()->Find(sessionId);
if (pSession) pSession->Send(std::move(wrapped));   // TCP
```

변경:
```cpp
CUdpSession_Manager::Get()->SendTo(sessionId, wrapped.data(),
    static_cast<u32_t>(wrapped.size()));
```

### 11.5.3 변경 부분 — `OnCommandBatch`

`OnCommandBatch` 자체는 transport-neutral (CommandBatch 를 받아 m_pendingCommands 에 push). **변경 X** — 단 `CSession_Manager::Get()->Find()` 사용처가 있으면 `CUdpSession_Manager::Get()->Find()` 로 sed.

### 11.5.4 sed 일괄 변경 (PowerShell)

```powershell
Set-Location C:\Users\user\Desktop\Winters\Server\Private

(Get-Content Game\GameRoom.cpp -Raw) `
    -replace 'CSession_Manager::Get\(\)','CUdpSession_Manager::Get()' `
    -replace '#include "Network/Session_Manager.h"','#include "Network/UdpSession_Manager.h"' `
    -replace 'pSession->Send\(std::move\(wrapped\)\)',
        'CUdpSession_Manager::Get()->SendTo(sessionId, wrapped.data(), static_cast<u32_t>(wrapped.size()))' `
| Set-Content Game\GameRoom.cpp -NoNewline
```

(★ wrapped 변수명 / sessionId 변수명 매크로 매칭 — 실제 코드와 매치 안 되면 수동 수정)

---

## 12. `Scene_InGame.cpp` 변경 (★ 거의 X)

**수정 전 (TCP)**:
```cpp
m_pNetwork = CClientNetwork::Create();
```

**수정 후 (UDP)**:
```cpp
m_pNetwork = CUdpClientNetwork::Create();
```

또는 **type alias** 로 caller side 변경 0:
```cpp
// Client/Public/Network/Client/ClientNetwork.h
#if defined(WINTERS_NET_UDP)
    using CClientNetwork = CUdpClientNetwork;
#else
    // 기존 TCP CClientNetwork 정의 그대로
#endif
```

권장: **type alias** — Scene_InGame / 기타 caller 변경 0 + config 로 swap.

---

## 13. 결정성 회귀 가드 (★ Codex 관점 — Sim-10 v2 §4)

M1 진입과 동시에 4 항목 grep CI 도입:

```bash
# 1. /fp:fast 회귀 (Server.vcxproj 가 Precise 인지)
rg "FloatingPointModel.*Fast" Engine/Include/ Server/Include/ Client/Include/

# 2. unordered_map/set in sim (allowlist 외)
rg "unordered_map|unordered_set" Shared/GameSim/ Server/Private/Game/ Server/Private/Network/

# 3. wall-clock in sim
rg "chrono::|GetTickCount|time\(0|time\(nullptr|QueryPerformance" \
   Shared/GameSim/ Server/Private/Game/

# 4. transport boundary (Game 내부에서 raw socket 호출 X)
rg "WSARecv|WSASend|recvfrom|sendto|AcceptEx" \
   Server/Public/Game/ Server/Private/Game/ Shared/
```

각 4종 0 hit (allowlist 외) 가 합격 조건. 04a v2 D-3 의 grep gate 와 동일.

---

## 14. 합격 게이트 (M1 전체)

### 14.1 빌드 합격
- ✅ Server.vcxproj — `CUdpCore / UdpSession / UdpSession_Manager / UdpPacketDispatcher` 4쌍 편입
- ✅ Client.vcxproj — `CUdpClientNetwork` 신규 편입 (또는 type alias 로 기존 슬롯 재사용)
- ✅ `IOCPCore / Session / Session_Manager / PacketDispatcher / FrameParser` 5개 (TCP) 는 별도 컨피그 또는 `_Legacy` 보존
- ✅ Server.vcxproj `Precise` 설정 확인

### 14.2 런타임 합격
- ✅ localhost 1 client UDP connect → Hello 자동 송신 → server session 발급 → Hello 수신
- ✅ 우클릭 → CommandBatch send → server tick → snapshot broadcast → client recv
- ✅ 5분 30Hz tick jitter < 5ms
- ✅ UDP localhost RTT < 1ms (packet round-trip)

### 14.3 결정성 가드
- ✅ 4 grep gate allowlist 외 0 hit

### 14.4 손실 무복구 검증 (M2 prerequisite)
- ✅ clumsy net 5% loss 환경 → command 손실 발생 (재전송 X — 의도) → 다음 우클릭 정상 도달
- ✅ snapshot 손실 시 다음 snapshot 으로 복구 (full snapshot 항상 broadcast)

---

## 15. 위험 / 디버깅

| 위험 | 완화 |
|---|---|
| `SIO_UDP_CONNRESET` 미설정 시 ICMP unreachable 받으면 socket 죽음 (Windows quirk) | UdpCore::Start + UdpClientNetwork::Connect 양쪽에서 명시 disable |
| MTU > 1200B payload 발생 시 silent drop | M1 = `len > 1200` reject + log. M2 fragment 본격 |
| Hello 자동 송신 race (Send 첫 호출 multiple thread) | `m_bHelloSent.exchange(true)` atomic guard |
| sourceAddr 변경 (NAT rebind) — session 끊김 | M1 = idle timeout 30초 후 신규 발급. M2 부터 sessionId 명시 envelope 으로 복구 |
| `recvfrom` 가 partial datagram 반환? | UDP 는 datagram-oriented — partial X (truncate 시 WSAEMSGSIZE 에러). MTU 이상 packet 은 OS 가 drop |
| Server 측 send queue 부재 — Submit 시 immediate sendto | M2 reliability 단계에서 send queue + retry. M1 = best-effort |

---

## 16. M1 → M2 진입 조건

M1 합격 후 M2 진입. M2 추가 작업:
- `UdpReliabilityChannel` (3 channel: Reliable / ReliableOrdered / Unreliable)
- `UdpFragmenter` (>1200B 분할/재조립)
- `SeqMath` (wrap-around)
- 5%/10%/20% loss + 100ms RTT 환경 입력 stable

M1 산출물 (transport-aware 5쌍) 위에 M2 가 reliability layer 만 추가. GameRoom / SnapshotBuilder 변경 0.

---

## 17. 한 줄

**M1 = TCP IOCP → UDP IOCP swap. transport-aware 5쌍 (UdpCore/Session/SessionManager/PacketDispatcher) 신규 박제 + FrameParser 제거 (datagram = frame). GameRoom/SnapshotBuilder/CommandExecutor 그대로. Hello 자동 송신 + sourceAddr 기반 session lookup. 결정성 4 grep gate 강제. 1주 (50h). 합격 = localhost UDP RTT < 1ms + 5분 jitter < 5ms + grep 0 hit. M2 reliability 진입 준비.**
