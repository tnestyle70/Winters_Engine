#pragma once

#include "WintersTypes.h"

struct JaxSimComponent
{
    bool_t bEmpowerActive = false;
    f32_t empowerTimerSec = 0.f;
    f32_t empowerWindowSec = 4.f;
    f32_t empowerBonusDamage = 45.f;

    bool_t bCounterStrikeActive = false;
    f32_t counterTimerSec = 0.f;
    f32_t counterDurationSec = 2.f;
    f32_t counterRadius = 2.2f;
    f32_t counterDamage = 60.f;

    bool_t bUltActive = false;
    f32_t ultTimerSec = 0.f;
    f32_t ultDurationSec = 8.f;
    u8_t ultAttackCounter = 0;
    f32_t ultThirdHitDamage = 70.f;
};
