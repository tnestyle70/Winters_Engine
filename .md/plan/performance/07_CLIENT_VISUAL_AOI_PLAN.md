Session - Client visual AOI로 화면 밖 update/render 후보를 줄인다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:
```cpp
    void CaptureNetworkActorInterpolationStarts();
    void BeginNetworkActorInterpolationForSnapshot(u64_t serverTick);
    void ApplyNetworkActorInterpolation(f32_t dt);
    void UpdateNetworkChampionLocomotion(f32_t dt);
    void OnAuthoritativeSnapshot(u64_t serverTick,
        u64_t serverTimeMs,
        u32_t lastAckedCommandSeq,
        u32_t localNetId);
```

아래에 추가:
```cpp
    Vec3 ResolveClientVisualAoiCenter() const;
    bool_t IsClientVisualAlwaysHot(EntityID entity) const;
    bool_t IsClientVisualHot(EntityID entity) const;
    bool_t IsClientVisualWarm(EntityID entity) const;
    bool_t ShouldUpdateClientVisual(EntityID entity) const;
    bool_t ShouldRenderClientVisual(EntityID entity,
        u8_t localTeam,
        bool_t bRevealAllForPlayback) const;
```

1-2. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/Scene_InGame.cpp

기존 코드:
```cpp
    constexpr f32_t kNetworkActorInterpDurationSec = 0.055f;
    constexpr f32_t kNetworkActorInterpTeleportSq = 9.f;
    constexpr f32_t kNetworkActorInterpMinMoveSq = 0.0001f;
    constexpr f32_t kNetworkActorInterpMinYaw = 0.0005f;
```

아래에 추가:
```cpp
    constexpr f32_t kClientVisualAoiHotRadius = 72.f;
    constexpr f32_t kClientVisualAoiWarmRadius = 108.f;
    constexpr f32_t kClientVisualAoiRenderRadius = 120.f;

    f32_t DistanceSqXZLocal(const Vec3& a, const Vec3& b)
    {
        const f32_t dx = a.x - b.x;
        const f32_t dz = a.z - b.z;
        return dx * dx + dz * dz;
    }
```

기존 코드:
```cpp
void CScene_InGame::SyncPlayerEntityTransformToECS()
{
    CInGamePlayerTransformBridge::SyncToECS(*this);
}
```

