#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "GameContext.h"

struct ViegoSimComponent
{
    bool_t bMistActive = false;
    f32_t mistTimerSec = 0.f;
    f32_t mistDurationSec = 4.f;
    bool_t bMistHadTargetable = false;

    bool_t bPossessionActive = false;
    bool_t bPossessionPending = false;
    eChampion pendingPossessionChampion = eChampion::END;
    EntityID pendingPossessedTarget = NULL_ENTITY;
    f32_t possessionApplyTimerSec = 0.f;
    f32_t possessionApplyDelaySec = 0.72f;
    EntityID possessedTarget = NULL_ENTITY;
    f32_t possessionTimerSec = 0.f;
    f32_t possessionDurationSec = 6.f;
};
