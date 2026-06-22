#pragma once

#include "GameContext.h"
#include "WintersTypes.h"

inline constexpr u8_t kLoLPublicSkillHintSlotCount = 5;

struct LoLPublicSkillHintData
{
    bool_t bVisible = false;
    u8_t uSlot = 0;
    f32_t fRangeMax = 0.f;
    f32_t fCooldownSec = 0.f;
};

struct LoLPublicChampionHintData
{
    bool_t bVisible = false;
    eChampion eChampionId = eChampion::END;
    u32_t uDataVersion = 1;
    u32_t uPublicHash = 0;
    LoLPublicSkillHintData skills[kLoLPublicSkillHintSlotCount] = {};
};
