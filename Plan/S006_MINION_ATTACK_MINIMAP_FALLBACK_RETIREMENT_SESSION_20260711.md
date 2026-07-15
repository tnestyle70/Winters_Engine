Session - 미니언 한 공격을 한 번의 정·역방향 clip traversal로 고정하고, Map11 미니맵 실측 calibration을 복원하며, authored WFX와 중복되는 전투 legacy fallback 이미지를 런타임에서 전부 폐기한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Client/Public/Manager/Minion_Manager.h

`eMinionVisualPhase`와 `MinionVisualPlaybackState`를 아래로 교체:

기존 코드:

```cpp
    enum class eMinionVisualPhase : uint8_t
    {
        Base,
        Attack,
        Recover,
        Death
    };

    struct MinionVisualPlaybackState
    {
        eMinionVisualPhase phase = eMinionVisualPhase::Base;
        uint32_t lastActionSeq = 0;
        uint16_t lastAnimId = 0;
        uint8_t baseState = 0xff;
        f32_t phaseTimer = 0.f;
        bool_t bPendingAttack = false;
    };
```

아래로 교체:

```cpp
    enum class eMinionVisualPhase : uint8_t
    {
        Base,
        Attack,
        Death
    };

    struct MinionVisualPlaybackState
    {
        eMinionVisualPhase phase = eMinionVisualPhase::Base;
        uint32_t lastActionSeq = 0;
        uint16_t lastAnimId = 0;
        uint8_t baseState = 0xff;
        f32_t phaseTimer = 0.f;
        bool_t bReverseLocalAttack = false;
    };
```

1-2. C:/Users/user/Desktop/Winters/Client/Private/Manager/Minion_Manager.cpp

`ResolveMinionAttackWindupSeconds`를 아래로 교체:

기존 코드:

```cpp
    f32_t ResolveMinionAttackWindupSeconds(const MinionStateComponent& state)
    {
        return state.attackWindup > 0.01f
            ? state.attackWindup
            : 0.25f;
    }
```

아래로 교체:

```cpp
    f32_t ResolveMinionAttackCycleSeconds(const MinionStateComponent& state)
    {
        const f32_t windupSeconds = state.attackWindup > 0.01f
            ? state.attackWindup
            : 0.25f;
        const f32_t recoverySeconds = state.attackRecovery > 0.01f
            ? state.attackRecovery
            : kMinionRecoverSeconds;
        return windupSeconds + recoverySeconds;
    }
```

`CMinion_Manager::UpdateMinionVisual` 전체를 아래로 교체:

