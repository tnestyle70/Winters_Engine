#pragma once

#include "WintersTypes.h"

struct NavigationControlComponent
{
    bool_t bMovementBlocked = false;
    bool_t bUseReverseFacing = false;
    bool_t bChaseFallbackEnabled = false;
    bool_t bThrottleRepath = false;
    f32_t fMoveSpeedMul = 1.f;
};
