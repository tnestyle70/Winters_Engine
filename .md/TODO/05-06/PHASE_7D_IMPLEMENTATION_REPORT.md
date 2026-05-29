# Phase 7D Implementation Report - Skill/FX Hook Migration 1

Date: 2026-05-06

## 1. 목적

7D의 큰 목표는 `Scene_InGame`에서 챔피언별 스킬/FX 구현을 제거하고, 각 챔피언 모듈이 자신의 gameplay/visual hook을 소유하게 만드는 것이다.

이번 구현은 전체 이관을 한 번에 진행하지 않고, 먼저 실제 코드에서 발견된 중복 실행 위험을 제거하고 Yone의 gameplay/state 실행 경로를 shared hook 쪽으로 옮겼다.

## 2. 발견한 문제

`Scene_InGame`의 cast frame 흐름은 현재 다음 순서다.

```cpp
CGameplayHookRegistry::Instance().Dispatch(hookId, gameCtx);
CVisualHookRegistry::Instance().Dispatch(hookId, visualCtx);
CSkillHookRegistry::Instance().Dispatch(hookId, skillCtx);
```

이 구조에서 Fiora는 같은 hook id를 `GameplayHookRegistry`와 `CSkillHookRegistry` 양쪽에 모두 등록하고 있었다.

결과적으로 Fiora의 BA/Q/W/E/R은 cast frame에서 다음처럼 실행될 수 있었다.

```txt
Shared Gameplay hook  -> 실제 데미지/상태 변경
Client Visual hook    -> FX 생성
Legacy SkillHook      -> 실제 데미지/상태 변경 재실행
```

즉, Fiora는 pure ECS/server-sync 방향으로 이미 shared gameplay hook이 준비됐는데도 legacy client hook까지 살아 있어 중복 타격 가능성이 있었다.

## 3. 수정한 코드

파일:

```txt
Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp
```

삭제한 등록:

```cpp
CSkillHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::OnCastFrame_BA);
CSkillHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::OnCastFrame_Q);
CSkillHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::OnCastFrame_W);
CSkillHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::OnCastFrame_E);
CSkillHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::OnCastFrame_R);
```

남긴 구조:

```cpp
// Fiora gameplay is now handled by the shared hook path below.
// Registering the same cast hook into CSkillHookRegistry makes
// Scene_InGame dispatch damage twice: GameplayHook -> SkillHook.

CGameplayHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Gameplay::OnCastFrame_BA);
CGameplayHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Gameplay::OnCastFrame_Q);
CGameplayHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Gameplay::OnCastFrame_W);
CGameplayHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Gameplay::OnCastFrame_E);
CGameplayHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Gameplay::OnCastFrame_R);

CVisualHookRegistry::Instance().Register(kFio_BA_Cast, &Fiora::Visual::OnCastFrame_BA_Visual);
CVisualHookRegistry::Instance().Register(kFio_Q_Cast,  &Fiora::Visual::OnCastFrame_Q_Visual);
CVisualHookRegistry::Instance().Register(kFio_W_Cast,  &Fiora::Visual::OnCastFrame_W_Visual);
CVisualHookRegistry::Instance().Register(kFio_E_Cast,  &Fiora::Visual::OnCastFrame_E_Visual);
CVisualHookRegistry::Instance().Register(kFio_R_Cast,  &Fiora::Visual::OnCastFrame_R_Visual);
```

### Yone gameplay helper 공통화

파일:

```txt
Client/Private/GameObject/Champion/Yone/Yone_Skills.cpp
Client/Private/GameObject/Champion/Yone/Yone_Registration.cpp
```

Yone의 BA/Q/W/E/R 실제 로직을 anonymous helper로 공통화했다.

```cpp
void ApplySoulUnbound(CWorld& world, EntityID caster, const Vec3& direction)
{
    if (!world.HasComponent<YoneStateComponent>(caster))
        world.AddComponent<YoneStateComponent>(caster);

    auto& state = world.GetComponent<YoneStateComponent>(caster);
    // E active state, MeshGroupVisibility, YoneSoulRequestComponent 갱신
}
```

