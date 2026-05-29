# Winters Engine — Network System 구현 계획서

> 작성일: 2026-04-08
> 구현 순서: Fiber Job System → Render Graph → GPU Driven Pipeline → **Network System**
> 참고: `.claude/.agent/ServerAgentMD/00_INDEX.agent.md` (15개 에이전트 태스크)

---

## 1. 원리 (Principles)

### IOCP (I/O Completion Port)

Windows 전용 비동기 I/O 모델. "recv를 미리 걸어놓고, OS 커널이 DMA로 데이터를 복사한 뒤 완료를 알려준다."

```
Reactor (Select/epoll):
  "소켓 A에 데이터 도착!" → 수동으로 recv() 호출

Proactor (IOCP):
  "미리 걸어둔 recv()가 완료됨. 여기 데이터." → OS가 이미 복사 완료
```

**핵심 특성**:
- Worker Thread Waiting Queue는 **LIFO** — 가장 최근 스레드를 깨워 캐시 히트율 극대화
- `NumberOfConcurrentThreads = CPU 코어 수` — 동시 실행 스레드 제한
- Worker 수 공식: `hardware_concurrency() × 2` (I/O 블로킹 여유분)
- 8~16 Worker Thread로 **10,000 동접** 처리 가능

### 서버 권위 모델

```
Client:  "나는 (100, 50)으로 이동하겠다"  ← 요청만
Server:  "속도 검증 통과. (100, 50) 승인"  ← 서버가 결정
Client:  "서버가 (100, 50)이라 했으니 거기 표시"  ← 서버 결과 반영
```

모든 판정(이동/공격/아이템/스킬)은 서버에서 수행. 클라이언트는 예측(prediction)만.

### 게임 서버 틱 시스템

```
[Tick N] ──── 50ms (20 TPS) ──── [Tick N+1]
   │
   ├─ ProcessRecvQueue()      ← 네트워크 수신 처리
   ├─ World.Tick(0.05f)       ← 고정 간격 월드 업데이트
   │  ├─ HandleMoveRequests()
   │  ├─ UpdateCombat()
   │  └─ UpdateBuffs()
   ├─ BroadcastState()        ← 클라이언트에 스냅샷 전송
   └─ Sleep(remaining)        ← CPU 과점유 방지
```

### 스레드 분리 원칙

```
IOCP Workers (4~8)  ──push──►  [Recv Queue]  ──pop──►  Logic Thread (1)
                                                              │
Logic Thread  ──push──►  [Send Queue]  ──pop──►  IOCP Workers
                                │
Logic Thread  ──push──►  [DB Queue]  ──pop──►  DB Workers (2~4)
```

**핵심**: 게임 로직은 **단일 스레드**. 모든 게임 상태에 독점 접근 → 락 불필요 → 동기화 버그 원천 차단.

---

## 2. 핵심 기술 (Core Tech)

### Win32 IOCP API

| API | 용도 |
|-----|------|
| `CreateIoCompletionPort()` | IOCP 핸들 생성 + 소켓 등록 |
| `GetQueuedCompletionStatus()` | Worker Thread 대기 + 완료 이벤트 수신 |
| `PostQueuedCompletionStatus()` | 종료 신호 전송 |
| `WSARecv()` / `WSASend()` | 비동기 수신/송신 (OVERLAPPED) |
| `AcceptEx()` | 비동기 Accept |

### 패킷 프로토콜 (기존 `Shared/PacketDef.h`)

```cpp
// 모든 패킷의 공통 헤더
struct PacketHeader {
    uint16_t    protocolVersion = 1;
    PacketType  type;
    uint16_t    size;       // 헤더 포함 전체 크기
    uint32_t    sequence;   // 송신 측 번호 (신뢰성)
    uint32_t    ack;        // 수신 확인 번호
};
```

**패킷 타입**: C2S_CONNECT, C2S_INPUT, C2S_PING / S2C_CONNECT_ACK, S2C_SNAPSHOT, S2C_SPAWN, S2C_DESPAWN, S2C_PONG

### Lock-Free Queue

