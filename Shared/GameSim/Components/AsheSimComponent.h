#pragma once

#include "WintersTypes.h"

#include <cstddef>
#include <type_traits>

struct AsheSimComponent
{
    u8_t focusStacks = 0;
    u8_t focusThreshold = 4;

    bool_t bQActive = false;
    u8_t reservedQTimerAlignment = 0u;
    f32_t qTimerSec = 0.f;
    f32_t qDurationSec = 4.f;
    f32_t qBonusDamage = 20.f;

    f32_t frostSlowDurationSec = 1.5f;
};

static_assert(std::is_trivially_copyable_v<AsheSimComponent>);
static_assert(sizeof(AsheSimComponent) == 20u);
static_assert(offsetof(AsheSimComponent, qTimerSec) == 4u);
