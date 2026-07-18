#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

class CWorld;
struct TickContext;

namespace SylasGameSim
{
	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
	void ArmPassiveOnSkillCast(CWorld& world, const TickContext& tc, EntityID caster);
	bool_t TryConsumePassiveBasicAttack(CWorld& world, EntityID caster);
	void EnqueuePassiveBasicAttackAreaDamage(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID primaryTarget);
	bool_t CanHijackUltimate(CWorld& world, const TickContext& tc, EntityID caster, EntityID target);
	void ApplyChainHit(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
}
