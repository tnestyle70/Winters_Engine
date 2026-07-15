#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

class CWorld;
struct TickContext;

namespace FioraGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	void CancelRuntime(CWorld& world, EntityID caster);
	bool_t CanCastGrandChallenge(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);
	f32_t ConsumeBasicAttackDamage(CWorld& world, EntityID caster, f32_t baseDamage);
}
