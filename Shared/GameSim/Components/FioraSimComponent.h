#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"
#include "ECS/Entity.h"

#include <type_traits>

struct FioraSimComponent
{
    bool_t bBladeworkActive = false;
    f32_t bladeworkTimerSec = 0.f;
    u8_t bladeworkHitsRemaining = 0;
    f32_t bladeworkDamageBonus = 30.f;

    bool_t bRiposteActive = false;
    f32_t riposteTimerSec = 0.f;
    f32_t riposteWindowSec = 0.75f;

    bool_t bGrandChallengeActive = false;
    f32_t grandChallengeTimerSec = 0.f;
    EntityID grandChallengeTarget = NULL_ENTITY;
};

static_assert(std::is_trivially_copyable_v<FioraSimComponent>);
