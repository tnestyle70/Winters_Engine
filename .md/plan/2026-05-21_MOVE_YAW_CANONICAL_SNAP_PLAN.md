Session - Move yaw near-angle correction을 제거하고 입력 방향 yaw를 즉시 canonical snap으로 반영한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/MoveSystem.cpp

`CMoveSystem::Execute`에서 yaw를 적용하는 구간의 기존 `ResolveChampionVisualYawNear` 사용을 제거한다.

기존 코드:

```cpp
        const Vec3 rot = transform.GetRotation();
        const f32_t targetYaw =
            ResolveChampionVisualYawNear(stat.championId, yawDirection, rot.y);
        const f32_t resolvedYaw = targetYaw;
        transform.SetRotation({
            rot.x,
            resolvedYaw,
            rot.z });
```

아래로 교체:

```cpp
        const Vec3 rot = transform.GetRotation();
        const f32_t resolvedYaw =
            ResolveChampionVisualYawFromDirection(stat.championId, yawDirection);
        transform.SetRotation({
            rot.x,
            resolvedYaw,
            rot.z });
```

의도:

```text
- MoveSystem은 더 이상 이전 yaw 근처의 2π 동치각을 고르지 않는다.
- appliedYaw는 항상 [-PI, PI] canonical yaw가 된다.
- yawDelta가 큰 것은 입력 방향이 실제로 크게 바뀐 결과로만 남긴다.
```

1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor.cpp

`HandleMove`의 yaw trace 계산도 같은 기준으로 맞춘다. 서버 명령 단계는 실제 회전을 하지 않지만, 로그의 `yawCalc`가 `MoveSystem`이 적용할 canonical yaw와 같아야 한다.

기존 코드:

```cpp
        const f32_t yawFromFacing =
            ResolveChampionVisualYawNear(champion, facingDirection, yawBefore);
```

아래로 교체:

```cpp
        const f32_t yawFromFacing =
            ResolveChampionVisualYawFromDirection(champion, facingDirection);
```

의도:

```text
- [YawTrace][ServerCommand] yawCalc가 더 이상 -8, 14 같은 unwrapped yaw로 나오지 않는다.
- ServerCommand yawAfter는 그대로 yawBefore여야 한다.
- 실제 yaw 적용은 MoveSystem에서 같은 tick에 canonical snap으로만 한다.
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerTransformBridge.cpp

클라 플레이어 transform에 yaw를 직접 박을 때도 `MakeChampionVisualYawNear`를 제거한다.

기존 코드:

```cpp
    Vec3 rot = scene.m_pPlayerTransform->GetRotation();
    const f32_t previousYaw = rot.y;
    const f32_t resolvedYaw = MakeChampionVisualYawNear(yaw, previousYaw);
    const f32_t rawDelta = yaw - previousYaw;
    const f32_t appliedDelta = NormalizeChampionVisualYaw(resolvedYaw - previousYaw);
    scene.m_pPlayerTransform->SetRotation({ rot.x, resolvedYaw, rot.z });
```

아래로 교체:

```cpp
    Vec3 rot = scene.m_pPlayerTransform->GetRotation();
    const f32_t previousYaw = rot.y;
    const f32_t resolvedYaw = NormalizeChampionVisualYaw(yaw);
    const f32_t rawDelta = yaw - previousYaw;
    const f32_t appliedDelta = NormalizeChampionVisualYaw(resolvedYaw - previousYaw);
    scene.m_pPlayerTransform->SetRotation({ rot.x, resolvedYaw, rot.z });
```

의도:

```text
- 클라 예측 yaw도 이전 yaw 근처 동치각으로 보정하지 않는다.
- SetPlayerYaw가 받은 yaw를 canonical yaw로 즉시 snap한다.
- 렌더 transform/cache에 -3.75, 8.91, 14.01 같은 누적 yaw가 남지 않는다.
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGamePlayerControlBridge.cpp

`PredictLocalMoveYaw`에서 이전 yaw 기준 보정을 제거한다.

기존 코드:

```cpp
        const f32_t yaw =
            ResolveChampionVisualYawNear(
                scene.GetPlayerChampionId(),
                direction,
                playerTransform->GetRotation().y);
        outYaw = yaw;

        CInGamePlayerTransformBridge::SetPlayerYaw(scene, yaw);
        return true;
```

아래로 교체:

```cpp
        const f32_t yaw =
            ResolveChampionVisualYawFromDirection(
                scene.GetPlayerChampionId(),
                direction);
        outYaw = yaw;

        CInGamePlayerTransformBridge::SetPlayerYaw(scene, yaw);
        return true;
```

의도:

```text
- 우클릭 예측 단계도 canonical yaw만 사용한다.
- 서버 MoveSystem과 클라 예측 yaw가 같은 표현값을 사용한다.
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameCombatInputBridge.cpp

공격 입력 yaw 보호도 이전 yaw 근처 보정을 제거한다.

기존 코드:

```cpp
        const f32_t predictedYaw = ResolveChampionVisualYawNear(
            scene.GetPlayerChampionId(),
            facingDirection,
            pPlayerTransform->GetRotation().y);
        CInGamePlayerTransformBridge::SetPlayerYaw(scene, predictedYaw);
        pSnapshotApplier->ProtectLocalMoveYaw(
            pNetworkView->GetMyNetEntityId(),
            commandSeq,
            predictedYaw);
```

아래로 교체:

```cpp
        const f32_t predictedYaw = ResolveChampionVisualYawFromDirection(
            scene.GetPlayerChampionId(),
            facingDirection);
        CInGamePlayerTransformBridge::SetPlayerYaw(scene, predictedYaw);
        pSnapshotApplier->ProtectLocalMoveYaw(
            pNetworkView->GetMyNetEntityId(),
            commandSeq,
            predictedYaw);
```

의도:

```text
- basic-attack 입력 보호 yaw도 canonical 값으로 저장한다.
- attack chase facing lock과 snapshot protect가 서로 다른 yaw 표현값을 들고 싸우지 않는다.
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/SnapshotApplier.cpp

스냅샷 적용에서 `MakeChampionVisualYawNear`를 제거하고, 서버 yaw/protected yaw/source yaw 모두 canonical 값으로 비교하고 적용한다.

기존 코드:

```cpp
            const f32_t serverYawNear = MakeChampionVisualYawNear(es->yaw(), rot.y);
            const f32_t protectedYawNear =
                MakeChampionVisualYawNear(m_localMoveYawProtection.yaw, rot.y);
            const bool_t bSnapshotCoversProtectedCommand =
                IsCommandSeqAtLeast(
                    lastAckedCommandSeq,
                    m_localMoveYawProtection.commandSeq);
            const bool_t bServerCaughtProtectedYaw =
                IsYawClose(serverYawNear, protectedYawNear, 0.20f);
            const bool_t bUseProtectedYaw =
                m_localMoveYawProtection.bActive &&
                bLocalChampion &&
                es->netId() == m_localMoveYawProtection.netId &&
                localNetId == m_localMoveYawProtection.netId &&
                !bServerActionLocked &&
                !bServerCaughtProtectedYaw &&
                m_localMoveYawProtection.protectedSnapshotCount < kLocalMoveYawMaxProtectedSnapshots;
            const f32_t sourceYaw = bLocalChampion
                ? (bUseProtectedYaw ? protectedYawNear : es->yaw())
                : es->yaw();
            const f32_t resolvedYaw = MakeChampionVisualYawNear(sourceYaw, rot.y);
            tf.SetRotation(Vec3{
                rot.x,
                resolvedYaw,
                rot.z
                });
```

아래로 교체:

```cpp
            const f32_t serverYaw = NormalizeChampionVisualYaw(es->yaw());
            const f32_t protectedYaw =
                NormalizeChampionVisualYaw(m_localMoveYawProtection.yaw);
            const bool_t bSnapshotCoversProtectedCommand =
                IsCommandSeqAtLeast(
                    lastAckedCommandSeq,
                    m_localMoveYawProtection.commandSeq);
            const bool_t bServerCaughtProtectedYaw =
                IsYawClose(serverYaw, protectedYaw, 0.20f);
            const bool_t bUseProtectedYaw =
                m_localMoveYawProtection.bActive &&
                bLocalChampion &&
                es->netId() == m_localMoveYawProtection.netId &&
                localNetId == m_localMoveYawProtection.netId &&
                !bServerActionLocked &&
                !bServerCaughtProtectedYaw &&
                m_localMoveYawProtection.protectedSnapshotCount < kLocalMoveYawMaxProtectedSnapshots;
            const f32_t sourceYaw = bLocalChampion
                ? (bUseProtectedYaw ? protectedYaw : serverYaw)
                : serverYaw;
            const f32_t resolvedYaw = sourceYaw;
            tf.SetRotation(Vec3{
                rot.x,
                resolvedYaw,
                rot.z
                });
```

