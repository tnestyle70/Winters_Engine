#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace GameplayStatus
{
	bool_t TryApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	bool_t TryApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc,
		const TickContext& tc);
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc,
		const TickContext& tc);
	bool_t RemoveStatusEffect(CWorld& world, EntityID target,
		eStatusEffectId effectId, EntityID source);
	bool_t StartAirborneMotion(CWorld& world, EntityID target,
		EntityID source, const Vec3& landingPosition,
		f32_t durationSec, f32_t arcHeight,
		bool_t bGatherToLanding);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
	void TickForcedMotions(CWorld& world, const TickContext& tc);
	void ClearStatusEffects(CWorld& world, EntityID entity);
	void CleanseCrowdControlEffects(CWorld& world, EntityID entity);
	void RebuildGameplayState(CWorld& world, EntityID entity);
}
