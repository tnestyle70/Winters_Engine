Session - Move facing intent를 서버 권위 상태로 보존해 우클릭 연타 yaw 뒤집힘을 제거한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/MoveTargetComponent.h

기존 코드:

```cpp
struct MoveTargetComponent
{
    Vec3 target{};
    Vec3 pathWaypoints[kMovePathMaxWaypoints]{};
    Vec3 facingTarget{};
    f32_t arriveRadius = 0.15f;
    u32_t facingSequenceNum = 0;
    u16_t pathCount = 0;
    u16_t pathIndex = 0;
    bool_t bHasTarget = false;
    bool_t bHasFacingTarget = false;
};
```

아래로 교체:

```cpp
static constexpr u16_t kMoveFacingIntentLockTicks = 6;

struct MoveTargetComponent
{
    Vec3 target{};
    Vec3 pathWaypoints[kMovePathMaxWaypoints]{};
    Vec3 facingTarget{};
    Vec3 facingDirection{};
    f32_t arriveRadius = 0.15f;
    u32_t facingSequenceNum = 0;
    u16_t pathCount = 0;
    u16_t pathIndex = 0;
    u16_t facingLockTicks = 0;
    bool_t bHasTarget = false;
    bool_t bHasFacingTarget = false;
};
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

기존 코드:

```cpp
    void ClearMoveFacingOverride(MoveTargetComponent& moveTarget)
    {
        moveTarget.facingTarget = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.bHasFacingTarget = false;
    }

    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        ClearMoveFacingOverride(moveTarget);
    }

    void SetMoveFacingOverride(
        MoveTargetComponent& moveTarget,
        const Vec3& facingTarget,
        u32_t sequenceNum)
    {
        moveTarget.facingTarget = facingTarget;
        moveTarget.facingSequenceNum = sequenceNum;
        moveTarget.bHasFacingTarget = true;
    }
```

아래로 교체:

```cpp
    void ClearMoveFacingOverride(MoveTargetComponent& moveTarget)
    {
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }

    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        ClearMoveFacingOverride(moveTarget);
    }

    void SetMoveFacingOverride(
        MoveTargetComponent& moveTarget,
        const Vec3& facingTarget,
        const Vec3& facingDirection,
        u32_t sequenceNum)
    {
        moveTarget.facingTarget = facingTarget;
        moveTarget.facingDirection =
            WintersMath::NormalizeXZ(facingDirection, Vec3{}, 0.0001f);
        moveTarget.facingSequenceNum = sequenceNum;
        moveTarget.facingLockTicks = kMoveFacingIntentLockTicks;
        moveTarget.bHasFacingTarget =
            moveTarget.facingDirection.x != 0.f ||
            moveTarget.facingDirection.z != 0.f;
    }
```

기존 코드:

```cpp
    Vec3 ResolveMoveFacingTarget(
        const Vec3& origin,
        const Vec3& rawTarget,
        const Vec3& commandDirection,
        const Vec3& resolvedTarget,
        const MoveTargetComponent& moveTarget)
    {
        const Vec3 clientIntentDirection =
            WintersMath::NormalizeXZ(commandDirection, Vec3{}, 0.0001f);
        if (clientIntentDirection.x != 0.f || clientIntentDirection.z != 0.f)
        {
            return Vec3{
                origin.x + clientIntentDirection.x,
                origin.y,
                origin.z + clientIntentDirection.z
            };
        }

        if (WintersMath::DistanceSqXZ(origin, rawTarget) > 0.0001f)
            return rawTarget;

        if (moveTarget.pathCount > 0)
        {
            const Vec3 waypoint = moveTarget.pathWaypoints[0];
            if (!IsFacingCandidateOpposedToIntent(origin, rawTarget, waypoint))
                return waypoint;
        }

        return resolvedTarget;
    }
