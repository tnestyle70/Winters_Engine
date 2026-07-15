#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

namespace GarenGameSim
{
    void RegisterHooks();
    bool_t CanCastDemacianJustice(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target);
}
