#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/HealthComponent.h"

#include <algorithm>

namespace
{
    bool_t IsAliveGameplayTarget(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    bool_t TryResolveTeam(CWorld& world, EntityID entity, u8_t& outTeam)
    {
        const eTeam team = GameplayStateQuery::ResolveEntityTeam(world, entity);
        if (team == eTeam::TEAM_END)
            return false;

        outTeam = static_cast<u8_t>(team);
        return true;
    }

    bool_t IsEnemy(CWorld& world, EntityID observer, EntityID target)
    {
        u8_t observerTeam = 0;
        u8_t targetTeam = 0;
        if (!TryResolveTeam(world, observer, observerTeam) ||
            !TryResolveTeam(world, target, targetTeam))
        {
            return true;
        }
        return observerTeam != targetTeam;
    }
}

namespace GameplayStateQuery
{
    f32_t ResolveGameplayRadius(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<SpatialAgentComponent>(entity))
            return (std::max)(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);
        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return 1.2f;
        if (entity != NULL_ENTITY &&
            (world.HasComponent<MinionComponent>(entity) ||
                world.HasComponent<MinionStateComponent>(entity)))
        {
            return 0.5f;
        }
        if (entity != NULL_ENTITY && world.HasComponent<StructureComponent>(entity))
            return 1.5f;
        if (entity != NULL_ENTITY && world.HasComponent<JungleComponent>(entity))
            return 1.25f;
        return 0.5f;
    }

    eTeam ResolveEntityTeam(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY)
            return eTeam::TEAM_END;

        if (world.HasComponent<SpatialAgentComponent>(entity))
        {
            const u8_t team = world.GetComponent<SpatialAgentComponent>(entity).team;
            if (team <= static_cast<u8_t>(eTeam::Neutral))
                return static_cast<eTeam>(team);
        }
        if (world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).team;
        if (world.HasComponent<MinionComponent>(entity))
            return world.GetComponent<MinionComponent>(entity).team;
        if (world.HasComponent<MinionStateComponent>(entity))
            return world.GetComponent<MinionStateComponent>(entity).team;
        if (world.HasComponent<StructureComponent>(entity))
            return world.GetComponent<StructureComponent>(entity).team;
        if (world.HasComponent<JungleComponent>(entity))
            return eTeam::Neutral;
        return eTeam::TEAM_END;
    }

    bool_t HasState(CWorld& world, EntityID entity, u32_t stateFlag)
    {
        return entity != NULL_ENTITY &&
            world.HasComponent<GameplayStateComponent>(entity) &&
            (world.GetComponent<GameplayStateComponent>(entity).stateFlags & stateFlag) != 0u;
    }

    bool_t CanMove(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotMoveFlag |
            kGameplayStateStunnedFlag;
        return !HasState(world, entity, kBlocked);
    }

    bool_t CanAttack(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotAttackFlag |
            kGameplayStateStunnedFlag |
            kGameplayStateDisarmedFlag;
        return !HasState(world, entity, kBlocked);
    }

    bool_t CanCast(CWorld& world, EntityID entity)
    {
        constexpr u32_t kBlocked =
            kGameplayStateCannotCastFlag |
            kGameplayStateStunnedFlag;
        return !HasState(world, entity, kBlocked);
    }

    bool_t CanBeSeenBy(CWorld& world, EntityID observer, EntityID target)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return false;
        if (observer == target)
            return true;
        if (observer != NULL_ENTITY && !IsEnemy(world, observer, target))
            return true;
        return !HasState(world, target, kGameplayStateInvisibleFlag);
    }

    bool_t CanBeTargetedBy(CWorld& world, EntityID observer, EntityID target)
    {
        if (!IsAliveGameplayTarget(world, target))
            return false;
        if (!world.HasComponent<TargetableTag>(target))
            return false;
        if (HasState(world, target, kGameplayStateUntargetableFlag))
            return false;
        return CanBeSeenBy(world, observer, target);
    }

    bool_t CanReceiveDamage(CWorld& world, EntityID source, EntityID target)
    {
        (void)source;
        return IsAliveGameplayTarget(world, target) &&
            !HasState(world, target, kGameplayStateUntargetableFlag);
    }

    bool_t CanReceiveProjectileHit(CWorld& world, EntityID source, EntityID target)
    {
        return CanReceiveDamage(world, source, target);
    }

    f32_t GetMoveSpeedMultiplier(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.HasComponent<GameplayStateComponent>(entity))
            return 1.f;

        const f32_t mul = world.GetComponent<GameplayStateComponent>(entity).fMoveSpeedMul;
        return mul > 0.f ? mul : 1.f;
    }
}
