Session - 미니언 기준 회전 관례를 포함해서 챔피언 서버 yaw, 클라 weak prediction yaw, 스냅샷 적용 yaw의 기준점과 오프셋을 하나로 맞춘다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.h

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/SkillDef.h"
```

아래에 추가:

```cpp
#include "WintersMath.h"
```

기존 코드:

```cpp
f32_t GetDefaultChampionVisualYawOffset(eChampion champion);
```

아래에 추가:

```cpp
f32_t ResolveChampionVisualYawFromDirection(eChampion champion, const Vec3& direction);
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

기존 코드:

```cpp
f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    (void)champion;
    return 3.14159265358979323846f;
}
```

아래로 교체:

```cpp
f32_t GetDefaultChampionVisualYawOffset(eChampion champion)
{
    (void)champion;
    return 3.14159265358979323846f;
}

f32_t ResolveChampionVisualYawFromDirection(eChampion champion, const Vec3& direction)
{
    return static_cast<f32_t>(
        std::atan2(direction.x, direction.z) +
        GetDefaultChampionVisualYawOffset(champion));
}
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp

기존 코드:

```cpp
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation({
            rot.x,
            std::atan2(dir.x, dir.z) + GetDefaultChampionVisualYawOffset(stat.championId),
            rot.z });
```

아래로 교체:

```cpp
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation({
            rot.x,
            ResolveChampionVisualYawFromDirection(stat.championId, dir),
            rot.z });
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

`RotateEntityTowardDirection` 안의 기존 코드:

```cpp
        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        const eChampion champion = ResolveChampion(world, entity);
        transform.SetRotation({
            rot.x,
            static_cast<f32_t>(
                std::atan2(direction.x, direction.z) +
                GetDefaultChampionVisualYawOffset(champion)),
            rot.z
            });
```

아래로 교체:

```cpp
        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        const eChampion champion = ResolveChampion(world, entity);
        transform.SetRotation({
            rot.x,
            ResolveChampionVisualYawFromDirection(champion, direction),
            rot.z
            });
```

1-5. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    bool_t TryResolveWalkableMoveTarget(const Vec3& rawTarget, Vec3& outTarget) const;
```

아래로 교체:

```cpp
    bool_t TryResolveWalkableMoveTarget(
        const Vec3& rawTarget,
        Vec3& outTarget,
        Vec3* pOutFirstWaypoint = nullptr) const;
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

anonymous namespace 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
    bool_t IsNetworkActionAnim(u16_t animID)
    {
        const auto id = static_cast<eNetAnimId>(animID);
        return id == eNetAnimId::BasicAttack ||
            id == eNetAnimId::SkillQ ||
            id == eNetAnimId::SkillW ||
            id == eNetAnimId::SkillE ||
            id == eNetAnimId::SkillR;
    }
```

아래에 추가:

```cpp
    std::vector<CNavGrid::Cell> SmoothClientMovePathCells(
        const CNavGrid& navGrid,
        const std::vector<CNavGrid::Cell>& path)
    {
        if (path.size() <= 2)
            return path;

        std::vector<CNavGrid::Cell> smoothed{};
        smoothed.reserve(path.size());
        smoothed.push_back(path.front());

        size_t anchor = 0;
        while (anchor + 1u < path.size())
        {
            size_t best = anchor + 1u;
            for (size_t probe = path.size() - 1u; probe > anchor + 1u; --probe)
            {
                if (navGrid.LineCellsWalkableForRadius(path[anchor], path[probe], 0.f))
                {
                    best = probe;
                    break;
                }
            }

            smoothed.push_back(path[best]);
            anchor = best;
        }

        return smoothed;
    }
