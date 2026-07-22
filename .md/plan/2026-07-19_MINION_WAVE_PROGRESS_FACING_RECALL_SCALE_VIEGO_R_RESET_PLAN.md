Session - 바텀 미니언 진행·탑 미니언 시선·귀환 FX 축소·비에고 강탈 R 초기화를 한 권위 경로에서 수정하고 검증
좌표: 없음 · 축: C2 이동>계산, C7 권위·통합
관련: 2026-07-18_CHAMPION_AI_AGGRESSION_MID_GROUP_YONE_COMBO_MINION_STUCK_PLAN.md / RESULT.md, 2026-07-18_IRELIA_E_UNIT_HIT_MARK_Q_RESET_RECALL_6S_PLAN.md / RESULT.md

## 1. 결정 기록

- 문제·제약: `Stage1.dat` 바텀 원본 waypoint 순서는 전진하지만 flow field는 셀마다 전체 lane segment 중 기하학적 최단 1개만 저장해 `currentWaypoint` 진행도를 모른다. 현재 flow는 목표 거리 비진행 이동을 4 tick까지 적용하고, 일반 이동 helper의 부분 이동·depenetration은 실제 목표 진행 없이도 `BlockedMoveFrames=0`으로 만든다.
- 시선 판정: A* 경로는 시작 셀을 건너뛰므로 “이전 셀을 waypoint로 다시 삽입”하는 증거는 없다. `FaceServerMinionTowardDirection`도 전달된 변위로 yaw를 만든다. 다만 UnitAI 뒤 depenetration이 위치만 쓰고 yaw를 갱신하지 않아 최종 snapshot 위치 변위와 시선이 어긋날 수 있다.
- 선택: 정규화 flow가 현재 lane waypoint 방향과 `-0.05`보다 작은 내적이면 같은 tick에 waypoint/A* fallback으로 넘기고, flow/toward helper 부분 이동의 성공 판단을 목표 거리 감소로 통일하며 blocked counter를 255에서 포화시킨다. 후처리 depenetration도 실제 적용 변위로 시선을 갱신한다.
- 통합 경계: 미니언 이동·yaw는 Server 권위, 비에고 cooldown은 Shared/GameSim 권위, 귀환 크기는 Client visual/WFX authoring이다. Bot AI는 계속 `GameCommand` 생산자이고 리신 FollowWave 로직은 수정하지 않는다.
- 검증 기준: GameRoom 30Hz에서 첫 웨이브가 나오는 300 tick 이후 Blue/Red × Top/Mid/Bot 6 슬롯을 관측한다. LaneMove가 활성 A* waypoint(없으면 lane waypoint) 최저 거리 기록을 180 tick(6초) 넘게 갱신하지 않거나, 정규화한 실제 XZ 변위와 저장 yaw의 내적이 `-0.05`보다 작으면 실패한다.
- 비에고: 강탈 직전 R cooldown이 80초여도 성공한 soul consume 명령이 `bPossessionPending=true`를 설정하는 즉시 R runtime 전체가 0이어야 한다. consume channel 동안 cast는 기존 action lock이 막고, form 적용 뒤 바로 R 사용이 가능하며 사용 시 정상 cooldown을 다시 시작하면서 form을 해제해야 한다.
- 귀환: 런타임 attack-range diameter override와 `recall.wfx` authoring 기본값을 모두 정확히 1/3(15.9→5.3)로 줄인다.
- 비용: 기존 Server/Client/GameSim/SimLab와 GameRoom harness만 사용하며 새 런타임 소유자·두 번째 이동기·클라이언트 gameplay truth를 만들지 않는다.
- 예산: 이번 구현/통합 검증을 목표 직접 산출물인 천장 작업 30% 이상으로 고정하고, 인접 AI 튜닝·flow-field 재설계·WFX 미감 튜닝은 범위 밖으로 둔다.
- 대가: flow field 자체에 progress index를 넣는 큰 재설계는 보류한다. 현재 waypoint와 모순되는 셀 방향만 거부해 기존 빠른 경로를 보존하고, 후속 WFX 미감은 사용자가 도구에서 조정한다.
- 협업 안전: 대상 cpp와 harness에는 기존 dirty 변경이 있고 `recall.wfx`는 untracked다. 구현 직전 scoped diff/status를 다시 읽고 아래 exact anchor만 패치하며 인접 변경을 덮어쓰거나 정리하지 않는다.

