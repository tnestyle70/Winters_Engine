Session - 서버 권위 champion 이동의 반전 버그 재발 없이 예전처럼 부드럽게 돌아가는 이동 연출을 복구한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
    void SyncPlayerEntityTransformFromECS();
    void SyncPlayerEntityTransformToECS();
    void UpdateNetworkChampionLocomotion(f32_t dt);
    bool_t IsNetworkChampionMoving(EntityID entity) const;
```

아래로 교체:

```cpp
    void SyncPlayerEntityTransformFromECS();
    void SyncPlayerEntityTransformToECS();
    void CaptureNetworkChampionInterpolationStarts();
    void BeginNetworkChampionInterpolationForSnapshot(u64_t serverTick);
    void ApplyNetworkChampionInterpolation(f32_t dt);
    void UpdateNetworkChampionLocomotion(f32_t dt);
    bool_t IsNetworkChampionMoving(EntityID entity) const;
```

기존 코드:

```cpp
    std::unordered_map<EntityID, Vec3>   m_NetworkChampionPrevPos{};
    std::unordered_map<EntityID, f32_t>  m_NetworkChampionMoveGraceSec{};
    std::unordered_map<EntityID, bool_t> m_NetworkChampionMoving{};
```

아래로 교체:

```cpp
    std::unordered_map<EntityID, Vec3>   m_NetworkChampionPrevPos{};
    std::unordered_map<EntityID, f32_t>  m_NetworkChampionMoveGraceSec{};
    std::unordered_map<EntityID, bool_t> m_NetworkChampionMoving{};
    struct NetworkChampionSnapshotInterpState
    {
        Vec3 vPendingStartPos{};
        Vec3 vPendingStartRot{};
        Vec3 vStartPos{};
        Vec3 vStartRot{};
        Vec3 vTargetPos{};
        Vec3 vTargetRot{};
        f32_t fElapsedSec = 0.f;
        f32_t fDurationSec = 0.06f;
        u64_t uSourceServerTick = 0;
        bool_t bActive = false;
        bool_t bHasPendingStart = false;
    };
    std::unordered_map<EntityID, NetworkChampionSnapshotInterpState> m_NetworkChampionInterpStates{};
    u64_t  m_uNetworkChampionInterpSnapshotTick = 0;
    bool_t m_bNetworkChampionInterpolationEnabled = true;
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:

```cpp
    constexpr f32_t kMoveTargetMaxSurfaceDeltaY = 3.f;
```

아래에 추가:

```cpp
    constexpr f32_t kNetworkChampionInterpDurationSec = 0.055f;
    constexpr f32_t kNetworkChampionInterpTeleportSq = 9.f;
    constexpr f32_t kNetworkChampionInterpMinMoveSq = 0.0001f;
    constexpr f32_t kNetworkChampionInterpMinYaw = 0.0005f;

    f32_t SmoothStep01(f32_t t)
    {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    }

    Vec3 LerpVec3(const Vec3& from, const Vec3& to, f32_t t)
    {
        return Vec3{
            from.x + (to.x - from.x) * t,
            from.y + (to.y - from.y) * t,
            from.z + (to.z - from.z) * t
        };
    }

    Vec3 LerpRotationNear(const Vec3& from, const Vec3& to, f32_t t)
    {
        Vec3 result = from;
        result.x = from.x + (to.x - from.x) * t;
        result.z = from.z + (to.z - from.z) * t;

        const f32_t targetYaw = MakeChampionVisualYawNear(to.y, from.y);
        result.y = from.y + NormalizeChampionVisualYaw(targetYaw - from.y) * t;
        return result;
    }
```

기존 코드:

```cpp
void CScene_InGame::SyncPlayerEntityTransformToECS()
{
    CInGamePlayerTransformBridge::SyncToECS(*this);
}

bool_t CScene_InGame::IsNetworkChampionMoving(EntityID entity) const
{
    const auto it = m_NetworkChampionMoveGraceSec.find(entity);
    return it != m_NetworkChampionMoveGraceSec.end() && it->second > 0.f;
}
```

아래로 교체:

```cpp
void CScene_InGame::SyncPlayerEntityTransformToECS()
{
    CInGamePlayerTransformBridge::SyncToECS(*this);
}

void CScene_InGame::CaptureNetworkChampionInterpolationStarts()
{
    if (!m_bNetworkAuthoritativeGameplay || !m_bNetworkChampionInterpolationEnabled)
        return;

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            auto& state = m_NetworkChampionInterpStates[e];
            state.vPendingStartPos = tf.GetPosition();
            state.vPendingStartRot = tf.GetRotation();
            state.bHasPendingStart = true;
        });
}

void CScene_InGame::BeginNetworkChampionInterpolationForSnapshot(u64_t serverTick)
{
    if (!m_bNetworkAuthoritativeGameplay || !m_bNetworkChampionInterpolationEnabled)
        return;
    if (serverTick == 0)
        return;

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            auto& state = m_NetworkChampionInterpStates[e];
            const Vec3 targetPos = tf.GetPosition();
            const Vec3 targetRot = tf.GetRotation();
            const Vec3 startPos = state.bHasPendingStart ? state.vPendingStartPos : targetPos;
            const Vec3 startRot = state.bHasPendingStart ? state.vPendingStartRot : targetRot;

            const f32_t dx = targetPos.x - startPos.x;
            const f32_t dz = targetPos.z - startPos.z;
            const f32_t distSq = dx * dx + dz * dz;
            const f32_t yawDelta = std::fabs(NormalizeChampionVisualYaw(targetRot.y - startRot.y));
            const bool_t bTinyChange =
                distSq <= kNetworkChampionInterpMinMoveSq &&
                yawDelta <= kNetworkChampionInterpMinYaw;
            const bool_t bTeleport = distSq >= kNetworkChampionInterpTeleportSq;
            const bool_t bLocalDashProtected =
                e == m_PlayerEntity && m_bKalistaPassiveDashActive;

            state.vStartPos = startPos;
            state.vStartRot = startRot;
            state.vTargetPos = targetPos;
            state.vTargetRot = targetRot;
            state.fElapsedSec = 0.f;
            state.fDurationSec = kNetworkChampionInterpDurationSec;
            state.uSourceServerTick = serverTick;
            state.bActive = !bTinyChange && !bTeleport && !bLocalDashProtected;
            state.bHasPendingStart = false;

            if (state.bActive)
            {
                tf.SetPosition(startPos);
                tf.SetRotation(startRot);
            }
            else
            {
                tf.SetPosition(targetPos);
                tf.SetRotation(targetRot);
            }
        });
}

void CScene_InGame::ApplyNetworkChampionInterpolation(f32_t dt)
{
    if (!m_bNetworkAuthoritativeGameplay || !m_bNetworkChampionInterpolationEnabled)
        return;

    for (auto& [entity, state] : m_NetworkChampionInterpStates)
    {
        if (!state.bActive)
            continue;
        if (!m_World.HasComponent<TransformComponent>(entity))
            continue;

        auto& tf = m_World.GetComponent<TransformComponent>(entity);
        state.fElapsedSec += dt;
        const f32_t denom = (state.fDurationSec > 0.001f) ? state.fDurationSec : 0.001f;
        const f32_t t = SmoothStep01(state.fElapsedSec / denom);

        tf.SetPosition(LerpVec3(state.vStartPos, state.vTargetPos, t));
        tf.SetRotation(LerpRotationNear(state.vStartRot, state.vTargetRot, t));

        if (t >= 1.f)
        {
            tf.SetPosition(state.vTargetPos);
            tf.SetRotation(state.vTargetRot);
            state.bActive = false;
        }
    }
}

bool_t CScene_InGame::IsNetworkChampionMoving(EntityID entity) const
{
    const auto it = m_NetworkChampionMoveGraceSec.find(entity);
    return it != m_NetworkChampionMoveGraceSec.end() && it->second > 0.f;
}
```

기존 코드:

```cpp
    WINTERS_PROFILE_SCOPE("Scene_InGame::OnUpdate");
    const bool_t bNetworkActive =
        CInGameNetworkBridge::Pump(m_pNetworkView, m_bUsingSharedNetwork);
```

아래로 교체:

```cpp
    WINTERS_PROFILE_SCOPE("Scene_InGame::OnUpdate");
    if (m_bNetworkAuthoritativeGameplay && m_bNetworkChampionInterpolationEnabled)
        CaptureNetworkChampionInterpolationStarts();

    const bool_t bNetworkActive =
        CInGameNetworkBridge::Pump(m_pNetworkView, m_bUsingSharedNetwork);

    const u64_t appliedSnapshotTick = m_pSnapshotApplier
        ? m_pSnapshotApplier->GetLastAppliedServerTick()
        : 0ull;
    if (m_bNetworkAuthoritativeGameplay &&
        bNetworkActive &&
        appliedSnapshotTick != 0 &&
        appliedSnapshotTick != m_uNetworkChampionInterpSnapshotTick)
    {
        BeginNetworkChampionInterpolationForSnapshot(appliedSnapshotTick);
        m_uNetworkChampionInterpSnapshotTick = appliedSnapshotTick;
    }
```

기존 코드:

```cpp
    if (s_frameCount <= 3) OutputDebugStringA("  [B] after Scheduler\n");

    SyncPlayerEntityTransformFromECS();

    if (bNetworkActive)
        UpdateNetworkChampionLocomotion(dt);
```

