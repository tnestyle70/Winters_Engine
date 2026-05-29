Session - Server NavGrid source mismatch and Champion/Bot AI pathing jitter root-cause fix.

1. 반영해야 하는 코드

1-0. 코드 기준 결론

높이 보정과 navgrid nearest-cell 탐색은 별개다.

- `CScene_InGame::ResolveMouseMapSurfacePos()`는 마우스 ray의 XZ에 대해 `CMapSurfaceSampler::SampleHeight()`를 호출해서 Y만 맵 표면으로 보정한다. navgrid를 보지 않는다.
- `CScene_InGame::TryProjectToMapSurface()`도 동일하게 XZ를 유지하고 Y만 WMesh 높이로 바꾼다. "주변을 쭉 탐색해서 가장 가까운 낮은 cell"을 찾는 로직이 아니다.
- 가장 가까운 walkable cell 탐색은 `TryResolveNearestWalkablePosition()`, `TryResolveWalkableMoveTarget()`, `CGameRoom::TryResolveMoveTarget()`, `CGameRoom::TryBuildMovePath()` 쪽에서 `CNavGrid::TryFindNearestWalkableCell()` / `CPathfinder::TryFindNearestReachableGoal()`로 수행된다. 이 탐색 기준은 높이가 아니라 navgrid의 blocked/walkable bit다.
- 플레이어 클릭이 navgrid 파일 없이도 동작한 것처럼 보인 이유는 클라이언트 bootstrap이 authored navgrid 로드 실패 시 `CreateMapNavGrid()` 후 `SetAllWalkable(true)` fallback grid를 만들기 때문이다. 즉 navgrid 포인터는 존재하지만 지형 벽은 막혀 있지 않고, 클릭 위치의 Y만 WMesh로 보정된다.
- 현재 로그의 핵심은 서버가 `Client/Bin/Data/Stage1.navgrid` 0-byte 파일을 먼저 발견하고 로드 실패한 뒤, valid한 `Client/Bin/Debug/Data/Stage1.navgrid` 후보로 넘어가지 않는다는 점이다.
- 그 다음 서버 terrain bake는 `GetGameSimRosterSpawnPosition()` fallback seed `(-30, 6)` 같은 구식 seed가 Stage seed와 섞여 하나라도 uncovered가 되면 bake 전체를 버리고 `SetAllWalkable(true)`로 떨어진다.
- 결과적으로 서버 grid는 "지형 벽 없음 + 구조물 carve만 있음" 상태가 되고, 그래서 debug yellow cell은 구조물만 보이고 Bot AI는 벽을 넘을 수 있다. 억제기/타워 근처 jitter도 이 상태에서 path target과 구조물 carve가 반복 충돌하면서 재현될 가능성이 높다.

1-1. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
    bool_t TryResolveServerAuthoredNavGridPath(
        const wchar_t* pStagePath,
        std::wstring& outPath);

    bool_t TryLoadServerAuthoredNavGrid(
        const wchar_t* pStagePath,
        std::unique_ptr<Engine::CNavGrid>& outGrid);
```

아래로 교체:

```cpp
    void CollectServerAuthoredNavGridCandidates(
        const wchar_t* pStagePath,
        std::vector<std::wstring>& outCandidates);

    bool_t TryLoadServerAuthoredNavGrid(
        const wchar_t* pStagePath,
        std::unique_ptr<Engine::CNavGrid>& outGrid);
