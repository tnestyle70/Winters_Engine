Session - 파괴된 구조물의 서버 내비 장애물을 즉시 해제하고 칼리스타 구조물 추격의 보정 종점 제자리걸음을 제거한다.
좌표: 없음 · 축: C2 이동>계산, C7 권위와 정합성
관련: 2026-07-19_MINION_WAVE_PROGRESS_FACING_RECALL_SCALE_VIEGO_R_RESET_PLAN.md / RESULT.md

## 1. 결정 기록

① 문제·제약: 구조물 사망 뒤에도 512×512 nav/path/flow에 시작 시 carve가 남고, 약 2m 보정된 도착점에서 `effectiveRange` 전체를 arrive radius로 써 0.1초마다 이동 완료·재탐색이 반복된다.
② 순진한 해법의 실패: 미니언 teleport는 영구 장애물을 지우지 못하고, 칼리스타 사거리 증가는 모든 표적 판정을 바꾸므로 증상만 가린다.
③ 메커니즘: terrain-only nav 사본에서 생존 구조물만 다시 carve하고, 추격 도착 반경은 `effectiveRange - raw/resolved offset - 0.05`로 한 곳에서 계산한다.
④ 대조: 매 tick 전체 grid를 다시 만들지 않고 작은 생존 hash만 읽어 구조물 사망/부활 때만 path·minion flow·AI goal을 갱신한다.
⑤ 대가: 구조물 상태 변화 1회마다 grid clone·inflation·flow rebuild 비용을 지불한다. 이동 구조물이나 런타임 반경 변경이 생기면 hash에 위치·반경 revision을 추가해야 한다.
- 예산: 이번 바닥 작업은 구현·자동 검증 70% 안에서 닫고, 30% 천장 예산은 normal F5 5v5 시각 캡처에 남긴다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

기존 코드:

```cpp
    void InitializeServerWalkableGrid(const Winters::Map::StageData* pStage, const wchar_t* pStagePath);
    void CarveServerStructuresOnNavGrid();
    void BuildServerPathNavGrid();
```

아래로 교체:

```cpp
    void InitializeServerWalkableGrid(const Winters::Map::StageData* pStage, const wchar_t* pStagePath);
    bool_t CaptureServerTerrainNavGrid();
    u64_t ComputeServerStructureNavigationStateHash();
    void RefreshServerStructureNavigationIfNeeded();
    bool_t CarveServerStructuresOnNavGrid();
    void BuildServerPathNavGrid();
```

기존 코드:

```cpp
    std::unique_ptr<Engine::CMapSurfaceSampler> m_pMapSurfaceSampler;
    std::unique_ptr<Engine::CNavGrid> m_pNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pPathNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pMinionLaneNavGrid;
```

아래로 교체:

```cpp
    std::unique_ptr<Engine::CMapSurfaceSampler> m_pMapSurfaceSampler;
    std::unique_ptr<Engine::CNavGrid> m_pTerrainNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pPathNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pMinionLaneNavGrid;
    u64_t m_serverStructureNavigationStateHash = 0ull;
```

### 2-2. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomNav.cpp

`InitializeServerWalkableGrid` 시작의 기존 코드:

```cpp
    m_pPathNavGrid.reset();
```

아래로 교체:

```cpp
    m_pTerrainNavGrid.reset();
    m_pPathNavGrid.reset();
    m_pMinionLaneNavGrid.reset();
    m_serverStructureNavigationStateHash = 0ull;
```

`InitializeServerWalkableGrid`의 authored 성공, 두 fail-closed 반환, 최종 bake 성공 경로에서 각각 기존 코드:

```cpp
            BuildServerPathNavGrid();
```

아래로 교체:

```cpp
            CaptureServerTerrainNavGrid();
            BuildServerPathNavGrid();
```

최종 bake 성공 경로의 기존 코드:

```cpp
    OutputServerAITrace(msg);
    BuildServerPathNavGrid();
}
```

아래로 교체:

```cpp
    OutputServerAITrace(msg);
    CaptureServerTerrainNavGrid();
    BuildServerPathNavGrid();
}
```

`CarveServerStructuresOnNavGrid` 바로 위에 추가:

```cpp
bool_t CGameRoom::CaptureServerTerrainNavGrid()
{
    m_pTerrainNavGrid.reset();
    if (!m_pNavGrid)
    {
        OutputServerAITrace(
            "[ServerNav] terrain clone aborted: source nav grid is missing\n");
        return false;
    }

    auto terrain = Engine::CNavGrid::Create(
        m_pNavGrid->Get_OriginX(),
        m_pNavGrid->Get_OriginZ());
    if (!terrain)
    {
        OutputServerAITrace(
            "[ServerNav] terrain clone aborted: allocation failed\n");
        return false;
    }

    terrain->Load_Bits(
        m_pNavGrid->Get_Bits(),
        Engine::CNavGrid::kByteSize);
    m_pTerrainNavGrid = std::move(terrain);
    return true;
}

u64_t CGameRoom::ComputeServerStructureNavigationStateHash()
{
    u64_t hash = 1469598103934665603ull;
    const auto structures =
        DeterministicEntityIterator<StructureComponent>::CollectSorted(m_world);
    for (EntityID entity : structures)
    {
        const bool_t bBlocking =
            m_world.HasComponent<TransformComponent>(entity) &&
            m_world.HasComponent<HealthComponent>(entity) &&
            !m_world.GetComponent<HealthComponent>(entity).bIsDead &&
            m_world.GetComponent<HealthComponent>(entity).fCurrent > 0.f;
        hash ^= static_cast<u64_t>(entity);
        hash *= 1099511628211ull;
        hash ^= bBlocking ? 1ull : 0ull;
        hash *= 1099511628211ull;
    }
    return hash;
}

void CGameRoom::RefreshServerStructureNavigationIfNeeded()
{
    const u64_t currentHash = ComputeServerStructureNavigationStateHash();
    if (currentHash == m_serverStructureNavigationStateHash)
        return;

    if (!m_pTerrainNavGrid)
        return;

    if (!CarveServerStructuresOnNavGrid())
        return;
    SanitizeServerMoversOnNavGrid();
    SanitizeServerWaypointPatrolsOnNavGrid();
    SanitizeServerMinionWaypointsOnNavGrid();
    RebuildServerMinionFlowFields();
    RefreshChampionAIGoals();
}
```

기존 `CarveServerStructuresOnNavGrid` 전체를 아래로 교체:

```cpp
bool_t CGameRoom::CarveServerStructuresOnNavGrid()
{
    if (!m_pTerrainNavGrid)
    {
        OutputServerAITrace(
            "[ServerNav] structure carve aborted: terrain clone is missing\n");
        return false;
    }

    auto rebuilt = Engine::CNavGrid::Create(
        m_pTerrainNavGrid->Get_OriginX(),
        m_pTerrainNavGrid->Get_OriginZ());
    if (!rebuilt)
    {
        OutputServerAITrace(
            "[ServerNav] structure carve aborted: grid allocation failed\n");
        return false;
    }
    rebuilt->Load_Bits(
        m_pTerrainNavGrid->Get_Bits(),
        Engine::CNavGrid::kByteSize);
    m_pNavGrid = std::move(rebuilt);

    u32_t carvedStructures = 0;
    m_world.ForEach<StructureComponent, TransformComponent, HealthComponent>(
        std::function<void(
            EntityID,
            StructureComponent&,
            TransformComponent&,
            HealthComponent&)>(
            [&](EntityID,
                StructureComponent& structure,
                TransformComponent& transform,
                HealthComponent& health)
            {
                if (health.bIsDead || health.fCurrent <= 0.f)
                    return;

                const f32_t radius = ResolveStageStructureRadius(
                    structure.kind,
                    structure.tier);
                const Vec3 pos = transform.GetPosition();
                const Engine::CNavGrid::Cell center = m_pNavGrid->WorldToCell(pos);
                const int32_t rCells = static_cast<int32_t>(
                    std::ceil(radius / Engine::CNavGrid::kCellSize));
                for (int32_t dy = -rCells; dy <= rCells; ++dy)
                {
                    for (int32_t dx = -rCells; dx <= rCells; ++dx)
                    {
                        if (dx * dx + dy * dy <= rCells * rCells)
                            m_pNavGrid->SetWalkable(center.x + dx, center.y + dy, false);
                    }
                }

                ++carvedStructures;
            }));

    m_serverStructureNavigationStateHash =
        ComputeServerStructureNavigationStateHash();

    char msg[192]{};
    sprintf_s(msg,
        "[ServerNav] structures carved=%u walkable=%u hash=%08X\n",
        carvedStructures,
        m_pNavGrid->CountWalkableCells(),
        m_pNavGrid->ComputeContentHash());
    OutputServerAITrace(msg);
    BuildServerPathNavGrid();
    return true;
}
```

### 2-3. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomUnitAI.cpp

`Phase_ServerMinionWave`의 기존 코드:

```cpp
    if (!IsInGamePhase() || !m_bGameplayObjectsSpawned)
        return;

    m_serverMinionWaves.TickWave(
```

아래로 교체:

```cpp
    if (!IsInGamePhase() || !m_bGameplayObjectsSpawned)
        return;

    RefreshServerStructureNavigationIfNeeded();

    m_serverMinionWaves.TickWave(
```

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChase/AttackChaseSystem.h

기존 코드:

```cpp
class CWorld;

class CAttackChaseSystem final
```

아래로 교체:

```cpp
class CWorld;

namespace AttackChaseGeometry
{
    f32_t ResolveMoveArriveRadius(
        f32_t effectiveRange,
        const Vec3& targetPosition,
        const Vec3& resolvedMoveTarget);
}

class CAttackChaseSystem final
```

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp

anonymous namespace 종료와 `CAttackChaseSystem::Execute` 사이에 추가:

```cpp
f32_t AttackChaseGeometry::ResolveMoveArriveRadius(
    f32_t effectiveRange,
    const Vec3& targetPosition,
    const Vec3& resolvedMoveTarget)
{
    const f32_t resolvedTargetOffset = std::sqrt(
        WintersMath::DistanceSqXZ(targetPosition, resolvedMoveTarget));
    return (std::max)(
        MoveTargetComponent{}.arriveRadius,
        effectiveRange - resolvedTargetOffset - kAttackChaseArriveSlack);
}
```

`SetChaseMoveTarget`의 기존 코드:

```cpp
        moveTarget.target = resolvedTarget;
        moveTarget.arriveRadius =
            std::max(MoveTargetComponent{}.arriveRadius,
                effectiveRange - kAttackChaseArriveSlack);
```

아래로 교체:

```cpp
        moveTarget.target = resolvedTarget;
        moveTarget.arriveRadius = AttackChaseGeometry::ResolveMoveArriveRadius(
            effectiveRange,
            target,
            resolvedTarget);
```

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 include:

```cpp
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Systems/AttackChase/AttackChaseSystem.h"
```

삭제할 코드:

```cpp
    constexpr f32_t kAttackChaseArriveSlack = 0.05f;
```

`StartAttackChase`의 구조물 목표 처리 블록을 아래로 교체:

```cpp
        if (world.HasComponent<TransformComponent>(cmd.targetEntity))
        {
            const Vec3 targetPosition =
                world.GetComponent<TransformComponent>(cmd.targetEntity).GetLocalPosition();
            Vec3 goal = targetPosition;

            auto& moveTarget = world.HasComponent<MoveTargetComponent>(cmd.issuerEntity)
                ? world.GetComponent<MoveTargetComponent>(cmd.issuerEntity)
                : world.AddComponent<MoveTargetComponent>(cmd.issuerEntity, MoveTargetComponent{});

            if (world.HasComponent<TransformComponent>(cmd.issuerEntity))
            {
                const Vec3 selfPos =
                    world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition();
                if (!TryAssignGridMovePath(tc, selfPos, goal, moveTarget))
                {
                    moveTarget.bHasTarget = false;
                    return;
                }
                moveTarget.arriveRadius = AttackChaseGeometry::ResolveMoveArriveRadius(
                    effectiveRange,
                    targetPosition,
                    goal);

                const Vec3 facingDirection =
                    ResolveAttackChaseFacingDirection(
                        selfPos,
                        targetPosition,
                        cmd.direction);
                const Vec3 facingTarget{
                    selfPos.x + facingDirection.x,
                    selfPos.y,
                    selfPos.z + facingDirection.z
                };
                SetMoveFacingOverride(
                    moveTarget,
                    facingTarget,
                    facingDirection,
                    cmd.sequenceNum);
            }
            else
            {
                ClearMovePath(moveTarget);
                moveTarget.arriveRadius = AttackChaseGeometry::ResolveMoveArriveRadius(
                    effectiveRange,
                    targetPosition,
                    goal);
            }

            moveTarget.target = goal;
            moveTarget.bHasTarget = true;
        }
```

### 2-7. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`FlatWalkable`의 query counter 위에 추가:

```cpp
        bool_t bOverridePathTarget = false;
        Vec3 overriddenPathTarget{};
```

`TryBuildMovePath`의 기존 본문을 아래로 교체:

```cpp
        {
            outTarget = bOverridePathTarget
                ? overriddenPathTarget
                : rawTarget;
            outWaypointCount = 0;
            if (maxWaypoints >= 1 && pOutWaypoints)
            {
                pOutWaypoints[0] = outTarget;
                outWaypointCount = 1;
            }
            return true;
        }
```

