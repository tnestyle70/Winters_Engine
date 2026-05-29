#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <cstdint>

enum class eSpatialKind : u32_t
{
    None = 0,
    Champion = 1u << 0,
    Minion = 1u << 1,
    Turret = 1u << 2,
    JungleMob = 1u << 3,
    Inhibitor = 1u << 4,
    Nexus = 1u << 5,
    Ward = 1u << 6,
    Projectile = 1u << 7,
    Bush = 1u << 8,
    All = 0xFFFFFFFFu
};

struct SpatialAgentComponent
{
    eSpatialKind kind = eSpatialKind::None;
    u8_t team = 0;
    f32_t radius = 0.5f;
    i32_t cachedCellX = INT32_MIN;
    i32_t cachedCellZ = INT32_MIN;
};

inline constexpr u32_t SpatialMask(eSpatialKind kind)
{
    return static_cast<u32_t>(kind);
}

inline constexpr u32_t operator|(eSpatialKind a, eSpatialKind b)
{
    return static_cast<u32_t>(a) | static_cast<u32_t>(b);
}
