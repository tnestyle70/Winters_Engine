#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

class CWorld;
enum class eTeam : uint8_t;

namespace GameplayStateQuery
{
    f32_t ResolveGameplayRadius(CWorld& world, EntityID entity);
    eTeam ResolveEntityTeam(CWorld& world, EntityID entity);
    bool_t HasState(CWorld& world, EntityID entity, u32_t stateFlag);
    bool_t CanMove(CWorld& world, EntityID entity);
    bool_t CanAttack(CWorld& world, EntityID entity);
    bool_t CanCast(CWorld& world, EntityID entity);
    bool_t CanBeSeenBy(CWorld& world, EntityID observer, EntityID target);
    bool_t CanBeTargetedBy(CWorld& world, EntityID observer, EntityID target);
    bool_t CanReceiveDamage(CWorld& world, EntityID source, EntityID target);
    bool_t CanReceiveProjectileHit(CWorld& world, EntityID source, EntityID target);
    f32_t GetMoveSpeedMultiplier(CWorld& world, EntityID entity);
}
