# Server Fiber 통합 박제 인덱스 (Server-side Fiber Integration Master Index)

> [!IMPORTANT]
> **Historical design index.** 아래 본문을 현재 구현 상태로 사용하지 않는다. 최신 기준은 [2026-07-13 canonical implementation plan](../../../plan/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN.md)과 [S023 결과 보고서](../../../build/2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT.md)다.
> As-built delta: JobSystem Submit race, Chase-Lev deque, FiberFull 및 stress 구현은 완료되었고, UDP v3 generic vertical slice와 server hub/client facade가 구현되었다. main F5 통합과 최종 build 상태는 S023 결과 보고서를 따른다. 6주 Fiber mastery 프로그램은 미착수이며, 현재 상태는 production UDP cutover가 아니다.
> 과거 UDP v2 수치인 **24 B header / 10 B fragment header / 1 MiB logical payload**는 historical design이다. 실제 v3 상수는 **40 B header / 16 B fragment header / 1,200 B datagram / 64 KiB logical payload**다.

> **상태 동기화 (2026-07-11 — DESIGN ONLY)**: Engine의 Main-push owner race 우회와 per-job FiberShell 본체는 이미 존재한다. 그러나 Server Stage 1은 operational하지 않다. `CServerEntry`는 mode 설정·Shutdown·getter·상태 반환이 stub이고 `main`/Tick 호출자가 0이다. 별도 AcceptThread도 없으며 TickThread shell은 mainline 성능 단계가 아니다. Engine 상세는 [JobSystem 상태 감사](../../../plan/2026-07-11_JOB_SYSTEM_CHASE_LEV_FIBER_STATE_AUDIT.md), UDP/Server 적용 순서는 [통합 감사](../../../plan/2026-07-11_FULL_UDP_AND_SERVER_FIBER_INTEGRATION_AUDIT.md)를 따른다.
>
작성일: 2026-05-07
대상: `WintersServer.exe` (Server/) — TCP IOCP MVP + 30Hz Tick GameRoom
핵심 결정: **Engine `CJobSystem` (Phase 5-A Chase-Lev MVP) 의 Fiber-aware 인터페이스 (L37-38, L48, L65-70, L79-80) 를 Server 에서 그대로 활용. Engine 본체 Fiber 구현은 별도 트랙 (`.md/plan/engine/FIBER_JOB_SYSTEM.md`) 에 위임. Server 는 (a) 단일 글로벌 `CJobSystem`, (b) `CGameRoom::TickThread` 를 Fiber 컨텍스트로 진입, (c) Phase 별 행동 보존 병렬화 — 3 단계로 점진 적용.**

---

## 0. 본 박제 묶음의 5 파일

| 파일 | 역할 | h/cpp 전문 |
|---|---|---|
| `00_INDEX_SERVER_FIBER_INTEGRATION.md` | 본 인덱스 — Owner scope, Stage 매트릭스, 8 게이트 통과 보고, 참조 매트릭스 | — |
| `01_SERVER_FIBER_SKELETON.md` | Stage 1 — `CServerEntry` 글로벌 (`CJobSystem` 단일 owner) + `main.cpp` 변경 + `CGameRoom::TickThread` `ConvertThreadToFiber` shell | ✅ 전문 |
| `02_SERVER_TICK_FIBER_AWARE.md` | Stage 2 — `CGameRoom::Tick` 직렬 phase 보존 + `WaitForCounter` 호출 가능한 컨텍스트 확보 (행동 변경 0) | ✅ 전문 |
| `03_SERVER_PARALLEL_PHASES.md` | Stage 3 — `Phase_BroadcastSnapshot` per-session 병렬화 + `Phase_ServerProjectiles` read-only loop 병렬화 (행동 보존) | ✅ 전문 |
| `04_SERVER_VERIFICATION_AND_GATES.md` | 8 게이트 통과 매트릭스 + P-1~P-19 회피 매트릭스 + 검증 스크립트 + 리스크 + Acceptance | — |

---

## 1. 한 줄 결론

```text
Server 는 Engine 의 CJobSystem(Phase 5-A) 을 단일 글로벌 owner 로 보유한다.
Engine Fiber 본체 구현(FIBER_JOB_SYSTEM.md) 이 들어오기 전이라도,
CJobSystem 의 SetExecutionMode(eJobExecutionMode::FiberShell) 호환 인터페이스를
Server 가 호출하는 형태로 박제한다.
이렇게 하면 Engine 측 Fiber 구현이 들어오는 시점에 Server 코드 0 변경으로
ThreadOnly → FiberShell 전환된다 (CJobSystem.h L23-L24 의 약속).
```

---

## 2. 사실 (Facts) — GATE A 통과용 코드 인용

본 인덱스가 의존하는 모든 코드 사실. **추정/필요/TBD 0** 인지 게이트 B 검증.