```cpp
concurrency::concurrent_queue<PacketJob>  g_RecvQueue;   // IOCP → Logic
concurrency::concurrent_queue<SendJob>    g_SendQueue;   // Logic → IOCP
concurrency::concurrent_queue<DBJob>      g_DBQueue;     // Logic → DB Worker
```

### AOI (Area of Interest) Grid

```
50m × 50m 셀 그리드
플레이어 이동 시 셀 변경 감지 → 3×3 이웃 셀에만 브로드캐스트
→ 100명 전체 브로드캐스트 대비 ~90% 대역폭 절감
```

### Write-Back Cache 정책

| 데이터 | 정책 | 이유 |
|--------|------|------|
| 돈/아이템 거래 | **즉시 DB** | 복구 불가능한 손실 방지 |
| HP/위치/경험치 | **지연 저장** (1~5분) | 빈번한 변경, DB 부하 최소화 |
| 로그아웃 시 | **즉시 전체 저장** | 데이터 유실 방지 |

---

## 3. 아키텍처 다이어그램

### 전체 네트워크 구조

```
┌─────────────────────────────────────────────────────────────┐
│                  GAME CLIENT (Winters Engine)                 │
│  DX11 Renderer | ECS | Network Layer | Input/Camera          │
│  ├─ NetworkClient (TCP 연결 + RecvBuffer)                    │
│  ├─ PacketSender / PacketReceiver                            │
│  └─ NetworkInterpolator (Server 20TPS → Client 60FPS lerp)  │
└────────────────────┬────────────────────────────────────────┘
                     │ TCP (향후 UDP/KCP 추가)
                     ▼
┌─────────────────────────────────────────────────────────────┐
│              DISTRIBUTED C++ SERVERS (IOCP)                  │
│                                                              │
│  Gate Server ◄──────► Login Server                          │
│      │                     │                                │
│      └────────┬────────────┘                                │
│               ▼                                              │
│         World Server(s) ← 게임 로직 심장부                   │
│   ┌─────────────────────────────┐                           │
│   │ IOCP Workers (8~16)        │ ← 비동기 I/O              │
│   │ Logic Thread (1)            │ ← 게임 로직 단일 스레드   │
│   │ DB Workers (2~4)            │ ← 비동기 DB 쿼리          │
│   │ AOI Grid (50m 셀)          │ ← 관심영역 브로드캐스트    │
│   └─────────────────────────────┘                           │
│               │                                              │
│       ┌───────┼───────────┬──────────────┐                  │
│       ▼       ▼           ▼              ▼                  │
│   Center   Chat     Match Server    Zone Manager            │
│   Server   Server                                            │
└────────────────┬───────────────────────┬────────────────────┘
                 │                       │
                 ▼                       ▼
        ┌──────────────────┐    ┌──────────────────┐
        │  PostgreSQL DB   │    │  Redis Cache      │
        │  (Persistent)    │    │  (Session/Score)  │
        └──────────────────┘    └──────────────────┘
```

### 서버 내부 스레드 구조

