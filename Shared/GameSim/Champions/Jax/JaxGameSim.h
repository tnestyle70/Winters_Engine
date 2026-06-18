#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace JaxGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);

    bool_t TryConsumeEmpowerForBasicAttack(CWorld& world, EntityID caster);

    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        u16_t actionFlags,
        f32_t baseDamage);
}
