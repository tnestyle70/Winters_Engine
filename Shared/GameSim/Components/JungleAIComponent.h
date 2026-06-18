#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

#include <type_traits>

struct JungleAIComponent
{
	EntityID target = NULL_ENTITY;
	u32_t attackSequence = 0;
	bool_t bAggro = false;
};

static_assert(std::is_trivially_copyable_v<JungleAIComponent>,
	"JungleAIComponent must be trivially_copyable for sim determinism.");
