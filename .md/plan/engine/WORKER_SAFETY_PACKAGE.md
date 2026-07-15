# Worker-Safety 통합 패키지 — 병렬 ECS 인프라 안정화

> **상태 동기화 (2026-07-11 — HISTORICAL PLAN)**: 본문의 `pAI/pNav Set_JobSystem` 임시 비활성은 과거 봉쇄 단계다. 현재 Main-push owner-race 경로는 수정됐고 Client local-only UnitAI/Nav 및 Transform의 ThreadOnly JobSystem 경로가 활성이다. 전용 JobSystem stress와 FiberFull은 여전히 미완료다. 최신 판정은 [상태 감사](../2026-07-11_JOB_SYSTEM_CHASE_LEV_FIBER_STATE_AUDIT.md)를 따른다.
>
**작성일**: 2026-04-28
**개정**: **v3 (Codex 3차 검토 반영)** — Decision 구조 분리 (desiredState/stopMovement/emitDamage), MinionStateComponent 5상태 확장 (Chase 추가), ISystem.Initialize 우회 (Set_JobSystem 시 resize), SystemAccess/TypeID 명시 정의, B-10c stale reference 전수 통일

**v2 → v3 차이**:
- §2.4 Decision 필드 분리 — `wantsAttack` 단일 → `desiredState` + `stopMovement` + `emitDamage` 3 필드 (cooldown 안 돼도 사거리 hold 가능)
- §2.4 MinionStateComponent.State 확장 — `{Idle, Moving, Attack, Dead}` 4상태 → `{Idle, LaneMove, Chase, Attack, Dead}` 5상태 (waypoint vs nav chase 분리)
- §2.4 ISystem.Initialize override 제거 → Set_JobSystem 시 per-slot buffer resize (현재 ISystem 인터페이스 변경 0)
- §4 SystemAccess + TypeID 정의 — `std::type_index` 채택 명시
- §6.3 / §9 — B-10c "deque race 정식 수정" 표현 → "잔여 (worker slot API + stress 검증)" 통일
- §외부 — Minion combat production 화 (animation state / attack windup / ranged / death) 는 본 패키지 외부, 별도 후속 phase

**전제**:
- Phase 5-A 완료, NavigationSystem 병렬화 활성화 직후 **CPUProfiler m_vStack race 로 crash**
- 사용자가 즉시 thread_local 분리 + merge mutex 로 해결
- Codex 1차 전수 스캔 — 추가 4건 worker-safety 위험 발견 (P1 ×2, P2 ×2, P3 ×1)
- Codex 2차 검토 — v1 의 5가지 부정확 지점 정정 (본 v2)
- ★ B-10c (JobSystem deque race) **본체는 이미 완료** — 코드 검증으로 확인. 남은 것은 worker slot API 노출 + stress 검증

**참조**:
- `06_POST_B10_INFRA_VS_CHAMPS.md` v4 §2 (B-10c JobSystem race + Scene 2줄 inject)
- `project_phase_5a_complete.md` (race 미결 박제)
- 본 사이클 학습 = 향후 메모리 + CLAUDE.md gotchas 박제 대상

---

## 0. 결론 먼저 — 사이클 분리 + 우선순위

| Phase | 내용 | 일수 | 상태 |
|---|---|---|---|
| **B-10c (대부분 완료)** | JobSystem deque race 정식 수정 — 본체 완료. 남은 것은 worker slot API 노출 + stress 검증 | 0.5일 (잔여) | ✅ EnqueueJob/GlobalQueue/WaitForCounter main drain 코드 검증 완료 |
| **★ B-10c-2 (신규)** | **Worker-Safety 통합 패키지** — Codex 1차 5건 + 2차 5건 (Decision/Apply 2-pass 포함) | **~3일** (v1 2.5일 → v2 3일 상향) | 본 계획서 |
| B-10d-pre | PlayerTransformAdapter 6 함수 | 1.5일 | 06 v4 §3 |
| B-10d | Riven Pure ECS | 2~3일 | 06 v4 §4 |

**B-10c 본체 완료 검증** (코드 직접 확인):
- ✅ [JobSystem.cpp:70-92](Engine/Private/Core/JobSystem.cpp:70) Submit 3 overload → `EnqueueJob` 통합 진입점
- ✅ [JobSystem.cpp:95-126](Engine/Private/Core/JobSystem.cpp:95) `EnqueueJob` — Shutdown/N==0 fallback + Worker self-deque + Global queue
- ✅ [JobSystem.cpp:165-175](Engine/Private/Core/JobSystem.cpp:165) `TryExecuteOneJob` — self deque → Global drain → Steal
- ✅ [JobSystem.cpp:221-231](Engine/Private/Core/JobSystem.cpp:221) `WaitForCounter` main 분기 — `hasGlobal` 플래그 + lock 밖 ExecuteItem
- ✅ Scene_InGame.cpp 2줄 inject (TransformSystem L92 + NavigationSystem L291 주석 해제)

**B-10c 잔여 (B-10c-2 와 흡수)**:
- `CJobSystem::Get_WorkerIdx()` / `Get_WorkerSlot()` 정적 메서드 노출
- `GetWorkerCount()` 노출 (per-worker buffer 크기 결정용)
- Stress 검증 (Submit 1만회 race 0)

**B-10c-2 분리 이유**:
- B-10c 는 "JobSystem 코어" — race-free Submit/WaitForCounter
- B-10c-2 는 "JobSystem 활성화로 드러난 주변 인프라 race" — Codex 5건 + 학습 박제

**완료된 것** (사용자 작업, 2026-04-28):
- ✅ CPUProfiler m_vStack 제거 → thread_local + merge mutex
- ✅ NavigationSystem 중복 `Nav::Execute` scope 제거
- ✅ JobSystem.h 공개 헤더 `<mutex>` include 명시
- ✅ JobSystem 본체 (EnqueueJob/GlobalQueue/WaitForCounter main drain) — 06 v4 §2 코드 적용 검증

**남은 것** (Codex 1차 5건 + 2차 정정):
- ❌ P1: MinionAI cross-entity HP/state write race → **Decision/Apply 2-pass 정식 해결**
- ❌ P1: FindClosestEnemy read while sibling jobs mutate → **DamageEvent 단독으로 안 닫힘. Decision/Apply 2-pass 필요** (Codex 2차 정정)
- ❌ P1: per-worker slot main help-stealing 충돌 → **slot 규칙 main=0, worker=idx+1** (Codex 2차 신규)
- ❌ P2: CommandBuffer push_back 비안전 → **per-worker (slot 규칙 동일)**
- ❌ P2: Scheduler component access contract 부재 → **기본값 = unknown/exclusive** (Codex 2차 정정)
- ❌ P3: AStar Counter early-return flush 누락 → **scope guard 패턴** (Codex 2차 정정)