```

기존 코드:

```cpp
    bool_t TryResolveServerAuthoredNavGridPath(
        const wchar_t* pStagePath,
        std::wstring& outPath)
    {
        outPath.clear();

        std::vector<std::wstring> candidates{};
        if (pStagePath && pStagePath[0] != L'\0')
        {
            std::wstring fromStage = pStagePath;
            const size_t dot = fromStage.find_last_of(L'.');
            if (dot != std::wstring::npos)
                fromStage.resize(dot);
            fromStage += L".navgrid";
            PushUniqueServerPath(candidates, fromStage);
            PushSiblingDebugDataPath(candidates, fromStage);
        }

        wchar_t exePath[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash + 1);
                PushUniqueServerPath(candidates, exeDir + L"Data\\Stage1.navgrid");
                PushUniqueServerPath(candidates, exeDir + L"..\\Data\\Stage1.navgrid");
                PushUniqueServerPath(candidates, exeDir + L"..\\..\\..\\Client\\Bin\\Debug\\Data\\Stage1.navgrid");
                PushUniqueServerPath(candidates, exeDir + L"..\\..\\..\\Client\\Bin\\Data\\Stage1.navgrid");
            }
        }

        wchar_t cwd[MAX_PATH]{};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
        {
            std::wstring cwdDir = cwd;
            if (!cwdDir.empty() && cwdDir.back() != L'\\' && cwdDir.back() != L'/')
                cwdDir.push_back(L'\\');
            PushUniqueServerPath(candidates, cwdDir + L"Client\\Bin\\Debug\\Data\\Stage1.navgrid");
            PushUniqueServerPath(candidates, cwdDir + L"Client\\Bin\\Data\\Stage1.navgrid");
            PushUniqueServerPath(candidates, cwdDir + L"Server\\Bin\\Debug\\Data\\Stage1.navgrid");
        }

        for (const std::wstring& candidate : candidates)
        {
            if (TryResolveExistingServerPath(candidate, outPath))
            {
                std::wstring msg = L"[ServerNav] authored navgrid path=" + outPath + L"\n";
                OutputServerAITraceW(msg.c_str());
                return true;
            }
        }

        return false;
    }
```

아래로 교체:

```cpp
    void CollectServerAuthoredNavGridCandidates(
        const wchar_t* pStagePath,
        std::vector<std::wstring>& outCandidates)
    {
        outCandidates.clear();

        if (pStagePath && pStagePath[0] != L'\0')
        {
            std::wstring fromStage = pStagePath;
            const size_t dot = fromStage.find_last_of(L'.');
            if (dot != std::wstring::npos)
                fromStage.resize(dot);
            fromStage += L".navgrid";
            PushUniqueServerPath(outCandidates, fromStage);
            PushSiblingDebugDataPath(outCandidates, fromStage);
        }

        wchar_t exePath[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash + 1);
                PushUniqueServerPath(outCandidates, exeDir + L"Data\\Stage1.navgrid");
                PushUniqueServerPath(outCandidates, exeDir + L"..\\Data\\Stage1.navgrid");
                PushUniqueServerPath(outCandidates, exeDir + L"..\\..\\..\\Client\\Bin\\Debug\\Data\\Stage1.navgrid");
                PushUniqueServerPath(outCandidates, exeDir + L"..\\..\\..\\Client\\Bin\\Data\\Stage1.navgrid");
            }
        }

        wchar_t cwd[MAX_PATH]{};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
        {
            std::wstring cwdDir = cwd;
            if (!cwdDir.empty() && cwdDir.back() != L'\\' && cwdDir.back() != L'/')
                cwdDir.push_back(L'\\');
            PushUniqueServerPath(outCandidates, cwdDir + L"Client\\Bin\\Debug\\Data\\Stage1.navgrid");
            PushUniqueServerPath(outCandidates, cwdDir + L"Client\\Bin\\Data\\Stage1.navgrid");
            PushUniqueServerPath(outCandidates, cwdDir + L"Server\\Bin\\Debug\\Data\\Stage1.navgrid");
        }
    }
```

기존 코드:

```cpp
    bool_t TryLoadServerAuthoredNavGrid(
        const wchar_t* pStagePath,
        std::unique_ptr<Engine::CNavGrid>& outGrid)
    {
        outGrid.reset();

        std::wstring navGridPath{};
        if (!TryResolveServerAuthoredNavGridPath(pStagePath, navGridPath))
            return false;

        outGrid = Engine::CNavGrid::LoadFromFile(navGridPath.c_str());
        if (!outGrid)
        {
            std::wstring msg = L"[ServerNav] authored navgrid load failed path=" + navGridPath + L"\n";
            OutputServerAITraceW(msg.c_str());
            return false;
        }

        return true;
    }
```

아래로 교체:

```cpp
    bool_t TryLoadServerAuthoredNavGrid(
        const wchar_t* pStagePath,
        std::unique_ptr<Engine::CNavGrid>& outGrid)
    {
        outGrid.reset();

        std::vector<std::wstring> candidates{};
        CollectServerAuthoredNavGridCandidates(pStagePath, candidates);

        for (const std::wstring& candidate : candidates)
        {
            std::wstring navGridPath{};
            if (!TryResolveExistingServerPath(candidate, navGridPath))
                continue;

            std::unique_ptr<Engine::CNavGrid> grid =
                Engine::CNavGrid::LoadFromFile(navGridPath.c_str());
            if (!grid)
            {
                std::wstring msg = L"[ServerNav] authored navgrid load failed path=" + navGridPath + L"\n";
                OutputServerAITraceW(msg.c_str());
                continue;
            }

            std::wstring msg = L"[ServerNav] authored navgrid path=" + navGridPath + L"\n";
            OutputServerAITraceW(msg.c_str());
            outGrid = std::move(grid);
            return true;
        }

        return false;
    }
