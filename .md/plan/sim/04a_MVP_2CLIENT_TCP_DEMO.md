# Phase 04a — MVP 2-Client TCP Demo

**작성일**: 2026-04-30
**위치**: v1.2 `04_IOCP_GAMEROOM.md` 의 본격 박제 (TCP) + Sim-10 v2 의 M1 (UDP) prerequisite
**목적**: **현재 코드베이스 sketch 를 마무리해서 1 server + 2 client 동기화 시연**. UDP 마이그 / Delta / AOI / Prediction 전부 보류.
**전제**: ① Codex 보정 적용된 `Server.vcxproj` (`/fp:precise` + `Mswsock.lib`) ② Engine/Client `Precise` 검증됨 ③ Shared/GameSim + Schemas 박제 완료 ④ Server sketch (IOCPCore 77줄, FrameParser 39줄, Session 40줄, 나머지 0)

**한 줄 명제**: **현 sketch + Shared 인프라 → 1 server (TCP IOCP, 30Hz tick) + 2 client (full snapshot apply) → A 이동/스킬 → B 화면 반영. 9 Server 작업 + 5 Client 작업 + 3 검증 = 약 35-50h. 합격 = LAN 2-client demo.**

---

## 0. 이 계획서의 위치

```
v1.2 04 (TCP IOCP outline)        ← reference
        │
        ▼
   ★ 본 계획서 04a (MVP demo)      ← 지금
        │
        ▼ (demo 합격 후)
   Sim-10 v2 M1 (UDP transport)    ← 다음
        │
        ▼
   M2 Reliability / M3 Delta+AOI / M4-M6
```

**왜 TCP 로 시작**: 현 sketch (IOCPCore + FrameParser + Session) 가 TCP 가정. UDP 로 가려면 처음부터 다시 = 추가 50-60h. 본 계획서는 sketch 를 살려서 먼저 **시각적으로 동작하는 demo** 까지만 가는 게 목표. UDP 의 production 효과 (HoL 회피 등) 는 Sim-10 v2 M1 에서.

---

## 1. 현재 코드베이스 진단

### Server (sketch)
| 파일 | 줄수 | 상태 | 본 계획서에서 |
|---|---|---|---|
| `Server/Public/Network/IOCPCore.h` | ~50 | typo (`workterCount`) + IOContext 에 `acceptSocket` 없음 | **D-1B** 본격화 |
| `Server/Private/Network/IOCPCore.cpp` | 77 | 부분 | **D-1B** 마무리 |
| `Server/Public/Network/FrameParser.h` | 22 | `TryExtract(ParsedFrame&, bool&)` consume 미완성 | **D-1C** TryPop 으로 변경 |
| `Server/Private/Network/FrameParser.cpp` | 39 | 부분 | **D-1C** 마무리 |
| `Server/Public/Network/Session.h` | 40 | 멤버만, public API 0 | **D-1D** 전면 박제 |
| `Server/Private/Network/Session.cpp` | 0 | 빈 | **D-1D** 신규 |
| `Server/Public/Network/Session_Manager.h` | 0 | 빈 | **D-1E** 신규 |
| `Server/Private/Network/Session_Manager.cpp` | 0 | 빈 | **D-1E** 신규 |
| `Server/Public/Network/PacketDispatcher.h` | 0 | 빈 | **D-1F** 신규 |
| `Server/Private/Network/PacketDispatcher.cpp` | 0 | 빈 | **D-1F** 신규 |
| `Server/Public/Game/GameRoom.h` | ~50 | sketch + typo (`m_tickIndezx`) | **D-1G** 전면 재작성 |
| `Server/Private/Game/GameRoom.cpp` | 0 | 빈 | **D-1G** 신규 |
| `Server/Public/Game/SnapshotBuilder.cpp` | 20 | header 없음 (cpp 안 class) | **D-1H** header 분리 |
| `Server/Public/Security/AntiCheatServer.h` | 0 | 빈 | **D-1G 통합** (range/cooldown 최소) |
| `Server/Private/main.cpp` | 39 | placeholder | **D-1I** bootstrap |
| `Server/Include/Server.vcxproj` | OK | `/fp:precise` + `Mswsock.lib` ✅ | **D-1A** Engine reference 추가 |

### Shared (활용 가능 ✅)
| 파일 | 상태 |
|---|---|
| `Shared/Network/PacketEnvelope.h` | 16-byte header, magic 0x5742, ePacketType, `WrapEnvelope`/`TryExtractFrame` ✅ |
| `Shared/GameSim/EntityIdMap.h` | 양방향 lookup ✅ |
| `Shared/GameSim/DeterministicRng.h` | XorShift64* + sub-stream ✅ |
| `Shared/GameSim/Systems/ICommandExecutor.h` | TickContext, GameCommandWire, GameCommand, BuildServerCommand, CDefaultCommandExecutor ✅ |
| `Shared/GameSim/Components/*` | Stat/Health/Skill/Buff/PendingHit/Champion ✅ |
| `Shared/Schemas/Command.fbs` | CommandBatch ✅ |
| `Shared/Schemas/Snapshot.fbs` | EntitySnapshot 16 필드 + Snapshot root ✅ (delta 무시 — full 만 사용) |

