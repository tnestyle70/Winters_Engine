#pragma once

#include "LoLMatchContext.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"
#include "SkillTypes.h"
#include "WintersTypes.h"

inline constexpr u32_t kChampionGameDataSchemaVersion = 1;
inline constexpr u8_t kChampionGameDataSkillSlotCount = 5;
inline constexpr u8_t kChampionGameDataSkillStageMax = 2;

struct ChampionGameDataSkillStage
{
    f32_t lockDurationSec = 0.6f;
};

struct ChampionGameDataSkill
{
    bool_t bValid = false;
    u8_t slot = 0;
    eTargetMode targetMode = eTargetMode::Self;
    u8_t stageCount = 1;
    f32_t stageWindowSec = 0.f;
    u8_t rankCount = 1;
    f32_t cooldownSecByRank[kSkillRankValueMax]{};
    f32_t manaCostByRank[kSkillRankValueMax]{};
    // Legacy client fallback only.
    f32_t cooldownSec = 0.f;
    f32_t rangeMax = 0.f;
    f32_t manaCost = 0.f;
    u16_t skillId = 0;
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t visualCueId = 0;
    ChampionGameDataSkillStage stages[kChampionGameDataSkillStageMax] = {};
};

struct ChampionGameDataPassiveDash
{
    bool_t bValid = false;
    f32_t distance = 0.f;
    f32_t durationSec = 0.f;
    f32_t inputGraceSec = 0.f;
};

struct ChampionGameData
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u32_t dataVersion = 1;
    u32_t authoringHash = 0;
    ChampionStatsDef stats{};
    ChampionGameDataSkill skills[kChampionGameDataSkillSlotCount] = {};
    ChampionGameDataPassiveDash passiveDash{};
};
