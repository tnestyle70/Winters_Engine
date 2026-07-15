#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

#include <type_traits>

struct JungleAIComponent
{
	EntityID target = NULL_ENTITY;
	u32_t attackSequence = 0;
	bool_t bAggro = false;
	bool_t bHasAnchor = false;
	bool_t bReturning = false;
	f32_t aggroRange = 0.f;
	f32_t leashRange = 0.f;
	f32_t anchorX = 0.f;
	f32_t anchorZ = 0.f;
};

static_assert(std::is_trivially_copyable_v<JungleAIComponent>,
	"JungleAIComponent must be trivially_copyable for sim determinism.");