### Client (network 신규)
| 파일 | 상태 | 본 계획서에서 |
|---|---|---|
| `Client/Public/Network/CommandSerializer.h` | stub | **D-2B** 본격 |
| `Client/Private/Network/CommandSerializer.cpp` | stub | **D-2B** 본격 |
| `Client/Public/Network/SnapshotApplier.h` | stub | **D-2C** 본격 |
| `Client/Private/Network/SnapshotApplier.cpp` | stub | **D-2C** 본격 |
| `Client/Public/Network/CClientNetwork.h` | **존재 안함** | **D-2A** 신규 |
| `Client/Private/Network/CClientNetwork.cpp` | **존재 안함** | **D-2A** 신규 |
| `Client/Private/Scene/Scene_InGame.cpp` | InGame 본격 | **D-2D** 통합 (Network connect, snapshot apply hook) |
| `Client/Private/GamePlay/InputSystem.cpp` | 입력 캡처 | **D-2E** Move/Skill 시 CommandBatch send |

---

## 2. MVP 정의 — 무엇을 demo 하는가

### 시각적 합격 기준
1. **Server 콘솔**: "Server listening on port 9000" → "Client A connected (sessionId=1)" → "Client B connected (sessionId=2)" → "Tick 30Hz running"
2. **Client A** (PC1 또는 localhost instance 1): 본인 챔피언 + Client B 챔피언 둘 다 보임. 본인 우클릭 이동 시 본인 캐릭터 이동.
3. **Client B** (PC2 또는 localhost instance 2): 본인 챔피언 + Client A 챔피언 둘 다 보임. **Client A 이동이 1-2 frame 지연 후 Client B 화면에 반영됨**.
4. **양쪽 동시 입력**: A 가 이동 + B 가 스킬 → 양쪽 다 시각 반영.

### MVP 미포함 (다음 사이클)
| 영역 | 이유 |
|---|---|
| UDP transport | Sim-10 v2 M1 — 별도 50h |
| Reliability 3-channel | Sim-10 v2 M2 |
| Snapshot delta | Sim-10 v2 M3 (현재는 full snapshot broadcast) |
| AOI | Sim-10 v2 M3 (현재는 모든 entity broadcast) |
| Lag compensation | Sim-10 v2 M4 |
| Client prediction | Sim-10 v2 M5 (현재는 server 권위 결과를 즉시 표시 — visible lag) |
| Render interpolation | Sim-10 v2 M5 (현재는 snap-to) |
| Encryption | Sim-11 |
| Replay | Sim-10 v2 M6 |
| Hook 2 분리 (Visual/Gameplay) | 별도 마이그 사이클 |

### MVP 결정성 1차 (M1 가드 적용)
- `Server.vcxproj` `/fp:precise` ✅
- `CGameRoom::Phase_DrainCommands` `std::stable_sort + tie-breaker` ✅
- `unordered_map` allowlist (Registries / EntityIdMap) ✅

---

## 3. Phase D-1 Server 본격 (9 작업)

### D-1A — Server.vcxproj Engine project reference (★ Codex 미반영분)

**목표**: Server EXE 가 `WintersEngine.dll` 의 `CWorld`/ECS/Components 를 link 할 수 있게.

**작업**:
- `Server/Include/Server.vcxproj` 의 `<ItemGroup>` 에 `<ProjectReference Include="..\..\Engine\Include\Engine.vcxproj">` 추가
- 또는 `<AdditionalLibraryDirectories>` + `<AdditionalDependencies>` 에 `WintersEngine.lib` 추가 (DLL 경로는 PostBuild 또는 PATH)
- `<AdditionalIncludeDirectories>` 에 `EngineSDK\inc` 또는 `Engine\Include`, `Engine\Public` 추가
- Pre-build: `Shared/Schemas/run_codegen.bat` (이미 vcxproj 에 박혀있을 가능성)

**합격**: `#include "ECS/World.h"` 가 컴파일 통과.

---

### D-1B — CIOCPCore 본격 (typo + AcceptEx 수명)

**목표**: listen socket + N worker thread + AcceptEx 정상 흐름.

**작업**:
- `IOCPCore.h` typo 수정: `workterCount` → `workerCount`, `workterId` → `workerId`
- `IOContext` 에 `SOCKET acceptSocket = INVALID_SOCKET;` 추가 (AcceptEx 전용)
- `IOCPCore.cpp` 의 `AcceptLoop` / `WorkerLoop` 마무리:
  - `PostAccept` → `AcceptEx` 호출 → IOContext 가 acceptSocket 소유
  - completion 시 `setsockopt(SO_UPDATE_ACCEPT_CONTEXT)` → `CSession_Manager::OnAccept` → `BindIOCP` → `PostInitialRecv`
- `Shutdown` 시 worker thread join + listen socket close

