#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"

class CWorld;
struct TickContext;

namespace SylasGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	void ApplyChainHit(CWorld& world, EntityID source, EntityID target);
}