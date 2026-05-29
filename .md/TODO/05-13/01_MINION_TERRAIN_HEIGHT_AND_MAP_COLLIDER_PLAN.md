# 01 미니언 지형 높이 + 이동 불가 벽 정의/차단 계획서

Current sequence

01/08 현재: wmesh 기반 이동 가능 구역/벽 정의 + 지형 Y 투영 + 이동 차단
-> 02/08: 우클릭 공격 추격
-> 03/08: B 귀환
-> 04/08: 미니맵
-> 05/08: 전장의 안개
-> 06/08: HUD 아틀라스/초상화/스킬 아이콘
-> 07/08: 스킬 레벨업 및 데미지 +5
-> 08/08: 인게임 상점 및 아이템 스탯

Smoke 정책: 자동 smoke 검증은 폐지한다. 이 slice의 동작 검증은 유저가 인게임에서 직접 수행한다. 빌드/`git diff --check`는 통합 위생 확인용으로만 둔다.

Bot AI is a GameCommand producer and must not directly mutate gameplay truth.
즉, Bot AI는 `GameCommand` 생산자이며 이동/공격/데미지 같은 gameplay truth를 직접 바꾸면 안 된다.

## 결론: 벽 정의

벽은 단순히 `worldY >= 3`인 곳이 아니다.

이 slice에서 벽은 아래 조건 중 하나라도 만족하는 NavGrid cell로 정의한다.

1. `wmesh`에서 지면 sample이 없는 cell.
2. surface normal이 너무 가파른 cell. 즉 바닥이 아니라 절벽/벽면에 가까운 triangle.
3. 인접 cell과의 높이 차이가 너무 큰 cell. 즉 0.5m grid 한 칸에서 챔피언이 자연스럽게 걸을 수 없는 단차.
4. 후보 지면처럼 보이더라도 lane waypoint, fountain, champion spawn 같은 playable seed에서 flood-fill로 연결되지 않는 cell.
5. structure/nav blocker가 carve한 cell.

핵심은 `높은 곳 = 벽`이 아니라 `플레이 가능한 지면과 연결된 완만한 표면 = 이동 가능`이다.

## 왜 Y>=3 단독 기준을 쓰면 안 되는가

`WintersAssetConverter info Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh` 실측:

```text
[Mesh] submeshes=1080 bones=0 vertices=729149 indices=1759152 stride=48
```

맵 transform 실측 앵커:

- [InGameBootstrapBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp:218) `scene.m_MapTransform.SetPosition(0.f, 0.f, 0.f);`
- [InGameBootstrapBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp:219) `scene.m_MapTransform.SetScale({ -0.01f, 0.01f, 0.01f });`
- [InGameBootstrapBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp:220) `scene.m_MapTransform.SetRotation(scene.m_vMapRotation);`

raw vertex Y 분포 실측:

```text
raw y p50=102.56 -> world y 약 1.03
raw y p95=412.92 -> world y 약 4.13
raw y max=1341.09 -> world y 약 13.41
raw y>=300, 즉 world y>=3 vertex 수 = 80,968 / 729,149
```

따라서 `worldY >= 3`을 전부 벽으로 만들면, 실제로 걸을 수 있는 높은 ramp/고지대/통로 표면까지 막을 위험이 있다. 반대로 `worldY < 3`에도 수직 벽면이나 장식 geometry가 섞일 수 있다. 그래서 높이 기준은 보조 조건으로만 쓰고, normal/slope/connectivity를 같이 써야 한다.

Goal

맵 전체 기준으로 이동 가능 구역과 이동 불가 벽을 `wmesh`에서 굽고, 플레이어/미니언/봇/서버 이동이 그 벽을 통과하지 못하게 한다. 동시에 지형 표면 Y를 사용해 액터가 맵 위아래 높이를 따라 움직이게 한다.

Client는 intent만 보낸다.
Server는 검증하고 gameplay truth를 변경한다.
Client는 서버 이벤트/스냅샷을 받아 animation, FX, UI, prediction cleanup만 반영한다.

Non-goals

