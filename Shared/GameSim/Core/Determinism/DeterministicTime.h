#pragma once

#include "WintersTypes.h"

#include <cstdint>

struct DeterministicTime
{
    static constexpr f32_t kFixedDt = 1.f / 30.f;
    static constexpr uint64_t kTicksPerSecond = 30;

    static f64_t TickToSec(uint64_t tick)
    {
        return tick * static_cast<f64_t>(kFixedDt);
    }

    static uint64_t SecToTick(f64_t seconds)
    {
        return static_cast<uint64_t>(seconds * kTicksPerSecond);
    }
};
