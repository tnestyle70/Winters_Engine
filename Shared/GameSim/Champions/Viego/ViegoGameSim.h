#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"

class CWorld;
struct TickContext;

namespace ViegoGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
    void ClearPossession(CWorld& world, EntityID viegoEntity);
    void TrySpawnSoulForKill(CWorld& world, const TickContext& tc,
        EntityID killer, EntityID deadChampion);
}
