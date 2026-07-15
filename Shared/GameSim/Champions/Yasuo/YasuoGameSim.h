#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/ProjectileKindComponent.h"

class CWorld;
struct TickContext;
struct GameCommand;
class ICommandExecutor;

namespace YasuoGameSim
{
    bool_t CanCastSweepingBlade(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target);
    bool_t TryBufferQDuringE(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd);
    u8_t ResolveQVariantStage(CWorld& world, EntityID caster);
    void RegisterQHit(CWorld& world, const TickContext& tc, EntityID caster, eProjectileKind kind);
    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius);
    void ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc, ICommandExecutor* pExecutor = nullptr);
    void CancelRuntime(CWorld& world, EntityID caster);
}
