#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"

class CWorld;

class CSkillRankSystem
{
public:
    static u8_t GetSpentPoints(const SkillRankComponent& ranks);
    static u8_t GetMaxRankForSlot(u8_t slot);
    static void SyncPointsForLevel(SkillRankComponent& ranks, u8_t championLevel);
    static bool TryLevelSkill(SkillRankComponent& ranks, u8_t slot);

    static u8_t GetRank(CWorld& world, EntityID entity, u8_t slot);
    static bool TryLevelSkill(CWorld& world, EntityID entity, u8_t slot);
};
