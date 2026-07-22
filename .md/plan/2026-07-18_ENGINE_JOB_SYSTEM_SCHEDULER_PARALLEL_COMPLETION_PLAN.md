# 2026-07-18 Engine JobSystem·Scheduler 병렬 완성 계획서

```text
Session - 이미 검증된 Job/Fiber 코어는 보존하고, Scheduler 안전 계약·실패 가시성·실제 workload worker 적용·이력서 실측을 닫는다.
좌표: 신규 좌표 후보 · 축: C3 공유는 비싸다, C8 검증이 병목
관련: 2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_IMPLEMENTATION_PLAN, 2026-07-13_UDP_JOB_SYSTEM_CHASE_LEV_FIBER_RESULT, 2026-07-17_SCHEDULER_PARALLEL_BATCH_IGNITION_REPORT, 05_ECS_JOB_FIBER_ATOMIC_ESSENCE_PLAN
```

## 0. 감사 판정

### 0-1. 결론

사용자의 기억은 **절반만 맞다**. `CJobSystem` 코어는 더 이상 미완성 thread-pool MVP가 아니다. 현재 코드와 2026-07-18 재실행 stress에서 다음이 실제 구현·검증됐다.

| 영역 | 현재 판정 | 코드/실측 증거 |
|---|---|---|
| Job publication/lifetime | 구현·stress 완료 | immutable `WorkItem*`, counter-before-publish/rollback, submit admission과 shutdown 경계: `Engine/Private/Core/JobSystem.cpp` |
| Work stealing | 구현·직접 경합 완료 | worker-owner bottom + thief top, 4,096 slot, last-item 100,000회 위반 0: `WorkStealingDeque.h`, `RunJobSystemStress.ps1 -Mode all` |
| FiberFull | 구현·stress 완료 | worker당 64 fiber, wait/resume origin pinning, FLS, nested `16/16`, saturation `64/64`, pool miss fallback 17 |
| Engine lifecycle | Client/Server 연결 완료 | Client `CGameInstance`는 `Initialize(0)` ThreadOnly, Server는 CLI로 ThreadOnly/FiberShell/FiberFull 선택 |
| ECS SystemScheduler | 부분 완료 | component access conflict batch와 phase barrier는 있음. 리플레이에서 `MaxBatchSize=2`, `ParallelBatches=1`, `SubmittedJobs=2/frame` |
| 실제 workload 병렬화 | 제한적 | Client Transform/Nav/LocalAI 내부 fan-out + LocalStatus/Vision co-batch. Server authoritative GameRoom tick은 직렬 single-writer |
| 병렬 안전 계약 | 미완료 | 비-ECS resource access, main-thread affinity, worker structural mutation 금지의 기계적 표현이 없음 |
| 실패 전달 | 미완료 | worker 예외는 `uFailed`만 올리고 Scheduler가 성공처럼 다음 batch를 진행함 |
| 성능 수치 | 미완료 | correctness 수치는 있음. Scheduler FPS 개선은 실행간 변동·병행 편집 오염으로 주장 금지 |

따라서 “worker 자체가 없다/JobSystem이 미완성”은 현재 사실이 아니다. 정확한 표현은 **코어 runtime은 상당히 완성됐지만, 안전한 Scheduler 계약과 실제 제품 workload 채택·성능 입증이 아직 완성되지 않았다**다.

### 0-2. 현재 숫자

- 2026-07-18 Debug correctness 재실행: fan-out `100,000`, Chase–Lev last-item race `100,000`회 위반 `0`, FiberFull nested wait/resume `16/16`, saturation wait/resume `64/64`, 전체 PASS.
- 2026-07-17 Release+WINTERS_PROFILING 리플레이: Scheduler median `0.0675 ms`, p95 `0.1006 ms`; 전체 frame median `0.808 ms`, p95 `1.271 ms`인 대표 캡처가 있으나 런간 ±18%와 baseline/after 오염 때문에 개선율은 미확정.
- 같은 리플레이 3런의 구조 카운터는 전 프레임 `MaxBatchSize 1→2`, `ParallelBatches 0→1`, `SubmittedJobs 0→2`로 변했다. 64 raw-event frame에서 LocalStatus/Vision의 비-main thread 실행이 각각 `10~21`/`16~24`회 관측됐다.
- Server 10-bot Release soak의 기존 3런 tick p95는 `2.403~3.035 ms`, p99는 `2.776~3.555 ms`로 30 Hz `33.333 ms` 예산 안이다. 다만 당시 final world hash가 런별로 달라, 병렬화 전 결정성 원인을 먼저 닫아야 한다.

### 0-3. 이번 계획에서 말하는 “완성”

범위는 Winters의 frame-oriented fork/join runtime이다. 다음은 완료 조건이다.

1. Job publish/steal/wait/shutdown correctness stress가 ThreadOnly/FiberShell/FiberFull에서 계속 PASS한다.
2. Scheduler가 component뿐 아니라 Engine resource와 main-thread affinity를 표현하고, 미선언 system은 자동으로 main-thread 직렬화한다.
3. worker job 실패가 batch 실패로 관측되어 부분 갱신 뒤 다음 batch를 조용히 실행하지 않는다.
4. worker에서 world structure를 직접 바꾸는 system은 0개다. 구조 변경이 필요한 system은 main-thread barrier에 남긴다.
5. normal F5/replay에서 worker 실행이 rawEvents/threadId와 counter로 증명된다. 배치 폭을 안전 근거 없이 3 이상으로 만드는 것은 목표가 아니다.
6. 실제 비용이 있는 immutable workload 한 곳을 jobify하고, serial/ThreadOnly/FiberFull 결과 byte/hash parity와 Release 3×3 실측을 통과한다.
7. 이력서에는 재현 명령·하드웨어·빌드·시나리오와 함께 correctness 수치와 성능 수치를 분리해서 기록한다.

비목표: arbitrary blocking I/O의 fiber화, cross-worker fiber migration, priority/cancellation general scheduler, lock-free global injection queue, Shared/GameSim의 Engine JobSystem 의존. 이 항목들은 현재 completion gate에 필요하지 않다.

### 0-4. 소유권 경계

```text
Engine
  CJobSystem / CJobCounter / Chase-Lev / Fiber / resource-aware SystemScheduler
  -> generic runtime primitive와 Client ECS scheduling만 소유

Shared/GameSim
  deterministic gameplay work/data dependency
  -> Engine/Fiber/Win32 타입 비의존, gameplay truth는 계속 직렬 권위 경로

Server
  CServerEntry가 Engine JobSystem을 소비
  -> mutable GameRoom을 job에 넘기지 않고 tick 경계 immutable DTO encode만 병렬화

Client
  Engine Scheduler를 소비
  -> D3D/ImGui/resource creation/structural presentation은 main-thread affinity
```

### 0-5. Server 재감사 — “풀은 연결, 실제 GameRoom job은 0”

2026-07-18 사용자 요청으로 Server 경계를 다시 감사했다. 결론은 다음과 같다.

| 항목 | 현재 상태 | 근거 |
|---|---|---|
| process-global worker pool | 연결 완료 | `CServerEntry::s_JobSystem`; `Stopped→Initializing→Ready→Stopping` lifecycle |
| 실행 mode/worker CLI | 연결 완료 | `--job-mode=thread|fiber-shell|fiber-full`, `--job-workers=1..256`; 기본 `ThreadOnly`, `max(1, hw-6)` |
| Server binary worker 실행 | startup probe만 완료 | parent/child 2 jobs, counter wait, FiberFull wait/resume를 `main.cpp` 시작 때 검증 |
| authoritative GameRoom tick | 직렬 | `Tick()`이 `m_stateMutex` 아래 command→simulation→event→snapshot→keyframe 순차 호출 |
| GameRoom production Submit/Wait | **0곳** | `Server/**` 검색에서 startup probe 이외 `Submit`/`WaitForCounter` 없음 |
| Engine ECS `CSystemSchedular` 소비 | **0곳** | Server는 `ISystem` phase plan을 만들거나 `CSystemSchedular`를 보유하지 않음 |
| snapshot encoding | 수신자별 직렬 반복 | `Phase_BroadcastSnapshot`이 session loop마다 `CSnapshotBuilder::Build` 후 즉시 `SendFrame` |
| multi-room shared pool | lifecycle만 준비 | pool은 process-global이지만 현재 `main`은 GameRoom 1개 생성; room workload 제출은 없음 |

따라서 사용자가 기억한 원래 방향은 맞지만, 정확한 역사적 계획은 **Client의
`CSystemSchedular` 객체를 Server에 그대로 꽂는 것**이 아니었다. 2026-05-07 Server
Stage 3 문서 [03_SERVER_PARALLEL_PHASES.md](../TODO/05-07/Server/03_SERVER_PARALLEL_PHASES.md)는
explicit tick phase 안에서 `Phase_BroadcastSnapshot`의 recipient encode를 Engine
`CJobSystem`에 직접 fan-out하고, send는 직렬로 유지하는 설계였다. S023은 그중
pool lifecycle과 startup probe까지만 완료했고 workload fan-out은 의도적으로 남겼다.

