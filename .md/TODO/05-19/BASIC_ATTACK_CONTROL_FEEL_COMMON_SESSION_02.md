Session - 2 BasicAttack impact tick과 우클릭 cancel/run 전환을 서버 GameSim 기준으로 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CombatActionSystem.h

새 파일:

```cpp
#pragma once

#include "Shared/GameSim/Systems/ICommandExecutor.h"

class CWorld;

class CCombatActionSystem final
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CCombatActionSystem() = delete;
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CombatActionSystem.cpp

새 파일:

```cpp
#include "Shared/GameSim/Systems/CombatActionSystem.h"

#include "Shared/GameSim/Champions/AsheGameSim.h"
#include "Shared/GameSim/Champions/FioraGameSim.h"
#include "Shared/GameSim/Champions/JaxGameSim.h"
#include "Shared/GameSim/Champions/KindredGameSim.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue.h"

#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/World.h"
#include "WintersMath.h"

#include <algorithm>

namespace
{
    eTeam ResolveTeam(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).team;
        if (world.HasComponent<MinionComponent>(entity))
            return world.GetComponent<MinionComponent>(entity).team;
        if (world.HasComponent<StructureComponent>(entity))
            return world.GetComponent<StructureComponent>(entity).team;
        return eTeam::Neutral;
    }

    eChampion ResolveChampion(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).id;
        if (world.HasComponent<StatComponent>(entity))
            return world.GetComponent<StatComponent>(entity).championId;
        return eChampion::NONE;
    }

    bool_t IsAliveForBasicAttackImpact(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    f32_t ResolveBasicAttackDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam sourceTeam)
    {
        f32_t damage = 55.f;
        if (world.HasComponent<StatComponent>(source))
        {
            const auto& stat = world.GetComponent<StatComponent>(source);
            if (stat.ad > 0.f)
                damage = stat.ad;
        }

        const eChampion champion = ResolveChampion(world, source);
        if (champion == eChampion::FIORA)
            return FioraGameSim::ConsumeBasicAttackDamage(world, source, damage);
        if (champion == eChampion::JAX)
            return JaxGameSim::ConsumeBasicAttackDamage(
                world, source, target, sourceTeam, damage);
        if (champion == eChampion::ASHE)
            return AsheGameSim::ConsumeBasicAttackDamage(
                world, source, target, sourceTeam, damage);
        if (champion == eChampion::KINDRED)
            return KindredGameSim::ConsumeBasicAttackDamage(
                world, source, target, sourceTeam, damage);

        return damage;
    }

    u32_t BuildGenericEffectId(CWorld& world, EntityID entity, u8_t slot)
    {
        u32_t champion = 0;
        if (world.HasComponent<ChampionComponent>(entity))
            champion = static_cast<u32_t>(world.GetComponent<ChampionComponent>(entity).id);
        return (champion << 8) | static_cast<u32_t>(slot);
    }

    u32_t BuildBasicAttackEffectId(CWorld& world, EntityID entity)
    {
        const eChampion champion = ResolveChampion(world, entity);
        const u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);

        u32_t effectId = 0;
        switch (champion)
        {
        case eChampion::NONE:
        case eChampion::END:
        case eChampion::IRELIA:
        case eChampion::YASUO:
            break;
        default:
            effectId = MakeGameplayHookId(champion, GameplayHookVariant::BA_CastFrame);
            break;
        }

        return effectId != 0 ? effectId : BuildGenericEffectId(world, entity, slot);
    }

    Vec3 ResolveEventPosition(CWorld& world, EntityID source, EntityID target)
    {
        if (target != NULL_ENTITY && world.HasComponent<TransformComponent>(target))
            return world.GetComponent<TransformComponent>(target).GetPosition();
        if (source != NULL_ENTITY && world.HasComponent<TransformComponent>(source))
            return world.GetComponent<TransformComponent>(source).GetPosition();
        return {};
    }

    Vec3 ResolveDirection(CWorld& world, EntityID source, EntityID target)
    {
        if (source != NULL_ENTITY &&
            target != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(source) &&
            world.HasComponent<TransformComponent>(target))
        {
            const Vec3 sourcePos =
                world.GetComponent<TransformComponent>(source).GetPosition();
            const Vec3 targetPos =
                world.GetComponent<TransformComponent>(target).GetPosition();
            return WintersMath::DirectionXZ(sourcePos, targetPos);
        }

        return {};
    }

    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
    }

    bool_t TryAssignQueuedMoveTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID entity,
        const Vec3& requestedTarget)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return false;

        auto& moveTarget = world.HasComponent<MoveTargetComponent>(entity)
            ? world.GetComponent<MoveTargetComponent>(entity)
            : world.AddComponent<MoveTargetComponent>(entity, MoveTargetComponent{});

        ClearMovePath(moveTarget);

        const Vec3 pos =
            world.GetComponent<TransformComponent>(entity).GetLocalPosition();
        Vec3 target = requestedTarget;
        target.y = pos.y;
        Vec3 resolvedTarget = target;

        if (tc.pWalkable)
        {
            Vec3 waypoints[kMovePathMaxWaypoints]{};
            u16_t waypointCount = 0;
            if (!tc.pWalkable->TryBuildMovePath(
                pos,
                target,
                waypoints,
                kMovePathMaxWaypoints,
                waypointCount,
                resolvedTarget))
            {
                moveTarget.bHasTarget = false;
                return false;
            }

            moveTarget.pathCount = waypointCount;
            for (u16_t i = 0; i < waypointCount; ++i)
                moveTarget.pathWaypoints[i] = waypoints[i];
        }

        moveTarget.arriveRadius = MoveTargetComponent{}.arriveRadius;
        if (WintersMath::DistanceSqXZ(pos, resolvedTarget) <=
            moveTarget.arriveRadius * moveTarget.arriveRadius)
        {
            moveTarget.bHasTarget = false;
            ClearMovePath(moveTarget);
            return false;
        }

        moveTarget.target = resolvedTarget;
        moveTarget.pathIndex = 0;
        moveTarget.bHasTarget = true;
        return true;
    }

    bool_t ApplyBasicAttackImpact(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        CombatActionComponent& action)
    {
        const EntityID target = action.entityTarget;
        if (!IsAliveForBasicAttackImpact(world, source) ||
            !IsAliveForBasicAttackImpact(world, target) ||
            !world.HasComponent<HealthComponent>(target) ||
            !GameplayStateQuery::CanAttack(world, source) ||
            !GameplayStateQuery::CanBeTargetedBy(world, source, target))
        {
            return false;
        }

        const eTeam sourceTeam = ResolveTeam(world, source);
        const eTeam targetTeam = ResolveTeam(world, target);
        if (sourceTeam == targetTeam && sourceTeam != eTeam::Neutral)
            return false;

        const f32_t damage =
            ResolveBasicAttackDamage(world, source, target, sourceTeam);

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = damage;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);

        ReplicatedEventComponent effectEvent{};
        effectEvent.kind = eReplicatedEventKind::EffectTrigger;
        effectEvent.sourceEntity = source;
        effectEvent.targetEntity = target;
        effectEvent.effectId = BuildBasicAttackEffectId(world, source);
        effectEvent.slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        effectEvent.flags = static_cast<u16_t>(eSkillSlot::BasicAttack);
        effectEvent.position = ResolveEventPosition(world, source, target);
        effectEvent.direction = ResolveDirection(world, source, target);
        effectEvent.durationMs = 500;
        effectEvent.startTick = tc.tickIndex;
        EnqueueReplicatedEvent(world, effectEvent);
        return true;
    }
}

void CCombatActionSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities =
        DeterministicEntityIterator<CombatActionComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        if (!world.HasComponent<CombatActionComponent>(entity))
            continue;

        auto& action = world.GetComponent<CombatActionComponent>(entity);
        if (action.eKind != eCombatActionKind::BasicAttack)
        {
            if (tc.tickIndex >= action.uEndTick)
                world.RemoveComponent<CombatActionComponent>(entity);
            continue;
        }

        if (!action.bImpactIssued && tc.tickIndex >= action.uImpactTick)
        {
            if (!ApplyBasicAttackImpact(world, tc, entity, action))
            {
                world.RemoveComponent<CombatActionComponent>(entity);
                continue;
            }

            action.bImpactIssued = true;
        }

        if (action.bImpactIssued && action.bQueuedMove)
        {
            TryAssignQueuedMoveTarget(world, tc, entity, action.vQueuedMoveTarget);
            world.RemoveComponent<CombatActionComponent>(entity);
            continue;
        }

        if (tc.tickIndex >= action.uEndTick)
            world.RemoveComponent<CombatActionComponent>(entity);
    }
}
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

삭제할 코드:

```cpp
#include "Shared/GameSim/Champions/AsheGameSim.h"
#include "Shared/GameSim/Champions/FioraGameSim.h"
#include "Shared/GameSim/Champions/JaxGameSim.h"
#include "Shared/GameSim/Champions/KindredGameSim.h"
```

