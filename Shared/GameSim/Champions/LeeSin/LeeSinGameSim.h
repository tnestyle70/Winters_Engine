#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"

class CWorld;
struct TickContext;

namespace LeeSinGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	void ApplySonicWaveMark(CWorld& world, EntityID source, EntityID target);
}