### 2.1 Engine `CJobSystem` 인터페이스 (Phase 5-A MVP)

**파일**: [`Engine/Public/Core/JobSystem.h`](../../../Engine/Public/Core/JobSystem.h:25)

```cpp
// L25-L93 (전문)
class CJobSystem
{
public:
    CJobSystem();
    ~CJobSystem();

    CJobSystem(const CJobSystem&) = delete;
    CJobSystem& operator=(const CJobSystem&) = delete;

    void Initialize(std::uint32_t iWorkerCount = 0);
    void Shutdown();
    void SetExecutionMode(eJobExecutionMode eMode);            // L37
    eJobExecutionMode GetExecutionMode() const;                // L38

    void Submit(std::function<void()> job);                                // L41
    void Submit(std::function<void()> job, CJobCounter* pCounter);         // L42
    void Submit(const JobDecl& decl, CJobCounter* pCounter = nullptr);     // L45
    void WaitForCounter(CJobCounter* pCounter, std::uint32_t iTarget = 0); // L48

    std::uint32_t GetWorkerCount() const                                   // L50
    {
        return static_cast<std::uint32_t>(m_vecWorkers.size());
    }

    static std::int32_t  Get_WorkerIdx();                                  // L55
    static std::uint32_t Get_WorkerSlot();                                 // L56

private:
    struct WorkItem { std::function<void()> fn; CJobCounter* pCounter = nullptr; };
    struct FiberShellCall                                                  // L65-L70
    {
        CJobSystem* pSystem = nullptr;
        WorkItem* pItem = nullptr;
        NativeFiberHandle hReturnFiber = nullptr;
    };

    void EnqueueJob(WorkItem&& item);
    void  WorkerLoop(std::uint32_t iWorkerIdx);
    bool  TryExecuteOneJob(std::uint32_t iWorkerIdx);
    void  ExecuteItem(WorkItem& item);
    void  ExecuteItemInline(WorkItem& item);
    bool  TryExecuteItemOnFiber(WorkItem& item);                           // L79
    static void WINTERS_FIBER_CALL FiberShellEntry(void* pParam);          // L80
    void  PushToSomeDeque(WorkItem&& item);
    std::uint32_t PickVictim(std::uint32_t iSelf, std::uint32_t N);

    std::vector<std::thread>   m_vecWorkers;
    std::vector<std::unique_ptr<CWorkStealingDeque<WorkItem>>>  m_vecDeques;
    std::atomic<bool> m_bShutdown{ false };
    std::atomic<std::uint32_t> m_iRoundRobin{ 0 };
    std::atomic<eJobExecutionMode> m_eExecutionMode{ eJobExecutionMode::ThreadOnly };

    std::mutex m_GlobalMutex;
    std::queue<WorkItem> m_GlobalQueue;
};
```

**의미 (직접 인용)**:
- L17-L24 코멘트 — `// Phase 5-B 에서 내부가 Fiber 로 교체되어도 public API 불변`. 즉 Server 가 호출하는 API 는 Stage 1~3 에서 안정.
- L48 `WaitForCounter` — `// Counter 가 iTarget 이하가 될 때까지 help-stealing. 블로킹 아님.` Main 도 Steal 한다 (코멘트 L22).
- L55-L56 `Get_WorkerIdx/Get_WorkerSlot` — TLS 기반. Fiber 컨텍스트에서 호출 시 다른 Worker 로 resume 되면 값이 변할 수 있음 (CLAUDE.md §1 Track 3 의 "`Get_WorkerSlot()` 함정").

### 2.2 Engine `CJobCounter` 인터페이스

**파일**: [`Engine/Public/Core/JobCounter.h`](../../../Engine/Public/Core/JobCounter.h:15)

```cpp
// L15-L46 (전문)
class CJobCounter
{
public:
    CJobCounter() = default;
    ~CJobCounter() = default;

    CJobCounter(const CJobCounter&) = delete;
    CJobCounter& operator=(const CJobCounter&) = delete;

    void Increment(std::uint32_t n = 1) { m_iCount.fetch_add(n, std::memory_order_relaxed); }
    void Decrement()                    { m_iCount.fetch_sub(1, std::memory_order_acq_rel); }
    bool IsComplete() const             { return m_iCount.load(std::memory_order_acquire) == 0; }
    std::uint32_t Load() const          { return m_iCount.load(std::memory_order_acquire); }

private:
    std::atomic<std::uint32_t> m_iCount{ 0 };
};
```

**의미**: counter 자체는 Wait 메서드 없음. 대기는 `CJobSystem::WaitForCounter(pCounter, 0)` 로 (코멘트 L9).

### 2.3 Engine Fiber 공개 placeholder 3 파일 (2026-05-07 snapshot)

