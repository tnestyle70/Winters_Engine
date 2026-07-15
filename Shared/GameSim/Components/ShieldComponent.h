#pragma once

#include "WintersTypes.h"

struct ShieldComponent
{
    f32_t fCurrent = 0.f;
    f32_t fMaximum = 0.f;
    u64_t uExpireTick = 0u;
};

static_assert(sizeof(ShieldComponent) == 16u);
