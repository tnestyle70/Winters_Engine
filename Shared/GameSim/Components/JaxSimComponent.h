#pragma once

#include "WintersTypes.h"

#include <cstddef>

struct JaxSimComponent
{
    bool_t bEmpowerActive = false;
    u8_t reservedEmpowerPadding[3]{};
    f32_t empowerTimerSec = 0.f;
    f32_t empowerWindowSec = 5.f;
    f32_t empowerBonusDamage = 45.f;

    bool_t bCounterStrikeActive = false;
    u8_t reservedCounterStrikePadding[3]{};
    f32_t counterTimerSec = 0.f;
    f32_t counterDurationSec = 2.f;
    u8_t counterRank = 1;
    u8_t reservedCounterRankPadding[3]{};
    f32_t counterRadius = 2.2f;
    f32_t counterDamage = 60.f;

    bool_t bUltActive = false;
    u8_t reservedUltActivePadding[3]{};
    f32_t ultTimerSec = 0.f;
    f32_t ultDurationSec = 8.f;
    u8_t ultAttackCounter = 0;
    u8_t reservedUltAttackPadding[3]{};
    f32_t ultThirdHitDamage = 70.f;
    u8_t ultThirdHitThreshold = 3;
    u8_t reservedUltThresholdPadding[3]{};
};

static_assert(sizeof(JaxSimComponent) == 64u);
static_assert(offsetof(JaxSimComponent, empowerTimerSec) == 4u);
static_assert(offsetof(JaxSimComponent, counterTimerSec) == 20u);
static_assert(offsetof(JaxSimComponent, counterRadius) == 32u);
static_assert(offsetof(JaxSimComponent, ultTimerSec) == 44u);
static_assert(offsetof(JaxSimComponent, ultThirdHitDamage) == 56u);
static_assert(offsetof(JaxSimComponent, ultThirdHitThreshold) == 60u);
