# Phase 04a v2 — D-0 Sub-plan: Shared cpp link closure + ServerSimSubset

**작성일**: 2026-04-30
**상위 문서**: `04a_MVP_2CLIENT_TCP_DEMO_v2.md` §3
**범위**: D-0A (Server.vcxproj Shared cpp 편입) + D-0B (ServerSimSubset 5종 + MoveTargetComponent 신규)
**합격**: D-1 진입 전 LNK2019 0건. ServerSimSubset 호출이 GameRoom Tick 에서 동작.

**한 줄**: **Server EXE 가 Shared/GameSim sim cpp 들을 link + Layer 1 sim subset 5종 박제. Move/Cooldown/Death 동작, CastSkill/BasicAttack 은 reject/log only (Layer 2 에서 본격).**

---

## 1. D-0A — Server.vcxproj 에 Shared cpp 편입 (3h)

### 1.1 변경 위치

**파일**: `C:\Users\user\Desktop\Winters\Server\Include\Server.vcxproj`
**섹션**: `<ItemGroup>` (ClCompile 그룹) — 기존 `Server/Private/*.cpp` 항목 뒤에 추가

### 1.2 추가 XML (★ 명시 경로 — wildcard 금지)

```xml
<ItemGroup>
  <!-- ──────────────────────────────────────────────────────── -->
  <!-- ★ D-0A — Shared/GameSim cpp 편입 (Sim-10 v2 M1 prerequisite) -->
  <!-- ──────────────────────────────────────────────────────── -->

  <!-- Registries (정적 lookup) -->
  <ClCompile Include="..\..\Shared\GameSim\Registries\ChampionStatsRegistry.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Registries\SkillScalingRegistry.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Registries\SkinRegistry.cpp" />

  <!-- 결정성 sim systems (이미 박제됨) -->
  <ClCompile Include="..\..\Shared\GameSim\Systems\BuffSystem.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\StatSystem.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\SkillRankSystem.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\GameplayHookRegistry.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\DamagePipeline.cpp" />

  <!-- ★ D-0B 신규 (본 sub-plan §2) -->
  <ClCompile Include="..\..\Shared\GameSim\Systems\CommandExecutor.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\MoveSystem.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\SkillCooldownSystem.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.cpp" />
  <ClCompile Include="..\..\Shared\GameSim\Systems\DeathSystem.cpp" />
</ItemGroup>

<ItemGroup>
  <!-- ★ D-0A — Shared 헤더 (IDE 표시용 — 컴파일은 안 됨) -->
  <ClInclude Include="..\..\Shared\GameSim\Components\MoveTargetComponent.h" />
  <ClInclude Include="..\..\Shared\GameSim\Systems\MoveSystem.h" />
  <ClInclude Include="..\..\Shared\GameSim\Systems\SkillCooldownSystem.h" />
  <ClInclude Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.h" />
  <ClInclude Include="..\..\Shared\GameSim\Systems\DeathSystem.h" />
</ItemGroup>
```

**AdditionalIncludeDirectories** 확인 (`<ClCompile>` 의 ItemDefinitionGroup 안):
```xml
<AdditionalIncludeDirectories>
  $(SolutionDir);
  $(SolutionDir)Shared;
  $(SolutionDir)EngineSDK\inc;
  $(SolutionDir)Engine\Public;
  $(SolutionDir)Server\Public;
  $(SolutionDir)Engine\ThirdPartyLib\FlatBuffers\Inc;
  %(AdditionalIncludeDirectories)
</AdditionalIncludeDirectories>
```

### 1.3 합격
- Visual Studio 솔루션 reload 후 Server 프로젝트가 위 cpp/h 12개 인식
- D-0B 파일들은 미존재 → 빌드 시 file-not-found 에러 (D-0B 진행하면서 해소)

---

## 2. D-0B — ServerSimSubset 5종 + MoveTargetComponent 신규 (8h)

### 2.1 `Shared/GameSim/Components/MoveTargetComponent.h` (신규, ★ 전문)

★ **Codex 보정 (P1-4)**: `Engine_Defines.h` 대신 `WintersTypes.h` + `WintersMath.h` + `<type_traits>` 만.
서버 sim 헤더가 D3D / DirectInput 같은 클라 의존을 끌어오지 않게 차단. 또 `std::is_trivially_copyable_v`
는 `<type_traits>` 가 없으면 unresolved (Engine_Defines 가 우연히 포함시켜준 것뿐 — 명시 include 가 안전).