아래에 추가:
```cpp
Vec3 CScene_InGame::ResolveClientVisualAoiCenter() const
{
    if (m_pCamera)
        return m_pCamera->GetAt();

    if (m_pPlayerTransform)
        return m_pPlayerTransform->GetPosition();

    if (m_PlayerEntity != NULL_ENTITY &&
        m_World.HasComponent<TransformComponent>(m_PlayerEntity))
    {
        return m_World.GetComponent<TransformComponent>(m_PlayerEntity).GetPosition();
    }

    return Vec3{};
}

bool_t CScene_InGame::IsClientVisualAlwaysHot(EntityID entity) const
{
    if (entity == NULL_ENTITY)
        return true;
    if (entity == m_PlayerEntity)
        return true;
    if (m_bReplayPlaybackMode)
        return true;

    const auto actionIt = m_NetworkActionAnimStates.find(entity);
    if (actionIt != m_NetworkActionAnimStates.end() &&
        actionIt->second.bActionActive)
    {
        return true;
    }

    if (m_World.HasComponent<ReplicatedStateComponent>(entity))
    {
        const auto& replicated = m_World.GetComponent<ReplicatedStateComponent>(entity);
        if ((replicated.stateFlags &
            (kSnapshotStateDeadFlag | kSnapshotStateAttackFlag)) != 0u)
        {
            return true;
        }
    }

    return false;
}

bool_t CScene_InGame::IsClientVisualHot(EntityID entity) const
{
    if (IsClientVisualAlwaysHot(entity))
        return true;
    if (!m_World.HasComponent<TransformComponent>(entity))
        return true;

    const Vec3 center = ResolveClientVisualAoiCenter();
    const Vec3 pos = m_World.GetComponent<TransformComponent>(entity).GetPosition();
    return DistanceSqXZLocal(center, pos) <=
        kClientVisualAoiHotRadius * kClientVisualAoiHotRadius;
}

bool_t CScene_InGame::IsClientVisualWarm(EntityID entity) const
{
    if (IsClientVisualAlwaysHot(entity))
        return true;
    if (!m_World.HasComponent<TransformComponent>(entity))
        return true;

    const Vec3 center = ResolveClientVisualAoiCenter();
    const Vec3 pos = m_World.GetComponent<TransformComponent>(entity).GetPosition();
    return DistanceSqXZLocal(center, pos) <=
        kClientVisualAoiWarmRadius * kClientVisualAoiWarmRadius;
}

bool_t CScene_InGame::ShouldUpdateClientVisual(EntityID entity) const
{
    return IsClientVisualHot(entity);
}

bool_t CScene_InGame::ShouldRenderClientVisual(
    EntityID entity,
    u8_t localTeam,
    bool_t bRevealAllForPlayback) const
{
    if (!UI::IsRenderableForLocal(
        const_cast<CWorld&>(m_World),
        entity,
        localTeam,
        bRevealAllForPlayback))
    {
        return false;
    }

    if (bRevealAllForPlayback || IsClientVisualAlwaysHot(entity))
        return true;
    if (!m_World.HasComponent<TransformComponent>(entity))
        return true;

    const Vec3 center = ResolveClientVisualAoiCenter();
    const Vec3 pos = m_World.GetComponent<TransformComponent>(entity).GetPosition();
    return DistanceSqXZLocal(center, pos) <=
        kClientVisualAoiRenderRadius * kClientVisualAoiRenderRadius;
}
```

기존 코드:
```cpp
    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            capture(e, tf);
        });

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID e, MinionStateComponent& ms, TransformComponent& tf)
        {
            if (ms.current == MinionStateComponent::Dead)
                return;

            capture(e, tf);
        });
```

아래로 교체:
```cpp
    uint64_t capturedCount = 0;
    uint64_t skippedCount = 0;

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            if (!IsClientVisualWarm(e))
            {
                ++skippedCount;
                return;
            }

            capture(e, tf);
            ++capturedCount;
        });

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID e, MinionStateComponent& ms, TransformComponent& tf)
        {
            if (ms.current == MinionStateComponent::Dead)
                return;
            if (!IsClientVisualWarm(e))
            {
                ++skippedCount;
                return;
            }

            capture(e, tf);
            ++capturedCount;
        });

    WINTERS_PROFILE_COUNT("AOI::InterpCaptured", capturedCount);
    WINTERS_PROFILE_COUNT("AOI::InterpCaptureSkipped", skippedCount);
```

기존 코드:
```cpp
    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            const bool_t bLocalDashProtected =
                e == m_PlayerEntity && m_bKalistaPassiveDashActive;
            begin(e, tf, bLocalDashProtected);
        });

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID e, MinionStateComponent& ms, TransformComponent& tf)
        {
            if (ms.current == MinionStateComponent::Dead)
                return;

            begin(e, tf, false);
        });
```

아래로 교체:
```cpp
    uint64_t beganCount = 0;
    uint64_t skippedCount = 0;

    m_World.ForEach<ChampionComponent, TransformComponent>(
        [&](EntityID e, ChampionComponent&, TransformComponent& tf)
        {
            if (!IsClientVisualWarm(e))
            {
                ++skippedCount;
                return;
            }

            const bool_t bLocalDashProtected =
                e == m_PlayerEntity && m_bKalistaPassiveDashActive;
            begin(e, tf, bLocalDashProtected);
            ++beganCount;
        });

    m_World.ForEach<MinionStateComponent, TransformComponent>(
        [&](EntityID e, MinionStateComponent& ms, TransformComponent& tf)
        {
            if (ms.current == MinionStateComponent::Dead)
                return;
            if (!IsClientVisualWarm(e))
            {
                ++skippedCount;
                return;
            }

            begin(e, tf, false);
            ++beganCount;
        });

    WINTERS_PROFILE_COUNT("AOI::InterpBegan", beganCount);
    WINTERS_PROFILE_COUNT("AOI::InterpBeginSkipped", skippedCount);
```

