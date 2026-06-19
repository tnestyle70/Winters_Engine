#pragma once

#include "GameContext.h"
#include "SkillTypes.h"
#include "WintersTypes.h"

inline constexpr u8_t kSkillAtomStageMax = 2;

struct SkillSlotBinding
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u8_t slot = 0;
    u16_t skillId = 0;
};

struct SkillTargetSpec
{
    bool_t bValid = false;
    eTargetShape shape[kSkillAtomStageMax] = { eTargetShape::Self, eTargetShape::Self };
    eTargetResolvePolicy resolvePolicy = eTargetResolvePolicy::Direct;
};

struct SkillCostSpec
{
    f32_t manaCost = 0.f;
};

struct SkillCooldownSpec
{
    f32_t cooldownSec = 0.f;
};

struct SkillRangeSpec
{
    f32_t rangeMax = 0.f;
};

struct SkillStageSpec
{
    u8_t stageCount = 1;
    f32_t stageWindowSec = 0.f;
    f32_t lockDurationSec[kSkillAtomStageMax] = {};
};

struct SkillFacingSpec
{
    eSkillFacingMode mode[kSkillAtomStageMax] =
    {
        eSkillFacingMode::None,
        eSkillFacingMode::None,
    };
};

struct SkillEffectSpec
{
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t replicatedCueId = 0;
};

struct SkillGameAtomBundle
{
    bool_t bValid = false;
    SkillSlotBinding slot{};
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillEffectSpec effect{};
};