## 2. 반영해야 하는 코드

### 2-0. C:/Users/user/Desktop/Winters/Server/Public/Game/GameRoom.h

`m_serverMinionWaves` 아래에 snapshot/checkpoint truth가 아닌 현재 tick facing 계산용 시작 위치를 보관한다. 다음 UnitAI 시작 때 비우고 entity key lookup만 하므로 새 이동 owner가 아니다.

```cpp
    std::unordered_map<EntityID, Vec3> m_serverMinionTickStartPositions{};
```

### 2-1. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoomUnitAI.cpp

기존 `FaceServerMinionTowardDirection` 바로 아래에 blocked counter 포화와 목표 진행 판정을 추가한다.

```cpp
    void RecordServerMinionBlockedMove(MinionStateComponent& state)
    {
        if (state.BlockedMoveFrames < (std::numeric_limits<u8_t>::max)())
            ++state.BlockedMoveFrames;
    }

    void UpdateServerMinionMoveProgress(
        MinionStateComponent& state,
        const Vec3& vBefore,
        const Vec3& vAfter,
        const Vec3& vGoal)
    {
        const f32_t fSlackSq =
            ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                .minionBehavior.flowFieldProgressSlackSq;
        const bool_t bProgressed =
            WintersMath::DistanceSqXZ(vAfter, vGoal) + fSlackSq <
            WintersMath::DistanceSqXZ(vBefore, vGoal);
        if (bProgressed)
            state.BlockedMoveFrames = 0u;
        else
            RecordServerMinionBlockedMove(state);
    }
```

`Phase_ServerUnitAI`은 수집 직전 map을 비우고, 각 유효 minion transform을 얻은 직후 tick 시작 위치를 기록한다.

```cpp
    m_serverMinionTickStartPositions.clear();
```

```cpp
        m_serverMinionTickStartPositions[entity] = transform.GetPosition();
```

`Phase_ServerMinionDepenetration`의 위치 적용 블록을 아래로 교체한다. 1차 integration에서 마지막 correction 방향만 사용하면 tick 전체 순변위와 반대가 될 수 있음이 확인됐으므로, 가능한 경우 UnitAI 시작 위치부터 최종 위치까지의 aggregate 변위를 사용한다.

기존 코드:

```cpp
        transform.SetPosition(vResolved);
```

아래로 교체:

```cpp
        Vec3 vFacingOrigin = vPos;
        const auto startIt = m_serverMinionTickStartPositions.find(entity);
        if (startIt != m_serverMinionTickStartPositions.end())
            vFacingOrigin = startIt->second;
        const Vec3 vActualMove{
            vResolved.x - vFacingOrigin.x,
            0.f,
            vResolved.z - vFacingOrigin.z };
        transform.SetPosition(vResolved);
        FaceServerMinionTowardDirection(transform, vActualMove);
```

`TryMoveServerMinionToward`의 depenetration 성공 블록과 일반 성공 블록은 실제 목표 진행으로 blocked 상태를 갱신하도록 아래로 교체한다.

```cpp
            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = moveState;
            UpdateServerMinionMoveProgress(state, vPos, vDepenetrated, vMoveGoal);
            outMoved = true;
            return true;
```

```cpp
    const Vec3 vActualMove{ vNext.x - vPos.x, 0.f, vNext.z - vPos.z };
    transform.SetPosition(vNext);
    FaceServerMinionTowardDirection(transform, vActualMove);
    state.current = moveState;
    UpdateServerMinionMoveProgress(state, vPos, vNext, vMoveGoal);
    outMoved = true;
    return true;
```

`++state.BlockedMoveFrames;` 두 지점은 아래로 교체한다.

```cpp
        RecordServerMinionBlockedMove(state);
```

`TryMoveServerMinionByFlowFields`에서 `fLenSq` 검사 바로 아래에 현재 waypoint 역방향 flow 거부를 추가한다. 직교/코너 방향은 허용하고 명확한 후진만 fallback한다.

