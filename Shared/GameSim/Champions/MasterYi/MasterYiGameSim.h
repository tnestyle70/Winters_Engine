#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

class CWorld;
struct TickContext;

namespace MasterYiGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
}