현재 시점의 정정된 방향은 다음과 같다.

```text
IOCP completion threads
  -> owned command/packet queue만 생산

room logical tick (single writer, deterministic order)
  -> [m_stateMutex] command / authoritative GameSim / event collection 직렬
  -> [m_stateMutex] immutable ReplicationFrame + RecipientView capture 1회
  -> unlock m_stateMutex
  -> Server recipient-batch scheduler
       -> process-global Engine CJobSystem에 encode jobs 제출
       -> indexed output + counter join + failure propagation
  -> sessionId 정렬 순서로 transport enqueue/send
```

Server가 쓰는 “scheduler”는 초기에는 **Server 전용 explicit phase + recipient batch
scheduler**다. Client ECS `CSystemSchedular`는 `ISystem/CWorld/component access`에 맞춰진
도구이고, Server tick은 command queue, RNG, entity map, replay, session binding 등
비-ECS 자원이 더 많다. 이를 억지 wrapper로 감싸면 모든 phase가 `CGameRoom` write로
직렬화되거나 거짓 access 선언이 생긴다. 두 번째 실제 consumer가 생기기 전에는
범용 `CJobGraph`를 Engine에 새로 추출하지 않는다.

### 0-6. 이번 재개 세션의 충돌 판정

현재 다른 세션의 dirty 변경이 다음 target에 존재한다.

```text
Server/Private/Game/SnapshotBuilder.cpp
Server/Public/Game/GameRoom.h
Server/Private/Game/GameRoom.cpp
Server/Private/Game/GameRoomChampionAI.cpp
Server/Private/Game/GameRoomCommands.cpp
Server/Private/Game/GameRoomUnitAI.cpp
```

그중 `SnapshotBuilder.cpp`와 `GameRoom.h`는 immutable replication 분리의 핵심 target이라
현 상태에서 source를 수정하면 안 된다. 이번 사용자 지시에 따라 이 세션은 계획만
갱신하며 source, project XML, work-packet, build output을 변경하지 않는다.
현재 dirty 변경의 owner packet ID/branch는 코드만으로 확인되지 않았으므로 **Stage C
상태는 Blocked**다. “파일이 잠시 안 바뀐다”를 handoff로 간주하지 않는다.

향후 구현 착수 조건:

1. `.md/collab/ACTIVE_WORK_PACKETS.md`에 Server scheduler packet을 `Reserved`로 등록한다.
2. 위 dirty 파일 owner가 `Handoff/Merged`인지 확인하고, 그렇지 않으면 해당 파일은 read-only로 둔다.
3. owned/read-only path, owner device·agent·branch, validation, report를 갖춘 전용 packet과
   ACTIVE row를 만든 뒤 owned-path 시작 SHA-256을 manifest로 저장한다.
4. `msbuild`, `cl`, `link` process가 0일 때만 build lane을 점유한다.
5. 편집 직전, build 직전, handoff 직전에 manifest를 재검사하고 owned/read-only hash가
   예상 밖으로 바뀌면 즉시 중단한다.
6. `/m:1 /nr:false`로 Debug와 Release를 순차 실행하고, 다른 세션의 build와 겹치지 않는다.

## 1. 결정 기록

```text
① 문제·제약: Job/Fiber correctness는 100,000-job/race stress를 통과했지만 normal replay의 scheduler 병렬 폭은 2, 제출은 2 jobs/frame, Server GameRoom workload jobification은 0이다. Scheduler 자체는 median 0.0675ms라 무작정 job을 늘리면 오버헤드가 이득보다 커진다.
② 순진한 해법의 실패: phase만 합치거나 모든 system을 worker로 보내면 `UnknownWritesAll`, `GetOrCreateStore` lazy insert, SpatialIndex 같은 비-ECS 자원, D3D/ImGui/main-thread 수명, 구조 변경 순서에서 race/비결정성이 생긴다. Client를 즉시 FiberFull 기본으로 바꿔도 의존성 wait가 적어 switch 비용만 늘 수 있다.
③ 메커니즘: 기존 Job/Fiber 코어는 보존하고 failure-aware counter + resource/affinity access contract + fail-closed Scheduler를 먼저 넣는다. 실제 병렬 수치는 mutable world가 아니라 tick 경계의 immutable capture→per-recipient encode→ordered send 수직 슬라이스에서 만든다.
④ 대조: 범용 task graph처럼 dependency/resource/affinity를 선언하되 Winters는 12개 내외 system과 명시 phase가 있으므로 전면 DAG/priority scheduler를 새로 만들지 않는다. authoritative sim single-writer를 보존하고 read-only/encode work만 fork한다.
⑤ 대가: access metadata와 barrier가 늘고 immutable DTO 복사 비용이 생긴다. baseline scope가 0.5ms 미만이거나 3×3 p95 중앙값 개선이 20% 미만이면 해당 jobification은 철회하고 serial 경로를 유지한다. 안전 때문에 구조 변경 system은 당분간 병렬 폭에 기여하지 않는다.
```

## 2. 반영해야 하는 코드

### Stage A — Scheduler 안전 계약과 실패 가시성 (먼저 적용)

#### 2-1. C:/Users/user/Desktop/Winters/Engine/Public/Core/JobCounter.h

기존 `Load()` 함수 아래에 추가:

```cpp
    void RecordFailure()
    {
        m_iFailureCount.fetch_add(1, std::memory_order_relaxed);
    }

    std::uint32_t GetFailureCount() const
    {
        return m_iFailureCount.load(std::memory_order_acquire);
    }

    bool HasFailure() const
    {
        return GetFailureCount() != 0u;
    }
```

기존 private 멤버:

```cpp
    std::atomic<std::uint32_t> m_iCount{ 0 };
```

아래로 교체:

```cpp
    std::atomic<std::uint32_t> m_iCount{ 0 };
    std::atomic<std::uint32_t> m_iFailureCount{ 0 };
```

의도: exception을 삼켜 worker loop를 살리는 기존 정책은 유지하되, fork/join owner가 batch 실패를 판정할 수 있게 한다.

#### 2-2. C:/Users/user/Desktop/Winters/Engine/Public/Core/JobSystem.h

`CJobSystemStats`의 `uFiberPoolMisses` 아래에 추가:

```cpp
    std::uint64_t uWorkerExecutions = 0;
    std::uint64_t uExternalHelpExecutions = 0;
```

class private atomic 통계의 `m_uFiberPoolMisses` 아래에 추가:

```cpp
    std::atomic<std::uint64_t> m_uWorkerExecutions{ 0 };
    std::atomic<std::uint64_t> m_uExternalHelpExecutions{ 0 };
```

기존 `uInlineExecutions`는 ABI/기존 보고서 호환을 위해 삭제·개명하지 않는다. 새 두 값이 “worker에서 실행됨”과 “main/external waiter가 help-execute함”을 분리한다.

#### 2-3. C:/Users/user/Desktop/Winters/Engine/Private/Core/JobSystem.cpp

`CJobSystem::GetStats()`의 기존 마지막 부분:

```cpp
    stats.uFiberPoolMisses = m_uFiberPoolMisses.load(std::memory_order_relaxed);
    return stats;
```

아래로 교체:

```cpp
    stats.uFiberPoolMisses = m_uFiberPoolMisses.load(std::memory_order_relaxed);
    stats.uWorkerExecutions = m_uWorkerExecutions.load(std::memory_order_relaxed);
    stats.uExternalHelpExecutions =
        m_uExternalHelpExecutions.load(std::memory_order_relaxed);
    return stats;
```

`CJobSystem::ExecuteItemInline` 함수에서 `if (!pItem) return;` 바로 아래에 추가:

```cpp
    const bool bWorkerExecution =
        t_pOwningJobSystem == this && t_iWorkerIdx >= 0;
    if (bWorkerExecution)
    {
        m_uWorkerExecutions.fetch_add(1, std::memory_order_relaxed);
    }
    else
    {
        m_uExternalHelpExecutions.fetch_add(1, std::memory_order_relaxed);
    }

    CJobCounter* const pCounter = pItem->pCounter;
```

같은 함수의 기존 코드:

```cpp
    CJobCounter* const pCounter = pItem->pCounter;
    pItem.reset();

    if (bFailed)
        m_uFailed.fetch_add(1, std::memory_order_relaxed);
```

아래로 교체:

```cpp
    if (bFailed)
    {
        m_uFailed.fetch_add(1, std::memory_order_relaxed);
        if (pCounter)
            pCounter->RecordFailure();
    }

    pItem.reset();
```