기존 코드:
```cpp
    //ECS owned ModelRenderer
    {
        WINTERS_PROFILE_SCOPE("Champion::AnimUpdate");
        m_World.ForEach<ChampionComponent, RenderComponent>(
            [dt](EntityID, ChampionComponent&, RenderComponent& rc)
            {
                if (rc.bSceneManaged) return;
                if (!rc.pRenderer || !rc.bAnimated) return;
                if (!rc.pRenderer->HasSkeleton()) return;
                rc.pRenderer->Update(dt);
            }
        );
    }
```

아래로 교체:
```cpp
    //ECS owned ModelRenderer
    {
        WINTERS_PROFILE_SCOPE("Champion::AnimUpdate");
        uint64_t animCount = 0;
        uint64_t aoiSkippedCount = 0;
        m_World.ForEach<ChampionComponent, RenderComponent, TransformComponent>(
            [this, dt, &animCount, &aoiSkippedCount](
                EntityID e,
                ChampionComponent&,
                RenderComponent& rc,
                TransformComponent&)
            {
                if (rc.bSceneManaged) return;
                if (!rc.pRenderer || !rc.bAnimated) return;
                if (!rc.pRenderer->HasSkeleton()) return;
                if (!ShouldUpdateClientVisual(e))
                {
                    ++aoiSkippedCount;
                    return;
                }

                rc.pRenderer->Update(dt);
                ++animCount;
            }
        );
        WINTERS_PROFILE_COUNT("AOI::ChampionAnimUpdated", animCount);
        WINTERS_PROFILE_COUNT("AOI::ChampionAnimSkipped", aoiSkippedCount);
    }
```

기존 코드:
```cpp
        if (m_bNetworkAuthoritativeGameplay)
            CMinion_Manager::Get()->TickVisuals(dt);
        else
            CMinion_Manager::Get()->Tick(dt);
```

아래로 교체:
```cpp
        if (m_bNetworkAuthoritativeGameplay)
        {
            CMinion_Manager::Get()->TickVisuals(
                dt,
                ResolveClientVisualAoiCenter(),
                kClientVisualAoiHotRadius,
                kClientVisualAoiWarmRadius);
        }
        else
        {
            CMinion_Manager::Get()->Tick(dt);
        }
```

1-3. C:/Users/tnest/Desktop/Winters/Client/Public/Manager/Minion_Manager.h

기존 코드:
```cpp
    void    TickVisuals(f32_t fDeltaTime);
```

아래로 교체:
```cpp
    void    TickVisuals(f32_t fDeltaTime,
        const Vec3& vAoiCenter = Vec3{},
        f32_t fHotRadius = 0.f,
        f32_t fWarmRadius = 0.f);
```

1-4. C:/Users/tnest/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

기존 코드:
```cpp
void CMinion_Manager::TickVisuals(f32_t fDeltaTime)
{
    if (!m_pWorld)
        return;
```

아래로 교체:
```cpp
void CMinion_Manager::TickVisuals(
    f32_t fDeltaTime,
    const Vec3& vAoiCenter,
    f32_t fHotRadius,
    f32_t fWarmRadius)
{
    if (!m_pWorld)
        return;

    const bool_t bUseAoi = fWarmRadius > 0.f;
    const f32_t fHotRadiusSq = fHotRadius * fHotRadius;
    const f32_t fWarmRadiusSq = fWarmRadius * fWarmRadius;

    auto isWithinAoi = [this, &vAoiCenter](EntityID id, f32_t radiusSq) -> bool_t
    {
        if (!m_pWorld->HasComponent<TransformComponent>(id))
            return true;

        const Vec3 pos =
            m_pWorld->GetComponent<TransformComponent>(id).GetPosition();
        return DistanceSqXZLocal(vAoiCenter, pos) <= radiusSq;
    };
```

