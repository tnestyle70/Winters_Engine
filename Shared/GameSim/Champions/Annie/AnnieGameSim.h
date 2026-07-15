#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

namespace AnnieGameSim
{
	bool_t CanCastDisintegrate(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
}