```cpp
void CMinion_Manager::UpdateMinionVisual(
    EntityID entity,
    MinionStateComponent& ms,
    RenderComponent& rc,
    f32_t fDeltaTime)
{
    if (!rc.pRenderer)
        return;

    auto& visual = m_mapVisualStates[entity];

    const PoseStateComponent* pPose = nullptr;
    if (m_pWorld && m_pWorld->HasComponent<PoseStateComponent>(entity))
        pPose = &m_pWorld->GetComponent<PoseStateComponent>(entity);

    const ActionStateComponent* pAction = nullptr;
    if (m_pWorld && m_pWorld->HasComponent<ActionStateComponent>(entity))
        pAction = &m_pWorld->GetComponent<ActionStateComponent>(entity);

    if (ms.current == MinionStateComponent::Dead)
    {
        if (visual.phase != eMinionVisualPhase::Death)
        {
            if (rc.pRenderer->HasAnimationByName("death"))
                rc.pRenderer->PlayAnimationByNameAdvanced("death", false, false, 1.f);
            visual.phase = eMinionVisualPhase::Death;
            visual.baseState = kInvalidMinionVisualBaseState;
        }

        ms.visualState = MinionStateComponent::Dead;
        ms.bAttackAnimRequested = false;
        return;
    }

    const uint16_t actionId = pAction
        ? pAction->actionId
        : static_cast<uint16_t>(eActionStateId::None);
    const uint32_t actionSeq = pAction ? pAction->sequence : 0u;
    const bool_t bNetworkBasicAttack =
        pAction &&
        ms.current == MinionStateComponent::Attack &&
        static_cast<eActionStateId>(actionId) == eActionStateId::BasicAttack &&
        actionSeq != 0u &&
        (visual.lastActionSeq != actionSeq || visual.lastAnimId != actionId);
    const bool_t bLocalBasicAttack =
        actionSeq == 0u &&
        ms.current == MinionStateComponent::Attack &&
        ms.bAttackAnimRequested;

    if (bNetworkBasicAttack || bLocalBasicAttack)
    {
        visual.lastActionSeq = actionSeq;
        visual.lastAnimId = actionId;
        ms.bAttackAnimRequested = false;

        if (rc.pRenderer->HasAnimationByName("attack"))
        {
            const f32_t attackSeconds = ResolveMinionAnimationSeconds(
                *rc.pRenderer,
                "attack",
                kFallbackMinionAttackSeconds);
            const f32_t cycleSeconds = ResolveMinionAttackCycleSeconds(ms);
            const bool_t bReverseAttack = actionSeq != 0u
                ? ((actionSeq & 1u) == 0u)
                : visual.bReverseLocalAttack;
            if (actionSeq == 0u)
                visual.bReverseLocalAttack = !visual.bReverseLocalAttack;

            rc.pRenderer->PlayAnimationByNameAdvanced(
                "attack",
                false,
                bReverseAttack,
                attackSeconds / cycleSeconds);
            visual.phase = eMinionVisualPhase::Attack;
            visual.phaseTimer = cycleSeconds;
            visual.baseState = kInvalidMinionVisualBaseState;
            ms.visualState = MinionStateComponent::Attack;
            ms.animUpdateAccumulator = kMinionHighPriorityAnimUpdateInterval;

#if defined(_DEBUG)
            static u32_t s_minionAnimTraceCount = 0u;
            if (s_minionAnimTraceCount < 128u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[MinionAnim] entity=%u seq=%u direction=%s clip=%.3f cycle=%.3f\n",
                    static_cast<u32_t>(entity),
                    actionSeq,
                    bReverseAttack ? "reverse" : "forward",
                    attackSeconds,
                    cycleSeconds);
                OutputDebugStringA(msg);
                ++s_minionAnimTraceCount;
            }
#endif
            return;
        }

        visual.phase = eMinionVisualPhase::Base;
        visual.baseState = kInvalidMinionVisualBaseState;
    }

    if (visual.phase == eMinionVisualPhase::Attack)
    {
        visual.phaseTimer -= fDeltaTime;
        if (visual.phaseTimer > 0.f)
            return;

        visual.phase = eMinionVisualPhase::Base;
        visual.baseState = kInvalidMinionVisualBaseState;
    }

    const MinionStateComponent::State baseState =
        ResolveMinionBaseVisualState(ms, pPose);
    const uint8_t baseStateValue = static_cast<uint8_t>(baseState);

    if (visual.baseState != baseStateValue)
    {
        const char* pBaseKeyword =
            ResolveMinionBaseAnimationKeyword(*rc.pRenderer, baseState);
        if (pBaseKeyword)
            rc.pRenderer->PlayAnimationByNameAdvanced(pBaseKeyword, true, false, 1.f);

        visual.baseState = baseStateValue;
    }

    visual.phase = eMinionVisualPhase::Base;
    ms.visualState = baseState;
    ms.bAttackAnimRequested = false;
}
```

한 `ActionStateComponent.sequence`에서 `attack` 재생 호출은 정확히 한 번이다. 홀수 sequence는 forward, 짝수 sequence는 reverse이며, local smoke의 sequence 0은 `bReverseLocalAttack`으로 교대한다. clip 전체 시간은 서버가 복제한 `attackWindup + attackRecovery`에 맞춘다. contact marker가 없는 현 asset에서 타격 프레임을 임의로 만들지 않으며, 실제 피해·투사체는 서버의 windup 경계 한 번만 유지한다.

