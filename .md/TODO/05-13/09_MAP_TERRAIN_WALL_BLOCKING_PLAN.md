# 09 맵 지형 벽 이동 차단 적용 계획서

## 진행 중인 전체 세션

1. 완료: 규칙 확인
   - `AGENTS.md`, `CLAUDE.md`, `.md/계획서작성규칙.md`, `.claude/gotchas.md`를 먼저 확인했다.
   - 계획서는 완료/진행/예정 세션을 먼저 나열하고, 이후 전체 경로와 h/cpp 수정 위치를 라인 단위로 짚는다.

2. 완료: 현재 코드 상태 확인
   - 포탑/넥서스/억제기 차단은 `NavGrid` cell carve 방식으로 이미 동작한다.
   - 지형 벽 차단용 `CMapSurfaceSampler`, `CMapWalkableBaker`, `CNavGrid::SegmentWalkable`, `CPathfinder::TryFindNearestReachableGoal`도 현재 worktree에 존재한다.
   - `GameplayCollisionSystem`은 현재 `Execute` 초반에서 return 하므로, 이번 목표는 physics push 재활성화가 아니라 `NavGrid` 기반 이동 차단으로 맞춘다.

3. 진행 중: 목표 적용 계획
   - 맵 지형의 이동 불가능한 벽을 `NavGrid`의 blocked cell로 굽는다.
   - 챔피언, 미니언, 봇 AI, 서버 권위 이동이 모두 같은 `NavGrid`/`IWalkableQuery`를 보게 한다.
   - 포탑과 동일한 체감, 즉 "그 위치로 갈 수 없음"을 목표로 하되, 벽을 엔티티 수천 개로 만들지 않는다.

4. 진행해야 하는 세션
   - Phase 1: 지형 walkable bake 기준 검증 및 보강.
   - Phase 2: Client 클릭 target 보정과 local segment 차단 검증.
   - Phase 3: Server command/move authority 차단 검증.
   - Phase 4: 튜닝 및 수동 플레이 검증.

## Assumptions

- "벽"은 `eSpatialKind::Wall` 같은 새 entity가 아니라 `CNavGrid`의 blocked cell이다.
- 현재 포탑 이동 차단과 같은 UX를 목표로 한다. 즉 클릭 target은 보정/거부되고, 매 tick 이동 segment가 blocked cell을 지나가면 멈춘다.
- 맵 파일은 현재 client/server가 사용 중인 `Client/Bin/Resource/Texture/MAP/output/sr_base_flip.wmesh` 기준이다.
- 완전한 상용 navmesh polygon 시스템, 벽 넘기 스킬, flash/dash 벽 판정은 이번 범위가 아니다.

## 추가/유지해야 하는 파일 전체 경로

현재 worktree 기준 신규 추가 파일은 없다. 아래 파일들은 이미 존재하므로, 구현 시 중복 생성하지 말고 현재 내용을 보강/검증한다.