**핵심 인터페이스 (현 sketch 유지 + 보완)**:
```cpp
// Server/Public/Network/IOCPCore.h
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
    bool PostAccept(SOCKET listenSocket);
    bool BindIOCP(SOCKET socket, u32_t sessionId);

    HANDLE m_hIOCP = nullptr;
    SOCKET m_listenSocket = INVALID_SOCKET;
    u16_t  m_port;
    u32_t  m_workerCount;
    std::vector<std::thread> m_workers;
    std::thread m_acceptThread;
    std::atomic<bool> m_bRunning{ false };
};

// IOContext 보강
struct IOContext
{
    OVERLAPPED overlapped{};
    WSABUF wsaBuf{};
    char buffer[8192]{};
    eIOOp op = eIOOp::Recv;
    u32_t sessionId = 0;
    SOCKET acceptSocket = INVALID_SOCKET;   // ★ AcceptEx 수명 보장
};
```

**합격**: 1 client telnet 으로 connect/disconnect 반복 시 leak 없음.

---

### D-1C — CFrameParser TryPop + Invalid 분리

**목표**: partial / sticky packet 안전 처리. magic/version mismatch → Invalid → caller 가 disconnect.

**작업**:
- `FrameParser.h` API 변경:
  - 기존: `bool TryExtract(ParsedFrame&, bool& bAbort)` — consume 미완성
  - 신규: `eFrameParseResult TryPop(ParsedFrameOwned&)` — `Complete` 시 buffer consume + payload owned copy
- `eFrameParseResult { NeedMore, Complete, Invalid }`
- `ParsedFrameOwned { type, sequence, vector<u8_t> payload }` (Shared/Network/PacketEnvelope.h 의 `ParsedFrame` 은 non-owned, 그대로 둠)
- 한도: payload 64KB, accumulated buffer 256KB → Invalid

**핵심 인터페이스**:
```cpp
// Server/Public/Network/FrameParser.h
enum class eFrameParseResult : u8_t
{
    NeedMore = 0,
    Complete = 1,
    Invalid  = 2,
};

struct ParsedFrameOwned
{
    ePacketType type = ePacketType::None;
    u32_t sequence = 0;
    std::vector<u8_t> payload;
};

class CFrameParser
{
public:
    void Append(const u8_t* bytes, u32_t len);
    eFrameParseResult TryPop(ParsedFrameOwned& outFrame);
    u32_t BufferedBytes() const { return static_cast<u32_t>(m_Buffer.size()); }
    void Clear();

private:
    static constexpr u32_t kMaxPayloadBytes = 64 * 1024;
    static constexpr u32_t kMaxBufferBytes  = 256 * 1024;
    std::vector<u8_t> m_Buffer;
};
```

**합격**:
- envelope 1 byte 씩 분할 송신 → 1 frame 으로 복원
- envelope 2개 1 chunk → 2 frame 순서대로 추출
- bad magic/version/payloadSize > 64KB → Invalid

---

### D-1D — CSession 본격 (recv/send/disconnect public API)

**목표**: 1 socket 에 대한 비동기 recv/send + send queue + sequence guard.

**작업**:
- `Session.h` 에 public API 추가:
  - `static shared_ptr<CSession> Create(SOCKET, u32_t sessionId)`
  - `bool PostInitialRecv()` / `bool PostRecv()`
  - `void OnRecvComplete(const u8_t*, u32_t)` — Worker thread 진입점
  - `void OnSendComplete(u32_t)`
  - `bool Send(std::vector<u8_t> packet)` — buffer ownership 전달
  - `void OnDisconnect()`
  - `EntityID GetControlledEntity() const`, `void SetControlledEntity(EntityID)`
  - `CFrameParser& GetRecvParser()`
  - sequence guard: `bool TryAcceptSequence(u32_t seq, bool& bSuspicious)`
  - lifetime: `AddPendingIo()` / `CompletePendingIo()` / `CanDestroy()`
- `Session.cpp` 신규 (현재 0줄):
  - `WSARecv` / `WSASend` 호출 + send queue mutex 보호
  - `OnRecvComplete` → `m_recvParser.Append` → `CPacketDispatcher::Instance().DrainFrames(sid, parser)` → 다음 `PostRecv` 예약

**합격**: 1 client connect → telnet 으로 16 bytes hex envelope 입력 → server 가 frame extract + dispatcher 호출 (log 확인).

---

### D-1E — CSession_Manager 신규

**목표**: `sessionId → shared_ptr<CSession>` 등록/조회/회수. AcceptEx completion 진입점.

**작업**:
- `Session_Manager.h/.cpp` 신규
- API:
  - `static CSession_Manager* Get()` — singleton
  - `shared_ptr<CSession> OnAccept(SOCKET, sockaddr_in)` — IOCPCore 가 호출
  - `void OnDisconnect(u32_t sessionId)`
  - `void OnRecvComplete(u32_t, const u8_t*, u32_t)` / `void OnSendComplete(u32_t, u32_t)`
  - `shared_ptr<CSession> Find(u32_t sessionId)`
  - `void ForEach(function<void(CSession&)>)` — sessionId 정렬 순회 (결정성)
  - `size_t Count() const`