그 다음 기존 `SkillHookContext` 경로와 신규 `GameplayHookContext` 경로가 같은 helper를 호출하도록 맞췄다.

```cpp
void OnCastFrame_E(GameplayHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    ApplySoulUnbound(*ctx.pWorld, ctx.casterEntity,
        ctx.pCommand->direction);
}
```

마지막으로 Yone의 legacy `CSkillHookRegistry` 등록을 끊었다.

```cpp
// Yone gameplay/state mutation is handled by the shared hook path.
// Keep cast execution single-source; VisualHook stays client-only.

CGameplayHookRegistry::Instance().Register(kYon_BA_Cast, &Yone::Gameplay::OnCastFrame_BA);
CGameplayHookRegistry::Instance().Register(kYon_Q_Cast, &Yone::Gameplay::OnCastFrame_Q);
CGameplayHookRegistry::Instance().Register(kYon_W_Cast, &Yone::Gameplay::OnCastFrame_W);
CGameplayHookRegistry::Instance().Register(kYon_E_Cast, &Yone::Gameplay::OnCastFrame_E);
CGameplayHookRegistry::Instance().Register(kYon_R_Cast, &Yone::Gameplay::OnCastFrame_R);
```

## 4. 현재 상태

```txt
Fiora gameplay: Shared GameplayHook 경로
Fiora visual:   Client VisualHook 경로
Fiora legacy SkillHook: 등록 해제

Yone gameplay/state: Shared GameplayHook 경로
Yone visual:         Client VisualHook 경로
Yone legacy SkillHook: 등록 해제
```

`Fiora::OnCastFrame_* (SkillHookContext&)` 함수 본체는 아직 삭제하지 않았다. 다음 단계에서 다른 챔피언들과 함께 legacy compatibility 함수를 일괄 제거하는 편이 안전하다.

## 5. 다음 7D 단계

### 7D-3 Ezreal 완료

파일:

```txt
Client/Public/GamePlay/VisualHookRegistry.h
Client/Private/Scene/Scene_InGame.cpp
Client/Private/GameObject/Champion/Ezreal/Ezreal_Skills.cpp
Client/Private/GameObject/Champion/Ezreal/Ezreal_Registration.cpp
```

Scene hook bridge 확장:

```cpp
VisualHookContext visualCtx{};
visualCtx.pWorld = &m_World;
visualCtx.casterEntity = m_PlayerEntity;
visualCtx.pDef = &def;
visualCtx.pCommand = &cmd;
visualCtx.pKeyOut = &key;
visualCtx.pFxMeshRenderer = m_pFxMeshRenderer.get();
const bool visualKeyHandled =
    CVisualHookRegistry::Instance().Dispatch(def.keySwapHookId, visualCtx);
```

`onCastAcceptedHookId`도 shared gameplay와 client visual을 모두 호출한다.

```cpp
const bool gameplayAcceptedHandled =
    CGameplayHookRegistry::Instance().Dispatch(def.onCastAcceptedHookId, gameCtx);

CVisualHookRegistry::Instance().Dispatch(def.onCastAcceptedHookId, visualCtx);

const bool legacyAcceptedHandled =
    CSkillHookRegistry::Instance().Dispatch(def.onCastAcceptedHookId, ctx);

acceptedHandled = gameplayAcceptedHandled || legacyAcceptedHandled;
```

Ezreal 등록에서 legacy `CSkillHookRegistry` 등록은 제거했다.

```cpp
CGameplayHookRegistry::Instance().Register(kEz_Q_Cast,
    &Ezreal::Gameplay::OnCastFrame_Q);
CVisualHookRegistry::Instance().Register(kEz_Q_Cast,
    &Ezreal::Visual::OnCastFrame_Q_Visual);
```

Ezreal 스킬 본체는 다음처럼 분리했다.

