Session - 미니언 기본 이동을 A* 유닛에서 lane flowfield 유닛으로 교체한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Server/Public/Game/ServerMinionFlowField.h

새 파일:

```cpp
#pragma once

#include "GameContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <array>
#include <vector>

namespace Engine { class CNavGrid; }

class CServerMinionFlowField final
{
public:
    void Clear();
    bool_t Build(
        const Engine::CNavGrid& navGrid,
        const std::vector<Vec3>(&waypoints)[2][3]);
    bool_t TryResolveDirection(eTeam team, u8_t lane, const Vec3& pos, Vec3& outDirection) const;
    bool_t HasField(eTeam team, u8_t lane) const;

private:
    struct FlowField
    {
        bool_t bReady = false;
        f32_t originX = 0.f;
        f32_t originZ = 0.f;
        std::vector<Vec3> directions{};
    };

    static u32_t ResolveFieldIndex(eTeam team, u8_t lane);

    static constexpr u32_t kFieldCount = 6u;
    std::array<FlowField, kFieldCount> m_fields{};
};
```

1-2. C:/Users/user/Desktop/Winters/Server/Private/Game/ServerMinionFlowField.cpp

새 파일:

```cpp
#include "Game/ServerMinionFlowField.h"

#include "Manager/Navigation/NavGrid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    Vec3 NormalizeXZOrZero(const Vec3& v)
    {
        return WintersMath::NormalizeXZ(v, Vec3{}, std::numeric_limits<f32_t>::epsilon());
    }

    Vec3 ResolveSegmentDirection(
        const std::vector<Vec3>& waypoints,
        size_t segmentIndex,
        f32_t t)
    {
        if (segmentIndex + 1u >= waypoints.size())
            return Vec3{};

        if (t > 0.85f && segmentIndex + 2u < waypoints.size())
        {
            return NormalizeXZOrZero(Vec3{
                waypoints[segmentIndex + 2u].x - waypoints[segmentIndex + 1u].x,
                0.f,
                waypoints[segmentIndex + 2u].z - waypoints[segmentIndex + 1u].z });
        }

        return NormalizeXZOrZero(Vec3{
            waypoints[segmentIndex + 1u].x - waypoints[segmentIndex].x,
            0.f,
            waypoints[segmentIndex + 1u].z - waypoints[segmentIndex].z });
    }
}

void CServerMinionFlowField::Clear()
{
    for (FlowField& field : m_fields)
    {
        field.bReady = false;
        field.directions.clear();
    }
}

bool_t CServerMinionFlowField::Build(
    const Engine::CNavGrid& navGrid,
    const std::vector<Vec3>(&waypoints)[2][3])
{
    Clear();

    bool_t bAnyBuilt = false;
    for (u32_t team = 0u; team < 2u; ++team)
    {
        for (u8_t lane = 0u; lane < 3u; ++lane)
        {
            const std::vector<Vec3>& laneWaypoints = waypoints[team][lane];
            if (laneWaypoints.size() < 2u)
                continue;

            FlowField& field = m_fields[team * 3u + lane];
            field.originX = navGrid.Get_OriginX();
            field.originZ = navGrid.Get_OriginZ();
            field.directions.assign(Engine::CNavGrid::kTotalCells, Vec3{});

            for (u32_t y = 0u; y < Engine::CNavGrid::kCellCountY; ++y)
            {
                for (u32_t x = 0u; x < Engine::CNavGrid::kCellCountX; ++x)
                {
                    if (!navGrid.IsWalkable(static_cast<int32_t>(x), static_cast<int32_t>(y)))
                        continue;

                    const Vec3 pos = navGrid.CellToWorld(static_cast<int32_t>(x), static_cast<int32_t>(y));
                    f32_t bestScore = (std::numeric_limits<f32_t>::max)();
                    size_t bestSegment = 0u;
                    f32_t bestT = 0.f;

                    for (size_t i = 1u; i < laneWaypoints.size(); ++i)
                    {
                        f32_t t = 0.f;
                        const f32_t score = WintersMath::DistanceSqPointToSegmentXZ(
                            pos,
                            laneWaypoints[i - 1u],
                            laneWaypoints[i],
                            &t,
                            std::numeric_limits<f32_t>::epsilon());
                        if (score < bestScore)
                        {
                            bestScore = score;
                            bestSegment = i - 1u;
                            bestT = t;
                        }
                    }

                    field.directions[y * Engine::CNavGrid::kCellCountX + x] =
                        ResolveSegmentDirection(laneWaypoints, bestSegment, bestT);
                }
            }

            field.bReady = true;
            bAnyBuilt = true;
        }
    }

    return bAnyBuilt;
}

bool_t CServerMinionFlowField::TryResolveDirection(
    eTeam team,
    u8_t lane,
    const Vec3& pos,
    Vec3& outDirection) const
{
    outDirection = Vec3{};
    const u32_t index = ResolveFieldIndex(team, lane);
    if (index >= m_fields.size())
        return false;

    const FlowField& field = m_fields[index];
    if (!field.bReady || field.directions.size() != Engine::CNavGrid::kTotalCells)
        return false;

    const int32_t cellX = static_cast<int32_t>(
        std::floor((pos.x - field.originX) / Engine::CNavGrid::kCellSize));
    const int32_t cellY = static_cast<int32_t>(
        std::floor((pos.z - field.originZ) / Engine::CNavGrid::kCellSize));
    if (cellX < 0 ||
        cellY < 0 ||
        cellX >= static_cast<int32_t>(Engine::CNavGrid::kCellCountX) ||
        cellY >= static_cast<int32_t>(Engine::CNavGrid::kCellCountY))
    {
        return false;
    }

    outDirection = field.directions[
        static_cast<u32_t>(cellY) * Engine::CNavGrid::kCellCountX +
        static_cast<u32_t>(cellX)];
    return (outDirection.x * outDirection.x + outDirection.z * outDirection.z) > 0.0001f;
}

bool_t CServerMinionFlowField::HasField(eTeam team, u8_t lane) const
{
    const u32_t index = ResolveFieldIndex(team, lane);
    return index < m_fields.size() && m_fields[index].bReady;
}

u32_t CServerMinionFlowField::ResolveFieldIndex(eTeam team, u8_t lane)
{
    const u32_t teamIndex = static_cast<u32_t>(team);
    if (teamIndex >= 2u || lane >= 3u)
        return kFieldCount;
    return teamIndex * 3u + lane;
}
```