```cpp
    const Vec3 vLaneTargetDelta{
        vLaneTarget.x - vPos.x,
        0.f,
        vLaneTarget.z - vPos.z };
    const Vec3 vLaneTargetDirection = WintersMath::NormalizeXZOrZero(
        vLaneTargetDelta,
        std::numeric_limits<f32_t>::epsilon());
    const f32_t fFlowLaneAlignment =
        vDir.x * vLaneTargetDirection.x + vDir.z * vLaneTargetDirection.z;
    if (fFlowLaneAlignment < -0.05f)
    {
        static u32_t s_flowFieldOpposedLogCount = 0u;
        if (s_flowFieldOpposedLogCount < 64u)
        {
            char msg[320]{};
            sprintf_s(
                msg,
                "[UnitAI] flow fallback reason=opposes-waypoint entity=%u team=%u lane=%u "
                "blocked=%u alignment=%.3f pos=(%.2f,%.2f) target=(%.2f,%.2f)\n",
                static_cast<u32_t>(entity),
                static_cast<u32_t>(state.team),
                static_cast<u32_t>(state.lane),
                static_cast<u32_t>(state.BlockedMoveFrames),
                fFlowLaneAlignment,
                vPos.x,
                vPos.z,
                vLaneTarget.x,
                vLaneTarget.z);
            OutputServerAITrace(msg);
            ++s_flowFieldOpposedLogCount;
        }
        return false;
    }
```

flow depenetration 성공 블록은 목표 비진행이 4 tick에 도달하면 이동을 적용하지 않고 fallback하도록 아래로 교체한다.

```cpp
            UpdateServerMinionMoveProgress(state, vPos, vDepenetrated, vLaneTarget);
            if (state.BlockedMoveFrames >=
                ServerData::GetActiveLoLSpawnObjectDefinitionPack()
                    .minionBehavior.flowFieldStallFramesBeforePathFallback)
            {
                return false;
            }

            const Vec3 vActualMove{ vDepenetrated.x - vPos.x, 0.f, vDepenetrated.z - vPos.z };
            transform.SetPosition(vDepenetrated);
            FaceServerMinionTowardDirection(transform, vActualMove);
            state.current = MinionStateComponent::LaneMove;
            outMoved = true;
            return true;
```

flow 일반 분기의 raw increment는 `RecordServerMinionBlockedMove(state);`로 교체한다. 이미 있는 거리 감소/4 tick fallback은 유지한다.

### 2-2. C:/Users/user/Desktop/Winters/Tools/Harness/GameRoomBotMatchSoak.cpp

`Game/GameRoom.h` 아래에 서버와 동일한 red-lane waypoint mapping helper 선언을 포함한다.

```cpp
#include "Server/Private/Game/GameRoomInternal.h"
```

`kDeadlineMissRateDenominator` 아래에 통합 검증 상수와 tracker를 추가한다.

```cpp
    constexpr u64_t kMinionLaneStallLimitTicks = 180u;
    constexpr f32_t kMinionProgressSlackSq = 0.01f;

    struct MinionMotionSample
    {
        EntityHandle handle{};
        Vec3 previousPosition{};
        Vec3 activeGoal{};
        f32_t bestWaypointDistanceSq = (std::numeric_limits<f32_t>::max)();
        u32_t activeGoalIndex = 0u;
        u64_t lastProgressTick = 0u;
        bool_t bInitialized = false;
        bool_t bTrackingLaneMove = false;
        bool_t bTrackingPathGoal = false;
    };

    struct MinionMotionTracker
    {
        std::unordered_map<EntityID, MinionMotionSample> samples{};
        std::array<bool_t, 6u> observedLaneSlots{};
        u64_t maxLaneStallTicks = 0u;
        u32_t stalledLaneMinionCount = 0u;
        u32_t opposedYawCount = 0u;
        EntityID firstOpposedYawEntity = NULL_ENTITY;
    };
```

`CGameRoomIntegrationProbeAccess::ObserveLifecycle` 아래에 실제 GameRoom 미니언 위치·waypoint·yaw 검증을 추가한다.

