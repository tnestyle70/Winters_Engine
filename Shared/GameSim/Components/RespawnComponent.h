#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

struct RespawnComponent
{
    f32_t respawnTimer = 0.f;
    f32_t respawnDelay = 3.f;
    Vec3 spawnPos{};
    bool_t bPending = false;
};

static_assert(std::is_trivially_copyable_v<RespawnComponent>,
    "RespawnComponent must remain POD for deterministic server simulation.");
