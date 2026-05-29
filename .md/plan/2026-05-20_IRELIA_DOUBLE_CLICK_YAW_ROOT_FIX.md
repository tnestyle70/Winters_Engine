Session - MoveSystem이 첫 path waypoint/avoidance 방향으로 raw click yaw를 덮는 문제를 서버 권위 이동 안에서 완전히 차단한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/MoveTargetComponent.h

기존 코드:

```cpp
struct MoveTargetComponent
{
    Vec3 target{};
    Vec3 pathWaypoints[kMovePathMaxWaypoints]{};
    f32_t arriveRadius = 0.15f;
    u16_t pathCount = 0;
    u16_t pathIndex = 0;
    bool_t bHasTarget = false;
};
```

아래로 교체:

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

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

기존 코드:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
    }
```

아래로 교체:

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

`CDefaultCommandExecutor::HandleMove` 안에서 아래 기존 코드 블록을 교체:

기존 코드:

```cpp
        if (WintersMath::DistanceSqXZ(pos, target) <= moveTarget.arriveRadius * moveTarget.arriveRadius)
        {
            moveTarget.bHasTarget = false;
            return;
        }
```

아래로 교체:

```cpp
        if (WintersMath::DistanceSqXZ(pos, target) <= moveTarget.arriveRadius * moveTarget.arriveRadius)
        {
            moveTarget.bHasTarget = false;
            ClearMovePath(moveTarget);
            return;
        }
```

`CDefaultCommandExecutor::HandleMove` 안에서 아래 기존 코드 바로 아래에 추가:

기존 코드:

```cpp
        const Vec3 facingTarget = ResolveMoveFacingTarget(
            pos,
            cmd.groundPos,
            target,
            moveTarget);
```

아래에 추가:

```cpp
        SetMoveFacingOverride(
            moveTarget,
            facingTarget,
            cmd.sequenceNum);
```

1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp

기존 코드:

```cpp
    constexpr f32_t kAvoidancePadding = 0.05f;
    constexpr f32_t kPathWaypointArriveRadius = 0.12f;
    constexpr u8_t kUnknownSpatialTeam = 0xffu;
```

아래로 교체:

```cpp
    constexpr f32_t kAvoidancePadding = 0.05f;
    constexpr f32_t kPathWaypointArriveRadius = 0.12f;
    constexpr u8_t kUnknownSpatialTeam = 0xffu;

    void ClearMoveFacingOverride(MoveTargetComponent& moveTarget)
    {
        moveTarget.facingTarget = {};
        moveTarget.facingSequenceNum = 0;
        moveTarget.bHasFacingTarget = false;
    }

    void ClearMoveRuntimeTarget(MoveTargetComponent& moveTarget)
    {
        moveTarget.bHasTarget = false;
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
        ClearMoveFacingOverride(moveTarget);
    }

    bool_t IsMoveYawCandidateOpposedToFacingIntent(
        const Vec3& origin,
        const Vec3& facingTarget,
        const Vec3& candidateDirection)
    {
        const Vec3 intent{
            facingTarget.x - origin.x,
            0.f,
            facingTarget.z - origin.z
        };
        const f32_t intentLenSq = intent.x * intent.x + intent.z * intent.z;
        const f32_t candidateLenSq =
            candidateDirection.x * candidateDirection.x +
            candidateDirection.z * candidateDirection.z;
        if (intentLenSq <= 0.0001f || candidateLenSq <= 0.0001f)
            return false;

        const f32_t dot =
            intent.x * candidateDirection.x +
            intent.z * candidateDirection.z;
        const f32_t minDot =
            -0.10f * std::sqrt(intentLenSq * candidateLenSq);
        return dot < minDot;
    }

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

기존 코드:

```cpp
        if (!moveTarget.bHasTarget)
            continue;
```

아래로 교체:

```cpp
        if (!moveTarget.bHasTarget)
        {
            ClearMoveFacingOverride(moveTarget);
            continue;
        }
```

기존 코드:

```cpp
                moveTarget.bHasTarget = false;
                continue;
```

아래로 교체:

```cpp
                ClearMoveRuntimeTarget(moveTarget);
                continue;
```

적용 주의:
위 교체는 `KalistaPassiveDashComponent` 분기와 `health.bIsDead` 분기의 동일한 두 줄 모두에 적용한다.

기존 코드:

```cpp
            moveTarget.bHasTarget = false;
            moveTarget.pathCount = 0;
            moveTarget.pathIndex = 0;
```

아래로 교체:

```cpp
            ClearMoveRuntimeTarget(moveTarget);
```

적용 주의:
위 교체는 `!GameplayStateQuery::CanMove`, `!bCurrentWalkable && moveTarget.pathCount == 0`, `moveTarget.pathIndex >= moveTarget.pathCount`, 최종 도착 처리, `TryClampMoveSegmentXZ` 실패, `bSegmentClamped` 처리의 동일한 3줄 블록에 모두 적용한다.