`RunBasicAttackGameFeelContractProbe` 아래에 추가:

```cpp
    bool_t RunAttackChaseResolvedTargetProbe()
    {
        CWorld world;
        DeterministicRng rng(2026071903ull);
        EntityIdMap entityMap;
        FlatWalkable walkable;
        auto executor = CDefaultCommandExecutor::Create();

        const EntityID kalista = SpawnChampion(
            world, entityMap, eChampion::KALISTA,
            static_cast<u8_t>(eTeam::Blue), 0u);
        const EntityID target = SpawnChampion(
            world, entityMap, eChampion::GAREN,
            static_cast<u8_t>(eTeam::Red), 5u);
        const Vec3 targetPosition{ 12.f, 0.f, 0.f };
        const Vec3 resolvedMoveTarget{ 10.f, 0.f, 0.f };
        world.GetComponent<TransformComponent>(kalista).SetPosition(Vec3{});
        world.GetComponent<TransformComponent>(target).SetPosition(targetPosition);
        walkable.bOverridePathTarget = true;
        walkable.overriddenPathTarget = resolvedMoveTarget;

        GameCommand attack{};
        attack.kind = eCommandKind::BasicAttack;
        attack.issuerEntity = kalista;
        attack.targetEntity = target;
        attack.sequenceNum = 77u;
        attack.direction = Vec3{ 1.f, 0.f, 0.f };
        TickContext tick1 = MakeProbeTickContext(1ull, rng, entityMap, walkable);
        const CommandExecutionResult result =
            executor->ExecuteCommand(world, tick1, attack);
        if (result.state != eCommandExecutionState::Accepted ||
            !world.HasComponent<AttackChaseComponent>(kalista) ||
            !world.HasComponent<MoveTargetComponent>(kalista))
        {
            std::printf("[SimLab][AttackChaseResolved] FAIL: chase was not created\n");
            return false;
        }

        const AttackChaseComponent& chase =
            world.GetComponent<AttackChaseComponent>(kalista);
        const MoveTargetComponent& initialMove =
            world.GetComponent<MoveTargetComponent>(kalista);
        const f32_t expected = AttackChaseGeometry::ResolveMoveArriveRadius(
            chase.effectiveRange,
            targetPosition,
            resolvedMoveTarget);
        const auto Near = [](f32_t lhs, f32_t rhs)
        {
            return std::fabs(lhs - rhs) <= 0.0001f;
        };
        if (!Near(initialMove.target.x, resolvedMoveTarget.x) ||
            !Near(initialMove.arriveRadius, expected))
        {
            std::printf(
                "[SimLab][AttackChaseResolved] FAIL: start target=%.3f radius=%.3f/%.3f\n",
                initialMove.target.x,
                initialMove.arriveRadius,
                expected);
            return false;
        }

        const Vec3 worstCaseArrival{
            resolvedMoveTarget.x - expected,
            resolvedMoveTarget.y,
            resolvedMoveTarget.z
        };
        const f32_t worstCaseRawDistance = std::sqrt(
            WintersMath::DistanceSqXZ(worstCaseArrival, targetPosition));
        if (worstCaseRawDistance > chase.effectiveRange - 0.049f)
        {
            std::printf(
                "[SimLab][AttackChaseResolved] FAIL: raw target remains out of range %.3f/%.3f\n",
                worstCaseRawDistance,
                chase.effectiveRange);
            return false;
        }

        world.GetComponent<AttackChaseComponent>(kalista).repathTimer = 0.f;
        std::vector<GameCommand> pendingCommands;
        TickContext tick2 = MakeProbeTickContext(2ull, rng, entityMap, walkable);
        CAttackChaseSystem::Execute(world, tick2, pendingCommands);
        const MoveTargetComponent& repathMove =
            world.GetComponent<MoveTargetComponent>(kalista);
        if (!pendingCommands.empty() ||
            !Near(repathMove.target.x, resolvedMoveTarget.x) ||
            !Near(repathMove.arriveRadius, expected))
        {
            std::printf(
                "[SimLab][AttackChaseResolved] FAIL: repath commands=%zu target=%.3f radius=%.3f/%.3f\n",
                pendingCommands.size(),
                repathMove.target.x,
                repathMove.arriveRadius,
                expected);
            return false;
        }

        std::printf(
            "[SimLab][AttackChaseResolved] PASS: offset=2.000 radius=%.3f\n",
            expected);
        return true;
    }
```