- 내부:
  - `unordered_map<u32_t, shared_ptr<CSession>> m_sessions` (allowlist — sim 외 lookup)
  - `vector<shared_ptr<CSession>> m_closingSessions` — outstanding OVERLAPPED 완료까지 수명 유지
  - `mutex m_mutex` — accept/disconnect race 방지

**합격**: 1 client connect → SessionId 발급 → Find OK. disconnect 후 outstanding completion 도착해도 use-after-free 없음.

---

### D-1F — CPacketDispatcher 신규

**목표**: FrameParser drain → CommandBatch FlatBuffers verify → Room queue.

**작업**:
- `PacketDispatcher.h/.cpp` 신규
- API:
  - `static CPacketDispatcher& Instance()` — singleton (Registry 류 컨벤션)
  - `void DrainFrames(u32_t sessionId, CFrameParser& parser)` — `TryPop` loop
  - `void DispatchCommandBatch(u32_t sessionId, const ParsedFrameOwned&)` — `VerifyCommandBatchBuffer` → `CGameRoom::EnqueueCommand`
  - `void RegisterRoom(u32_t roomId, CGameRoom*)`
  - `void RouteSession(u32_t sessionId, u32_t roomId)` — MVP 는 1 룸이라 단순
- Heartbeat = no-op (FrameParser 통과 자체가 keep-alive)
- Unknown type → `pSession->FlagSuspicious()`

**합격**: dummy CommandBatch 송신 → server 가 `EnqueueCommand` 호출까지 도달.

---

### D-1G — CGameRoom 본격 (★ 핵심 — 30Hz tick + stable_sort drain)

**목표**: 30Hz 단일 thread tick + Worker → Tick 큐 drain (stable_sort) + Shared System chain + AntiCheat 1차 + full snapshot broadcast.

**작업**:
- `GameRoom.h` 전면 재작성 (현 sketch typo 폐기)
- `GameRoom.cpp` 신규
- 멤버:
  - `CWorld m_world`, `EntityIdMap m_entityMap`, `DeterministicRng m_rng`
  - `u64_t m_tickIndex`, `atomic<u64_t> m_visibleTickIndex`
  - `mutex m_pendingMutex`, `vector<PendingCommand> m_pendingCommands`
  - `vector<GameCommand> m_pendingExecCommands`
  - `vector<u32_t> m_sessionIds` (정렬 유지), `unordered_map<u32_t, EntityID> m_sessionToEntity` (allowlist)
- API:
  - `static unique_ptr<CGameRoom> Create(u32_t roomId)`
  - `void Start()` / `void Stop()`
  - `void EnqueueCommand(u32_t sessionId, GameCommandWire wire, u64_t acceptedTick)` — Worker thread 진입
  - `void OnSessionJoin(u32_t sessionId, EntityID assignedEntity)`
  - `void OnSessionLeave(u32_t sessionId)`
  - `u64_t GetCurrentTickIndex() const { return m_visibleTickIndex.load(); }`
- Tick (단일 thread, 30Hz `chrono::steady_clock` + sleep_until):
  1. `Phase_DrainCommands` — queue swap + **`std::stable_sort` (acceptedTick, sessionId, sequenceNum)** + AntiCheat + `BuildServerCommand`
  2. `Phase_ExecuteCommands` — `m_pExecutor->ExecuteCommand`
  3. `Phase_SimulationSystems` — Shared systems 고정 순서 (Stat/Buff/SkillCooldown/Move/PendingHit/Projectile/DamageQueue/Death)
  4. `Phase_BroadcastSnapshot` — 모든 session 에게 full snapshot send (AOI X)

**핵심 인터페이스**:
```cpp
// Server/Public/Game/GameRoom.h
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

    void EnqueueCommand(u32_t sessionId, GameCommandWire wire, u64_t acceptedTick);

    void OnSessionJoin(u32_t sessionId, EntityID assignedEntity);
    void OnSessionLeave(u32_t sessionId);

    u32_t GetRoomId() const { return m_roomId; }
    u64_t GetCurrentTickIndex() const { return m_visibleTickIndex.load(std::memory_order_relaxed); }

private:
    CGameRoom(u32_t roomId);

    void TickThread();
    void Tick();

    void Phase_DrainCommands(TickContext& tc);
    void Phase_ExecuteCommands(TickContext& tc);
    void Phase_SimulationSystems(TickContext& tc);
    void Phase_BroadcastSnapshot(TickContext& tc);

    u32_t m_roomId;
    std::atomic<bool> m_bRunning{ false };
    std::thread m_tickThread;

    CWorld m_world;
    EntityIdMap m_entityMap;
    DeterministicRng m_rng{ 0xC0FFEEull };
    u64_t m_tickIndex = 0;
    std::atomic<u64_t> m_visibleTickIndex{ 0 };

    std::unique_ptr<ICommandExecutor> m_pExecutor;
    std::unique_ptr<CSnapshotBuilder> m_pSnapBuilder;

    std::mutex m_pendingMutex;
    std::vector<PendingCommand> m_pendingCommands;
    std::vector<GameCommand>    m_pendingExecCommands;

    std::vector<u32_t> m_sessionIds;                            // 정렬 유지
    std::unordered_map<u32_t, EntityID> m_sessionToEntity;      // allowlist
};
```

