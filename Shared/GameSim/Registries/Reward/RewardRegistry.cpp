#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"

#include "Shared/GameSim/Definitions/EconomyGameplayDef.h"

namespace
{
    RewardDef MakeMinionReward(
        eMinionRewardKind kind,
        f32_t soloXP,
        f32_t sharedXP,
        f32_t gold,
        f32_t maxGold = 0.f,
        f32_t growthAmount = 0.f,
        f32_t growthIntervalSec = 0.f)
    {
        RewardDef reward{};
        reward.sourceKind = eRewardSourceKind::Minion;
        reward.subKind = static_cast<u8_t>(kind);
        reward.experience.nearbyXP = soloXP;
        reward.experience.teamXP = sharedXP;
        reward.experience.shareRadius = 20.f;
        reward.gold.killerGold = gold;
        reward.gold.maxKillerGold = maxGold;
        reward.gold.goldGrowthAmount = growthAmount;
        reward.gold.goldGrowthIntervalSec = growthIntervalSec;
        return reward;
    }

    RewardDef MakeChampionReward(
        f32_t killerGold,
        f32_t assistGold,
        f32_t firstBloodBonusGold,
        f32_t victimNextLevelXPFactor,
        f32_t shareRadius)
    {
        RewardDef reward{};
        reward.sourceKind = eRewardSourceKind::Champion;
        reward.experience.shareRadius = shareRadius;
        reward.experience.victimNextLevelXPFactor = victimNextLevelXPFactor;
        reward.gold.killerGold = killerGold;
        reward.gold.assistGold = assistGold;
        reward.gold.firstBloodBonusGold = firstBloodBonusGold;
        return reward;
    }

    RewardDef MakeTurretReward(f32_t killerGold)
    {
        // 봇 가치판단(ChampionAIValuation::GetTurretGoldValue)이 이 레지스트리를
        // 조회하므로 실보상과 갈라지지 않는다.
        RewardDef reward{};
        reward.sourceKind = eRewardSourceKind::Turret;
        reward.gold.killerGold = killerGold;
        return reward;
    }

    RewardDef MakeJungleReward(u8_t subKind, f32_t gold, f32_t xp)
    {
        RewardDef reward{};
        reward.sourceKind = eRewardSourceKind::Jungle;
        reward.subKind = subKind;
        reward.experience.nearbyXP = xp;
        reward.experience.shareRadius = 20.f;
        reward.gold.killerGold = gold;
        return reward;
    }
}

CRewardRegistry& CRewardRegistry::Instance()
{
    static CRewardRegistry s_inst;
    return s_inst;
}

CRewardRegistry::CRewardRegistry()
{
    LoadDefaultSummonersRift();
}

void CRewardRegistry::Reset()
{
    m_ExperienceCurve = ChampionExperienceCurveDef{};
    m_Rewards.clear();
}

void CRewardRegistry::LoadDefaultSummonersRift()
{
    Reset();

    ChampionExperienceCurveDef curve{};
    curve.requiredForNextLevel[1] = 280.f;
    curve.requiredForNextLevel[2] = 380.f;
    curve.requiredForNextLevel[3] = 480.f;
    curve.requiredForNextLevel[4] = 580.f;
    curve.requiredForNextLevel[5] = 680.f;
    curve.requiredForNextLevel[6] = 780.f;
    curve.requiredForNextLevel[7] = 880.f;
    curve.requiredForNextLevel[8] = 980.f;
    curve.requiredForNextLevel[9] = 1080.f;
    curve.requiredForNextLevel[10] = 1180.f;
    curve.requiredForNextLevel[11] = 1280.f;
    curve.requiredForNextLevel[12] = 1380.f;
    curve.requiredForNextLevel[13] = 1480.f;
    curve.requiredForNextLevel[14] = 1580.f;
    curve.requiredForNextLevel[15] = 1680.f;
    curve.requiredForNextLevel[16] = 1780.f;
    curve.requiredForNextLevel[17] = 1880.f;
    curve.requiredForNextLevel[18] = 0.f;
    SetExperienceCurve(curve);

    AddReward(MakeChampionReward(300.f, 150.f, 100.f, 0.50f, 20.f));
    AddReward(MakeMinionReward(eMinionRewardKind::Melee, 61.75f, 80.60f, 21.f));
    AddReward(MakeMinionReward(eMinionRewardKind::Ranged, 30.40f, 39.68f, 14.f));
    AddReward(MakeMinionReward(eMinionRewardKind::Siege, 95.f, 124.f, 60.f, 90.f, 3.f, 90.f));
    AddReward(MakeMinionReward(eMinionRewardKind::Super, 95.f, 124.f, 60.f, 90.f, 3.f, 90.f));
    AddReward(MakeTurretReward(250.f));
    AddReward(MakeJungleReward(kJungleRewardSubBaron, 300.f, 600.f));
    AddReward(MakeJungleReward(kJungleRewardSubEpic, 150.f, 250.f));
    AddReward(MakeJungleReward(kJungleRewardSubSmall, 35.f, 75.f));
}