```cpp
#pragma once
#include "WintersTypes.h"
#include "WintersMath.h"

#include <type_traits>

// ★ Sim-10 v2 / 04a v2 D-0B — Layer 1 Move sync 용
//   Move command 가 들어오면 MoveSystem 이 이 컴포넌트의 target 으로 적분
//   POD — trivially_copyable 보장
struct MoveTargetComponent
{
    Vec3   target{};
    f32_t  arriveRadius = 0.15f;   // 도착 판정 반경 (m)
    bool_t bHasTarget = false;
};

static_assert(std::is_trivially_copyable_v<MoveTargetComponent>,
    "MoveTargetComponent must be trivially_copyable for sim determinism (Sim-2)");
```

### 2.2 `Shared/GameSim/Systems/MoveSystem.h` (신규, ★ 전문)

```cpp
#pragma once

class CWorld;
struct TickContext;

// ★ 04a v2 D-0B — Server-authoritative move integration
//   MoveTargetComponent.target 방향으로 StatComponent.moveSpeed * tc.fDt 만큼 적분
//   도착 시 bHasTarget = false
class CMoveSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CMoveSystem() = delete;
};
```

### 2.3 `Shared/GameSim/Systems/MoveSystem.cpp` (신규, ★ 전문)

```cpp
#include "Shared/GameSim/Systems/MoveSystem.h"

#include "Shared/GameSim/Systems/ICommandExecutor.h"  // TickContext
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"

#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"

#include <cmath>

void CMoveSystem::Execute(CWorld& world, const TickContext& tc)
{
    // ★ 결정성 — sorted EntityID 순회
    auto entities = DeterministicEntityIterator<MoveTargetComponent>::CollectSorted(world);

    for (EntityID e : entities)
    {
        auto& mt = world.GetComponent<MoveTargetComponent>(e);
        if (!mt.bHasTarget) continue;

        // 죽은 엔티티는 이동 X
        if (world.HasComponent<HealthComponent>(e))
        {
            const auto& hp = world.GetComponent<HealthComponent>(e);
            if (hp.bIsDead) { mt.bHasTarget = false; continue; }
        }

        if (!world.HasComponent<TransformComponent>(e)) continue;
        if (!world.HasComponent<StatComponent>(e))      continue;

        auto& tf = world.GetComponent<TransformComponent>(e);
        const auto& stat = world.GetComponent<StatComponent>(e);

        // ★ Codex 보정 (P1-3): D-1G phase chain 에 CTransformSystem 미포함 →
        //   m_WorldMatrix 는 매 tick 갱신되지 않으므로 GetWorldPosition() 은 stale.
        //   서버 sim entity 는 root (parent 없음) → local == world. SetPosition 이
        //   동일 frame 안에서 적분한 값을 다시 읽으려면 local 직접 사용해야 함.
        //   Layer 2 에서 hierarchy / mount point 도입 시 phase chain 에 CTransformSystem
        //   추가 후 GetWorldPosition() 으로 복원.
        const Vec3 pos = tf.GetLocalPosition();
        const Vec3 to{ mt.target.x - pos.x, 0.f, mt.target.z - pos.z };
        const f32_t dist = std::sqrtf(to.x * to.x + to.z * to.z);

        if (dist <= mt.arriveRadius)
        {
            mt.bHasTarget = false;
            continue;
        }

        const f32_t step = stat.moveSpeed * tc.fDt;
        const f32_t advance = std::min(step, dist);
        const f32_t inv = 1.f / dist;

        const Vec3 next{
            pos.x + to.x * inv * advance,
            pos.y,
            pos.z + to.z * inv * advance
        };

        tf.SetPosition(next);
        tf.m_bLocalDirty = true;
        tf.m_bWorldDirty = true;
    }
}
```

### 2.4 `Shared/GameSim/Systems/SkillCooldownSystem.h` (신규, ★ 전문)

```cpp
#pragma once

class CWorld;
struct TickContext;

// ★ 04a v2 D-0B — Layer 1 = cooldown tick only.
//   Layer 2 에서 castStage / windup / hit / recovery 본격 박제
class CSkillCooldownSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CSkillCooldownSystem() = delete;
};
```

