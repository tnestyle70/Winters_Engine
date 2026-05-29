# Minion Crowd Separation + Fan-out Spawn — 통합 박제

**작성일**: 2026-05-07
**전제**: B-13 v2 04 (Spatial Hash) 박제 완료 + Worker-Safety + MinionCombat (M0~M7) 풀 사이클 동작. **본 계획서 = MINION_CROWD_COLLISION_PLAN.md (2026-04-28 stub) 의 정식 박제 + 부채꼴 spawn 추가**.
**가이드**: [`.md/process/PLAN_AUTHORING_PITFALLS.md`](../../../process/PLAN_AUTHORING_PITFALLS.md)
**범위**:
1. **Stage 1**: `CMinionSeparationSystem` 신규 (Boids Separation Force, O(N²)) — 미니언 한 점 겹침 해소.
2. **Stage 2**: `CMinion_Manager::Spawn_Wave` 의 spawn 위치 산출을 **부채꼴 / 2×3 grid** 로 변경 — 동일 frame N 마리가 같은 좌표 spawn 사고 방지.
**합격 정의**: 같은 적 미니언 다굴 시 미니언이 부채꼴로 둘러쌈 (한 점 겹침 0) + spawn wave 시 N 마리가 일정 간격으로 부채꼴 분포.

---

## §0. 본 계획서가 잡는 결함 한 줄

**증상 1 (Crowd)**: NavigationSystem 이 path 따라 이동 → 같은 적 미니언/챔프 향해 N 마리가 같은 cell 로 수렴. 충돌 처리 0 → 시각적으로 한 점에 겹침. ([MINION_CROWD_COLLISION_PLAN.md §0](../../../plan/engine/MINION_CROWD_COLLISION_PLAN.md))

**증상 2 (Spawn)**: [Minion_Manager.cpp:495-499](../../../../Client/Private/Manager/Minion_Manager.cpp:495) 의 `DoSpawnWave` 가 `m_iCurrentRound = 0; m_fNextRoundCountdown = 0.f;` 만 — 실제 spawn 은 [L183](../../../../Client/Private/Manager/Minion_Manager.cpp:183) 의 `Spawn_Minion(type, team, way)` 에서 일어남. **spawn 위치 = `m_vecWaypoints[t][l][0]` (waypoint 0 단일 지점)** — 같은 frame 5 마리 spawn 시 모두 동일 좌표 → 즉시 중첩.

**핵심**:
- Stage 1 = 별도 시스템 (Phase 11) 으로 separation force 적용.
- Stage 2 = `Spawn_Minion` 의 spawn 위치 인자에 fan offset 추가.

---

## §1. Preflight Evidence Table — TODO 0

| # | 항목 | 실측값 | 출처 |
|---|---|---|---|
| 1 | 미니언 spawn 컴포넌트 | TransformComponent + MinionStateComponent + HealthComponent + VelocityComponent + TargetableTag + MinionComponent + SpatialAgentComponent + VisionSourceComponent + VisibilityComponent + NavAgentComponent + RenderComponent | [Minion_Manager.cpp:399-458](../../../../Client/Private/Manager/Minion_Manager.cpp:399) |
| 2 | DoSpawnWave 본체 | 단지 round count 초기화 — 실제 spawn 은 Tick 의 round-up 분기 | [Minion_Manager.cpp:495-499](../../../../Client/Private/Manager/Minion_Manager.cpp:495) |
| 3 | Spawn 트리거 | Tick 안의 `Spawn_Minion(type, team, way)` 호출 (L183) | [Minion_Manager.cpp:183](../../../../Client/Private/Manager/Minion_Manager.cpp:183) |
| 4 | Spawn 위치 | `Spawn_Minion` 안에서 `m_vecWaypoints[t][l][0]` (waypoint 0) — fan offset 0 | §2.A 본문 인용 |
| 5 | LoL 미니언 wave 구성 | 근접 3 + 원거리 3 + (3 wave 마다 공성 1) — 라인당 1 wave 5~7 마리 | [Minion_Manager.cpp:493](../../../../Client/Private/Manager/Minion_Manager.cpp:493) |
| 6 | NavAgentComponent 의 path 갱신 | NavigationSystem (phase 3) 이 `bPathDirty` 보고 path 재계산 | [NavigationSystem.cpp](../../../../Engine/Private/ECS/Systems/NavigationSystem.cpp) §4.0 grep |
| 7 | VelocityComponent 정의 | `vDirection (Vec3)` + `fSpeed (f32_t)` | [CoreComponents.h](../../../../Engine/Public/ECS/Components/CoreComponents.h) §4.0 grep |
| 8 | SystemScheduler Phase 모델 | uint32_t phase. 같은 phase = JobSystem 병렬 (단 SystemAccessConflict 면 직렬 분리) | [SystemScheduler.cpp:40-89](../../../../Engine/Private/ECS/SystemScheduler.cpp:40) |
| 9 | 기존 Phase 사용 현황 | 0=Transform, 1=SpatialHash, 2=MinionAI, 3=Navigation, 4=StatusEffect, 5=Vision, 6=TurretAI, 7=TurretProjectile, 8=BehaviorTree, 9=YoneSoulSpawn, 10=MCTS | [B-13 v2 §2 표](../../../plan/B13/00_INDEX_MASTER.md), 본 코드 GetPhase() 실측 |
| 10 | MinionAI 의 VelocityComponent 처리 | DecisionPass + ApplyPass 2-pass — `bSetNavTarget` flag 기반 NavAgent.vTarget 설정. VelocityComponent 직접 write X (NavSystem 이 NavAgent → VelocityComponent 변환) | [MinionAISystem.h:34-44](../../../../Engine/Public/ECS/Systems/MinionAISystem.h:34) |
| 11 | NavigationSystem 의 VelocityComponent write | `NavSystem` (phase 3) 이 path 진행률에서 vDirection / fSpeed 갱신 | §4.0 grep |
| 12 | Boids Separation 공식 | `for nearby in radius { force += normalize(self - other) * (radius - dist) / radius; }` 정규화 후 vDirection 에 weight 합산 | [MINION_CROWD_COLLISION_PLAN.md §2.1](../../../plan/engine/MINION_CROWD_COLLISION_PLAN.md) |
| 13 | Same-team only separation | 적 미니언 사이에는 separation 적용 0 (서로 멀어지면 attack 못 침). 같은 팀만 | LoL 정식 동작 + §2.E 검증 |
| 14 | Phase 11 대상 컴포넌트 access | read TransformComponent + read MinionStateComponent + write VelocityComponent. Phase 3 (Nav) 이 같은 write — 다른 phase | §3 GATE F |
| 15 | 부채꼴 산출 공식 | spawn 위치 = base + R*[cos(angle), 0, sin(angle)], angle = forward + (i - (N-1)/2) * step. forward = waypoint[1] - waypoint[0] | §4.4 |