**파일**: [`Engine/Public/Core/Fiber/FiberTypes.h`](../../../Engine/Public/Core/Fiber/FiberTypes.h:1) (전문 18줄)

```cpp
// L1-L18
#pragma once

#include "WintersTypes.h"

enum class eJobExecutionMode : u8_t
{
    ThreadOnly = 0,
    FiberShell,
};

using NativeFiberHandle = void*;

#if defined(_MSC_VER)
#define WINTERS_FIBER_CALL __stdcall
#else
#define WINTERS_FIBER_CALL
#endif
```

**파일**: [`Engine/Public/Core/Fiber/Fiber.h`](../../../Engine/Public/Core/Fiber/Fiber.h:1) (전문 20줄): get/set native handle 만.

**파일**: [`Engine/Public/Core/Fiber/FiberPool.h`](../../../Engine/Public/Core/Fiber/FiberPool.h:1) (전문 22줄): `Reset()` + `GetCount()` 만.

**2026-07-11 정정**: 공개 `CFiber/CFiberPool`은 여전히 placeholder지만 Engine `JobSystem.cpp`에는 per-job `CreateFiber/SwitchToFiber/DeleteFiber` FiberShell 본체가 존재한다. 미구현인 것은 pool과 `WaitForCounter` yield/resume다. Server는 mode를 set하지 않고 Tick도 변환하지 않으므로 아래는 아직 목표 설계다.

### 2.4 Server `CGameRoom` Tick 모델

**파일**: [`Server/Public/Game/GameRoom.h`](../../../Server/Public/Game/GameRoom.h:63) L63-L165

핵심 멤버:
- `std::thread m_tickThread;` (L133) — 30Hz Tick 전용 OS thread
- `std::mutex m_stateMutex;` (L135) — Tick + Network worker 직렬화
- `CWorld m_world;` (L136)
- `std::unique_ptr<Engine::CSpatialHashSystem> m_pSpatialSystem;` (L144)
- `std::unique_ptr<Engine::CTurretAISystem> m_pTurretAI;` (L145)

**파일**: [`Server/Private/Game/GameRoom.cpp`](../../../Server/Private/Game/GameRoom.cpp:268)

```cpp
// L268-L302 — TickThread 본문 (전문)
void CGameRoom::TickThread()
{
    using clock = std::chrono::steady_clock;
    auto next = clock::now();
    const auto period = std::chrono::microseconds(33333);
    i64_t maxJitterMicros = 0;
    i64_t totalTicks = 0;
    auto lastReport = clock::now();

    while (m_bRunning.load(std::memory_order_relaxed))
    {
        const auto tickStart = clock::now();
        Tick();

        i64_t jitter = std::chrono::duration_cast<std::chrono::microseconds>(
            tickStart - next).count();
        if (jitter < 0)
            jitter = -jitter;
        if (jitter > maxJitterMicros)
            maxJitterMicros = jitter;
        ++totalTicks;

        if (clock::now() - lastReport > std::chrono::seconds(30))
        {
            std::cout << "[Tick] count=" << totalTicks
                << " maxJitter=" << maxJitterMicros << " us\n";
            maxJitterMicros = 0;
            totalTicks = 0;
            lastReport = clock::now();
        }

        next += period;
        std::this_thread::sleep_until(next);
    }
}
```

```cpp
// L304-L325 — Tick 본문 (전문)
void CGameRoom::Tick()
{
    std::lock_guard stateLock(m_stateMutex);

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
    Phase_BroadcastEvents(tc);
    Phase_BroadcastSnapshot(tc);
}
```

```cpp
// L371-L383 — Phase_SimulationSystems 본문 (전문)
void CGameRoom::Phase_SimulationSystems(TickContext& tc)
{
    CStatSystem::Execute(m_world);
    CBuffSystem::Execute(m_world, tc);
    CSkillCooldownSystem::Execute(m_world, tc);
    CMoveSystem::Execute(m_world, tc);
    Phase_ServerMinionWave(tc);
    Phase_ServerMinionAI(tc);
    Phase_ServerTurretAI(tc);
    Phase_ServerProjectiles(tc);
    CDamageQueueSystem::Execute(m_world, tc);
    CDeathSystem::Execute(m_world, tc);
}
```

```cpp
// L514-L523 — Phase_ServerTurretAI (Engine ISystem 인스턴스 호출)
void CGameRoom::Phase_ServerTurretAI(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    if (m_pSpatialSystem)
        m_pSpatialSystem->Execute(m_world, tc.fDt);
    if (m_pTurretAI)
        m_pTurretAI->Execute(m_world, tc.fDt);
}
```