`--gamefeel-only`의 기존 코드:

```cpp
        const bool_t bPass = RunBasicAttackGameFeelContractProbe() &&
            RunAttackSpeedLabMatrixProbe();
```

아래로 교체:

```cpp
        const bool_t bPass = RunBasicAttackGameFeelContractProbe() &&
            RunAttackChaseResolvedTargetProbe() &&
            RunAttackSpeedLabMatrixProbe();
```

full run의 `bBasicAttackGameFeelContractProbePass` 바로 아래에 추가하고 최종 `bPass`에도 같은 순서로 연결:

```cpp
    const bool_t bAttackChaseResolvedTargetProbePass =
        RunAttackChaseResolvedTargetProbe();
```

```cpp
        bBasicAttackGameFeelContractProbePass &&
        bAttackChaseResolvedTargetProbePass &&
        bAttackSpeedLabMatrixProbePass &&
```

### 2-8. C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomBotMatchSoak.cpp

기존 include:

```cpp
#include "ECS/Components/TransformComponent.h"
```

아래에 추가:

```cpp
#include "Manager/Navigation/NavGrid.h"
```

`CGameRoomIntegrationProbeAccess::PrepareBotMatch` 아래에 추가:

```cpp
    static bool_t RunStructureNavigationRefreshProbe(
        CGameRoom& room,
        std::string& outError)
    {
        outError.clear();
        if (!room.m_pTerrainNavGrid || !room.m_pNavGrid)
        {
            outError = "server terrain/nav grid is missing";
            return false;
        }

        EntityID structureEntity = NULL_ENTITY;
        Engine::CNavGrid::Cell probeCell{};
        room.m_world.ForEach<StructureComponent, TransformComponent, HealthComponent>(
            [&](EntityID entity,
                StructureComponent&,
                TransformComponent& transform,
                HealthComponent& health)
            {
                if (structureEntity != NULL_ENTITY ||
                    health.bIsDead ||
                    health.fCurrent <= 0.f)
                {
                    return;
                }

                const Engine::CNavGrid::Cell center =
                    room.m_pTerrainNavGrid->WorldToCell(transform.GetPosition());
                for (int32_t dy = -12; dy <= 12 && structureEntity == NULL_ENTITY; ++dy)
                {
                    for (int32_t dx = -12; dx <= 12; ++dx)
                    {
                        const Engine::CNavGrid::Cell candidate{
                            center.x + dx,
                            center.y + dy
                        };
                        if (room.m_pTerrainNavGrid->IsWalkable(candidate.x, candidate.y) &&
                            !room.m_pNavGrid->IsWalkable(candidate.x, candidate.y))
                        {
                            structureEntity = entity;
                            probeCell = candidate;
                            break;
                        }
                    }
                }
            });
        if (structureEntity == NULL_ENTITY)
        {
            outError = "no live structure-carved terrain cell was found";
            return false;
        }

        HealthComponent& health =
            room.m_world.GetComponent<HealthComponent>(structureEntity);
        const f32_t originalCurrent = health.fCurrent;
        const bool_t originalDead = health.bIsDead;
        health.fCurrent = 0.f;
        health.bIsDead = true;
        room.RefreshServerStructureNavigationIfNeeded();
        const bool_t bDeadReleased =
            room.m_pNavGrid->IsWalkable(probeCell.x, probeCell.y);

        health.fCurrent = originalCurrent;
        health.bIsDead = originalDead;
        room.RefreshServerStructureNavigationIfNeeded();
        const bool_t bRestoredBlocked =
            !room.m_pNavGrid->IsWalkable(probeCell.x, probeCell.y);
        if (!bDeadReleased || !bRestoredBlocked)
        {
            outError = "structure nav cell did not release/reblock across death restore";
            return false;
        }
        return true;
    }
```

Debug 전용 probe room들 뒤, 본 soak room 생성 전에 추가:

```cpp
    auto structureNavProbeRoom = CGameRoom::Create(
        options.roomId ^ 0x20000000u);
    if (!structureNavProbeRoom ||
        !CGameRoomIntegrationProbeAccess::PrepareBotMatch(
            *structureNavProbeRoom,
            options.seed,
            error) ||
        !CGameRoomIntegrationProbeAccess::RunStructureNavigationRefreshProbe(
            *structureNavProbeRoom,
            error))
    {
        if (structureNavProbeRoom)
            structureNavProbeRoom->Stop();
        std::cerr << "RESULT status=FAIL reason=structure_nav_probe_failed detail=\""
            << error << "\"\n";
        return 1;
    }
    std::cout << "STRUCTURE_NAV_PROBE status=PASS"
        << " live_blocked=1 dead_released=1 restored_blocked=1\n";
    structureNavProbeRoom->Stop();
    structureNavProbeRoom.reset();
```