중복 선언이 생기지 않도록 기존 `CJobCounter* const pCounter = ...` 줄은 위 추가 블록으로 이동한다.

#### 2-4. C:/Users/user/Desktop/Winters/Engine/Public/ECS/SystemAccess.h

파일 전체를 아래로 교체:

```cpp
#pragma once

#include <typeindex>
#include <vector>

#include "WintersTypes.h"

enum class eSystemAccessMode : u8_t
{
    Read,
    Write,
};

enum class eSystemThreadAffinity : u8_t
{
    AnyWorker,
    MainThread,
};

struct SystemTypedAccess
{
    std::type_index type{ typeid(void) };
    eSystemAccessMode mode = eSystemAccessMode::Read;
    const char* pszDebugName = nullptr;
};

struct SystemAccessDesc
{
    bool_t bUnknown = true;
    bool_t bWritesWorldStructure = false;
    eSystemThreadAffinity eThreadAffinity = eSystemThreadAffinity::AnyWorker;
    std::vector<SystemTypedAccess> vecComponents;
    std::vector<SystemTypedAccess> vecResources;
};

class CSystemAccessBuilder
{
public:
    template<typename T>
    CSystemAccessBuilder& Read()
    {
        Add<T>(m_desc.vecComponents, eSystemAccessMode::Read);
        return *this;
    }

    template<typename T>
    CSystemAccessBuilder& Write()
    {
        Add<T>(m_desc.vecComponents, eSystemAccessMode::Write);
        return *this;
    }

    template<typename T>
    CSystemAccessBuilder& ReadResource()
    {
        Add<T>(m_desc.vecResources, eSystemAccessMode::Read);
        return *this;
    }

    template<typename T>
    CSystemAccessBuilder& WriteResource()
    {
        Add<T>(m_desc.vecResources, eSystemAccessMode::Write);
        return *this;
    }

    CSystemAccessBuilder& CreatesOrDestroysEntities()
    {
        m_desc.bUnknown = false;
        m_desc.bWritesWorldStructure = true;
        return *this;
    }

    CSystemAccessBuilder& MainThreadOnly()
    {
        m_desc.eThreadAffinity = eSystemThreadAffinity::MainThread;
        return *this;
    }

    CSystemAccessBuilder& UnknownWritesAll()
    {
        m_desc.bUnknown = true;
        m_desc.bWritesWorldStructure = true;
        m_desc.vecComponents.clear();
        m_desc.vecResources.clear();
        return *this;
    }

    const SystemAccessDesc& GetDesc() const
    {
        return m_desc;
    }

private:
    template<typename T>
    void Add(std::vector<SystemTypedAccess>& accesses, eSystemAccessMode mode)
    {
        m_desc.bUnknown = false;
        accesses.push_back(
            SystemTypedAccess{ std::type_index(typeid(T)), mode, typeid(T).name() });
    }

    SystemAccessDesc m_desc{};
};

inline bool SystemTypedAccessConflicts(
    const std::vector<SystemTypedAccess>& lhs,
    const std::vector<SystemTypedAccess>& rhs)
{
    for (const SystemTypedAccess& a : lhs)
    {
        for (const SystemTypedAccess& b : rhs)
        {
            if (a.type != b.type)
                continue;
            if (a.mode == eSystemAccessMode::Write ||
                b.mode == eSystemAccessMode::Write)
            {
                return true;
            }
        }
    }
    return false;
}

inline bool SystemAccessConflicts(
    const SystemAccessDesc& lhs,
    const SystemAccessDesc& rhs)
{
    if (lhs.bUnknown || rhs.bUnknown)
        return true;
    if (lhs.bWritesWorldStructure || rhs.bWritesWorldStructure)
        return true;
    return SystemTypedAccessConflicts(lhs.vecComponents, rhs.vecComponents) ||
        SystemTypedAccessConflicts(lhs.vecResources, rhs.vecResources);
}
```

#### 2-5. C:/Users/user/Desktop/Winters/Engine/Public/ECS/ISystem.h

기존 default `DescribeAccess`:

```cpp
    virtual void DescribeAccess(CSystemAccessBuilder& builder) const
    {
        builder.UnknownWritesAll();
    }
```

아래로 교체:

```cpp
    virtual void DescribeAccess(CSystemAccessBuilder& builder) const
    {
        builder.UnknownWritesAll().MainThreadOnly();
    }
```

미선언 system은 성능만 잃고 correctness는 잃지 않는 fail-safe 기본값이 된다.

#### 2-6. C:/Users/user/Desktop/Winters/Engine/Public/ECS/SystemScheduler.h

파일 전체를 아래로 교체:

```cpp
#pragma once

#include <map>
#include <memory>
#include <vector>

#include "ECS/ISystem.h"
#include "WintersAPI.h"

class CJobSystem;
class CWorld;

class WINTERS_ENGINE CSystemSchedular
{
public:
    CSystemSchedular() = default;
    ~CSystemSchedular() = default;

    CSystemSchedular(const CSystemSchedular&) = delete;
    CSystemSchedular& operator=(const CSystemSchedular&) = delete;
    CSystemSchedular(CSystemSchedular&&) = default;
    CSystemSchedular& operator=(CSystemSchedular&&) = default;

    void Initialize(CJobSystem* pJobSystem);
    void RegisterSystem(std::unique_ptr<ISystem> system);
    bool_t Execute(CWorld& world, float fTimeDelta);

private:
    struct PlannedSystem
    {
        ISystem* pSystem = nullptr;
        SystemAccessDesc Access{};
    };

    struct SystemBatch
    {
        std::vector<PlannedSystem> Systems;
        bool_t bMainThreadOnly = false;
    };

    using PhaseExecutionPlan = std::vector<SystemBatch>;

    void RebuildExecutionPlan();

    CJobSystem* m_pJobSystem = nullptr;
    std::map<uint32_t, std::vector<std::unique_ptr<ISystem>>> m_mapPhases;
    std::map<uint32_t, PhaseExecutionPlan> m_mapExecutionPlan;
    std::uint64_t m_uUnknownSystemCount = 0;
    std::uint64_t m_uMainThreadSystemCount = 0;
    std::uint64_t m_uWorkerEligibleSystemCount = 0;
    bool_t m_bExecutionPlanDirty = true;
};
```

`Schedular` 오탈자 rename은 ABI/호출부 잡음을 만드는 별도 cleanup이므로 이번 slice에서 하지 않는다.

#### 2-7. C:/Users/user/Desktop/Winters/Engine/Private/ECS/SystemScheduler.cpp

파일 전체를 아래로 교체한다. 등록 순서와 phase barrier를 보존하고,
`MainThread` system 양쪽에서 worker batch를 flush한다. 제출 도중 예외가 나더라도 이미
공개된 job을 join한 뒤 stack counter를 파괴한다.

