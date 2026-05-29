#pragma once

#include "WintersTypes.h"

struct AsheSimComponent
{
    u8_t focusStacks = 0;
    u8_t focusThreshold = 4;

    bool_t bQActive = false;
    f32_t qTimerSec = 0.f;
    f32_t qDurationSec = 4.f;
    f32_t qBonusDamage = 20.f;

    f32_t frostSlowDurationSec = 1.5f;
};