**TODO**: §1.6 (NavigationSystem path 갱신 정확한 라인), §1.7 (VelocityComponent 헤더 위치), §1.11 (NavSystem 의 vDirection 갱신 위치) — §4.0 grep 단계에서 해소.

---

## §2. Code Reality Snapshot — 직접 인용

### §2.A — DoSpawnWave 비어있음 증명

[Client/Private/Manager/Minion_Manager.cpp:493-499](../../../../Client/Private/Manager/Minion_Manager.cpp:493):

```cpp
// ─────────────────────────────────────────────────────────────
// DoSpawnWave — 3라인 × 2팀, 근접 3 + 원거리 3
// ─────────────────────────────────────────────────────────────
void CMinion_Manager::DoSpawnWave()
{
    m_iCurrentRound = 0;
    m_fNextRoundCountdown = 0.f;   // 첫 라운드 즉시
}
```

→ **실제 spawn 위치 산출 0**. round 진입 시 `Tick` 의 round-up 분기에서 `Spawn_Minion(type, team, way)` 호출.

### §2.B — Spawn_Minion 위치 결정

[Client/Private/Manager/Minion_Manager.cpp:399-405](../../../../Client/Private/Manager/Minion_Manager.cpp:399):

```cpp
EntityID id = m_pWorld->CreateEntity();

auto& xform = m_pWorld->AddComponent<TransformComponent>(id);
// ... xform.SetPosition(waypoints[0]) — fan offset 0
auto& ms = m_pWorld->AddComponent<MinionStateComponent>(id);
ms.current         = MinionStateComponent::LaneMove;
```

→ **§4.0 grep 으로 정확한 SetPosition 호출 라인 확인**. 모든 미니언이 waypoint 0 → 같은 frame 5 마리 = 5 중첩.

### §2.C — Tick round-up spawn 호출

[Client/Private/Manager/Minion_Manager.cpp:178-185](../../../../Client/Private/Manager/Minion_Manager.cpp:178) (대략, §4.0 grep 으로 확정):

```cpp
// round-up 시
for (int type : roundComposition[m_iCurrentRound]) {
    for (int team : {0, 1}) {
        for (int way : {0, 1, 2}) {
            const EntityID id = Spawn_Minion(type, team, way);
            // ... fan offset 0 — 모든 미니언 같은 좌표
        }
    }
}
```

→ **본 박제 §4.4 변경**: `Spawn_Minion(type, team, way, fanIndex, fanCount)` 로 시그니처 확장 또는 위치 인자 추가.

### §2.D — MinionAI 의 2-pass 분리 (write VelocityComponent 0)

[Engine/Public/ECS/Systems/MinionAISystem.h:34-44](../../../../Engine/Public/ECS/Systems/MinionAISystem.h:34):

```cpp
struct MinionDecision
{
    EntityID self = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    MinionStateComponent::State desiredState = MinionStateComponent::Idle;
    Vec3 navTarget{};
    bool_t bSetNavTarget = false;
    bool_t bStopMovement = false;
    bool_t bStartAttack = false;
    f32_t cooldownAfterTick = 0.f;
};
```

