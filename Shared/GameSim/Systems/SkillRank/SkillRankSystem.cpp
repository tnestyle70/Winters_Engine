#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"

#include "Shared/GameSim/Core/World/World.h"

u8_t CSkillRankSystem::GetSpentPoints(const SkillRankComponent& ranks)
{
    u8_t total = 0;
    for (u8_t i = 0; i < SkillRankComponent::kSlotCount; ++i)
        total = static_cast<u8_t>(total + ranks.ranks[i]);
    return total;
}

u8_t CSkillRankSystem::GetMaxRankForSlot(u8_t slot)
{
    if (slot == 4)
        return 3;
    if (slot >= 1 && slot <= 3)
        return 5;
    return 0;
}

void CSkillRankSystem::SyncPointsForLevel(SkillRankComponent& ranks, u8_t championLevel)
{
    const u8_t spent = GetSpentPoints(ranks);
    ranks.pointsAvailable = (championLevel > spent) ? static_cast<u8_t>(championLevel - spent) : 0;
}

bool CSkillRankSystem::TryLevelSkill(SkillRankComponent& ranks, u8_t slot)
{
    if (slot >= SkillRankComponent::kSlotCount)
        return false;
    if (ranks.pointsAvailable == 0)
        return false;
    if (ranks.ranks[slot] >= GetMaxRankForSlot(slot))
        return false;

    ++ranks.ranks[slot];
    --ranks.pointsAvailable;
    return true;
}

u8_t CSkillRankSystem::GetRank(CWorld& world, EntityID entity, u8_t slot)
{
    if (slot >= SkillRankComponent::kSlotCount)
        return 0;
    if (!world.HasComponent<SkillRankComponent>(entity))
        return 0;
    return world.GetComponent<SkillRankComponent>(entity).ranks[slot];
}

bool CSkillRankSystem::TryLevelSkill(CWorld& world, EntityID entity, u8_t slot)
{
    if (!world.HasComponent<SkillRankComponent>(entity))
        return false;
    return TryLevelSkill(world.GetComponent<SkillRankComponent>(entity), slot);
}
