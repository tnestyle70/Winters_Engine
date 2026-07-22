Session - 실패한 Fiora 이펙트 계획을 서버 성공 경계부터 다시 연결해 패시브/R/W/E를 실제 인게임 동작으로 복구한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-17_FIORA_VITAL_GRAND_CHALLENGE_E_RIPOSTE_FX_PLAN.md · 2026-07-17_FIORA_VITAL_GRAND_CHALLENGE_E_RIPOSTE_FX_RESULT.md

## 1. 결정 기록

① 문제·제약: 인게임 실측에서 패시브 방향 적중 0종, R 4표식/완료 힐존 0종, W hard-CC 반격 0종, E weapon glow/2타 crit 0종이었다. 제드 세션의 공용 7개 파일과 충돌하지 않고 4기능을 복구하며 이번 세션 빌드는 0회로 유지한다.
② 순진한 해법의 실패: cast visual에 표식·힐존·spark를 즉시 붙인 기존 방식은 실제 hit/CC 성공을 보지 않아 R 힐존이 피오라에게 즉시 뜨고 모든 BA가 E spark를 재생했다.
③ 메커니즘: `FioraSimComponent`의 passive/R bitmask·W parry·E hit ordinal → 깨끗한 `DamageQueueSystem`/`StatusEffectSystem` 실제 성공 경계 → stage가 구분된 서버 EffectTrigger → Fiora 전용 visual handle 제거와 WFX로 닫는다.
④ 대조: persistent snapshot schema/EventApplier 확장은 제드 세션과 충돌하므로 이번 복구에서는 서버 상태가 truth를 소유하고 EffectTrigger가 spawn/pop/clear를 모두 전달한다. late join 재구성은 후속 hardening으로 남긴다.
⑤ 대가: 수치는 충돌 회피를 위해 이번 슬라이스에서 3% 최대 체력 고정 피해, 패시브 8초/1.5초 재생성, R 8초, 힐존 반경 6·5초·0.5초당 40으로 고정된다. reconnect 중 지속 표식 재구성이 필요해지는 순간 snapshot row 후속 작업이 필요하다.

천장 30%는 빌드 재개 후 실제 2-client 인게임 캡처에 고정하고, 바닥 70%는 서버 상태·cue·WFX 정적 연결과 회귀 게이트에 쓴다. 이번 구현은 직전 실패 통찰을 동작 코드와 실패 RESULT로 환전하는 세션이다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/FioraSimComponent.h

`struct FioraSimComponent` 전체를 아래로 교체한다.

```cpp
struct FioraSimComponent
{
    bool_t bBladeworkActive = false;
    f32_t bladeworkTimerSec = 0.f;
    u8_t bladeworkHitsRemaining = 0;
    u8_t bladeworkPendingHitOrdinal = 0;
    f32_t bladeworkDamageBonus = 30.f;

    bool_t bRiposteActive = false;
    bool_t bRiposteCaughtHardCC = false;
    bool_t bRiposteReleasePending = false;
    u8_t riposteSkillRank = 1;
    f32_t riposteTimerSec = 0.f;
    f32_t riposteWindowSec = 0.75f;
    Vec3 riposteDirection{ 0.f, 0.f, 1.f };
    EntityID riposteReleaseTarget = NULL_ENTITY;
    f32_t riposteRange = 6.f;
    f32_t riposteRadius = 0.8f;
    f32_t riposteDamage = 80.f;
    f32_t riposteSlowDurationSec = 1.5f;
    f32_t riposteSlowMoveSpeedMul = 0.5f;
    f32_t riposteStunDurationSec = 1.5f;

    bool_t bPassiveVitalActive = false;
    u8_t passiveVitalDirection = 0;
    f32_t passiveVitalTimerSec = 0.f;
    f32_t passiveRespawnTimerSec = 0.f;
    EntityID passiveVitalTarget = NULL_ENTITY;

    bool_t bGrandChallengeActive = false;
    u8_t grandChallengeActiveMask = 0;
    u8_t grandChallengeRank = 1;
    f32_t grandChallengeTimerSec = 0.f;
    EntityID grandChallengeTarget = NULL_ENTITY;

    bool_t bGrandChallengeHealActive = false;
    f32_t grandChallengeHealTimerSec = 0.f;
    f32_t grandChallengeHealTickTimerSec = 0.f;
    Vec3 grandChallengeHealCenter{};
};
```

