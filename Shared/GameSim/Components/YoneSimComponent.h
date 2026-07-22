#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

#include <cstddef>
#include <type_traits>

struct YoneSimComponent
{
    bool_t bSoulUnboundActive = false;
    bool_t bReturning = false;
    u8_t sourceERank = 1u;
    u8_t reservedTimerAlignment = 0u;
    f32_t soulTimerSec = 0.f;
    f32_t soulDurationSec = 5.f;
    Vec3 anchorPosition{};
};

static_assert(std::is_trivially_copyable_v<YoneSimComponent>);
static_assert(sizeof(YoneSimComponent) == 24u);
static_assert(offsetof(YoneSimComponent, soulTimerSec) == 4u);

struct YoneSoulMarkComponent
{
    EntityID sourceEntity = NULL_ENTITY;
    f32_t storedPostMitigationDamage = 0.f;
    f32_t remainingSec = 0.f;
    u8_t sourceERank = 1u;
    u8_t reservedAlignment[3]{};
};

static_assert(std::is_trivially_copyable_v<YoneSoulMarkComponent>);
static_assert(sizeof(YoneSoulMarkComponent) == 16u);
static_assert(offsetof(YoneSoulMarkComponent, storedPostMitigationDamage) == 4u);