→ **MinionAI 가 VelocityComponent 를 직접 write 안 함** — `bSetNavTarget` 플래그 후 NavAgent.vTarget 설정. **VelocityComponent write 는 NavigationSystem (phase 3)** 이 담당.

→ **MinionSeparationSystem 이 VelocityComponent.vDirection 에 write 시 NavSystem 과 phase 충돌 검사 필요**.

### §2.E — SystemScheduler 의 access conflict 분기

[Engine/Private/ECS/SystemScheduler.cpp:40-89](../../../../Engine/Private/ECS/SystemScheduler.cpp:40):

```cpp
void CSystemSchedular::Execute(CWorld& world, float fTimeDelta)
{
    for (auto& [phase, systems] : m_mapPhases)
    {
        // ...
        for (auto& sys : systems)
        {
            SystemAccessDesc desc = BuildAccessDesc(*sys);
            if (ConflictsWithBatch(desc, batchDescs))
                flushBatch();   // 충돌 시 직렬 분리

            batch.push_back(sys.get());
            batchDescs.push_back(std::move(desc));
        }
        flushBatch();
    }
}
```

→ **같은 phase 시스템 다중 등록 시 access conflict (write/write 같은 컴포넌트) 면 자동 직렬 분리, 아니면 JobSystem 병렬 실행**. P-9 (fractional phase) 회피 + Producer/Consumer 가 다른 정수 phase 강제.

### §2.F — Crowd Plan §2.1 (Boids Separation 원리)

[`.md/plan/engine/MINION_CROWD_COLLISION_PLAN.md` §2.1](../../../plan/engine/MINION_CROWD_COLLISION_PLAN.md):

```cpp
Vec3 separationForce = {0, 0, 0};
i32_t neighborCount = 0;
for (otherMinion in nearby) {
    if (other == self) continue;
    Vec3 delta = self.pos - other.pos;
    f32_t dist = length(delta);
    if (dist < kSeparationRadius && dist > 0.001f) {
        separationForce += normalize(delta) * (kSeparationRadius - dist) / kSeparationRadius;
        ++neighborCount;
    }
}
if (neighborCount > 0) {
    separationForce /= neighborCount;
    vel.vDirection = normalize(vel.vDirection + separationForce * kSeparationWeight);
}
```

→ 본 박제 §4.2 의 핵심 — 위 의사코드를 컴파일 가능 코드 + Worker-safety 패턴으로 변환.

### §2.G — VelocityComponent 정의 (§4.0 grep 으로 정확한 위치)

기대 형태 (§4.0 검증 필수):

```cpp
struct VelocityComponent
{
    Vec3   vDirection = { 0.f, 0.f, 0.f };
    f32_t  fSpeed = 0.f;
};
```

→ **field 이름 정확 검증 의무 — P-13 회피**.

---

## §3. 8 GATE 통과 검증

| GATE | 항목 | 통과 |
|---|---|---|
| **A — 사실 수집** | DoSpawnWave / Spawn_Minion / Tick round / MinionAISystem / SystemScheduler / Crowd Plan §2.1 | ✅ §2.A~F |
| **B — TODO 0** | §1.6 (NavSystem grep), §1.7 (VelocityComponent), §1.11 (vDirection write 위치), §2.G | ⚠️ §4.0 강제 |
| **C — 호출 경로 grep** | `Spawn_Minion` 호출자 + VelocityComponent write 위치 + SystemAccessBuilder 패턴 — §4.0 | ⚠️ §4.0 |
| **D — ECS 책임 경계** | Separation 은 ECS 시스템 — Scene 직접 호출 0. write 컴포넌트 = VelocityComponent.vDirection only | ✅ |
| **E — 향후 자료형** | 미니언 동시 100~500 마리. O(N²) = 100K 비교 — 1ms 내. 1000+ 시 SpatialIndex.QueryRadius 로 O(N log N) 교체 (§7) | ✅ 1차 OK |
| **F — Scheduler 동시성** | Separation = phase 11 (Manager.Tick 직전, NavSystem(3) 후, MinionAI(2) 와 다른 phase). VelocityComponent write 가 NavSystem 후 적용 — Producer NavSystem(3) → Consumer Separation(11) ✓ 정수 phase | ✅ |
| **G — Owner Scope** | `CMinionSeparationSystem` = `unique_ptr` → SystemScheduler 소유. World read-only access (TransformComponent / MinionStateComponent) + write VelocityComponent | ✅ |
| **H — 인용 의미 + 행동 보존** | Crowd Plan §2.1 의 의사코드 ↔ 박제 본문 의미 일치. **MinionAI 행동 (적 검색 / Chase / Attack) 변경 0** — separation 은 vDirection 보정만 | ✅ |

→ **B + C 가 §4 진입 직전 강제 GATE**.

---