POD `static_assert`는 유지한다.

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Fiora/FioraGameSim.h

기존 `namespace FioraGameSim` 선언 전체를 아래로 교체한다.

```cpp
class CWorld;
struct DamageRequest;
struct DamageResult;
struct StatusEffectApplyDesc;
struct TickContext;

namespace FioraGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
    void CancelRuntime(CWorld& world, EntityID caster);
    bool_t CanCastGrandChallenge(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target);
    f32_t ConsumeBasicAttackDamage(CWorld& world, EntityID caster, f32_t baseDamage);
    bool_t PrepareDamageRequest(CWorld& world, DamageRequest& request);
    void OnDamageResolved(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        const DamageResult& result);
    bool_t TryParryCrowdControl(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc,
        const TickContext* pTickContext);
}
```

### 2-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Fiora/FioraGameSim.cpp

기존 Q dash는 유지한다. 중복 `GameplayStateQuery.h` include 하나와 routine `std::cout`는 삭제하고, 다음 exact helper/state machine을 anonymous namespace와 `FioraGameSim` namespace에 추가한다.

```cpp
constexpr f32_t kPassiveAcquireRange = 8.f;
constexpr f32_t kPassiveVitalLifetimeSec = 8.f;
constexpr f32_t kPassiveRespawnSec = 1.5f;
constexpr f32_t kVitalSideDotThreshold = 0.55f;
constexpr f32_t kVitalTargetMaxHpRatio = 0.03f;
constexpr f32_t kGrandChallengeDurationSec = 8.f;
constexpr f32_t kGrandChallengeHealDurationSec = 5.f;
constexpr f32_t kGrandChallengeHealRadius = 6.f;
constexpr f32_t kGrandChallengeHealIntervalSec = 0.5f;
constexpr f32_t kGrandChallengeHealAmount = 40.f;

u16_t BuildFioraEffectFlags(eSkillSlot slot, u8_t stage, u8_t ordinal)
{
    return static_cast<u16_t>(
        (static_cast<u16_t>(stage & 0x0fu) << 12) |
        (static_cast<u16_t>(ordinal & 0x0fu) << 8) |
        static_cast<u16_t>(slot));
}

Vec3 VitalDirection(u8_t direction)
{
    switch (direction & 3u)
    {
    case 0: return { 1.f, 0.f, 0.f };
    case 1: return { -1.f, 0.f, 0.f };
    case 2: return { 0.f, 0.f, 1.f };
    default: return { 0.f, 0.f, -1.f };
    }
}

void EmitFioraEffect(
    CWorld& world,
    const TickContext& tc,
    EntityID caster,
    EntityID target,
    u16_t variant,
    eSkillSlot slot,
    u8_t stage,
    u8_t ordinal,
    const Vec3& position,
    const Vec3& direction,
    u16_t durationMs);
void SpawnPassiveVital(CWorld& world, const TickContext& tc, EntityID caster,
    FioraSimComponent& state);
void ReleaseRiposte(CWorld& world, const TickContext& tc, EntityID caster,
    FioraSimComponent& state);
void StartGrandChallengeHeal(CWorld& world, const TickContext& tc,
    EntityID caster, EntityID target, FioraSimComponent& state);
void HealGrandChallengeAllies(CWorld& world, EntityID caster,
    const FioraSimComponent& state);
bool_t TryConsumeVital(CWorld& world, const TickContext& tc,
    const DamageRequest& request, FioraSimComponent& state);
```

`OnW`는 즉시 slow를 삭제하고 방향·수치만 저장한다. 0.75초가 끝날 때 `ReleaseRiposte`가 cone target에 W damage request를 만들며, 실제 피해 성공 뒤 `OnDamageResolved`가 slow 또는 stun을 적용한다.