## 3. 검증

예측:
- `STRUCTURE_NAV_PROBE status=PASS live_blocked=1 dead_released=1 restored_blocked=1`이 출력되고 1,800 tick soak에서 `minion_lane_slots=6`, `max_minion_lane_stall_ticks<=180`, `minion_opposed_yaw=0`이 유지된다.
- `--gamefeel-only`에서 `[AttackChaseResolved] PASS: offset=2.000`이 출력되고 시작·repath 모두 같은 보정 arrive radius를 사용한다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다. 이 변경은 nav 파생 상태와 GameSim move target만 갱신한다.
- measured scope는 harness `tick_p99_us`, 예산은 33,333us/tick이다. grid rebuild는 구조물 상태 변화 때만 발생한다.

검증 명령:

```powershell
git diff --check

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsroot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsroot 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Winters.sln '/t:Build' /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& Tools/Bin/Debug/SimLab.exe --gamefeel-only
& Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42 -Runs 2 -Configuration Debug -HeartbeatTicks 1800 -SkipServerBuild
```

미검증:
- 같은 카메라에서 파괴 포탑을 실제로 통과하는 미니언과 실제 칼리스타 공격 애니메이션은 normal F5 수동 시각 검증 영역이다. cell release probe와 기존 1,800 tick soak는 이를 보조하지만 실제 화면 횡단을 대체하지 않는다.

확인 필요:
- 없음. 소스 적용 전 독립 비평 게이트만 남았다.

## 협업·변경 차선

- 구현 직전 관련 파일별 `git diff`와 계획 anchor를 다시 확인한다. 기존 승리/경제 작업과 진행 중인 정글 오브젝트 세션의 변경은 사용자 소유 변경으로 보존한다.
- `GameRoom.h`, `GameRoomUnitAI.cpp`, `CommandExecutor.cpp`, SimLab, Harness처럼 이미 dirty인 파일은 primary agent 한 명이 exact anchor의 최소 `apply_patch`만 적용한다. 관련 anchor가 달라졌거나 작업 중 파일이 다시 변하면 즉시 중단하고 최신 delta를 재검토한다.
- 빌드는 다른 세션과 겹치지 않는 단일 MSBuild 차선에서 `/m:1 /nr:false`로만 실행한다. 빌드 중 생성물 또는 대상 파일이 외부에서 갱신되면 해당 결과를 폐기하고 다시 검증한다.
- `SanitizeServerMinionWaypointsOnNavGrid()`가 런타임 보정 waypoint를 유지하는 기존 동작은 이번 원인인 stale cell/path/flow 해제와 별개라 변경하지 않는다. 실제 파괴 포탑 횡단은 수동 5v5 관찰 항목으로 남긴다.

## 서브 에이전트 비평

- 1차 비평 주체: `/root/critique_nav_level_ui_plans` (read-only)
- 1차 판정: P0 0, P1 4, P2 3, 게이트 실패.
- 이 계획에 해당하는 처분:
  - terrain clone/rebuild 실패가 침묵한다는 P2를 수용했다. capture/carve를 `bool_t`로 바꾸고 실패 진단을 남기며, 원본 사본 또는 새 grid가 없으면 hash·flow 갱신 전에 refresh를 중단한다.
  - 보정 도착점 수식만 검사한다는 P2를 수용했다. 최악 방향 도착 위치에서도 raw target이 `effectiveRange` 안인지 SimLab이 직접 검증한다.
  - waypoint sanitize가 authored waypoint를 영구 변경할 수 있다는 P2는 이번 slice에서 보류한다. stale 구조물 cell/path/flow가 확정 원인이고, waypoint 원본 소유권 변경은 별도 수명 설계가 필요하므로 실제 미니언 횡단 QA로 잔여 위험을 명시한다.
  - dirty target 충돌 전략이 없다는 P1을 수용해 위 협업·변경 차선과 단일 빌드 차선을 추가했다.
- 최종 재비평: residual P0 0, P1 0. terrain/carve fail-fast, chase 시작·repath helper 공유, 최악 방향 raw target 거리 검증을 확인했다.
- 상태: 독립 비평 게이트 PASS, 소스 적용 가능.
