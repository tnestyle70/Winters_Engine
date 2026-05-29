#pragma once

#include "Defines.h"

struct AsheStateComponent
{
    bool_t bQActive = false;
    f32_t fQTimer = 0.f;
    f32_t fQDurationSec = 4.f;
    u8_t focusStacks = 0;
    u8_t focusThreshold = 4;
    f32_t fQAttackSpeedBonus = 0.4f;
    f32_t fQDamageBonus = 20.f;

    u8_t volleyArrowCount = 8;
    f32_t fVolleyConeAngleDeg = 45.f;
    f32_t fVolleyRange = 9.0f;

    f32_t fHawkshotRange = 25.f;
    f32_t fHawkshotVisionDurationSec = 5.f;

    f32_t fCrystalArrowSpeed = 30.f;
    f32_t fCrystalArrowMaxDist = 200.f;
    f32_t fCrystalArrowStunMin = 1.5f;
    f32_t fCrystalArrowStunMax = 3.5f;

    f32_t fFrostSlowPercent = 0.15f;
    f32_t fFrostSlowDuration = 2.0f;
};