```cpp
state.bRiposteActive = true;
state.bRiposteCaughtHardCC = false;
state.bRiposteReleasePending = false;
state.riposteReleaseTarget = NULL_ENTITY;
state.riposteSkillRank = ctx.skillRank;
state.riposteWindowSec = ResolveFioraSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::EffectDurationSec, 0.75f);
state.riposteTimerSec = state.riposteWindowSec;
state.riposteDirection = ResolveCommandOrFacingDirection(world, ctx);
state.riposteRange = ResolveFioraSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::Range, 6.f);
state.riposteRadius = ResolveFioraSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::Radius, 0.8f);
state.riposteDamage = ResolveFioraSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::BaseDamage, 80.f);
if (state.riposteDamage <= 0.f)
    state.riposteDamage = 80.f;
state.riposteSlowDurationSec = ResolveFioraSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::SlowDurationSec, 1.5f);
state.riposteSlowMoveSpeedMul = ResolveFioraSkillEffectParam(
    ctx, eSkillSlot::W, eSkillEffectParamId::MoveSpeedMul, 0.5f);
state.riposteStunDurationSec = 1.5f;
ClearMove(world, ctx.casterEntity);
```

`OnR`의 즉시 80 damage enqueue를 삭제하고 아래로 교체한다.

```cpp
ClearPassiveVital(world, *ctx.pTickCtx, ctx.casterEntity, state, false);
state.bGrandChallengeActive = true;
state.grandChallengeTimerSec = kGrandChallengeDurationSec;
state.grandChallengeTarget = target;
state.grandChallengeActiveMask = 0x0fu;
state.grandChallengeRank = ctx.skillRank;
state.bGrandChallengeHealActive = false;

const Vec3 targetPosition =
    world.GetComponent<TransformComponent>(target).GetPosition();
EmitFioraEffect(world, *ctx.pTickCtx, ctx.casterEntity, target,
    GameplayHookVariant::R_Recovery, eSkillSlot::R, 1u, 0u,
    targetPosition, {}, 8000u);
for (u8_t direction = 0u; direction < 4u; ++direction)
{
    EmitFioraEffect(world, *ctx.pTickCtx, ctx.casterEntity, target,
        GameplayHookVariant::R_CastFrame, eSkillSlot::R, 2u,
        static_cast<u8_t>(direction + 1u), targetPosition,
        VitalDirection(direction), 8000u);
}
```

`ConsumeBasicAttackDamage`는 E hit ordinal을 남기도록 아래 핵심으로 교체한다.

```cpp
const u8_t hitOrdinal = static_cast<u8_t>(3u - state.bladeworkHitsRemaining);
state.bladeworkPendingHitOrdinal = hitOrdinal;
--state.bladeworkHitsRemaining;
if (state.bladeworkHitsRemaining == 0)
    state.bBladeworkActive = false;
return baseDamage + state.bladeworkDamageBonus;
```

`PrepareDamageRequest`는 pending E 2타만 `StatComponent::critDamage`로 raw damage를 확정하고 ratio/CanCrit를 제거한다. `OnDamageResolved`는 actual success에서 E spark, W slow/stun, passive/R 방향 표식 pop과 3% true damage enqueue를 수행한다. `TryParryCrowdControl`은 Riposte 동안 `Stunned|Airborne`을 거부하고 최초 1회 W stage 2 cue를 발행한다.

### 2-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamageQueueSystem.cpp

기존 include 아래에 추가:

```cpp
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
```

기존 코드:

```cpp
        const DamageResult result = ApplyDamageRequest(world, tc, request);
```

아래로 교체:

```cpp
        const bool_t bFioraForcedCrit =
            FioraGameSim::PrepareDamageRequest(world, request);
        DamageResult result = ApplyDamageRequest(world, tc, request);
        if (bFioraForcedCrit && result.finalAmount > 0.f)
            result.bWasCrit = true;
        FioraGameSim::OnDamageResolved(world, tc, request, result);
```

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp

include 아래에 추가:

```cpp
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
```

두 `TryApplyStatusEffect` overload의 `ApplyStatusEffectInternal` 호출 전에 각각 추가한다.

```cpp
if (FioraGameSim::TryParryCrowdControl(world, target, desc, nullptr))
    return false;
```

```cpp
if (FioraGameSim::TryParryCrowdControl(world, target, desc, &tc))
    return false;
```

### 2-6. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Fiora/Fiora_Skills.h

`namespace Visual` 선언 전체를 아래로 교체한다.

```cpp
namespace Visual
{
    void OnCastFrame_Q_Visual(VisualHookContext& ctx);
    void OnCastFrame_W_Visual(VisualHookContext& ctx);
    void OnRecovery_W_Visual(VisualHookContext& ctx);
    void OnCastFrame_E_Visual(VisualHookContext& ctx);
    void OnRecovery_E_Visual(VisualHookContext& ctx);
    void OnCastFrame_R_Visual(VisualHookContext& ctx);
    void OnRecovery_R_Visual(VisualHookContext& ctx);
    void OnPassiveTrigger_Visual(VisualHookContext& ctx);
}
```

