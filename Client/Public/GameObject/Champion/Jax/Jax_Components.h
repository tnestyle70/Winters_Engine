#pragma once

#include "Defines.h"

struct JaxStateComponent
{
    bool_t bEmpowerActive = false;
    f32_t fEmpowerTimer = 0.f;
    f32_t fEmpowerWindowSec = 10.f;
    f32_t fEmpowerDamageBonus = 40.f;

    bool_t bCounterActive = false;
    f32_t fCounterTimer = 0.f;
    f32_t fCounterWindowSec = 2.0f;

    bool_t bUltActive = false;
    f32_t fUltTimer = 0.f;
    f32_t fUltDurationSec = 8.f;
    u8_t ultAttackCounter = 0;
    f32_t fUltAOEDamage = 100.f;
    f32_t fUltAOERadius = 2.0f;
};
