#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

static constexpr u16_t kMovePathMaxWaypoints = 512;
static constexpr u16_t kMoveFacingIntentLockTicks = 6;

struct MoveTargetComponent
{
    Vec3 target{};
    Vec3 pathWaypoints[kMovePathMaxWaypoints]{};
    Vec3 facingTarget{};
    Vec3 facingDirection{};
    f32_t arriveRadius = 0.15f;
    u32_t facingSequenceNum = 0;
    u16_t pathCount = 0;
    u16_t pathIndex = 0;
    u16_t facingLockTicks = 0;
    bool_t bHasTarget = false;
    bool_t bHasFacingTarget = false;
};

static_assert(std::is_trivially_copyable_v<MoveTargetComponent>,
    "MoveTargetComponent must be trivially_copyable for sim determinism.");
