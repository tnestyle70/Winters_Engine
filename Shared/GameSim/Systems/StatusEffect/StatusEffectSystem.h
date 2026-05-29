#pragma once

#include "ECS/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace GameplayStatus
{
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
	void RebuildGameplayState(CWorld& world, EntityID entity);
}