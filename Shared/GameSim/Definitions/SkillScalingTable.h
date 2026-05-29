#pragma once

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "WintersTypes.h"

struct SkillScalingTable
{
    static constexpr u8_t kMaxRank = 5;

    u16_t scalingTableId = 0;
    u16_t skillId = 0;
    f32_t damage[kMaxRank] = {};
    f32_t cooldownSec[kMaxRank] = {};
    f32_t manaCost[kMaxRank] = {};
    f32_t adRatio[kMaxRank] = {};
    f32_t bonusAdRatio[kMaxRank] = {};
    f32_t apRatio[kMaxRank] = {};
    f32_t targetMaxHpRatio[kMaxRank] = {};
    f32_t targetMissingHpRatio[kMaxRank] = {};
    eDamageType damageType = eDamageType::Physical;
    u32_t flags = DamageFlag_None;
};