## §4. 변경점

### §4.0 — 진입 게이트 (Bash/Grep 의무)

```bash
# §1.6 + §1.7 + §1.11 + GATE B/C 해소
grep -rn "VelocityComponent" Engine/Public/ECS/Components/
grep -rn "VelocityComponent" Engine/Private/ECS/Systems/NavigationSystem.cpp
grep -rn "Spawn_Minion" Client/Private/Manager/Minion_Manager.cpp
grep -rn "DescribeAccess\|CSystemAccessBuilder" Engine/

# 부수 검증
grep -rn "m_vecWaypoints\[" Client/Private/Manager/Minion_Manager.cpp
```

**기대 산출**:
- VelocityComponent 정확한 헤더/필드.
- NavSystem 의 vDirection write 라인 (Producer 검증).
- Spawn_Minion 호출자 N 곳 (Tick 의 round-up 분기 정확한 위치).
- DescribeAccess 패턴 (Read/Write 등록 방식).
- Waypoints 배열 접근 패턴.

### §4.1 — Stage 1: CMinionSeparationSystem 신규 헤더

**파일**: `Engine/Public/ECS/Systems/MinionSeparationSystem.h` (신규)

```cpp
#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/ISystem.h"

#include <memory>

class CWorld;
class CJobSystem;

NS_BEGIN(Engine)

class WINTERS_ENGINE CMinionSeparationSystem final : public ISystem
{
public:
    ~CMinionSeparationSystem() override = default;

    static std::unique_ptr<CMinionSeparationSystem> Create()
    {
        return std::unique_ptr<CMinionSeparationSystem>(
            new CMinionSeparationSystem());
    }

    // Phase 11 — Manager.Tick 직전 (NavSystem 의 vDirection 갱신 후 보정)
    u32_t GetPhase() const override { return 11; }
    const char* GetName() const override { return "MinionSeparationSystem"; }
    void DescribeAccess(CSystemAccessBuilder& builder) const override;
    void Execute(CWorld& world, f32_t fTimeDelta) override;

    void Set_JobSystem(CJobSystem* pJS) { m_pJobSystem = pJS; }

    // ImGui 튜닝 — CLAUDE.md §6.8 (신규 시스템 ImGui 슬라이더 의무)
    void SetSeparationRadius(f32_t r) { m_fSeparationRadius = r; }
    void SetSeparationWeight(f32_t w) { m_fSeparationWeight = w; }
    void SetEnabled(bool_t b) { m_bEnabled = b; }
    f32_t GetSeparationRadius() const { return m_fSeparationRadius; }
    f32_t GetSeparationWeight() const { return m_fSeparationWeight; }
    bool_t GetEnabled() const { return m_bEnabled; }

private:
    CMinionSeparationSystem() = default;

    f32_t m_fSeparationRadius = 1.0f;     // 이웃 인식 반경 (1m)
    f32_t m_fSeparationWeight = 0.5f;     // nav direction 대비 separation 비중
    i32_t m_iMaxNeighbors = 8;            // O(N²) 절감 — 가까운 8 마리만
    bool_t m_bEnabled = true;

    CJobSystem* m_pJobSystem = nullptr;
};

NS_END
```

**근거**:
- **Phase 11** = NavSystem(3) 후 Manager.Tick 직전. Producer (NavSystem 의 vDirection write) → Consumer (Separation 의 vDirection read+write). 다른 정수 phase = race 0.
- **`DescribeAccess`** 박제 의무 — SystemScheduler 가 access conflict 검사 시 사용.
- **Set_JobSystem** + worker-safe — `MINION_COMBAT_AND_WORKER_SAFETY.md` 정책 (4) "self entity write only" 패턴.
- **ImGui 슬라이더** — CLAUDE.md §6.8 강제.

### §4.2 — Stage 1: CMinionSeparationSystem 본체

**파일**: `Engine/Private/ECS/Systems/MinionSeparationSystem.cpp` (신규)