```

기존 코드:

```cpp
bool_t CScene_InGame::TryResolveWalkableMoveTarget(const Vec3& rawTarget, Vec3& outTarget) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid || !m_pPlayerTransform)
        return false;

    const Vec3 playerPos = m_pPlayerTransform->GetPosition();
    CNavGrid::Cell start = pGrid->WorldToCell(playerPos);
    const CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);

    if (!pGrid->IsWalkable(start.x, start.y))
    {
        CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 8, nearestStart))
            return false;
        static u32_t s_startBlockedLogCount = 0;
        if (s_startBlockedLogCount < 64u)
        {
            char msg[224]{};
            sprintf_s(
                msg,
                "[MoveTarget] start-blocked player=(%.2f,%.2f) cell=(%d,%d) nearest=(%d,%d)\n",
                playerPos.x,
                playerPos.z,
                start.x,
                start.y,
                nearestStart.x,
                nearestStart.y);
            OutputDebugStringA(msg);
            ++s_startBlockedLogCount;
        }
        start = nearestStart;
    }

    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[MoveTarget] reject=out-of-nav-bounds goal=(%d,%d) origin=(%.2f,%.2f)\n",
            rawGoal.x,
            rawGoal.y,
            pGrid->Get_OriginX(),
            pGrid->Get_OriginZ());
        OutputDebugStringA(msg);
        return false;
    }

    CNavGrid::Cell resolved{};
    std::vector<CNavGrid::Cell> path{};
    if (!CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        char msg[160]{};
        sprintf_s(
            msg,
            "[MoveTarget] raw blocked goal=(%d,%d) reject=no-reachable-cell\n",
            rawGoal.x,
            rawGoal.y);
        OutputDebugStringA(msg);
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    if (!TryProjectToMapSurface(outTarget, 0.05f))
        outTarget.y = playerPos.y;

    if (resolved.x != rawGoal.x || resolved.y != rawGoal.y)
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[MoveTarget] raw goal=(%d,%d) bfs-corrected=(%d,%d) path=%zu\n",
            rawGoal.x,
            rawGoal.y,
            resolved.x,
            resolved.y,
            path.size());
        OutputDebugStringA(msg);
    }

    return true;
}
```

아래로 교체:

```cpp
bool_t CScene_InGame::TryResolveWalkableMoveTarget(
    const Vec3& rawTarget,
    Vec3& outTarget,
    Vec3* pOutFirstWaypoint) const
{
    const CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid || !m_pPlayerTransform)
        return false;

    const Vec3 playerPos = m_pPlayerTransform->GetPosition();
    CNavGrid::Cell start = pGrid->WorldToCell(playerPos);
    const CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);

    if (!pGrid->IsWalkable(start.x, start.y))
    {
        CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 8, nearestStart))
            return false;
        static u32_t s_startBlockedLogCount = 0;
        if (s_startBlockedLogCount < 64u)
        {
            char msg[224]{};
            sprintf_s(
                msg,
                "[MoveTarget] start-blocked player=(%.2f,%.2f) cell=(%d,%d) nearest=(%d,%d)\n",
                playerPos.x,
                playerPos.z,
                start.x,
                start.y,
                nearestStart.x,
                nearestStart.y);
            OutputDebugStringA(msg);
            ++s_startBlockedLogCount;
        }
        start = nearestStart;
    }

    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[MoveTarget] reject=out-of-nav-bounds goal=(%d,%d) origin=(%.2f,%.2f)\n",
            rawGoal.x,
            rawGoal.y,
            pGrid->Get_OriginX(),
            pGrid->Get_OriginZ());
        OutputDebugStringA(msg);
        return false;
    }

    CNavGrid::Cell resolved{};
    std::vector<CNavGrid::Cell> path{};
    if (!CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        char msg[160]{};
        sprintf_s(
            msg,
            "[MoveTarget] raw blocked goal=(%d,%d) reject=no-reachable-cell\n",
            rawGoal.x,
            rawGoal.y);
        OutputDebugStringA(msg);
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    if (!TryProjectToMapSurface(outTarget, 0.05f))
        outTarget.y = playerPos.y;

    if (pOutFirstWaypoint)
    {
        *pOutFirstWaypoint = outTarget;
        const std::vector<CNavGrid::Cell> smoothedPath =
            SmoothClientMovePathCells(*pGrid, path);
        if (smoothedPath.size() > 1)
        {
            Vec3 waypoint =
                pGrid->CellToWorld(smoothedPath[1].x, smoothedPath[1].y);
            if (!TryProjectToMapSurface(waypoint, 0.05f))
                waypoint.y = playerPos.y;
            *pOutFirstWaypoint = waypoint;
        }
    }

    if (resolved.x != rawGoal.x || resolved.y != rawGoal.y)
    {
        char msg[192]{};
        sprintf_s(
            msg,
            "[MoveTarget] raw goal=(%d,%d) bfs-corrected=(%d,%d) path=%zu\n",
            rawGoal.x,
            rawGoal.y,
            resolved.x,
            resolved.y,
            path.size());
        OutputDebugStringA(msg);
    }

    return true;
}
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp

기존 코드:

```cpp
    void PredictLocalMoveYaw(CScene_InGame& scene, const Vec3& target)
    {
        CTransform* playerTransform = scene.GetPlayerTransformPtr();
        if (!playerTransform)
            return;

        const Vec3 origin = playerTransform->GetPosition();
        const f32_t dx = target.x - origin.x;
        const f32_t dz = target.z - origin.z;
        if ((dx * dx + dz * dz) <= 0.0001f)
            return;

        const f32_t yaw =
            std::atan2f(dx, dz) +
            GetDefaultChampionVisualYawOffset(scene.GetPlayerChampionId());

        Vec3 rot = playerTransform->GetRotation();
        rot.y = yaw;
        playerTransform->SetRotation(rot);

        CWorld& world = scene.GetWorld();
        const EntityID playerEntity = scene.GetPlayerEntity();
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(playerEntity))
        {
            auto& tf = world.GetComponent<TransformComponent>(playerEntity);
            Vec3 ecsRot = tf.GetRotation();
            ecsRot.y = yaw;
            tf.SetRotation(ecsRot);
        }
    }
```