### 2-7. C:/Users/user/Desktop/Winters/Client/Public/GameObject/Champion/Fiora/Fiora_FxPresets.h

`namespace Fiora::Fx` 전체를 아래로 교체한다.

```cpp
namespace Fiora::Fx
{
    enum class eVitalVisualKind : u8_t
    {
        Passive,
        GrandChallenge,
    };

    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t lifetime);
    void SpawnWParryActive(CWorld& world, EntityID owner, const Vec3& dir, f32_t duration);
    void SpawnWParrySuccess(CWorld& world, EntityID owner, const Vec3& dir, f32_t duration);
    void SpawnWRelease(CWorld& world, EntityID owner, const Vec3& dir, f32_t lifetime);
    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t duration);
    void StopEBladeworkBuff(CWorld& world, EntityID owner);
    void SpawnEHitSpark(CWorld& world, EntityID target, f32_t lifetime);
    void SpawnVital(CWorld& world, EntityID owner, EntityID target,
        const Vec3& outward, eVitalVisualKind kind, f32_t duration);
    void ConsumeVital(CWorld& world, EntityID owner, EntityID target,
        const Vec3& outward, eVitalVisualKind kind);
    void ClearVitals(CWorld& world, EntityID owner, eVitalVisualKind kind);
    void SpawnRRing(CWorld& world, EntityID owner, EntityID target,
        const Vec3& center, f32_t duration);
    void ClearRPresentation(CWorld& world, EntityID owner);
    void SpawnRHealZone(CWorld& world, const Vec3& center, f32_t duration);
}
```

### 2-8. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Fiora/Fiora_FxPresets.cpp

기존 cue constants와 `namespace Fiora::Fx`를 교체해 `EntityHandle` 기반 map을 둔다. vital key는 `(owner,target,kind,cardinal direction)`이며 spawn 시 이전 handle을 파괴하고, pop/clear가 같은 handle을 파괴한다. attached billboard의 `vAttachOffset`와 `anchor.vAnchorOffset`을 `outward * gap + y`로 수정해 target이 이동해도 방향 표식이 따라가게 한다. E buff와 R ring도 owner별 handle을 보관해 2타/4표식 완료 순간 제거한다.

### 2-9. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Fiora/Fiora_Skills.cpp

기존 BA cast spark를 삭제한다. `namespace Visual` 전체를 아래 stage 계약으로 교체한다.

```cpp
void OnCastFrame_W_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    if (ctx.skillStage >= 2u)
        Fx::SpawnWParrySuccess(*ctx.pWorld, ctx.casterEntity,
            ctx.pCommand->direction, 0.75f);
    else
        Fx::SpawnWParryActive(*ctx.pWorld, ctx.casterEntity,
            ctx.pCommand->direction, 0.75f);
}

void OnRecovery_W_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    Fx::SpawnWRelease(*ctx.pWorld, ctx.casterEntity,
        ctx.pCommand->direction, 0.30f);
}

void OnRecovery_E_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    if (ctx.pCommand->targetEntityId != NULL_ENTITY)
        Fx::SpawnEHitSpark(*ctx.pWorld, ctx.pCommand->targetEntityId, 0.40f);
    if (ctx.skillStage >= 3u)
        Fx::StopEBladeworkBuff(*ctx.pWorld, ctx.casterEntity);
}

void OnCastFrame_R_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand || ctx.skillStage < 2u) return;
    Fx::SpawnVital(*ctx.pWorld, ctx.casterEntity,
        ctx.pCommand->targetEntityId, ctx.pCommand->direction,
        Fx::eVitalVisualKind::GrandChallenge, ctx.fEffectLifetimeSec);
}

void OnRecovery_R_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    if (ctx.skillStage == 1u)
        Fx::SpawnRRing(*ctx.pWorld, ctx.casterEntity,
            ctx.pCommand->targetEntityId, ctx.pCommand->groundPos,
            ctx.fEffectLifetimeSec);
    else if (ctx.skillStage == 2u)
        Fx::ConsumeVital(*ctx.pWorld, ctx.casterEntity,
            ctx.pCommand->targetEntityId, ctx.pCommand->direction,
            Fx::eVitalVisualKind::GrandChallenge);
    else if (ctx.skillStage == 3u)
    {
        Fx::ClearRPresentation(*ctx.pWorld, ctx.casterEntity);
        Fx::SpawnRHealZone(*ctx.pWorld, ctx.pCommand->groundPos,
            ctx.fEffectLifetimeSec);
    }
    else if (ctx.skillStage >= 4u)
        Fx::ClearRPresentation(*ctx.pWorld, ctx.casterEntity);
}

void OnPassiveTrigger_Visual(VisualHookContext& ctx)
{
    if (!ctx.pWorld || !ctx.pCommand) return;
    if (ctx.skillStage == 1u)
        Fx::SpawnVital(*ctx.pWorld, ctx.casterEntity,
            ctx.pCommand->targetEntityId, ctx.pCommand->direction,
            Fx::eVitalVisualKind::Passive, ctx.fEffectLifetimeSec);
    else if (ctx.skillStage == 2u)
        Fx::ConsumeVital(*ctx.pWorld, ctx.casterEntity,
            ctx.pCommand->targetEntityId, ctx.pCommand->direction,
            Fx::eVitalVisualKind::Passive);
    else
        Fx::ClearVitals(*ctx.pWorld, ctx.casterEntity,
            Fx::eVitalVisualKind::Passive);
}
```