- `C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/MapSurfaceSampler.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/MapSurfaceSampler.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/MapWalkableBaker.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/MapWalkableBaker.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/NavGrid.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/NavGrid.cpp`
- `C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/Pathfinder.h`
- `C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/Pathfinder.cpp`
- `C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h`
- `C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp`
- `C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp`
- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp`
- `C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h`
- `C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp`

SDK mirror가 필요한 빌드 구조라면 아래 헤더도 같이 유지한다.

- `C:/Users/user/Desktop/Winters/EngineSDK/inc/Manager/Navigation/MapSurfaceSampler.h`
- `C:/Users/user/Desktop/Winters/EngineSDK/inc/Manager/Navigation/MapWalkableBaker.h`
- `C:/Users/user/Desktop/Winters/EngineSDK/inc/Manager/Navigation/NavGrid.h`
- `C:/Users/user/Desktop/Winters/EngineSDK/inc/Manager/Navigation/Pathfinder.h`

프로젝트 파일에 신규 cpp/h 등록이 빠져 있으면 아래 파일을 확인한다.

- `C:/Users/user/Desktop/Winters/Engine/Include/Engine.vcxproj`
- `C:/Users/user/Desktop/Winters/Engine/Include/Engine.vcxproj.filters`
- `C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj`
- `C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj.filters`
- `C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj`
- `C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj.filters`

## Phase 1 - 지형 walkable bake

목표: `wmesh`에서 이동 가능한 표면만 `NavGrid` true로 만들고, 벽/절벽/맵 외곽은 false로 둔다.

수정/확인 위치:

- `C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/MapSurfaceSampler.h:17`
  - `MapSurfaceSample`이 `height`, `normalY`를 유지해야 한다.

```cpp
struct MapSurfaceSample
{
    f32_t height = 0.f;
    f32_t normalY = 1.f;
};
```

- `C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/MapSurfaceSampler.cpp:207`
  - `RasterizeTriangle`에서 triangle normalY를 계산하고 cell에 저장한다.
  - 현재 `h > cell.height` 기준은 높은 장식 top surface를 ground처럼 고를 수 있으므로, 튜닝 중 벽 오검출이 생기면 `normalY` 우선 선택 기준을 보강한다.

```cpp
const Vec3 ab{ b.x - a.x, b.y - a.y, b.z - a.z };
const Vec3 ac{ c.x - a.x, c.y - a.y, c.z - a.z };
const f32_t normalY = std::fabs(Vec3::Cross(ab, ac).Normalized().y);
```

- `C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/MapWalkableBaker.h:15`
  - bake parameter는 최소 범위만 둔다. 불필요한 옵션을 추가하지 않는다.

```cpp
struct MapWalkableBakeDesc
{
    f32_t minNormalY = 0.45f;
    f32_t maxStepHeight = 0.75f;
    f32_t maxWorldY = 8.0f;
    i32_t agentRadiusCells = 1;
};
```

- `C:/Users/user/Desktop/Winters/Engine/Private/Manager/Navigation/MapWalkableBaker.cpp:120`
  - `BakeIntoNavGrid`는 `candidate -> playable seed flood fill -> clearance erosion -> navGrid.SetWalkable` 순서를 유지한다.

```cpp
navGrid.SetAllWalkable(false);
...
navGrid.SetWalkable(x, y, true);
```

검증:

- Debug output에서 `[MapWalkable] candidates=... connected=... final=... seeds=...`가 찍힌다.
- `final`이 0이면 fallback으로 전부 walkable이 되므로, 이 상태는 성공이 아니다.

## Phase 2 - Client 클릭 target과 local movement 차단

목표: 유저가 벽을 클릭해도 blocked cell을 그대로 이동 target으로 쓰지 않는다. local/offline 이동도 벽 segment를 통과하지 않는다.

수정/확인 위치:

- `C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp:250`
  - `CNavGrid::Create` 직후 `scene.BakeMapWalkableNavGrid()`를 호출한다.

```cpp
scene.m_pNavGrid = CNavGrid::Create(
    -(CNavGrid::kCellCountX * CNavGrid::kCellSize) * 0.5f,
    -(CNavGrid::kCellCountY * CNavGrid::kCellSize) * 0.5f);
scene.BakeMapWalkableNavGrid();
```

- `C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp:325`
  - 구조물 spawn 이후 `scene.Mark_StructuresOnNavGrid()`를 호출해 포탑/넥서스/억제기 carve를 유지한다.
  - 순서는 `terrain bake` 먼저, `structure carve` 나중이어야 한다.

- `C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:1739`
  - `BakeMapWalkableNavGrid`는 champion/waypoint seed를 넣어 playable region만 연결한다.

- `C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp:1813`
  - `TryResolveWalkableMoveTarget`은 `CPathfinder::TryFindNearestReachableGoal`로 raw goal을 보정한다.
  - `IsWalkable == true`만 보지 말고, 현재 위치에서 A* path가 있어야 target으로 인정한다.

```cpp
if (!CPathfinder::TryFindNearestReachableGoal(
    m_pNavGrid.get(),
    start,
    rawGoal,
    24,
    resolved,
    &path))
{
    return false;
}
```

- `C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp:166`
  - 클릭 이동 target 생성 직후 walkable resolver를 통과시킨다.

```cpp
Vec3 ground = scene.ResolveMouseMapSurfacePos();
const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
if (bValidGround && scene.TryResolveWalkableMoveTarget(ground, ground))
{
    ...
}
```

- `C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp:220`
  - direct local movement는 매 step마다 segment를 검사한다.

```cpp
Vec3 next = cur;
next.x += moveDir.x * step;
next.z += moveDir.z * step;
if (!scene.IsWalkableMoveSegment(cur, next))
{
    scene.m_bMoving = false;
    scene.m_vPlayerDest = cur;
    ...
}
```

검증:

- 벽 클릭 시 `[MoveTarget] raw blocked goal=... corrected=...` 또는 `reject=no-reachable-cell`이 찍힌다.
- DebugDraw `NavGrid blocked cells`를 켰을 때 구조물뿐 아니라 맵 외곽/절벽/벽도 blocked로 보인다.

## Phase 3 - Server authoritative movement 차단

목표: client가 잘못된 좌표를 보내도 서버가 벽을 통과시키지 않는다. 미니언/챔피언/bot이 모두 같은 server walkable query를 본다.

수정/확인 위치:

- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ICommandExecutor.h:13`
  - shared sim이 engine class를 직접 몰라도 되게 `IWalkableQuery` interface만 본다.