**AntiCheat 1차 (D-1G 통합)**:
- range 검증 (CastSkill 사거리)
- cooldown 검증 (SkillStateComponent.cooldowns[slot] <= 0)
- 그 외 (targetMode/issuer spoof) 는 다음 사이클

**합격**:
- 30Hz tick 5분 jitter < 5ms
- 같은 input → 같은 결과 (deterministic order 검증)

---

### D-1H — CSnapshotBuilder header 분리 + full snapshot 직렬화

**목표**: `Server/Public/Game/SnapshotBuilder.h` 신규 + `Build()` 본격.

**작업**:
- 현재 `SnapshotBuilder.cpp` 안에 박힌 class 선언 → 별도 헤더로 분리
- `SnapshotBuilder.h`:
  ```cpp
  class CSnapshotBuilder final
  {
  public:
      static std::unique_ptr<CSnapshotBuilder> Create();

      // MVP: AOI 무시, 모든 entity 포함
      flatbuffers::DetachedBuffer Build(
          const CWorld& world,
          const EntityIdMap& entityMap,
          u64_t serverTick,
          u64_t rngState,
          u32_t lastAckedSeq,
          NetEntityId yourNetId);
  };
  ```
- `Build()` 본문:
  - `DeterministicEntityIterator<TransformComponent>::CollectSorted(world)` 로 entity sorted
  - 각 entity 의 Transform/Stat/Health/Mana/SkillState/SkillRank/Buff 읽어서 `EntitySnapshot` 박제
  - `CreateSnapshot(builder, ...)` 후 `builder.Finish` + `Release()`

**합격**: 5v5 = 10 entity Snapshot < 1KB.

---

### D-1I — Server/Private/main.cpp bootstrap

**목표**: placeholder → 실제 server 진입점.

**작업**:
- `WSAStartup`
- `CIOCPCore::Create(9000, 4)->Start()`
- `CGameRoom::Create(1)` + `CPacketDispatcher::Instance().RegisterRoom(1, room.get())`
- `room->Start()`
- 메인 thread 는 콘솔 입력 대기 (Q 누르면 종료)

**핵심 코드**:
```cpp
// Server/Private/main.cpp
#include "Network/IOCPCore.h"
#include "Network/Session_Manager.h"
#include "Network/PacketDispatcher.h"
#include "Game/GameRoom.h"
#include <iostream>
#include <WinSock2.h>

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    auto core = CIOCPCore::Create(9000, 4);
    if (!core || !core->Start())
        return 2;

    auto room = CGameRoom::Create(1);
    CPacketDispatcher::Instance().RegisterRoom(1, room.get());
    room->Start();

    std::cout << "Server listening on port 9000. Press Q+Enter to quit.\n";
    std::string line;
    while (std::getline(std::cin, line))
        if (line == "q" || line == "Q") break;

    room->Stop();
    core->Shutdown();
    WSACleanup();
    return 0;
}
```

**합격**: `WintersServer.exe` 실행 시 listen 시작 + Q 로 정상 종료.

---

## 4. Phase D-2 Client 본격 (5 작업)

### D-2A — CClientNetwork 신규

**목표**: TCP connect + send/recv worker thread + FrameParser mirror.

**파일**: `Client/Public/Network/CClientNetwork.h` + `Client/Private/Network/CClientNetwork.cpp` (신규)

**API**:
```cpp
class CClientNetwork final
{
public:
    static std::unique_ptr<CClientNetwork> Create();
    ~CClientNetwork();

    bool Connect(const char* host, u16_t port);
    void Disconnect();

    bool Send(std::vector<u8_t> packet);   // ownership 전달

    // recv worker 가 호출하는 callback (main thread 로 marshal)
    using FrameCallback = std::function<void(ePacketType, u32_t seq, const u8_t* payload, u32_t len)>;
    void SetFrameCallback(FrameCallback fn);

    // main thread 가 매 frame 호출 — 누적된 frame 들을 callback 으로 dispatch
    void PumpReceivedFrames();

    u32_t GetMyNetEntityId() const { return m_myNetId; }
    void  SetMyNetEntityId(u32_t id) { m_myNetId = id; }

private:
    CClientNetwork() = default;

    void RecvThread();

    SOCKET m_socket = INVALID_SOCKET;
    std::thread m_recvThread;
    std::atomic<bool> m_bRunning{ false };

    // Recv buffer + frame parser (Server CFrameParser mirror — Client 측 별도 박제 또는 공통 헤더)
    std::vector<u8_t> m_recvAccum;
    std::mutex m_recvMutex;
    std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>> m_pendingFrames;

    FrameCallback m_callback;
    u32_t m_myNetId = 0;
};
```

