#pragma once

#include "WintersTypes.h"

struct ChampionExperienceCurveDef
{
    static constexpr u8_t kMaxChampionLevel = 18;

    f32_t requiredForNextLevel[kMaxChampionLevel + 1] = {};
};

struct ExperienceRewardDef
{
    f32_t killerXP = 0.f;
    f32_t nearbyXP = 0.f;
    f32_t teamXP = 0.f;
    f32_t shareRadius = 0.f;
    f32_t victimNextLevelXPFactor = 0.f;
};
