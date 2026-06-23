#pragma once

#include "GameContext.h"
#include "Shared/GameSim/Definitions/DefinitionIds.h"
#include "WintersTypes.h"

inline constexpr u8_t kChampionSkillSlotCount = 5u;

struct ChampionStatBlock
{
    f32_t baseHp = 600.f;
    f32_t hpPerLevel = 100.f;
    f32_t baseMana = 300.f;
    f32_t manaPerLevel = 50.f;
    f32_t baseAd = 60.f;
    f32_t adPerLevel = 3.5f;
    f32_t baseAp = 0.f;
    f32_t apPerLevel = 0.f;
    f32_t baseArmor = 30.f;
    f32_t armorPerLevel = 4.f;
    f32_t baseMr = 30.f;
    f32_t mrPerLevel = 1.25f;
    f32_t baseAttackSpeed = 0.60f;
    f32_t attackSpeedRatio = 0.60f;
    f32_t attackSpeedPerLevel = 0.025f;
    f32_t baseAttackRange = 5.5f;
    f32_t baseMoveSpeed = 5.f;
    f32_t navArriveRadius = 0.15f;
    f32_t spatialRadius = 0.75f;
    f32_t sightRange = 19.f;
};

struct PassiveDashGameplayDef
{
    bool_t bValid = false;
    f32_t distance = 0.f;
    f32_t durationSec = 0.f;
    f32_t inputGraceSec = 0.f;
};

struct ChampionGameplayDef
{
    DefinitionKey key = kInvalidDefinitionKey;
    ChampionDefId id{};
    eChampion legacyChampion = eChampion::END;
    u32_t dataVersion = 0u;
    u32_t authoringHash = 0u;
    ChampionStatBlock stats{};
    SkillDefId skillLoadout[kChampionSkillSlotCount] = {};
    PassiveDashGameplayDef passiveDash{};
};