### 2-10. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Fiora/Fiora_Registration.cpp

기존 hook constants 아래에 추가한다.

```cpp
constexpr u32_t kFio_Passive = MakeHookId(eChampion::FIORA, HookVariant::Passive_Trigger);
constexpr u32_t kFio_W_Recovery = MakeHookId(eChampion::FIORA, HookVariant::W_Recovery);
constexpr u32_t kFio_E_Recovery = MakeHookId(eChampion::FIORA, HookVariant::E_Recovery);
constexpr u32_t kFio_R_Recovery = MakeHookId(eChampion::FIORA, HookVariant::R_Recovery);
```

W `SkillDef`의 `Self/0/None`을 아래로 교체한다.

```cpp
s.targetMode = eTargetMode::Direction;
s.cooldownSec = 12.f; s.rangeMax = 6.0f; s.manaCost = 0.f;
s.rotate = eRotateMode::TowardsCursor;
```

client `CGameplayHookRegistry` 5개 등록은 삭제한다. visual 등록은 BA spark 등록을 삭제하고 아래를 추가한다.

```cpp
CVisualHookRegistry::Instance().Register(kFio_Passive, &Fiora::Visual::OnPassiveTrigger_Visual);
CVisualHookRegistry::Instance().Register(kFio_W_Recovery, &Fiora::Visual::OnRecovery_W_Visual);
CVisualHookRegistry::Instance().Register(kFio_E_Recovery, &Fiora::Visual::OnRecovery_E_Visual);
CVisualHookRegistry::Instance().Register(kFio_R_Recovery, &Fiora::Visual::OnRecovery_R_Visual);
```

### 2-11. Fiora WFX

기존 `ba_hit.wfx` cue name은 `Fiora.E.Hit`, `r_mark.wfx`는 `Fiora.R.Vital` + `fiora_base_r_timeout_alphaslice.png`, `r_heal.wfx`는 12.1×12.1 GroundDecal/5초로 교체한다. `e_buff.wfx`에는 다음 bone history ribbon을 추가한다.

```json
{
  "name": "e_weapon_history_trail",
  "render_type": "Ribbon",
  "blend_mode": "Additive",
  "depth_mode": "DepthTestWriteOff",
  "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_e_cas_trails_blue.png",
  "lifetime": 5.0,
  "fade_in": 0.04,
  "fade_out": 0.35,
  "width": 0.32,
  "ribbon_point_count": 20,
  "history_trail": true,
  "trail_sample_interval": 0.025,
  "trail_head_width_scale": 1.0,
  "trail_tail_width_scale": 0.05,
  "trail_head_alpha_scale": 0.85,
  "trail_tail_alpha_scale": 0.0,
  "uv_scroll": [0.0, -1.8],
  "color": [0.45, 1.15, 2.1, 0.85],
  "anchor": {
    "type": "Bone",
    "name": "BUFFBONE_GLB_WEAPON_1",
    "offset": [0.0, 0.0, 0.0],
    "inherit_rotation": true,
    "fallback": "Entity"
  }
}
```