```cpp
void OnCastFrame_Q(GameplayHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    ScheduleMysticShot(*ctx.pWorld, ctx.casterEntity, ctx.casterTeam,
        ctx.pCommand->direction);
}

void OnCastFrame_Q_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    SpawnMysticShotVisual(*ctx.pWorld, ctx.pFxMeshRenderer,
        ctx.casterEntity, *ctx.pCommand);
}
```

현재 Ezreal 상태:

```txt
BA/Q/W/R gameplay: Shared GameplayHook 경로
BA/Q/W/R visual:   Client VisualHook 경로
E teleport:        onCastAccepted GameplayHook 경로
E flash:           onCastAccepted VisualHook 경로
E key swap:        VisualHook keySwap 경로
legacy SkillHook:  등록 해제
```

```txt
7D-4. Irelia/Yasuo/Kalista
  - Scene_InGame fallback branch가 아직 큼
  - 목표: fallback branch를 champion module hook으로 이관
```

### 7D-4A Kalista 완료

파일:

```txt
Client/Public/GameObject/Champion/Kalista/Kalista_Skills.h
Client/Private/GameObject/Champion/Kalista/Kalista_Skills.cpp
Client/Private/GameObject/Champion/Kalista/Kalista_Registration.cpp
Client/Private/Scene/Scene_InGame.cpp
Client/Include/Client.vcxproj
Client/Include/Client.vcxproj.filters
```

신규 Kalista registration:

```cpp
constexpr u32_t kKal_BA_Cast = MakeHookId(eChampion::KALISTA, HookVariant::BA_CastFrame);
constexpr u32_t kKal_Q_Cast = MakeHookId(eChampion::KALISTA, HookVariant::Q_CastFrame);
constexpr u32_t kKal_E_OnAccept = MakeHookId(eChampion::KALISTA, HookVariant::E_OnCastAccepted);

CSkillHookRegistry::Instance().Register(kKal_BA_Cast, &Kalista::OnCastFrame_BA);
CSkillHookRegistry::Instance().Register(kKal_Q_Cast, &Kalista::OnCastFrame_Q);
CSkillHookRegistry::Instance().Register(kKal_E_OnAccept, &Kalista::OnCastAccepted_E);
```

BA/Q projectile fallback은 `Scene_InGame`에서 제거하고 `Kalista_Skills.cpp`로 이동했다.

```cpp
void OnCastFrame_Q(SkillHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand)
        return;

    const Vec3 origin = GetCasterPosition(*ctx.pWorld, ctx.casterEntity);
    const Vec3 forward = NormalizeXZ(ctx.pCommand->direction);

    SpawnSpear(*ctx.pWorld, ctx.pFxMeshRenderer,
        ctx.casterEntity, ctx.casterTeam,
        origin, forward,
        kQSpeed, kQMaxDist, kQRadius, kQDamage,
        kQFlySpearScale, kQStuckSpearScale);
}
```

E Rend trigger도 `onCastAcceptedHookId`로 이동했다.

```cpp
void OnCastAccepted_E(SkillHookContext& ctx)
{
    if (!ctx.pWorld)
        return;

    CKalistaRendSystem::TriggerExplode(*ctx.pWorld,
        ctx.casterEntity, kERendBaseDamage, kERendStackDamage);
}
```

`Scene_InGame`에는 static init dead-strip 방지 호출만 추가했다.

```cpp
extern void Kalista_KeepAlive();
Kalista_KeepAlive();
```

현재 Kalista 상태:

```txt
BA projectile: Kalista_Skills SkillHook 경로
Q projectile:  Kalista_Skills SkillHook 경로
E rend:        Kalista_Skills onCastAccepted SkillHook 경로
Passive dash:  아직 Scene_InGame recovery branch 유지
Tuner values:  다음 단계에서 Kalista config/component로 이관 필요
```

## 6. 검증

```txt
Client Debug ClCompile 통과
Client Debug full build 통과
```