- 완전한 상용 수준 navmesh polygon system은 만들지 않는다.
- wmesh 원본을 수작업으로 분리하거나 아트 리소스를 새로 만들지 않는다.
- 전투 사거리/시야 판정을 3D 거리로 바꾸지 않는다. XZ gameplay 거리 기준은 유지한다.
- 벽 넘기 스킬, flash 벽 판정, dash clipping은 다음 slice로 분리한다.
- smoke 테스트를 되살리지 않는다.

Why this order

현재 문제는 두 층이다.

1. 액터가 지형 Y를 자연스럽게 따라가지 않는다.
2. 벽/절벽/장식 geometry를 이동 불가로 굽지 않아 플레이어와 AI가 맵 밖/벽 안으로 이동할 수 있다.

이동 가능 구역 정의가 먼저 잡혀야 02 우클릭 공격 추격, 03 귀환, 04 미니맵, 05 전장의 안개가 모두 같은 월드 좌표 신뢰도를 갖는다.

Current-code evidence

- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:110) `CMapSurfaceSampler`가 현재 `Scene_InGame.cpp` 안 private class로 박혀 있다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:113) `LoadFromWMesh`가 wmesh vertex/index를 읽어 512x512 height grid를 만든다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:1742) `SampleHeight` 실패 시 지형 투영이 실패한다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:1762) `ProjectGameplayActorsToMapSurface`가 이미 챔피언/미니언 Y를 client-side로 보정한다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:285) `RasterizeTriangle`은 현재 triangle 높이만 저장하고 normal/slope/walkable 정보를 저장하지 않는다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:320) 같은 XZ cell에서 더 높은 triangle을 선택한다. 이 방식은 절벽 위/장식 top surface가 ground처럼 선택될 수 있다.
- [NavGrid.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/NavGrid.cpp:11) `CNavGrid`는 기본값이 전부 walkable이다.
- [Pathfinder.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/Pathfinder.cpp:42) A*는 start/goal이 walkable이 아니면 empty path를 반환한다.
- [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:1840) 현재 NavGrid blocker는 structure만 carve한다.
- [InGamePlayerControlBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp:166) 플레이어 클릭 이동 target은 `ResolveMouseMapSurfacePos`만 타고 walkable 검증을 하지 않는다.
- [InGamePlayerControlBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp:220) local player 이동은 candidate가 벽인지 확인하지 않고 XZ를 더한다.
- [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp:872) 서버 move command는 `HandleMove`에서 groundPos를 `MoveTargetComponent`에 넣는다.
- [MoveSystem.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp:291) 서버/shared movement는 다음 위치를 만들 때 `pos.y`를 유지하고 walkable grid를 보지 않는다.
- [GameRoom.cpp](C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp:1185) 서버 sim은 `CMoveSystem::Execute`를 통해 authoritative 이동을 수행한다.

Files touched

- 이동/벽 bake 공용화:
  - 추가: [MapSurfaceSampler.h](C:/Users/user/Desktop/Winters/Client/Public/Map/MapSurfaceSampler.h)
  - 추가: [MapSurfaceSampler.cpp](C:/Users/user/Desktop/Winters/Client/Private/Map/MapSurfaceSampler.cpp)
  - 추가: [MapWalkableBaker.h](C:/Users/user/Desktop/Winters/Client/Public/Map/MapWalkableBaker.h)
  - 추가: [MapWalkableBaker.cpp](C:/Users/user/Desktop/Winters/Client/Private/Map/MapWalkableBaker.cpp)
- Client scene 연결:
  - 수정: [Scene_InGame.h](C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h)
  - 수정: [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp)
  - 수정: [InGameBootstrapBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp)
  - 수정: [InGamePlayerControlBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp)
- NavGrid 기능 보강:
  - 수정: [NavGrid.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/NavGrid.h)
  - 수정: [NavGrid.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/NavGrid.cpp)
  - 수정: [Pathfinder.cpp](C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/Pathfinder.cpp)
- 서버 권위 이동:
  - 추가: [WalkableGrid.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/WalkableGrid.h)
  - 추가: [WalkableGrid.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/WalkableGrid.cpp)
  - 수정: [ICommandExecutor.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h)
  - 수정: [MoveSystem.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.h)
  - 수정: [MoveSystem.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp)
  - 수정: [CommandExecutor.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp)
  - 수정: [GameRoom.h](C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h)
  - 수정: [GameRoom.cpp](C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp)
