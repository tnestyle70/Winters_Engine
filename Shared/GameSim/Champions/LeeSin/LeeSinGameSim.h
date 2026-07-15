#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

class CWorld;
struct TickContext;

namespace LeeSinGameSim
{
    bool_t CanCastSafeguard(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target);
    bool_t CanCastDragonRage(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target);
    void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	void ApplySonicWaveMark(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
}