CONFIRM_NEEDED:
위 코드에서 `DistanceSqXZLocal`은 Scene_InGame.cpp의 익명 namespace 함수와 이름이 같아도 파일 범위가 달라 충돌하지 않는다.
구현 직전 Minion_Manager.cpp 익명 namespace에 같은 helper가 이미 있는지 확인하고, 없으면 아래 함수를 namespace 안에 추가한다.

```cpp
f32_t DistanceSqXZLocal(const Vec3& a, const Vec3& b)
{
    const f32_t dx = a.x - b.x;
    const f32_t dz = a.z - b.z;
    return dx * dx + dz * dz;
}
```

기존 코드:
```cpp
    m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
        [this, fDeltaTime](EntityID id, MinionStateComponent& ms, RenderComponent& rc)
        {
            UpdateMinionVisual(id, ms, rc, fDeltaTime);
        });
```

아래로 교체:
```cpp
    uint64_t visualUpdateCount = 0;
    uint64_t visualAoiSkippedCount = 0;
    m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
        [this, fDeltaTime, bUseAoi, fWarmRadiusSq,
            &isWithinAoi, &visualUpdateCount, &visualAoiSkippedCount](
            EntityID id,
            MinionStateComponent& ms,
            RenderComponent& rc)
        {
            const bool_t bHighPriority =
                ms.current == MinionStateComponent::Attack ||
                ms.current == MinionStateComponent::Dead;
            if (bUseAoi && !bHighPriority && !isWithinAoi(id, fWarmRadiusSq))
            {
                ++visualAoiSkippedCount;
                return;
            }

            UpdateMinionVisual(id, ms, rc, fDeltaTime);
            ++visualUpdateCount;
        });
    WINTERS_PROFILE_COUNT("AOI::MinionVisualUpdated", visualUpdateCount);
    WINTERS_PROFILE_COUNT("AOI::MinionVisualSkipped", visualAoiSkippedCount);
```

기존 코드:
```cpp
                const bool_t bHighPriorityAnim =
                    ms.current == MinionStateComponent::Attack ||
                    ms.current == MinionStateComponent::Dead;
                const f32_t updateInterval = bHighPriorityAnim
                    ? kMinionHighPriorityAnimUpdateInterval
                    : (std::max)(ms.animUpdateInterval, kMinionBaseAnimUpdateInterval);
```

아래에 추가:
```cpp
                if (bUseAoi && !bHighPriorityAnim && !isWithinAoi(id, fHotRadiusSq))
                {
                    ++skippedCount;
                    ++aoiSkippedCount;
                    return;
                }
```

기존 코드:
```cpp
        uint64_t animCount = 0;
        uint64_t skippedCount = 0;
        uint64_t visibilitySkippedCount = 0;
        uint64_t budgetSkippedCount = 0;
        m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
            [this, fDeltaTime, localTeam,
                &animCount, &skippedCount, &visibilitySkippedCount, &budgetSkippedCount](
                EntityID id,
```

아래로 교체:
```cpp
        uint64_t animCount = 0;
        uint64_t skippedCount = 0;
        uint64_t visibilitySkippedCount = 0;
        uint64_t budgetSkippedCount = 0;
        uint64_t aoiSkippedCount = 0;
        m_pWorld->ForEach<MinionStateComponent, RenderComponent>(
            [this, fDeltaTime, localTeam, bUseAoi, fHotRadiusSq, &isWithinAoi,
                &animCount, &skippedCount, &visibilitySkippedCount,
                &budgetSkippedCount, &aoiSkippedCount](
                EntityID id,
```

