# Session - Red Team AI / Red Player Two-Track Fix

## 0. 전제와 성공 기준

- 서버 권위 흐름은 유지한다: `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`.
- Red AI가 멈춘 1차 원인은 서버가 `Stage1.navgrid`를 무조건 채택하면서 발생한다. 현재 authored navgrid는 x 범위가 `-128..128`인데, Stage1 Red 기지/웨이포인트는 x `180`대라 Red 목적지와 Red 미니언 스폰이 navgrid 바깥으로 떨어진다.
- AI override가 `none`으로 바로 돌아가는 현상은 Red 이동 목적지가 서버 navgrid 밖이라 move command가 실행 단계에서 무효화되는 것과, AI debug mask가 decision/action-lock early return 뒤에서만 갱신되는 문제가 겹친다.
- Red 플레이어 검증은 기존 `--smoke-slot=5`로 가능하지만, 팀 기준 검증 루트가 명확하지 않다. `--smoke-team=red`/`--smoke-red`를 추가하고, 네트워크 권위 모드에서는 stale client navgrid가 move command 전송을 막지 않게 한다.

## 1. 반영해야 하는 코드

### 파일: `Server/Private/Game/GameRoom.cpp`

#### 1-A. Stage bounds를 덮지 못하는 authored navgrid 거부

기존 앵커:

```cpp
    constexpr int32_t kStageChampionSpawnWalkableSearchRadius = 16;
    constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;
```

아래로 교체:

```cpp
    constexpr int32_t kStageChampionSpawnWalkableSearchRadius = 16;
    constexpr f32_t kServerNavGridStageBoundsPadding = 4.f;
    constexpr int32_t kServerNavGridSeedCoverageRadius = 24;
    constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;
```

기존 앵커:

```cpp
    struct StageBaseAnchors
    {
        Vec3 nexus{};
        Vec3 twinCenter{};
        bool_t bHasNexus = false;
        bool_t bHasTwinCenter = false;
    };
```

아래에 추가:

```cpp
    struct StageGameplayBounds
    {
        f32_t minX = (std::numeric_limits<f32_t>::max)();
        f32_t minZ = (std::numeric_limits<f32_t>::max)();
        f32_t maxX = -(std::numeric_limits<f32_t>::max)();
        f32_t maxZ = -(std::numeric_limits<f32_t>::max)();
        bool_t bAny = false;
    };

    void IncludeStageGameplayBoundsPoint(StageGameplayBounds& bounds, const Vec3& p)
    {
        bounds.minX = (std::min)(bounds.minX, p.x);
        bounds.minZ = (std::min)(bounds.minZ, p.z);
        bounds.maxX = (std::max)(bounds.maxX, p.x);
        bounds.maxZ = (std::max)(bounds.maxZ, p.z);
        bounds.bAny = true;
    }

    bool_t BuildStageGameplayBounds(
        const Winters::Map::StageData& stage,
        StageGameplayBounds& outBounds)
    {
        outBounds = StageGameplayBounds{};

        for (const auto& waypoint : stage.minionWaypoints)
        {
            IncludeStageGameplayBoundsPoint(
                outBounds,
                Vec3{ waypoint.px, waypoint.py, waypoint.pz });
        }

        for (const auto& structure : stage.structures)
        {
            if (structure.bVisible == 0u)
                continue;

            IncludeStageGameplayBoundsPoint(
                outBounds,
                Vec3{ structure.px, structure.py, structure.pz });
        }

        return outBounds.bAny;
    }

    bool_t DoesServerNavGridCoverStageBounds(
        const Engine::CNavGrid& navGrid,
        const Winters::Map::StageData& stage,
        f32_t padding,
        StageGameplayBounds& outBounds)
    {
        if (!BuildStageGameplayBounds(stage, outBounds))
            return true;

        const f32_t navMinX = navGrid.Get_OriginX();
        const f32_t navMinZ = navGrid.Get_OriginZ();
        const f32_t navMaxX =
            navMinX + Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize;
        const f32_t navMaxZ =
            navMinZ + Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize;

        return outBounds.minX >= navMinX + padding &&
            outBounds.minZ >= navMinZ + padding &&
            outBounds.maxX <= navMaxX - padding &&
            outBounds.maxZ <= navMaxZ - padding;
    }
```

기존 앵커:

```cpp
    if (TryLoadServerAuthoredNavGrid(pStagePath, authoredGrid))
    {
        m_pNavGrid = std::move(authoredGrid);
        OutputServerNavGridSummary("authored navgrid loaded", *m_pNavGrid);
        BuildServerPathNavGrid();
        return;
    }

    OutputServerAITrace("[ServerNav] authored navgrid missing; fallback bake will not match yellow debug cells\n");
```

아래로 교체:

```cpp
    if (TryLoadServerAuthoredNavGrid(pStagePath, authoredGrid))
    {
        bool_t bUseAuthoredGrid = true;
        StageGameplayBounds stageBounds{};
        if (pStage &&
            !DoesServerNavGridCoverStageBounds(
                *authoredGrid,
                *pStage,
                kServerNavGridStageBoundsPadding,
                stageBounds))
        {
            char msg[320]{};
            sprintf_s(
                msg,
                "[ServerNav] authored navgrid rejected: stage bounds x=(%.2f,%.2f) z=(%.2f,%.2f) outside origin=(%.2f,%.2f) size=(%.2f,%.2f)\n",
                stageBounds.minX,
                stageBounds.maxX,
                stageBounds.minZ,
                stageBounds.maxZ,
                authoredGrid->Get_OriginX(),
                authoredGrid->Get_OriginZ(),
                Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize,
                Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize);
            OutputServerAITrace(msg);
            bUseAuthoredGrid = false;
        }

        if (bUseAuthoredGrid)
        {
            m_pNavGrid = std::move(authoredGrid);
            OutputServerNavGridSummary("authored navgrid loaded", *m_pNavGrid);
            BuildServerPathNavGrid();
            return;
        }
    }

    OutputServerAITrace("[ServerNav] authored navgrid missing or rejected; fallback bake will not match yellow debug cells\n");
```

기존 앵커:

```cpp
    if (!bBaked)
    {
        m_pNavGrid->SetAllWalkable(true);
        OutputServerAITrace("[ServerNav] terrain bake failed; fallback all-walkable grid\n");
        OutputServerNavGridSummary("fallback authored grid", *m_pNavGrid);
        BuildServerPathNavGrid();
        return;
    }
```

아래로 교체:

```cpp
    bool_t bSeedsCovered = bBaked;
    if (bSeedsCovered)
    {
        for (const Vec3& seed : seeds)
        {
            const Engine::CNavGrid::Cell cell = m_pNavGrid->WorldToCell(seed);
            Engine::CNavGrid::Cell nearest{};
            if (m_pNavGrid->IsWalkable(cell.x, cell.y) ||
                m_pNavGrid->TryFindNearestWalkableCell(
                    cell,
                    kServerNavGridSeedCoverageRadius,
                    nearest))
            {
                continue;
            }

            char seedMsg[192]{};
            sprintf_s(
                seedMsg,
                "[ServerNav] terrain bake seed uncovered pos=(%.2f,%.2f) cell=(%d,%d)\n",
                seed.x,
                seed.z,
                cell.x,
                cell.y);
            OutputServerAITrace(seedMsg);
            bSeedsCovered = false;
            break;
        }
    }

    if (!bBaked || !bSeedsCovered)
    {
        m_pNavGrid->SetAllWalkable(true);
        OutputServerAITrace("[ServerNav] terrain bake failed or missed gameplay seeds; fallback all-walkable grid\n");
        OutputServerNavGridSummary("fallback authored grid", *m_pNavGrid);
        BuildServerPathNavGrid();
        return;
    }
```

### 파일: `Shared/GameSim/Systems/ChampionAISystem.cpp`

#### 1-B. Debug mask 갱신을 decision/action-lock early return 앞으로 이동

기존 앵커:

```cpp
            ai.decisionTimer -= tc.fDt;
            if (ai.decisionTimer > 0.f)
                return;

            if (IsChampionAIActionLocked(world, self, champion.id, tc))
                return;

            ai.decisionTimer = ai.decisionInterval;

            const Vec3 selfPos = selfTf.GetPosition();
            const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
```

아래로 교체:

```cpp
            ai.decisionTimer -= tc.fDt;

            const Vec3 selfPos = selfTf.GetPosition();
            const ChampionAIProfile& profile = GetChampionAIProfile(champion.id);
```

기존 앵커:

```cpp
            ai.debugAvailableSkillMask =
                BuildChampionAIAvailableSkillMask(world, self, champion.id, profile, ctx);

            if (ai.debugForcedDecisionCount > 0)
```

