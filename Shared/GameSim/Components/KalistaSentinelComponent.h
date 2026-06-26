#pragma once

#include "../Definitions/LoLMatchContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

#include <type_traits>

struct KalistaSentinelComponent
{
    EntityID owner = NULL_ENTITY;
    eTeam team = eTeam::Neutral;
    Vec3 start{};
    Vec3 end{};
    Vec3 forward{ 0.f, 0.f, 1.f };
    f32_t elapsedSec = 0.f;
    f32_t lifetimeSec = 12.f;
    f32_t patrolSpeed = 3.5f;
    f32_t sightRange = 10.f;
    f32_t radius = 0.45f;
    f32_t halfAngleCos = 0.8660254f;
};

static_assert(std::is_trivially_copyable_v<KalistaSentinelComponent>,
    "KalistaSentinelComponent must be trivially copyable for GameSim determinism.");