**의미**:
- Tick thread 는 **단일 OS thread**. Phase 들은 **정적 함수 직렬 호출** (CStatSystem::Execute 등).
- `m_stateMutex` 로 Tick 전체 직렬화 — Tick 내부에서 mutex 잡고 yield 시 데드락 위험 (FIBER_JOB_SYSTEM.md §9-4 Fiber-unsafe 패턴 금지).
- `Phase_ServerTurretAI` 만 `ISystem` 인스턴스 (Spatial/TurretAI). 이 두 시스템은 Engine 측 ECS 시스템.
- **CSystemSchedular 는 Server tick path 에 없음** — Server 는 정적 호출 직렬.

### 2.5 Server `CIOCPCore` Worker 모델

**파일**: [`Server/Private/Network/IOCPCore.cpp`](../../../Server/Private/Network/IOCPCore.cpp:157) L157-L247

핵심:
- `WorkerLoop(u32_t workerId)` — `m_workerCount` 개의 OS thread 가 각자 `GetQueuedCompletionStatus(m_hIOCP, ..., INFINITE)` 호출 (L167).
- 완료 시 `CSession_Manager::Get()->OnRecvComplete(...)` 등 호출.
- **IOCP worker = 일반 OS thread**. Fiber 미적용. **이건 의도된 결정 — IOCP 는 OS thread 단위로 GQCS 큐 매핑하므로 Fiber 가 worker 위에서 yield 시 IOCP 큐 entry 미소비 위험.**

### 2.6 Server `main.cpp` 진입점

**파일**: [`Server/Private/main.cpp`](../../../Server/Private/main.cpp:37) L37-L112

핵심:
- `CGameRoom* g_pRoom = nullptr;` (L15) — 글로벌. Sim-04a v2 단일 GameRoom MVP.
- `int main(...)` (L37):
  - WSAStartup (L44-L48)
  - `CGameRoom::Create(1)` + `g_pRoom = room.get()` (L51-L59)
  - `CPacketDispatcher::Instance().RegisterRoom(1, room.get())` (L60)
  - `room->Start()` (L61) — Tick thread 가동
  - `CIOCPCore::Create(9000, 4)` + `core->Start()` (L63-L65) — port 9000, worker 4
  - quit loop or smoke timer (L73-L106)
  - `core->Shutdown()` + `room->Stop()` + `g_pRoom = nullptr` + `WSACleanup()` (L107-L110)

### 2.7 Server.vcxproj 빌드 설정

**파일**: [`Server/Include/Server.vcxproj`](../../../Server/Include/Server.vcxproj:54)

핵심:
- L52: `<FloatingPointModel>Precise</FloatingPointModel>` ✅ (Sim-10 v2 M1 룰)
- L54: AdditionalIncludeDirectories — `$(MSBuildThisFileDirectory)..\..\Engine\Public` 포함 ✅ (Engine 헤더 직접 include 가능)
- L61: `<AdditionalDependencies>ws2_32.lib;Mswsock.lib;%(AdditionalDependencies)</AdditionalDependencies>` ✅ (Sim-10 v2 M1 룰)
- L64: PostBuild copy `WintersEngine.dll` ✅
- L100-L120: Server 측 .cpp ItemGroup. `main.cpp` (L106) 와 `Network/IOCPCore.cpp` (L108) 등록.

**의미**: Server 가 Engine DLL 을 dynamic load. Engine 의 `CJobSystem`/`CJobCounter` 를 직접 use 가능. **신규 박제 .cpp 추가 시 Server.vcxproj 의 ItemGroup 에 등록 필요**.

### 2.8 인접 박제 (05-07 디렉토리)

**파일**: [`.md/TODO/05-07/00_TCP_UDP_MIGRATION_INDEX.md`](../00_TCP_UDP_MIGRATION_INDEX.md) §2.3 — GameRoom 의 fixed tick + PendingCommand drain + stable_sort + SnapshotBuilder 가 **이미 완성된 뼈대**. UDP 이주 핵심은 sim 자체 변경이 아니라 transport 분리.

**파일**: [`.md/TODO/05-07/04_IMPLEMENTATION_SEQUENCE_AND_VERIFICATION.md`](../04_IMPLEMENTATION_SEQUENCE_AND_VERIFICATION.md) §1 — Phase A~K 의 13 단계. 본 박제 (Server Fiber) 는 **Phase A 의 사전 정리** 단계와 호환되어야 함 (TCP 회귀 0 / IOCP worker 변경 0).

**파일**: [`.md/TODO/05-07/07_TCP_UDP_FULL_SERVER_CLIENT_ROADMAP.md`](../07_TCP_UDP_FULL_SERVER_CLIENT_ROADMAP.md) §2 "완성형 서버 tick" L92-L113 — 18 phase 목표 본문. 본 박제는 현재 5 phase (Drain/Execute/Sim/Broadcast/Snapshot) 를 변경 0 유지 + 향후 18 phase 로 확장될 때 Fiber 분해 토대 제공.

### 2.9 CLAUDE.md 인용 (정합성 보장)

