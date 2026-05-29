#pragma once

class CWorld;
struct TickContext;

namespace IreliaGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
}
