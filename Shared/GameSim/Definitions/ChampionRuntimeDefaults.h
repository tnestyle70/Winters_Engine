#pragma once

#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "SkillTypes.h"
#include "WintersMath.h"

StatComponent BuildDefaultChampionStat(eChampion champion, u8_t level = 1);
ChampionStatsDef BuildDefaultChampionStatsDef(eChampion champion);
void RegisterDefaultChampionSkillScalingTables();
f32_t GetDefaultChampionSkillRange(eChampion champion, u8_t slot);
f32_t GetDefaultChampionSkillCooldown(eChampion champion, u8_t slot);

f32_t GetDefaultChampionVisualYawOffset(eChampion champion);
// Canonical yaw is for wire values, debug deltas, and comparisons.
// Do not store canonical yaw directly into Transform body rotation.
f32_t NormalizeChampionVisualYaw(f32_t yaw);
f32_t ResolveChampionVisualYawFromDirection(eChampion champion, const Vec3& direction);
// Near yaw is for continuous Transform body rotation state.
f32_t MakeChampionVisualYawNear(f32_t yaw, f32_t referenceYaw);
f32_t ResolveChampionVisualYawNear(eChampion champion, const Vec3& direction,
    f32_t referenceYaw);

struct ChampionSkillTimingDefaults
{
    f32_t lockDurationSec = 0.6f;
};

struct ChampionBasicAttackTimingDefaults
{
    f32_t fWindupSec = 0.25f;
    f32_t fActionDurationSec = 0.75f;
};

ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot);
ChampionSkillTimingDefaults GetDefaultChampionSkillTiming(eChampion champion, u8_t slot, u8_t stage);
ChampionBasicAttackTimingDefaults GetDefaultChampionBasicAttackTiming(eChampion champion);
bool_t IsDefaultChampionSkillTwoStage(eChampion champion, u8_t slot);
f32_t GetDefaultChampionSkillStageWindowSec(eChampion champion, u8_t slot);
u16_t EncodeSkillPlaybackRateQ8(f32_t playSpeed);
u64_t GetDefaultChampionBasicAttackWindupTicks(eChampion champion);
u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot);
u64_t GetDefaultChampionSkillActionLockTicks(eChampion champion, u8_t slot, u8_t stage);