**파일**: [`CLAUDE.md`](../../../CLAUDE.md) §1.A Track 3 (★ 신규 — Fiber JobSystem Phase 5-B):

```text
- 호출부 유지 — Submit / WaitForCounter / CJobCounter API 불변, Fiber 는
  CJobSystem 내부 구현 디테일
- 1차 목표 = Fiber shell only — WaitForCounter yield 없이, 단순
  ConvertThreadToFiber + 1 job 실행 + 복귀
- Get_WorkerSlot() 함정 — Fiber 가 다른 worker 에서 resume 가능 →
  thread-local worker index 만으로 위험. MinionAISystem.cpp:297 의 per-slot
  buffer 패턴 영향. Fiber 적용 시 "현재 실행 컨텍스트의 stable scratch slot"
  반환으로 변경
```

**역사 기록**: 2026-05-07 당시 `CLAUDE.md` §1.C 미결:

```text
- (★ Fiber 진입 전제) Phase 5-A Chase-Lev Main-push race —
  Submit 이 Main thread 호출 시 Worker Deque Push 위반.
  Scene_InGame.cpp:397 의 pNav->Set_JobSystem(pJS) 주석 복구 필요.
  옵션 A: Main → Global Queue, Worker → 자기 Deque.
```

**2026-07-11 정정**: 이 선결은 충족됐다. Tick/main/external submit은 현재 Global Queue로 라우팅된다. Server Stage가 미착수인 이유는 owner-race가 아니라 `CServerEntry`/main/Tick wiring과 검증이 구현되지 않았기 때문이다.

---

## 3. Owner Scope 결정 매트릭스 (P-10 회피)

본 박제가 신설하는 모든 객체의 owner scope. PITFALLS §P-10 매트릭스와 일치.

| 객체 | Owner | 이유 |
|---|---|---|
| `CJobSystem` 인스턴스 | **`CServerEntry` 글로벌** (1 process 1) | RHI Device 와 동일 매트릭스 — 프로세스당 1. 멀티 GameRoom (Sim-10 v2) 시 모든 GameRoom 이 공유. |
| `CGameRoom::m_tickThread` | `CGameRoom` 멤버 | Tick thread 는 GameRoom 별 1개. Sim-10 v2 진입 시 GameRoom N 개 → Tick thread N 개. 모두 같은 `CJobSystem` 의 Submit 호출. |
| `CJobCounter` (Phase 별) | **함수 로컬** (Tick 안 stack) | Counter 는 Submit~WaitForCounter 사이만 살아있음. 멤버화 X. |
| `Tick thread → Fiber 변환 결과` | Tick thread 자체 (`thread_local`) | `ConvertThreadToFiber` 결과는 호출 thread 에 종속. Fiber handle 자체는 thread 종료 시 정리. |
| Per-session snapshot 작업 (Stage 3) | 함수 로컬 vector<JobDecl> | Submit 직전 stack 에 모은 후 Submit 호출. |

**금지**:
- `CJobSystem* CGameInstance::Get_JobSystem()` 같은 게터를 Server 에 두면 GameInstance 는 Client 전용. Server 는 별도 `CServerEntry` 또는 main.cpp 의 namespace 글로벌.
- `CGameRoom::m_pJobSystem` 멤버 — 멀티 GameRoom 시 N 개 instance 가 같은 JobSystem 공유해야 하므로 GameRoom 멤버는 raw 포인터 캐시만 OK (소유 X).

---

## 4. Stage 매트릭스 (Phase A ~ Phase C)

각 Stage 는 **미래 독립 PR 계획**이다. 2026-07-11 현재 Stage 1도 완료되지 않았으며, `CServerEntry` 파일/프로젝트 등록만 있고 동작 본체와 호출 wiring이 없다.