아래로 교체:

```cpp
    if (s_frameCount <= 3) OutputDebugStringA("  [B] after Scheduler\n");

    if (m_bNetworkAuthoritativeGameplay &&
        bNetworkActive &&
        m_bNetworkChampionInterpolationEnabled)
    {
        ApplyNetworkChampionInterpolation(dt);
    }

    SyncPlayerEntityTransformFromECS();

    if (bNetworkActive)
        UpdateNetworkChampionLocomotion(dt);
```

1-3. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameBootstrapBridge.cpp

기존 코드:

```cpp
        [&scene](EntityID entity)
        {
            scene.m_ChampionRenderers.erase(entity);
            scene.m_NetworkChampionPrevPos.erase(entity);
            scene.m_NetworkChampionMoveGraceSec.erase(entity);
            scene.m_NetworkChampionMoving.erase(entity);
        },
```

아래로 교체:

```cpp
        [&scene](EntityID entity)
        {
            scene.m_ChampionRenderers.erase(entity);
            scene.m_NetworkChampionPrevPos.erase(entity);
            scene.m_NetworkChampionMoveGraceSec.erase(entity);
            scene.m_NetworkChampionMoving.erase(entity);
            scene.m_NetworkChampionInterpStates.erase(entity);
        },
```

1-4. C:/Users/user/Desktop/Winters/Client/Private/Scene/InGameLifecycleBridge.cpp

기존 코드:

```cpp
    scene.m_ChampionRenderers.clear();
    scene.m_NetworkChampionPrevPos.clear();
    scene.m_NetworkChampionMoveGraceSec.clear();
    scene.m_NetworkChampionMoving.clear();
    scene.m_NetworkActionAnimStates.clear();
```

아래로 교체:

```cpp
    scene.m_ChampionRenderers.clear();
    scene.m_NetworkChampionPrevPos.clear();
    scene.m_NetworkChampionMoveGraceSec.clear();
    scene.m_NetworkChampionMoving.clear();
    scene.m_NetworkChampionInterpStates.clear();
    scene.m_uNetworkChampionInterpSnapshotTick = 0;
    scene.m_NetworkActionAnimStates.clear();
```

2. 검증

미검증:
- 계획서만 작성했으며, 위 코드는 아직 반영하지 않았다.

검증 명령:
- `git diff --check -- Client/Public/Scene/Scene_InGame.h Client/Private/Scene/Scene_InGame.cpp Client/Private/Scene/InGameBootstrapBridge.cpp Client/Private/Scene/InGameLifecycleBridge.cpp`
- `"C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nodeReuse:false`
- 필요하면 같은 변경을 `Debug-DX12|x64`에도 빌드한다.

수동 확인:
- VS 여러 시작 프로젝트에서 `Server`, `Client`를 함께 F5 실행한다.
- 우클릭 연타/동일 위치 더블클릭 시 뒤돌기 로그가 재발하지 않는지 확인한다.
- `[YawTrace][SyncFromECS]`에서 `halfTurn=1` 또는 `oldVsEcsDot`이 -1 근처로 튀지 않아야 한다.
- `[YawTrace][SnapshotApply]`는 서버 yaw 또는 보호 yaw를 적용하되, 그 직후 client NavSystem이 다시 덮는 패턴이 없어야 한다.
- 캐릭터와 카메라가 스냅샷 틱마다 딱딱 끊기는 대신 약 0.055초 안에서 따라붙는지 확인한다.
- 우클릭 이동 시작/연타 시 예전처럼 몸이 부드럽게 돌아가는 연출이 복구되어야 하며, 이 연출이 복구되지 않으면 반영 완료로 보지 않는다.
- 이동 입력 방향이 눈에 띄게 밀리거나, 클릭 위치가 체감상 어긋나면 백업 플랜을 적용한다.

백업 플랜:
- 1차 백업: `m_bNetworkChampionInterpolationEnabled = false`로 기본값을 바꿔 즉시 현재 안정 상태로 되돌린다.
- 2차 백업: local player는 보간 대상에서 제외하고 remote champion만 보간한다. `BeginNetworkChampionInterpolationForSnapshot` 안에서 `e == m_PlayerEntity`이면 `state.bActive = false`로 스냅샷 값을 그대로 둔다.
- 3차 백업: 위치 보간만 끄고 yaw 보간만 유지한다. 우클릭 체감 입력이 밀리는 경우 위치는 서버 스냅샷 그대로, yaw만 `LerpRotationNear`로 완화한다.
- 금지할 롤백: 끊김을 숨기기 위해 네트워크 권위 모드에서 `CNavigationSystem`을 다시 등록하지 않는다. 이 경로는 이미 snapshot-applied yaw를 `SyncFromECS` 전에 덮어 반전 버그를 만들었다.