```cpp
    static void ObserveMinionMotion(
        CGameRoom& room,
        u64_t tick,
        MinionMotionTracker& tracker)
    {
        tracker.stalledLaneMinionCount = 0u;
        room.m_world.ForEach<MinionStateComponent>(
            [&](EntityID entity, MinionStateComponent& state)
            {
                if (!room.m_world.HasComponent<TransformComponent>(entity))
                    return;

                const TransformComponent& transform =
                    room.m_world.GetComponent<TransformComponent>(entity);
                const Vec3 position = transform.GetPosition();
                MinionMotionSample& sample = tracker.samples[entity];
                const EntityHandle handle = room.m_world.GetEntityHandle(entity);
                if (!sample.bInitialized || sample.handle != handle)
                {
                    sample = MinionMotionSample{};
                    sample.handle = handle;
                    sample.previousPosition = position;
                    sample.lastProgressTick = tick;
                    sample.bInitialized = true;
                }

                const bool_t bHasPathGoal =
                    state.PathCount > 0u && state.PathIndex < state.PathCount;
                const u8_t waypointLane = ResolveServerWaypointLane(
                    state.team,
                    state.lane);
                const u32_t waypointCount = room.GetServerMinionWaypointCount(
                    state.team,
                    waypointLane);
                const bool_t bTrackLaneMove =
                    state.current == MinionStateComponent::LaneMove &&
                    (bHasPathGoal || state.currentWaypoint < waypointCount);
                if (!bTrackLaneMove)
                {
                    sample.previousPosition = position;
                    sample.lastProgressTick = tick;
                    sample.bTrackingLaneMove = false;
                    return;
                }

                const u32_t teamIndex = static_cast<u32_t>(state.team);
                if (teamIndex < 2u && state.lane < 3u)
                    tracker.observedLaneSlots[teamIndex * 3u + state.lane] = true;

                const Vec3 target = bHasPathGoal
                    ? state.PathWaypoints[state.PathIndex]
                    : room.GetServerMinionWaypoint(
                        state.team,
                        waypointLane,
                        state.currentWaypoint);
                const u32_t activeGoalIndex = bHasPathGoal
                    ? static_cast<u32_t>(state.PathIndex)
                    : state.currentWaypoint;
                const f32_t targetDistanceSq =
                    WintersMath::DistanceSqXZ(position, target);

                if (sample.bTrackingLaneMove)
                {
                    const Vec3 displacement{
                        position.x - sample.previousPosition.x,
                        0.f,
                        position.z - sample.previousPosition.z };
                    const f32_t moveLengthSq =
                        displacement.x * displacement.x + displacement.z * displacement.z;
                    if (moveLengthSq > 0.0001f)
                    {
                        const f32_t yaw = transform.GetRotation().y;
                        const f32_t facingDotMove =
                            (-std::sin(yaw) * displacement.x -
                                std::cos(yaw) * displacement.z) /
                            std::sqrt(moveLengthSq);
                        if (facingDotMove < -0.05f)
                        {
                            ++tracker.opposedYawCount;
                            if (tracker.firstOpposedYawEntity == NULL_ENTITY)
                                tracker.firstOpposedYawEntity = entity;
                        }
                    }
                }

                const bool_t bGoalChanged =
                    !sample.bTrackingLaneMove ||
                    sample.bTrackingPathGoal != bHasPathGoal ||
                    sample.activeGoalIndex != activeGoalIndex ||
                    WintersMath::DistanceSqXZ(sample.activeGoal, target) > 0.01f;
                if (bGoalChanged)
                {
                    sample.activeGoal = target;
                    sample.activeGoalIndex = activeGoalIndex;
                    sample.bestWaypointDistanceSq = targetDistanceSq;
                    sample.lastProgressTick = tick;
                }
                else if (targetDistanceSq + kMinionProgressSlackSq <
                    sample.bestWaypointDistanceSq)
                {
                    sample.bestWaypointDistanceSq = targetDistanceSq;
                    sample.lastProgressTick = tick;
                }

                const u64_t stallTicks = tick - sample.lastProgressTick;
                tracker.maxLaneStallTicks =
                    (std::max)(tracker.maxLaneStallTicks, stallTicks);
                if (stallTicks > kMinionLaneStallLimitTicks)
                    ++tracker.stalledLaneMinionCount;

                sample.previousPosition = position;
                sample.bTrackingLaneMove = true;
                sample.bTrackingPathGoal = bHasPathGoal;
            });
    }
```

main loop 초기화·tick 관측·실패 게이트를 추가한다.

```cpp
    LifecycleTracker lifecycle{};
    MinionMotionTracker minionMotion{};
    CGameRoomIntegrationProbeAccess::ObserveLifecycle(*room, 0u, lifecycle);
    CGameRoomIntegrationProbeAccess::ObserveMinionMotion(*room, 0u, minionMotion);
```

```cpp
        CGameRoomIntegrationProbeAccess::ObserveLifecycle(*room, tick, lifecycle);
        CGameRoomIntegrationProbeAccess::ObserveMinionMotion(*room, tick, minionMotion);
        if (minionMotion.opposedYawCount != 0u)
        {
            failure = "lane minion faced opposite its applied movement";
            break;
        }
        if (minionMotion.stalledLaneMinionCount != 0u)
        {
            failure = "lane minion made no waypoint progress for over 6 seconds";
            break;
        }
```