```

아래로 교체:

```cpp
    Vec3 ResolveMoveFacingTarget(
        const Vec3& origin,
        const Vec3& rawTarget,
        const Vec3& commandDirection,
        const Vec3& resolvedTarget,
        const MoveTargetComponent& moveTarget)
    {
        const Vec3 clientIntentDirection =
            WintersMath::NormalizeXZ(commandDirection, Vec3{}, 0.0001f);
        if (clientIntentDirection.x != 0.f || clientIntentDirection.z != 0.f)
        {
            return Vec3{
                origin.x + clientIntentDirection.x,
                origin.y,
                origin.z + clientIntentDirection.z
            };
        }

        if (WintersMath::DistanceSqXZ(origin, rawTarget) > 0.0001f)
            return rawTarget;

        if (moveTarget.pathCount > 0)
        {
            const Vec3 waypoint = moveTarget.pathWaypoints[0];
            if (!IsFacingCandidateOpposedToIntent(origin, rawTarget, waypoint))
                return waypoint;
        }

        return resolvedTarget;
    }

    Vec3 ResolveMoveFacingDirection(
        const Vec3& origin,
        const Vec3& facingTarget,
        const Vec3& commandDirection)
    {
        const Vec3 clientIntentDirection =
            WintersMath::NormalizeXZ(commandDirection, Vec3{}, 0.0001f);
        if (clientIntentDirection.x != 0.f || clientIntentDirection.z != 0.f)
            return clientIntentDirection;

        return WintersMath::DirectionXZ(origin, facingTarget, Vec3{});
    }
```

기존 코드:

```cpp
        const Vec3 facingTarget = ResolveMoveFacingTarget(
            pos,
            cmd.groundPos,
            cmd.direction,
            target,
            moveTarget);
        SetMoveFacingOverride(
            moveTarget,
            facingTarget,
            cmd.sequenceNum);
        const Vec3 facingDirection{
            facingTarget.x - pos.x,
            0.f,
            facingTarget.z - pos.z
        };
        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const f32_t yawBefore = transform.GetRotation().y;
        const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
        const f32_t yawOffset = GetDefaultChampionVisualYawOffset(champion);
        const f32_t yawFromFacing =
            ResolveChampionVisualYawNear(champion, facingDirection, yawBefore);
        const Vec3 clientIntentDirection =
            WintersMath::NormalizeXZ(cmd.direction, Vec3{}, 0.0001f);
        const bool_t bHasClientIntentDirection =
            clientIntentDirection.x != 0.f || clientIntentDirection.z != 0.f;
```

아래로 교체:

```cpp
        const Vec3 facingTarget = ResolveMoveFacingTarget(
            pos,
            cmd.groundPos,
            cmd.direction,
            target,
            moveTarget);
        const Vec3 facingDirection =
            ResolveMoveFacingDirection(pos, facingTarget, cmd.direction);
        SetMoveFacingOverride(
            moveTarget,
            facingTarget,
            facingDirection,
            cmd.sequenceNum);
        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const f32_t yawBefore = transform.GetRotation().y;
        const eChampion champion = ResolveChampion(world, cmd.issuerEntity);
        const f32_t yawOffset = GetDefaultChampionVisualYawOffset(champion);
        const f32_t yawFromFacing =
            ResolveChampionVisualYawNear(champion, facingDirection, yawBefore);
        const Vec3 clientIntentDirection =
            WintersMath::NormalizeXZ(cmd.direction, Vec3{}, 0.0001f);
        const bool_t bHasClientIntentDirection =
            clientIntentDirection.x != 0.f || clientIntentDirection.z != 0.f;
```

기존 코드:

```cpp
                "[YawTrace][ServerCommand] tick=%llu seq=%u entity=%u champion=%u yawOffset=%.4f facingSource=%s cmdDir=(%.3f,%.3f) pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) pathCount=%u path0=(%.3f,%.3f,%.3f) path0Opposed=%u facing=(%.3f,%.3f,%.3f) yawBefore=%.4f yawCalc=%.4f yawAfter=%.4f yawDelta=%.4f\n",
```

아래로 교체:

```cpp
                "[YawTrace][ServerCommand] tick=%llu seq=%u entity=%u champion=%u yawOffset=%.4f facingSource=%s cmdDir=(%.3f,%.3f) facingDir=(%.3f,%.3f) lockTicks=%u pos=(%.3f,%.3f,%.3f) raw=(%.3f,%.3f,%.3f) resolved=(%.3f,%.3f,%.3f) pathCount=%u path0=(%.3f,%.3f,%.3f) path0Opposed=%u facing=(%.3f,%.3f,%.3f) yawBefore=%.4f yawCalc=%.4f yawAfter=%.4f yawDelta=%.4f\n",
```

위 문자열 교체 후 같은 `sprintf_s` 인자 목록에서 `clientIntentDirection.z,` 바로 아래에 추가:

```cpp
                facingDirection.x,
                facingDirection.z,
                static_cast<u32_t>(moveTarget.facingLockTicks),
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp

기존 코드:

