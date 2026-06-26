#pragma once

#include "WintersTypes.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

class CWorld;
struct TickContext;

namespace YasuoGameSim
{
    u8_t ResolveQVariantStage(CWorld& world, EntityID caster);
    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius);
    void ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