1-3. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
#include "Shared/GameSim/Systems/ICommandExecutor.h"
#include "WintersMath.h"
#include "WintersTypes.h"
```

아래에 추가:

```cpp
#include "Game/ServerMinionFlowField.h"
```

기존 코드:

```cpp
    void CacheServerMinionWaypoints(const Winters::Map::StageData& stage);
```

아래에 추가:

```cpp
    void RebuildServerMinionFlowFields();
```

기존 코드:

```cpp
    bool_t TryMoveServerMinionToward(
        EntityID entity,
        MinionStateComponent& state,
        TransformComponent& transform,
        const Vec3& vTarget,
        f32_t fArriveRadius,
        TickContext& tc,
        u32_t& PathBuildBudget,
        bool_t& outMoved,
        MinionStateComponent::State moveState);
```

아래에 추가:

```cpp
    bool_t TryMoveServerMinionByFlowField(
        EntityID entity,
        MinionStateComponent& state,
        TransformComponent& transform,
        TickContext& tc,
        bool_t& outMoved);
```

기존 코드:

```cpp
    std::vector<Vec3> m_serverMinionWaypoints[2][3];
```

아래에 추가:

```cpp
    CServerMinionFlowField m_serverMinionFlowField;
```

1-4. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
    void CGameRoom::CacheServerMinionWaypoints(const Winters::Map::StageData& stage)
```

`CacheServerMinionWaypoints(...)` 함수 끝 바로 아래에 추가:

```cpp
void CGameRoom::RebuildServerMinionFlowFields()
{
    const Engine::CNavGrid* pGrid = m_pPathNavGrid ? m_pPathNavGrid.get() : m_pNavGrid.get();
    if (!pGrid)
    {
        m_serverMinionFlowField.Clear();
        return;
    }

    const bool_t bBuilt = m_serverMinionFlowField.Build(*pGrid, m_serverMinionWaypoints);
    char msg[160]{};
    sprintf_s(msg, "[MinionFlowField] rebuild built=%d\n", bBuilt ? 1 : 0);
    OutputServerAITrace(msg);
}
```