```cpp
#include "WintersPCH.h"
#include "ECS/Systems/MinionSeparationSystem.h"
#include "ECS/World.h"
#include "ECS/SystemAccess.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Core/JobSystem.h"
#include "ProfilerAPI.h"

#include <cmath>
#include <vector>

NS_BEGIN(Engine)

void CMinionSeparationSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    // ★ §4.0 grep 으로 builder API 정확 검증 후 박제
    // 기대 형태:
    // builder.AddRead<TransformComponent>();
    // builder.AddRead<MinionStateComponent>();
    // builder.AddWrite<VelocityComponent>();
    // ... 실제 API 명은 §4.0 grep 결과로 확정
}

void CMinionSeparationSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    if (!m_bEnabled)
        return;

    WINTERS_PROFILE_SCOPE("MinionSeparation::Execute");

    // 1. Snapshot — read TransformComponent + MinionStateComponent + team
    struct MinionSnap
    {
        EntityID id = NULL_ENTITY;
        Vec3 pos{};
        u8_t team = 0;
        bool_t bAlive = false;
    };

    std::vector<MinionSnap> vecSnaps;
    vecSnaps.reserve(128);

    world.ForEach<MinionStateComponent, TransformComponent>(
        function<void(EntityID, MinionStateComponent&, TransformComponent&)>(
            [&](EntityID id, MinionStateComponent& ms, TransformComponent& tf)
            {
                if (ms.current == MinionStateComponent::Dead)
                    return;
                vecSnaps.push_back({
                    id,
                    tf.GetPosition(),
                    static_cast<u8_t>(ms.team),
                    true
                });
            }));

    if (vecSnaps.size() < 2)
        return;

    // 2. ApplyPass — write VelocityComponent.vDirection
    //    self entity write only (worker-safe)
    const f32_t r = m_fSeparationRadius;
    const f32_t w = m_fSeparationWeight;
    const f32_t r2 = r * r;
    const i32_t maxN = m_iMaxNeighbors;

    i32_t profileApplied = 0;

    world.ForEach<MinionStateComponent, VelocityComponent>(
        function<void(EntityID, MinionStateComponent&, VelocityComponent&)>(
            [&](EntityID id, MinionStateComponent& ms, VelocityComponent& vel)
            {
                if (ms.current == MinionStateComponent::Dead)
                    return;
                if (vel.fSpeed <= 0.001f)
                    return;   // 정지 미니언 = separation 0 (Attack 중 등)

                // self pos
                Vec3 selfPos{};
                {
                    auto it = std::find_if(vecSnaps.begin(), vecSnaps.end(),
                        [id](const MinionSnap& s) { return s.id == id; });
                    if (it == vecSnaps.end())
                        return;
                    selfPos = it->pos;
                }

                // Neighbor scan — same team only (LoL 정식 — 적 미니언 사이 0)
                Vec3 force{ 0.f, 0.f, 0.f };
                i32_t neighborCount = 0;
                const u8_t selfTeam = static_cast<u8_t>(ms.team);

                for (const MinionSnap& other : vecSnaps)
                {
                    if (other.id == id) continue;
                    if (other.team != selfTeam) continue;
                    if (neighborCount >= maxN) break;

                    const f32_t dx = selfPos.x - other.pos.x;
                    const f32_t dz = selfPos.z - other.pos.z;
                    const f32_t distSq = dx * dx + dz * dz;
                    if (distSq >= r2) continue;
                    if (distSq < 0.0001f) continue;   // 완벽 중첩 = skip (NaN 방지)

                    const f32_t dist = std::sqrt(distSq);
                    const f32_t strength = (r - dist) / r;
                    const f32_t invDist = 1.f / dist;
                    force.x += dx * invDist * strength;
                    force.z += dz * invDist * strength;
                    ++neighborCount;
                }

                if (neighborCount == 0) return;

                // Average + apply weight + 합성
                force.x /= static_cast<f32_t>(neighborCount);
                force.z /= static_cast<f32_t>(neighborCount);

                // vel.vDirection 은 NavSystem 이 set 한 nav forward — separation force 합산 후 정규화
                vel.vDirection.x += force.x * w;
                vel.vDirection.z += force.z * w;

                // y=0 평면 운동 — y 컴포넌트 보존 0 (NavSystem 이 0 보장)
                const f32_t mag = std::sqrt(
                    vel.vDirection.x * vel.vDirection.x +
                    vel.vDirection.z * vel.vDirection.z);
                if (mag > 0.001f)
                {
                    vel.vDirection.x /= mag;
                    vel.vDirection.z /= mag;
                }

                ++profileApplied;
            }));

    WINTERS_PROFILE_COUNT("MinionSeparation::Applied", profileApplied);
    WINTERS_PROFILE_COUNT("MinionSeparation::Snaps", static_cast<i32_t>(vecSnaps.size()));
}

NS_END
```

**근거**:
- **Phase 11 = NavSystem(3) 후** — `vel.vDirection` 이 이미 nav forward 로 set 된 상태에서 separation force 합산 후 정규화. nav 의도 (path 따라가기) 보존.
- **Same team only** — `other.team != selfTeam` skip. 적 미니언 사이 separation = 0. LoL 정식 (적끼리는 hard collision 직접 처리, separation 아님).
- **Max neighbors 8** — O(N²) 의 worst case 부하 cap. 100 미니언 = 100 × 8 = 800 비교 × 1 frame = 작은 부하.
- **`vel.fSpeed <= 0.001f` 분기** — Attack/Idle 미니언은 separation 0. 이동 중 미니언만 보정.
- **`distSq < 0.0001f` 분기** — 완벽 중첩 NaN 방지. 다음 frame 이 자연 분리.
- **`y` 컴포넌트 보존 0** — NavSystem 이 y=0 평면 운동 가정.
- **`MinionSnap` snapshot** — ForEach 안에서 다른 entity 의 TransformComponent 직접 query 시 race 위험. 1 pass snapshot 후 2 pass apply.
- **Profile counter** — `MinionSeparation::Applied` 매 frame 갱신 미니언 수.

