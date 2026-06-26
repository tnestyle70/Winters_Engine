#pragma once

#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"

struct SkillGameplayDef
{
    DefinitionKey key = kInvalidDefinitionKey;
    SkillDefId id{};
    ChampionDefId ownerChampionId{};
    u8_t slot = 0u;
    u16_t legacySkillId = 0u;
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillEffectSpec effect{};
    SummonPolicySpec summonPolicy{};
};