```cpp
struct IWalkableQuery
{
    virtual ~IWalkableQuery() = default;
    virtual bool_t IsWalkableXZ(const Vec3& pos) const = 0;
    virtual bool_t SegmentWalkableXZ(const Vec3& from, const Vec3& to) const = 0;
    virtual bool_t TryResolveMoveTarget(const Vec3& from, const Vec3& rawTarget, Vec3& outTarget) const = 0;
    virtual bool_t TrySampleHeight(f32_t x, f32_t z, f32_t& outY) const = 0;
};
```

- `C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h:76`
  - `CGameRoom final : public IWalkableQuery`를 유지한다.

- `C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp:1034`
  - `TickContext` 생성 시 마지막 인자로 `this`를 넘겨 server tick 전체에 walkable query를 제공한다.

```cpp
TickContext tc{
    m_tickIndex,
    DeterministicTime::kFixedDt,
    DeterministicTime::TickToSec(m_tickIndex),
    &m_rng,
    &m_entityMap,
    NULL_ENTITY,
    this
};
```

- `C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp:1388`
  - server도 client와 같은 wmesh/transform으로 walkable grid를 bake한다.

- `C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp:1467`
  - server structure carve는 client와 동일한 반경 정책을 유지한다.

- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp:905`
  - move command 수신 시 raw target을 바로 `MoveTargetComponent`에 넣지 말고 `TryResolveMoveTarget`으로 보정/거부한다.

```cpp
if (tc.pWalkable)
{
    Vec3 resolved = target;
    if (!tc.pWalkable->TryResolveMoveTarget(pos, target, resolved))
    {
        moveTarget.bHasTarget = false;
        return;
    }
    target = resolved;
}
```

- `C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp:297`
  - 매 tick 이동 segment가 blocked cell을 지나면 이동 target을 해제한다.

```cpp
if (tc.pWalkable && !tc.pWalkable->SegmentWalkableXZ(pos, next))
{
    moveTarget.bHasTarget = false;
    if (!IsActionAnimationLocked(stat, anim, tc))
        SetNetAnimation(anim, eNetAnimId::Idle, tc, false);
    continue;
}
```

검증:

- client가 벽 안쪽 좌표를 보내도 server에서 `[Command] move reject reason=no-walkable-path` 또는 `[ServerNav] corrected move goal ...`이 찍힌다.
- server authoritative snapshot에서 챔피언/미니언 위치가 blocked cell 안으로 들어가지 않는다.

## Phase 4 - 튜닝 및 수동 검증

튜닝 위치:

- `C:/Users/user/Desktop/Winters/Engine/Public/Manager/Navigation/MapWalkableBaker.h:15`
  - `minNormalY`, `maxStepHeight`, `maxWorldY`, `agentRadiusCells`

튜닝 원칙:

- `worldY >= 3` 같은 단일 height 기준은 사용하지 않는다.
- `normalY`, 인접 cell 높이 차, seed connectivity를 함께 사용한다.
- 막히면 안 되는 곳: mid lane, river ramp, jungle entrance, fountain 출발 지점.
- 막혀야 하는 곳: 맵 외곽, 절벽, 수직 벽면, 장식 mesh top, 구조물 반경.

수동 검증 체크리스트:

- 플레이어가 벽/절벽/맵 외곽을 클릭해도 들어가지 않는다.
- 벽 근처 클릭은 도달 가능한 가까운 walkable cell로 보정된다.
- 도달 가능한 후보가 없으면 이동 명령 자체가 거부된다.
- 미니언 wave가 라인을 따라 이동하고, 벽 또는 포탑 반경을 관통하지 않는다.
- bot chase 중 벽을 직선으로 뚫고 가지 않는다.
- 포탑/넥서스/억제기 차단은 기존처럼 유지된다.

명령 검증:

```text
git diff --check
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
```

## Rollback

- 지형 wall bake만 되돌리려면 client/server의 `BakeMapWalkableNavGrid` 또는 `InitializeServerWalkableGrid` bake 호출을 끄고 `SetAllWalkable(true)` fallback을 사용한다.
- 구조물 차단은 `Mark_StructuresOnNavGrid`와 `CarveServerStructuresOnNavGrid`가 별도이므로 유지한다.
- `TickContext::pWalkable`을 null로 두면 shared move system은 기존 이동 방식으로 fallback한다.

## Success criteria

1. 지형 벽 cell이 `NavGrid`에서 blocked로 표시된다 -> verify: DebugDraw `NavGrid blocked cells`.
2. client click target이 blocked cell이면 보정 또는 거부된다 -> verify: `[MoveTarget]` log.
3. server move command가 blocked target/segment를 통과시키지 않는다 -> verify: `[Command]`, `[ServerNav]` log와 snapshot 위치.
4. 챔피언/미니언/bot이 같은 기준으로 벽을 통과하지 않는다 -> verify: 수동 인게임 플레이.
5. 포탑/구조물 차단은 기존처럼 유지된다 -> verify: structure carve log와 직접 이동 테스트.
