#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

class CWorld;
struct GameCommand;
struct TickContext;

namespace ZedGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	bool_t CanCastDeathMark(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);

	bool_t ApplyLivingShadowMove(CWorld& world, const TickContext& tc, GameCommand& cmd);
	bool_t TryGetShadowSource(CWorld& world, EntityID caster, Vec3& outPosition, Vec3& outDirection);
}