```

기존 코드:

```cpp
    std::vector<Vec3> seeds{};
    seeds.reserve(128);
```

아래에 추가:

```cpp
    bool_t stageSpawnSeedAdded[kGameRosterSlotCount]{};
```

기존 코드:

```cpp
        for (const LobbySlotState& slot : m_lobbySlots)
        {
            if (!slot.bHuman && !slot.bBot)
                continue;

            Vec3 spawn{};
            if (TryResolveStageFountainSpawn(
                *pStage,
                slot.slotId,
                static_cast<eTeam>(slot.team),
                spawn))
            {
                addSeed(spawn);
            }
        }
```

아래로 교체:

```cpp
        for (const LobbySlotState& slot : m_lobbySlots)
        {
            if (!slot.bHuman && !slot.bBot)
                continue;

            Vec3 spawn{};
            if (TryResolveStageFountainSpawn(
                *pStage,
                slot.slotId,
                static_cast<eTeam>(slot.team),
                spawn))
            {
                addSeed(spawn);
                if (slot.slotId < kGameRosterSlotCount)
                    stageSpawnSeedAdded[slot.slotId] = true;
            }
        }
```

기존 코드:

```cpp
    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (!slot.bHuman && !slot.bBot)
            continue;

        if (IsRedSylasSmokeDummySlot(slot))
        {
            addSeed(GetRedSylasSmokeDummyPosition());
            for (u8_t i = 0; i < GetRedSylasSmokePatrolPointCount(); ++i)
                addSeed(GetRedSylasSmokePatrolPoint(i));
            continue;
        }

        addSeed(GetGameSimRosterSpawnPosition(slot.slotId, slot.team, slot.bBot));
    }
```

아래로 교체:

```cpp
    for (const LobbySlotState& slot : m_lobbySlots)
    {
        if (!slot.bHuman && !slot.bBot)
            continue;

        if (IsRedSylasSmokeDummySlot(slot))
        {
            addSeed(GetRedSylasSmokeDummyPosition());
            for (u8_t i = 0; i < GetRedSylasSmokePatrolPointCount(); ++i)
                addSeed(GetRedSylasSmokePatrolPoint(i));
            continue;
        }

        if (pStage &&
            slot.slotId < kGameRosterSlotCount &&
            stageSpawnSeedAdded[slot.slotId])
        {
            continue;
        }

        addSeed(GetGameSimRosterSpawnPosition(slot.slotId, slot.team, slot.bBot));
    }
```

기존 코드:

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

아래로 교체:

```cpp
    u32_t uncoveredSeedCount = 0;
    if (bBaked)
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
            ++uncoveredSeedCount;
        }
    }

    if (!bBaked || m_pNavGrid->CountWalkableCells() == 0u)
    {
        m_pNavGrid->SetAllWalkable(true);
        OutputServerAITrace("[ServerNav] terrain bake failed; fallback all-walkable grid\n");
        OutputServerNavGridSummary("fallback authored grid", *m_pNavGrid);
        BuildServerPathNavGrid();
        return;
    }

    if (uncoveredSeedCount > 0u)
    {
        char seedSummary[192]{};
        sprintf_s(
            seedSummary,
            "[ServerNav] terrain bake kept with uncovered fallback seeds=%u walkable=%u\n",
            uncoveredSeedCount,
            m_pNavGrid->CountWalkableCells());
        OutputServerAITrace(seedSummary);
    }
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

수정 없음. 근거 확인용 앵커:

```cpp
        if (!tc.pWalkable->TryBuildMovePath(
            from,
            ioTarget,
            waypoints,
            kMovePathMaxWaypoints,
            waypointCount,
            resolved))
```

플레이어 우클릭 이동과 Bot AI가 만든 `Move` 명령은 여기서 같은 `tc.pWalkable->TryBuildMovePath()`로 들어간다. 따라서 우선 수정 지점은 AI별 분기 추가가 아니라 서버 `CGameRoom`의 navgrid source와 bake fallback이다.

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/ChampionAI/ChampionAISystem.cpp