void CRewardRegistry::LoadFromEconomyDef(const EconomyGameplayDef& economy)
{
    Reset();

    ChampionExperienceCurveDef curve{};
    for (u8_t level = 0; level <= ChampionExperienceCurveDef::kMaxChampionLevel; ++level)
        curve.requiredForNextLevel[level] = economy.xpRequiredForNextLevel[level];
    SetExperienceCurve(curve);

    AddReward(MakeChampionReward(
        economy.championKill.killerGold,
        economy.championKill.assistGold,
        economy.championKill.firstBloodBonusGold,
        economy.championKill.victimNextLevelXPFactor,
        economy.championKill.shareRadius));
    AddReward(MakeMinionReward(eMinionRewardKind::Melee,
        economy.melee.soloXP, economy.melee.sharedXP, economy.melee.gold,
        economy.melee.maxGold, economy.melee.growthAmount, economy.melee.growthIntervalSec));
    AddReward(MakeMinionReward(eMinionRewardKind::Ranged,
        economy.ranged.soloXP, economy.ranged.sharedXP, economy.ranged.gold,
        economy.ranged.maxGold, economy.ranged.growthAmount, economy.ranged.growthIntervalSec));
    AddReward(MakeMinionReward(eMinionRewardKind::Siege,
        economy.siege.soloXP, economy.siege.sharedXP, economy.siege.gold,
        economy.siege.maxGold, economy.siege.growthAmount, economy.siege.growthIntervalSec));
    AddReward(MakeMinionReward(eMinionRewardKind::Super,
        economy.super.soloXP, economy.super.sharedXP, economy.super.gold,
        economy.super.maxGold, economy.super.growthAmount, economy.super.growthIntervalSec));
    AddReward(MakeTurretReward(economy.turretGold));
    AddReward(MakeJungleReward(kJungleRewardSubBaron, economy.jungle.baronGold, economy.jungle.baronXP));
    AddReward(MakeJungleReward(kJungleRewardSubEpic, economy.jungle.epicGold, economy.jungle.epicXP));
    AddReward(MakeJungleReward(kJungleRewardSubSmall, economy.jungle.smallCampGold, economy.jungle.smallCampXP));
}

void CRewardRegistry::SetExperienceCurve(const ChampionExperienceCurveDef& curve)
{
    m_ExperienceCurve = curve;
}

f32_t CRewardRegistry::GetRequiredExperienceForNextLevel(u8_t level) const
{
    if (level == 0 || level >= ChampionExperienceCurveDef::kMaxChampionLevel)
        return 0.f;
    return m_ExperienceCurve.requiredForNextLevel[level];
}

void CRewardRegistry::AddReward(const RewardDef& reward)
{
    m_Rewards[MakeKey(reward.sourceKind, reward.subKind)] = reward;
}

const RewardDef* CRewardRegistry::FindReward(eRewardSourceKind sourceKind, u8_t subKind) const
{
    const auto it = m_Rewards.find(MakeKey(sourceKind, subKind));
    return (it != m_Rewards.end()) ? &it->second : nullptr;
}

u32_t CRewardRegistry::MakeKey(eRewardSourceKind sourceKind, u8_t subKind)
{
    return (static_cast<u32_t>(sourceKind) << 8u) | static_cast<u32_t>(subKind);
}