| Stage | 목표 | 변경 파일 | 검증 | 행동 변화 |
|---|---|---|---|---|
| **Stage 1** (`01_SERVER_FIBER_SKELETON.md`) | `CServerEntry` 글로벌 + `main.cpp` Initialize/Shutdown + `CJobSystem::SetExecutionMode(FiberShell)` 호출 + `CGameRoom::TickThread` 시작 시 `ConvertThreadToFiber` shell. **Phase 분해 X. Submit 호출 0**. | `Server/Public/Game/ServerEntry.h` (신규) `Server/Private/Game/ServerEntry.cpp` (신규) `Server/Private/main.cpp` (수정) `Server/Private/Game/GameRoom.cpp` (TickThread 만 수정) `Server/Include/Server.vcxproj` (신규 .cpp 등록) | Server 30s smoke + Tick jitter 변화 0 + IOCP 회귀 0 + Crash 0 | **0** (행동 보존) |
| **Stage 2** (`02_SERVER_TICK_FIBER_AWARE.md`) | Tick 안에서 `CServerEntry::Get_JobSystem()->WaitForCounter(...)` 호출 가능한 컨텍스트 확보. **Phase 분해 시도 시 즉시 yield 안 하고 inline 실행 (FiberShell 1 job 실행)**. 2-pass 패턴 (Decision-Apply) 구조 표준화. `Phase_BroadcastEvents` 의 stable_sort 보존. | `Server/Public/Game/GameRoom.h` (helper 메서드 추가) `Server/Private/Game/GameRoom.cpp` (Phase_BroadcastEvents 구조 정리, 분해 X) `Server/Public/Game/SnapshotBuilder.h` (per-session API 표면) `Server/Private/Game/SnapshotBuilder.cpp` (helper 분리) | Stage 1 + per-session SnapshotBuilder 호출 결과 byte-identical 비교 (직렬 vs Fiber shell 모드) | **0** (행동 보존) |
| **Stage 3** (`03_SERVER_PARALLEL_PHASES.md`) | `Phase_BroadcastSnapshot` per-session SnapshotBuilder 호출 N 개 동시 Submit + WaitForCounter. `Phase_ServerProjectiles` 의 read-only loop (collision query) 분할. **Read-only 만 병렬화**. | `Server/Private/Game/GameRoom.cpp` (Phase_BroadcastSnapshot/Phase_ServerProjectiles 본체 변경) `Server/Public/Game/SnapshotBuilder.h` (PerSessionInput POD) | Stage 2 + 8 session 동시 snapshot 결과 byte-identical 비교 + Tick max time 측정 (8 session × 50KB snapshot 직렬 vs 병렬) | **byte-identical 보장** (P-14 회피) |

**선결 조건 상태 (2026-07-11)**:
- ✅ Main-push owner-race 경로: worker self deque / external Global Queue로 수정됨.
- ❌ Stage 1 operational skeleton: `CServerEntry` 구현, `main` lifetime, Tick fiber shell, smoke 검증이 남음.

---

## 5. 8 게이트 통과 보고 (PLAN_AUTHORING_PITFALLS §2)

| 게이트 | 통과 여부 | 본 박제 위치 | 회피 함정 |
|---|---|---|---|
| **A — 사실 수집** | ✅ | §2 (Engine API + Server tick/IOCP/main + Server.vcxproj + 인접 plan + CLAUDE.md). 모든 인용에 줄 번호 + 직접 인용 블록. | P-1, P-2, P-5 |
| **B — TODO 0** | ✅ | §2.1~2.9 모든 행이 확정값. "필요"/"추정"/"TBD" 0. (단 Engine Fiber 본체 구현 여부는 Stage 2 의 _전제_ 로 명시 — 박제 자체에는 TBD 없음) | P-6 |
| **C — 호출 경로 grep** | ✅ | `Submit/WaitForCounter` 의 호출자 = (a) `Phase_BroadcastSnapshot`, (b) `Phase_ServerProjectiles`. 이외 호출 경로 0 (다른 Phase 는 정적 함수 직렬). 본 박제는 두 경로만 변경 — 04_VERIFICATION 의 §2 grep 표 참조. | P-3, P-13 |
| **D — ECS 책임 경계** | ✅ | `CJobSystem` 은 Engine 의 ISystem 과 **독립**. Server 는 직접 Submit 만. ECS 컴포넌트/이벤트 신설 X. 멀티 GameRoom (Sim-10 v2) 진입 시 Server 의 모든 GameRoom 이 같은 `CJobSystem` 공유 — 깨짐 0. | P-4 |
| **E — 향후 사례 자료형** | ✅ | Counter 는 `std::atomic<uint32_t>` (`CJobCounter::m_iCount`). 32-bit 한도 = 4,294,967,296. Tick 당 동시 Job 수 = max 8 session × 1 snapshot = 8 → 4G 한도와 비교 시 충분. | P-7 |
| **F — Scheduler / 동시성 모델** | ✅ | 본 박제는 **새 phase 추가 없음**. 기존 phase 5개 직렬 유지. Stage 3 의 read-only 병렬화는 **같은 Phase 안에서만** (Phase_BroadcastSnapshot 안 N session). Producer→Consumer 의존 없음 (per-session 독립). | P-9 |
| **G — Owner Scope 매트릭스** | ✅ | §3 매트릭스 — `CJobSystem` 1 process 1, `CJobCounter` 함수 로컬, Tick thread per-GameRoom. PITFALLS §P-10 와 일치 (RHI Device 매트릭스). | P-10, P-11 |
| **H — 인용 의미 일치 + 행동 보존** | ✅ | §2.9 인용 (CLAUDE.md §1.A Track 3) 와 본 박제 결정 (FiberShell 모드 + WaitForCounter 호출부 유지) 같은 방향. Stage 3 의 병렬화 = **read-only 만**, mask 확장 0. | P-8, P-14, P-15 |

---

