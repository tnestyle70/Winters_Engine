# Phase 7D-5B~5D Yasuo SkillHook 반영 보고

작성일: 2026-05-06

## 1. 반영 범위

```txt
7D-5B Yasuo Q animation keySwapHook 분리
7D-5C Yasuo Q/W onCastAcceptedHook 분리
7D-5D Yasuo E/R onCastAcceptedHook 분리
```

목표는 `Scene_InGame`에 남아 있던 Yasuo Q/W/E/R 전용 시전 처리 코드를 챔피언 모듈 소유로 옮기는 것이다.
E/R은 로컬 dash state machine이 아직 Scene에 있으므로 `SkillHookContext` callback으로 실행만 위임한다.

---

## 2. 등록 코드

파일:

```txt
Client/Private/GameObject/Champion/Yasuo/Yasuo_Registration.cpp
```

핵심:

```cpp
constexpr u32_t kYas_Q_KeySwap =
    MakeHookId(eChampion::YASUO, HookVariant::Q_KeySwap);
constexpr u32_t kYas_Q_OnCastAccepted =
    MakeHookId(eChampion::YASUO, HookVariant::Q_OnCastAccepted);
constexpr u32_t kYas_W_OnCastAccepted =
    MakeHookId(eChampion::YASUO, HookVariant::W_OnCastAccepted);
```

SkillDef 연결:

```cpp
s.keySwapHookId = kYas_Q_KeySwap;
s.onCastAcceptedHookId = kYas_Q_OnCastAccepted;

s.onCastAcceptedHookId = kYas_W_OnCastAccepted;
```

Hook registry 연결:

```cpp
CVisualHookRegistry::Instance().Register(
    kYas_Q_KeySwap, &Yasuo::Visual::OnKeySwap_Q);

CSkillHookRegistry::Instance().Register(
    kYas_Q_OnCastAccepted, &Yasuo::OnCastAccepted_Q);
CSkillHookRegistry::Instance().Register(
    kYas_W_OnCastAccepted, &Yasuo::OnCastAccepted_W);
CSkillHookRegistry::Instance().Register(
    kYas_E_OnCastAccepted, &Yasuo::OnCastAccepted_E);
CSkillHookRegistry::Instance().Register(
    kYas_R_OnCastAccepted, &Yasuo::OnCastAccepted_R);
```

---

## 3. Yasuo Q 처리

파일:

```txt
Client/Private/GameObject/Champion/Yasuo/Yasuo_Skills.cpp
```

상태:

```txt
YasuoStateComponent.bEActive == true   -> EQ ring
YasuoStateComponent.qStackCount >= 2   -> tornado
그 외                                  -> straight Q + stack 증가
```

핵심:

```cpp
void OnCastAccepted_Q(SkillHookContext& ctx)
{
    CWorld& world = *ctx.pWorld;
    auto& ys = world.GetComponent<YasuoStateComponent>(ctx.casterEntity);
    const YasuoTuning& tuning = GetTuning();
    const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
    const Vec3 forward = ResolveCasterForward(world, ctx.casterEntity, ctx.pCommand);

    if (ys.bEActive)
    {
        YasuoFx::SpawnEQRing(world, ctx.casterEntity, origin, 0.6f, tuning.eqRadius);
        CPendingHitSystem::Schedule(world, ctx.casterEntity, ctx.casterTeam,
            forward, tuning.eqDelay, eProjectileKind::EQRing,
            0.f, 0.f, tuning.eqRadius, tuning.eqDamage, 0.f);
    }
    else if (ys.qStackCount >= 2)
    {
        YasuoFx::SpawnQTornado(world, ctx.pFxMeshRenderer,
            origin, forward, tuning.qTornadoSpeed, tuning.qTornadoLifetime,
            tuning.qTornadoScale, tuning.qTornadoColor);
        ys.qStackCount = 0;
        ys.qStackTimer = 0.f;
    }
    else
    {
        YasuoFx::SpawnQStraight(world, origin, forward, tuning.qSpeed, tuning.qLifetime);
        YasuoFx::SpawnQBuildUp(world, ctx.casterEntity, 0.3f);
        ys.qStackCount += 1;
        ys.qStackTimer = 6.f;
    }
}
```

---

## 4. Yasuo W 처리

```cpp
void OnCastAccepted_W(SkillHookContext& ctx)
{
    CWorld& world = *ctx.pWorld;
    const YasuoTuning& tuning = GetTuning();
    const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
    const Vec3 forward = ResolveCasterForward(world, ctx.casterEntity, ctx.pCommand);

    YasuoFx::SpawnWWindWall(world, ctx.pFxMeshRenderer,
        origin, forward, tuning.wLifetime, tuning.wWidth, tuning.wHeight,
        tuning.wMeshScale);
    CWindWallSystem::Spawn(world,
        origin, forward, tuning.wLifetime, tuning.wWidth, tuning.wHeight,
        ctx.casterEntity, eTeam::Neutral);
}
```

---

## 5. Scene_InGame 변경

기존:

```txt
ApplyLocalPrediction
  -> if champ == YASUO
    -> Q / W / E / R 모두 직접 처리
```

변경:

```txt
ApplyLocalPrediction
  -> CSkillHookRegistry::Dispatch(def.onCastAcceptedHookId, ctx)
  -> Q/W/E/R은 Yasuo_Skills.cpp에서 처리되어 acceptedHandled=true
  -> Scene fallback의 champ == YASUO branch 제거
```

Scene이 hook에 제공하는 callback:

```cpp
ctx.startTargetDash = [this](EntityID target)
    {
        StartYasuoDash(target);
    };
ctx.startUltimateDash = [this](EntityID target)
    {
        StartYasuoRDash(target);
    };
ctx.findAirborneTarget = [this](const Vec3& origin, f32_t radius) -> EntityID
    {
        return FindAirborneEnemyNear(origin, radius);
    };
```

Yasuo E:

```cpp
void OnCastAccepted_E(SkillHookContext& ctx)
{
    auto& ys = ctx.pWorld->GetComponent<YasuoStateComponent>(ctx.casterEntity);
    ys.bEActive = true;
    ys.eActiveTimer = 0.5f;

    if (ctx.startTargetDash)
        ctx.startTargetDash(ctx.pCommand->targetEntityId);

    YasuoFx::SpawnEDashTrail(*ctx.pWorld, ctx.casterEntity, GetTuning().eDashDuration);
}
```

Yasuo R:

```cpp
void OnCastAccepted_R(SkillHookContext& ctx)
{
    const Vec3 origin = ResolveCasterPosition(*ctx.pWorld, ctx.casterEntity);
    const EntityID airborne = ctx.findAirborneTarget(origin, GetTuning().rSearchRadius);
    if (airborne == NULL_ENTITY)
        return;

    if (ctx.startUltimateDash)
        ctx.startUltimateDash(airborne);

    const Vec3 vLandPos =
        ctx.pWorld->GetComponent<TransformComponent>(airborne).m_LocalPosition;
    YasuoFx::SpawnRLastBreath(*ctx.pWorld, ctx.pFxMeshRenderer,
        vLandPos, ctx.casterEntity, GetTuning().rSequenceDuration);
}
```

---

## 6. 다음 단계

```txt
7D-6 Irelia Q/R fallback branch 분리
  - Q: dash/mark reset callback 필요
  - R: UltWave spawn + hit path 분리
```
