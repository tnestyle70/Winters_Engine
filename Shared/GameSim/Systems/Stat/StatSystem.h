#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"

class CWorld;

class CStatSystem
{
public:
    static StatComponent BuildBaseStats(const ChampionStatsDef& def, u8_t level);
    static void Recompute(CWorld& world, EntityID entity, StatComponent& stat);
    static void Execute(CWorld& world);

private:
    static f32_t Clamp(f32_t value, f32_t minValue, f32_t maxValue);
};