아래로 교체:

```cpp
            ai.debugAvailableSkillMask =
                BuildChampionAIAvailableSkillMask(world, self, champion.id, profile, ctx);

            const bool_t bHasDebugOverride = ai.debugForcedDecisionCount > 0;
            if (ai.decisionTimer > 0.f && !bHasDebugOverride)
                return;

            if (IsChampionAIActionLocked(world, self, champion.id, tc))
                return;

            ai.decisionTimer = ai.decisionInterval;

            if (ai.debugForcedDecisionCount > 0)
```

### 파일: `Client/Private/Scene/InGamePlayerControlBridge.cpp`

#### 1-C. 네트워크 권위 모드에서 stale client navgrid가 move command를 막지 않게 처리

기존 앵커:

```cpp
            const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
            if (bValidGround &&
                scene.TryResolveWalkableMoveTarget(
                    ground,
                    resolvedGround,
                    &predictedFacingTarget))
            {
```

아래로 교체:

```cpp
            const bool_t bValidGround = fabsf(ground.x) + fabsf(ground.z) > 0.001f;
            bool_t bAcceptedMoveTarget = false;
            if (bValidGround)
            {
                bAcceptedMoveTarget = scene.TryResolveWalkableMoveTarget(
                    ground,
                    resolvedGround,
                    &predictedFacingTarget);
                if (!bAcceptedMoveTarget && bNetworkActive)
                {
                    resolvedGround = ground;
                    predictedFacingTarget = ground;
                    bAcceptedMoveTarget = true;
                }
            }

            if (bAcceptedMoveTarget)
            {
```

### 파일: `Client/Private/Scene/Scene_BanPick.cpp`

#### 1-D. Red 플레이어 스모크 진입 옵션 추가

기존 앵커:

```cpp
        u32_t slot = 0;
        if (ParseCommandU32(pCommandLine, L"--smoke-slot=", slot) && slot < kGameRosterSlotCount)
            options.slotId = static_cast<u8_t>(slot);
```

아래로 교체:

```cpp
        u32_t slot = 0;
        bool_t bExplicitSlot = false;
        if (ParseCommandU32(pCommandLine, L"--smoke-slot=", slot) && slot < kGameRosterSlotCount)
        {
            options.slotId = static_cast<u8_t>(slot);
            bExplicitSlot = true;
        }

        if (!bExplicitSlot)
        {
            wchar_t teamToken[16] = {};
            const bool_t bHasTeamToken =
                ParseCommandToken(pCommandLine, L"--smoke-team=", teamToken, 16);
            if (HasCommandFlag(pCommandLine, L"--smoke-red") ||
                (bHasTeamToken && _wcsicmp(teamToken, L"red") == 0))
            {
                options.slotId = 5;
            }
            else if (HasCommandFlag(pCommandLine, L"--smoke-blue") ||
                (bHasTeamToken && _wcsicmp(teamToken, L"blue") == 0))
            {
                options.slotId = 0;
            }
        }
```

## 2. 검증

1. 정적 검증
   - `git diff --check`
   - 기대: 공백 오류 없음. 기존 LF/CRLF 경고는 허용.

2. 빌드 검증
   - Server Debug x64 빌드.
   - Client Debug x64 빌드.
   - 기대: `GameRoom.cpp`, `ChampionAISystem.cpp`, `InGamePlayerControlBridge.cpp`, `Scene_BanPick.cpp` 컴파일 성공.

3. Red AI 수동 검증
   - 서버/클라 실행 후 Red AI 위치가 x `180`대 stage spawn에서 시작하는지 확인.
   - AI 패널에서 Red 챔피언의 `Safe`, `Wave` override를 누른 뒤 `MoveToOuterTurret` 또는 `LaneCombat`이 실제 위치 변화로 이어지는지 확인.
   - 서버 로그에 `authored navgrid rejected` 이후 fallback bake/path grid 로그가 찍히는지 확인.

4. Red 플레이어 검증
   - BanPick smoke를 `--banpick-smoke --smoke-team=red --smoke-start` 또는 `--banpick-smoke --smoke-red --smoke-start`로 실행.
   - 로컬 player `MySlotId=5`, `MyTeam=1`로 인게임 진입하는지 확인.
   - Red 플레이어 우클릭 이동이 stale client navgrid 때문에 전송 전 단계에서 막히지 않는지 확인.
