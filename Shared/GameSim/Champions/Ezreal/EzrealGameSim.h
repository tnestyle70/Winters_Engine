#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

class CWorld;
struct DamageRequest;
struct SkillProjectileComponent;
struct TickContext;

namespace EzrealGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);

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
