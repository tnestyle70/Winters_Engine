Session - BasicAttack chase가 Move facing intent lock을 우회하지 않게 한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

`namespace` 내부에서 `SetMoveFacingOverride` 함수 바로 아래에 추가:

기존 코드:

```cpp
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

아래에 추가:

```cpp
    Vec3 ResolveAttackChaseFacingDirection(
        const Vec3& selfPos,
        const Vec3& targetPos,
        const Vec3& commandDirection)
    {
        const Vec3 targetDirection =
            WintersMath::DirectionXZ(selfPos, targetPos, Vec3{});
        const Vec3 clientDirection =
            WintersMath::NormalizeXZ(commandDirection, Vec3{}, 0.0001f);

        if (clientDirection.x != 0.f || clientDirection.z != 0.f)
        {
            if (targetDirection.x == 0.f && targetDirection.z == 0.f)
                return clientDirection;

            const f32_t dot =
                clientDirection.x * targetDirection.x +
                clientDirection.z * targetDirection.z;
            if (dot > -0.10f)
                return clientDirection;
        }

        return targetDirection;
    }
```

`StartAttackChase` 안에서 아래 블록을 교체:

기존 코드:

```cpp
            if (world.HasComponent<TransformComponent>(cmd.issuerEntity))
            {
                const Vec3 selfPos =
                    world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition();
                if (!TryAssignGridMovePath(tc, selfPos, goal, moveTarget))
                {
                    moveTarget.bHasTarget = false;
                    return;
                }
            }
            else
            {
                ClearMovePath(moveTarget);
            }
```

아래로 교체:

```cpp
            if (world.HasComponent<TransformComponent>(cmd.issuerEntity))
            {
                const Vec3 selfPos =
                    world.GetComponent<TransformComponent>(cmd.issuerEntity).GetLocalPosition();
                if (!TryAssignGridMovePath(tc, selfPos, goal, moveTarget))
                {
                    moveTarget.bHasTarget = false;
                    return;
                }

                const Vec3 facingDirection =
                    ResolveAttackChaseFacingDirection(selfPos, goal, cmd.direction);
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
            }
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/AttackChaseSystem.cpp

`namespace` 내부의 `ClearMovePath` 함수를 교체:

기존 코드:

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

아래로 교체:

```cpp
    void ClearMovePath(MoveTargetComponent& moveTarget)
    {
        moveTarget.pathCount = 0;
        moveTarget.pathIndex = 0;
    }
```

2. 검증

검증 명령:

```text
git diff --check
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64
```

런타임 확인:

```text
1. 이렐리아로 적 챔피언/미니언 근처에서 우클릭 이동과 우클릭 공격을 빠르게 섞는다.
2. [Command] basic-attack chase start issuer=1 target=... seq=N 직후 같은 issuer의 [YawTrace][MoveSystem]이 source=intent-lock hadFacing=1 seq=N으로 떠야 한다.
3. 실패 로그였던 source=move hadFacing=0 seq=0 yawDelta=1~3대가 basic-attack chase start 직후에는 더 이상 나오면 안 된다.
4. AttackChaseSystem repath가 여러 번 돌아도 lockTicks가 6으로 계속 리셋되지 않고 6,5,4...로 자연 감소해야 한다.
5. lockTicks=0 이후 source=move로 바뀌는 것은 정상이며, 같은 active target 기준 yawDelta가 3.1 근처로 튀면 아직 다른 덮어쓰기 경로가 남은 것이다.
```

미검증:

```text
- 실제 이렐리아 우클릭 공격 chase 재현에서 화면 뒤집힘이 사라지는지는 런타임 재현으로 확인 필요.
```