기존 코드:

```cpp
    bool_t HasActiveBasicAttackAction(CWorld& world, EntityID entity)
    {
        return entity != NULL_ENTITY &&
            world.HasComponent<CombatActionComponent>(entity) &&
            world.GetComponent<CombatActionComponent>(entity).eKind ==
                eCombatActionKind::BasicAttack;
    }
```

아래에 추가:

```cpp
    bool_t ConsumeMoveForCombatAction(CWorld& world, const TickContext& tc, const GameCommand& cmd)
    {
        if (!HasActiveBasicAttackAction(world, cmd.issuerEntity))
            return false;

        auto& action = world.GetComponent<CombatActionComponent>(cmd.issuerEntity);
        const bool_t bImpactDue = tc.tickIndex >= action.uImpactTick;
        if (!action.bImpactIssued && bImpactDue)
        {
            action.bQueuedMove = true;
            action.vQueuedMoveTarget = cmd.groundPos;
            action.vQueuedMoveDirection = cmd.direction;
            ClearMoveTarget(world, cmd.issuerEntity);
            return true;
        }

        if (!action.bImpactIssued)
        {
            if (action.eMovePolicy == eCombatActionMovePolicy::LockUntilEnd &&
                tc.tickIndex < action.uEndTick)
            {
                return true;
            }

            if (action.eMovePolicy == eCombatActionMovePolicy::QueueMoveUntilImpact)
            {
                action.bQueuedMove = true;
                action.vQueuedMoveTarget = cmd.groundPos;
                action.vQueuedMoveDirection = cmd.direction;
                ClearMoveTarget(world, cmd.issuerEntity);
                return true;
            }

            ClearCombatAction(world, cmd.issuerEntity);
            return false;
        }

        ClearCombatAction(world, cmd.issuerEntity);
        return false;
    }
```

기존 코드:

```cpp
    if (HasActiveBasicAttackAction(world, cmd.issuerEntity))
        ClearCombatAction(world, cmd.issuerEntity);
```

아래로 교체:

```cpp
    if (ConsumeMoveForCombatAction(world, tc, cmd))
        return;
```

삭제할 범위:
`CDefaultCommandExecutor::HandleBasicAttack(...)` 안의 `if (champion == eChampion::FIORA)` 줄부터 `EnqueueDamageRequest(world, request);` 줄까지 삭제한다.

삭제할 범위:
`CDefaultCommandExecutor::HandleBasicAttack(...)` 안의 `u32_t effectId = BuildPrimarySkillHookId(` 줄부터 `EnqueueReplicatedEvent(world, effectEvent);` 줄까지 삭제한다.

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/MoveSystem.h"

#include "Shared/GameSim/Components/HealthComponent.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/MoveSystem.h"

#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
```

기존 코드:

```cpp
    bool_t IsActionAnimationLocked(
        const StatComponent& stat,
        const NetAnimationComponent& anim,
        const TickContext& tc)
```

아래로 교체:

```cpp
    bool_t IsActionAnimationLocked(
        CWorld& world,
        EntityID entity,
        const StatComponent& stat,
        const NetAnimationComponent& anim,
        const TickContext& tc)
```

기존 코드:

```cpp
        if (tc.tickIndex < anim.animStartTick)
            return false;

        const u8_t slot = SlotFromActionAnimation(currentAnim);
```

아래로 교체:

```cpp
        if (tc.tickIndex < anim.animStartTick)
            return false;

        if (currentAnim == eNetAnimId::BasicAttack)
        {
            if (!world.HasComponent<CombatActionComponent>(entity))
                return false;

            const auto& action = world.GetComponent<CombatActionComponent>(entity);
            if (action.eKind != eCombatActionKind::BasicAttack)
                return false;
            if (action.bImpactIssued || tc.tickIndex >= action.uImpactTick)
                return false;

            return true;
        }

        const u8_t slot = SlotFromActionAnimation(currentAnim);
