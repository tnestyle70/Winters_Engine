#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace AsheGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);

    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        f32_t baseDamage);
}
