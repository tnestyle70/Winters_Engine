#pragma once

#include "Engine_Defines.h"

struct FioraStateComponent
{
    bool   bBladeworkActive = false;
    f32_t  fBladeworkTimer  = 0.f;
    u8_t   bladeworkHitsRemaining = 0;
    f32_t  fBladeworkDamageBonus = 30.f;

    bool   bRiposteActive = false;
    f32_t  fRiposteTimer  = 0.f;
    f32_t  fRiposteWindowSec = 0.75f;

    bool      bRActive = false;
    f32_t     fRTimer  = 0.f;
    EntityID  rTargetEntity = NULL_ENTITY;
    f32_t     fRHealZoneRadius = 6.f;
};