1-3. C:/Users/user/Desktop/Winters/Client/Public/UI/MinimapPanel.h

`MinimapProjection`을 아래로 교체:

기존 코드:

```cpp
    struct MinimapProjection
    {
        Vec2 vWorldAtUv00{ 104.50f, 181.02f };
        Vec2 vWorldAtUv10{ 285.52f, 0.00f };
        Vec2 vWorldAtUv01{ -76.52f, 0.00f };
    };
```

아래로 교체:

```cpp
    struct MinimapProjection
    {
        Vec2 vWorldAtUv00{ 104.50f, 156.69f };
        Vec2 vWorldAtUv10{ 198.885f, 0.00f };
        Vec2 vWorldAtUv01{ 10.115f, 0.00f };
    };
```

이 값은 현재 정확한 mid 중심 `(104.5, 0)`을 보존하면서, f9d4d5c 이전 실측 landmark의 X/Z span을 중심 대칭으로 복원한다. `Stage1.dat` 구조물·웨이포인트는 현재 UV 약 `0.247~0.753`에서 약 `0.059~0.951`로 확장된다. `MinimapPanel.cpp`의 아이콘 크기나 사각형 크기는 수정하지 않는다. 같은 projection을 쓰는 아이콘, 카메라 박스, 클릭 이동, FOW가 함께 보정된다.

1-4. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Projectile/ProjectileVisualCatalog.h

`ProjectileVisualDesc`를 아래로 교체:

```cpp
struct ProjectileVisualDesc
{
    const char* pszSpawnCue = nullptr;
    const char* pszHitCue = nullptr;
    const char* pszAttachedCue = nullptr;
};
```

fallback texture, 크기, enable 필드는 전부 삭제한다.

1-5. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Projectile/ProjectileVisualCatalog.cpp

파일의 anonymous namespace와 `Resolve`가 소비하는 visual 상수를 아래 계약으로 교체한다:

```cpp
namespace
{
    constexpr u16_t kStructureProjectileKind = 100;

    constexpr ProjectileVisualDesc kNoProjectileVisual{};
    constexpr ProjectileVisualDesc kEzrealMysticShotVisual{
        nullptr, "Ezreal.Q.Hit", nullptr
    };
    constexpr ProjectileVisualDesc kLeeSinQVisual{
        "LeeSin.Q.Projectile", "LeeSin.Q.Hit", "LeeSin.Q.Mark"
    };
    constexpr ProjectileVisualDesc kZedShurikenVisual{
        "Zed.Q.Projectile", "Zed.Q.Hit", nullptr
    };
    constexpr ProjectileVisualDesc kAsheVolleyArrowVisual{
        "Ashe.W.Arrow", "Ashe.W.Hit", nullptr
    };
    constexpr ProjectileVisualDesc kAsheCrystalArrowVisual{
        "Ashe.R.Arrow", "Ashe.R.Hit", nullptr
    };
    constexpr ProjectileVisualDesc kStructureProjectileVisual{
        "Turret.Projectile.Red", "Turret.Projectile.Hit.Red", nullptr
    };
    constexpr ProjectileVisualDesc kMinionRangedBlueVisual{
        "Minion.Ranged.Projectile.Blue", nullptr, nullptr
    };
    constexpr ProjectileVisualDesc kMinionRangedRedVisual{
        "Minion.Ranged.Projectile.Red", nullptr, nullptr
    };
}
```

`Resolve`의 Wind/Tornado/EQRing/Sylas/default는 `kNoProjectileVisual`을 반환한다. Kalista generic glow/fire sphere와 Ezreal PNG fallback 상수는 삭제한다.

1-6. C:/Users/user/Desktop/Winters/Client/Public/Network/Client/EventApplier.h

삭제할 코드:

```cpp
    EntityID SpawnBillboard(CWorld& world, const Vec3& pos, const Vec3& velocity,
        const wchar_t* texturePath, f32_t width, f32_t height, f32_t lifetime,
        EntityID attachTo = NULL_ENTITY);
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

삭제할 include:

```cpp
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
```

삭제할 상수:

```cpp
    constexpr const wchar_t* kTurretTopBeamTexture =
        L"Texture/Object/Turret/particles/TurretTopBeam.png";
    constexpr const wchar_t* kEffectTexture = L"Texture/FX/Kalista/common_global_indicator_ring_bright.png";
    constexpr const wchar_t* kDamageTexture = L"Texture/FX/Kalista/common_color-hit-physical.png";
```

삭제할 함수 전체:

```cpp
    void SpawnTurretTopBeam(CWorld& world, EntityID ownerEntity, const Vec3& fallbackPos)
```

`ApplyProjectileSpawn`에서 아래 호출과 generic spawn fallback 계산·두 `SpawnBillboard` 분기를 삭제한다:

```cpp
    const EntityID ownerEntity = ResolveLiveEntity(world, entityMap, ev->ownerNet());
    if (bStructureProjectile)
        SpawnTurretTopBeam(world, ownerEntity, pos);

    const bool_t bShouldSpawnGenericProjectile = ...;
```

`netId == NULL_NET_ENTITY`이면 WFX cue 결과만 남긴 뒤 그대로 return한다. network projectile entity 생성, `projectileVisualEntities`, structure WFX mesh 검사는 보존한다.

`ApplyProjectileHit`의 named hit/attached cue 호출은 보존하고 아래 generic hit fallback을 삭제한다:

```cpp
    if (!bPlayedWfxCue &&
        visual.bUseGenericHitFallback &&
        visual.pszFallbackHitTexture)
    {
        SpawnBillboard(...);
    }
```

`ApplyEffectTrigger` 끝은 아래 named cue 호출까지만 남기고 generic ring을 삭제한다:

```cpp
    if (const char* pszCueName = ResolveEffectTriggerCue(hookChampion, hookSlot, skillStage))
    {
        FxCueContext fx{};
        fx.vWorldPos = pos;
        fx.vForward = WintersMath::Normalize3D(Vec3{ ev->dirX(), ev->dirY(), ev->dirZ() });
        fx.attachTo = bKeepEventPosition ? NULL_ENTITY : attachTo;
        fx.pFxMeshRenderer = m_pFxMeshRenderer;
        CFxCuePlayer::Play(world, pszCueName, fx);
    }
```

`ApplyDamage`에서 damage number와 kill count는 보존하고 아래 legacy hit sprite를 삭제한다:

```cpp
    if (!IsMinionEntity(world, source) && !IsMinionEntity(world, target))
        SpawnBillboard(world, pos, Vec3{}, kDamageTexture, 1.0f, 1.0f, 0.25f, target);
```

마지막 `CEventApplier::SpawnBillboard` 함수 전체를 삭제한다. `CFxCuePlayer`가 누락 cue를 Debug Output에 최대 64회 기록하므로 새 placeholder는 추가하지 않는다.

1-8. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

삭제할 메서드 선언:

```cpp
    void    DrawHUDStatusFlash(ImDrawList* pDraw, const ImVec2& root, f32_t hudW, f32_t hudH);
    void    UpdateHUDStatusTimers(EntityID localEntity, f32_t hp, bool_t bStunned, f32_t dt);
```

삭제할 SRV:

```cpp
    void* m_pSRV_HUDHit = nullptr;
    void* m_pSRV_HUDStun = nullptr;
```

삭제할 상태:

```cpp
    bool_t                    m_bShowHUDStatusFlash = true;
    f32_t                     m_fHUDHitFlashDuration = 0.5f;
    f32_t                     m_fHUDStunFlashDuration = 0.5f;
    f32_t                     m_fHUDHitFlashTimer = 0.f;
    f32_t                     m_fHUDStunFlashTimer = 0.f;
    EntityID                  m_LastHUDLocalEntity = NULL_ENTITY;
    f32_t                     m_fLastHUDHP = 0.f;
    bool_t                    m_bHasLastHUDHP = false;
    bool_t                    m_bWasLocalStunned = false;