**합격**: `Connect("127.0.0.1", 9000)` → server log 에 "Client connected" 출력. `Disconnect()` 시 server 가 OnDisconnect 인지.

---

### D-2B — CCommandSerializer 본격

**목표**: 입력 (Move/CastSkill/BasicAttack) → `GameCommandWire` → `CommandBatch` flatbuffer → `WrapEnvelope(ePacketType::CommandBatch)` → `Send`.

**파일**: 기존 `Client/Public/Network/CommandSerializer.h` + `.cpp` 본격화.

**API**:
```cpp
class CCommandSerializer final
{
public:
    static std::unique_ptr<CCommandSerializer> Create();

    // 단일 command 즉시 송신 (MVP — batching 보류)
    void SendMove(CClientNetwork& net, NetEntityId targetNet, const Vec3& groundPos);
    void SendCastSkill(CClientNetwork& net, u8_t slot, NetEntityId targetNet,
                       const Vec3& groundPos, const Vec3& direction);
    void SendBasicAttack(CClientNetwork& net, NetEntityId targetNet);

private:
    CCommandSerializer() = default;

    // 내부 — CommandBatch flatbuffer 빌드 → envelope wrap
    std::vector<u8_t> BuildCommandBatch(const std::vector<GameCommandWire>& wires);

    u32_t m_nextSequenceNum = 1;
    u64_t m_clientTick = 0;
};
```

**합격**: 좌클릭 (BA) → server log 에 `EnqueueCommand` 호출 + Tick 안에서 ApplyDamage 발동.

---

### D-2C — CSnapshotApplier 본격

**목표**: `ePacketType::Snapshot` 수신 → FlatBuffers verify → `EntitySnapshot[]` → 로컬 `CWorld` 의 Position/Hp/AnimId 갱신.

**파일**: 기존 `Client/Public/Network/SnapshotApplier.h` + `.cpp` 본격화.

**API**:
```cpp
class CSnapshotApplier final
{
public:
    static std::unique_ptr<CSnapshotApplier> Create();

    // FrameCallback 으로 호출 (main thread)
    void OnSnapshot(CWorld& world, EntityIdMap& entityMap,
                    const u8_t* payload, u32_t len);

    u64_t GetLastAppliedServerTick() const { return m_lastServerTick; }

private:
    CSnapshotApplier() = default;

    // 새 entity (이전 snapshot 에 없던 NetEntityId) → CreateEntity + Bind + Component 박제
    EntityID OnNewEntity(CWorld& world, EntityIdMap& map,
                         const Shared::Schema::EntitySnapshot* es);

    // 기존 entity 갱신
    void ApplyToExisting(CWorld& world, EntityID e,
                         const Shared::Schema::EntitySnapshot* es);

    u64_t m_lastServerTick = 0;
    std::unordered_set<u32_t> m_seenNetIds;   // allowlist (sim 외 cache)
};
```

**MVP 처리**:
- 새 NetEntityId 발견 시 Champion entity blueprint 사용 (또는 minimal Transform + Stat 박제)
- 기존 entity 는 Transform.position / Health.current / SkillState.cooldowns 갱신
- **Render component 는 Scene_InGame 이 별도로 관리** (sim ECS 와 분리)

**합격**: server snapshot 30Hz 수신 → 다른 client 의 챔피언이 본인 화면에 보임.

---

### D-2D — Scene_InGame 통합

**목표**: Scene 진입 시 `CClientNetwork::Connect` + 매 frame `PumpReceivedFrames` + Snapshot callback 등록.

**파일**: `Client/Private/Scene/Scene_InGame.cpp` 수정.

**작업**:
- `OnEnter()`: `CClientNetwork::Create()` + `Connect("127.0.0.1", 9000)` (또는 IP 기반 — 메뉴에서 입력)
- `OnUpdate(dt)`:
  - `m_pNetwork->PumpReceivedFrames()` 호출
  - 입력 시 `m_pCommandSerializer->SendMove/SendCastSkill/...` 호출
- FrameCallback:
  - `ePacketType::Snapshot` → `m_pSnapshotApplier->OnSnapshot(m_World, m_EntityMap, payload, len)`
  - `ePacketType::Hello` → server 가 할당한 본인 NetEntityId 저장 (MVP — Hello packet 1차 박제)
- `OnExit()`: `m_pNetwork->Disconnect()`

**Hello packet (★ MVP 신규)**:
- Server 가 Session connect 시 **자동으로 Hello 송신** (sessionId, assignedEntity, NetEntityId)
- Client 가 `SetMyNetEntityId(id)` 로 본인 entity 식별
- Hello 도 `WrapEnvelope(ePacketType::Hello)` 사용 — payload 는 단순 struct (FlatBuffers 까지 갈 필요 X)

**합격**: 2 client 띄움 → 양쪽 본인 챔피언 + 다른 client 챔피언 둘 다 보임.

---

