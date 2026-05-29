#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

#include <type_traits>

struct IreliaSimComponent
{
	Vec3 blade1Pos{};
	Vec3 blade2Pos{};
	Vec3 dashStartPos{};
	Vec3 dashEndPos{};
	u64_t blade1Tick = 0;
	u64_t blade2Tick = 0;
	f32_t dashElapsedSec = 0.f;
	f32_t dashDurationSec = 0.f;
	EntityID dashTarget = NULL_ENTITY;
	bool_t bHasBlade1 = false;
	bool_t bHasBlade2 = false;
	bool_t bDashActive = false;
};

static_assert(std::is_trivially_copyable_v<IreliaSimComponent>);