```

1-9. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

삭제할 경로, load, release:

```cpp
    constexpr const wchar_t* kPathHUDHit = L"Resource/Texture/UI/HUD/lol_ingame_hit.png";
    constexpr const wchar_t* kPathHUDStun = L"Resource/Texture/UI/HUD/lol_ingame_stun.png";
```

```cpp
    if (FAILED(Load_TextureSRV(kPathHUDHit, &m_pSRV_HUDHit))) { ... }
    if (FAILED(Load_TextureSRV(kPathHUDStun, &m_pSRV_HUDStun))) { ... }
```

```cpp
    ReleaseSRV(m_pSRV_HUDHit);
    ReleaseSRV(m_pSRV_HUDStun);
```

`Render_Overlay`에서 `UpdateHUDStatusTimers` 호출 블록, `UpdateHUDStatusTimers` 함수 전체, `DrawHUDStatusFlash` 함수 전체, `DrawActorHUDOverlay`의 `DrawHUDStatusFlash(...)` 호출을 삭제한다.

HUD tuner에서 아래 전체를 삭제한다:

```cpp
    ImGui::Text("Hit Flash: %s", m_pSRV_HUDHit ? "loaded" : "FALLBACK");
    ImGui::Text("Stun Flash: %s", m_pSRV_HUDStun ? "loaded" : "FALLBACK");
    ImGui::Checkbox("HUD Hit/Stun Flash", &m_bShowHUDStatusFlash);
    ImGui::SliderFloat("Hit Flash Sec", &m_fHUDHitFlashDuration, 0.1f, 2.0f, "%.2f");
    ImGui::SliderFloat("Stun Flash Sec", &m_fHUDStunFlashDuration, 0.1f, 2.0f, "%.2f");
    if (ImGui::Button("Test Hit Flash"))
        m_fHUDHitFlashTimer = m_fHUDHitFlashDuration;
    ImGui::SameLine();
    if (ImGui::Button("Test Stun Flash"))
        m_fHUDStunFlashTimer = m_fHUDStunFlashDuration;
```

HUD frame, portrait, passive, 상태창, damage number, HP trail/bar는 유지한다.

1-10. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp

삭제할 include:

```cpp
#include "GameObject/Champion/Annie/Annie_FxPresets.h"
```

`PlayStunReadyCue`, BA, Q, W, E, R visual은 WFX를 한 번 호출하고 실패 시 추가 code FX를 만들지 않도록 아래 형태로 교체한다:

```cpp
    void PlayStunReadyCue(VisualHookContext& ctx)
    {
        PlayAnnieCue(ctx,
            "Annie.Stun.Ready",
            ResolveCasterPosition(ctx),
            ResolveForward(ctx),
            ctx.casterEntity);
    }
```

```cpp
                PlayAnnieCue(ctx, "Annie.BA.Projectile", casterPos, forward, NULL_ENTITY, &endPos);
                PlayAnnieCue(ctx, "Annie.BA.Hit", targetPos, forward, target);
```

```cpp
            PlayAnnieCue(ctx, "Annie.Q.Fireball", casterPos, forward, NULL_ENTITY, &endPos);
            PlayAnnieCue(ctx, "Annie.W.Cone", casterPos, forward, NULL_ENTITY);
            PlayAnnieCue(ctx, "Annie.E.Shield", casterPos, forward, ctx.casterEntity);
            PlayAnnieCue(ctx, "Annie.R.Summon", ctx.pCommand->groundPos, forward, NULL_ENTITY);
```

각 스킬의 `AdvanceVisualStunStack` 호출은 유지한다.

1-11. Annie fallback 전용 파일과 프로젝트 항목

삭제 파일:

```text
C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Annie/Annie_FxPresets.cpp
C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Annie/Annie_FxPresets.h
```

C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj 에서 삭제:

```xml
    <ClCompile Include="..\Private\GameObject\Champion\Annie\Annie_FxPresets.cpp" />
    <ClInclude Include="..\Public\GameObject\Champion\Annie\Annie_FxPresets.h" />