### 2.5 `Shared/GameSim/Systems/SkillCooldownSystem.cpp` (신규, ★ 전문)

```cpp
#include "Shared/GameSim/Systems/SkillCooldownSystem.h"

#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"

#include "ECS/World.h"

void CSkillCooldownSystem::Execute(CWorld& world, const TickContext& tc)
{
    // ★ Codex C9 — 실 구조: SkillStateComponent.slots[5].cooldownRemaining
    auto entities = DeterministicEntityIterator<SkillStateComponent>::CollectSorted(world);

    for (EntityID e : entities)
    {
        auto& sk = world.GetComponent<SkillStateComponent>(e);

        for (u8_t i = 0; i < 5; ++i)
        {
            if (sk.slots[i].cooldownRemaining > 0.f)
            {
                sk.slots[i].cooldownRemaining -= tc.fDt;
                if (sk.slots[i].cooldownRemaining < 0.f)
                    sk.slots[i].cooldownRemaining = 0.f;
            }
        }

        // ★ Layer 2 — castStage / windup / hit / recovery tick 은 별도 ActiveCastSystem
        //   본 시스템은 cooldown 만.
    }
}
```

### 2.6 `Shared/GameSim/Systems/DamageQueueSystem.h` (신규, ★ 전문)

```cpp
#pragma once

class CWorld;
struct TickContext;

// ★ 04a v2 D-0B — Layer 1 = stub (no-op).
//   Layer 2 에서 DamageRequestComponent → HealthComponent.current -= damage
class CDamageQueueSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CDamageQueueSystem() = delete;
};
```

### 2.7 `Shared/GameSim/Systems/DamageQueueSystem.cpp` (신규, ★ 전문 — Layer 1 stub)

```cpp
#include "Shared/GameSim/Systems/DamageQueueSystem.h"

#include "Shared/GameSim/Systems/ICommandExecutor.h"

void CDamageQueueSystem::Execute(CWorld& world, const TickContext& tc)
{
    (void)world;
    (void)tc;

    // ★ Layer 1 = no-op. Layer 2 (D-4E) 에서 본격:
    //   - DamageRequestComponent 큐 순회 (sorted)
    //   - HealthComponent.current -= dmg
    //   - bIsDead 판정 → DeathSystem 으로 cascade
    //   - Layer 2 에서 EventBatch 에 DamageEvent emit
}
```

### 2.8 `Shared/GameSim/Systems/DeathSystem.h` (신규, ★ 전문)

```cpp
#pragma once

class CWorld;
struct TickContext;

// ★ 04a v2 D-0B — HP 0 → bIsDead 처리. Move/Skill 차단.
//   디버그 키로 HP 회복 시 bIsDead = false (Layer 1 검증용)
class CDeathSystem
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CDeathSystem() = delete;
};
```

### 2.9 `Shared/GameSim/Systems/DeathSystem.cpp` (신규, ★ 전문)

```cpp
#include "Shared/GameSim/Systems/DeathSystem.h"

#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"

#include "ECS/World.h"

void CDeathSystem::Execute(CWorld& world, const TickContext& tc)
{
    (void)tc;

    auto entities = DeterministicEntityIterator<HealthComponent>::CollectSorted(world);

    for (EntityID e : entities)
    {
        auto& hp = world.GetComponent<HealthComponent>(e);

        // ★ Codex 보정 (P1-1, P1-2):
        //   - 필드명: current/maxValue → fCurrent/fMaximum
        //     (실제 정의: Engine/Public/ECS/Components/CoreComponents.h:18)
        //   - cascade guard: 기존 `!hp.bIsDead && ...` 게이트는 사고 유발.
        //     CStatSystem (StatSystem.cpp:92) / CDamagePipeline (DamagePipeline.cpp:83)
        //     이 이미 hp.bIsDead 를 set 하면, DeathSystem 의 cascade (Move/Skill 중단) 가
        //     스킵됨. → guard 제거. hp.fCurrent <= 0.f 이면 항상 정규화 + cascade.
        //     wasDead 는 Layer 2 의 DeathEvent emit edge 판정용으로만 보관.
        const bool_t wasDead = hp.bIsDead;
        if (hp.fCurrent <= 0.f)
        {
            hp.fCurrent = 0.f;
            hp.bIsDead = true;

            // 이동 중단 — bIsDead 가 이미 set 됐어도 매 tick 강제 (cascade race 방지)
            if (world.HasComponent<MoveTargetComponent>(e))
            {
                auto& mt = world.GetComponent<MoveTargetComponent>(e);
                mt.bHasTarget = false;
            }

            // 활성 cast 중단 (Layer 2 에서 ActiveCastComponent 도 정리)
            if (world.HasComponent<SkillStateComponent>(e))
            {
                auto& sk = world.GetComponent<SkillStateComponent>(e);
                for (u8_t i = 0; i < 5; ++i)
                    sk.slots[i].currentStage = 0;
            }

            // ★ Layer 2 — !wasDead 시점에만 DeathEvent emit, respawn timer 시작
            (void)wasDead;
        }

        // 디버그 회복 (Layer 1 검증용)
        if (hp.bIsDead && hp.fCurrent > 0.f)
            hp.bIsDead = false;
    }
}
```