```

같은 파일의 모든 기존 호출:

```cpp
IsActionAnimationLocked(stat, anim, tc)
```

아래로 교체:

```cpp
IsActionAnimationLocked(world, entity, stat, anim, tc)
```

1-5. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Systems/BuffSystem.h"
#include "Shared/GameSim/Systems/DamageQueueSystem.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/BuffSystem.h"
#include "Shared/GameSim/Systems/CombatActionSystem.h"
#include "Shared/GameSim/Systems/DamageQueueSystem.h"
```

기존 코드:

```cpp
    CRecallSystem::Execute(m_world, tc);
    CWaypointPatrolSystem::Execute(m_world, tc);
    CMoveSystem::Execute(m_world, tc);
```

아래로 교체:

```cpp
    CRecallSystem::Execute(m_world, tc);
    CWaypointPatrolSystem::Execute(m_world, tc);
    CCombatActionSystem::Execute(m_world, tc);
    CMoveSystem::Execute(m_world, tc);
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
                else if (actionState.baseSeq != pNetAnim->actionSeq)
                {
                    actionState.baseSeq = pNetAnim->actionSeq;
                    actionState.bBaseAnimationPending = true;
                }
```

아래로 교체:

```cpp
                else if (actionState.baseSeq != pNetAnim->actionSeq)
                {
                    actionState.baseSeq = pNetAnim->actionSeq;
                    actionState.bBaseAnimationPending = true;
                    if (actionState.bActionActive)
                    {
                        actionState.bActionActive = false;
                        actionState.bTransitionActive = false;
                        actionState.transitionRemainingSec = 0.f;
                    }
                }
```

1-7. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatFormula.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Systems\CommandExecutor.cpp" />
```

아래로 교체:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatFormula.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatActionSystem.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Systems\CommandExecutor.cpp" />
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\AsheSimComponent.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\BotLaneAIComponent.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\FioraSimComponent.h" />
```

아래로 교체:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\AsheSimComponent.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\BotLaneAIComponent.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\CombatActionComponent.h" />
    <ClInclude Include="..\..\Shared\GameSim\Components\FioraSimComponent.h" />
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\CombatFormula.h" />
    <ClInclude Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.h" />
```

아래로 교체:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\CombatFormula.h" />
    <ClInclude Include="..\..\Shared\GameSim\Systems\CombatActionSystem.h" />
    <ClInclude Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.h" />
```

1-8. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters

기존 코드:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatFormula.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Shared\GameSim\Systems\CommandExecutor.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
```

아래로 교체:

```xml
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatFormula.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Shared\GameSim\Systems\CombatActionSystem.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
    <ClCompile Include="..\..\Shared\GameSim\Systems\CommandExecutor.cpp">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClCompile>
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\CombatFormula.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
```

아래로 교체:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Systems\CombatFormula.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Systems\CombatActionSystem.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Systems\DamageQueueSystem.h">
      <Filter>04. Shared\GameSim\Systems</Filter>
    </ClInclude>
```

기존 코드:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\BotLaneAIComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Components\AttackChaseComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
```

아래로 교체:

```xml
    <ClInclude Include="..\..\Shared\GameSim\Components\BotLaneAIComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Components\CombatActionComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
    <ClInclude Include="..\..\Shared\GameSim\Components\AttackChaseComponent.h">
      <Filter>04. Shared\GameSim\Components</Filter>
    </ClInclude>
```

2. 검증

검증 완료:

```powershell
git diff --check -- Shared/GameSim/Systems/CombatActionSystem.h Shared/GameSim/Systems/CombatActionSystem.cpp Shared/GameSim/Systems/CommandExecutor.cpp Shared/GameSim/Systems/MoveSystem.cpp Server/Private/Game/GameRoom.cpp Client/Private/Scene/Scene_InGame.cpp Server/Include/Server.vcxproj Server/Include/Server.vcxproj.filters .md/TODO/05-19/BASIC_ATTACK_CONTROL_FEEL_COMMON_SESSION_02.md
```

```powershell
git diff --check
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

수동 확인 필요:
- 우클릭 BA 직후 impact 전 이동 우클릭: 피해/FX 없이 Run으로 전환된다.
- impact tick 이후 이동 우클릭: 피해/FX는 한 번만 발생하고 BA backswing을 끊고 Run으로 전환된다.
- 이동 입력이 impact와 같은 tick에 들어와도 `bQueuedMove`로 보존되어 hit가 누락되지 않는다.
