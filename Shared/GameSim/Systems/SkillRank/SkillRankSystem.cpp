#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/SkillTypes.h"

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

bool CSkillRankSystem::TryLevelSkill(
    SkillRankComponent& ranks,
    u8_t championLevel,
    u8_t slot)
{
    if (slot >= SkillRankComponent::kSlotCount || ranks.pointsAvailable == 0u)
        return false;

    const u8_t nextRank = static_cast<u8_t>(ranks.ranks[slot] + 1u);
    if (nextRank > GetMaxRankForSlot(slot))
        return false;

    const u8_t requiredLevel = slot == static_cast<u8_t>(eSkillSlot::R)
        ? static_cast<u8_t>(6u + (nextRank - 1u) * 5u)
        : static_cast<u8_t>(nextRank * 2u - 1u);
    if (championLevel < requiredLevel)
        return false;

    ranks.ranks[slot] = nextRank;
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
    if (!world.HasComponent<SkillRankComponent>(entity) ||
        !world.HasComponent<ChampionComponent>(entity))
    {
        return false;
    }

    return TryLevelSkill(
        world.GetComponent<SkillRankComponent>(entity),
        world.GetComponent<ChampionComponent>(entity).level,
        slot);
}