### §4.3 — Stage 1: vcxproj + 시스템 등록

**vcxproj**:

```xml
<!-- Engine/Include/Engine.vcxproj -->
<ItemGroup>
  <ClInclude Include="..\Public\ECS\Systems\MinionSeparationSystem.h" />
</ItemGroup>
<ItemGroup>
  <ClCompile Include="..\Private\ECS\Systems\MinionSeparationSystem.cpp" />
</ItemGroup>
```

`Engine.vcxproj.filters` 에도 동일 추가 (Filter = `Public\ECS\Systems` / `Private\ECS\Systems`).

**시스템 등록** — `Client/Private/Scene/InGameBootstrapBridge.cpp`:

```cpp
#include "ECS/Systems/MinionSeparationSystem.h"

// ... 기존 시스템 등록 후
// MinionAISystem, NavigationSystem 등록 직후
{
    auto pSep = Engine::CMinionSeparationSystem::Create();
    scene.m_pMinionSeparation = pSep.get();   // raw 포인터 캐시 (ImGui 튜너용)
    if (scene.m_pJobSystem)
        pSep->Set_JobSystem(scene.m_pJobSystem);
    scene.m_pScheduler->RegisterSystem(std::move(pSep));
}
```

**Scene_InGame 멤버 추가**:

```cpp
// Client/Public/Scene/Scene_InGame.h
class CScene_InGame final : public IScene
{
private:
    Engine::CMinionSeparationSystem* m_pMinionSeparation = nullptr;   // SystemScheduler owned, raw 캐시
};
```

### §4.4 — Stage 2: Spawn 부채꼴 박제

**파일**: `Client/Private/Manager/Minion_Manager.cpp`

**변경 1 — Spawn_Minion 시그니처 확장**:

```cpp
// 기존 (헤더)
EntityID Spawn_Minion(eMinionType eType, eMinionTeam eTeamParam, eMinionWay eWay);

// 변경 (헤더)
EntityID Spawn_Minion(eMinionType eType, eMinionTeam eTeamParam, eMinionWay eWay,
    i32_t fanIndex = 0, i32_t fanCount = 1);
```

**변경 2 — Spawn_Minion 본체 위치 산출**:

기존 (예시 — §4.0 grep 으로 정확 라인 확정):

```cpp
EntityID CMinion_Manager::Spawn_Minion(eMinionType eType, eMinionTeam eTeamParam, eMinionWay eWay)
{
    // ... waypoints 배열 추출
    const Vec3* pWaypoints = nullptr;
    u32_t count = 0;
    GetWayPoints(eTeamParam, eWay, &pWaypoints, &count);
    if (!pWaypoints || count == 0) return NULL_ENTITY;

    EntityID id = m_pWorld->CreateEntity();
    auto& xform = m_pWorld->AddComponent<TransformComponent>(id);
    xform.SetPosition(pWaypoints[0]);   // ★ fan offset 0
    // ...
}
```

변경:

```cpp
EntityID CMinion_Manager::Spawn_Minion(eMinionType eType, eMinionTeam eTeamParam, eMinionWay eWay,
    i32_t fanIndex, i32_t fanCount)
{
    const Vec3* pWaypoints = nullptr;
    u32_t count = 0;
    GetWayPoints(eTeamParam, eWay, &pWaypoints, &count);
    if (!pWaypoints || count == 0) return NULL_ENTITY;

    // 부채꼴 spawn 위치 산출
    Vec3 spawnPos = pWaypoints[0];
    if (fanCount > 1 && count >= 2)
    {
        // forward = waypoint[1] - waypoint[0]
        const Vec3 fwd{
            pWaypoints[1].x - pWaypoints[0].x,
            0.f,
            pWaypoints[1].z - pWaypoints[0].z
        };
        const f32_t fwdMag = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
        if (fwdMag > 0.001f)
        {
            const Vec3 fwdN{ fwd.x / fwdMag, 0.f, fwd.z / fwdMag };

            // perpendicular (right-hand) — y 축 회전 90도
            const Vec3 right{ -fwdN.z, 0.f, fwdN.x };

            // 부채꼴 폭 = (fanCount - 1) * step. step = 1.5m (미니언 충돌 직경)
            constexpr f32_t kFanStep = 1.5f;
            const f32_t centerOffset = static_cast<f32_t>(fanIndex) - 0.5f * static_cast<f32_t>(fanCount - 1);
            const f32_t lateral = centerOffset * kFanStep;

            // 백오프 — fan 의 가운데가 가장 앞, 양 끝이 살짝 뒤
            const f32_t back = std::abs(centerOffset) * 0.4f;

            spawnPos.x += right.x * lateral - fwdN.x * back;
            spawnPos.z += right.z * lateral - fwdN.z * back;
        }
    }

    EntityID id = m_pWorld->CreateEntity();
    auto& xform = m_pWorld->AddComponent<TransformComponent>(id);
    xform.SetPosition(spawnPos);
    // ... 이하 컴포넌트 부착 동일
    return id;
}
```