- 프로젝트 파일:
  - 수정: [Client.vcxproj](C:/Users/user/Desktop/Winters/Client/Client.vcxproj)
  - 수정: [Client.vcxproj.filters](C:/Users/user/Desktop/Winters/Client/Client.vcxproj.filters)
  - 수정: [Shared.vcxproj](C:/Users/user/Desktop/Winters/Shared/Shared.vcxproj) 또는 실제 Shared가 포함되는 프로젝트
  - 수정: 관련 `.filters`

Insertion/replacement anchors

## 1. CMapSurfaceSampler를 Scene private class에서 분리

파일: [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:110)

`class CMapSurfaceSampler final` 블록을 새 파일로 이동한다.

새 위치:

```cpp
// Client/Public/Map/MapSurfaceSampler.h
class CMapSurfaceSampler final
{
public:
    struct Sample
    {
        f32_t height = 0.f;
        f32_t normalY = 1.f;
        bool_t bValid = false;
    };

    bool_t LoadFromWMesh(const wchar_t* pPath, const Mat4& matWorld);
    bool_t SampleHeight(f32_t x, f32_t z, f32_t& outHeight) const;
    bool_t Sample(f32_t x, f32_t z, Sample& outSample) const;
    bool_t IsReady() const { return m_bReady; }
    f32_t GetMinX() const { return m_fMinX; }
    f32_t GetMinZ() const { return m_fMinZ; }
    f32_t GetMaxX() const { return m_fMaxX; }
    f32_t GetMaxZ() const { return m_fMaxZ; }
};
```

핵심은 `height`만 저장하지 말고 `normalY` 또는 walkable 후보 판정에 필요한 slope 정보를 같이 저장하는 것이다.

## 2. triangle rasterize 시 normalY 저장

파일: [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:285)

현재:

```cpp
const f32_t h = a.y * w0 + b.y * w1 + c.y * w2;
f32_t& cell = m_vecHeights[static_cast<size_t>(iz) * kGridDim + ix];
if (!IsValidHeight(cell) || h > cell)
    cell = h;
```

변경 방향:

```cpp
const Vec3 ab{ b.x - a.x, b.y - a.y, b.z - a.z };
const Vec3 ac{ c.x - a.x, c.y - a.y, c.z - a.z };
const Vec3 n = Normalize(Cross(ab, ac));
const f32_t normalY = std::fabs(n.y);

const f32_t h = a.y * w0 + b.y * w1 + c.y * w2;
SurfaceCell& cell = m_vecCells[static_cast<size_t>(iz) * kGridDim + ix];
if (!cell.bValid || SelectBetterSurface(cell, h, normalY))
{
    cell.height = h;
    cell.normalY = normalY;
    cell.bValid = true;
}
```

핵심은 vertical wall triangle이 height map에 ground처럼 들어오지 않도록 normal 정보를 보존하는 것이다.

## 3. MapWalkableBaker 추가

파일: [MapWalkableBaker.h](C:/Users/user/Desktop/Winters/Client/Public/Map/MapWalkableBaker.h)

Full new-file contents:

```cpp
#pragma once

#include "WintersTypes.h"
#include "WintersMath.h"

#include <vector>

namespace Engine { class CNavGrid; }
class CMapSurfaceSampler;

struct MapWalkableBakeDesc
{
    f32_t minNormalY = 0.45f;
    f32_t maxStepHeight = 0.75f;
    f32_t maxWorldY = 8.0f;
    i32_t agentRadiusCells = 2;
};

class CMapWalkableBaker final
{
public:
    static bool_t BakeIntoNavGrid(
        const CMapSurfaceSampler& surface,
        Engine::CNavGrid& navGrid,
        const std::vector<Vec3>& playableSeeds,
        const MapWalkableBakeDesc& desc = {});
};
```

핵심은 `candidate -> seed flood fill -> radius erosion -> structure carve` 순서로 굽는 것이다.

## 4. CNavGrid에 전체 초기화/가까운 walkable 조회 추가

파일: [NavGrid.h](C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/NavGrid.h)

include 아래 API 추가:

```cpp
void SetAllWalkable(bool_t bWalkable);
bool_t TryFindNearestWalkableCell(Cell from, int32_t maxRadius, Cell& outCell) const;
bool_t SegmentWalkable(const Vec3& from, const Vec3& to, f32_t stepWorld = kCellSize) const;
```