---

## 1. 현재 worker-safety 진단 매트릭스

### 1.1 안전 자원 (검증 완료)

| 자원 | 정책 | 위치 |
|---|---|---|
| CPUProfiler scope stack | thread_local | [CPUProfiler.cpp:16](Engine/Private/Core/Profiler/CPUProfiler.cpp:16) `t_vProfilerStack` |
| CPUProfiler event/counter merge | mutex | [CPUProfiler.h:32](Engine/Public/Core/Profiler/CPUProfiler.h:32) `m_Mutex` |
| Pathfinder A* 작업 버퍼 | thread_local | [Pathfinder.cpp:47-49](Engine/Private/Manager/Navigation/Pathfinder.cpp:47) `tls_gScore/parent/closed` |
| WorkStealingDeque | atomic (Chase-Lev) | [WorkStealingDeque.h](Engine/Public/Core/JobSystem/WorkStealingDeque.h) |
| CJobCounter | atomic | [JobCounter.h](Engine/Public/Core/JobCounter.h) |
| ResourceCache | mutex | [ResourceCache.h](Engine/Public/Resource/ResourceCache.h) |
| Win32 OutputDebugStringA | API thread-safe | — |
| sprintf_s + 지역 buffer | 함수 지역 변수 | — |

### 1.2 위험 자원 (해결 필요)

| 자원 | 정책 | 발견 위치 | 우선순위 |
|---|---|---|---|
| **MinionAISystem cross-entity write** | 없음 (race) | [MinionAISystem.cpp:124-138](Engine/Private/ECS/Systems/MinionAISystem.cpp:124) | **P1 즉시** |
| **FindClosestEnemy read while sibling write** | 없음 (race) | [MinionAISystem.cpp:185-206](Engine/Private/ECS/Systems/MinionAISystem.cpp:185) | **P1 즉시** |
| **CCommandBuffer push_back** | 없음 (잠재) | [CommandBuffer.cpp:5-17](Engine/Private/ECS/CommandBuffer.cpp:5) | P2 1일 내 |
| **Scheduler component access contract** | 없음 | [SystemScheduler.cpp:22-43](Engine/Private/ECS/SystemScheduler.cpp:22) | P2 1일 내 |
| **AStar::NodesVisited counter contention** | mutex (안전하나 contention) | [Pathfinder.cpp:68](Engine/Private/Manager/Navigation/Pathfinder.cpp:68) | P3 10분 |

---

## 2. P1 — MinionAI cross-entity write race

### 2.1 현재 코드 ([MinionAISystem.cpp:124-138](Engine/Private/ECS/Systems/MinionAISystem.cpp:124))

```cpp
// worker thread 에서 ProcessMinion 병렬 실행 — 각 미니언이 타 entity 의 HP/state 직접 write
if (ms.attackCooldown <= 0.f && world.HasComponent<HealthComponent>(ms.attackTargetId))
{
    auto& tgtHp = world.GetComponent<HealthComponent>(ms.attackTargetId);   // ★ 타 entity
    tgtHp.fCurrent -= ms.attackDamage;                                      // ★ race
    if (tgtHp.fCurrent <= 0.f)
    {
        tgtHp.fCurrent = 0.f;
        tgtHp.bIsDead = true;                                               // ★ race
        if (world.HasComponent<MinionStateComponent>(ms.attackTargetId))
        {
            auto& tgtMs = world.GetComponent<MinionStateComponent>(ms.attackTargetId);
            tgtMs.current = MinionStateComponent::Dead;                     // ★ race
        }
    }
    ms.attackCooldown = ms.attackCooldownMax;
}
```

### 2.2 시나리오

- 라인전 미니언 5명이 같은 적 미니언 T 공격
- worker 1: `tgtHp.fCurrent -= 10` (read 100, write 90)
- worker 2~5: 동시 read 100, write 90 — **double-decrement 손실 4회**
- 결과: HP 가 50 (정확히) 빠져야 하는데 10만 빠짐 → 미니언 안 죽음

### 2.3 즉시 봉쇄 (Phase B-10c-2 M0, 5분)

[Scene_InGame.cpp:296](Client/Private/Scene/Scene_InGame.cpp:296) `pAI->Set_JobSystem(pJS);` 임시 주석 처리:

```cpp
{
    auto pAI = CMinionAISystem::Create();
    // pAI->Set_JobSystem(pJS);  // ★ B-10c-2 임시 비활성 — DamageEvent reduce 도입까지
    m_pScheduler->RegisterSystem(std::move(pAI));
}
```

→ `vecMinions.size() < kParallelThreshold || m_pJobSystem == nullptr` 조건이 단일 스레드 fallback 으로 전환. crash 회피.

### 2.4 정식 해결 — Decision/Apply 2-pass (Phase B-10c-2, 1.5일)

★ **Codex 2차 정정**: DamageEvent 만으로는 FindClosest read race 가 닫히지 않는다. ProcessMinion 이 self entity 의 `MinionStateComponent.current`, `attackTargetId`, `attackCooldown`, `NavAgentComponent`, `VelocityComponent` 를 worker 에서 직접 write 하므로, sibling worker 의 FindClosestEnemy 입장에서는 **자기 entity write 도 cross-entity read race**.

**정식 해결 = MinionAI 전체를 2-pass 로 분리**:

```
Pass 1 (worker thread, parallel):
  - World 를 read-only 로 순회 (FindClosestEnemy + 거리 판정 + HP read)
  - 결과는 MinionDecision 구조체에 push (per-worker buffer)
  - World 의 어떤 component 도 write 하지 않음

Pass 2 (main thread, single-thread reduce):
  - 모든 worker buffer 의 MinionDecision 일괄 적용
  - self state/target/cooldown/NavAgent/Velocity write
  - HP write + Dead 처리 + MinionStateComponent::Dead 연쇄
```

→ **같은 프레임 안에서 read 와 write 가 시간상 분리** → race 0.

#### M1. MinionDecision 정의 ([Engine/Public/ECS/Events/MinionDecision.h](Engine/Public/ECS/Events/MinionDecision.h) 신규)

