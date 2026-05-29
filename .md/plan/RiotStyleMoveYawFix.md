Session - Riot식 클릭 이동 반응으로 raw intent yaw와 navgrid path movement를 분리한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

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

아래에 추가:

```cpp
    bool_t IsFacingCandidateOpposedToIntent(
        const Vec3& origin,
        const Vec3& intentTarget,
        const Vec3& candidate)
    {
        const Vec3 intent{
            intentTarget.x - origin.x,
            0.f,
            intentTarget.z - origin.z
        };
        const Vec3 candidateDir{
            candidate.x - origin.x,
            0.f,
            candidate.z - origin.z
        };
        const f32_t intentLenSq = intent.x * intent.x + intent.z * intent.z;
        const f32_t candidateLenSq =
            candidateDir.x * candidateDir.x + candidateDir.z * candidateDir.z;
        if (intentLenSq <= 0.0001f || candidateLenSq <= 0.0001f)
            return false;

        const f32_t dot = intent.x * candidateDir.x + intent.z * candidateDir.z;
        const f32_t minDot = -0.10f * sqrtf(intentLenSq * candidateLenSq);
        return dot < minDot;
    }
```

기존 코드:

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
            Vec3 waypoint = pGrid->CellToWorld(
                smoothedPath[1].x,
                smoothedPath[1].y);
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

    auto ProjectMoveTarget = [&](Vec3& ioTarget)
        {
            if (!TryProjectToMapSurface(ioTarget, 0.05f))
                ioTarget.y = playerPos.y;
        };

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

    if (pGrid->IsWalkable(rawGoal.x, rawGoal.y) &&
        pGrid->SegmentWalkable(playerPos, rawTarget, 0.f))
    {
        outTarget = rawTarget;
        ProjectMoveTarget(outTarget);
        if (pOutFirstWaypoint)
            *pOutFirstWaypoint = outTarget;
        return true;
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
    ProjectMoveTarget(outTarget);

    if (pOutFirstWaypoint)
    {
        *pOutFirstWaypoint = outTarget;
        const std::vector<CNavGrid::Cell> smoothedPath =
            SmoothClientMovePathCells(*pGrid, path);
        if (smoothedPath.size() > 1)
        {
            Vec3 waypoint = pGrid->CellToWorld(
                smoothedPath[1].x,
                smoothedPath[1].y);
            ProjectMoveTarget(waypoint);

            Vec3 intentFacingTarget = rawTarget;
            ProjectMoveTarget(intentFacingTarget);
            *pOutFirstWaypoint = IsFacingCandidateOpposedToIntent(
                playerPos,
                intentFacingTarget,
                waypoint)
                ? intentFacingTarget
                : waypoint;
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

1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
bool_t CGameRoom::TryBuildMovePath(
    const Vec3& from,
    const Vec3& rawTarget,
    Vec3* pOutWaypoints,
    u16_t maxWaypoints,
    u16_t& outWaypointCount,
    Vec3& outTarget) const
{
    outWaypointCount = 0;
    outTarget = rawTarget;
    if (!pOutWaypoints || maxWaypoints == 0)
        return false;

    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
    {
        (void)TrySampleHeight(outTarget.x, outTarget.z, outTarget.y);
        pOutWaypoints[outWaypointCount++] = outTarget;
        return true;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(from);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return false;
        start = nearestStart;
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    Engine::CNavGrid::Cell resolved{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    if (!TrySampleHeight(outTarget.x, outTarget.z, outTarget.y))
        outTarget.y = from.y;

    Engine::CNavGrid::Cell lastAppended{ -1, -1 };
    auto AppendCell = [&](Engine::CNavGrid::Cell cell) -> bool_t
        {
            if (cell.x == lastAppended.x && cell.y == lastAppended.y)
                return true;
            if (outWaypointCount >= maxWaypoints)
                return false;

            Vec3 waypoint = pGrid->CellToWorld(cell.x, cell.y);
            if (!TrySampleHeight(waypoint.x, waypoint.z, waypoint.y))
                waypoint.y = outTarget.y;

            pOutWaypoints[outWaypointCount++] = waypoint;
            lastAppended = cell;
            return true;
        };

    const std::vector<Engine::CNavGrid::Cell> smoothedPath = SmoothServerPathCells(*pGrid, path);
    if (smoothedPath.size() <= 1)
        return AppendCell(resolved);

    for (size_t i = 1; i < smoothedPath.size(); ++i)
    {
        if (!AppendCell(smoothedPath[i]))
            return false;
    }

    return outWaypointCount > 0;
}
```

아래로 교체:

```cpp
bool_t CGameRoom::TryBuildMovePath(
    const Vec3& from,
    const Vec3& rawTarget,
    Vec3* pOutWaypoints,
    u16_t maxWaypoints,
    u16_t& outWaypointCount,
    Vec3& outTarget) const
{
    outWaypointCount = 0;
    outTarget = rawTarget;
    if (!pOutWaypoints || maxWaypoints == 0)
        return false;

    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
    {
        (void)TrySampleHeight(outTarget.x, outTarget.z, outTarget.y);
        pOutWaypoints[outWaypointCount++] = outTarget;
        return true;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(from);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return false;
        start = nearestStart;
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    if (pGrid->IsWalkable(rawGoal.x, rawGoal.y) &&
        pGrid->SegmentWalkable(from, rawTarget, 0.f))
    {
        outTarget = rawTarget;
        if (!TrySampleHeight(outTarget.x, outTarget.z, outTarget.y))
            outTarget.y = from.y;
        pOutWaypoints[outWaypointCount++] = outTarget;
        return true;
    }

    Engine::CNavGrid::Cell resolved{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        return false;
    }

    outTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    if (!TrySampleHeight(outTarget.x, outTarget.z, outTarget.y))
        outTarget.y = from.y;

    Engine::CNavGrid::Cell lastAppended{ -1, -1 };
    auto AppendCell = [&](Engine::CNavGrid::Cell cell) -> bool_t
        {
            if (cell.x == lastAppended.x && cell.y == lastAppended.y)
                return true;
            if (outWaypointCount >= maxWaypoints)
                return false;

            Vec3 waypoint = pGrid->CellToWorld(cell.x, cell.y);
            if (!TrySampleHeight(waypoint.x, waypoint.z, waypoint.y))
                waypoint.y = outTarget.y;

            pOutWaypoints[outWaypointCount++] = waypoint;
            lastAppended = cell;
            return true;
        };

    const std::vector<Engine::CNavGrid::Cell> smoothedPath = SmoothServerPathCells(*pGrid, path);
    if (smoothedPath.size() <= 1)
        return AppendCell(resolved);

    for (size_t i = 1; i < smoothedPath.size(); ++i)
    {
        if (!AppendCell(smoothedPath[i]))
            return false;
    }

    return outWaypointCount > 0;
}
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

기존 코드:

```cpp
    void RotateEntityTowardDirection(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (entity == NULL_ENTITY || !world.HasComponent<TransformComponent>(entity))
            return;

        const f32_t lenSq = direction.x * direction.x + direction.z * direction.z;
        if (lenSq <= 0.0001f)
            return;

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        const eChampion champion = ResolveChampion(world, entity);
        transform.SetRotation({
            rot.x,
            ResolveChampionVisualYawFromDirection(champion, direction),
            rot.z
            });
    }
```

아래에 추가:

```cpp
    bool_t IsFacingCandidateOpposedToIntent(
        const Vec3& origin,
        const Vec3& intentTarget,
        const Vec3& candidate)
    {
        const Vec3 intent{
            intentTarget.x - origin.x,
            0.f,
            intentTarget.z - origin.z
        };
        const Vec3 candidateDir{
            candidate.x - origin.x,
            0.f,
            candidate.z - origin.z
        };
        const f32_t intentLenSq = intent.x * intent.x + intent.z * intent.z;
        const f32_t candidateLenSq =
            candidateDir.x * candidateDir.x + candidateDir.z * candidateDir.z;
        if (intentLenSq <= 0.0001f || candidateLenSq <= 0.0001f)
            return false;

        const f32_t dot = intent.x * candidateDir.x + intent.z * candidateDir.z;
        const f32_t minDot = -0.10f * sqrtf(intentLenSq * candidateLenSq);
        return dot < minDot;
    }

    Vec3 ResolveMoveFacingTarget(
        const Vec3& origin,
        const Vec3& rawTarget,
        const Vec3& resolvedTarget,
        const MoveTargetComponent& moveTarget)
    {
        if (moveTarget.pathCount > 0)
        {
            const Vec3 waypoint = moveTarget.pathWaypoints[0];
            if (!IsFacingCandidateOpposedToIntent(origin, rawTarget, waypoint))
                return waypoint;
        }

        if (WintersMath::DistanceSqXZ(origin, rawTarget) > 0.0001f)
            return rawTarget;

        return resolvedTarget;
    }
```

기존 코드:

```cpp
        const Vec3 facingTarget = moveTarget.pathCount > 0
            ? moveTarget.pathWaypoints[0]
            : target;
        RotateEntityTowardDirection(
            world,
            cmd.issuerEntity,
            Vec3{
                facingTarget.x - pos.x,
                0.f,
                facingTarget.z - pos.z
            });
```

아래로 교체:

```cpp
        const Vec3 facingTarget = ResolveMoveFacingTarget(
            pos,
            cmd.groundPos,
            target,
            moveTarget);
        RotateEntityTowardDirection(
            world,
            cmd.issuerEntity,
            Vec3{
                facingTarget.x - pos.x,
                0.f,
                facingTarget.z - pos.z
            });
```

2. 검증

검증 명령:
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal`

수동 확인:
- 이동 가능한 cell 내부를 클릭했을 때 플레이어가 raw click 방향으로 즉시 회전하는지 확인.
- 벽이나 장애물 너머를 클릭했을 때 이동 경로는 보정되지만 첫 회전이 반대 방향으로 튀지 않는지 확인.
- `[MoveTarget] raw goal=... bfs-corrected=...` 로그가 없는 일반 walkable direct click에서 cell center 회전이 개입하지 않는지 확인.
- 서버 스냅샷 yaw가 ack 이후에도 첫 waypoint 반대 방향 yaw를 한 프레임 이상 보여주지 않는지 확인.
- 우클릭 연타 시 이동 표시, run animation, 서버 위치 보정이 서로 엇갈리지 않는지 확인.
