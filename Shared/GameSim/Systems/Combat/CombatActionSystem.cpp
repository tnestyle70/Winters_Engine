#include "Shared/GameSim/Systems/Combat/CombatActionSystem.h"

#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"

#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
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
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }

    bool_t TryAssignQueuedMoveTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID entity,
        const Vec3& requestedTarget,
        const Vec3& requestedDirection)
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
        moveTarget.facingTarget = requestedTarget;
        moveTarget.facingDirection = WintersMath::NormalizeXZ(
            requestedDirection,
            WintersMath::DirectionXZ(pos, requestedTarget, Vec3{}),
            0.0001f);
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = kMoveFacingIntentLockTicks;
        moveTarget.bHasFacingTarget =
            moveTarget.facingDirection.x != 0.f ||
            moveTarget.facingDirection.z != 0.f;
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

        if (ResolveChampion(world, source) == eChampion::IRELIA)
            return true;

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
            TryAssignQueuedMoveTarget(
                world,
                tc,
                entity,
                action.vQueuedMoveTarget,
                action.vQueuedMoveDirection);
            world.RemoveComponent<CombatActionComponent>(entity);
            continue;
        }

        if (tc.tickIndex >= action.uEndTick)
            world.RemoveComponent<CombatActionComponent>(entity);
    }
}