기존 코드:
```cpp
        WINTERS_PROFILE_COUNT("Anim::UpdateCalls", animCount);
        WINTERS_PROFILE_COUNT("Anim::Skipped", skippedCount);
        WINTERS_PROFILE_COUNT("Anim::VisibilitySkipped", visibilitySkippedCount);
        WINTERS_PROFILE_COUNT("Anim::BudgetSkipped", budgetSkippedCount);
```

아래에 추가:
```cpp
        WINTERS_PROFILE_COUNT("Anim::AoiSkipped", aoiSkippedCount);
```

1-5. C:/Users/tnest/Desktop/Winters/Client/Private/Scene/InGameRenderBridge.cpp

기존 코드:
```cpp
                if (!UI::IsRenderableForLocal(scene.m_World, e, localTeam, bRevealAllForPlayback))
                    return;
```

아래로 교체:
```cpp
                if (!scene.ShouldRenderClientVisual(e, localTeam, bRevealAllForPlayback))
                    return;
```

적용 위치:
```text
같은 파일의 Champion normal pass, Champion main render, ContactShadow champion loop에 있는 동일한 조건 3곳에 적용한다.
Structure/Jungle/Minion render는 1차 적용 후 profiler에서 남는 비용을 보고 별도 세션에서 candidate-list 기반으로 줄인다.
```

1-6. C:/Users/tnest/Desktop/Winters/EngineSDK/inc

기존 코드:
```text
직접 수정하지 않는다.
```

아래에 추가:
```text
이번 1차 AOI 계획은 Engine public header를 바꾸지 않는다.
EngineSDK 동기화는 필요 없다.
```

2. 검증

```text
빌드:
- git diff --check
- "C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe" Client/Include/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m

Profiler 확인:
- `AOI::ChampionAnimUpdated`
- `AOI::ChampionAnimSkipped`
- `AOI::MinionVisualUpdated`
- `AOI::MinionVisualSkipped`
- `Anim::AoiSkipped`
- `AOI::InterpCaptured`
- `AOI::InterpCaptureSkipped`
- `AOI::InterpBegan`
- `AOI::InterpBeginSkipped`

합격선:
- 카메라 밖 미니언/챔피언이 많을 때 `Scene_InGame::OnUpdate`, `Champion::AnimUpdate`, `Minion::AnimUpdate`가 내려간다.
- 화면 안으로 들어온 미니언/챔피언이 1초 이상 멈춘 애니메이션 상태로 보이지 않는다.
- 로컬 플레이어, 공격/피격/사망/액션 중인 대상은 AOI 밖이어도 visual update가 끊기지 않는다.
- Replay playback에서는 AOI가 과하게 잘라내지 않는다.

수동 QA:
- F5 일반 플레이에서 `--uncapped --no-vsync`로 30초 측정.
- 카메라를 미니언 웨이브 밖으로 이동했을 때 `AOI::*Skipped`가 증가하는지 확인.
- 다시 웨이브를 화면 안에 넣었을 때 위치는 서버 snapshot 기준으로 즉시 맞고, 애니메이션은 다음 hot update부터 자연스럽게 재개되는지 확인.
- FOW 밖 적/미니언이 render와 healthbar에 나오지 않는지 확인.

확인 필요:
- UI healthbar는 아직 전체 ForEach 후 screen/FOW 필터 성격이므로, 1차 적용 후 `UI::RHIHealthBars`가 계속 크면 UI_Manager에 같은 AOI center/radius를 전달하는 S08로 분리한다.
- FX system은 스킬별 lifetime/owner 관계가 있어 1차에서 무리하게 skip하지 않는다. `Fx::Update`, `FxMesh::Update`, `FxBeam::Update`가 다시 병목이면 owner/entity AOI 기반 budget 세션으로 분리한다.
```