```cpp
#include "WintersPCH.h"
#include "ECS/SystemScheduler.h"
#include "Core/JobCounter.h"
#include "Core/JobSystem.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"

#include <algorithm>

namespace
{
    constexpr size_t kMinParallelBatchSize = 2u;

    SystemAccessDesc BuildAccessDesc(const ISystem& system)
    {
        CSystemAccessBuilder builder;
        system.DescribeAccess(builder);
        return builder.GetDesc();
    }
}

void CSystemSchedular::Initialize(CJobSystem* pJobSystem)
{
    m_pJobSystem = pJobSystem;
}

void CSystemSchedular::RegisterSystem(std::unique_ptr<ISystem> system)
{
    const uint32_t phase = system->GetPhase();
    m_mapPhases[phase].push_back(std::move(system));
    m_bExecutionPlanDirty = true;
}

void CSystemSchedular::RebuildExecutionPlan()
{
    m_mapExecutionPlan.clear();
    m_uUnknownSystemCount = 0;
    m_uMainThreadSystemCount = 0;
    m_uWorkerEligibleSystemCount = 0;

    for (auto& [phase, systems] : m_mapPhases)
    {
        PhaseExecutionPlan& phasePlan = m_mapExecutionPlan[phase];
        phasePlan.reserve(systems.size());

        SystemBatch workerBatch;
        workerBatch.Systems.reserve(systems.size());

        auto flushWorkerBatch = [&]()
        {
            if (workerBatch.Systems.empty())
                return;

            phasePlan.push_back(std::move(workerBatch));
            workerBatch = SystemBatch{};
            workerBatch.Systems.reserve(systems.size());
        };

        for (auto& system : systems)
        {
            SystemAccessDesc access = BuildAccessDesc(*system);
            if (access.bUnknown)
                ++m_uUnknownSystemCount;

            if (access.eThreadAffinity == eSystemThreadAffinity::MainThread)
            {
                ++m_uMainThreadSystemCount;
                flushWorkerBatch();

                SystemBatch mainBatch;
                mainBatch.bMainThreadOnly = true;
                mainBatch.Systems.push_back(
                    PlannedSystem{ system.get(), std::move(access) });
                phasePlan.push_back(std::move(mainBatch));
                continue;
            }

            ++m_uWorkerEligibleSystemCount;
            bool bConflicts = false;
            for (const PlannedSystem& planned : workerBatch.Systems)
            {
                if (SystemAccessConflicts(access, planned.Access))
                {
                    bConflicts = true;
                    break;
                }
            }

            if (bConflicts)
                flushWorkerBatch();

            workerBatch.Systems.push_back(
                PlannedSystem{ system.get(), std::move(access) });
        }

        flushWorkerBatch();
    }

    m_bExecutionPlanDirty = false;
}

bool_t CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    if (m_bExecutionPlanDirty)
        RebuildExecutionPlan();

    const CJobSystemStats jobStatsBefore = m_pJobSystem
        ? m_pJobSystem->GetStats()
        : CJobSystemStats{};

    uint64_t sequentialBatchCount = 0;
    uint64_t parallelBatchCount = 0;
    uint64_t submittedJobCount = 0;
    uint64_t maxBatchSize = 0;
    uint64_t failedBatchCount = 0;

    auto emitCounters = [&]()
    {
        WINTERS_PROFILE_COUNT(
            "Scheduler::SequentialBatches",
            sequentialBatchCount);
        WINTERS_PROFILE_COUNT(
            "Scheduler::ParallelBatches",
            parallelBatchCount);
        WINTERS_PROFILE_COUNT(
            "Scheduler::SubmittedJobs",
            submittedJobCount);
        WINTERS_PROFILE_COUNT("Scheduler::MaxBatchSize", maxBatchSize);
        WINTERS_PROFILE_COUNT("Scheduler::FailedBatches", failedBatchCount);
        WINTERS_PROFILE_COUNT(
            "Scheduler::UnknownSystems",
            m_uUnknownSystemCount);
        WINTERS_PROFILE_COUNT(
            "Scheduler::MainThreadSystems",
            m_uMainThreadSystemCount);
        WINTERS_PROFILE_COUNT(
            "Scheduler::WorkerEligibleSystems",
            m_uWorkerEligibleSystemCount);
        if (m_pJobSystem)
        {
            const CJobSystemStats jobStatsAfter = m_pJobSystem->GetStats();
            WINTERS_PROFILE_COUNT(
                "JobSystem::WorkerExecutions",
                jobStatsAfter.uWorkerExecutions -
                    jobStatsBefore.uWorkerExecutions);
            WINTERS_PROFILE_COUNT(
                "JobSystem::ExternalHelpExecutions",
                jobStatsAfter.uExternalHelpExecutions -
                    jobStatsBefore.uExternalHelpExecutions);
        }
    };

    for (auto& [phase, phasePlan] : m_mapExecutionPlan)
    {
        (void)phase;
        for (const SystemBatch& batch : phasePlan)
        {
            maxBatchSize = (std::max)(
                maxBatchSize,
                static_cast<uint64_t>(batch.Systems.size()));

            if (batch.bMainThreadOnly ||
                batch.Systems.size() < 2u ||
                m_pJobSystem == nullptr)
            {
                ++sequentialBatchCount;
                try
                {
                    for (const PlannedSystem& planned : batch.Systems)
                        planned.pSystem->Execute(world, fTimeDelta);
                }
                catch (...)
                {
                    ++failedBatchCount;
                    OutputDebugStringA(
                        "[SystemScheduler] main-thread batch failed.\n");
                    emitCounters();
                    return false;
                }
                continue;
            }

            ++parallelBatchCount;
            CJobCounter counter;
            bool_t bSubmitFailed = false;
            try
            {
                for (const PlannedSystem& planned : batch.Systems)
                {
                    ISystem* const pSystem = planned.pSystem;
                    m_pJobSystem->Submit(
                        [pSystem, &world, fTimeDelta]()
                        {
                            try
                            {
                                pSystem->Execute(world, fTimeDelta);
                            }
                            catch (...)
                            {
                                OutputDebugStringA(
                                    "[SystemScheduler] worker system failed.\n");
                                throw;
                            }
                        },
                        &counter);
                    ++submittedJobCount;
                }
            }
            catch (...)
            {
                bSubmitFailed = true;
                OutputDebugStringA(
                    "[SystemScheduler] batch submission failed; joining published jobs.\n");
            }

            // 부분 publication 뒤 예외가 나도 counter 수명보다 job이 오래 살지 않는다.
            m_pJobSystem->WaitForCounter(&counter);
            if (bSubmitFailed || counter.HasFailure())
            {
                ++failedBatchCount;
                emitCounters();
                return false;
            }
        }
    }

    emitCounters();
    return true;
}
```

#### 2-8. C:/Users/user/Desktop/Winters/Engine/Public/ECS/Systems/SpatialHashSystem.h

기존 `Execute` 선언 아래에 추가:

```cpp
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
```

#### 2-9. C:/Users/user/Desktop/Winters/Engine/Private/ECS/Systems/SpatialHashSystem.cpp

`namespace Engine` 진입과 `Execute` 사이에 추가:

```cpp
void CSpatialHashSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.Read<TransformComponent>()
        .Write<SpatialAgentComponent>()
        .WriteResource<CSpatialIndex>();
}
```

필요 include:

```cpp
#include "ECS/Components/TransformComponent.h"
```

#### 2-10. C:/Users/user/Desktop/Winters/Engine/Private/ECS/Systems/VisionSystem.cpp

`CVisionSystem::DescribeAccess`의 마지막 기존 코드:

```cpp
        .Read<SpatialAgentComponent>()
        .Read<VisionSourceComponent>()
        .Read<VisionConeComponent>();
```

아래로 교체:

```cpp
        .Read<SpatialAgentComponent>()
        .Read<VisionSourceComponent>()
        .Read<VisionConeComponent>()
        .ReadResource<CSpatialIndex>()
        .ReadResource<CConcealmentVolumeIndex>();
```

#### 2-11. C:/Users/user/Desktop/Winters/Client/Public/ECS/Systems/YoneSoulSpawnSystem.h

기존 `Execute` 선언 아래에 추가:

```cpp
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
```

#### 2-12. C:/Users/user/Desktop/Winters/Client/Private/ECS/Systems/YoneSoulSpawnSystem.cpp

`Create()`와 `Execute()` 사이에 추가:

```cpp
void CYoneSoulSpawnSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    builder.CreatesOrDestroysEntities().MainThreadOnly();
}
```

이 system은 `ModelRenderer` 생성, FX spawn, world create/destroy를 수행하므로 worker 후보가 아니다. component 목록을 길게 나열해 병렬화를 암시하지 않고 structural+main 계약으로 닫는다.

#### 2-13. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

`m_pScheduler` 멤버 아래에 추가:

```cpp
    bool_t m_bSchedulerExecutionFailed = false;
```

#### 2-14. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

`OnUpdate`에서 main-menu/replay-exit lifecycle 처리 직후의 기존 코드:

```cpp
    if (m_bExitReplayToMyInfoRequested)
    {
        m_bExitReplayToMyInfoRequested = false;
        ChangeToMyInfoScene();
        return;
    }

    CGameInstance::Get()->UI_Set_StatusPanelOpen(CInput::Get().IsKeyDown(VK_TAB));
```

아래로 교체:

```cpp
    if (m_bExitReplayToMyInfoRequested)
    {
        m_bExitReplayToMyInfoRequested = false;
        ChangeToMyInfoScene();
        return;
    }

    if (m_bSchedulerExecutionFailed)
        return;

    CGameInstance::Get()->UI_Set_StatusPanelOpen(CInput::Get().IsKeyDown(VK_TAB));
```

이 위치는 menu/scene 이탈은 허용하지만 `PumpNetwork`, replay/snapshot 적용, ECS sync 등
world mutation보다 앞이다. 별도 error scene을 이번 slice에서 새로 만들지 않고, Debug 진단과
해당 match update의 fail-closed 정지만 적용한다.

Scheduler block의 기존 코드:

```cpp
        if (m_pScheduler)
            m_pScheduler->Execute(m_World, dt);
```

아래로 교체:

```cpp
        if (m_pScheduler && !m_pScheduler->Execute(m_World, dt))
        {
            m_bSchedulerExecutionFailed = true;
            OutputDebugStringA(
                "[Scene_InGame] scheduler batch failed; frame updates stopped.\n");
            return;
        }
```

#### 2-15. C:/Users/user/Desktop/Winters/Tools/Harness/JobSystemStress.cpp

include block의 `Core/JobSystem/WorkStealingDeque.h` 아래에 추가:

```cpp
#include "ECS/ISystem.h"
#include "ECS/SystemScheduler.h"
#include "ECS/World.h"
```

기존 출력의 `fiber_pool_misses` 아래에 추가:

```cpp
            << " worker_executions=" << stats.uWorkerExecutions
            << " external_help_executions=" << stats.uExternalHelpExecutions
```

기존 exception completion case에는 다음 판정을 추가:

```cpp
        Require(counter.GetFailureCount() == 1u,
            "exception completion did not propagate group failure");
```

`RunMode` 앞에 아래 전체 probe를 추가한다.

```cpp
    struct SchedulerTestComponentA {};
    struct SchedulerTestComponentB {};
    struct SchedulerTestResource {};

    enum class eSchedulerTestAccess
    {
        ComponentAWrite,
        ComponentBWrite,
        ResourceRead,
        ResourceWrite,
        MainThread,
        Throwing,
    };

    struct SchedulerTestProbe
    {
        std::atomic<int> iActive{ 0 };
        std::atomic<int> iMaxActive{ 0 };
        std::atomic<int> iExecuted{ 0 };
        std::atomic<bool> bExpectedThread{ true };
        std::barrier<>* pEntryBarrier = nullptr;
        std::thread::id ExpectedThread{};
    };

    class CSchedulerTestSystem final : public ISystem
    {
    public:
        CSchedulerTestSystem(
            const char* pName,
            eSchedulerTestAccess eAccess,
            SchedulerTestProbe& probe)
            : m_pName(pName), m_eAccess(eAccess), m_probe(probe)
        {
        }

        uint32_t GetPhase() const override { return 5u; }
        const char* GetName() const override { return m_pName; }

        void DescribeAccess(CSystemAccessBuilder& builder) const override
        {
            switch (m_eAccess)
            {
            case eSchedulerTestAccess::ComponentAWrite:
                builder.Write<SchedulerTestComponentA>();
                break;
            case eSchedulerTestAccess::ComponentBWrite:
                builder.Write<SchedulerTestComponentB>();
                break;
            case eSchedulerTestAccess::ResourceRead:
                builder.ReadResource<SchedulerTestResource>();
                break;
            case eSchedulerTestAccess::ResourceWrite:
                builder.WriteResource<SchedulerTestResource>();
                break;
            case eSchedulerTestAccess::MainThread:
                builder.Read<SchedulerTestComponentA>().MainThreadOnly();
                break;
            case eSchedulerTestAccess::Throwing:
                builder.Write<SchedulerTestComponentA>();
                break;
            }
        }

        void Execute(CWorld&, float) override
        {
            const int iActive =
                m_probe.iActive.fetch_add(1, std::memory_order_acq_rel) + 1;
            int iObserved = m_probe.iMaxActive.load(std::memory_order_relaxed);
            while (iActive > iObserved &&
                !m_probe.iMaxActive.compare_exchange_weak(
                    iObserved,
                    iActive,
                    std::memory_order_relaxed))
            {
            }

            if (m_probe.ExpectedThread != std::thread::id{} &&
                std::this_thread::get_id() != m_probe.ExpectedThread)
            {
                m_probe.bExpectedThread.store(false, std::memory_order_release);
            }
            if (m_probe.pEntryBarrier)
                m_probe.pEntryBarrier->arrive_and_wait();

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            m_probe.iExecuted.fetch_add(1, std::memory_order_relaxed);
            m_probe.iActive.fetch_sub(1, std::memory_order_acq_rel);
            if (m_eAccess == eSchedulerTestAccess::Throwing)
                throw std::runtime_error("intentional scheduler failure");
        }

    private:
        const char* m_pName = nullptr;
        eSchedulerTestAccess m_eAccess;
        SchedulerTestProbe& m_probe;
    };

    std::unique_ptr<ISystem> MakeSchedulerTestSystem(
        const char* pName,
        eSchedulerTestAccess eAccess,
        SchedulerTestProbe& probe)
    {
        return std::make_unique<CSchedulerTestSystem>(pName, eAccess, probe);
    }

    void RunSchedulerContractStress()
    {
        CJobSystem jobs;
        jobs.SetExecutionMode(eJobExecutionMode::ThreadOnly);
        jobs.Initialize(4);
        CWorld world;

        {
            std::barrier entryBarrier(2);
            SchedulerTestProbe probe;
            probe.pEntryBarrier = &entryBarrier;
            CSystemSchedular scheduler;
            scheduler.Initialize(&jobs);
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "DisjointA", eSchedulerTestAccess::ComponentAWrite, probe));
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "DisjointB", eSchedulerTestAccess::ComponentBWrite, probe));
            Require(scheduler.Execute(world, 0.0f),
                "disjoint scheduler batch failed");
            Require(probe.iExecuted.load() == 2 && probe.iMaxActive.load() == 2,
                "disjoint systems did not overlap");
        }

        {
            SchedulerTestProbe probe;
            CSystemSchedular scheduler;
            scheduler.Initialize(&jobs);
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "WriterA1", eSchedulerTestAccess::ComponentAWrite, probe));
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "WriterA2", eSchedulerTestAccess::ComponentAWrite, probe));
            Require(scheduler.Execute(world, 0.0f),
                "component-conflict scheduler batch failed");
            Require(probe.iExecuted.load() == 2 && probe.iMaxActive.load() == 1,
                "component writers overlapped");
        }

        {
            SchedulerTestProbe probe;
            CSystemSchedular scheduler;
            scheduler.Initialize(&jobs);
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "ResourceReader", eSchedulerTestAccess::ResourceRead, probe));
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "ResourceWriter", eSchedulerTestAccess::ResourceWrite, probe));
            Require(scheduler.Execute(world, 0.0f),
                "resource-conflict scheduler batch failed");
            Require(probe.iExecuted.load() == 2 && probe.iMaxActive.load() == 1,
                "resource reader/writer overlapped");
        }

        {
            SchedulerTestProbe probe;
            probe.ExpectedThread = std::this_thread::get_id();
            CSystemSchedular scheduler;
            scheduler.Initialize(&jobs);
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "MainThread", eSchedulerTestAccess::MainThread, probe));
            Require(scheduler.Execute(world, 0.0f),
                "main-thread scheduler batch failed");
            Require(probe.bExpectedThread.load(std::memory_order_acquire),
                "main-thread-only system ran on a worker");
        }

        {
            SchedulerTestProbe probe;
            CSystemSchedular scheduler;
            scheduler.Initialize(&jobs);
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "Throwing", eSchedulerTestAccess::Throwing, probe));
            scheduler.RegisterSystem(MakeSchedulerTestSystem(
                "ThrowPartner", eSchedulerTestAccess::ComponentBWrite, probe));
            const CJobSystemStats before = jobs.GetStats();
            Require(!scheduler.Execute(world, 0.0f),
                "throwing scheduler batch reported success");
            const CJobSystemStats after = jobs.GetStats();
            Require(after.uFailed == before.uFailed + 1u,
                "scheduler worker failure was not counted");

            CJobCounter survivorCounter;
            std::atomic<bool> bSurvivorRan{ false };
            jobs.Submit([&bSurvivorRan]() {
                bSurvivorRan.store(true, std::memory_order_release);
            }, &survivorCounter);
            jobs.WaitForCounter(&survivorCounter);
            Require(bSurvivorRan.load(std::memory_order_acquire),
                "worker loop did not survive scheduler failure");
        }

        jobs.Shutdown();
        std::cout << "case=scheduler_contracts result=pass\n";
    }
```

`main`의 ThreadOnly 전용 probe 마지막 기존 호출:

```cpp
            RunWorkerLifecycleGuard();
```

아래로 교체:

```cpp
            RunWorkerLifecycleGuard();
            RunSchedulerContractStress();
```

부분 Submit 예외는 현 API에서 deterministic injection 지점이 없는 `bad_alloc` 계열이다.
production API에 test-only fault hook을 추가하지 않는다. 대신 §2-7의 catch→무조건
`WaitForCounter`→return 순서를 독립 비평과 코드리뷰 gate로 고정하고, 기존
`RunSubmitShutdownRace`/`RunExternalWaitShutdownRace`를 MSVC AddressSanitizer 구성에서도
100회 반복한다. allocator fault harness가 필요해지면 별도 테스트 전용 executable
계획으로 분리하며, 이 미주를 삭제하고 검증했다고 주장하지 않는다.

### Stage B — normal Client workload 증명 (Stage A 뒤)

#### 2-16. C:/Users/user/Desktop/Winters/Engine/Private/ECS/SystemScheduler.cpp

다음 frame JSON counter를 추가한다.

```text
Scheduler::UnknownSystems
Scheduler::MainThreadSystems
Scheduler::WorkerEligibleSystems
Scheduler::FailedBatches
```

`ExecuteItemInline` hot path에서는 atomic 누적만 하고, Scheduler가 실행 전/후 stats delta를
frame당 한 번 `JobSystem::WorkerExecutions`/`ExternalHelpExecutions` counter로 방출한다.
profiler의 전역 counter mutex를 job마다 잡아 측정 대상을 오염시키지 않는다. rawEvents의
`threadId`와 함께 써서 실제 worker 실행을 판정한다.