핵심은 기본값 전부 walkable 상태를 폐지하고, bake가 허용한 cell만 true로 만드는 것이다.

주의: `TryFindNearestWalkableCell`은 "기하학적으로 가까운 walkable cell"만 찾는다. 벽 클릭 보정의 최종 선택은 반드시 A*로 도달 가능한지까지 확인해야 한다. 즉 최종 보정 함수는 `nearest walkable`이 아니라 `nearest reachable walkable`을 찾아야 한다.

## 5. Bootstrap에서 NavGrid를 wmesh 기반으로 굽기

파일: [InGameBootstrapBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp:250)

현재:

```cpp
scene.m_pNavGrid = CNavGrid::Create(
    -(CNavGrid::kCellCountX * CNavGrid::kCellSize) * 0.5f,
    -(CNavGrid::kCellCountY * CNavGrid::kCellSize) * 0.5f);
```

변경 방향:

```cpp
scene.m_pNavGrid = CNavGrid::Create(
    -(CNavGrid::kCellCountX * CNavGrid::kCellSize) * 0.5f,
    -(CNavGrid::kCellCountY * CNavGrid::kCellSize) * 0.5f);

scene.BakeMapWalkableGridFromSurface();
```

핵심은 `Mark_StructuresOnNavGrid()` 이전에 지형 기반 walkable을 굽고, 그 다음 structure blocker를 carve하는 것이다.

## 6. 클릭 지점이 벽이면 가까운 이동 가능 지점으로 보정

파일: [InGamePlayerControlBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp:166)

현재:

```cpp
Vec3 ground = scene.ResolveMouseMapSurfacePos();
```

변경 방향:

```cpp
Vec3 ground = scene.ResolveMouseMapSurfacePos();
if (!scene.TryResolveWalkableMoveTarget(ground, ground))
    return;
```

핵심은 벽 클릭을 그대로 move command로 보내지 않는 것이다.

### 6-A. 벽 클릭 보정 파이프라인 상세

파일: [Scene_InGame.h](C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h)

`TryProjectToMapSurface` 아래에 helper를 추가한다.

```cpp
bool_t TryResolveWalkableMoveTarget(const Vec3& rawTarget, Vec3& outTarget) const;
bool_t IsWalkableMoveSegment(const Vec3& from, const Vec3& to) const;
```

파일: [Scene_InGame.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp)

`TryResolveWalkableMoveTarget`은 아래 순서로 동작한다.

```text
rawTarget = 마우스가 찍은 wmesh 표면 위치
startCell = 현재 플레이어 위치의 NavGrid cell
rawGoalCell = rawTarget의 NavGrid cell

1. startCell이 blocked면 이동 거부
2. rawGoalCell이 walkable이면 A* 경로를 먼저 검사
3. rawGoalCell까지 A* path가 있으면 rawTarget을 그대로 사용
4. rawGoalCell이 blocked이거나 path가 없으면 주변 ring search 시작
5. 각 후보 cell은 IsWalkable == true여야 함
6. 각 후보 cell마다 CPathfinder::Find_Path(startCell, candidate)을 호출
7. path가 비어 있지 않은 후보만 "도달 가능"으로 인정
8. rawGoalCell에 가장 가깝고, path 길이가 짧은 후보를 선택
9. 선택된 cell center를 world position으로 바꾼 뒤 TryProjectToMapSurface로 Y 보정
10. 후보가 없으면 move command를 보내지 않음
```

복붙용 구현 골격:

