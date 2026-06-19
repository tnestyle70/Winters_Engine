#pragma once

#include "WintersTypes.h"

inline constexpr u8_t kSummonerSpellGameDataSlotCount = 2;

struct SummonerSpellGameData
{
    bool_t bValid = false;
    u16_t spellId = 0;
    f32_t rangeMax = 0.f;
    f32_t cooldownSec = 0.f;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
};