**변경 3 — Tick 의 round-up 분기 호출자**:

```cpp
// Tick 안의 round-up 분기 (§4.0 grep 으로 정확한 라인 확정)
// 예시:
const i32_t kRoundCount = static_cast<i32_t>(roundComposition[m_iCurrentRound].size());
i32_t fanIdx = 0;
for (eMinionType type : roundComposition[m_iCurrentRound])
{
    for (eMinionTeam team : { eMinionTeam::Blue, eMinionTeam::Red })
    {
        for (eMinionWay way : { eMinionWay::Top, eMinionWay::Mid, eMinionWay::Bot })
        {
            const EntityID id = Spawn_Minion(type, team, way, fanIdx, kRoundCount);
        }
    }
    ++fanIdx;
}
```

**근거**:
- **`fanIndex / fanCount`** = round 안에서 N 마리 spawn 시 i 번째 / 전체. spawn 위치 = waypoint[0] + lateral * right - back * forward.
- **`right = perpendicular(forward)`** — 라인 진행 방향에 수직 = 부채꼴 폭.
- **`back = |centerOffset| * 0.4f`** — 가운데 미니언 가장 앞, 양 끝 살짝 뒤 — 부채꼴 효과.
- **`kFanStep = 1.5m`** — 미니언 SpatialAgent.radius=0.5 → 직경 1.0 + 여유 0.5.
- **fanCount = 1 시 fan offset 0** — 단일 미니언 spawn 시 기존 동작 유지 (행동 보존, P-14).

### §4.5 — Stage 1+2 ImGui 튜너 (선택)

**파일**: `Client/Private/UI/MinionTunerPanel.cpp` (또는 기존 `ChampionTuner` 확장)

```cpp
// 기존 ImGui 패널에 항목 추가
if (ImGui::CollapsingHeader("Minion Separation"))
{
    if (m_pMinionSeparation)
    {
        bool_t bEnabled = m_pMinionSeparation->GetEnabled();
        if (ImGui::Checkbox("Enabled##sep", &bEnabled))
            m_pMinionSeparation->SetEnabled(bEnabled);

        f32_t r = m_pMinionSeparation->GetSeparationRadius();
        if (ImGui::SliderFloat("Radius (m)##sep", &r, 0.5f, 3.0f, "%.2f"))
            m_pMinionSeparation->SetSeparationRadius(r);

        f32_t w = m_pMinionSeparation->GetSeparationWeight();
        if (ImGui::SliderFloat("Weight##sep", &w, 0.0f, 1.5f, "%.2f"))
            m_pMinionSeparation->SetSeparationWeight(w);
    }
}
```

---

## §5. 검증 결정 포인트

### §5.1 — 시각 검증

1. F5 빌드 + InGame 진입 (Bot 매치).
2. Spawn Wave 실행 (`DEBUG_SpawnWaveNow` 또는 자연 wave).
3. **Spawn 시점**: 미니언 5~7 마리가 부채꼴로 분포. 한 점 spawn 0.
4. **이동 중**: 라인 따라 이동하는 미니언이 좌우로 약간 흔들림 (separation 효과). 한 줄 강제 X.
5. **다굴 시점**: 같은 적 미니언 향해 5+ 마리 다굴 시 적 주변에 부채꼴로 둘러쌈. 한 점 겹침 0.
6. **ImGui 슬라이더**:
   - `Radius = 0` — separation 무효, 한 점 겹침 (회귀 검증).
   - `Radius = 1.0, Weight = 0.5` — 자연스러운 분포.
   - `Radius = 3.0, Weight = 1.5` — 과도한 분리, 미니언이 line 이탈.
7. **Same team only** 검증: 적 미니언 사이 separation 0 — 적이 아군과 정상 평타 거리 도달.

### §5.2 — Profiler 검증

- `WINTERS_PROFILE_COUNT("MinionSeparation::Applied", count)` — 매 frame separation 적용된 미니언 수.
- `WINTERS_PROFILE_COUNT("MinionSeparation::Snaps", count)` — 미니언 snapshot 수.
- Frame time 영향 — 100 미니언 = 1ms 미만 기대. 1000+ 시 SpatialIndex 교체 (§7).

### §5.3 — Definition of Done

- [ ] §4.0 grep 으로 VelocityComponent / NavSystem write / Spawn_Minion 호출자 식별
- [ ] §4.1 헤더 박제
- [ ] §4.2 cpp 박제 (snapshot + applypass)
- [ ] §4.3 vcxproj + 시스템 등록 + Scene_InGame 멤버
- [ ] §4.4 Spawn_Minion 시그니처 확장 + Tick round-up 호출자 변경
- [ ] §4.5 ImGui 튜너 (선택)
- [ ] §5.1 시각 검증 7 항목 통과
- [ ] CMinion_Manager::DoSpawnWave 동작 확인 (회귀 0)