### D-2E — InputSystem → CommandSerializer hook

**목표**: 기존 `Client/Private/GamePlay/InputSystem.cpp` 의 우클릭 (Move) / Q/W/E/R (Skill) / 좌클릭 (BA) 입력에서 `CommandSerializer` 호출.

**작업**:
- 기존 InputSystem 이 직접 World 를 변경 (local-authoritative 패턴) → MVP 는 **server authoritative 로 마이그**:
  - 입력 시 `CommandSerializer::SendMove(...)` 만 호출 (local World 변경 X)
  - server 가 다음 snapshot 으로 본인 캐릭터 위치 반영
- **시각적 lag** (RTT 만큼 캐릭터 반응 지연) 발생 — Sim-10 v2 M5 Prediction 까지는 어쩔 수 없음. Demo 합격 기준에 포함.

**합격**: 우클릭 → server tick 후 1-2 frame 늦게 본인 캐릭터 이동 시작.

---

## 5. Phase D-3 검증 (3 작업)

### D-3A — 1 client localhost smoke

**목표**: Server + 1 Client 로 connect/disconnect/snapshot loop 검증.

**시나리오**:
1. `WintersServer.exe` 실행
2. `WintersGame.exe` 실행 → 메인메뉴 → InGame 진입
3. Server 콘솔에 "Client connected (sessionId=1, NetEntityId=X)" 출력
4. Client 화면에 본인 챔피언 표시
5. 우클릭 이동 → 1-2 frame 후 server snapshot 으로 본인 위치 갱신
6. `Q` 키로 Client 종료 → Server 콘솔에 "Client disconnected" 출력

**합격**: 5분 무중단 동작.

---

### D-3B — 2 client localhost smoke

**목표**: 1 server + 2 client (같은 PC, 2 instance) 로 동기화 시연.

**시나리오**:
1. Server 실행
2. Client A 실행 (port 자동) → InGame 진입 → Server 가 NetEntityId=1 할당
3. Client B 실행 → InGame 진입 → Server 가 NetEntityId=2 할당
4. Client A 화면에 본인 + Client B 챔피언 보임 (반대도 동일)
5. Client A 우클릭 이동 → Client B 화면에 Client A 캐릭터 이동 반영 (RTT < 50ms localhost)
6. Client B 가 스킬 (Q) → Client A 화면에 Client B 스킬 이펙트 시각화
7. 동시 입력 — 양쪽 다 잘 반영

**합격**: 5분 무중단 + 시각 반영.

---

### D-3C — Tick jitter 측정

**목표**: 30Hz tick 의 실제 주기 jitter 가 < 5ms 인지 검증.

**작업**:
- `CGameRoom::TickThread` 안에 jitter 측정 코드:
  ```cpp
  auto actualNext = clock::now();
  int64_t jitterMicros = duration_cast<microseconds>(actualNext - next).count();
  m_maxJitterMicros = std::max(m_maxJitterMicros, jitterMicros);
  ```
- 5분 구동 후 콘솔에 `max jitter = X us` 출력
- < 5ms (5000us) 이면 합격

**합격**: localhost 환경에서 max jitter < 5ms.

---

## 6. 의존성 그래프

```
Engine.vcxproj (이미 빌드됨, /fp:precise) ✅
        │
        ▼
Server.vcxproj (★ D-1A: Engine ref 추가)
        │
        ├── D-1B IOCPCore (typo + acceptSocket)
        ├── D-1C FrameParser (TryPop)
        ├── D-1D Session
        ├── D-1E Session_Manager
        ├── D-1F PacketDispatcher
        ├── D-1G GameRoom (★ stable_sort drain)
        ├── D-1H SnapshotBuilder (header 분리)
        └── D-1I main.cpp
        │
        ▼
WintersServer.exe ✅
        │
        ▼ (TCP 9000 listen)
        │
Client.vcxproj (이미 빌드됨, /fp:precise) ✅
        │
        ├── D-2A CClientNetwork (신규)
        ├── D-2B CommandSerializer (본격)
        ├── D-2C SnapshotApplier (본격)
        ├── D-2D Scene_InGame (Network hook)
        └── D-2E InputSystem (CommandSerializer hook)
        │
        ▼
WintersGame.exe (2 instance) ✅
        │
        ▼
   D-3 검증 (1 client / 2 client / jitter)
```

---

## 7. 시간 견적