```
┌──────────────────────────────────────────────────────────────┐
│                        World Server                           │
│                                                              │
│  ┌───────────────┐   concurrent_queue   ┌────────────────┐  │
│  │ IOCP Worker 0 │───────push──────────►│                │  │
│  │ IOCP Worker 1 │───────push──────────►│  Recv Queue    │  │
│  │ IOCP Worker 2 │───────push──────────►│  (PacketJob)   │  │
│  │   ...         │───────push──────────►│                │  │
│  │ IOCP Worker N │───────push──────────►└───────┬────────┘  │
│  └───────────────┘                              │ pop        │
│         ▲                                       ▼            │
│         │                              ┌─────────────────┐   │
│    Send Queue ◄────push───────────────│  Logic Thread   │   │
│    (SendJob)                          │  (Single!)      │   │
│                                       │  ├─ GameWorld   │   │
│                                       │  ├─ Players[]   │   │
│                                       │  ├─ AOI Grid    │   │
│                                       │  └─ CombatSys   │   │
│                                       └───────┬─────────┘   │
│                                               │ push         │
│                                       ┌───────▼─────────┐   │
│  ┌───────────────┐                   │  DB Queue       │   │
│  │ DB Worker 0   │◄─────pop─────────│  (DBJob)        │   │
│  │ DB Worker 1   │◄─────pop─────────└─────────────────┘   │
│  └───────────────┘                                          │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. 파일 목록

### 서버 신규 파일 (Wave 1~10 전체)

| Wave | 파일 경로 | 용도 | 핵심 타입 |
|------|-----------|------|-----------|
| 2 | `Server/Network/OverlappedEx.h` | OVERLAPPED 확장 | `IOType`, `OverlappedEx` |
| 2 | `Server/Network/IOCPCore.h\|cpp` | IOCP 핸들 + Worker Thread | `IOCPCore` |
| 2 | `Server/Network/Listener.h\|cpp` | AcceptEx 비동기 Accept | `Listener` |
| 3 | `Server/Network/RecvBuffer.h\|cpp` | 64KB 링 버퍼 | `RecvBuffer` |
| 3 | `Server/Network/SendQueue.h\|cpp` | 송신 큐 | `SendQueue` |
| 3 | `Server/Network/Session.h\|cpp` | 연결 1개 = 세션 1개 | `Session` |
| 3 | `Server/Network/SessionManager.h\|cpp` | 세션 풀 + ID 재활용 | `SessionManager` |
| 4 | `Server/Packet/PacketHandler.h\|cpp` | 패킷 ID → 핸들러 디스패치 | `PacketHandler` |
| 5 | `Server/Logic/ThreadArch.h` | Lock-Free Queue 타입 정의 | `PacketJob`, `SendJob`, `DBJob` |
| 6 | `Server/Logic/GameServer.h\|cpp` | 20 TPS 메인 루프 | `GameServer` |
| 6 | `Server/Logic/GameWorld.h\|cpp` | 월드 시뮬레이션 | `GameWorld` |
| 6 | `Server/Logic/Player.h\|cpp` | 서버 측 플레이어 | `Player` |
| 6 | `Server/Logic/CombatSystem.h\|cpp` | 전투 판정 (서버 권위) | `CombatSystem` |
| 7 | `Server/Logic/AOIGrid.h\|cpp` | 50m 셀 그리드 | `AOIGrid` |
| 8 | `Server/Zone/ZoneManager.h\|cpp` | 존/채널 관리 | `ZoneManager`, `Zone`, `Channel` |
| 6 | `Server/DB/DBWorker.h\|cpp` | 비동기 DB Worker Pool | `DBWorker` |
| 6 | `Server/DB/PlayerCache.h\|cpp` | Write-Back 캐시 | `PlayerCache` |
| 7 | `Server/Distributed/GateServer.h\|cpp` | 게이트 서버 | `GateServer` |
| 7 | `Server/Distributed/LoginServer.h\|cpp` | 로그인 서버 | `LoginServer` |
| 9 | `Server/Distributed/CenterServer.h\|cpp` | 중앙 서버 | `CenterServer` |
| 9 | `Server/Distributed/ChatServer.h\|cpp` | 채팅 서버 | `ChatServer` |
| 9 | `Server/Distributed/MatchServer.h\|cpp` | 매치 서버 | `MatchServer` |
| 9 | `Server/Distributed/ServerIPC.h\|cpp` | 서버 간 통신 | `ServerIPC` |

### Shared 신규/수정 파일

| 파일 경로 | 용도 |
|-----------|------|
| `Shared/PacketDef.h` | 기존 유지 (이미 구현됨) |
| `Shared/PacketID.h` | 패킷 ID 확장 (SS_ 서버간 패킷 추가) |
| `Shared/GameConst.h` | TICK_RATE, AOI_CELL_SIZE, MAX_SPEED 등 |
| `Shared/MathUtils.h` | 거리 계산, AABB 충돌 유틸 |

### 클라이언트 네트워크 파일

| 파일 경로 | 용도 |
|-----------|------|
| `Client/Header/Network/NetworkClient.h` | TCP 연결 + RecvBuffer |
| `Client/Code/Network/NetworkClient.cpp` | WSAStartup, connect, recv 루프 |
| `Client/Header/Network/PacketSender.h` | 패킷 직렬화 + 송신 |
| `Client/Code/Network/PacketSender.cpp` | send() 래핑 |
| `Client/Header/Network/PacketReceiver.h` | 수신 패킷 디스패치 |
| `Client/Code/Network/PacketReceiver.cpp` | RecvBuffer + 핸들러 호출 |
| `Client/Header/Network/NetworkInterpolator.h` | 20 TPS → 60 FPS 보간 |
| `Client/Code/Network/NetworkInterpolator.cpp` | 선형 보간 + 스냅샷 버퍼링 |

### 수정 파일

| 파일 경로 | 변경 내용 |
|-----------|-----------|
| `Server/Code/main.cpp` | placeholder → GameServer 초기화 + 실행 |
| `Server/Server.vcxproj` | 신규 파일 등록 + ws2_32.lib 확인 |
| `Client/Client.vcxproj` | Network 파일 등록 + ws2_32.lib 추가 |
| `Client/Header/CGameApp.h` | NetworkClient 멤버 + ProcessNetwork() |
| `Client/Code/CGameApp.cpp` | OnUpdate에서 네트워크 처리 통합 |

---

## 5. 핵심 구조체/클래스 시그니처

### 5-1. `Server/Network/OverlappedEx.h`

```cpp
#pragma once
#include <WinSock2.h>
#include <cstdint>