```cpp
#pragma once
#include "WintersAPI.h"
#include "ECS/Entity.h"
#include "WintersTypes.h"
#include "WintersMath.h"

struct MinionDecision
{
    EntityID  self;            // 결정 주체
    EntityID  attackTarget;    // NULL_ENTITY = 타겟 없음
    Vec3      navTarget;       // 이동 목표 (desiredState == Chase 또는 LaneMove)

    // ★ Codex 3차 정정 — desiredState 분리 (cooldown 안 돼도 사거리 hold 가능)
    MinionStateComponent::State desiredState;   // Idle / LaneMove / Chase / Attack / Dead
    bool      stopMovement;    // true = NavAgent.bHasGoal=false + Velocity zero (사거리 안 hold)
    bool      emitDamage;      // true = DamageEvent 생성 + cooldown 리셋 (cooldown ready 시에만)
    f32_t     damage;          // emitDamage 시 적용량
};

// 1차 — 단순 DamageEvent (CommandBuffer 등 다른 시스템이 재사용)
struct DamageEvent
{
    EntityID  source;
    EntityID  target;
    f32_t     amount;
    bool      bKill;
};
```

#### M2. CJobSystem worker slot API 노출 (★ Codex 2차 정정 — main=0, worker=idx+1)

★ **slot 규칙 핵심**:
- `Get_WorkerIdx()`: main = -1, worker = [0, N) (기존)
- `Get_WorkerSlot()`: **main = 0, worker = idx+1** (신규)
- per-worker buffer 크기 = `Get_WorkerCount() + 1` (slot 0 = main, slot 1~N = worker)

**왜 main slot 분리가 필수인가** (Codex 2차):
- [JobSystem.cpp:221-231](Engine/Private/Core/JobSystem.cpp:221) `WaitForCounter` 가 main thread 에서도 Global drain → MinionAI job 실행 가능
- main 의 `t_iWorkerIdx == -1` 인데, `(idx >= 0) ? idx : 0` 식으로 처리하면 main 과 worker 0 가 같은 slot 0 에 동시 push → race 폭발

```cpp
// JobSystem.h
class CJobSystem
{
public:
    static int32_t  Get_WorkerIdx();          // main = -1, worker = [0, N)
    static uint32_t Get_WorkerSlot();         // ★ main = 0, worker = idx+1
    uint32_t        Get_WorkerCount() const   // ★ buffer size 결정용
        { return static_cast<uint32_t>(m_vecDeques.size()); }
};

// JobSystem.cpp
int32_t  CJobSystem::Get_WorkerIdx()  { return t_iWorkerIdx; }
uint32_t CJobSystem::Get_WorkerSlot()
{
    const int32_t idx = t_iWorkerIdx;
    return (idx < 0) ? 0u : static_cast<uint32_t>(idx + 1);
}
```

→ **DamageEvent / CommandBuffer / 미래 EventBus 모두 동일 slot 규칙 사용**.

#### M3a. MinionStateComponent enum 5상태 확장 (★ Codex 3차)

[Engine/Public/ECS/Components/GameplayComponents.h:163](Engine/Public/ECS/Components/GameplayComponents.h:163) 정정:

```cpp
// before
enum State : uint8_t { Idle, Moving, Attack, Dead };

// after — Chase 추가 (waypoint 와 nav chase 분리)
enum State : uint8_t {
    Idle,         // 타겟 없음, lane 진입 대기
    LaneMove,     // lane waypoint 따라 이동 (Minion_Manager 가 처리)
    Chase,        // ★ 신규 — 적 추적 (NavigationSystem 이 NavAgent 로 처리)
    Attack,       // 사거리 안 — 정지 + 공격 모션 (cooldown 무관)
    Dead
};
```

**Minion_Manager waypoint 정정** ([Minion_Manager.cpp:98](Client/Private/Manager/Minion_Manager.cpp:98)):
```cpp
// before — Attack 만 스킵
if (ms.current == MinionStateComponent::Attack) return;

// after — Attack/Chase/Dead 스킵 (Chase 는 NavigationSystem 이 NavAgent 로 처리)
if (ms.current == MinionStateComponent::Attack ||
    ms.current == MinionStateComponent::Chase ||
    ms.current == MinionStateComponent::Dead)
    return;
```

→ Chase 상태 미니언은 lane waypoint 가 무시 → NavAgent 가 enemy 위치로 이동 가능.

#### M3b. CMinionAISystem 멤버 — per-slot buffer (★ ISystem.Initialize 우회)

★ **Codex 3차 정정**: 현재 [ISystem.h:6-12](Engine/Public/ECS/ISystem.h:6) 에 `Initialize()` 메서드 없음. v2 의 `Initialize() override` 는 컴파일 실패. 가벼운 우회 = `Set_JobSystem()` 에서 resize.

```cpp
class CMinionAISystem : public ISystem
{
private:
    std::vector<std::vector<MinionDecision>> m_vecDecisionsPerSlot;
    std::vector<std::vector<DamageEvent>>    m_vecDamagesPerSlot;
    CJobSystem* m_pJobSystem = nullptr;

public:
    // ★ ISystem 시그니처 그대로. Initialize override 추가 X
    uint32_t    GetPhase() const override { return 2; }
    void        Execute(CWorld& world, f32_t fTimeDelta) override;
    const char* GetName() const override  { return "MinionAI"; }

    // ★ Set_JobSystem 시점에 buffer resize (worker count 확정 시점)
    void Set_JobSystem(CJobSystem* pJS)
    {
        m_pJobSystem = pJS;
        const uint32_t numWorkers = pJS ? pJS->Get_WorkerCount() : 0u;
        m_vecDecisionsPerSlot.resize(numWorkers + 1);   // ★ +1 = main slot 0
        m_vecDamagesPerSlot.resize(numWorkers + 1);
    }

private:
    void DecisionPass(CWorld& world, EntityID id, f32_t dt);   // worker, read-only
    void ApplyPass(CWorld& world, f32_t dt);                   // main, all writes
    void Push_Decision(const MinionDecision& dec);
};
```

**대안 (worker count 가 런타임 변동 시)**: Execute 시작부 lazy resize:
```cpp
void CMinionAISystem::Execute(CWorld& world, f32_t dt)
{
    const uint32_t need = (m_pJobSystem ? m_pJobSystem->Get_WorkerCount() : 0u) + 1;
    if (m_vecDecisionsPerSlot.size() != need)
    {
        m_vecDecisionsPerSlot.resize(need);
        m_vecDamagesPerSlot.resize(need);
    }
    // ... 이하 동일
}
```

→ 1차 권장 = Set_JobSystem 시점 resize (단순). 런타임 worker pool 변동 도입 시 lazy resize 로 교체.

#### M4. DecisionPass — worker, read-only on World

★ **자기 entity 의 어떤 component 도 write 하지 않음**. FindClosestEnemy / 거리 판정 / cooldown 차감 결과 → MinionDecision 으로 push.