```cpp
    void ClearMoveFacingOverride(MoveTargetComponent& moveTarget)
    {
        moveTarget.facingTarget = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

아래로 교체:

```cpp
    void ClearMoveFacingOverride(MoveTargetComponent& moveTarget)
    {
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

기존 코드:

```cpp
    Vec3 ResolveMoveYawDirection(
        MoveTargetComponent& moveTarget,
        const Vec3& origin,
        const Vec3& moveDirection,
        bool_t& outUsedFacingIntent,
        bool_t& outMoveYawOpposed)
    {
        outUsedFacingIntent = false;
        outMoveYawOpposed = false;

        if (!moveTarget.bHasFacingTarget)
            return moveDirection;

        const Vec3 facingDirection{
            moveTarget.facingTarget.x - origin.x,
            0.f,
            moveTarget.facingTarget.z - origin.z
        };
        const f32_t facingLenSq =
            facingDirection.x * facingDirection.x +
            facingDirection.z * facingDirection.z;
        if (facingLenSq <= 0.0001f)
        {
            ClearMoveFacingOverride(moveTarget);
            return moveDirection;
        }

        outMoveYawOpposed = IsMoveYawCandidateOpposedToFacingIntent(
            origin,
            moveTarget.facingTarget,
            moveDirection);
        if (!outMoveYawOpposed)
        {
            ClearMoveFacingOverride(moveTarget);
            return moveDirection;
        }

        outUsedFacingIntent = true;
        return facingDirection;
    }
```

아래로 교체:

```cpp
    Vec3 ResolveMoveYawDirection(
        MoveTargetComponent& moveTarget,
        const Vec3& origin,
        const Vec3& moveDirection,
        bool_t& outUsedFacingIntent,
        bool_t& outMoveYawOpposed,
        bool_t& outFacingLocked)
    {
        outUsedFacingIntent = false;
        outMoveYawOpposed = false;
        outFacingLocked = false;

        if (!moveTarget.bHasFacingTarget)
            return moveDirection;

        Vec3 facingDirection =
            WintersMath::NormalizeXZ(moveTarget.facingDirection, Vec3{}, 0.0001f);
        if (facingDirection.x == 0.f && facingDirection.z == 0.f)
        {
            facingDirection = WintersMath::DirectionXZ(
                origin,
                moveTarget.facingTarget,
                Vec3{});
        }
        if (facingDirection.x == 0.f && facingDirection.z == 0.f)
        {
            ClearMoveFacingOverride(moveTarget);
            return moveDirection;
        }

        outMoveYawOpposed = IsMoveYawCandidateOpposedToFacingIntent(
            origin,
            moveTarget.facingTarget,
            moveDirection);

        outFacingLocked = moveTarget.facingLockTicks > 0;
        if (outFacingLocked)
        {
            --moveTarget.facingLockTicks;
            outUsedFacingIntent = true;
            return facingDirection;
        }

        if (!outMoveYawOpposed)
        {
            ClearMoveFacingOverride(moveTarget);
            return moveDirection;
        }

        outUsedFacingIntent = true;
        return facingDirection;
    }
```

기존 코드:

```cpp
        const bool_t bHadFacingIntent = moveTarget.bHasFacingTarget;
        const Vec3 traceFacingTarget = moveTarget.facingTarget;
        const u32_t traceFacingSequenceNum = moveTarget.facingSequenceNum;
        bool_t bUsedFacingIntent = false;
        bool_t bMoveYawOpposed = false;
        const Vec3 yawDirection = ResolveMoveYawDirection(
            moveTarget,
            pos,
            dir,
            bUsedFacingIntent,
            bMoveYawOpposed);
```

아래로 교체:

```cpp
        const bool_t bHadFacingIntent = moveTarget.bHasFacingTarget;
        const Vec3 traceFacingTarget = moveTarget.facingTarget;
        const Vec3 traceFacingDirection = moveTarget.facingDirection;
        const u32_t traceFacingSequenceNum = moveTarget.facingSequenceNum;
        const u16_t traceFacingLockTicks = moveTarget.facingLockTicks;
        bool_t bUsedFacingIntent = false;
        bool_t bMoveYawOpposed = false;
        bool_t bFacingLocked = false;
        const Vec3 yawDirection = ResolveMoveYawDirection(
            moveTarget,
            pos,
            dir,
            bUsedFacingIntent,
            bMoveYawOpposed,
            bFacingLocked);
```

기존 코드:

```cpp
                "[YawTrace][MoveSystem] tick=%llu entity=%u champion=%u source=%s hadFacing=%u pathYawOpposed=%u pos=(%.3f,%.3f,%.3f) active=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) facing=(%.3f,%.3f,%.3f) pathIndex=%u pathCount=%u seq=%u prevYaw=%.4f appliedYaw=%.4f yawDelta=%.4f\n",
```

아래로 교체:

```cpp
                "[YawTrace][MoveSystem] tick=%llu entity=%u champion=%u source=%s hadFacing=%u pathYawOpposed=%u lockTicks=%u facingDir=(%.3f,%.3f) pos=(%.3f,%.3f,%.3f) active=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) facing=(%.3f,%.3f,%.3f) pathIndex=%u pathCount=%u seq=%u prevYaw=%.4f appliedYaw=%.4f yawDelta=%.4f\n",
```

위 문자열 교체 후 같은 `sprintf_s` 인자 목록에서 `bMoveYawOpposed ? 1u : 0u,` 바로 아래에 추가:

```cpp
                static_cast<u32_t>(traceFacingLockTicks),
                traceFacingDirection.x,
                traceFacingDirection.z,
```

기존 코드:

```cpp
                bUsedFacingIntent ? "intent" : "move",
```

아래로 교체:

```cpp
                bUsedFacingIntent
                    ? (bFacingLocked ? "intent-lock" : "intent")
                    : "move",
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChaseSystem.cpp

기존 코드:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

아래로 교체:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CombatActionSystem.cpp

기존 코드:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

아래로 교체:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

기존 코드:

```cpp
    bool_t TryAssignQueuedMoveTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID entity,
        const Vec3& requestedTarget)
