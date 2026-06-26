#pragma once

#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionGameData.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

namespace ChampionGameDataDB
{
    u32_t GetSchemaVersion();
    u32_t GetBuildHash();

    const ChampionGameData* FindChampion(eChampion champion);
    const ChampionGameDataSkill* FindSkill(eChampion champion, u8_t slot);

    ChampionStatsDef ResolveStats(eChampion champion);
    StatComponent BuildStat(eChampion champion, u8_t level = 1);

    f32_t ResolveSkillRange(eChampion champion, u8_t slot);
    f32_t ResolveSkillCooldown(eChampion champion, u8_t slot);
    ChampionSkillTimingDefaults ResolveSkillTiming(eChampion champion, u8_t slot, u8_t stage = 1);
    ChampionBasicAttackTimingDefaults ResolveBasicAttackTiming(eChampion champion);

    u64_t ResolveBasicAttackWindupTicks(eChampion champion);
    u64_t ResolveSkillActionLockTicks(eChampion champion, u8_t slot, u8_t stage = 1);

    bool_t IsSkillTwoStage(eChampion champion, u8_t slot);
    f32_t ResolveSkillStageWindowSec(eChampion champion, u8_t slot);

    const ChampionGameDataPassiveDash* FindPassiveDash(eChampion champion);
    f32_t ResolvePassiveDashDistance(eChampion champion);
    f32_t ResolvePassiveDashDurationSec(eChampion champion);
    f32_t ResolvePassiveDashInputGraceSec(eChampion champion);
    u64_t ResolvePassiveDashInputGraceTicks(eChampion champion);
}