`SpawnServerGameplayObjects(...)` 안의 기존 코드:

```cpp
            SanitizeServerMinionWaypointsOnNavGrid();
            RefreshBotLaneWaitGoals();
```

아래로 교체:

```cpp
            SanitizeServerMinionWaypointsOnNavGrid();
            RebuildServerMinionFlowFields();
            RefreshBotLaneWaitGoals();
```

`SpawnServerGameplayObjects(...)` fallback branch의 기존 코드:

```cpp
    SanitizeServerMinionWaypointsOnNavGrid();
    RefreshBotLaneWaitGoals();
```

아래로 교체:

```cpp
    SanitizeServerMinionWaypointsOnNavGrid();
    RebuildServerMinionFlowFields();
    RefreshBotLaneWaitGoals();
```

`TryMoveServerMinionToward(...)` 바로 위에 추가:

```cpp
bool_t CGameRoom::TryMoveServerMinionByFlowField(
    EntityID entity,
    MinionStateComponent& state,
    TransformComponent& transform,
    TickContext& tc,
    bool_t& outMoved)
{
    const u8_t waypointLane = ResolveServerWaypointLane(state.team, state.lane);
    Vec3 vDir{};
    if (!m_serverMinionFlowField.TryResolveDirection(state.team, waypointLane, transform.GetPosition(), vDir))
        return false;

    const f32_t fLenSq = vDir.x * vDir.x + vDir.z * vDir.z;
    if (fLenSq <= 0.0001f)
        return false;

    const Vec3 vPos = transform.GetPosition();
    const f32_t fStep = state.moveSpeed * tc.fDt;
    Vec3 vNext{};
    if (!TryResolveMinionMoveStep(entity, vPos, vDir, fStep, vNext))
    {
        ++state.BlockedMoveFrames;
        return false;
    }

    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = MinionStateComponent::LaneMove;
    state.PathCount = 0u;
    state.PathIndex = 0u;
    state.BlockedMoveFrames = 0u;
    outMoved = true;
    return true;
}
```

`Phase_ServerMinionAI(...)` lane movement branch의 기존 코드:

```cpp
                (void)TryMoveServerMinionToward(
                    entity,
                    state,
                    transform,
                    laneTarget,
                    0.8f,
                    tc,
                    PathBuildBudget,
                    bMoved,
                    MinionStateComponent::LaneMove);
```

아래로 교체:

```cpp
                if (!TryMoveServerMinionByFlowField(entity, state, transform, tc, bMoved))
                {
                    (void)TryMoveServerMinionToward(
                        entity,
                        state,
                        transform,
                        laneTarget,
                        0.8f,
                        tc,
                        PathBuildBudget,
                        bMoved,
                        MinionStateComponent::LaneMove);
                }
```

1-5. C:/Users/user/Desktop/Winters/.md/TODO/05-19/MINION_GAMEROOM_SPLIT_PIPELINE_SESSIONS.md

기존 코드:

```text
| Session 03 | 다음 진행 | stuck/avoidance, path movement, A* 끊김 방어를 함께 정리 | path build/A* attempt/smoothing budget, rebuild jitter, movement 후보 선택을 한 경계로 묶음 |
```

아래로 교체:

```text
| Session 03 | 다음 진행 | 미니언 기본 이동을 lane flowfield로 교체 | LaneMove는 flowfield, Chase/off-lane/stuck recovery만 A* fallback 사용 |
```

2. 검증

검증 명령:

```powershell
git diff --check -- Server/Public/Game/GameRoom.h Server/Private/Game/GameRoom.cpp Server/Public/Game/ServerMinionFlowField.h Server/Private/Game/ServerMinionFlowField.cpp .md/TODO/05-19/MINION_GAMEROOM_SPLIT_PIPELINE_SESSIONS.md
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
```

확인 필요:

- 새 `ServerMinionFlowField.h/.cpp`가 `Server.vcxproj`와 `Server.vcxproj.filters`에 포함되는지 확인.
- 런타임에서 LaneMove 중 `AStar::NodesVisited`가 증가하지 않는지 확인.
- Chase/off-lane/stuck fallback에서만 `TryBuildMovePath(...)`가 호출되는지 확인.
