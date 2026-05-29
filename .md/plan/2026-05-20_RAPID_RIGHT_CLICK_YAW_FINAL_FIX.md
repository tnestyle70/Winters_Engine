Session - Rapid right-click move yaw final fix

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

`RotateEntityTowardDirection` 함수 안에서 아래 기존 코드를 아래로 교체:

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

아래로 교체:

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
        const f32_t resolvedYaw =
            ResolveChampionVisualYawNear(champion, direction, rot.y);
        transform.SetRotation({
            rot.x,
            resolvedYaw,
            rot.z
            });
    }
```

`CDefaultCommandExecutor::HandleMove` 안에서 아래 기존 코드를 아래로 교체:

기존 코드:

```cpp
        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const f32_t yawBefore = transform.GetRotation().y;
        const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
        const f32_t yawFromFacing =
            ResolveChampionVisualYawFromDirection(champion, facingDirection);
        const Vec3 firstWaypoint = moveTarget.pathCount > 0
            ? moveTarget.pathWaypoints[0]
            : Vec3{};
        const bool_t bFirstWaypointOpposed =
            moveTarget.pathCount > 0 &&
            IsFacingCandidateOpposedToIntent(pos, cmd.groundPos, firstWaypoint);
        RotateEntityTowardDirection(
            world,
            cmd.issuerEntity,
            facingDirection);

        static u32_t s_moveYawTraceCount = 0;
        if (s_moveYawTraceCount < 512u)
        {
            const f32_t yawAfter = transform.GetRotation().y;
            char msg[768]{};
            sprintf_s(
                msg,
                "[YawTrace][ServerCommand] tick=%llu seq=%u entity=%u champion=%u pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) pathCount=%u path0=(%.3f,%.3f,%.3f) path0Opposed=%u facing=(%.3f,%.3f,%.3f) yawBefore=%.4f yawCalc=%.4f yawAfter=%.4f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                cmd.sequenceNum,
                static_cast<u32_t>(cmd.issuerEntity),
                static_cast<u32_t>(champion),
                pos.x,
                pos.y,
                pos.z,
                cmd.groundPos.x,
                cmd.groundPos.y,
                cmd.groundPos.z,
                target.x,
                target.y,
                target.z,
                static_cast<u32_t>(moveTarget.pathCount),
                firstWaypoint.x,
                firstWaypoint.y,
                firstWaypoint.z,
                bFirstWaypointOpposed ? 1u : 0u,
                facingTarget.x,
                facingTarget.y,
                facingTarget.z,
                yawBefore,
                yawFromFacing,
                yawAfter);
            OutputCommandDebug(msg);
            ++s_moveYawTraceCount;
        }
```

아래로 교체:

```cpp
        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const f32_t yawBefore = transform.GetRotation().y;
        const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
        const f32_t yawFromFacing =
            ResolveChampionVisualYawNear(champion, facingDirection, yawBefore);
        const Vec3 firstWaypoint = moveTarget.pathCount > 0
            ? moveTarget.pathWaypoints[0]
            : Vec3{};
        const bool_t bFirstWaypointOpposed =
            moveTarget.pathCount > 0 &&
            IsFacingCandidateOpposedToIntent(pos, cmd.groundPos, firstWaypoint);
        RotateEntityTowardDirection(
            world,
            cmd.issuerEntity,
            facingDirection);

        static u32_t s_moveYawTraceCount = 0;
        if (s_moveYawTraceCount < 512u)
        {
            const f32_t yawAfter = transform.GetRotation().y;
            const f32_t yawDelta = NormalizeChampionVisualYaw(yawAfter - yawBefore);
            char msg[832]{};
            sprintf_s(
                msg,
                "[YawTrace][ServerCommand] tick=%llu seq=%u entity=%u champion=%u pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) pathCount=%u path0=(%.3f,%.3f,%.3f) path0Opposed=%u facing=(%.3f,%.3f,%.3f) yawBefore=%.4f yawCalc=%.4f yawAfter=%.4f yawDelta=%.4f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                cmd.sequenceNum,
                static_cast<u32_t>(cmd.issuerEntity),
                static_cast<u32_t>(champion),
                pos.x,
                pos.y,
                pos.z,
                cmd.groundPos.x,
                cmd.groundPos.y,
                cmd.groundPos.z,
                target.x,
                target.y,
                target.z,
                static_cast<u32_t>(moveTarget.pathCount),
                firstWaypoint.x,
                firstWaypoint.y,
                firstWaypoint.z,
                bFirstWaypointOpposed ? 1u : 0u,
                facingTarget.x,
                facingTarget.y,
                facingTarget.z,
                yawBefore,
                yawFromFacing,
                yawAfter,
                yawDelta);
            OutputCommandDebug(msg);
            ++s_moveYawTraceCount;
        }
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp

`MoveSystem` 이동 tick에서 실제 이동 방향으로 회전을 갱신하는 부분의 아래 기존 코드를 아래로 교체:

기존 코드:

```cpp
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation({
            rot.x,
            ResolveChampionVisualYawFromDirection(stat.championId, dir),
            rot.z });
```

아래로 교체:

```cpp
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation({
            rot.x,
            ResolveChampionVisualYawNear(stat.championId, dir, rot.y),
            rot.z });
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

`CSnapshotApplier::OnSnapshot` 로컬 챔피언 yaw trace 안에서 아래 기존 코드를 아래로 교체:

기존 코드:

```cpp
                    sprintf_s(
                        msg,
                        "[YawTrace][SnapshotApply] tick=%llu ack=%u net=%u entity=%u source=%s prevYaw=%.4f serverYaw=%.4f sourceYaw=%.4f appliedYaw=%.4f protectedYaw=%.4f protectedSeq=%u protectedAck=%u actionLocked=%u state=0x%08X pos=(%.3f,%.3f,%.3f)\n",
                        static_cast<unsigned long long>(m_lastServerTick),
                        lastAckedCommandSeq,
                        es->netId(),
                        static_cast<u32_t>(e),
                        bAwaitingLocalMoveAck ? "protected" : "server",
                        rot.y,
                        es->yaw(),
                        sourceYaw,
                        resolvedYaw,
                        m_localMoveYawProtection.yaw,
                        m_localMoveYawProtection.commandSeq,
                        m_localMoveYawProtection.bAckedByServer ? 1u : 0u,
                        bServerActionLocked ? 1u : 0u,
                        es->stateFlags(),
                        snapshotPos.x,
                        snapshotPos.y,
                        snapshotPos.z);
```

아래로 교체:

```cpp
                    sprintf_s(
                        msg,
                        "[YawTrace][SnapshotApply] tick=%llu ack=%u net=%u entity=%u source=%s prevYaw=%.4f serverYaw=%.4f sourceYaw=%.4f appliedYaw=%.4f yawDelta=%.4f protectedYaw=%.4f protectedSeq=%u protectedAck=%u actionLocked=%u state=0x%08X pos=(%.3f,%.3f,%.3f)\n",
                        static_cast<unsigned long long>(m_lastServerTick),
                        lastAckedCommandSeq,
                        es->netId(),
                        static_cast<u32_t>(e),
                        bAwaitingLocalMoveAck ? "protected" : "server",
                        rot.y,
                        es->yaw(),
                        sourceYaw,
                        resolvedYaw,
                        yawDelta,
                        m_localMoveYawProtection.yaw,
                        m_localMoveYawProtection.commandSeq,
                        m_localMoveYawProtection.bAckedByServer ? 1u : 0u,
                        bServerActionLocked ? 1u : 0u,
                        es->stateFlags(),
                        snapshotPos.x,
                        snapshotPos.y,
                        snapshotPos.z);
```

1-4. C:/Users/user/Desktop/Winters/.claude/gotchas.md

아래 기존 코드 바로 아래에 추가:

기존 코드:

```text
- 2026-05-20 - [Movement actors] champion and minion facing conventions are not automatically interchangeable -> before a global yaw change, compare champion MoveSystem/CommandExecutor, client prediction, SnapshotApplier, and minion `FaceMoveDirection`; legacy minions can intentionally face with a negated movement direction.
```

아래에 추가:

```text
- 2026-05-20 - [Server yaw storage] normalizing body yaw at every server move tick can re-cross the +PI/-PI boundary during rapid right-clicks -> store champion body yaw with `ResolveChampionVisualYawNear` against the current yaw in CommandExecutor and MoveSystem, then normalize only for wire/log delta comparisons.
```

2. 검증

검증 명령:

```text
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:MultiProcessorCompilation=false /maxcpucount:1 /v:minimal
```

수동 확인:

```text
1. Irelia로 인게임 진입.
2. 넥서스 아래쪽 피킹 드랍 재현 위치에서 우클릭 이동.
3. 캐릭터가 더 이상 언더맵으로 떨어지지 않는지 확인.
4. 같은 위치에서 위/아래 방향으로 우클릭을 빠르게 따닥 반복.
5. 좌/우 방향도 같은 방식으로 반복.
```

로그 기대값:

```text
- [YawTrace][ServerCommand]에서 `mode=direct`, `pathCount=1`, `path0Opposed=0`인 로컬 플레이어 이동은 waypoint/navgrid 문제가 아니다.
- 빠른 우클릭 중 `yawAfter`는 `yawBefore` 근처의 unwrapped 표현으로 저장되어야 한다.
  예: 기존처럼 `2.4261 -> -2.2970`로 저장하지 않고 `2.4261 -> 3.9862`처럼 현재 yaw 기준 가까운 표현을 쓴다.
- `yawDelta`는 항상 [-3.1416, 3.1416] 범위 안이어야 한다.
- [YawTrace][SnapshotApply]의 `yawDelta`도 같은 범위여야 한다.
- `source=protected`가 최신 move command ack 전까지 유지되어야 한다.
- `[MoveTarget] reject-surface-y` 또는 `[ServerNav] reject-surface-y`가 찍혀도 캐릭터 위치 Y가 음수 언더맵으로 확정되면 안 된다.
```

미검증:

```text
- 런타임 체감 검증은 사용자가 직접 빠른 우클릭 재현으로 확인해야 한다.
- 그래도 뒤로 걷는 느낌이 남으면 다음 분기는 yaw 수학이 아니라 Irelia/Sylas body mesh forward offset 검증이다. 이 경우 `GetDefaultChampionVisualYawOffset`만 대상으로 별도 축 검증을 진행한다.
```