```cpp
bool_t CScene_InGame::TryResolveWalkableMoveTarget(
    const Vec3& rawTarget,
    Vec3& outTarget) const
{
    if (!m_pNavGrid || m_PlayerEntity == NULL_ENTITY)
        return false;
    if (!m_World.HasComponent<TransformComponent>(m_PlayerEntity))
        return false;

    const Vec3 playerPos =
        m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition();
    const CNavGrid::Cell start = m_pNavGrid->WorldToCell(playerPos);
    const CNavGrid::Cell rawGoal = m_pNavGrid->WorldToCell(rawTarget);

    if (!m_pNavGrid->IsWalkable(start.x, start.y))
        return false;

    auto acceptCell = [&](CNavGrid::Cell cell, std::vector<CNavGrid::Cell>* pOutPath) -> bool_t
    {
        if (!m_pNavGrid->IsWalkable(cell.x, cell.y))
            return false;

        std::vector<CNavGrid::Cell> path =
            CPathfinder::Find_Path(m_pNavGrid.get(), start, cell);
        if (path.empty())
            return false;

        if (pOutPath)
            *pOutPath = std::move(path);
        return true;
    };

    std::vector<CNavGrid::Cell> acceptedPath;
    if (acceptCell(rawGoal, &acceptedPath))
    {
        outTarget = rawTarget;
        (void)TryProjectToMapSurface(outTarget, 0.05f);
        return true;
    }

    constexpr int32_t kMaxCorrectionRadius = 16;
    CNavGrid::Cell best{};
    bool_t bFound = false;
    u32_t bestPathLen = UINT32_MAX;
    i32_t bestGoalDistSq = INT32_MAX;

    for (int32_t r = 1; r <= kMaxCorrectionRadius; ++r)
    {
        for (int32_t dy = -r; dy <= r; ++dy)
        {
            for (int32_t dx = -r; dx <= r; ++dx)
            {
                if (std::abs(dx) != r && std::abs(dy) != r)
                    continue;

                const CNavGrid::Cell candidate{ rawGoal.x + dx, rawGoal.y + dy };
                std::vector<CNavGrid::Cell> path;
                if (!acceptCell(candidate, &path))
                    continue;

                const i32_t goalDistSq = dx * dx + dy * dy;
                const u32_t pathLen = static_cast<u32_t>(path.size());
                if (!bFound ||
                    goalDistSq < bestGoalDistSq ||
                    (goalDistSq == bestGoalDistSq && pathLen < bestPathLen))
                {
                    bFound = true;
                    best = candidate;
                    bestGoalDistSq = goalDistSq;
                    bestPathLen = pathLen;
                }
            }
        }

        if (bFound)
            break;
    }

    if (!bFound)
        return false;

    outTarget = m_pNavGrid->CellToWorld(best.x, best.y);
    (void)TryProjectToMapSurface(outTarget, 0.05f);
    return true;
}
```

핵심 불변식:

- `CPathfinder::Find_Path`는 blocked goal을 자동 보정하지 않는다.
- 벽 클릭 보정은 `TryResolveWalkableMoveTarget`이 담당한다.
- 보정 target은 `IsWalkable == true`만으로 충분하지 않고, 현재 위치에서 A* path가 비어 있지 않아야 한다.
- A* path가 없는 후보는 가까워도 버린다.
- 후보가 없으면 이동 intent 자체를 보내지 않는다.

예상 로그:

```text
[MoveTarget] raw blocked goal=(123,87) corrected=(121,89) path=18
[MoveTarget] raw blocked goal=(123,87) reject=no-reachable-cell
```

## 7. local direct movement도 segment walkable로 막기

파일: [InGamePlayerControlBridge.cpp](C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp:220)

현재:

```cpp
cur.x += moveDir.x * step;
cur.z += moveDir.z * step;
```

변경 방향:

```cpp
Vec3 next = cur;
next.x += moveDir.x * step;
next.z += moveDir.z * step;

if (!scene.IsWalkableMoveSegment(cur, next))
{
    scene.m_bMoving = false;
    return;
}

cur = next;
```

핵심은 목표지만 보정하는 것이 아니라, 매 프레임 벽 통과도 막는 것이다.

## 8. 서버 MoveSystem도 walkable query를 본다

파일: [ICommandExecutor.h](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h:13)

`TickContext`에 optional query를 추가한다.

```cpp
struct IWalkableQuery
{
    virtual ~IWalkableQuery() = default;
    virtual bool_t IsWalkableXZ(const Vec3& pos) const = 0;
    virtual bool_t SegmentWalkableXZ(const Vec3& from, const Vec3& to) const = 0;
    virtual bool_t TryClampToNearestWalkable(const Vec3& in, Vec3& out) const = 0;
    virtual bool_t TrySampleHeight(f32_t x, f32_t z, f32_t& outY) const = 0;
};

struct TickContext
{
    uint64_t tickIndex = 0;
    f32_t fDt = DeterministicTime::kFixedDt;
    f64_t fSimulatedTimeSec = 0;
    DeterministicRng* pRng = nullptr;
    EntityIdMap* pEntityMap = nullptr;
    EntityID localPlayer = NULL_ENTITY;
    const IWalkableQuery* pWalkable = nullptr;
};
```

