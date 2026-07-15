#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"

class CWorld;
struct TickContext;

namespace RivenGameSim
{
    u8_t ResolveQVariantStage(CWorld& world, EntityID caster);
    bool_t CanCastBladeOfTheExile(CWorld& world, EntityID caster, u8_t stage);
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