같은 함수 안의 로그 계산 변수도 이름과 값을 맞춘다.

기존 코드:

```cpp
                const f32_t yawDelta = NormalizeChampionVisualYaw(resolvedYaw - rot.y);
                const f32_t serverYawDelta = NormalizeChampionVisualYaw(serverYawNear - rot.y);
                const f32_t protectedYawDelta = NormalizeChampionVisualYaw(protectedYawNear - rot.y);
                const f32_t serverVsProtectedDelta =
                    NormalizeChampionVisualYaw(serverYawNear - protectedYawNear);
```

아래로 교체:

```cpp
                const f32_t yawDelta = NormalizeChampionVisualYaw(resolvedYaw - rot.y);
                const f32_t serverYawDelta = NormalizeChampionVisualYaw(serverYaw - rot.y);
                const f32_t protectedYawDelta = NormalizeChampionVisualYaw(protectedYaw - rot.y);
                const f32_t serverVsProtectedDelta =
                    NormalizeChampionVisualYaw(serverYaw - protectedYaw);
```

기존 코드:

```cpp
                const Vec3 serverForward =
                    GameplayForwardFromVisualYaw(championId, serverYawNear);
                const Vec3 appliedForward =
                    GameplayForwardFromVisualYaw(championId, resolvedYaw);
```

아래로 교체:

```cpp
                const Vec3 serverForward =
                    GameplayForwardFromVisualYaw(championId, serverYaw);
                const Vec3 appliedForward =
                    GameplayForwardFromVisualYaw(championId, resolvedYaw);
```

기존 코드:

```cpp
                        es->yaw(),
                        sourceYaw,
                        resolvedYaw,
```

아래로 교체:

```cpp
                        serverYaw,
                        sourceYaw,
                        resolvedYaw,
```

의도:

```text
- 스냅샷 적용이 더 이상 이전 프레임 yaw 근처의 2π 동치각을 만들지 않는다.
- protected yaw와 server yaw를 같은 canonical 표현으로 비교한다.
- SnapshotApply/RenderApply 로그에서 tfYaw/cacheYaw/appliedYaw가 [-PI, PI] 안에 머문다.
```

2. 검증

검증 명령:

```text
git diff --check -- Shared/GameSim/Systems/MoveSystem.cpp Shared/GameSim/Systems/CommandExecutor.cpp Client/Private/Scene/InGamePlayerTransformBridge.cpp Client/Private/Scene/InGamePlayerControlBridge.cpp Client/Private/Scene/InGameCombatInputBridge.cpp Client/Private/Network/Client/SnapshotApplier.cpp
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' .\Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1
```

런타임 확인:

```text
1. 더블클릭 이동에서 [YawTrace][ServerCommand] yawCalc가 -8, 8, 14 같은 누적/unwrapped 값으로 나오지 않아야 한다.
2. [YawTrace][MoveSystem] appliedYaw도 [-3.1416, 3.1416] 범위 안 canonical yaw여야 한다.
3. basic-attack chase start 직후에는 계속 source=intent-lock hadFacing=1 seq=N 이어야 한다.
4. path0Opposed=0 / facingSource=client-dir 상태에서 뒤도는 현상이 남으면 waypoint가 아니라 SnapshotApply 또는 RenderApply 로그를 본다.
5. [YawTrace][SnapshotApply] appliedYaw/sourceYaw/serverYaw와 [YawTrace][RenderApply] tfYaw/cacheYaw가 모두 canonical 범위 안에 있는지 확인한다.
6. 같은 클릭 방향을 빠르게 반복할 때 yawDelta가 거의 0이어야 한다.
7. 반대 방향을 찍었을 때 큰 yawDelta가 한 tick에 한 번 찍히는 것은 정상이다. 단, 화면에서 보간 회전처럼 흐르면 SnapshotApply/RenderApply 쪽에 남은 보정이 있는 것이다.
```

판정:

```text
- 이번 문제는 waypoint/tick lock 문제가 아니라 yaw 표현 보정 문제로 본다.
- 실패 로그는 source=move hadFacing=0 seq=0이 아니라, canonical 범위를 벗어난 appliedYaw/tfYaw/cacheYaw 또는 SnapshotApply source=protected/server 간 반대 방향 보정이다.
```