파일: [MoveSystem.cpp](C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp:291)

현재:

```cpp
const Vec3 next{
    pos.x + dir.x * advance,
    pos.y,
    pos.z + dir.z * advance
};

transform.SetPosition(next);
```

변경 방향:

```cpp
Vec3 next{
    pos.x + dir.x * advance,
    pos.y,
    pos.z + dir.z * advance
};

if (tc.pWalkable)
{
    if (!tc.pWalkable->SegmentWalkableXZ(pos, next))
    {
        moveTarget.bHasTarget = false;
        continue;
    }

    f32_t groundY = pos.y;
    if (tc.pWalkable->TrySampleHeight(next.x, next.z, groundY))
        next.y = groundY + 0.05f;
}

transform.SetPosition(next);
```

핵심은 서버 authoritative movement가 벽을 직접 통과하지 못하게 하는 것이다.

Phase-by-phase completion criteria

## Phase 1: 지형 샘플러 분리 및 walkable 후보 생성

완료 조건:

- `CMapSurfaceSampler`가 `Scene_InGame.cpp` private class에서 분리된다.
- height + normalY sample이 가능하다.
- `MapWalkableBaker`가 `CNavGrid`를 기본 blocked 상태에서 playable candidate만 true로 만든다.
- log:

```text
[MapSurface] loaded grid=512 bounds=(...)
[MapWalkable] candidates=... connected=... blocked=... seeds=...
```

## Phase 2: Client 이동 target/직접 이동 차단

완료 조건:

- 벽 클릭 시 raw blocked cell을 그대로 쓰지 않고, A*로 도달 가능한 가장 가까운 walkable cell로 보정한다.
- 가까운 walkable cell이 있어도 A* path가 비어 있으면 후보에서 제외한다.
- 보정 가능한 후보가 없으면 move command를 보내지 않는다.
- local direct movement가 blocked segment에서 멈춘다.
- debug overlay에서 blocked cell이 벽/절벽/외곽에 보인다.
- log:

```text
[NavGrid] terrain walkable baked connected=...
[Move] reject blocked target=(...)
```

## Phase 3: Server 권위 이동 차단

완료 조건:

- `TickContext::pWalkable` 또는 동등한 query가 서버 `CMoveSystem`에 전달된다.
- 서버 `MoveTargetComponent` 이동이 blocked segment를 통과하지 않는다.
- bot/minion 이동도 같은 query를 사용한다.
- log:

```text
[ServerNav] walkable grid loaded cells=...
[Move] blocked entity=... from=(...) to=(...)
```

## Phase 4: tuning

완료 조건:

- `minNormalY`, `maxStepHeight`, `agentRadiusCells`를 실제 플레이로 조정한다.
- mid lane, river ramp, jungle entrance가 열려 있고 외곽 cliff/wall은 막힌다.

Verification commands and expected results

- 자동 smoke 없음.
- 문서/패치 위생: `git diff --check`.
- 빌드 확인: 기존 Client/Server 빌드 명령 사용.
- 유저 수동 검증:
  - 플레이어가 벽/절벽/맵 외곽을 클릭해도 들어가지 않는다.
  - 미니언 웨이브가 라인과 ramp를 따라 이동하고, 벽을 관통하지 않는다.
  - Jax/Ezreal/Ashe Bot AI가 chase 중 벽을 통과하지 않는다.
  - DebugDraw `NavGrid blocked cells`에서 막힌 cell이 구조물뿐 아니라 맵 벽/외곽에도 표시된다.

Rollback scope

- `MapWalkableBaker` 호출을 제거하면 기존 structure-only NavGrid로 돌아간다.
- `TickContext::pWalkable`를 null로 두면 `CMoveSystem`은 기존 이동 방식으로 fallback한다.
- `CMapSurfaceSampler` 분리만 남겨도 기존 Y projection 기능은 유지 가능하다.

Next slice

이 slice가 통과되면 02/08 우클릭 공격 추격으로 넘어간다. 공격 추격은 반드시 이 walkable query를 사용해 target 접근 경로가 벽을 뚫지 않게 만든다.
