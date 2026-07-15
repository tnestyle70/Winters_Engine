#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

class CWorld;
struct DamageRequest;
struct SkillProjectileComponent;
struct TickContext;

namespace AsheGameSim
{
    bool_t TryRegisterVolleyHit(
        CWorld& world,
        EntityID ledgerEntity,
        EntityID target);
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);

    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        f32_t baseDamage);

    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest);

    bool_t HandleProjectileHit(
        CWorld& world,
        const TickContext& tc,
        const SkillProjectileComponent& projectile,
        EntityID target,
        DamageRequest& outDamage,
        bool_t& outEnqueue);
}
