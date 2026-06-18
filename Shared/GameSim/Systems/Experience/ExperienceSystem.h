#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

class CExperienceSystem
{
public:
    static void InitializeChampionExperience(CWorld& world, EntityID entity, u8_t level);
    static void GrantExperience(CWorld& world, EntityID entity, f32_t amount);
    static void GrantKillRewards(CWorld& world, const TickContext& tc, EntityID killer, EntityID victim);

private:
    static f32_t ResolveChampionKillExperience(CWorld& world, EntityID victim);
    static u32_t GrantGold(CWorld& world, EntityID entity, f32_t amount);

    CExperienceSystem() = delete;
};
