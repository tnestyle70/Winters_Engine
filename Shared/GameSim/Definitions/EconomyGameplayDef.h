#pragma once

#include "Shared/GameSim/Definitions/ExperienceDef.h"
#include "WintersTypes.h"

// 경제/보상/XP 진실값 정의.
// 값의 진실은 Data/LoL/ServerPrivate/Gameplay/EconomyGameplayDefs.json 이고,
// 아래 기본값은 팩 미장착(bValid=false) 시 레거시 상수 폴백과 동일해야 한다.

struct EconomyChampionKillRewardDef
{
    f32_t killerGold = 300.f;
    f32_t assistGold = 150.f;
    f32_t firstBloodBonusGold = 100.f;
    f32_t victimNextLevelXPFactor = 0.50f;
    f32_t shareRadius = 20.f;
};

struct EconomyMinionRewardDef
{
    f32_t soloXP = 0.f;
    f32_t sharedXP = 0.f;
    f32_t gold = 0.f;
    f32_t maxGold = 0.f;
    f32_t growthAmount = 0.f;
    f32_t growthIntervalSec = 0.f;
};

struct EconomyJungleRewardDef
{
    f32_t smallCampGold = 80.f;
    f32_t smallCampXP = 240.f;
    f32_t epicGold = 0.f;
    f32_t epicXP = 0.f;
    f32_t baronGold = 0.f;
    f32_t baronXP = 0.f;
};

struct ObjectiveGameplayDef
{
    f32_t teamGoldPerChampion = 2000.f;
    u8_t teamLevelGrant = 3u;
    f32_t buffDurationSec = 300.f;
    f32_t baronRecallDurationMultiplier = 0.5f;
    f32_t baronAuraRadius = 12.f;
    f32_t baronMinionHpMultiplier = 3.f;
    f32_t baronMinionAttackDamageMultiplier = 2.f;
    f32_t baronMinionScaleMultiplier = 2.f;
    f32_t elderAttackDamageMultiplier = 1.7f;
    f32_t elderBurnDurationSec = 3.f;
    f32_t elderBurnTickIntervalSec = 1.f;
    f32_t elderBurnTargetMaxHpRatioPerTick = 0.01f;
    f32_t elderExecuteThresholdRatio = 0.2f;
    f32_t blueManaRegenPerSec = 10.f;
    f32_t redHealthRegenPerSec = 10.f;
    f32_t redBurnDurationSec = 3.f;
    f32_t redBurnTickIntervalSec = 1.f;
    f32_t redBurnDamagePerTick = 10.f;
};

struct EconomyGameplayDef
{
    // ChampionExperienceCurveDef 와 같은 구조: [level] = 다음 레벨까지 필요 XP (0/18 = 0).
    f32_t xpRequiredForNextLevel[ChampionExperienceCurveDef::kMaxChampionLevel + 1] =
    {
        0.f,
        280.f, 380.f, 480.f, 580.f, 680.f, 780.f, 880.f, 980.f, 1080.f,
        1180.f, 1280.f, 1380.f, 1480.f, 1580.f, 1680.f, 1780.f, 1880.f,
        0.f,
    };

    EconomyChampionKillRewardDef championKill{};
    EconomyMinionRewardDef melee{ 61.75f, 80.60f, 21.f, 0.f, 0.f, 0.f };
    EconomyMinionRewardDef ranged{ 30.40f, 39.68f, 14.f, 0.f, 0.f, 0.f };
    EconomyMinionRewardDef siege{ 95.f, 124.f, 60.f, 90.f, 3.f, 90.f };
    EconomyMinionRewardDef super{ 95.f, 124.f, 60.f, 90.f, 3.f, 90.f };
    f32_t turretGold = 1500.f;
    f32_t turretTeamGold = 1000.f;
    EconomyJungleRewardDef jungle{};
    ObjectiveGameplayDef objectives{};

    u64_t passiveGoldStartTick = 3300ull;      // 110s * 30Hz (LoL 1:50)
    u64_t passiveGoldIntervalTicks = 30ull;    // 1s
    u32_t passiveGoldPerGrant = 2u;            // ~2.0g/s

    f32_t assistCreditWindowSec = 10.f;
    f32_t recallDurationSec = 6.f;

    // 팩 미장착 판별용: 코드젠/오버레이가 채운 팩만 true.
    bool_t bValid = false;
};
