#pragma once

#include "Defines.h"
#include "WintersMath.h"

struct AnnieStateComponent
{
    u8_t stunStacks = 0;
    u8_t stunThreshold = 4;
    bool_t bNextStunReady = false;

    bool_t bEShieldActive = false;
    f32_t fEShieldTimer = 0.f;
    f32_t fEShieldDurationSec = 5.f;
    f32_t fEShieldAmount = 80.f;

    bool_t bTibbersActive = false;
    f32_t fTibbersTimer = 0.f;
    f32_t fTibbersDurationSec = 6.f;
    Vec3 vTibbersPos = { 0.f, 0.f, 0.f };
};
