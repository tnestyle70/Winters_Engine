#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace GameplayStatus
{
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc,
		const TickContext& tc);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
	void RebuildGameplayState(CWorld& world, EntityID entity);
}