새 파일 `Data/LoL/FX/Champions/Fiora/passive_vital.wfx`는 전체 내용:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.Passive.Vital",
  "emitters": [{
    "name": "passive_crest",
    "render_type": "Billboard",
    "blend_mode": "Additive",
    "depth_mode": "DepthTestWriteOff",
    "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_crest.png",
    "lifetime": 8.0,
    "fade_in": 0.08,
    "fade_out": 0.4,
    "width": 2.2,
    "height": 2.2,
    "color": [1.25, 0.92, 0.35, 1.0],
    "attach_offset": [0.0, 0.0, 0.0],
    "billboard": true
  }]
}
```

새 파일 `Data/LoL/FX/Champions/Fiora/vital_pop.wfx`는 전체 내용:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.Vital.Pop",
  "emitters": [{
    "name": "vital_hit_flash",
    "render_type": "Billboard",
    "blend_mode": "Additive",
    "depth_mode": "DepthTestWriteOff",
    "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_hit_flash.png",
    "lifetime": 0.45,
    "fade_in": 0.01,
    "fade_out": 0.30,
    "width": 2.6,
    "height": 2.6,
    "color": [1.5, 1.1, 0.45, 1.0],
    "attach_offset": [0.0, 1.2, 0.0],
    "billboard": true
  }]
}
```

새 파일 `Data/LoL/FX/Champions/Fiora/r_ring.wfx`는 전체 내용:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.R.Ring",
  "emitters": [{
    "name": "r_healzone_ring",
    "render_type": "GroundDecal",
    "blend_mode": "AlphaBlend",
    "depth_mode": "DepthTestWriteOff",
    "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_r_healzone_ring.png",
    "lifetime": 8.0,
    "fade_in": 0.10,
    "fade_out": 0.50,
    "width": 12.1,
    "height": 12.1,
    "color": [0.95, 0.75, 0.28, 0.75],
    "attach_offset": [0.0, 0.05, 0.0],
    "billboard": false
  }]
}
```

새 파일 `Data/LoL/FX/Champions/Fiora/w_parry_success.wfx`는 전체 내용:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.W.ParrySuccess",
  "emitters": [{
    "name": "w_cc_indicator_red",
    "render_type": "GroundDecal",
    "blend_mode": "Additive",
    "depth_mode": "DepthTestWriteOff",
    "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_w_indicator_red.png",
    "lifetime": 0.75,
    "fade_in": 0.01,
    "fade_out": 0.25,
    "width": 6.0,
    "height": 1.6,
    "color": [1.5, 0.25, 0.12, 0.9],
    "attach_offset": [0.0, 0.05, 0.0],
    "billboard": false
  }]
}
```

새 파일 `Data/LoL/FX/Champions/Fiora/w_release.wfx`는 전체 내용:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Fiora.W.Release",
  "emitters": [{
    "name": "w_sword_sharpflash_blue",
    "render_type": "Beam",
    "blend_mode": "Additive",
    "depth_mode": "DepthTestWriteOff",
    "texture": "Client/Bin/Resource/Texture/Character/Fiora/particles/fiora_base_w_sword_sharpflash_blue.png",
    "lifetime": 0.30,
    "fade_in": 0.01,
    "fade_out": 0.18,
    "width": 1.0,
    "color": [0.55, 0.85, 1.6, 0.95],
    "attach_offset": [0.0, 1.05, 0.2],
    "end_offset": [0.0, 1.05, 6.0],
    "uv_scroll": [0.0, -1.2],
    "billboard": false
  }]
}
```

### 2-12. C:/Users/user/Desktop/Winters/Client/Public/GamePlay/SkillHookRegistry.h 및 통합 보정

기존 `HookVariant::R_Recovery` 아래에 서버와 같은 passive hook 상수를 추가한다.

```cpp
constexpr u16_t Passive_Trigger = 0x0051;
```

`FioraGameSim.cpp`의 `EmitFioraEffect` 시작부에는 아래를 추가하고 `event.slot`, `BuildFioraEffectFlags`의 slot 인자를 `eventSlot`로 교체한다. Passive cue가 일반 BA action 재생 조건으로 오인되는 것을 막되 `skillId`의 실제 의미는 BA로 유지한다.

```cpp
const eSkillSlot eventSlot =
    variant == GameplayHookVariant::Passive_Trigger
        ? eSkillSlot::W
        : slot;