기존 코드:

```cpp
        transform.SetPosition(resolvedNext);

        const Vec3 rot = transform.GetRotation();
        transform.SetRotation({
            rot.x,
            ResolveChampionVisualYawNear(stat.championId, dir, rot.y),
            rot.z });
```

아래로 교체:

```cpp
        transform.SetPosition(resolvedNext);

        bool_t bUsedFacingIntent = false;
        bool_t bMoveYawOpposed = false;
        const Vec3 yawDirection = ResolveMoveYawDirection(
            moveTarget,
            pos,
            dir,
            bUsedFacingIntent,
            bMoveYawOpposed);

        const Vec3 rot = transform.GetRotation();
        const f32_t resolvedYaw =
            ResolveChampionVisualYawNear(stat.championId, yawDirection, rot.y);
        transform.SetRotation({
            rot.x,
            resolvedYaw,
            rot.z });

        static u32_t s_moveSystemYawTraceCount = 0;
        const f32_t yawDelta = NormalizeChampionVisualYaw(resolvedYaw - rot.y);
        if ((bUsedFacingIntent || bMoveYawOpposed || std::fabs(yawDelta) > 1.0f) &&
            s_moveSystemYawTraceCount < 512u)
        {
            char msg[768]{};
            sprintf_s(
                msg,
                "[YawTrace][MoveSystem] tick=%llu entity=%u champion=%u source=%s pathYawOpposed=%u pos=(%.3f,%.3f,%.3f) active=(%.3f,%.3f,%.3f) target=(%.3f,%.3f,%.3f) facing=(%.3f,%.3f,%.3f) pathIndex=%u pathCount=%u seq=%u prevYaw=%.4f appliedYaw=%.4f yawDelta=%.4f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                static_cast<u32_t>(entity),
                static_cast<u32_t>(stat.championId),
                bUsedFacingIntent ? "intent" : "move",
                bMoveYawOpposed ? 1u : 0u,
                pos.x,
                pos.y,
                pos.z,
                activeTarget.x,
                activeTarget.y,
                activeTarget.z,
                moveTarget.target.x,
                moveTarget.target.y,
                moveTarget.target.z,
                moveTarget.facingTarget.x,
                moveTarget.facingTarget.y,
                moveTarget.facingTarget.z,
                static_cast<u32_t>(moveTarget.pathIndex),
                static_cast<u32_t>(moveTarget.pathCount),
                moveTarget.facingSequenceNum,
                rot.y,
                resolvedYaw,
                yawDelta);
            WintersOutputAIDebugStringA(msg);
            ++s_moveSystemYawTraceCount;
        }
```

1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChaseSystem.cpp

기존 코드:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
    }
```

아래로 교체:

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

1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CombatActionSystem.cpp

기존 코드:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
    }
```

아래로 교체:

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

1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/WaypointPatrolSystem.cpp

기존 코드:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
    }
```

아래로 교체:

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

2. 검증

미검증:
- 빌드 미검증
- 이렐리아 우클릭 더블 클릭 런타임 미검증
- path 첫 waypoint가 raw 클릭 의도와 반대인 지형/장애물 케이스 미검증

검증 명령:
- `git diff --check`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Server`
- `msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /t:Client`

수동 확인:
- 이렐리아로 완전히 이동 가능한 직선 지점에도 빠른 우클릭 더블 클릭을 반복한다.
- direct walkable 케이스에서는 `[YawTrace][ServerCommand] path0Opposed=0` 또는 direct path가 나오고, `[YawTrace][MoveSystem] source=move`가 3.0 근처 `yawDelta` 없이 지나가야 한다.
- 이렐리아로 벽 모서리, 미니언 근처, walkable cell 경계에서 우클릭 더블 클릭을 반복한다.
- 재현 지점에서 `[YawTrace][ServerCommand] path0Opposed=1`이 뜨는지 확인한다.
- 같은 `seq` 이후 `[YawTrace][MoveSystem] pathYawOpposed=1 source=intent`가 나와야 한다.
- 같은 구간에서 `[YawTrace][MoveSystem] source=move yawDelta`가 3.0 근처로 튀면 아직 path yaw가 raw intent를 덮는 것이다.
- 이 패치 후에도 화면이 뒤집히면 다음 범인은 `SnapshotApplier`다. 그때만 `[YawTrace][SnapshotApply] source=server yawDelta≈3.1`와 `protectedSeq/ack` 관계를 보고 snapshot 보호 계획을 별도 적용한다.
- `yawDelta`가 6.2 근처면 이 계획의 문제가 아니라 PI/-PI 표현 래핑 문제다. `yawDelta`가 3.1 근처면 실제 180도 반대 yaw가 아직 들어온 것이다.
