#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

struct YoneSimComponent
{
    bool_t bSoulUnboundActive = false;
    bool_t bReturning = false;
    f32_t soulTimerSec = 0.f;
    f32_t soulDurationSec = 5.f;
    Vec3 anchorPosition{};
};

static_assert(std::is_trivially_copyable_v<YoneSimComponent>);
