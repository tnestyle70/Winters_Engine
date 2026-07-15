#pragma once

#include "Shared/GameSim/Definitions/ExperienceDef.h"
#include "WintersTypes.h"

enum class eRewardSourceKind : u8_t
{
    Unknown = 0,
    Champion,
    Minion,
    Turret,
    Structure,
    Jungle,
};

enum class eMinionRewardKind : u8_t
{
    Melee = 0,
    Ranged = 1,
    Siege = 2,
    Super = 3,
};

// 정글 보상 subKind 규약 — JungleComponent.subKind(0=Baron, 1=Dragon, 2+=소형 캠프)와 정렬.
// DamageQueueSystem kJungleSubBaron/kJungleSubDragon 과 동일한 번호를 쓴다.
inline constexpr u8_t kJungleRewardSubBaron = 0u;
inline constexpr u8_t kJungleRewardSubEpic = 1u;
inline constexpr u8_t kJungleRewardSubSmall = 2u;

struct GoldRewardDef
{
    f32_t killerGold = 0.f;
    f32_t assistGold = 0.f;
    f32_t teamGold = 0.f;
    f32_t globalGold = 0.f;
    f32_t firstBloodBonusGold = 0.f;
    f32_t goldGrowthAmount = 0.f;
    f32_t goldGrowthIntervalSec = 0.f;
    f32_t maxKillerGold = 0.f;
};

struct RewardDef
{
    eRewardSourceKind sourceKind = eRewardSourceKind::Unknown;
    u8_t subKind = 0;
    ExperienceRewardDef experience{};
    GoldRewardDef gold{};
};