```cpp
void CMinionAISystem::DecisionPass(CWorld& world, EntityID id, f32_t dt)
{
    if (!world.HasComponent<MinionStateComponent>(id)) return;
    if (!world.HasComponent<MinionComponent>(id))      return;
    if (!world.HasComponent<TransformComponent>(id))   return;

    // ★ const 참조 — write 없음
    const auto& ms     = world.GetComponent<MinionStateComponent>(id);
    const auto& minion = world.GetComponent<MinionComponent>(id);
    const auto& xform  = world.GetComponent<TransformComponent>(id);

    if (ms.current == MinionStateComponent::Dead) return;

    MinionDecision dec{};
    dec.self = id;

    // 쿨다운은 ApplyPass 가 차감 — Decision 은 "쿨다운 만료 시 attack" 만 결정
    const bool bCooldownReady = (ms.attackCooldown <= 0.f);

    // 1) 현재 타겟 유효성 (read-only)
    EntityID curTarget = ms.attackTargetId;
    if (curTarget != NULL_ENTITY)
    {
        const bool bValid = world.IsAlive(curTarget)
            && world.HasComponent<HealthComponent>(curTarget)
            && world.HasComponent<TransformComponent>(curTarget);
        if (!bValid) curTarget = NULL_ENTITY;
        else
        {
            const auto& hp = world.GetComponent<HealthComponent>(curTarget);
            if (hp.bIsDead || hp.fCurrent <= 0.f) curTarget = NULL_ENTITY;
        }
    }

    const Vec3 myPos = xform.GetWorldPosition();

    // 2) 타겟 없으면 탐색 (read-only iteration)
    if (curTarget == NULL_ENTITY)
    {
        curTarget = FindClosestEnemy(world, id, myPos,
            static_cast<uint8_t>(minion.team), ms.sightRange);
    }

    if (curTarget == NULL_ENTITY)
    {
        dec.attackTarget = NULL_ENTITY;
        // 타겟 없음 → 기존이 Attack/Chase 였다면 Idle 로 (lane 진입은 Minion_Manager 가 처리)
        dec.desiredState = MinionStateComponent::Idle;
        dec.stopMovement = false;     // lane waypoint 가 다시 가져가도록
        dec.emitDamage   = false;
        Push_Decision(dec);
        return;
    }

    dec.attackTarget = curTarget;

    // 3) 거리 판정 (read-only)
    const auto& tgtXform = world.GetComponent<TransformComponent>(curTarget);
    const Vec3 tgtPos = tgtXform.GetWorldPosition();
    const f32_t dx = tgtPos.x - myPos.x;
    const f32_t dz = tgtPos.z - myPos.z;
    const f32_t distSq = dx * dx + dz * dz;
    const f32_t rangeSq = ms.attackRange * ms.attackRange;
    const f32_t sightSq = ms.sightRange  * ms.sightRange;

    if (distSq <= rangeSq)
    {
        // ★ Codex 3차 정정 — 사거리 안 = 항상 Attack 상태 + 정지 (cooldown 무관)
        //   damage 만 cooldown ready 시에만 발생 → 사거리 hold 가능
        dec.desiredState = MinionStateComponent::Attack;
        dec.stopMovement = true;
        dec.emitDamage   = bCooldownReady;
        dec.damage       = ms.attackDamage;
    }
    else if (distSq <= sightSq)
    {
        // ★ Codex 3차 정정 — Chase 상태 (Moving 이 아닌)
        //   Chase = NavAgent 로 적 추적 (Minion_Manager waypoint 가 무시함)
        dec.desiredState = MinionStateComponent::Chase;
        dec.navTarget    = tgtPos;
        dec.stopMovement = false;
        dec.emitDamage   = false;
    }
    else
    {
        // 시야 밖 — 타겟 포기, lane 진입
        dec.attackTarget = NULL_ENTITY;
        dec.desiredState = MinionStateComponent::Idle;
        dec.stopMovement = false;
        dec.emitDamage   = false;
    }

    Push_Decision(dec);
}

void CMinionAISystem::Push_Decision(const MinionDecision& dec)
{
    const uint32_t slot = CJobSystem::Get_WorkerSlot();   // ★ main=0, worker=idx+1
    m_vecDecisionsPerSlot[slot].push_back(dec);
}
```

#### M5. ApplyPass — main thread, all writes (single-thread reduce)

```cpp
void CMinionAISystem::ApplyPass(CWorld& world, f32_t dt)
{
    // 1) Cooldown 차감 (모든 살아있는 미니언) — 메인 스레드에서 single-thread 로
    world.ForEach<MinionStateComponent>(
        function<void(EntityID, MinionStateComponent&)>(
            [dt](EntityID, MinionStateComponent& ms)
            {
                if (ms.attackCooldown > 0.f) ms.attackCooldown -= dt;
            }));

    // 2) Decision 적용 — self entity write (★ v3 — desiredState 기반)
    for (auto& vecBuf : m_vecDecisionsPerSlot)
    {
        for (const auto& dec : vecBuf)
        {
            if (!world.IsAlive(dec.self)) continue;
            if (!world.HasComponent<MinionStateComponent>(dec.self)) continue;

            auto& ms = world.GetComponent<MinionStateComponent>(dec.self);
            ms.attackTargetId = dec.attackTarget;
            ms.current        = dec.desiredState;   // ★ Idle/LaneMove/Chase/Attack 직접 set

            // ★ stopMovement — Attack 상태 (cooldown 무관) 시 정지
            if (dec.stopMovement)
            {
                if (world.HasComponent<NavAgentComponent>(dec.self))
                    world.GetComponent<NavAgentComponent>(dec.self).bHasGoal = false;

                if (world.HasComponent<VelocityComponent>(dec.self))
                {
                    auto& v = world.GetComponent<VelocityComponent>(dec.self);
                    v.vDirection = { 0.f, 0.f, 0.f };
                    v.fSpeed = 0.f;
                }
            }

            // ★ Chase 상태 — NavAgent 로 enemy 추적
            if (dec.desiredState == MinionStateComponent::Chase)
            {
                if (world.HasComponent<NavAgentComponent>(dec.self))
                {
                    auto& nav = world.GetComponent<NavAgentComponent>(dec.self);
                    nav.vTarget    = dec.navTarget;
                    nav.bHasGoal   = true;
                    nav.bPathDirty = true;
                }
            }

            // ★ emitDamage — cooldown ready 시에만 damage event + cooldown 리셋
            if (dec.emitDamage)
            {
                ms.attackCooldown = ms.attackCooldownMax;
                m_vecDamagesPerSlot[0].push_back(DamageEvent{
                    dec.self, dec.attackTarget, dec.damage, true });
            }
        }
        vecBuf.clear();
    }

    // 3) Damage 적용 — target HP/state write
    for (auto& vecBuf : m_vecDamagesPerSlot)
    {
        for (const auto& evt : vecBuf)
        {
            if (!world.IsAlive(evt.target)) continue;
            if (!world.HasComponent<HealthComponent>(evt.target)) continue;

            auto& hp = world.GetComponent<HealthComponent>(evt.target);
            hp.fCurrent -= evt.amount;
            if (hp.fCurrent <= 0.f && evt.bKill)
            {
                hp.fCurrent = 0.f;
                hp.bIsDead = true;
                if (world.HasComponent<MinionStateComponent>(evt.target))
                    world.GetComponent<MinionStateComponent>(evt.target).current
                        = MinionStateComponent::Dead;
            }
        }
        vecBuf.clear();
    }
}
```

