#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

static constexpr u8_t kWaypointPatrolMaxPoints = 8;

enum class eWaypointPatrolMode : u8_t
{
    PingPong,
    Loop,
};

struct WaypointPatrolComponent
{
    Vec3 points[kWaypointPatrolMaxPoints]{};
    u8_t pointCount = 0;
    u8_t currentIndex = 0;
    i8_t direction = 1;
    eWaypointPatrolMode mode = eWaypointPatrolMode::PingPong;
    f32_t arriveRadius = 0.35f;
    bool_t bActive = true;
};

static_assert(std::is_trivially_copyable_v<WaypointPatrolComponent>,
    "WaypointPatrolComponent must be trivially_copyable for sim determinism.");