enum class IOType : uint8_t { Accept, Recv, Send };

struct OverlappedEx : OVERLAPPED
{
    IOType   type   = IOType::Recv;
    SOCKET   socket = INVALID_SOCKET;
    WSABUF   wsaBuf{};
    char     buffer[4096]{};

    OverlappedEx()
    {
        ZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED));
        wsaBuf.buf = buffer;
        wsaBuf.len = sizeof(buffer);
    }

    void Reset()
    {
        ZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED));
    }
};
```

### 5-2. `Server/Network/IOCPCore.h`

```cpp
#pragma once
#include "OverlappedEx.h"
#include <thread>
#include <vector>

class IOCPCore
{
public:
    bool    Initialize(int workerCount = 0);     // 0 = auto (hardware_concurrency * 2)
    void    RegisterSocket(SOCKET sock);
    void    PostRecv(OverlappedEx* pOvl);
    void    PostSend(SOCKET sock, const char* data, int len);
    void    Shutdown();

private:
    void    WorkerThread();

    HANDLE                  m_hIOCP = INVALID_HANDLE_VALUE;
    std::vector<std::thread> m_Workers;
    bool                    m_bRunning = false;
};
```

### 5-3. `Server/Network/Session.h`

```cpp
#pragma once
#include "RecvBuffer.h"
#include "SendQueue.h"
#include "OverlappedEx.h"

class Session
{
public:
    void    OnConnected(SOCKET sock, uint64_t sessionID);
    void    OnDisconnected();
    void    OnRecvCompleted(int bytes);
    void    Send(const char* data, int len);

    uint64_t    GetSessionID() const    { return m_SessionID; }
    uint64_t    GetPlayerID() const     { return m_PlayerID; }
    void        SetPlayerID(uint64_t id){ m_PlayerID = id; }
    bool        IsConnected() const     { return m_bConnected; }

private:
    SOCKET          m_Socket        = INVALID_SOCKET;
    uint64_t        m_SessionID     = 0;
    uint64_t        m_PlayerID      = 0;
    bool            m_bConnected    = false;
    int32_t         m_ZoneID        = -1;
    RecvBuffer      m_RecvBuffer;
    SendQueue       m_SendQueue;
    OverlappedEx    m_RecvOverlapped;
    bool            m_bSending      = false;
};
```

### 5-4. `Server/Network/RecvBuffer.h`

```cpp
#pragma once
#include <cstdint>
#include "../../Shared/PacketDef.h"

class RecvBuffer
{
public:
    static constexpr int BUFFER_SIZE = 65536;   // 64KB

