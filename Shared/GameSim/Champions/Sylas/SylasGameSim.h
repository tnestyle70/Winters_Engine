#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"

class CWorld;
struct TickContext;

namespace SylasGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	bool_t CanHijackUltimate(CWorld& world, const TickContext& tc, EntityID caster, EntityID target);
	void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
}