#### M6. Execute — DecisionPass 병렬 + ApplyPass 단일

```cpp
void CMinionAISystem::Execute(CWorld& world, f32_t dt)
{
    WINTERS_PROFILE_SCOPE("MinionAI::Execute");

    // 1) ForEach 로 살아있는 미니언 수집 (메인 스레드)
    std::vector<EntityID> vecMinions;
    world.ForEach<MinionStateComponent, MinionComponent, TransformComponent>(
        function<void(EntityID, MinionStateComponent&, MinionComponent&, TransformComponent&)>(
            [&](EntityID id, MinionStateComponent& ms, MinionComponent&, TransformComponent&)
            {
                if (ms.current != MinionStateComponent::Dead)
                    vecMinions.push_back(id);
            }));

    // 2) DecisionPass — worker 병렬 (read-only on world)
    if (vecMinions.size() < kParallelThreshold || m_pJobSystem == nullptr)
    {
        WINTERS_PROFILE_SCOPE("MinionAI::DecisionPass");
        for (EntityID id : vecMinions)
            DecisionPass(world, id, dt);
    }
    else
    {
        WINTERS_PROFILE_SCOPE("MinionAI::DecisionPassParallel");
        CJobCounter counter;
        CWorld* pWorld = &world;
        CMinionAISystem* pThis = this;
        for (EntityID id : vecMinions)
        {
            m_pJobSystem->Submit(
                [pThis, pWorld, id, dt]() { pThis->DecisionPass(*pWorld, id, dt); },
                &counter);
        }
        m_pJobSystem->WaitForCounter(&counter);
    }

    // 3) ApplyPass — 메인 스레드 단일 (모든 write)
    {
        WINTERS_PROFILE_SCOPE("MinionAI::ApplyPass");
        ApplyPass(world, dt);
    }
}
```

### 2.5 왜 Decision/Apply 2-pass 가 정식 해결인가 (Codex 2차 핵심)

★ **DamageEvent 단독 (v1) 으로는 부족했던 이유**:

ProcessMinion 이 v1 에서 자기 entity 의 다음 component 를 write 했음:
- `ms.current = Attack/Moving/Idle/Dead`
- `ms.attackTargetId = ...`
- `ms.attackCooldown = ...`
- `nav.vTarget / bHasGoal / bPathDirty`
- `vel.vDirection / fSpeed`

이 self write 가 **sibling worker 의 FindClosestEnemy 입장에서는 cross-entity read race**. v1 의 `DamageEvent reduce` 는 HP write 만 막고 위 self state write 는 그대로 둠 → race 미해결.

★ **v2 의 Decision/Apply 2-pass 가 같이 닫는 것**:
- DecisionPass (worker): **0 write** — 모든 component read-only
- ApplyPass (main): all writes single-thread
- 한 프레임 안에서 read phase 와 write phase 가 시간상 분리 → 모든 race 0

### 2.6 검증 마일스톤 (v2 — Decision/Apply)

```
M0   pAI->Set_JobSystem 임시 비활성 (5분) — crash 봉쇄 (이미 권장)
M1   MinionDecision.h 정의 + DamageEvent.h 정의 (10분)
M2   CJobSystem::Get_WorkerIdx + Get_WorkerSlot + Get_WorkerCount 정적 메서드 (15분)
M3   CMinionAISystem 멤버 m_vecDecisionsPerSlot + m_vecDamagesPerSlot + Initialize (15분)
M4   DecisionPass — read-only 로직 분리 (1.5시간)
M5   ApplyPass — Cooldown + Decision + Damage 3 단계 (1시간)
M6   Execute — DecisionPass 병렬 + ApplyPass 단일 호출 (15분)
M7   pAI->Set_JobSystem 재활성 (1분)
M8   F5 — 미니언 16+ 라인전 단일 vs 병렬 결과 동일 (deterministic)
M9   Stress — 16+ 미니언이 1 타겟 집중 공격 시 데미지 손실 0
M10  Stress — main thread 가 WaitForCounter 중 DecisionPass job 실행 시 slot 0 충돌 0
```

---

## 3. P2 — CommandBuffer thread-local

### 3.1 현재 코드 ([CommandBuffer.cpp:5-17](Engine/Private/ECS/CommandBuffer.cpp:5))

```cpp
void CCommandBuffer::DeferCreate(std::function<void(CWorld&, EntityID)> initFn)
{
    m_vecCreates.push_back(move(initFn));        // ★ vector race 위험
}

void CCommandBuffer::DeferDestroy(EntityID entity)
{
    m_vecDestroys.push_back(entity);             // ★ 동일
}

void CCommandBuffer::DeferCommand(std::function<void(CWorld&)> cmd)
{
    m_vecCommands.push_back(move(cmd));          // ★ 동일
}
```

### 3.2 현재는 미사용이지만 잠재 위험

- 현재 worker thread 에서 호출되는 ECS 시스템 (NavigationSystem/MinionAISystem) 은 CommandBuffer 사용 안 함
- 단 미래에 어떤 system 이 worker 에서 `world.GetCommandBuffer().DeferDestroy(id)` 호출하면 즉시 race 폭발
- **CPUProfiler m_vStack 사고와 똑같은 패턴**

### 3.3 해결 — per-worker buffer + main flush (Phase B-10c-2 M9~M11, 0.5일)

**패턴 B (MinionAI 와 동일)**:

```cpp
class CCommandBuffer
{
private:
    // ★ B-10c-2 — worker 별 격리 buffer (push 시 lock 없음)
    std::vector<std::vector<std::function<void(CWorld&, EntityID)>>>  m_vecCreatesPerWorker;
    std::vector<std::vector<EntityID>>                                m_vecDestroysPerWorker;
    std::vector<std::vector<std::function<void(CWorld&)>>>            m_vecCommandsPerWorker;

public:
    void Resize_Workers(uint32_t numWorkers)
    {
        // ★ +1 = main slot 0 (통일). 호출 측은 CJobSystem::Get_WorkerCount() 전달
        m_vecCreatesPerWorker.resize(numWorkers + 1);
        m_vecDestroysPerWorker.resize(numWorkers + 1);
        m_vecCommandsPerWorker.resize(numWorkers + 1);
    }

    void DeferCreate(std::function<void(CWorld&, EntityID)> initFn)
    {
        const u32_t slot = CJobSystem::Get_WorkerSlot();   // ★ main=0, worker=idx+1 (통일 규칙)
        m_vecCreatesPerWorker[slot].push_back(std::move(initFn));
    }

    // DeferDestroy / DeferCommand 동일 패턴

    // Flush — 메인 스레드 (Scheduler 모든 phase 끝난 후)
    void Flush(CWorld& world);
};

void CCommandBuffer::Flush(CWorld& world)
{
    // Creates → Commands → Destroys 순서
    for (auto& vec : m_vecCreatesPerWorker)
    {
        for (auto& fn : vec) {
            EntityID e = world.CreateEntity();
            fn(world, e);
        }
        vec.clear();
    }
    for (auto& vec : m_vecCommandsPerWorker)
    {
        for (auto& cmd : vec) cmd(world);
        vec.clear();
    }
    for (auto& vec : m_vecDestroysPerWorker)
    {
        for (EntityID e : vec) world.DestroyEntity(e);
        vec.clear();
    }
}
```

### 3.4 마일스톤

```
M9   per-worker buffer 멤버 + Resize_Workers (1시간)
M10  Defer* 메서드 → CJobSystem::Get_WorkerIdx() 경유 push (1시간)
M11  Flush 메서드 + Scene OnUpdate 끝에 호출 (1시간)
M12  단위 테스트 — 16 worker 가 동시 DeferDestroy 100회 → 1600개 정상 처리
```

---

## 4. P2 — Scheduler component access contract

### 4.1 현재 위험 ([SystemScheduler.cpp:22-43](Engine/Private/ECS/SystemScheduler.cpp:22))

```cpp
for (auto& [phase, systems] : m_mapPhases)
{
    if (systems.size() == 1 || m_pJobSystem == nullptr) {
        // 단일 스레드 — 안전
    }
    else {
        // ★ 같은 phase 의 모든 system 을 worker 에 동시 Submit
        // 두 system 이 같은 component 를 read/write 하면 race
        for (auto& sys : systems) {
            m_pJobSystem->Submit([&sys, &world, fTimeDelta](){
                sys->Execute(world, fTimeDelta);
            }, &counter);
        }
    }
}
```

### 4.2 시나리오

- Phase 2 에 NavigationSystem + MinionAISystem 등록
- 둘 다 `MinionStateComponent` write
- 같은 phase 라 병렬 Submit → worker 1 이 NavigationSystem 의 ProcessAgent 안 stun guard 에서 read 중, worker 2 가 MinionAISystem 의 ProcessMinion 안 state write → race

### 4.3 해결 — ISystem 에 access contract 메타데이터 (Phase B-10c-2 M13~M15, 1일)

★ **Codex 3차 정정**: `SystemAccess` / `TypeID` / `Get_AccessContract` 모두 현재 ECS API 에 없는 신규 타입. 정의 위치 + TypeID 의 실체 명시 필요.

**TypeID 채택 — `std::type_index`**:
- 표준 `<typeindex>` 헤더 — RTTI 기반 컴파일 시간 type identity
- 자체 component type id 시스템 도입은 별도 작업 — 1차는 `std::type_index` 로 시작
- 향후 ECS 가 자체 component registry 도입 시 typedef 만 교체

```cpp
// Engine/Public/ECS/ISystem.h — 정정
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <typeindex>      // ★ 신규
#include <vector>

// ★ B-10c-2 신규 — Component access contract
struct SystemAccess
{
    bool                              bExclusive = true;  // 기본 = 단독 실행 (안전)
    std::vector<std::type_index>      reads;              // read-only component types
    std::vector<std::type_index>      writes;             // write component types
};

class ISystem
{
public:
    virtual ~ISystem() = default;
    virtual uint32_t    GetPhase() const = 0;
    virtual void        Execute(CWorld& world, float fTimeDelta) = 0;
    virtual const char* GetName() const = 0;

    // ★ B-10c-2 신규 — 기본값 = 단독 실행 (미구현 시 보수적)
    virtual void Get_AccessContract(SystemAccess& out) const
    {
        out.bExclusive = true;
        out.reads.clear();
        out.writes.clear();
    }
};

// ★ 시스템 측 사용 예 (TransformSystem)
void CTransformSystem::Get_AccessContract(SystemAccess& out) const override
{
    out.bExclusive = false;
    out.writes.push_back(std::type_index(typeid(TransformComponent)));
    // reads = TransformComponent 만 (자기 component) — bExclusive false 로 다른 시스템과 병렬 가능
}
```

**왜 std::type_index 로 시작하나**:
- 표준 — 추가 의존성 0
- ECS Component 가 POD struct 라 RTTI 활성화 가정 (현재 Winters 가 그렇게 빌드 중)
- 비교/해시 모두 표준 지원 — `unordered_set<type_index>` 로 충돌 검사 가능
- 향후 ECS registry 도입 시 `using TypeID = std::type_index;` 한 줄 정의 → `using TypeID = ComponentRegistry::Id;` 로 교체

★ **왜 기본값이 bExclusive 인가** (Codex 2차):
- 빈 reads/writes 를 "접근 없음" 으로 해석하면 **미등록 시스템들이 무차별 병렬화**
- ECS 추가 시 contract 박제를 잊으면 즉시 race
- "잊으면 직렬화 (안전)" vs "잊으면 병렬화 (위험)" — 안전한 default 선택
- contract 박제 후 명시적으로 `bExclusive = false` 로 설정 + reads/writes 채워야 병렬화 후보

**Scheduler 변경**:

