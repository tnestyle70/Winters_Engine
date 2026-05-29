#pragma once

class CWorld;
struct TickContext;

namespace AnnieGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
}