    void                Write(const char* data, int len);
    const PacketHeader* PeekPacket() const;
    void                PopPacket(int packetSize);

    char*   WritePtr()  { return m_Buffer + m_WritePos; }
    int     FreeSize()  { return BUFFER_SIZE - m_WritePos; }

private:
    char    m_Buffer[BUFFER_SIZE]{};
    int     m_ReadPos   = 0;
    int     m_WritePos  = 0;
};
```

### 5-5. `Server/Logic/GameServer.h`

```cpp
#pragma once
#include "GameWorld.h"
#include "../Network/IOCPCore.h"
#include "../Network/SessionManager.h"

class GameServer
{
public:
    bool    Initialize(uint16_t port);
    void    Run();                          // 20 TPS 메인 루프
    void    Shutdown();

private:
    void    ProcessRecvQueue();
    void    ProcessSendQueue();
    void    SleepRemaining(/* ... */);

    IOCPCore        m_IOCP;
    SessionManager  m_SessionMgr;
    GameWorld       m_World;
    bool            m_bRunning = false;
};
```

### 5-6. `Server/Logic/AOIGrid.h`

```cpp
#pragma once
#include <unordered_set>
#include <unordered_map>
#include <cstdint>

class AOIGrid
{
public:
    static constexpr float CELL_SIZE = 50.f;

    struct CellCoord { int32_t x, z; };

    void    AddPlayer(uint32_t playerID, float posX, float posZ);
    void    RemovePlayer(uint32_t playerID);
    void    UpdatePosition(uint32_t playerID, float newX, float newZ);

    // 3x3 이웃 셀의 플레이어 ID 목록
    std::vector<uint32_t> GetNearbyPlayers(uint32_t playerID) const;

private:
    CellCoord   ToCellCoord(float x, float z) const;

    std::unordered_map<uint64_t, std::unordered_set<uint32_t>> m_Cells;
    std::unordered_map<uint32_t, CellCoord> m_PlayerCells;
};
```

### 5-7. `Client/Header/Network/NetworkClient.h`

```cpp
#pragma once
#include <string>
#include <functional>
#include <WinSock2.h>
#include "../../Shared/PacketDef.h"

class NetworkClient
{
public:
    bool    Connect(const std::string& ip, uint16_t port);
    void    Disconnect();
    void    Send(const PacketHeader* packet);
    void    ProcessNetwork();               // 게임루프에서 매 프레임 호출

    using PacketCallback = std::function<void(const PacketHeader*)>;
    void    RegisterHandler(PacketType type, PacketCallback cb);

    bool    IsConnected() const { return m_bConnected; }

private:
    void    RecvLoop();                     // 별도 스레드에서 실행

    SOCKET              m_Socket = INVALID_SOCKET;
    bool                m_bConnected = false;
    RecvBuffer          m_RecvBuffer;
    std::unordered_map<PacketType, PacketCallback> m_Handlers;
    std::mutex          m_PacketMutex;
    std::vector<std::vector<char>> m_PendingPackets;    // recv 스레드 → 메인 스레드
};
```

### 5-8. `Client/Header/Network/NetworkInterpolator.h`

```cpp
#pragma once
#include "../../Shared/PacketDef.h"

class NetworkInterpolator
{
public:
    void    PushSnapshot(const S2C_SnapshotPacket& snapshot);
    void    Interpolate(float deltaTime);

    PlayerState GetInterpolatedState(uint32_t playerID) const;

private:
    struct SnapshotEntry {
        uint32_t    serverTick;
        float       timestamp;
        S2C_SnapshotPacket data;
    };

