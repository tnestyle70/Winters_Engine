#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

class CWorld;
struct DamageRequest;
struct DamageResult;
struct StatusEffectApplyDesc;
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
	bool_t PrepareDamageRequest(CWorld& world, DamageRequest& request);
	void OnDamageResolved(
		CWorld& world,
		const TickContext& tc,
		const DamageRequest& request,
		const DamageResult& result);
	bool_t TryParryCrowdControl(
		CWorld& world,
		EntityID target,
		const StatusEffectApplyDesc& desc,
		const TickContext* pTickContext);
}
