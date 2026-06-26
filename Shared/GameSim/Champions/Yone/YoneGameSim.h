#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace YoneGameSim
{
    u8_t ResolveEStage(CWorld& world, EntityID caster);
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
