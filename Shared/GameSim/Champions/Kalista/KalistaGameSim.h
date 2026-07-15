#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

class CWorld;
struct DamageRequest;
struct TickContext;

namespace KalistaGameSim
{
    bool_t CanBeginOathswornContract(
        CWorld& world,
        EntityID kalista,
        EntityID ally);
    bool_t TryBeginOathswornContract(
        CWorld& world,
        const TickContext& tc,
        EntityID kalista,
        EntityID ally);
    bool_t TryLaunchCarriedAlly(
        CWorld& world,
        const TickContext& tc,
        EntityID carried,
        const Vec3& targetPosition);
    bool_t CanCastFateCall(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        u8_t stage);
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest);
    // 평타/Q 투사체 적중 시 서버 권위 Rend 스택(박힌 창) 1 추가.
    void ApplyRendStackOnHit(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target);
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