수정 없음. 근거 확인용 앵커:

```cpp
        GameCommand move = MakeAICommand(ai, tc, self, eCommandKind::Move);
        move.groundPos = goal;
        outCommands.push_back(move);
```

Champion AI는 직접 transform을 움직이지 않고 서버 command만 만든다. 실제 경로 생성은 `CommandExecutor` -> `CGameRoom::TryBuildMovePath()`다.

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChase/AttackChaseSystem.cpp

수정 없음. 근거 확인용 앵커:

```cpp
            if (!tc.pWalkable->TryBuildMovePath(
                selfPos,
                target,
                waypoints,
                kMovePathMaxWaypoints,
                waypointCount,
                resolvedTarget))
```

타워/억제기 공격 chase도 같은 서버 walkable query로 path를 받는다. Phase 1 수정 후에도 같은 자리 떨림이 남으면 그때는 이 파일의 repath 주기/target stability를 별도 패치로 잡는다. 현재 증거상 1차 원인은 서버 grid가 terrain wall을 잃은 상태다.

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Move/MoveSystem.cpp

수정 없음. 근거 확인용 앵커:

```cpp
            if (!tc.pWalkable->TryClampMoveSegmentXZ(pos, next, radius, resolvedNext))
            {
                ClearMoveRuntimeTarget(moveTarget);
                if (!IsActionAnimationLocked(world, entity, stat, anim, tc))
                    SetNetAnimation(anim, eNetAnimId::Idle, tc, false);
                continue;
            }
```

MoveSystem은 path waypoint를 따라 이동하다가 서버 walkable query로 segment clamp를 수행한다. 서버 grid가 all-walkable이면 벽은 clamp 대상이 아니고, 구조물만 clamp 대상이 된다. 그래서 구조물 근처에서만 노란 cell/막힘이 보이고 지형 벽은 통과되는 현상과 일치한다.

1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

수정 없음. 근거 확인용 앵커:

```cpp
    if (!m_pMapSurfaceSampler->SampleHeight(ioPos.x, ioPos.z, height))
        return false;

    ioPos.y = height + fYOffset;
```

그리고:

```cpp
                if (!m_pMapSurfaceSampler->SampleHeight(p.x, p.z, height))
                {
                    bSurfaceHit = false;
                    break;
                }
```

이 두 지점이 "높이 보정"이다. navgrid nearest 탐색이 아니라 WMesh height sample이다.

1-7. C:/Users/user/Desktop/Winters/Client/Private/UI/DebugDrawSystem.cpp

수정 없음. 근거 확인용 앵커:

```cpp
                if (pGrid->IsWalkable(ix, iy)) continue;
                const f32_t x = ox + (ix + 0.5f) * cs;
                const f32_t z = oz + (iy + 0.5f) * cs;
                DrawWireBox(pDraw, mVP, { x, 0.1f, z }, { cs * 0.5f, 0.1f, cs * 0.5f }, 0x8000FFFFu, 1.f);
```

현재 debug draw는 blocked cell만 그린다. 즉 노란색은 blocked cell이고, walkable 전체를 흰색 촘촘한 grid로 그리는 기능은 현재 코드에 없다.

1-8. C:/Users/user/Desktop/Winters/Server/Include/Server.vcxproj

수정 없음. 이번 계획은 기존 cpp/h 안에서만 수정하므로 vcxproj 등록 변경이 필요 없다. 이미 아래 항목들이 포함되어 있다.

```xml
    <ClCompile Include="..\Private\Game\GameRoom.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Systems\CommandExecutor\CommandExecutor.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Systems\Move\MoveSystem.cpp" />
    <ClCompile Include="..\..\Shared\GameSim\Systems\AttackChase\AttackChaseSystem.cpp" />
```

2. 검증

2-1. 파일 상태 확인

```powershell
Get-Item C:\Users\user\Desktop\Winters\Client\Bin\Data\Stage1.navgrid, C:\Users\user\Desktop\Winters\Client\Bin\Debug\Data\Stage1.navgrid | Select-Object FullName,Length,LastWriteTime
```

현재 확인값:

```text
C:\Users\user\Desktop\Winters\Client\Bin\Data\Stage1.navgrid            Length=0
C:\Users\user\Desktop\Winters\Client\Bin\Debug\Data\Stage1.navgrid      Length=32788
```

