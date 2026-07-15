#pragma once

#include "../Definitions/LoLMatchContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
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

enum class eKalistaFateCallStage : uint8_t
{
    None = 0,
    Pulling,
    Carrying,
    Launching,
};

struct KalistaFateCallComponent
{
    EntityID entityCarried = NULL_ENTITY;
    eKalistaFateCallStage eStage = eKalistaFateCallStage::None;
    Vec3 vPullStart{};
    f32_t fPullElapsedSec = 0.f;
    f32_t fPullDurationSec = 0.25f;
    Vec3 vLaunchStart{};
    Vec3 vLaunchEnd{};
    f32_t fRemainingSec = 0.f;
    f32_t fLaunchElapsedSec = 0.f;
    f32_t fLaunchDurationSec = 0.35f;
    f32_t fCollisionRadius = 2.25f;
    f32_t fAirborneDurationSec = 0.75f;
    f32_t fAirborneArcHeight = 2.1f;
};

static_assert(std::is_trivially_copyable_v<KalistaSentinelComponent>,
    "KalistaSentinelComponent must be trivially copyable for GameSim determinism.");
static_assert(std::is_trivially_copyable_v<KalistaFateCallComponent>,
    "KalistaFateCallComponent must be trivially copyable for GameSim determinism.");