```

아래로 교체:

```cpp
    bool_t TryAssignQueuedMoveTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID entity,
        const Vec3& requestedTarget,
        const Vec3& requestedDirection)
```

기존 코드:

```cpp
        moveTarget.target = resolvedTarget;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = requestedTarget;
        moveTarget.facingSequenceNum = 0;
        moveTarget.bHasFacingTarget = true;
        moveTarget.bHasTarget = true;
        return true;
```

아래로 교체:

```cpp
        moveTarget.target = resolvedTarget;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = requestedTarget;
        moveTarget.facingDirection = WintersMath::NormalizeXZ(
            requestedDirection,
            WintersMath::DirectionXZ(pos, requestedTarget, Vec3{}),
            0.0001f);
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = kMoveFacingIntentLockTicks;
        moveTarget.bHasFacingTarget =
            moveTarget.facingDirection.x != 0.f ||
            moveTarget.facingDirection.z != 0.f;
        moveTarget.bHasTarget = true;
        return true;
```

기존 코드:

```cpp
            TryAssignQueuedMoveTarget(world, tc, entity, action.vQueuedMoveTarget);
```

아래로 교체:

```cpp
            TryAssignQueuedMoveTarget(
                world,
                tc,
                entity,
                action.vQueuedMoveTarget,
                action.vQueuedMoveDirection);
```

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/WaypointPatrolSystem.cpp

기존 코드:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

아래로 교체:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        moveTarget.facingTarget = {};
        moveTarget.facingDirection = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.facingLockTicks = 0;
        moveTarget.bHasFacingTarget = false;
    }
```

2. 검증

미검증:
- 이 계획서는 코드 반영 전 계획서다.
- 런타임 재현은 아직 미검증이다.

검증 명령:
- `git diff --check`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64`
- `& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64`

수동 확인:
- 봇 없이 이렐리아만 둔다.
- 우클릭 연타 재현 시 `[YawTrace][ServerCommand] facingSource=client-dir`가 나온다.
- 같은 seq 이후 `[YawTrace][MoveSystem] source=intent-lock hadFacing=1 lockTicks>0`가 최소 몇 tick 유지된다.
- intent-lock 구간에서 `[YawTrace][MoveSystem] yawDelta`가 avoidance 때문에 1.2, 2.8, 3.1 근처로 튀지 않아야 한다.
- lock이 끝난 뒤에는 `[YawTrace][MoveSystem] source=move`로 자연스럽게 넘어가야 한다.
- `[YawTrace][SnapshotApply] halfTurn=1 source=server`가 계속 뜨면 snapshot 보호 계획으로 넘어간다.
- SnapshotApply도 정상인데 화면만 반대로 보이면 `SyncFromECS`/model yaw offset 쪽을 별도 계획으로 분리한다.

보안/권위 판단:
- 회전을 순수 Client 권위로 넘기지 않는다.
- Client가 보낸 것은 gameplay truth가 아니라 Move command의 input intent다.
- 서버가 finite normalized direction만 승인해 `MoveTargetComponent`에 짧은 lock tick으로 저장한다.
- position/path/collision/attack/skill 판정은 계속 서버 권위다.
- 공격/스킬 direction은 기존 검증 경로를 유지하고, Move intent-facing은 이동 presentation yaw에만 사용한다.
