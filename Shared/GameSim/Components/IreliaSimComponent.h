#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

#include <type_traits>

struct IreliaSimComponent
{
	static constexpr u8_t kRMaxTrackedTargets = 16;

	Vec3 blade1Pos{};
	Vec3 blade2Pos{};
	Vec3 dashStartPos{};
	Vec3 dashEndPos{};
	Vec3 rWavePos{};
	Vec3 rWaveDir{};
	u64_t blade1Tick = 0;
	u64_t blade2Tick = 0;
	f32_t dashElapsedSec = 0.f;
	f32_t dashDurationSec = 0.f;
	f32_t rWaveTravelled = 0.f;
	f32_t rWallRemainingSec = 0.f;
	EntityID dashTarget = NULL_ENTITY;
	EntityID rHitTargets[kRMaxTrackedTargets]{};
	EntityID rWallTargets[kRMaxTrackedTargets]{};
	u8_t rRank = 0;
	u8_t rHitTargetCount = 0;
	u8_t rWallTargetCount = 0;
	bool_t bHasBlade1 = false;
	bool_t bHasBlade2 = false;
	bool_t bDashActive = false;
	bool_t bRWaveActive = false;
	bool_t bRWallActive = false;
};

static_assert(std::is_trivially_copyable_v<IreliaSimComponent>);
