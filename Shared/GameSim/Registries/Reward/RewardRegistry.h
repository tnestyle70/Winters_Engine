#pragma once

#include "Shared/GameSim/Definitions/GoldRewardDef.h"
#include "WintersTypes.h"

#include <cstddef>
#include <unordered_map>

class CRewardRegistry
{
public:
    static CRewardRegistry& Instance();

    void Reset();
    void LoadDefaultSummonersRift();

    void SetExperienceCurve(const ChampionExperienceCurveDef& curve);
    const ChampionExperienceCurveDef& GetExperienceCurve() const { return m_ExperienceCurve; }
    f32_t GetRequiredExperienceForNextLevel(u8_t level) const;

    void AddReward(const RewardDef& reward);
    const RewardDef* FindReward(eRewardSourceKind sourceKind, u8_t subKind = 0) const;
    std::size_t Count() const { return m_Rewards.size(); }

private:
    CRewardRegistry();
    ~CRewardRegistry() = default;
    CRewardRegistry(const CRewardRegistry&) = delete;
    CRewardRegistry& operator=(const CRewardRegistry&) = delete;

    static u32_t MakeKey(eRewardSourceKind sourceKind, u8_t subKind);

    ChampionExperienceCurveDef m_ExperienceCurve{};
    std::unordered_map<u32_t, RewardDef> m_Rewards{};
};
