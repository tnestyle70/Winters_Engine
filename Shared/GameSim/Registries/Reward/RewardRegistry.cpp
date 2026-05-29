#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"

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

    RewardDef MakeChampionReward()
    {
        RewardDef reward{};
        reward.sourceKind = eRewardSourceKind::Champion;
        reward.experience.shareRadius = 20.f;
        reward.experience.victimNextLevelXPFactor = 0.50f;
        reward.gold.killerGold = 300.f;
        reward.gold.assistGold = 150.f;
        reward.gold.firstBloodBonusGold = 100.f;
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

    AddReward(MakeChampionReward());
    AddReward(MakeMinionReward(eMinionRewardKind::Melee, 61.75f, 80.60f, 21.f));
    AddReward(MakeMinionReward(eMinionRewardKind::Ranged, 30.40f, 39.68f, 14.f));
    AddReward(MakeMinionReward(eMinionRewardKind::Siege, 95.f, 124.f, 60.f, 90.f, 3.f, 90.f));
    AddReward(MakeMinionReward(eMinionRewardKind::Super, 95.f, 124.f, 60.f, 90.f, 3.f, 90.f));
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