## 6. 19 함정 회피 매트릭스 (PLAN_AUTHORING_PITFALLS §1)

| 함정 | 본 박제 회피 |
|---|---|
| P-1 데이터 미검증 | §2 의 모든 인용에 실제 코드 줄 번호 + 직접 인용. Engine Fiber stub 3 파일 전문 박제. |
| P-2 PIMPL 추측 | `CJobSystem` private 멤버 (L65-93) 박제 — 실제 `JobSystem.h` 라인 인용. PIMPL X (`CJobSystem` 은 직접 멤버). |
| P-3 Render 호출 경로 단일 가정 | Server 에 Render 경로 없음. 단 `Submit` 호출 경로는 Stage 3 에서 Phase_BroadcastSnapshot + Phase_ServerProjectiles 2 경로 모두 박제. |
| P-4 ECS / Scene 결합 | Server 는 Scene 없음. `extern` 사용 0. `CGameRoom*` 글로벌 1개 (`g_pRoom`) 는 Sim-04a v2 단계 기존 결정 — 본 박제가 추가 글로벌 만들지 않음. `CServerEntry` 는 Engine 의 RHI Device 매트릭스 (프로세스당 1) 와 동일. |
| P-5 유령 의존 | §0.5 인용 모든 .md / .h / .cpp 가 실재. §2 grep 으로 검증. |
| P-6 TODO 박제 진입 | §1 의 모든 사실이 확정. "Engine Fiber 본체 구현 후 진입" 같은 _전제_ 만 명시 — 본 박제 자체에는 TBD 없음. |
| P-7 자료형 미래 사례 | Counter 32-bit, Server 동시 Job 8개 — 충분. |
| P-8 인용 의미 반전 | CLAUDE.md §1.A Track 3 인용 직접 블록. 본 박제 결정 (FiberShell 모드 + 호출부 유지) 정확히 일치. |
| P-9 Scheduler 동시성 가정 | 본 박제는 새 phase 0. fractional phase 0. 같은 phase 병렬화 시 read+write 0 검증 (Stage 3 의 per-session snapshot 은 m_world read-only). |
| P-10 Owner Scope | §3 매트릭스. `CServerEntry` 프로세스당 1. |
| P-11 도메인 상수 Engine | Engine 측 신규 상수 0. Server 측 상수도 (worker count 4 — main.cpp L63 기존값 유지). |
| P-12 음수 좌표 truncation | 본 박제는 좌표 계산 X. Spatial cell 계산은 ECS Spatial 시스템 책임 (Engine). |
| P-13 미존재 API | §2.1 의 `CJobSystem` 모든 호출 API 가 실재. `SetExecutionMode/Submit/WaitForCounter` — 헤더 인용. |
| P-14 행동 변경 | Stage 1 + Stage 2 행동 변화 0. Stage 3 byte-identical 검증. mask 확장 0. |
| P-15 헤더 외부 의존 미include | 신규 헤더 (`ServerEntry.h`) 가 사용하는 모든 외부 타입 (`CJobSystem`, `CJobCounter`, `eJobExecutionMode`) 의 정의 헤더를 직접 include. (`Core/JobSystem.h`, `Core/JobCounter.h`, `Core/Fiber/FiberTypes.h`) |
| P-16 산술 검증 | §5 게이트 E 산술 박제 (8 session × 1 snapshot = 8 ≪ 4G). FIBER_POOL_SIZE = 128 (FIBER_JOB_SYSTEM.md L233) 가정 시 30Hz × 5 phase × 8 session = 1200/s ≪ 128 동시 보유 가능. |
| P-17 Typedef 일괄 변경 | 본 박제는 typedef 변경 0. 신규 alias 0. |
| P-18 RHI/Engine 인프라 미인지 | `CJobSystem`/`CJobCounter` 가 이미 Phase 5-A MVP 로 박혀있음 — 재사용. 별도 신설 0. |
| P-19 Render/Sim 결합 | Server 에 Render 없음. |

---

## 7. 리스크 매트릭스 + 대응

