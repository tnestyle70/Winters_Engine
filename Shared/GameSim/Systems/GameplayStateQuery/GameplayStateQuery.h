#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <vector>

class CWorld;
enum class eTeam : uint8_t;

namespace GameplayStateQuery
{
    enum class eGameplayTargetKind : uint8_t
    {
        None = 0,
        Champion,
        MinionOrSummon,
        JungleMonster,
        Structure,
    };

    f32_t ResolveGameplayRadius(CWorld& world, EntityID entity);
    eTeam ResolveEntityTeam(CWorld& world, EntityID entity);
    eGameplayTargetKind ResolveTargetKind(CWorld& world, EntityID entity);
    bool_t IsMobileCombatUnit(CWorld& world, EntityID entity);
    // 구조물 발밑은 스폰 시 내브그리드에서 carve 되어 목표 중심 세그먼트 검사가 항상 실패한다.
    // 공격 세그먼트 게이트(SegmentWalkableXZ)는 구조물 타겟을 면제한다. 사거리 검사는 유지.
    bool_t IsAttackSegmentGateExemptTarget(CWorld& world, EntityID target);
    bool_t HasState(CWorld& world, EntityID entity, u32_t stateFlag);
    bool_t CanMove(CWorld& world, EntityID entity);
    bool_t CanAttack(CWorld& world, EntityID entity);
    bool_t CanCast(CWorld& world, EntityID entity);
    bool_t CanBeSeenBy(CWorld& world, EntityID observer, EntityID target);
    bool_t CanBeTargetedBy(CWorld& world, EntityID observer, EntityID target);
    bool_t CanReceiveDamage(CWorld& world, EntityID source, EntityID target);
    bool_t CanReceiveProjectileHit(CWorld& world, EntityID source, EntityID target);
    bool_t CanReceiveEnemyAbilityHit(CWorld& world, EntityID source, EntityID target);
    bool_t CanReceiveCrowdControl(CWorld& world, EntityID source, EntityID target);
    std::vector<EntityID> CollectEnemyMobileUnitsInCircle(
        CWorld& world,
        EntityID source,
        const Vec3& center,
        f32_t radius);
    std::vector<EntityID> CollectEnemyMobileUnitsInSegment(
        CWorld& world,
        EntityID source,
        const Vec3& start,
        const Vec3& end,
        f32_t halfWidth);
    std::vector<EntityID> CollectEnemyMobileUnitsInCone(
        CWorld& world,
        EntityID source,
        const Vec3& origin,
        const Vec3& direction,
        f32_t range,
        f32_t halfAngleRad);
    f32_t GetMoveSpeedMultiplier(CWorld& world, EntityID entity);
}