---

## §6. Codex Pre-Mortem

### M-1. Phase 11 충돌 — 다른 시스템과 같은 phase
**증상**: Phase 11 에 다른 시스템 등록 시 race.
**원인**: 현재 코드 phase 0~10 사용 → 11 단독.
**해결**: §4.0 grep `GetPhase()` 모든 시스템 — 11 미사용 확인. 충돌 시 phase 12.

### M-2. NavSystem 의 vDirection 정규화 가정 깨짐
**증상**: NavSystem 이 vDirection 을 비정규화 상태로 set → Separation 합산 후 magnitude 폭주.
**원인**: NavSystem 의 vDirection 갱신 패턴 (§4.0 grep).
**해결**: §4.2 끝의 정규화 (`mag > 0.001f`) 가 항상 수행되므로 OK. 단 NavSystem 이 vDirection.fSpeed 분리 set 인지 검증.

### M-3. Same team check 가 다른 team enum 사용
**증상**: `MinionStateComponent::team (eTeam Blue/Red/Neutral)` vs `SpatialAgentComponent::team (u8_t 0/1)` 불일치.
**원인**: Cast 정책. CLAUDE.md §5.6 의 enum class 이름 충돌 함정.
**해결**: §4.2 의 `static_cast<u8_t>(ms.team)` — eTeam::Blue=0, Red=1 가정. §4.0 grep 으로 enum 값 검증.

### M-4. Snapshot vector reallocation 동안 ForEach 변경
**증상**: ForEach 안에서 vecSnaps.push_back 시 capacity 초과 → reallocation.
**원인**: 동시성 0 (단일 thread ForEach). race 0.
**해결**: `vecSnaps.reserve(128)` 으로 reallocation 1~2회로 제한. 1000+ 미니언 시 reserve(2048).

### M-5. fanCount=0 분모
**증상**: `fanCount = 0` 으로 호출 시 `0.5 * (0-1) = -0.5` 계산 후 division 0.
**원인**: 호출자 실수.
**해결**: §4.4 의 `if (fanCount > 1 && count >= 2)` 가드 — fanCount<=1 면 spawn 위치 = waypoint[0] (fan 비활성).

### M-6. Spawn 좌표가 NavGrid 외부 → Pathfinder empty path silent fail
**증상**: 부채꼴 lateral 1.5m 가 NavGrid walkable 셀 외부면 Pathfinder 가 empty path 반환 → CLAUDE.md §5.6 silent fail.
**원인**: NavGrid 셀 경계 + lateral 1.5m 합 = 일부 미니언이 unwalkable.
**해결**: §4.4 의 lateral 합 후 NavGrid::IsWalkable 검사 → false 면 lateral 절반으로 줄임. 또는 spawn 위치 = waypoint[0] fallback. 첫 박제 = simple, 회귀 시 fallback 박제.

### M-7. Profile counter slot conflict
**증상**: `MinionSeparation::Applied` 카운터가 다른 시스템 카운터 이름과 충돌.
**원인**: ProfilerAPI 의 internal map.
**해결**: 카운터 이름 prefix unique — `MinionSep::Applied` 또는 `MinionSeparation::Applied`. 본 박제 `MinionSeparation::*` 사용.

---

## §7. 후속 (별도 박제)

| Cycle | 내용 |
|---|---|
| Stage 3 | SpatialIndex.QueryRadius 로 O(N²) → O(N log N) (1000+ 미니언) |
| Phase 7 Jolt Physics | Capsule 충돌 + RigidBody 정식. Soft Separation 흡수 |
| Crowd Plan §3 M5 추가 | ImGui 슬라이더 튜닝 자동화 (preset 저장/로드) |
| Champion vs Minion hard collision | 챔프와 미니언 사이 막힘 (LoL 정식) — 별도 시스템 |
| Bot AI separation | Bot 챔프도 separation 적용 (군중 공격 시) |

---

## §8. 다음 세션 진입 명령

```
"Minion Separation + Fan-out 진입.
.md/TODO/05-07/Minion/00_MINION_SEPARATION_AND_FAN.md §4.0 grep 으로
VelocityComponent / NavSystem / Spawn_Minion 호출자 식별
→ §4.1 헤더 → §4.2 cpp → §4.3 vcxproj/등록 → §4.4 Spawn 부채꼴
→ §5.1 시각 검증 7 항목.
M-3 (eTeam cast), M-6 (NavGrid 외부) 우선 검증."
```

진입 직전 체크리스트:
- [ ] devenv.exe 종료
- [ ] `git checkout -b feature/minion-separation-fan`
- [ ] Engine 단독 빌드 1회
- [ ] MINION_CROWD_COLLISION_PLAN.md 와 본 계획서 cross-check (`§3 M1` ~ M9 마일스톤)

---

**END OF MINION SEPARATION + FAN PLAN**