```

C:/Users/user/Desktop/Winters/Client/Include/Client.vcxproj.filters 에서 해당 `ClCompile`/`ClInclude` 블록 전체를 삭제한다.

C:/Users/user/Desktop/Winters/Client/Private/GameObject/FX/FxLegacyManifest.cpp 의 `Annie.Stun.Ready` source path를 아래로 교체:

```cpp
                "Client/Private/GameObject/Champion/Annie/Annie_Skills.cpp",
```

1-12. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Jax/Jax_FxPresets.cpp

`FxBillboardComponent`, `FxSystem` include, 모든 PNG 상수, `SpawnBillboard` helper를 삭제한다. `PlayJaxCue`는 cue 호출만 수행하고 7개 public 함수는 아래와 같이 WFX wrapper로 유지한다:

```cpp
    void PlayJaxCue(CWorld& world, const char* pszCueName, EntityID attachTo,
        const Vec3& worldPos, const Vec3& forward,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr)
    {
        FxCueContext fx{};
        fx.vWorldPos = worldPos;
        fx.vForward = forward;
        fx.attachTo = attachTo;
        fx.pFxMeshRenderer = pRenderer;
        CFxCuePlayer::PlayAll(world, pszCueName, fx, nullptr);
    }
```

```cpp
    void SpawnBAHitFlash(CWorld& world, EntityID target, f32_t fLifetime)
    {
        (void)fLifetime;
        if (target == NULL_ENTITY) return;
        PlayJaxCue(world, kCueBAHit, target,
            ResolveEntityWorldPos(world, target), { 0.f, 0.f, 1.f });
    }