| Phase | 작업 | 시간 |
|---|---|---|
| **D-1A** | Server.vcxproj Engine reference + 빌드 검증 | 1h |
| **D-1B** | IOCPCore typo + acceptSocket + AcceptEx 흐름 | 4h |
| **D-1C** | FrameParser TryPop + Invalid 분리 | 2h |
| **D-1D** | Session 본격 (recv/send/disconnect) | 5h |
| **D-1E** | Session_Manager 신규 | 3h |
| **D-1F** | PacketDispatcher 신규 | 3h |
| **D-1G** | **GameRoom 본격 (★ Tick + stable_sort + AntiCheat 1차)** | **8h** |
| **D-1H** | SnapshotBuilder header 분리 + Build 본격 | 3h |
| **D-1I** | main.cpp bootstrap | 1h |
| **D-2A** | CClientNetwork 신규 | 5h |
| **D-2B** | CommandSerializer 본격 | 3h |
| **D-2C** | SnapshotApplier 본격 (NetId mapping + Component 갱신) | 5h |
| **D-2D** | Scene_InGame 통합 + Hello packet | 3h |
| **D-2E** | InputSystem 마이그 (server authoritative) | 2h |
| **D-3A** | 1 client smoke | 1h |
| **D-3B** | 2 client smoke | 2h |
| **D-3C** | Jitter 측정 | 1h |
| **합계** | | **52h** |

**병렬 가능**: D-1B/C/D/E/F/G/H 는 Server side 라 순차. D-2 는 Server 어느 정도 진행 후 병렬 가능 (Client 가 dummy server mock 으로 먼저 박제 가능).

---

## 8. 종료 조건 (Demo 합격)

본 계획서 합격 = 다음 모두 통과:

1. ✅ `WintersServer.exe` 가 placeholder 가 아니라 IOCP 서버로 listen.
2. ✅ TCP framing — partial / sticky / bad header 정상 처리.
3. ✅ CommandBatch FlatBuffers verify 후 GameRoom queue 진입.
4. ✅ GameRoom 단일 thread 30Hz tick (jitter < 5ms).
5. ✅ stable_sort drain (acceptedTick, sessionId, sequenceNum) — 결정성 1차.
6. ✅ Snapshot full broadcast (AOI X, Delta X) — 모든 entity 모든 client 에게.
7. ✅ Client 가 본인 + 다른 client 챔피언 모두 시각화.
8. ✅ Client 입력 → server tick → 다른 client 화면 반영 (< 50ms localhost).
9. ✅ AntiCheat 1차 (range/cooldown) — 위반 시 reject.
10. ✅ 5분 무중단 동작 + connect/disconnect 반복 leak 없음.
11. ✅ `/fp:precise` 강제 + 회귀 grep 4종 (allowlist 외) 0 hit.

---

## 9. 합격 후 다음 단계

| 다음 사이클 | 영역 | 시간 |
|---|---|---|
| **Sim-10 v2 M1** | UDP 마이그 (TCP→UDP 포팅) | 50h |
| **Sim-10 v2 M2** | Reliability 3-channel + Fragment | 30h |
| **Sim-10 v2 M3** | Snapshot Delta + AOI + LOD | 50h |
| **Sim-10 v2 M5** | Client Prediction (시각 lag 제거) | 60h |
| Sim-11 | Encryption | 30h |
| ... | ... | ... |

본 계획서 (04a) 합격은 **Sim-10 v2 M1 진입의 prerequisite** — UDP 로 가기 전에 TCP 위에서 동기화 흐름 자체가 검증되어야 안전.

---

## 10. 위험 / 롤백

### 10.1 위험
| # | 위험 | 완화 |
|---|---|---|
| **R1** | Engine.dll 의 `WintersEngine.lib` 가 Server link 시 누락 → LNK2019 | D-1A 에서 `<ProjectReference>` 명시 + EngineSDK/inc 추가 |
| **R2** | 2 client 동시 connect 시 race 로 sessionId 중복 | `Session_Manager.m_nextSessionId` 가 `atomic<u32_t>` (D-1E 박제) |
| **R3** | Snapshot broadcast 가 send queue overflow (1MB 누적) | MVP 는 send queue 무한, 운영 시 한도 + drop oldest 정책 |
| **R4** | InputSystem 마이그 시 local-authoritative 코드와 충돌 | D-2E 에서 기존 입력 → World 변경 코드 비활성화 (commented + TODO) |
| **R5** | Hello packet 후 Client 가 본인 NetEntityId 인지 못함 | D-2D 의 FrameCallback 에서 Hello 우선 처리 + 그 후에 InGame 진입 |

### 10.2 롤백
- 각 Phase 별 commit. Server vcxproj / sketch 코드는 git 으로 복원 가능.
- D-1G (GameRoom) 가 가장 큰 변경 — 별도 branch 권장.
- Client 측 InputSystem 마이그 (D-2E) 가 게임플레이 깨질 위험 — 기존 코드는 `#ifdef WINTERS_LOCAL_AUTHORITATIVE` 로 보존 가능.

---

## 11. 한 줄

**04a = 현 sketch (TCP IOCP) 마무리 → 1 server + 2 client 동기화 demo. 9 Server 작업 (D-1A~D-1I) + 5 Client 작업 (D-2A~D-2E) + 3 검증 (D-3A~D-3C) = 약 52h. AOI/Delta/Prediction/Encryption 전부 보류 — Sim-10 v2 M1-M5 + Sim-11 에서 본격. 합격 = LAN 2-client demo + jitter < 5ms + 결정성 1차 (stable_sort + Precise + allowlist 외 grep 0 hit). 이게 통과해야 UDP 마이그 안전.**