```cpp
void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    for (auto& [phase, systems] : m_mapPhases)
    {
        if (systems.size() == 1 || m_pJobSystem == nullptr) {
            for (auto& sys : systems) sys->Execute(world, fTimeDelta);
            continue;
        }

        // ★ Component access conflict 분석
        std::vector<std::vector<ISystem*>> vecGroups = Group_NonConflicting(systems);

        for (auto& group : vecGroups)
        {
            if (group.size() == 1) {
                group[0]->Execute(world, fTimeDelta);
            }
            else {
                CJobCounter counter;
                for (auto* sys : group) {
                    m_pJobSystem->Submit(
                        [sys, &world, fTimeDelta]() { sys->Execute(world, fTimeDelta); },
                        &counter);
                }
                m_pJobSystem->WaitForCounter(&counter);
            }
        }
    }
}

// 같은 phase 에서 component 충돌 없는 그룹으로 분할
std::vector<std::vector<ISystem*>> Group_NonConflicting(...)
{
    // 단순: write 끼리 또는 write/read 끼리 충돌 → 별 그룹
    // read-only 끼리는 같은 그룹 가능
    // 1차는 보수적 — 모든 system 을 별 그룹 (= 직렬화). 2차에 정교화
}
```

### 4.4 마일스톤

```
M13  ISystem 에 Get_AccessContract 추가 + 기존 시스템 4개 (Transform/Status/Nav/MinionAI) 에 contract 박제
M14  Scheduler 의 Group_NonConflicting 구현 (1차 = 모든 system 직렬화 — 보수적)
M15  F5 회귀 0 — 같은 phase 에 Nav + MinionAI 등록해도 안전 동작
M16  (옵션) Group_NonConflicting 정교화 — read-only 끼리 병렬 허용
```

**1차 보수적 처방**: 같은 phase = 직렬화. 병렬 이득 ↓ 단 race 0. 정교화는 Phase 5-C 또는 Fiber 통합 시.

---

## 5. P3 — AStar Counter mutex contention

### 5.1 현재 코드 ([Pathfinder.cpp:68](Engine/Private/Manager/Navigation/Pathfinder.cpp:68))

```cpp
while (!open.empty())
{
    // ... A* 노드 처리 ...
    WINTERS_PROFILE_COUNT("AStar::NodesVisited", 1);   // ★ 매 노드마다 mutex lock
    // ...
}
```

### 5.2 문제

- A* 가 100~1000 노드 방문 시 매 노드마다 mutex lock
- 16 worker 가 동시 pathfinding 시 contention 폭발
- crash 는 안 나지만 성능 저하

### 5.3 해결 — local 누적 + scope guard (10분, ★ Codex 2차 정정)

★ **단순 함수 끝 1회 flush 는 부족**: [Pathfinder.cpp](Engine/Private/Manager/Navigation/Pathfinder.cpp) 에 **3개 early return 경로** 가 있음:
- L96 `return emptyPath;` (도달 불가, A* 루프 끝난 후)
- L106 `if (iP < 0) { return emptyPath; }` (경로 재구성 실패)
- L110 `return path;` (정상 종료)

각 return 전마다 flush 안 하면 실패 경로 counter 누락.

**해결 = scope guard 패턴 (RAII)**:

```cpp
std::vector<CNavGrid::Cell> CPathfinder::Find_Path(...)
{
    WINTERS_PROFILE_SCOPE("AStar::FindPath");

    // 함수 시작 전 early return 들 — A* 시작 안 했으니 counter 0 (flush 불필요)
    std::vector<CNavGrid::Cell> emptyPath;
    if (!pGrid) return emptyPath;
    if (!pGrid->IsWalkable(start.x, start.y)) return emptyPath;
    if (!pGrid->IsWalkable(goal.x, goal.y))  return emptyPath;

    // ★ A* 시작 시점에서 scope guard 시동 — return 어디서든 자동 flush
    uint32_t nodesVisited = 0;
    struct CounterFlush
    {
        const uint32_t& count;
        ~CounterFlush()
        {
            WINTERS_PROFILE_COUNT("AStar::NodesVisited",
                static_cast<i32_t>(count));
        }
    } guard{ nodesVisited };

    // ... thread_local 버퍼 초기화 + A* 루프 ...
    while (!open.empty())
    {
        // ... 노드 처리 ...
        ++nodesVisited;             // ★ race 없음, lock 없음
    }

    // 모든 return 경로 (L96 / L106 / L110) — guard ~CounterFlush() 가 자동 flush
    if (...) return emptyPath;      // L96 — 도달 불가
    if (iP < 0) return emptyPath;   // L106 — 경로 재구성 실패
    return path;                    // L110 — 정상
}
```

**효과**:
- mutex lock 횟수: 노드 수 → 1회 (16 worker × 1000 노드 = 16,000 lock → 16 lock)
- 모든 return 경로에서 counter 누락 0 (실패 경로도 정확히 기록)
- C++ RAII — 추가 코드 최소

---

## 6. 통합 검증 — F5 회귀 + Stress

### 6.1 회귀 (M16 후 종합 F5)

| 항목 | 합격 |
|---|---|
| 기존 7 챔프 (Irelia~Zed) | 모든 스킬/이동/dash 동작 동일 |
| 미니언 라인전 | 같은 타겟 집중 공격 시 HP 정확히 차감 |
| Pathfinding | 16+ 미니언 동시 repath 시 crash 0 |
| Profiler 패널 | scope 시간 + counter 정확히 표시 |
| 단일 스레드 vs 병렬 결과 | 동일 (deterministic) |

### 6.2 Stress 테스트

- 16 미니언이 1 타겟 집중 공격 → 데미지 손실 0 (M8)
- 16 worker 가 동시 DeferDestroy 100회 → 1600 entity 정상 처리 (M12)
- Phase 에 Nav + MinionAI 등록 + 1000 frame 반복 → race 0 (M15)

### 6.3 동시 검증 — B-10c 잔여 (worker slot API + stress)

★ **Codex 3차 정정**: B-10c **본체는 이미 완료** (EnqueueJob/GlobalQueue/WaitForCounter main drain 검증). 잔여 = `Get_WorkerIdx/Get_WorkerSlot/Get_WorkerCount` 노출 + Submit 1만회 stress 검증. 본 사이클 (B-10c-2) M2 와 자연 흡수.

---

## 7. CLAUDE.md gotchas + MEMORY 박제

### 7.1 CLAUDE.md gotchas 신규 5건