### 2.10 `Shared/GameSim/Systems/CommandExecutor.cpp` (신규, ★ 전문)

```cpp
#include "Shared/GameSim/Systems/ICommandExecutor.h"

#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Systems/SkillRankSystem.h"   // ★ Codex P2-6 — TryLevelSkill

#include "ECS/World.h"

#include <algorithm>

// ─────────────────────────────────────────────────────────────
// CDefaultCommandExecutor — Layer 1 minimal
//   Move 만 본격, CastSkill/BasicAttack 은 reject/log only
//   Layer 2 (D-4) 에서 CastSkill/BasicAttack/PendingHit 본격
// ─────────────────────────────────────────────────────────────

std::unique_ptr<CDefaultCommandExecutor> CDefaultCommandExecutor::Create()
{
    return std::unique_ptr<CDefaultCommandExecutor>(new CDefaultCommandExecutor());
}

void CDefaultCommandExecutor::ExecuteCommand(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    if (cmd.issuerEntity == NULL_ENTITY) return;

    // 죽은 엔티티 reject
    if (world.HasComponent<HealthComponent>(cmd.issuerEntity))
    {
        const auto& hp = world.GetComponent<HealthComponent>(cmd.issuerEntity);
        if (hp.bIsDead) return;
    }

    switch (cmd.kind)
    {
        case eCommandKind::Move:        HandleMove(world, tc, cmd);        break;
        case eCommandKind::CastSkill:   HandleCastSkill(world, tc, cmd);   break;
        case eCommandKind::BasicAttack: HandleBasicAttack(world, tc, cmd); break;
        case eCommandKind::LevelSkill:  HandleLevelSkill(world, tc, cmd);  break;
        case eCommandKind::BuyItem:     HandleBuyItem(world, tc, cmd);     break;
        default: break;
    }
}

// ★ Layer 1 — MoveTargetComponent 자동 보장 + target 저장
void CDefaultCommandExecutor::HandleMove(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;

    if (!world.HasComponent<MoveTargetComponent>(cmd.issuerEntity))
        world.AddComponent<MoveTargetComponent>(cmd.issuerEntity, MoveTargetComponent{});

    auto& mt = world.GetComponent<MoveTargetComponent>(cmd.issuerEntity);
    mt.target = cmd.groundPos;
    mt.bHasTarget = true;
}

// ★ Layer 1 = "cooldown-only accept" (reject 가 아님 — tick chain smoke 검증용)
//   Codex 보정 (P2-5): doc 가 reject/log only 라고 했지만 실제로는 cooldown 을 set
//   하는 authoritative state 변경. 의미를 "cooldown-only accept" 로 정리.
//   시각적 cast / damage / hook dispatch 는 Layer 2 (D-4) 에서 본격.
void CDefaultCommandExecutor::HandleCastSkill(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;

    if (!world.HasComponent<SkillStateComponent>(cmd.issuerEntity)) return;
    if (cmd.slot >= 5) return;

    auto& sk = world.GetComponent<SkillStateComponent>(cmd.issuerEntity);
    if (sk.slots[cmd.slot].cooldownRemaining > 0.f) return;

    // Layer 1 cooldown 만 set — snapshot 으로 client 에 cooldown 흐름 노출 (시각 검증)
    sk.slots[cmd.slot].cooldownRemaining = 6.f;   // placeholder — Layer 2 에서 SkillDef.cooldownSec

    // Layer 2 추가:
    //   - ActiveCastComponent 추가 (windup/hit/recovery 단계 진행)
    //   - GameplayHookRegistry::Dispatch(onCastAcceptedHookId, ctx)
    //   - EventBatch 에 CastEvent emit
}

// ★ Layer 1 = reject/log only. Layer 2 에서 본격
void CDefaultCommandExecutor::HandleBasicAttack(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)world;
    (void)tc;
    (void)cmd;

    // Layer 2:
    //   - target 유효성 + 사거리 + cooldown 검증
    //   - PendingHitComponent schedule
    //   - DamageRequestComponent emit (hit time 도달 시)
}

// ★ Codex 보정 (P2-6): 직접 ranks[slot] += 1 은 도메인 규칙 (pointsAvailable / R 최대
//   3랭크 / BA slot 0 금지) 을 우회 → 기존 CSkillRankSystem::TryLevelSkill(world, e, slot)
//   사용. bool 반환은 무시해도 OK (Layer 1 = best-effort, Layer 2 에서 실패 reject log).
void CDefaultCommandExecutor::HandleLevelSkill(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)tc;

    if (cmd.slot >= 5) return;
    (void)CSkillRankSystem::TryLevelSkill(world, cmd.issuerEntity, cmd.slot);
}

void CDefaultCommandExecutor::HandleBuyItem(CWorld& world, const TickContext& tc,
    const GameCommand& cmd)
{
    (void)world;
    (void)tc;
    (void)cmd;

    // Layer 2 — Shop 시스템 통합
}

// ★ BuildServerCommand — sessionId → controlledEntity 결정 (issuer spoof 방지)
GameCommand BuildServerCommand(const GameCommandWire& wire,
    uint32_t sessionId, EntityID controlledEntity,
    const EntityIdMap& map)
{
    (void)sessionId;

    GameCommand cmd{};
    cmd.kind          = wire.kind;
    cmd.issuerEntity  = controlledEntity;   // ★ wire 가 아닌 server 결정값
    cmd.issuedAtTick  = 0;                   // GameRoom 이 채움
    cmd.sequenceNum   = wire.sequenceNum;
    cmd.slot          = wire.slot;
    cmd.targetEntity  = (wire.targetNet != NULL_NET_ENTITY)
                          ? map.FromNet(wire.targetNet)
                          : NULL_ENTITY;
    cmd.groundPos     = wire.groundPos;
    cmd.direction     = wire.direction;
    cmd.itemId        = wire.itemId;
    return cmd;
}
```

