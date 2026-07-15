#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace KindredGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	bool_t CanCastLambsRespite(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		const Vec3& center);
	f32_t ResolveUltimateDurationSec(CWorld& world, const TickContext& tc, EntityID caster);
	f32_t ConsumeBasicAttackDamage(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target,
		eTeam casterTeam,
		f32_t baseDamage);
}
