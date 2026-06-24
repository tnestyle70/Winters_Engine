#pragma once

#include "ECS/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "WintersTypes.h"

struct ChampionAISkillRule
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    f32_t minRange = 0.f;
    f32_t score = 0.f;
};

enum class eChampionAIComboTargetMode : u8_t
{
    TargetEntity,
    AwayFromTarget,
    WardBehindTarget,
    LastOwnWard,
    SylasHijackTarget,
    SylasStolenUltimateTarget,
};

struct ChampionAIComboStep
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    u16_t itemId = 0;
    f32_t minRange = 0.f;
    f32_t maxRange = 0.f;
    f32_t selfHpMinRatio = 0.f;
    f32_t enemyHpMaxRatio = 1.f;
    u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity);
};

struct ChampionAIComboPlan
{
    ChampionAIComboStep steps[10]{};
    u8_t stepCount = 0;
};

struct ChampionAIProfile
{
    eChampion champion = eChampion::END;
    f32_t preferredRange = 1.5f;
    f32_t championScanRange = 6.f;
    f32_t minionScanRange = 10.f;
    f32_t structureScanRange = 18.f;
    f32_t leashRange = 14.f;
    f32_t aggression = 1.f;
    f32_t kiteBias = 0.f;
    f32_t retreatHpRatio = 0.35f;
    f32_t reengageHpRatio = 0.55f;
    f32_t minionPressureWeight = 1.f;
    f32_t turretRiskWeight = 1.f;
    f32_t lastHitWeight = 1.f;
    f32_t siegeWeight = 1.f;
    ChampionAISkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
};

const ChampionAIProfile& GetChampionAIProfile(eChampion champion);
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion);