2-2. 빌드

```powershell
msbuild C:\Users\user\Desktop\Winters\Winters.sln /t:Server /p:Configuration=Debug /p:Platform=x64
msbuild C:\Users\user\Desktop\Winters\Winters.sln /t:Client /p:Configuration=Debug /p:Platform=x64
```

2-3. 서버 로그 성공 기준

authored navgrid가 있는 경우 기대 로그:

```text
[ServerNav] authored navgrid load failed path=C:\Users\user\Desktop\Winters\Client\Bin\Data\Stage1.navgrid
[ServerNav] authored navgrid path=C:\Users\user\Desktop\Winters\Client\Bin\Debug\Data\Stage1.navgrid
[ServerNav] authored navgrid loaded origin=(-23.50,-128.00) walkable=...
[ServerNav] structures carved=30 walkable=...
[ServerNav] path grid inflated radius=0.50 authoredWalkable=... pathWalkable=...
```

authored navgrid를 일부러 지운 경우 기대 로그:

```text
[MapWalkable] candidates=... connected=... final=... seeds=...
[ServerNav] walkable grid baked cells=... seeds=... hash=...
[ServerNav] structures carved=30 walkable=...
```

실패 로그:

```text
[ServerNav] terrain bake failed or missed gameplay seeds; fallback all-walkable grid
[ServerNav] fallback authored grid origin=... walkable=262144
```

수정 후에는 valid authored navgrid가 있으면 위 실패 로그가 나오면 안 된다. authored navgrid가 없더라도 bake 성공 cell이 있으면 seed 일부 uncovered만으로 all-walkable로 떨어지면 안 된다.

2-4. 클라이언트 debug draw 확인

Render Debug에서 아래를 켠다.

```text
NavGrid blocked cells
PathGrid blocked cells
```

기대 결과:

- 노란색 `NavGrid blocked cells`는 authored grid 기준 blocked cell이다.
- 보라/주황 계열 `PathGrid blocked cells`는 agent radius inflate 후의 blocked cell이다.
- 현재 코드는 walkable 전체 흰 grid를 그리지 않는다. 흰 grid가 필요하면 별도 debug draw 기능을 추가해야 한다.
- 구조물만 보이고 지형 벽 blocked cell이 안 보이면 서버/클라가 all-walkable fallback 또는 구조물 carve만 보고 있는 상태를 의심한다.

2-5. 플레이어 이동 검증

검증 케이스:

- Red team Irelia spawn 후 즉시 우클릭 이동.
- Blue team Kalista가 bot lane 쪽으로 이동, 1차 포탑 아래쪽/벽 클릭.
- 지형 벽 위를 우클릭했을 때 raw click Y는 WMesh height로 잡히되, 최종 이동 path는 `TryBuildMovePath()`에서 nearest reachable walkable goal로 보정되어야 한다.
- 서버 로그에 `[Command] move reject reason=no-grid-path`가 반복되면 navgrid origin/coverage 문제를 다시 본다.

2-6. Bot AI 검증

검증 케이스:

- Bot이 lane goal / safe anchor로 이동할 때 terrain wall을 직선으로 넘지 않는다.
- 억제기 타워/타워 공격 chase 중 같은 위치에서 떨리는 현상이 사라지는지 확인한다.
- `ChampionAI` 로그의 `cmdPos`가 지형 밖/구조물 중심이어도 `ServerPath` 로그에서 `resolved`가 reachable cell로 보정되어야 한다.
- `start blocked` 로그가 반복되면 spawn sanitize와 structure carve 반경을 다음 패치 대상으로 본다.

2-7. Phase 1 이후에도 jitter가 남을 때의 다음 확인점

아래 증거가 있을 때만 추가 코드를 넣는다.

- `ServerNav`가 authored/baked terrain grid를 정상 사용한다.
- Bot이 wall-crossing은 하지 않는다.
- 그런데 특정 tower/inhibitor around chase에서만 path가 짧게 재생성되며 떨린다.

그 경우 다음 패치는 `AttackChaseSystem.cpp`의 chase target stability 또는 `MoveSystem.cpp`의 no-progress repath guard를 다룬다. 지금 단계에서 먼저 넣지 않는 이유는 현재 로그만으로는 근본 원인이 server navgrid source/fallback에 있고, 공유 이동 시스템에 상태 필드를 추가하면 deterministic sim surface가 커지기 때문이다.