#### 2-17. Client registered system 계약 감사

normal network/replay 등록 5개는 다음으로 고정한다.

| System | Phase | 계약/판정 |
|---|---:|---|
| Transform | 0 | `Write<Transform>`; root tree 내부 fan-out만 허용 |
| SpatialHash | 1 | Transform read, SpatialAgent + SpatialIndex write |
| LocalStatus | 5 | status 3종 write, worker eligible |
| Vision | 5 | Visibility write + Transform/Spatial/Vision read + index resource read, worker eligible |
| YoneSoulSpawn | 9 | structural + renderer/FX, main only |

offline-only Turret/Projectile/BT/MCTS는 Shared boundary와 구조 변경/behavior callback 때문에 이번 plan에서 억지로 worker eligible로 바꾸지 않는다. default unknown+main으로 안전하게 남기고 별도 adapter/cutover 세션에서만 다룬다.

현재 replay에서 안전한 co-batch 후보는 LocalStatus+Vision뿐이므로 `MaxBatchSize=2`는 성공이다. 3 이상을 completion gate로 두지 않는다.

### Stage C — 실제 성능 환전 수직 슬라이스 (Stage A/B 완료 후 별도 세션 PLAN 선행)

#### 2-18. Server immutable snapshot encode jobification

이 단계는 실제 workload를 이력서 성능 수치로 환전하는 구현 작업이다. 공개 산출물에
배정한 30% 천장 예산은 Stage D에서 별도로 지킨다.

```text
[m_stateMutex] serial GameRoom owner
  -> SnapshotBuilder가 world/entityMap을 1회 읽어 immutable SnapshotFrameInput 생성
  -> session별 {sid, ack, feedback, yourNetId}를 deterministic index로 고정
  -> replay canonical snapshot/keyframe에 필요한 owner work 완료
unlock m_stateMutex
  -> JobSystem에 bounded contiguous recipient partitions submit
  -> index별 owned DetachedBuffer 수거 + counter wait
  -> GameRoom owner가 sid index 순서대로 SendFrame
```

금지:

- job이 mutable `CWorld`, `CGameRoom`, `CSession`, socket/OVERLAPPED를 캡처하지 않는다.
- job에서 `SendFrame`을 호출하지 않는다.
- Shared/GameSim에 `CJobSystem`, Fiber, Win32 타입을 추가하지 않는다.
- snapshot byte/hash parity 없이 speedup만 채택하지 않는다.

대상 파일:

```text
C:/Users/user/Desktop/Winters/Server/Public/Game/SnapshotBuilder.h
C:/Users/user/Desktop/Winters/Server/Private/Game/SnapshotBuilder.cpp
C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomTick.cpp
C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomReplication.cpp
C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomBotMatchSoak.cpp
C:/Users/user/Desktop/Winters/Tools/Harness/RunGameRoomBotMatchSoak.ps1
```

실제 적용은 한 packet으로 몰지 않고 다음 순서로 닫는다.

#### Stage C0 — 착수 전 freeze·baseline·결정성 blocker

```text
owned packet A1 (Engine safety):
  Engine JobCounter/JobSystem/SystemAccess/SystemScheduler + JobSystemStress만

owned packet A2 (Client adoption, A1 handoff 후):
  SpatialHash/Vision/Yone access 계약 + Scene failure bridge + Client replay profiler만

owned packet B (Server replication, A1 handoff 후):
  SnapshotBuilder.h/.cpp + GameRoom.h + GameRoomTick.cpp + GameRoomReplication.cpp
  + server snapshot harness만

read-only:
  GameRoom.cpp/Commands/AI/UnitAI, Shared/GameSim, schemas, transports
```

- A1/A2/B는 동시에 열지 않는다. A1이 `Handoff`되고 EngineSDK가 동기화된 뒤
  A2 또는 B를 하나만 연다. Server completion은 A2 완료를 기다릴 필요는 없다.
- 현재 dirty인 `SnapshotBuilder.cpp`/`GameRoom.h` owner가 종료되기 전 B는 `Blocked`다.
- 기존 same-seed 10-bot final world hash 불일치를 먼저 재현·원인 분리한다. serial에서도
  불일치하면 snapshot jobification과 무관한 blocker로 별도 처리한다.
- serial baseline에 `Capture`, `EncodeOneRecipient`, `EncodeAllRecipients`, total tick을
  분리 계측한다. `EncodeAllRecipients` p95가 `0.5 ms` 미만이면 Stage C1~C2를 중단한다.

#### Stage C1 — immutable replication contract

`CSnapshotBuilder::Build(CWorld&, ...)`를 worker에서 N번 호출하는 2026-05-07 설계는
현재 기준으로 그대로 적용하지 않는다. `GetComponent<T>()`가 내부적으로
`GetOrCreateStore<T>()` 경로를 갖고 있어, “지금은 store가 존재한다”는 전제만으로
worker-safe API를 선언하면 유지보수 중 lazy insertion race가 재발할 수 있기 때문이다.

병렬화 전에 중복 제거 대안을 같은 serial baseline으로 비교한다.

```text
A. 현재: recipient마다 world traversal + full FlatBuffer encode
B. Capture 1회 + recipient마다 full encode
C. team/view variant 공통 entity payload + recipient metadata 분리
D. canonical common payload 1회 + 작은 recipient companion packet
```

B가 이번 최소 변경 후보지만 자동 채택하지 않는다. C/D가 protocol/schema 변경 없이
가능한지 먼저 확인하고, 직렬 중복 제거만으로 목표를 달성하면 N-way encode를 추가하지
않는다. C/D가 wire contract 변경을 요구하면 별도 network schema plan으로 분리하고,
이번 slice는 B의 남은 encode 비용이 p95 `0.5 ms` 이상일 때만 C2로 진행한다.

분리 계약:

```text
Capture(CWorld, EntityIdMap, tick-common state)     // room owner only
  -> immutable SnapshotFrameInput                  // entity order/net id 고정

EncodeRecipient(SnapshotFrameInput, RecipientView) // AnyWorker, pure output
  -> DetachedBuffer                                // owned result
```

`RecipientView`는 `sessionId`, `lastAckedSeq`, feedback 5개, `yourNetId`만 보유한다.
`SnapshotFrameInput`은 FlatBuffer offset이나 `CWorld` pointer를 보유하지 않고, encoding에
필요한 값형/POD/vector만 보유한다. capture 뒤 world mutation이 없어도 worker가 world를
참조하지 않는 계약을 기계적으로 보장한다.

#### Stage C2 — Server recipient-batch scheduler + global pool 소비

- session view를 `sessionId` 오름차순으로 수집하고 output vector index를 고정한다.
- recipient 0~1개, JobSystem 미초기화, worker 0~1개, serial diagnostic mode는 직렬 encode한다.
- 2개 이상이면 `CServerEntry::Get_JobSystem()`의 process-global pool에
  `min(recipientCount, workerCount)`개의 contiguous index partition을 제출한다. 한 room이
  recipient 수만큼 global queue를 무제한 점유하지 않는다.
- 각 job은 immutable frame/view와 자기 index range의 output만 사용한다.
- 일부 Submit이 실패하면 이미 공개된 job을 counter로 join한 뒤 batch 전체를 실패 처리한다.
- encode exception은 index별 failure flag에 기록하고, 실패 batch는 해당 tick snapshot을
  부분 전송하지 않는다. worker loop는 살아 있어야 한다.
- 실패 output은 전부 폐기한다. Debug/harness는 즉시 실패로 보고한다. Release room은
  snapshot job path를 sticky-disable하고 **같은 immutable frame 전체를 같은 tick에
  serial로 다시 encode**한다. 일부 parallel output과 serial output을 섞지 않는다.
- same-tick serial fallback까지 실패하면 그 tick snapshot을 보내지 않고 연속 실패
  counter를 증가시킨다. 성공 시 0으로 reset하며, 3 tick 연속 실패하면 bounded 진단 후
  `m_bRunning=false`로 room tick을 fail-closed 정지한다. 무한 silent starvation은 금지다.
- join 후 room owner가 sessionId 순으로 `SendFrame`한다. job에서 session/socket/hub 호출은 0개다.
- replay snapshot은 recipient encode와 섞지 않고 room owner의 canonical serial output으로
  보존한다. replay bytes/hash가 바뀌면 채택하지 않는다.
- production 기본 mode는 `ThreadOnly`로 유지한다. `FiberFull`은 nested wait가 실제로
  발생하고 Release 3×3에서 total tick 회귀가 없을 때만 opt-in 비교 경로로 유지하며,
  단순 encode fan-out에서 더 빠르다고 가정하지 않는다.