아래로 교체:

```cpp
    void PredictLocalMoveYaw(CScene_InGame& scene, const Vec3& facingTarget)
    {
        CTransform* playerTransform = scene.GetPlayerTransformPtr();
        if (!playerTransform)
            return;

        const Vec3 origin = playerTransform->GetPosition();
        const Vec3 direction{
            facingTarget.x - origin.x,
            0.f,
            facingTarget.z - origin.z
        };
        if ((direction.x * direction.x + direction.z * direction.z) <= 0.0001f)
            return;

        const f32_t yaw =
            ResolveChampionVisualYawFromDirection(scene.GetPlayerChampionId(), direction);

        Vec3 rot = playerTransform->GetRotation();
        rot.y = yaw;
        playerTransform->SetRotation(rot);

        CWorld& world = scene.GetWorld();
        const EntityID playerEntity = scene.GetPlayerEntity();
        if (playerEntity != NULL_ENTITY &&
            world.HasComponent<TransformComponent>(playerEntity))
        {
            auto& tf = world.GetComponent<TransformComponent>(playerEntity);
            Vec3 ecsRot = tf.GetRotation();
            ecsRot.y = yaw;
            tf.SetRotation(ecsRot);
        }
    }
```

기존 코드:

```cpp
            Vec3 ground = scene.ResolveMouseMapSurfacePos();
            Vec3 resolvedGround = ground;

            const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
            if (bValidGround && scene.TryResolveWalkableMoveTarget(ground, resolvedGround))
            {
                if (input.IsRButtonPressed())
                    SpawnMovementIndicator(scene, resolvedGround);

                if (bNetworkActive && scene.m_pCommandSerializer && scene.m_pNetworkView)
                {
                    scene.m_pCommandSerializer->SendMove(*scene.m_pNetworkView, resolvedGround);
                    PredictLocalMoveYaw(scene, resolvedGround);
```

아래로 교체:

```cpp
            Vec3 ground = scene.ResolveMouseMapSurfacePos();
            Vec3 resolvedGround = ground;
            Vec3 predictedFacingTarget = ground;

            const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
            if (bValidGround &&
                scene.TryResolveWalkableMoveTarget(
                    ground,
                    resolvedGround,
                    &predictedFacingTarget))
            {
                if (input.IsRButtonPressed())
                    SpawnMovementIndicator(scene, resolvedGround);

                if (bNetworkActive && scene.m_pCommandSerializer && scene.m_pNetworkView)
                {
                    scene.m_pCommandSerializer->SendMove(*scene.m_pNetworkView, resolvedGround);
                    PredictLocalMoveYaw(scene, predictedFacingTarget);
```

기존 코드:

```cpp
                        f32_t yaw =
                            atan2f(moveDir.x, moveDir.z) +
                            GetDefaultChampionVisualYawOffset(scene.GetPlayerChampionId());
                        Vec3 rot = scene.m_pPlayerTransform->GetRotation();
                        scene.m_pPlayerTransform->SetRotation({ rot.x, yaw, rot.z });
```

아래로 교체:

```cpp
                        f32_t yaw =
                            ResolveChampionVisualYawFromDirection(
                                scene.GetPlayerChampionId(),
                                moveDir);
                        Vec3 rot = scene.m_pPlayerTransform->GetRotation();
                        scene.m_pPlayerTransform->SetRotation({ rot.x, yaw, rot.z });
```

1-8. C:/Users/user/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

새 `kMinion...` 상수는 추가하지 않는다. 클라 미니언 로컬 이동 회전은 Engine `MinionAISystem::FaceToward`의 `+PI` 관례와 맞추되, 기존 파일에서 이미 쓰던 `XM_PI`를 그대로 사용한다.

기존 코드:

```cpp
        const f32_t yaw = atan2f(dir.x, dir.z);
        const Vec3 rot = tf.GetRotation();
        tf.SetRotation({ rot.x, yaw, rot.z });
```

아래로 교체:

```cpp
        const f32_t yaw = atan2f(dir.x, dir.z) + XM_PI;
        const Vec3 rot = tf.GetRotation();
        tf.SetRotation({ rot.x, yaw, rot.z });
```

2. 검증

검증 명령:

```powershell
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Server/Include/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

확인 필요:

```text
1. 우클릭 직후 클라 prediction yaw가 최종 클릭점이 아니라 서버와 같은 path 첫 waypoint 기준으로 도는지 확인.
2. 서버 SnapshotApplier가 받은 yaw가 클라 prediction yaw를 반대 방향으로 덮어쓰지 않는지 확인.
3. 이렐리아로 4회 이상 연속 이동 입력을 넣어도 1회째 이동 중 yaw가 왕복 보정되지 않는지 확인.
4. 미니언 로컬 이동 FaceMoveDirection이 Engine MinionAISystem의 FaceToward와 같은 +PI 관례를 쓰는지 확인.
```
