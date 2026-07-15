#include "Shared/GameSim/Systems/WaypointPatrol/WaypointPatrolSystem.h"

#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/WaypointPatrolComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

namespace
{
    constexpr f32_t kSamePatrolTargetDistSq = 0.01f;

    bool_t IsPatrolEntityAlive(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    void AdvancePatrolIndex(WaypointPatrolComponent& patrol)
    {
        if (patrol.pointCount <= 1)
            return;

        if (patrol.mode == eWaypointPatrolMode::Loop)
        {
            patrol.currentIndex = static_cast<u8_t>(
                (static_cast<u32_t>(patrol.currentIndex) + 1u) % patrol.pointCount);
            return;
        }

        const i32_t next = static_cast<i32_t>(patrol.currentIndex) + patrol.direction;
        if (next < 0 || next >= patrol.pointCount)
        {
            patrol.direction = static_cast<i8_t>(-patrol.direction);
            patrol.currentIndex = static_cast<u8_t>(
                static_cast<i32_t>(patrol.currentIndex) + patrol.direction);
        }
        else
        {
            patrol.currentIndex = static_cast<u8_t>(next);
        }
    }

    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.blockedMoveTicks = 0;
        moveTarget.bestMoveDistance = -1.f;
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }

    bool_t TryAssignPatrolMovePath(
        const TickContext& tc,
        const Vec3& from,
        Vec3& ioTarget,
        MoveTargetComponent& moveTarget)
    {
        ClearMovePath(moveTarget);
        if (!tc.pWalkable)
            return true;

        Vec3 waypoints[kMovePathMaxWaypoints]{};
        u16_t waypointCount = 0;
        Vec3 resolved = ioTarget;
        if (!tc.pWalkable->TryBuildMovePath(
            from,
            ioTarget,
            waypoints,
            kMovePathMaxWaypoints,
            waypointCount,
            resolved))
        {
            return false;
        }

        ioTarget = resolved;
        moveTarget.pathCount = waypointCount;
        moveTarget.pathIndex = 0;
        for (u16_t i = 0; i < waypointCount; ++i)
            moveTarget.pathWaypoints[i] = waypoints[i];

        return true;
    }
}

void CWaypointPatrolSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<WaypointPatrolComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        if (!IsPatrolEntityAlive(world, entity) ||
            !world.HasComponent<WaypointPatrolComponent>(entity) ||
            !world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        WaypointPatrolComponent& patrol = world.GetComponent<WaypointPatrolComponent>(entity);
        if (!patrol.bActive || patrol.pointCount == 0)
            continue;

        if (patrol.currentIndex >= patrol.pointCount)
            patrol.currentIndex = 0;
        if (patrol.direction == 0)
            patrol.direction = 1;

        const Vec3 pos = world.GetComponent<TransformComponent>(entity).GetPosition();
        Vec3 target = patrol.points[patrol.currentIndex];

        if (WintersMath::DistanceSqXZ(pos, target) <= patrol.arriveRadius * patrol.arriveRadius)
        {
            AdvancePatrolIndex(patrol);
            target = patrol.points[patrol.currentIndex];
        }

        MoveTargetComponent& moveTarget = world.HasComponent<MoveTargetComponent>(entity)
            ? world.GetComponent<MoveTargetComponent>(entity)
            : world.AddComponent<MoveTargetComponent>(entity, MoveTargetComponent{});

        if (moveTarget.bHasTarget &&
            WintersMath::DistanceSqXZ(moveTarget.target, target) <= kSamePatrolTargetDistSq)
        {
            continue;
        }

        if (!TryAssignPatrolMovePath(tc, pos, target, moveTarget))
        {
            moveTarget.bHasTarget = false;
            moveTarget.pathCount = 0;
            moveTarget.pathIndex = 0;
            continue;
        }

        patrol.points[patrol.currentIndex] = target;
        moveTarget.target = target;
        moveTarget.arriveRadius = patrol.arriveRadius;
        moveTarget.bHasTarget = true;
    }
}