---

## 3. 합격 게이트

### 3.1 D-0A 합격
- ✅ Visual Studio 솔루션 reload → Server 프로젝트가 추가된 cpp/h 12개 인식
- ✅ AdditionalIncludeDirectories 에 Shared 경로 포함

### 3.2 D-0B 합격 (★ Codex P3 보정 — 실제 파일 수 10개)
- ✅ **10 신규 파일** 생성:
  1. `Shared/GameSim/Components/MoveTargetComponent.h` (1)
  2. `Shared/GameSim/Systems/MoveSystem.h` + `MoveSystem.cpp` (2)
  3. `Shared/GameSim/Systems/SkillCooldownSystem.h` + `SkillCooldownSystem.cpp` (2)
  4. `Shared/GameSim/Systems/DamageQueueSystem.h` + `DamageQueueSystem.cpp` (2)
  5. `Shared/GameSim/Systems/DeathSystem.h` + `DeathSystem.cpp` (2)
  6. `Shared/GameSim/Systems/CommandExecutor.cpp` (1, 헤더 `ICommandExecutor.h` 는 기존)
  - 총 **10 파일** (5 logical units × 평균 2 파일)
- ✅ Server 빌드 시 LNK2019 0건 (`/p:MultiProcessorCompilation=false /maxcpucount:1`)
- ✅ static_assert (`MoveTargetComponent` trivially_copyable) 통과

