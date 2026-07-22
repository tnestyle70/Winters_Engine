#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"

class CWorld;
struct TickContext;

class CStatSystem
{
public:
    static StatComponent BuildBaseStats(const ChampionStatsDef& def, u8_t level);
    static StatComponent BuildBaseStats(
        const ChampionStatBlock& stats,
        eChampion legacyChampion,
        u8_t level);
    static void Recompute(CWorld& world, EntityID entity, StatComponent& stat);
    static void Recompute(
        CWorld& world,
        EntityID entity,
        StatComponent& stat,
        const GameplayDefinitionPack& definitions);
    static void Execute(CWorld& world);
    static void Execute(CWorld& world, const GameplayDefinitionPack& definitions);
    static void TickResourceRegeneration(CWorld& world, const TickContext& tc);

private:
    static f32_t Clamp(f32_t value, f32_t minValue, f32_t maxValue);
    static void ApplyRuntimeModifiers(
        CWorld& world,
        EntityID entity,
        StatComponent& stat,
        u32_t oldBuffHash,
        u32_t oldItemHash,
        const GameplayDefinitionPack* pDefinitions);
};