    static constexpr int MAX_SNAPSHOTS = 4;
    SnapshotEntry   m_Snapshots[MAX_SNAPSHOTS]{};
    int             m_Count = 0;
    float           m_RenderTime = 0.f;     // 100ms 뒤 (2틱 지연 보간)
};
```

---

## 6. IOCP 5대 함정 (반드시 준수)

| # | 함정 | 대응 |
|---|------|------|
| 1 | OVERLAPPED 재사용 시 초기화 누락 | `ZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED))` 매 WSARecv 전 |
| 2 | `WSA_IO_PENDING`을 에러로 착각 | `WSAGetLastError() == WSA_IO_PENDING`이면 **정상** (비동기 진행 중) |
| 3 | 소켓 닫기 순서 | `closesocket()` → 마지막 completion notification 처리 후 세션 정리 |
| 4 | Send 버퍼 수명 | 스택 변수 금지, 힙 할당 or OverlappedEx 내부 버퍼 사용 |
| 5 | 동시 WSARecv | 소켓당 **최대 1개** concurrent recv |

---

## 7. 구현 순서 (Wave 기반)

```
Wave 1:  [01] Network Basics (학습)     +  [04] Shared 패킷 확장
Wave 2:  [02] IOCP Core (OverlappedEx, IOCPCore, Listener)
Wave 3:  [03] Session + RecvBuffer + SendQueue + SessionManager
Wave 4:  [05] PacketHandler (ID → 핸들러 디스패치)
Wave 5:  [06] Thread Architecture (Recv/Send/DB Queue 분리)
Wave 6:  [07] GameServer (20 TPS)  +  [10] DB Cache  +  [11] Redis      ← 최대 병렬 3
Wave 7:  [08] AOI Grid             +  [12] Gate/Login Server
Wave 8:  [09] Zone/Channel System
Wave 9:  [13] Center/Chat/Match    +  [14] Server IPC
Wave 10: [15] Client Integration (NetworkClient + Interpolator)
```

**예상 총 소요**: ~47일 (6~7주)

---

## 8. vcxproj 변경

### Server.vcxproj

```xml
<!-- 신규 폴더 필터 -->
<Filter Include="Network" />
<Filter Include="Packet" />
<Filter Include="Logic" />
<Filter Include="Zone" />
<Filter Include="DB" />
<Filter Include="Distributed" />

<!-- 링크: ws2_32.lib (이미 포함 확인) -->
<AdditionalDependencies>ws2_32.lib;%(AdditionalDependencies)</AdditionalDependencies>

<!-- /utf-8 필수 -->
<AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
```

### Client.vcxproj

```xml
<!-- 신규 필터 -->
<Filter Include="Network" />

<!-- 링크 추가 -->
<AdditionalDependencies>ws2_32.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

---

## 9. 테스트 계획

### 단위 테스트
1. **OverlappedEx**: ZeroMemory 후 wsaBuf 정상 초기화 확인
2. **RecvBuffer**: 64KB 가득 채우기 → PeekPacket → PopPacket → 리셋 확인
3. **SessionManager**: 100개 Acquire/Release → ID 재활용 → FindSession
4. **AOI Grid**: 플레이어 추가/이동/제거 → GetNearbyPlayers 정확성
5. **PacketHandler**: 알 수 없는 패킷 ID → 에러 로깅 (크래시 없음)

### 통합 테스트
1. **Echo 서버**: 클라이언트 connect → send → recv 동일 데이터 확인
2. **10 클라이언트 동접**: 모두 connect → 각자 input → snapshot 수신
3. **GameServer 20 TPS**: 틱 간격 50±2ms 유지 확인
4. **AOI 브로드캐스트**: 멀리 있는 플레이어는 snapshot에 미포함
5. **서버 권위**: 클라이언트가 비정상 좌표 전송 → 서버에서 거부

### 스트레스 테스트
1. **100 동접**: IOCP Worker 8개로 100명 동시 접속 + 입력 처리
2. **패킷 폭풍**: 1명이 초당 1000개 패킷 전송 → Rate Limiting 확인
3. **긴 세션**: 24시간 연속 접속 → 메모리 누수 없음 확인

### 검증 방법
- `Wireshark`: 패킷 캡처로 프로토콜 검증
- `Task Manager`: Worker Thread CPU 분배 확인
- `IOCP 로그`: 세션 생성/삭제/패킷 수신/송신 카운터
- `네트워크 시뮬레이터`: 지연/패킷 로스 시뮬레이션 (Clumsy)