```

`EnqueuePhysicalDamage`의 `request.skillId` 대입부는 아래로 교체한다. 현재 Fiora W data damage formula가 0이므로 W release의 서버 계산 80 damage가 0으로 덮이지 않게 하고, `iSourceSlot=W`는 그대로 유지해 후속 slow/stun 판정을 보존한다.

```cpp
request.skillId = slot == eSkillSlot::W
    ? 0u
    : static_cast<u16_t>(
        (static_cast<u32_t>(eChampion::FIORA) << 8) |
        static_cast<u8_t>(slot));
```

`OnW`의 `ClearMove` 아래와 `OnE`의 state 설정 아래에는 각각 stage 1 `EffectTrigger`를 추가한다. 이 cue가 local cast visual을 서버 truth로 갱신하므로 다른 클라이언트도 yellow W indicator와 E weapon glow를 본다. `OnCastFrame_W_Visual` 및 `OnCastFrame_E_Visual`은 `fEffectLifetimeSec > 0`이면 서버 duration을 사용하고 각각 0.75초/5초를 local fallback으로 쓴다.

R heal tick loop 조건은 아래로 교체해 5초가 끝난 큰 `dt` 프레임에서 만료된 heal tick을 몰아서 적용하지 않는다.

```cpp
while (state.grandChallengeHealTickTimerSec <= 0.f &&
    state.grandChallengeHealTimerSec > 0.f)
```

## 3. 검증

예측:
- 패시브는 8m 내 가장 가까운 적 1명에 서버 RNG 방향 1개만 생성되고, 피오라가 그 방향에 있을 때 실제 BA/Q/W/E 강화 BA 피해가 성공한 경우에만 mark handle이 제거·pop되고 다음 틱 3% 최대 체력 true damage가 들어간다.
- W는 cast 동안 yellow이며 Jax E/Viego W의 `Stunned|Airborne` 적용이 거부된 최초 tick에 red가 된다. 0.75초 release가 적중하면 normal은 slow, hard CC 포착은 1.5초 stun이다.
- E cast는 `BUFFBONE_GLB_WEAPON_1` history ribbon을 만들고 실제 E 강화 BA에서만 yellow spark가 난다. 두 번째 적중은 raw damage×`critDamage`, DamageEvent `bWasCrit=true`다.
- R cast 직후 enemy target에 ring 1개와 +X/-X/+Z/-Z vital 4개가 보이고 healzone은 없다. 네 방향을 모두 실제 hit로 파괴한 tick의 enemy 위치에만 12.1 크기 healzone이 5초 생기며 같은 팀 champion만 반경 6에서 0.5초마다 40 회복한다.
- Bot AI는 GameCommand 생산자이며 게임플레이 진실을 직접 변경하지 않는다.
- 깨질 수 있는 것: EffectTrigger-only persistence는 reconnect/late join 중간 상태를 재구성하지 못한다. 이번 게이트는 normal live match이며 reconnect 게이트 없음으로 기록한다.

검증 명령:
- 이번 세션 정적: `git diff --check`
- 이번 세션 WFX JSON: `py -c "import json,glob; [json.load(open(p,encoding='utf-8')) for p in glob.glob(r'Data/LoL/FX/Champions/Fiora/*.wfx')]; print('Fiora WFX JSON OK')"`
- 빌드 재개 후 서버: `msbuild Server/Server.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- 빌드 재개 후 클라이언트: `msbuild Client/Client.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1`
- 제드 merge 뒤 SimLab에 BA/Q/W/E 방향 hit·R 4bit·Jax E/Viego W matrix를 추가하고 `msbuild Tools/SimLab/SimLab.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1` 후 실행한다.

미검증:
- 사용자 요청에 따라 이번 세션 Client/Server/SimLab build와 인게임 실행은 하지 않는다.
- reconnect/full snapshot 도중 이미 활성인 passive/R/W/E presentation 재구성.

확인 필요:
- 패시브/R 표식의 최종 화면 크기·offset은 사용자가 직접 조정할 수 있도록 WFX `width/height`와 Fiora preset gap 한 곳에 모은다.
- 제드 세션 종료 뒤 data-driven 수치와 SimLab 공용 파일을 최신 diff 기준으로 별도 merge한다.