- `TickThread` 자체는 Job/Fiber로 전환하지 않는다. room owner는 immutable frame/view를
  만든 뒤 `m_stateMutex`를 해제하고 external OS thread로 counter를 join한다. 현재 외부
  `WaitForCounter`가 global pool의 다른 job도 help-execute할 수 있으므로 lock 안 join은
  금지한다. 향후 tick을 fiber로 바꾸더라도 동일한 lock 밖 handoff를 보존한다.

이 단계가 완료되면 Server source에서 production `Submit`/`WaitForCounter`는 recipient-batch
scheduler 한 경로에만 존재해야 한다. startup probe는 별도 lifecycle 검증 경로로 남는다.

#### Stage C3 — authoritative simulation scheduler 방향

snapshot slice가 닫혀도 곧바로 champion/minion/projectile phase를 병렬화하지 않는다.

1. 현재 explicit `Tick()` 순서를 Server scheduler의 canonical phase table로 문서화한다.
2. 각 phase에 World component read/write뿐 아니라 RNG, EntityIdMap, command/event queue,
   SpatialIndex, replay/keyframe resource access를 선언한다.
3. 독립성이 증명된 `read-only decide → deterministic indexed apply` phase만 후보로 둔다.
4. mutable apply, entity create/destroy, RNG draw, event sequence 할당은 room owner가 수행한다.
5. phase candidate마다 serial/parallel command list와 world/replay hash parity를 먼저 통과한다.

초기 후보는 snapshot encode 하나다. Server projectile/minion/Champion GameSim jobification은
각각 별도 측정에서 p95 비용과 conflict graph가 나온 뒤 같은 PLAN을 갱신해 진입한다.
Client ECS `CSystemSchedular`를 Server에 직접 소유시키는 작업은 이번 completion 조건이 아니다.
첫 Server 완료 단위의 정식 명칭은 **Server recipient-batch scheduler**로 고정한다.
`Server full phase scheduler`, `Engine CJobGraph`, `parallel authoritative GameSim` 완료라고
확대해 주장하지 않는다.

#### Stage C4 — source 적용 전 계획 갱신 gate

현재 soak fixture는 authoritative session view가 사실상 1개이므로 그대로는 `5-session`
성능 증거가 아니다. Stage C 구현 전 이 PLAN 갱신은 동일 `SnapshotFrameInput`에 서로 다른
`{sid, ack, feedback, yourNetId}` 5개를 주입하고 실제 socket send 없이 detached bytes를
수거하는 전용 encode benchmark mode를 위 두 harness 파일에 완전 코드로 추가해야 한다.
이 mode는 production `Capture`/`EncodeRecipient`/`RunRecipientBatch` symbol을 직접
호출해야 하며 benchmark 전용 submit/join 복제 구현은 금지한다. source grep과 link map으로
production batch symbol 1개만 존재하는지 확인한다.

`SnapshotBuilder.cpp`의 현재 `Build(CWorld&, ...)`는 수백 필드를 직접 읽으므로 이 master plan에서 불완전 code block으로 분해하지 않는다.

```text
CONFIRM_NEEDED: Stage C 구현을 시작하는 세션은 위 7파일의 당시 본문·dirty diff·active
work packet을 다시 고정하고, SnapshotFrameInput 전체 타입·Capture/Encode 전문·기존
Build 삭제/호환 경로·harness 전문을 **이 PLAN에 갱신**한 뒤 독립 비평을 다시 받아야 한다.
현재 `SnapshotBuilder.cpp`와 `GameRoom.h`가 다른 세션 dirty 상태이므로 코드 전문을 현재
본문 기준으로 박제하면 충돌 없는 지시서가 아니다. owner handoff 전에는 source edit 금지다.
```

Stage C serial baseline에서 병렬 후보인 `Snapshot::EncodeAllRecipients` p95가 `0.5 ms`
미만이면 jobification을 철회한다. `Capture`는 직렬 owner 비용으로 별도 공개하되, capture가
작다는 이유만으로 encode 후보를 기각하지 않는다. 그 경우 이력서 환전은 이미 구현된
correctness/worker 증명과 기술 글로 끝내며, 수치 만들기용 가짜 최적화를 하지 않는다.

Stage C benchmark 계약:

```text
- Release, 동일 PC/전원 모드/CPU affinity, worker=4 고정, background load 정리
- source SHA, executable SHA-256, CPU 모델/논리 코어, execution mode를 evidence JSON에 기록
- seed=42, warm-up 300 tick 제외, 측정 1,800 tick, active session view=5
- Serial/ThreadOnly/FiberFull을 round마다 순환 배치하여 각 3회(총 9런)
- 입력 SnapshotFrameInput hash와 5개 recipient output byte hash가 모든 mode/run에서 동일
- run별 Capture/EncodeAllRecipients/total tick p50·p95·p99와 deadline miss를 원본 JSON에 기록
- 3런 p95 중앙값으로 20% gate를 판정하고, total tick p99도 함께 공개
- job별 profiler counter 호출 금지; atomic 누적 후 tick/frame당 한 번만 방출
```

### Stage D — 문서/공개 환전 (전체 시간의 30%)

외부 마감 제안: **2026-07-25**까지 다음을 공개 가능한 형태로 고정한다.

1. 90초 데모/프로파일 캡처: scheduler batch와 worker thread lane, ThreadOnly/FiberFull parity.
2. 이력서 한 줄: 숫자는 아래 §4의 `확정 후` 템플릿만 사용.
3. 기술 글 1편: publication→Chase-Lev last-item→Fiber wait/resume→resource-aware scheduler→immutable encode의 결정 연결.

예상 일정은 Stage A `1일`, Stage B `0.5일`, determinism blocker `0.5~1일`,
조건부 Stage C `1~2일`, Stage D `0.5~1일`이다. 시간 배분은 Stage A/B/C
구현·검증 70%, 데모·이력서·글 30%로 고정한다. Stage A/B를 끝없이 다듬느라
Stage D를 미루지 않는다.

## 3. 검증

이 절은 **향후 구현 세션의 종료 조건**이다. 2026-07-18 이번 재개 세션에서는 사용자
지시에 따라 아래 build/harness를 실행하지 않는다.

```text
예측:
- JobSystem stress all: 기존 100,000 fan-out/race와 nested/saturation/shutdown 전부 PASS, counter failure=1 probe 추가 PASS.
- Scheduler synthetic: component/resource 충돌은 active max=1, disjoint는 실제 동시 진입, main affinity는 main thread id, throw batch는 Execute=false.
- normal replay: MaxBatchSize=2, ParallelBatches=1, SubmittedJobs=2/frame 유지; 명시 계약 후 UnknownSystems=0, FailedBatches=0.
- rawEvents: LocalStatus/Vision 또는 내부 Transform jobs 중 worker threadId 실행이 매 캡처 관측. ExternalHelp도 0일 필요 없음 — caller help는 설계된 work sharing이다.
- Client 전체 frame p95는 Stage A 전 3런 중앙값 대비 +5% 이내. Scheduler p95 0.1006ms가 너무 작아 유의 speedup은 기대하지 않는다.
- Stage C 채택 시 serial/ThreadOnly/FiberFull의 snapshot bytes, replay hash, final world hash가 10회 동일. Bot AI는 GameCommand 생산자이며 gameplay truth를 직접 변경하지 않는다.
- Stage C 성능 채택 gate: 5-session 동일 workload에서 EncodeAllRecipients p95가 baseline 대비 20% 이상 감소하고 total server tick p99가 33.333ms 아래, deadline miss 0. 하나라도 실패하면 serial rollback.
- Server production stats: startup probe를 제외한 submitted/executed delta가 planned partition 수와 일치하고 worker execution이 1회 이상, failed batch는 0.
- Server source grep: production Submit/Wait는 recipient-batch scheduler 1경로, worker의 CWorld/CGameRoom/CSession/socket capture는 0곳.
- Engine public header 변경 후 EngineSDK/inc는 UpdateLib.bat 산출물로 동기화된다.
```

향후 build lane 명령(`/m:1 /nr:false`, 다른 compiler process 0 확인 후 순차 실행):

```powershell
$buildProcesses = Get-Process msbuild, cl, link -ErrorAction SilentlyContinue
if ($buildProcesses) { throw "build lane busy: $($buildProcesses.Id -join ',')" }
$vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsRoot = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsRoot "MSBuild\Current\Bin\MSBuild.exe"

& $msbuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false
& ./UpdateLib.bat
& Tools/Harness/RunJobSystemStress.ps1 -Mode all

& $msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false
& $msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false
& $msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false
& $msbuild Winters.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Winters.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m:1 /nr:false
```

향후 correctness/runtime gate:

```powershell
& Tools/Harness/RunGameRoomBotMatchSoak.ps1 -Configuration Debug -TickCount 1800 -Seed 42 -Runs 10 -SkipServerBuild
& Tools/Harness/RunGameRoomBotMatchSoak.ps1 -Configuration Release -TickCount 1800 -Seed 42 -Runs 10 -SkipServerBuild
& Tools/Profiler/run_profile_session.ps1 -Label "job-scheduler-completion-baseline-1"
python Tools/Profiler/analyze_profiler_capture.py Profiles/<capture>.json --target-fps 144
git diff --check
```

추가할 5-recipient snapshot mode의 frozen-binary 매트릭스:

```text
Debug:   Serial 10회, ThreadOnly(worker=4) 10회, FiberFull(worker=4) 10회 — bytes/hash/실패 검증
Release: Serial/ThreadOnly/FiberFull 각 3회×3 round — Capture/Encode/total tick p50·p95·p99
Network: TCP, UDP(Debug ticket gate), dual 각각 active+bound session 5개로 30초 smoke;
         startup probe가 아닌 production planned-partition submitted/executed delta 확인
Shutdown: active snapshot batch 중 room stop/server shutdown 100회; accepted job drain와 UAF 0
```

```text
미검증:
- Stage C SnapshotFrameInput 전체 필드/코드 전문은 dirty owner handoff 후 이 PLAN 재갱신 전까지 미확정.
- cross-worker fiber migration, priority/cancellation, lock-free global queue는 비목표.
- 현재 dirty tree에서 관찰된 Server final world hash 런별 차이의 원인은 이번 감사에서 진단하지 않았다. Stage C 전 별도 blocker로 닫아야 한다.
- 이번 재개 세션은 계획 전용이므로 source 적용, Debug/Release build, runtime/soak 실행을 하지 않았다.

확인 필요:
- Stage A 적용 직전 병행 세션이 JobSystem/SystemScheduler/Scene_InGame/SnapshotBuilder를 수정 중인지 재확인.
- Release profiler 3×3 source SHA와 binary SHA를 증거 JSON에 기록.
- UpdateLib.bat 실행 후 EngineSDK/inc public header parity 확인.
```

## 4. 이력서 수치 사용 규칙

### 지금 바로 방어 가능한 문장

```text
C++20 기반 JobSystem을 구현해 4,096-slot Chase–Lev work-stealing deque,
worker당 64개 pooled Fiber, counter wait/resume와 shutdown admission을 구성하고,
100,000-job fan-out 및 100,000회 last-item owner/thief 경합에서 누락·중복 0을 검증했습니다.
```

```text
ECS SystemScheduler에 component access 기반 conflict batching을 연결해
실게임 replay에서 병렬 batch 0→1/frame, MaxBatchSize 1→2,
2 jobs/frame 제출과 worker thread 실행을 profiler raw event로 확인했습니다.
```

두 번째 문장에는 FPS 향상률을 붙이지 않는다. 기존 전/후 frame 수치는 오염·변동 때문에 개선 증거가 아니다.

### 구현 후에만 채우는 문장

```text
5-session authoritative snapshot encode를 immutable capture + N-way worker job으로 분리해
Release 3×3 기준 p95 [BEFORE]ms→[AFTER]ms([DELTA]%)로 단축했고,
ThreadOnly/FiberFull 10회 snapshot byte·replay/world hash parity와 30Hz deadline miss 0을 유지했습니다.
```

`jobs/s` Debug stress 숫자와 FiberFull이 ThreadOnly보다 빠르다는 문장은 사용 금지다. 2026-07-18 단일 correctness 실행에서도 workload에 따라 FiberFull이 더 느린 case가 있어, 반복 Release benchmark 전에는 성능 결론이 아니다.

## 서브 에이전트 비평

독립 read-only 비평: `/root/job_scheduler_plan_fast_critique`, 2026-07-18.

| 등급 | 지적 | 처분 | 반영/이유 |
|---|---|---|---|
| P0 | 여러 job 중 Submit 예외 시 이미 공개된 job이 stack `CJobCounter` 파괴 뒤 접근할 수 있음 | 수용 | §2-7을 catch 후에도 무조건 `WaitForCounter`하고 실패 반환하는 전문으로 교체. 이 join 전에는 unwind하지 않는다. |
| P1 | `RebuildExecutionPlan`이 산문/`CONFIRM_NEEDED`라 구현 가능한 계획이 아님 | 수용 | 등록 순서, phase, main 양측 flush, component/resource 충돌, unknown/main/worker 통계를 포함한 파일 전체 전문으로 확정. |
| P1 | Scene failure guard가 Scheduler block에 있어 다음 frame의 network/world mutation을 막지 못함 | 수용 | menu/scene lifecycle 처리 뒤, `PumpNetwork`와 replay/snapshot/ECS sync 전에 guard를 이동. 이번 slice는 error scene 추가 대신 match update fail-closed를 선택. |
| P1 | synthetic scheduler test 전문과 적용 anchor가 없음 | 수용 | §2-15에 component/resource conflict, 실제 overlap, main affinity, throw propagation, worker 생존 probe 전문과 main 호출 anchor를 추가. deterministic allocator failure는 production test hook을 만들지 않고 미검증으로 명시하며 catch→join은 코드리뷰 gate로 둠. |
| P1 | 3×3/5-session benchmark 재현 계약이 약하고 job별 profiler counter가 측정을 교란함 | 수용 | hot path는 atomic만 누적하고 frame당 1회 방출. source/binary SHA, CPU/worker/mode, warm-up/tick/session/input hash, 순환 3×3, 원본 p50/p95/p99 계약을 Stage C에 고정. 현 soak의 1-session 한계와 5-session 전용 fixture 필요도 명시. |

추가 자체 검토로 normal replay의 명시 계약 후 `UnknownSystems=0`, Engine build→`UpdateLib`→
stress 순서, Stage C 전 server final-hash determinism blocker를 정정했다. 비평 gate는
종료됐지만 Stage C의 `CONFIRM_NEEDED`는 의도된 별도 수직 슬라이스 gate이므로,
동일 PLAN 갱신과 독립 비평 전에는 snapshot source를 수정하지 않는다.

### Server 재개 독립 비평

독립 read-only 비평: `/root/server_scheduler_plan_critique`, 2026-07-18. 비평 agent는
파일 수정·build·test를 수행하지 않았다.

| 등급 | 지적 | 처분 | 반영/이유 |
|---|---|---|---|
| P1 | `m_stateMutex` 아래 외부 Wait가 global pool의 다른 job을 help-execute할 수 있음 | 수용 | Tick lock 안에서는 simulation+immutable capture만 수행하고 unlock 뒤 submit/join/send하도록 0-5와 Stage C를 수정. TickThread fiber 전환도 비목표로 고정. |
| P1 | dirty target owner/packet/branch가 없어 충돌 gate를 실제 실행할 수 없음 | 수용 | owner 미확인 상태를 `Blocked`로 명시. 구현 전 owner/device/agent/branch/owned/read-only/validation/report packet과 편집·build·handoff 3시점 hash gate를 요구. |
| P1 | 전체 encode 중복 제거 검토 없이 병렬화를 먼저 선택함 | 수용 | 현재 N traversal/full encode, capture 1회+N encode, team/view 공통 payload, common+recipient companion 네 대안을 serial baseline에서 먼저 비교. 남는 encode p95가 0.5ms 이상일 때만 jobify. |
| P1 | benchmark가 production scheduler를 복제할 수 있음 | 수용 | harness가 production Capture/Encode/RunRecipientBatch symbol을 직접 호출하도록 고정하고 benchmark-only submit/join을 금지. |
| P1 | 1-client transport smoke는 production parallel path를 밟지 않음 | 수용 | TCP/UDP/dual 각각 active+bound session 5개와 planned-partition stats를 runtime gate로 변경. |
| P1 | batch 실패 뒤 silent starvation/운영 정책이 없음 | 수용 | output 전량 폐기→same-frame serial fallback→parallel sticky-disable. serial도 3 tick 연속 실패하면 room tick fail-closed 정지로 고정. |
| P2 | 2026-05-07 역사 근거의 직접 anchor가 없음 | 부분 수용 | 역사 문서는 실제 `.md/TODO/05-07/Server/03_SERVER_PARALLEL_PHASES.md`에 존재하므로 날짜를 바꾸지 않고 직접 링크를 추가. 최신 as-built는 S023 결과와 함께 구분. |
| P2 | Server scheduler 명칭이 향후 범용 scheduler 주장으로 확장될 수 있음 | 수용 | 첫 완료 단위를 `Server recipient-batch scheduler`로 고정하고 full phase scheduler/CJobGraph/parallel GameSim은 비목표로 명시. |

Server 재개 비평 gate는 종료됐다. 다만 owner 미확인 dirty target과 Stage C 코드 전문
`CONFIRM_NEEDED`가 남아 있으므로 구현 gate는 열리지 않았다. 이번 세션은 계획 전용이며
source/프로젝트/build/runtime 변경은 0이다.