| 리스크 | 영향 | 대응 |
|---|---|---|
| Engine 측 Fiber 본체 구현이 들어오기 전 Server 가 `eJobExecutionMode::FiberShell` 모드 set | Engine 의 `TryExecuteItemOnFiber()` ([JobSystem.h:79](Engine/Public/Core/JobSystem.h:79)) 가 stub 일 가능성 → set 무시되거나 ThreadOnly 와 동일 동작 | Stage 1 은 set 만 하고 행동 변화 검증 0. Stage 2 는 Engine race fix + Fiber 본체 또는 동등 stub 반영 후 진입. |
| Tick thread 안에서 mutex 잡고 `WaitForCounter` 호출 | Fiber yield 시 mutex 다른 thread 에서 못 풀어 데드락 | `m_stateMutex` 는 Tick 시작에서 lock_guard 로 잡힘 (GameRoom.cpp:306). 본 박제는 **Phase 안에서 추가 mutex 잡지 않음**. WaitForCounter 호출 위치는 lock_guard 안이지만 **Counter 가 0 보장 후 release 되므로 데드락 없음** (다른 thread 가 stateMutex 안 잡음). 단 04_VERIFICATION §3 의 deadlock 검증 항목으로 명시. |
| IOCP worker 가 Fiber 컨텍스트 모르고 일반 OS thread 로 GQCS | 의도된 결정. `CIOCPCore::WorkerLoop` 변경 0. | 본 박제는 **IOCP worker 변경 0 명시** — 04_VERIFICATION §1.4 . |
| Sim-10 v2 진입 시 멀티 GameRoom 진입 | 모든 GameRoom 이 같은 `CJobSystem` 공유. Counter 는 GameRoom 별 Tick 함수 로컬 — 자연스럽게 격리. | §3 매트릭스 + Stage 3 의 per-session 병렬화가 GameRoom 단위로 자연 확장. |
| `Get_WorkerSlot()` 함정 (CLAUDE.md §1.A Track 3) | Fiber 가 다른 worker 로 resume 시 worker idx 변동 | Server 의 5 phase 정적 함수는 `Get_WorkerSlot()` 호출 0 (grep 검증 — 04_VERIFICATION §2.5). MinionAI 가 호출하는 경우는 Engine 측 책임. Server 박제 자체는 함정 회피. |

---

## 8. 검증 체크리스트 요약 (04_VERIFICATION 의 §10 본문)

박제 적용 시 PR 마다:

```text
[ ] devenv.exe 종료 후 Engine project 단독 빌드 성공 (PostBuild EngineSDK/inc 동기화)
[ ] Server.vcxproj ItemGroup 에 신규 .cpp 등록됨
[ ] Server 빌드 (Debug/Release) 성공 — /utf-8 + /fp:precise + Mswsock.lib 유지
[ ] WintersServer.exe --smoke-seconds=30 동작 (Tick jitter 로그 정상)
[ ] CIOCPCore::WorkerLoop 의 GQCS 회귀 0
[ ] 30 sec smoke 동안 Tick maxJitter < 1000us 유지
[ ] Stage 1: ConvertThreadToFiber 성공 로그 1회 + ConvertFiberToThread 성공 로그 1회
[ ] Stage 2: helper 메서드 추가 후 행동 byte-identical
[ ] Stage 3: 8 session smoke (모킹) 에서 snapshot byte-identical (직렬 vs 병렬)
[ ] CSession_Manager + CGameRoom + g_pRoom 의 글로벌 컨벤션 변경 0
[ ] grep -rn "Submit\|WaitForCounter" Server/ 결과가 본 박제 표와 일치
```

상세: [`04_SERVER_VERIFICATION_AND_GATES.md`](04_SERVER_VERIFICATION_AND_GATES.md)

---

## 9. 진입 순서 권장

```text
1. ✅ Engine 측 Phase 5-A Main-push owner-race 경로 확인 완료
2. 01_SERVER_FIBER_SKELETON.md를 현재 코드에 맞게 재감사한 뒤 적용
   - 빌드 → 30s smoke → 합격 기준 통과
3. (선택) Engine FiberPool/FiberFull 구현 진입
   - per-job FiberShell은 이미 있으나 기본 OFF/미검증. Stage 2의 비차단 wait 효과는 FiberFull 이후에만 생김
4. 02_SERVER_TICK_FIBER_AWARE.md 박제 적용
   - 빌드 → byte-identical 검증
5. 03_SERVER_PARALLEL_PHASES.md 박제 적용
   - 빌드 → 8 session 모킹 + byte-identical + Tick max time 측정
6. 04_SERVER_VERIFICATION_AND_GATES.md 의 acceptance matrix 통과 보고
```

---

## 10. 다음 단계 (out of scope)

본 박제는 _Server Fiber 진입_ 만 다룬다. 다음은 별도 트랙:

- **Engine 측 Fiber 본체 구현** — `.md/plan/engine/FIBER_JOB_SYSTEM.md` v2 (codex 추가 검토 반영) 가 들어오면 Server Stage 2~3 의 효과가 본격화. 본 박제와 독립.
- **AnimUpdate 병렬화** (CLAUDE.md §1.C 미결) — Phase 5-B Fiber 진입 후. 본 박제와 독립.
- **Phase_SimulationSystems 직렬→병렬 분해** — Sim 결정성 깨질 위험. 04 sub-plan 에 향후 박제 후보로만 명시.
- **Multi-GameRoom (Sim-10 v2)** — 본 박제는 단일 GameRoom 가정. 멀티 진입 시 본 박제의 Owner scope 매트릭스가 자연스럽게 확장됨.

---

**END OF INDEX**