### 3.3 통합 합격 (D-1G 진입 가능)
- ✅ `CGameRoom::Phase_SimulationSystems` 가 `CMoveSystem::Execute / CSkillCooldownSystem::Execute / CDamageQueueSystem::Execute / CDeathSystem::Execute` 호출 시 컴파일 OK
- ✅ `CDefaultCommandExecutor::Create()` 가 호출 가능

---

## 4. 위험 / 디버깅 메모 (★ Codex 1차 검토 후 갱신)

| 위험 | 완화 / 검증 결과 |
|---|---|
| `SkillStateComponent.slots[5]` 구조 | ✅ 검증 완료 — `GameplayComponents.h` 의 `SkillSlotRuntime { cooldownRemaining, currentStage, stageWindow }` 와 일치 |
| `HealthComponent` 필드명 | ✅ Codex P1-1 — 실제 `fCurrent / fMaximum / bIsDead` (CoreComponents.h:18). plan 전문 보정 완료 |
| `DeathSystem` cascade 가 StatSystem/DamagePipeline 의 bIsDead 와 race | ✅ Codex P1-2 — `!hp.bIsDead` guard 제거. `hp.fCurrent <= 0.f` 이면 항상 cascade |
| `TransformComponent.SetPosition` API | ✅ 검증 완료 — `SetPosition(Vec3)` / `(x,y,z)` 둘 다 존재 + dirty flag set |
| `MoveSystem` 이 GetWorldPosition() 호출 | ✅ Codex P1-3 — phase chain 에 CTransformSystem 미포함 → stale. `GetLocalPosition()` 으로 보정 (root entity 가정) |
| `MoveTargetComponent.h` 가 Engine_Defines 끌어옴 | ✅ Codex P1-4 — `WintersTypes.h` + `WintersMath.h` + `<type_traits>` 만 사용 |
| `HandleCastSkill` 문구 vs 동작 불일치 | ✅ Codex P2-5 — "cooldown-only accept" 로 의미 정리 |
| `HandleLevelSkill` 직접 증가 (도메인 규칙 우회) | ✅ Codex P2-6 — `CSkillRankSystem::TryLevelSkill` 사용 |
| `DeterministicEntityIterator` 시그니처 | ✅ 검증 완료 — `template<TComponent> CollectSorted(CWorld&)` |

---

## 5. 한 줄

**D-0 = Server.vcxproj 에 Shared cpp 12개 명시 편입 + ServerSimSubset 5종 (Move/SkillCooldown/DamageQueue stub/Death) + MoveTargetComponent + CommandExecutor cpp 본격 박제 (총 10 신규 파일). Layer 1 = Move 본격 + CastSkill cooldown-only accept + BasicAttack reject/log only. Layer 2 진입 시 ActiveCastComponent + EventBatch 추가.**

---

## 부록 A — Codex 1차 검토 반영 매핑

| # | Findings | 위치 | 적용 patch |
|---|---|---|---|
| P1-1 | `hp.current` 필드명 오류 | DeathSystem.cpp §2.9 | `current/maxValue → fCurrent/fMaximum` |
| P1-2 | DeathSystem cascade guard 가 StatSystem/DamagePipeline 의 bIsDead 와 race | DeathSystem.cpp §2.9 | `!hp.bIsDead` guard 제거 + wasDead 만 보관 |
| P1-3 | `GetWorldPosition()` stale (CTransformSystem 미포함) | MoveSystem.cpp §2.3 | `GetLocalPosition()` 으로 보정 + Layer 2 복원 노트 |
| P1-4 | MoveTargetComponent.h 가 `Engine_Defines` 끌어옴 + `<type_traits>` 누락 | MoveTargetComponent.h §2.1 | `WintersTypes.h` + `WintersMath.h` + `<type_traits>` |
| P2-5 | HandleCastSkill 문구 vs 동작 불일치 | CommandExecutor.cpp §2.10 | "cooldown-only accept" 로 의미 정리 |
| P2-6 | HandleLevelSkill 이 도메인 규칙 우회 | CommandExecutor.cpp §2.10 | `CSkillRankSystem::TryLevelSkill(world, e, slot)` 사용 |
| P3 | "6 신규 파일" 카운트 오류 | §3.2 합격 게이트 | "10 신규 파일" + breakdown 명시 |