최종 tick 검사 뒤 600 tick 이상 run은 6개 팀·라인 슬롯 관측을 요구한다.

```cpp
    const u32_t observedMinionLaneSlotCount = static_cast<u32_t>(std::count(
        minionMotion.observedLaneSlots.begin(),
        minionMotion.observedLaneSlots.end(),
        true));
    if (failure.empty() && finalTick >= 600u && observedMinionLaneSlotCount != 6u)
        failure = "GameRoom soak did not observe all six team/lane minion slots";
```

`RESULT` 출력에 아래 필드를 추가한다.

```cpp
        << " minion_lane_slots=" << observedMinionLaneSlotCount
        << " max_minion_lane_stall_ticks=" << minionMotion.maxLaneStallTicks
        << " minion_opposed_yaw=" << minionMotion.opposedYawCount
        << " first_opposed_yaw_entity="
        << static_cast<u32_t>(minionMotion.firstOpposedYawEntity)
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

`TryHandleViegoSoulBasicAttack`에서 성공한 영혼 소비가 pending flag와 대상 정보를 설정한 바로 아래에 R runtime 전체 초기화를 추가한다. `ViegoGameSim::ApplyViegoPossession`은 이후 Q/W/E만 빌린 runtime으로 바꾸고, clear도 Q/W/E만 복원하므로 새 R cooldown은 별도 원본 복원에 덮이지 않는다.

기존 코드:

```cpp
        viego.bPossessionActive = false;
        viego.bPossessionPending = true;
        viego.pendingPossessionChampion = soul.champion;
        viego.pendingPossessedTarget = soul.deadChampion;
```

바로 아래에 추가:

```cpp
        if (world.HasComponent<SkillStateComponent>(cmd.issuerEntity))
        {
            auto& skillState =
                world.GetComponent<SkillStateComponent>(cmd.issuerEntity);
            skillState.slots[static_cast<u8_t>(eSkillSlot::R)] =
                SkillSlotRuntime{};
        }
```

### 2-4. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunViegoPossessionProbe`의 기존 Q cooldown 설정 아래에 강탈 전 R cooldown을 추가한다.

```cpp
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::R)].cooldownRemaining = 80.f;
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::R)].cooldownDuration = 80.f;
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::R)].currentStage = 1u;
        viegoSkills.slots[static_cast<u8_t>(eSkillSlot::R)].stageWindow = 3.f;
```

소비 명령이 pending flag를 설정한 직후, consume channel이 끝나기 전부터 R runtime 전체가 초기화됐는지 검증한다.

```cpp
        const SkillSlotRuntime& pendingPossessionR =
            viegoSkills.slots[static_cast<u8_t>(eSkillSlot::R)];
        if (pendingPossessionR.cooldownRemaining != 0.f ||
            pendingPossessionR.cooldownDuration != 0.f ||
            pendingPossessionR.currentStage != 0u ||
            pendingPossessionR.stageWindow != 0.f)
        {
            std::printf("[SimLab][Viego] FAIL: soul consume did not reset R runtime\n");
            return false;
        }
```

### 2-5. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameNetwork.cpp

anonymous namespace의 network interpolation 상수 아래에 visual scale을 추가한다.

```cpp
    constexpr f32_t kRecallVisualScale = 1.f / 3.f;
```

기존 코드:

```cpp
    const f32_t recallDiameter = recallRadius * 2.f;
```

아래로 교체:

```cpp
    const f32_t recallDiameter = recallRadius * 2.f * kRecallVisualScale;
```

### 2-6. C:/Users/user/Desktop/Winters/Data/LoL/FX/recall.wfx

authoring 기본 크기도 1/3로 교체한다.

```json
      "width": 5.3,
      "height": 5.3,
```

## 3. 검증 — 예측을 먼저 쓴다

예측:

- `GameRoomBotMatchSoak` 1,800 tick × 같은 seed 2회가 `minion_lane_slots=6`, `max_minion_lane_stall_ticks<=180`, `minion_opposed_yaw=0`으로 PASS하고 replay/world hash가 run 간 동일하다.
- `SimLab`의 Viego possession probe가 80초 R cooldown을 강탈 순간 0으로 만들고, R 사용 뒤에는 cooldown `>0` 및 form 해제를 동시에 통과한다.
- `recall.wfx`는 JSON으로 파싱되고 width/height가 5.3이며, Client Debug 빌드가 런타임 1/3 override를 포함한다.
- Server/GameSim/Client/SimLab/Harness Debug x64 빌드와 `git diff --check`가 통과한다.
- 리신 FollowWave는 코드 변경 없이 웨이브 전진을 따라가며, authoritative flow `Bot AI GameCommand -> Server movement -> Snapshot`은 유지된다.

검증 명령:

```powershell
git diff -- Server/Private/Game/GameRoomUnitAI.cpp Tools/Harness/GameRoomBotMatchSoak.cpp Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp Tools/SimLab/main.cpp Client/Private/Scene/Scene_InGameNetwork.cpp
git status --short -- Data/LoL/FX/recall.wfx
git diff --check

$wfx = Get-Content -LiteralPath Data/LoL/FX/recall.wfx -Raw | ConvertFrom-Json
if ($wfx.emitters[0].width -ne 5.3 -or $wfx.emitters[0].height -ne 5.3) { throw 'recall size mismatch' }
if (-not (Test-Path -LiteralPath $wfx.emitters[0].texture)) { throw 'recall texture missing' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsroot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild = Join-Path $vsroot 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild Shared/GameSim/Include/GameSim.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Tools/SimLab/SimLab.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false
& $msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false

& Tools/Bin/Debug/SimLab.exe
& Tools/Harness/RunGameRoomBotMatchSoak.ps1 -TickCount 1800 -Seed 42 -Runs 2 -Configuration Debug -HeartbeatTicks 1800 -SkipServerBuild
```

미검증:

- 계획 시점에는 source 수정·빌드·runtime 검증을 아직 실행하지 않았다.
- 실제 화면에서 귀환 FX의 체감 크기와 바텀 벽 구간 3개 연속 웨이브는 빌드/harness 뒤 F5 시각 확인이 필요하다. 자동 검증은 동일 권위 GameRoom의 6개 팀·라인 진행·후진 yaw를 닫는다.

확인 필요:

- 독립 sub-agent critique 결과와 수용/기각 disposition을 아래 gate에 기록한 뒤에만 구현을 시작한다.

## 서브 에이전트 비평

- 상태: 독립 read-only critique 완료, P0 없음.
- P1 검증 경로 오류 — 수용. 실제 `Server/Include`, `Client/Include`, `Tools/Bin/Debug/SimLab.exe`, vswhere 기반 MSBuild를 사용하고 별도 harness vcxproj 가정은 삭제했다.
- P1 raw lane waypoint stall 오탐 — 수용. 유효한 `PathWaypoints[PathIndex]`를 우선 active goal로 추적하고 goal 종류/index/좌표 변경 때 기준을 재설정한다.
- P1 yaw 부동소수점/후처리 오탐 — 수용. 정규화 내적 `< -0.05`만 후진으로 판정한다. 검증이 aggregate tick 변위와 마지막 correction yaw 차이를 드러내면 해당 증거로 구현을 보강하고 PASS를 강제하지 않는다.
- P1 dirty worktree 위험 — 수용. 구현 직전 scoped diff/status를 재확인하고 exact anchor만 패치한다.
- P2 flow 직교 거부 — 수용. lane target 정규화 방향과의 내적이 `-0.05` 미만인 명확한 역방향만 거부한다.
- P2 “모든 부분 이동” 과장 — 수용. 문구를 flow/toward helper 분기로 좁혔다.
- P2 Viego reset 시점 — 부분 기각. form 적용 위치의 상태 보존 논리는 맞지만, 사용자 요구가 `bPossessionPending` flag 입력 즉시이므로 성공한 soul consume 명령에서 초기화하고 SimLab도 channel 전 즉시 상태를 검증한다.
- P2 recall 1/3 범위 — 수용. runtime/WFX를 함께 줄이고 실제 체감은 F5 수동 확인으로 남긴다.
- 첫 integration 결과 — 예측 이탈 수용. 1차 run은 tick 392/entity 70에서 `minion_opposed_yaw=1`로 실패해, 후처리 correction yaw와 tick 전체 순변위가 반대가 될 수 있음을 증명했다. `GameRoom`에 checkpoint 대상이 아닌 한 tick 시작 위치 map을 두고 최종 depenetration 뒤 aggregate 변위로 yaw를 확정한 뒤 같은 검증을 재실행한다.