```

Q/W/E/R 함수도 기존 cue name, attachTo, position, forward, renderer를 그대로 넘긴 뒤 종료한다. cue 실패 뒤 texture spawn은 없다.

1-13. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Yone/Yone_FxPresets.cpp

`FxBillboardComponent`, `FxSystem`, `<cmath>`, `kFallback*`, `ResolveEntityWorldPos`, `SpawnFallbackBillboard`를 삭제한다. `PlayCue`와 6개 함수는 아래 cue-only 형태로 유지한다:

```cpp
    void PlayCue(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
    {
        CFxCuePlayer::Play(world, pszCueName, ctx);
    }
```

```cpp
void YoneFx::SpawnBasicAttackImpact(CWorld& world, EntityID target,
    const Vec3& vHitPos, const Vec3& vForward)
{
    PlayCue(world, kCueBasicAttackImpact,
        MakeCue(target, nullptr, vHitPos, vForward));
}
```

Q/W/E/R도 기존 `MakeCue` 입력을 보존해 한 번만 호출한다. fallback-only `caster`/`target` 인자는 `(void)` 처리한다.

1-14. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Yasuo/YasuoFxPresets.cpp

`FxSystem`, `FxMeshSystem`, billboard/mesh component include, 모든 `kPath*`, `HasCue`, `<cmath>`를 삭제한다. 각 함수는 WFX만 호출한다:

```cpp
EntityID YasuoFx::SpawnQStraight(CWorld& world, const Vec3& vOrigin,
    const Vec3& vForward, f32_t fSpeed, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    cue.bOverrideVelocity = true;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    return CFxCuePlayer::Play(world, kCueQSlash, cue);
}
```

```cpp
void YasuoFx::SpawnEQRing(CWorld& world, EntityID owner,
    const Vec3& vCenter, f32_t fLifetime, f32_t fRadius)
{
    (void)fRadius;
    FxCueContext ground{};
    ground.vWorldPos = vCenter;
    ground.bOverrideLifetime = true;
    ground.fLifetimeOverride = fLifetime;
    CFxCuePlayer::Play(world, kCueEQRing, ground);

    FxCueContext glow{};
    glow.attachTo = owner;
    glow.vWorldPos = ResolveEntityWorldPos(world, owner);
    glow.bOverrideLifetime = true;
    glow.fLifetimeOverride = fLifetime;
    CFxCuePlayer::Play(world, kCueEQInnerWind, glow);
}
```

Q build, tornado, wind wall, E dash, R도 기존 context/lifetime/renderer를 WFX에 넘긴다. fallback-only size/color 인자는 `(void)` 처리한다. 반환형 함수는 cue 실패 시 `NULL_ENTITY`를 그대로 반환한다.

1-15. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Irelia/Irelia_Skills.cpp

아래 helper 이름을 교체한다:

```text
SpawnWHoldCueOrLegacy      -> PlayWHoldCue
SpawnWReleaseCueOrLegacy   -> PlayWReleaseCue
SpawnTargetMarkCueOrLegacy -> PlayTargetMarkCue
SpawnRPulseCueOrLegacy     -> PlayRPulseCue
SpawnRHitCueOrLegacy       -> PlayRHitCue
SpawnEConnectCueOrLegacy   -> PlayEConnectCue
```

각 helper에서 `CFxCuePlayer` 호출 뒤 실행되는 `IreliaFx::*` fallback과 수동 E beam 생성 블록을 삭제한다. 대표 최종 형태:

```cpp
    bool_t PlayTargetMarkCue(CWorld& world, EntityID target, f32_t lifetime)
    {
        if (target == NULL_ENTITY || !world.HasComponent<TransformComponent>(target))
            return false;

        FxCueContext mark{};
        mark.attachTo = target;
        mark.vWorldPos = world.GetComponent<TransformComponent>(target).GetPosition();
        mark.bOverrideLifetime = true;
        mark.fLifetimeOverride = lifetime;
        return CFxCuePlayer::PlayAll(world, "Irelia.Target.Mark", mark, nullptr) != NULL_ENTITY;
    }
```

```cpp
    bool_t PlayEConnectCue(
        CWorld& world,
        Engine::CFxStaticMeshRenderer* pFxMeshRenderer,
        const Vec3& p1,
        const Vec3& p2,
        const IreliaTuning& t)
    {
        const Vec3 forward = WintersMath::NormalizeXZOrZero(Vec3{
            p2.x - p1.x, 0.f, p2.z - p1.z });

        FxCueContext fx{};
        fx.vWorldPos = p1;
        fx.vEndWorldPos = p2;
        fx.vForward = forward;
        fx.pFxMeshRenderer = pFxMeshRenderer;
        fx.bOverrideEndWorldPos = true;
        fx.bOverrideLifetime = true;
        fx.fLifetimeOverride = t.fEConnectLifetime;

        if (CFxCuePlayer::Play(world, "Irelia.E.Connect", fx) == NULL_ENTITY)
            return false;

        FxCueContext pop = fx;
        pop.bOverrideEndWorldPos = false;
        pop.bOverrideLifetime = false;
        pop.vWorldPos = p1;
        CFxCuePlayer::Play(world, "Irelia.E.ConnectPop", pop);
        pop.vWorldPos = p2;
        CFxCuePlayer::Play(world, "Irelia.E.ConnectPop", pop);
        return true;
    }
```

항상 실행되는 Irelia Q trail, E blade placement, R blade fan은 primary visual이므로 유지한다.

1-16. 기존 문서의 회귀 설계 표시

C:/Users/user/Desktop/Winters/Plan/S001_MINION_MOVEMENT_TARGETING_ATTACK_ANIMATION_SESSION_20260711.md 와 C:/Users/user/Desktop/Winters/Plan/S001_MINION_NAVGRID_RESULT_20260711.md 첫 줄 바로 아래에 추가:

```text
> SUPERSEDED(2026-07-11): 한 공격에서 forward 전체 clip 뒤 reverse 전체 clip을 재생하는 설계는 이중 공격 모션 회귀를 만들었으므로 S006에서 폐기했다. 현재 계약은 action sequence당 정방향 또는 역방향 traversal 정확히 1회다.
```

C:/Users/user/Desktop/Winters/.md/plan/2026-06-24_GAMEPLAY_DASH_WALL_MINIMAP_TURRET_CURSORLOCK_PLAN.md 와 C:/Users/user/Desktop/Winters/.md/plan/2026-06-24_GAMEPLAY_DASH_WALL_MINIMAP_TURRET_CURSORLOCK_RESULT.md 첫 줄 바로 아래에 추가:

```text
> SUPERSEDED(2026-07-11, minimap calibration only): NavGrid 256 정사각형을 미니맵 texture landmark footprint로 간주한 ±181.02 projection은 실제 Stage 오브젝트를 중앙 50%로 축소했다. S006의 centered measured calibration을 현재 계약으로 사용한다.
```

WFX가 참조하는 실제 PNG, `Data/LoL/FX/**/*.wfx`, damage number, HP bar/trail, skill icon, attack cursor/range는 삭제하지 않는다.

2. 검증

정적 계약 검사:

```powershell
rg -n "eMinionVisualPhase::Recover|bPendingAttack|reverseSpeed" Client/Private/Manager/Minion_Manager.cpp Client/Public/Manager/Minion_Manager.h
# 기대: 0건

rg -n "PlayAnimationByNameAdvanced\(\"attack\"" Client/Private/Manager/Minion_Manager.cpp
# 기대: attack 시작 경로 1건

rg -n "pszFallbackSpawnTexture|pszFallbackHitTexture|bUseGenericSpawnFallback|bUseGenericHitFallback|kEffectTexture|kDamageTexture|kTurretTopBeamTexture|SpawnTurretTopBeam|kPathHUDHit|kPathHUDStun|DrawHUDStatusFlash|UpdateHUDStatusTimers|CueOrLegacy|SpawnFallbackBillboard" Client Engine --glob "*.cpp" --glob "*.h"
# 기대: 이번 런타임 범위 0건. 문서/도구 manifest의 일반 fallback 용어는 제외.

rg -n "Annie_FxPresets" Client/Include Client/Private Client/Public --glob "*.vcxproj" --glob "*.filters" --glob "*.cpp" --glob "*.h"
# 기대: 0건
```

미니맵 수학 검사:

```text
world (104.5, 0) -> UV (0.5, 0.5)
Blue Nexus (22.7776, 0.9204) -> UV 약 (0.064, 0.930)
Red Nexus  (187.012, 0.102)  -> UV 약 (0.937, 0.063)
Stage 구조물+웨이포인트 전체 -> U/V 0..1 내부
UV -> World -> UV round-trip epsilon <= 1e-4
```

빌드와 diff:

```powershell
git diff --check
msbuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
Engine/UpdateLib.bat
msbuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
msbuild Server/Include/Server.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
```

인게임 30초 환전 캡처:

```text
1. melee 한 개체 로그: seq 1/2/3/4당 [MinionAnim] 1줄, direction F/R/F/R.
2. ranged 한 개체도 공격 하나에 모션 하나, 투사체 하나. HP 감소/투사체 수는 기존처럼 한 번.
3. 미니맵 Blue/Red Nexus가 좌하/우상 base 원형 중심, outer turret가 lane landmark에 위치.
4. camera box, click jump, FOW가 같은 위치를 가리킴.
5. 공격/피격 시 generic ring, physical hit card, projectile fallback, fullscreen hit/stun 이미지가 0회.
6. Annie/Jax/Yone/Yasuo/Irelia 대표 Q/BA 한 번씩 authored WFX는 보이고, 누락 시 [FxCuePlayer] Missing cue만 출력.
```

미검증 경계:

```text
WANIM에 contact event가 없으므로 시각 접촉 프레임과 서버 windup impact의 프레임 단위 일치는 이번 세션에서 보증하지 않는다. 수동 캡처에서 어긋나면 다음 세션에 per-asset normalized impact marker와 piecewise time mapping을 추가하며, 한 공격에 두 번째 reverse traversal을 다시 넣지 않는다.
Turret top-beam 제거 뒤 muzzle flash가 필요하면 Turret.Attack.Red를 authoritative attack cue에 연결하는 별도 세션으로 처리한다.
```