```
- ★ 병렬 ECS 시스템 worker-safety 정책 5종:
  (1) thread_local — 작업 버퍼/scope stack
  (2) atomic — 단순 카운터/flag
  (3) lock + buffer + main merge — 결과 수집
  (4) self-entity only — ECS Component write
  (5) per-worker buffer + main flush — cross-entity write
  Set_JobSystem 활성 직전 위 5종 중 하나에 해당하는지 검증 의무.

- CPUProfiler scope stack thread_local 분리 (2026-04-28 박제):
  단일 vector stack 을 thread 가 공유하면 push_back 중 vector 재할당 crash.
  thread_local stack + merge mutex 패턴 강제.

- ECS cross-entity write 금지 (2026-04-28 박제):
  worker thread 에서 다른 entity 의 Component 직접 write 금지.
  per-worker DamageEvent buffer + 메인 reduce phase 패턴 강제.

- CommandBuffer 는 per-worker buffer:
  DeferCreate/DeferDestroy/DeferCommand 모두 worker 별 vector 에 push.
  Flush 는 메인 스레드 (Scheduler 모든 phase 끝난 후).

- Scheduler component access contract:
  같은 phase 의 system 다수가 같은 component 를 mutate 하면 race.
  ISystem::Get_AccessContract() 로 reads/writes 선언 → Scheduler 가 충돌 시 직렬화.
```

### 7.2 MEMORY.md 신규 항목

- `project_phase_b10c_2_worker_safety.md` — 본 사이클 결과 박제

---

## 8. 06 v4 와의 통합

### 8.1 06 v4 §0 표 갱신 권장

**기존**:
```
| B-10c | JobSystem race + system inject 복구 | 1일 |
```

**갱신 후**:
```
| B-10c (잔여)    | worker slot API 노출 (Get_WorkerIdx/Slot/Count) + stress 검증 — 본체는 이미 완료, B-10c-2 M2 와 흡수 | 0.5일 |
| B-10c-2  | Worker-Safety 통합 패키지 (Codex 5건 + Profiler 학습 박제) | 2.5일 |
```

### 8.2 06 v4 §2 부록 — B-10c-2 신규 섹션

본 계획서 §1.2 + §2 + §3 + §4 + §5 의 핵심 매트릭스를 06 v4 §2 끝에 1페이지 요약으로 박제. 풀 본문은 본 계획서 참조.

---

## 9. 일정 + 의존성

### 9.1 작업 견적

| 단계 | 작업 | 시간 |
|---|---|---|
| **즉시** | M0 — pAI->Set_JobSystem 비활성 (crash 봉쇄) | 5분 |
| 즉시 | P3 — AStar scope guard + local 누적 | 10분 |
| **B-10c-2 본격** | M1~M10 — Decision/Apply 2-pass + 5상태 + desiredState/stopMovement/emitDamage 분리 + Minion_Manager waypoint 정정 (P1, ★ v3 정식) | **2일** (v2 1.5일 → v3 2일) |
| | CommandBuffer per-slot (P2, slot 규칙 통일) | 0.5일 |
| | Scheduler access contract (P2, std::type_index + bExclusive 기본) | 1일 |
| | 통합 회귀/Stress 검증 (3 stress 시나리오 + 사거리 hold + chase override 검증) | 0.5일 |
| **합계** | | **~4일 + 즉시 15분** (v2 ~3일 → v3 ~4일) |

### 9.2 의존성 그래프

```
[즉시 봉쇄]
  ├─ M0  pAI->Set_JobSystem 비활성 (crash 회피)
  └─ P3  AStar local 누적 (성능)
       ↓
[B-10c-2 본격]
  ├─ CJobSystem::Get_WorkerIdx() 정적 메서드 (M2) ★ 다른 모든 작업의 전제조건
  ├─ M1~M8  DamageEvent reduce (P1) ★ M0 봉쇄 해제
  ├─ M9~M12 CommandBuffer thread-local (P2) ★ M2 의존
  └─ M13~M16 Scheduler access contract (P2) ★ 독립
       ↓
[통합 검증]
  ├─ F5 회귀 0 (7 챔프)
  ├─ Stress (16 미니언 집중 공격)
  └─ Stress (16 worker DeferDestroy)
       ↓
[06 v4 B-10c 잔여 — worker slot API 검증] (본 사이클 M2 와 자연 흡수)
       ↓
[06 v4 B-10d-pre / B-10d Riven] — 인프라 안정 후 진입
```

### 9.3 우선순위 결정 — 권장 진입 순서

```
1. 즉시 봉쇄 (15분)        — M0 비활성 + P3 local 누적
2. M1~M2 (30분)            — DamageEvent.h + Get_WorkerIdx 노출
3. M3~M8 (1일)             — DamageEvent reduce 본격
4. M9~M12 (0.5일)          — CommandBuffer thread-local
5. M13~M16 (1일)           — Scheduler access contract
6. 통합 검증 (0.5일)       — F5 + Stress
7. (B-10c 본체 ✅ 이미 완료 — worker slot API 는 본 사이클 M2 에서 흡수)
8. 06 v4 B-10d-pre 진입     — Player adapter (1.5일)
9. 06 v4 B-10d 진입         — Riven Pure ECS (2~3일)
```

---

## 10. 한 줄

**v3 (Codex 1차+2차+3차 통합, 15건): 즉시 봉쇄 (MinionAI Set_JobSystem 비활성 5분 + AStar scope guard 10분) → 정식 해결 (Decision/Apply 2-pass + desiredState 분리 + MinionStateComponent 5상태 + worker slot main=0/worker=idx+1 + CommandBuffer per-slot + Scheduler access contract bExclusive + std::type_index, ~3.5일) → B-10c 본체 ✅ 완료 (worker slot API 만 본 사이클 M2 에서 흡수) → B-10d-pre → Riven. 본 사이클 완료 = 병렬 ECS 인프라 production-ready, FindClosest race + cooldown hold + chase override 모두 같이 닫힘.**

---

## ★ 별도 후속 phase — Minion Combat Production 패키지 (본 사이클 외부)

Codex 3차 추가 코멘트 박제. 본 worker-safety 패키지 완료 후 진입할 후속 작업:

| # | 패키지 | 내용 |
|---|---|---|
| 1 | **MinionAI Decision/Apply** | 본 패키지 (B-10c-2) — state/target/nav/damage intent 결정 |
| 2 | **MinionAnimationState** | state 변경 시 run/idle/attack/death anim 전환. ModelRenderer.PlayAnimationByName 자동 |
| 3 | **Attack windup/hit frame** | castFrame/recoveryFrame 도입 (챔프 SkillTable 패턴 미러). damage 는 hit frame 에서만 발생 |
| 4 | **Ranged/Siege** | Caster minion projectile spawn (KalistaProjectileSystem 패턴 미러) + impact damage |
| 5 | **Death** | death anim → hide/destroy. CommandBuffer.DeferDestroy 활용 (B-10c-2 §3 의 per-slot pattern) |

**진입 조건**: 본 패키지 (B-10c-2) 통과 → 06 v4 B-10d-pre → Riven 완료 후. 별도 계획서 작성 예정 (`MINION_COMBAT_PACKAGE.md`).
