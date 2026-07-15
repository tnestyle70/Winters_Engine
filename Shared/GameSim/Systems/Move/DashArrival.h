#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Core/World/World.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"

// dash 도착 시점에 1회 호출한다. transform의 현재 위치(=도착 위치)가 벽이면
// fromBeforeDash(=dash 시작점, walkable 가정)를 기준으로 가장 가까운 walkable 셀로 스냅한다.
// 전진 중 매 프레임 호출하면 안 된다(도착 분기에서만).
inline void SnapDashArrivalToWalkable(CWorld& world, const TickContext& tc,
    EntityID entity, const Vec3& fromBeforeDash)
{
    if (!tc.pWalkable || !world.HasComponent<TransformComponent>(entity))
        return;

    auto& transform = world.GetComponent<TransformComponent>(entity);
    const Vec3 arrived = transform.GetLocalPosition();

    if (tc.pWalkable->IsWalkableXZ(arrived))
        return;   // 이미 walkable이면 손대지 않는다.

    Vec3 snapped = arrived;
    if (!tc.pWalkable->TryResolveMoveTarget(fromBeforeDash, arrived, snapped))
        return;   // 스냅 실패 시 기존 위치 유지(현행 동작 보존).

    f32_t surfaceY = snapped.y;
    if (tc.pWalkable->TrySampleHeight(snapped.x, snapped.z, surfaceY))
        snapped.y = surfaceY;

    transform.SetPosition(snapped);
}
