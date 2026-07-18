Session - 제드 패시브의 50% 체력 조건·패시브 BA 애니메이션·잃은 체력 추가 피해와 E/R 지정 WMesh 이펙트를 기존 서버 권위 피해·cue 경로에 결속한다.
좌표: 신규 좌표 후보 · 축: C7 · C8
관련: 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_PLAN.md · 2026-07-16_GAMEPLAY_FORMULA_SKILL_ANIMATION_DATA_DRIVEN_REMEDIATION_RESULT.md

## 1. 결정 기록

① 문제·제약: 제드 E는 이미 반경 2.75 서버 피해가 있으나 billboard이고, BA에는 50% 조건·패시브 stage·추가 피해가 없으며, R은 3초 뒤 잃은 체력 30% 피해만 있고 lethal marker 판정·해제 cue가 없다. 지정 WMesh 4개와 패시브 애니메이션은 이미 cooked resource로 존재한다.
② 순진한 해법의 실패: 클라이언트 HP로 패시브/R lethal을 판정하면 snapshot 지연·방어력·관통·보호막·킨드레드 체력 하한 때문에 거짓 표시가 생긴다. E 피해를 새로 만들면 이미 있는 본체/그림자 중복 제거와 피해가 이중 적용된다.
③ 메커니즘: ServerPrivate JSON 조건/계수 → BA 시작 stage 2 flag → impact의 별도 무치명타·무흡혈 추가 피해 → EffectTrigger 1회 → ClientPublic stage 2 애니메이션/WFX로 고정한다. R은 비변형 DamagePipeline preview가 lethal 전이를 감지해 stage 4 show/stage 5 hide를 보내고 stage 2 pop에서 반드시 제거한다.
④ 대조군: 신규 GameplayAbility·별도 클라이언트 전투 모델 대신 기존 `GameCommand → CombatAction → DamageRequest → ReplicatedEvent → VisualHook → WFX`를 확장한다. E gameplay는 현행 `ZedGameSim::OnE`를 대조군으로 보존한다.
⑤ 대가: 패시브 기본값은 명세에 수치가 없어 `잃은 체력 10%`로 두며 JSON/Champion Tuner에서 즉시 조절한다. 대상별 재사용 대기시간은 요청에 없으므로 추가하지 않는다. R marker는 현재 서버 상태의 kill preview이며 미래 입력 자체를 예언하지는 않지만 heal/shield 상태 전이마다 show/hide를 갱신한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json

기존 코드:

```json
    {
      "key": "skill.zed.basic_attack",
      "params": {},
      "damage": {
```

아래로 교체:

```json
    {
      "key": "skill.zed.basic_attack",
      "params": {
        "missingHealthDamageRatio": 0.1,
        "targetHealthThresholdRatio": 0.5
      },
      "damage": {
```

### 2-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/SkillAtomData.h

기존 enum tail:

```cpp
    RectLengthPerRank,
    FormationDelaySec,
    DamagePerSpear,
};
```

아래로 교체해 기존 param ID 숫자를 보존한다:

```cpp
    RectLengthPerRank,
    FormationDelaySec,
    DamagePerSpear,
    TargetHealthThresholdRatio,
};
```

### 2-3. C:/Users/user/Desktop/Winters/Tools/LoLData/Build-LoLDefinitionPack.py

기존 코드:

```python
    "vanishDurationSec": "VanishDurationSec",
    "missingHealthDamageRatio": "MissingHealthDamageRatio",
    "minHealthAmount": "MinHealthAmount",
```

아래로 교체:

```python
    "vanishDurationSec": "VanishDurationSec",
    "missingHealthDamageRatio": "MissingHealthDamageRatio",
    "targetHealthThresholdRatio": "TargetHealthThresholdRatio",
    "minHealthAmount": "MinHealthAmount",
```

### 2-4. C:/Users/user/Desktop/Winters/Client/Private/UI/ChampionTuner.cpp

기존 코드:

```cpp
    constexpr std::array<ParamOption, 55> kParamOptions =
```

아래로 교체:

```cpp
    constexpr std::array<ParamOption, 56> kParamOptions =
```

기존 코드:

```cpp
        ParamOption{ "MissingHealthDamageRatio", eSkillEffectParamId::MissingHealthDamageRatio },
        ParamOption{ "MinHealthAmount", eSkillEffectParamId::MinHealthAmount },
```

아래로 교체:

```cpp
        ParamOption{ "MissingHealthDamageRatio", eSkillEffectParamId::MissingHealthDamageRatio },
        ParamOption{ "TargetHealthThresholdRatio", eSkillEffectParamId::TargetHealthThresholdRatio },
        ParamOption{ "MinHealthAmount", eSkillEffectParamId::MinHealthAmount },
```

### 2-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/CombatActionComponent.h

기존 코드:

```cpp
    constexpr u16_t JaxEmpower = 0x0001u;
    constexpr u16_t SylasPassive = 0x0002u;
```

아래로 교체:

```cpp
    constexpr u16_t JaxEmpower = 0x0001u;
    constexpr u16_t SylasPassive = 0x0002u;
    constexpr u16_t ZedPassive = 0x0004u;
```

### 2-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Zed/ZedGameSim.h

기존 코드:

```cpp
	bool_t CanCastDeathMark(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);
```

아래로 교체:

```cpp
	bool_t CanTriggerPassiveBasicAttack(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);
	void EnqueuePassiveBasicAttackDamage(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);
	bool_t CanCastDeathMark(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);
```

### 2-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/CommandExecutor/CommandExecutor.cpp

기존 코드:

```cpp
    const bool_t bSylasPassiveAttack =
        champion == eChampion::SYLAS &&
        SylasGameSim::TryConsumePassiveBasicAttack(world, cmd.issuerEntity);
    const ChampionBasicAttackTimingDefaults attackTiming =
```

아래로 교체:

```cpp
    const bool_t bSylasPassiveAttack =
        champion == eChampion::SYLAS &&
        SylasGameSim::TryConsumePassiveBasicAttack(world, cmd.issuerEntity);
    const bool_t bZedPassiveAttack =
        champion == eChampion::ZED &&
        ZedGameSim::CanTriggerPassiveBasicAttack(
            world,
            tc,
            cmd.issuerEntity,
            cmd.targetEntity);
    const ChampionBasicAttackTimingDefaults attackTiming =
```

기존 코드:

```cpp
    const u8_t attackActionStage = bSylasPassiveAttack ? 2u : 1u;
    action.uStage = attackActionStage;
    action.uFlags = bJaxEmpowerAttack ? CombatActionFlags::JaxEmpower : 0u;
    if (bSylasPassiveAttack)
        action.uFlags |= CombatActionFlags::SylasPassive;
```

아래로 교체:

```cpp
    const u8_t attackActionStage =
        (bSylasPassiveAttack || bZedPassiveAttack) ? 2u : 1u;
    action.uStage = attackActionStage;
    action.uFlags = bJaxEmpowerAttack ? CombatActionFlags::JaxEmpower : 0u;
    if (bSylasPassiveAttack)
        action.uFlags |= CombatActionFlags::SylasPassive;
    if (bZedPassiveAttack)
        action.uFlags |= CombatActionFlags::ZedPassive;
```

### 2-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

include 목록의 `KindredGameSim.h` 아래에 추가:

```cpp
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"
```

기존 코드:

```cpp
        if (!bProjectileImpactDeferred)
        {
            EnqueueDamageRequest(world, request);
        }

        if (bProjectileImpactDeferred && resolvedChampion == eChampion::ASHE)
```

아래로 교체:

```cpp
        if (!bProjectileImpactDeferred)
        {
            EnqueueDamageRequest(world, request);
            if ((action.uFlags & CombatActionFlags::ZedPassive) != 0u)
            {
                ZedGameSim::EnqueuePassiveBasicAttackDamage(
                    world,
                    tc,
                    source,
                    target);
            }
        }

        if (bProjectileImpactDeferred && resolvedChampion == eChampion::ASHE)
```

제드는 근접 BA이므로 projectile 지연 경로가 아니라 같은 impact tick의 두 번째 `DamageRequest`로만 추가 피해를 넣는다. 이 요청에는 crit/lifesteal/on-hit flag를 주지 않는다.

### 2-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamagePipeline.h

기존 코드:

```cpp
f32_t ApplyResistance(f32_t amount, f32_t resistance);
DamageResult ApplyDamageRequest(CWorld& world, const TickContext& tc,
    const DamageRequest& req);
```

아래로 교체:

```cpp
f32_t ApplyResistance(f32_t amount, f32_t resistance);
bool_t WouldNonCriticalDamageRequestKill(
    CWorld& world,
    const TickContext& tc,
    const DamageRequest& req);
DamageResult ApplyDamageRequest(CWorld& world, const TickContext& tc,
    const DamageRequest& req);
```

### 2-10. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Damage/DamagePipeline.cpp

`ApplyResistance` 아래에 추가:

```cpp
bool_t WouldNonCriticalDamageRequestKill(
    CWorld& world,
    const TickContext& tc,
    const DamageRequest& req)
{
    if ((req.flags & DamageFlag_CanCrit) != 0u ||
        req.target == NULL_ENTITY ||
        req.target == req.source ||
        !world.IsAlive(req.target) ||
        !world.HasComponent<HealthComponent>(req.target) ||
        world.HasComponent<ViegoSoulComponent>(req.target))
    {
        return false;
    }

    const HealthComponent& hp = world.GetComponent<HealthComponent>(req.target);
    if (hp.bIsDead || hp.fCurrent <= 0.f ||
        !GameplayStateQuery::CanReceiveDamage(world, req.source, req.target))
    {
        return false;
    }

    eTeam targetTeam = eTeam::Neutral;
    if (TryGetTeam(world, req.target, targetTeam) &&
        targetTeam == req.sourceTeam &&
        targetTeam != eTeam::Neutral)
    {
        return false;
    }

    f32_t amount = ApplyTypedResistance(
        world,
        req,
        ResolveDamageType(req),
        BuildRawDamage(world, req));

    bool_t bYasuoShieldWillActivate = false;
    f32_t genericShield = 0.f;
    if (world.HasComponent<ChampionComponent>(req.target) &&
        world.HasComponent<YasuoStateComponent>(req.target) &&
        world.GetComponent<ChampionComponent>(req.target).id == eChampion::YASUO)
    {
        const YasuoStateComponent& state =
            world.GetComponent<YasuoStateComponent>(req.target);
        bYasuoShieldWillActivate =
            amount > 0.f &&
            state.fPassiveShieldRemaining <= 0.f &&
            state.fPassiveFlowMax > 0.f &&
            state.fPassiveFlow >= state.fPassiveFlowMax;
        if (bYasuoShieldWillActivate)
            genericShield = state.fPassiveFlowMax;
    }
    if (!bYasuoShieldWillActivate &&
        world.HasComponent<ShieldComponent>(req.target))
    {
        const ShieldComponent& shield = world.GetComponent<ShieldComponent>(req.target);
        if (tc.tickIndex < shield.uExpireTick)
            genericShield = std::max(0.f, shield.fCurrent);
    }
    amount = std::max(0.f, amount - genericShield);

    if (world.HasComponent<AnnieSimComponent>(req.target))
    {
        const AnnieSimComponent& annie =
            world.GetComponent<AnnieSimComponent>(req.target);
        if (annie.fEShieldRemainingSec > 0.f)
            amount = std::max(0.f, amount - std::max(0.f, annie.fEShieldAmount));
    }

    if (ResolveActiveHealthFloor(world, req.target, hp.fMaximum) > 0.f)
        return false;
    return amount >= hp.fCurrent;
}
```

파일 include 목록에 아래를 추가:

```cpp
#include "Shared/GameSim/Components/ShieldComponent.h"
```

### 2-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/ZedSimComponent.h

기존 코드:

```cpp
struct ZedDeathMarkComponent
{
    EntityID entitySource = NULL_ENTITY;
    u8_t rank = 1;
    f32_t fRemainingSec = 0.f;
    f32_t fMissingHealthDamageRatio = 0.f;
};
```

아래로 교체:

```cpp
struct ZedDeathMarkComponent
{
    EntityID entitySource = NULL_ENTITY;
    u8_t rank = 1;
    bool_t bLethalMarkerVisible = false;
    u8_t reservedMarkerAlignment[2]{};
    f32_t fRemainingSec = 0.f;
    f32_t fMissingHealthDamageRatio = 0.f;
};
static_assert(sizeof(ZedDeathMarkComponent) == 16u);
```

### 2-12. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Zed/ZedGameSim.cpp

기존 상수 아래에 추가:

```cpp
    constexpr u8_t kZedRSourceShadowStage = 3u;
    constexpr u8_t kZedRLethalMarkerShowStage = 4u;
    constexpr u8_t kZedRLethalMarkerHideStage = 5u;
    constexpr f32_t kZedPassiveHealthThresholdFallback = 0.5f;
    constexpr f32_t kZedPassiveMissingHealthRatioFallback = 0.1f;
```

`ResolveZedSkillEffectParam(const GameplayHookContext&)` 아래에 world/tick용 overload를 추가:

```cpp
    f32_t ResolveZedSkillEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::ZED,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }
```

`namespace ZedGameSim`의 `CanCastDeathMark` 위에 추가:

```cpp
    bool_t CanTriggerPassiveBasicAttack(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (caster == NULL_ENTITY || target == NULL_ENTITY ||
            !world.IsAlive(caster) || !world.IsAlive(target) ||
            !world.HasComponent<HealthComponent>(target) ||
            world.HasComponent<StructureComponent>(target))
        {
            return false;
        }

        const HealthComponent& health = world.GetComponent<HealthComponent>(target);
        if (health.bIsDead || health.fMaximum <= 0.f || health.fCurrent <= 0.f)
            return false;

        const f32_t threshold = std::clamp(
            ResolveZedSkillEffectParam(
                world,
                tc,
                caster,
                eSkillSlot::BasicAttack,
                eSkillEffectParamId::TargetHealthThresholdRatio,
                kZedPassiveHealthThresholdFallback),
            0.f,
            1.f);
        return health.fCurrent / health.fMaximum <= threshold;
    }

    void EnqueuePassiveBasicAttackDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        const f32_t ratio = std::max(0.f, ResolveZedSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::BasicAttack,
            eSkillEffectParamId::MissingHealthDamageRatio,
            kZedPassiveMissingHealthRatioFallback));
        if (ratio <= 0.f)
            return;

        DamageRequest request{};
        request.source = caster;
        request.target = target;
        request.sourceTeam = ResolveTeam(world, caster);
        request.type = eDamageType::Physical;
        request.iSourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
        request.eSourceKind = eDamageSourceKind::BasicAttack;
        request.targetMissingHpRatioOverride = ratio;
        EnqueueDamageRequest(world, request);
    }
```

기존 `world.ForEach<ZedDeathMarkComponent>` 블록을 아래로 교체한다. stage 4의 `durationMs`는 현재 `fRemainingSec`를 millisecond로 clamp한 값이고, pop stage 2는 marker를 무조건 지우는 terminal cue다.

```cpp
        world.ForEach<ZedDeathMarkComponent>(
            std::function<void(EntityID, ZedDeathMarkComponent&)>(
                [&](EntityID entity, ZedDeathMarkComponent& mark)
                {
                    const bool_t bCanPredict =
                        mark.entitySource != NULL_ENTITY &&
                        world.IsAlive(mark.entitySource) &&
                        world.IsAlive(entity) &&
                        world.HasComponent<HealthComponent>(entity);
                    if (bCanPredict)
                    {
                        const HealthComponent& health =
                            world.GetComponent<HealthComponent>(entity);
                        const f32_t missingHealth =
                            std::max(0.f, health.fMaximum - health.fCurrent);
                        const f32_t damage =
                            missingHealth * mark.fMissingHealthDamageRatio;
                        const DamageRequest previewRequest =
                            BuildZedPhysicalDamageRequest(
                                mark.entitySource,
                                entity,
                                ResolveTeam(world, mark.entitySource),
                                damage,
                                static_cast<u8_t>(eSkillSlot::R),
                                mark.rank);
                        const bool_t bLethal =
                            WouldNonCriticalDamageRequestKill(
                                world,
                                tc,
                                previewRequest);

                        if (bLethal != mark.bLethalMarkerVisible)
                        {
                            mark.bLethalMarkerVisible = bLethal;
                            const Vec3 targetPos = ResolvePosition(world, entity);
                            const Vec3 sourcePos =
                                ResolvePosition(world, mark.entitySource);
                            const Vec3 dir = WintersMath::DirectionXZ(
                                sourcePos,
                                targetPos,
                                ResolveForward(world, mark.entitySource));
                            const f32_t durationMs = std::clamp(
                                mark.fRemainingSec * 1000.f,
                                0.f,
                                65535.f);
                            EmitZedEffect(
                                world,
                                mark.entitySource,
                                entity,
                                static_cast<u8_t>(eSkillSlot::R),
                                mark.rank,
                                bLethal
                                    ? kZedRLethalMarkerShowStage
                                    : kZedRLethalMarkerHideStage,
                                targetPos,
                                dir,
                                bLethal
                                    ? static_cast<u16_t>(durationMs)
                                    : 0u,
                                tc.tickIndex);
                        }
                    }

                    mark.fRemainingSec = std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f ||
                        mark.entitySource == NULL_ENTITY ||
                        !world.IsAlive(mark.entitySource))
                    {
                        explodingMarks.push_back(entity);
                    }
                }));
```

### 2-13. C:/Users/user/Desktop/Winters/Data/LoL/ClientPublic/Visual/ChampionVisualDefs.json

`skill.zed.basic_attack`의 기존 코드:

```json
          "stages": [
            {
              "animationPlaybackSpeed": 1.0,
              "castFrame": 6.0,
              "recoveryFrame": 14.0
            }
          ]
```

아래로 교체:

```json
          "stages": [
            {
              "animationPlaybackSpeed": 1.0,
              "castFrame": 6.0,
              "recoveryFrame": 14.0
            },
            {
              "animationPlaybackSpeed": 1.0,
              "castFrame": 6.0,
              "recoveryFrame": 14.0
            }
          ]
```

### 2-14. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Zed/Zed_Registration.cpp

기존 코드:

```cpp
                SkillDef s = *legacy;
                s.castHookId = ResolveZedCastHook(slot);
                CSkillRegistry::Instance().Add(eChampion::ZED, slot, s);
```

아래로 교체:

```cpp
                SkillDef s = *legacy;
                s.castHookId = ResolveZedCastHook(slot);
                if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
                {
                    s.stageCount = 2;
                    s.stage2TargetMode = eTargetMode::UnitTarget;
                    s.stage2AnimKey = "skinned_mesh_zed_attack_passive";
                    s.stage2LockSec = s.lockDurationSec;
                    s.stage2Rotate = s.rotate;
                    s.stage2VisualCastFrame = 6.f;
                    s.stage2VisualRecoveryFrame = 14.f;
                    s.stage2VisualPlaySpeed = 1.f;
                }
                CSkillRegistry::Instance().Add(eChampion::ZED, slot, s);
```

### 2-15. C:/Users/user/Desktop/Winters/Client/Public/GamePlay/VisualHookRegistry.h

기존 코드:

```cpp
	u8_t skillStage = 1;
	std::string* pKeyOut = nullptr;
```

아래로 교체:

```cpp
	u8_t skillStage = 1;
	f32_t fEffectLifetimeSec = 0.f;
	std::string* pKeyOut = nullptr;
```

### 2-16. C:/Users/user/Desktop/Winters/Client/Public/GamePlay/SkillHookContext.h

기존 코드:

```cpp
	u8_t skillStage = 1;
	f32_t fDeltaTime = 0.f;
```

아래로 교체:

```cpp
	u8_t skillStage = 1;
	f32_t fEffectLifetimeSec = 0.f;
	f32_t fDeltaTime = 0.f;
```

### 2-17. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
        ctx.pCommand = &command;
        ctx.skillStage = skillStage;
        ctx.pFxMeshRenderer = m_pFxMeshRenderer;
```

아래로 교체:

```cpp
        ctx.pCommand = &command;
        ctx.skillStage = skillStage;
        ctx.fEffectLifetimeSec = ev->durationMs() > 0u
            ? static_cast<f32_t>(ev->durationMs()) / 1000.f
            : 0.f;
        ctx.pFxMeshRenderer = m_pFxMeshRenderer;
```

### 2-18. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Zed/Zed_Registration.cpp

기존 코드:

```cpp
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.pKeyOut = visualCtx.pKeyOut;
```

아래로 교체:

```cpp
        skillCtx.pCommand = visualCtx.pCommand;
        skillCtx.skillStage = visualCtx.skillStage;
        skillCtx.fEffectLifetimeSec = visualCtx.fEffectLifetimeSec;
        skillCtx.pKeyOut = visualCtx.pKeyOut;
```

### 2-19. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Zed/Zed_Skills.cpp

기존 include tail:

```cpp
#include <cmath>
```

아래로 교체:

```cpp
#include <cmath>
#include <unordered_map>
#include <vector>
```

`PlayCue`의 `FxCueContext` 설정에 추가:

```cpp
        fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
```

anonymous namespace에 아래 lethal marker 추적 코드를 추가한다. stage 4는 `PlayAll` 결과 handle을 보관하고 stage 5와 stage 2 pop은 살아 있는 handle만 제거한다.

```cpp
    std::unordered_map<EntityID, std::vector<EntityHandle>> s_lethalMarkerFxByTarget;

    void ClearLethalMarkerFx(CWorld& world, EntityID target)
    {
        const auto it = s_lethalMarkerFxByTarget.find(target);
        if (it == s_lethalMarkerFxByTarget.end())
            return;

        for (const EntityHandle handle : it->second)
        {
            if (world.IsAlive(handle))
                world.DestroyEntity(handle);
        }
        s_lethalMarkerFxByTarget.erase(it);
    }

    void ShowLethalMarkerFx(
        SkillHookContext& ctx,
        EntityID target,
        const Vec3& position,
        const Vec3& forward)
    {
        if (!ctx.pWorld || target == NULL_ENTITY)
            return;

        ClearLethalMarkerFx(*ctx.pWorld, target);
        FxCueContext fx{};
        fx.vWorldPos = position;
        fx.vForward = forward;
        fx.attachTo = target;
        fx.pFxMeshRenderer = ctx.pFxMeshRenderer;
        fx.bOverrideLifetime = true;
        fx.fLifetimeOverride = ctx.fEffectLifetimeSec > 0.f
            ? ctx.fEffectLifetimeSec
            : 3.f;

        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(
            *ctx.pWorld,
            "Zed.R.LethalMarker",
            fx,
            &spawned);
        if (spawned.empty())
            return;

        std::vector<EntityHandle>& handles = s_lethalMarkerFxByTarget[target];
        handles.reserve(spawned.size());
        for (EntityID entity : spawned)
        {
            if (entity != NULL_ENTITY && ctx.pWorld->IsAlive(entity))
                handles.push_back(ctx.pWorld->GetEntityHandle(entity));
        }
        if (handles.empty())
            s_lethalMarkerFxByTarget.erase(target);
    }
```

`OnCastFrame`의 slot 분기 시작에 추가:

```cpp
        if (slot == static_cast<u8_t>(eSkillSlot::BasicAttack))
        {
            if (ctx.skillStage >= 2u && ctx.pCommand)
            {
                const EntityID target = ctx.pCommand->targetEntityId;
                const Vec3 targetPos = ResolveEntityPosition(*ctx.pWorld, target);
                PlayCue(ctx, "Zed.PassiveBA.Hit",
                    { targetPos.x, targetPos.y + 1.0f, targetPos.z },
                    forward, target, 0.45f);
            }
        }
```

R의 기존 `else if (ctx.skillStage >= 2u)` 부분을 아래 exact match로 교체한다. `>= 2`를 유지하지 않아 marker stage가 pop으로 오인되지 않게 한다.

```cpp
            else if (ctx.skillStage == 4u && target != NULL_ENTITY)
            {
                const Vec3 targetPos = ResolveEntityPosition(*ctx.pWorld, target);
                ShowLethalMarkerFx(ctx, target, targetPos, forward);
            }
            else if (ctx.skillStage == 5u)
            {
                ClearLethalMarkerFx(*ctx.pWorld, target);
            }
            else if (ctx.skillStage == 2u)
            {
                ClearLethalMarkerFx(*ctx.pWorld, target);
                const Vec3 popPos = target != NULL_ENTITY
                    ? ResolveEntityPosition(*ctx.pWorld, target)
                    : ResolveCommandPosition(ctx);
                PlayCue(ctx, "Zed.R.Pop", { popPos.x, popPos.y + 1.25f, popPos.z },
                    forward, target, 0.8f);
            }
```

### 2-20. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/passive_ba_hit.wfx

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Zed.PassiveBA.Hit",
  "emitters": [
    {
      "name": "passive_circle_timer",
      "render_type": "MeshParticle",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Zed/particles/fbx/common_circletimer.wmesh",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/render/common_circletimer.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 0.45,
      "fade_in": 0.02,
      "fade_out": 0.18,
      "scale": [0.30, 0.30, 0.30],
      "color": [0.18, 0.03, 0.04, 0.92],
      "attach_offset": [0.0, 1.10, 0.0],
      "world_yaw_spin_speed": 3.2,
      "blockable_by_wind_wall": false
    },
    {
      "name": "passive_hit_slash",
      "render_type": "Billboard",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/zed_e_hitslash.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 0.34,
      "width": 2.4,
      "height": 2.4,
      "color": [0.75, 0.08, 0.10, 0.95],
      "attach_offset": [0.0, 1.10, 0.0],
      "fade_in": 0.01,
      "fade_out": 0.16,
      "billboard": true,
      "blockable_by_wind_wall": false
    }
  ]
}
```

### 2-21. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/e_slash.wfx

파일 전체를 아래로 교체:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Zed.E.Slash",
  "emitters": [
    {
      "name": "e_black_mesh_ring",
      "render_type": "MeshParticle",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Zed/particles/fbx/zed_atkswipe.wmesh",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/render/zed_atkswipe.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 0.5,
      "fade_in": 0.02,
      "fade_out": 0.20,
      "scale": [2.75, 2.75, 2.75],
      "color": [0.025, 0.025, 0.035, 0.94],
      "attach_offset": [0.0, 0.08, 0.0],
      "world_yaw_spin_speed": 1.8,
      "blockable_by_wind_wall": false
    }
  ]
}
```

### 2-22. C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/r_mark.wfx

파일 전체를 아래로 교체:

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Zed.R.Mark",
  "emitters": [
    {
      "name": "r_red_cross_swipe",
      "render_type": "MeshParticle",
      "blend_mode": "Additive",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Zed/particles/fbx/zed_crossswipe.wmesh",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/render/zed_crossswipe.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 3.0,
      "fade_in": 0.05,
      "fade_out": 0.30,
      "scale": [2.1, 2.1, 2.1],
      "color": [1.55, 0.05, 0.08, 0.96],
      "attach_offset": [0.0, 1.20, 0.0],
      "blockable_by_wind_wall": false
    }
  ]
}
```

### 2-23. 새 파일: C:/Users/user/Desktop/Winters/Data/LoL/FX/Champions/Zed/r_lethal_marker.wfx

```json
{
  "schema": "WintersWfx",
  "version": 1,
  "name": "Zed.R.LethalMarker",
  "emitters": [
    {
      "name": "r_lethal_head_marker",
      "render_type": "MeshParticle",
      "blend_mode": "AlphaBlend",
      "depth_mode": "DepthTestWriteOff",
      "model": "Client/Bin/Resource/Texture/Character/Zed/particles/fbx/zed_base_r_hit_marker.wmesh",
      "texture": "Client/Bin/Resource/Texture/Character/Zed/particles/render/zed_base_r_hit_marker.png",
      "max_particles": 1,
      "spawn_rate": 0.0,
      "lifetime": 3.0,
      "fade_in": 0.05,
      "fade_out": 0.12,
      "scale": [0.045, 0.045, 0.045],
      "color": [1.0, 0.05, 0.08, 0.98],
      "attach_offset": [0.0, 2.50, 0.0],
      "world_yaw_spin_speed": 4.2,
      "blockable_by_wind_wall": false
    }
  ]
}
```

### 2-24. C:/Users/user/Desktop/Winters/Tools/SimLab/main.cpp

`RunSylasPassiveBasicAttackProbe` 다음에 `RunZedPassiveDeathMarkProbe`를 추가한다. probe는 다음을 한 함수에서 검증한다.

```text
1. 대상 HP 50.1%: BA stage 1, ZedPassive flag 없음.
2. 대상 HP 50.0%: BA stage 2, ZedPassive flag 있음, impact에 일반 BA 1건 + 잃은 체력 10% 추가 요청 1건, stage 2 cue 1건.
3. R preview: 방어력 적용 후 lethal true, 유효 ShieldComponent/Annie E/Yasuo ready shield/Kindred health floor에서 false.
4. ZedGameSim::Tick: lethal 전이 stage 4, 보호막 전이 stage 5, 제거 후 stage 4 재표시, pop stage 2.
```

`--zed-passive-r-only` CLI와 기본 전체 실행 pass aggregate에 probe를 연결한다. Bot AI는 계속 `GameCommand` 생산자이며 HP·피해·marker truth를 직접 변경하지 않는다.

### 2-25. 생성 산출물

`SkillGameplayDefs.json`, `Shared/Private/Data/LoLGameplayDefinitionPack.cpp`, `Client/Private/Data/LoLVisualDefinitionPack.cpp` 등 생성 산출물은 직접 편집하지 않는다. canonical JSON과 param map 변경 뒤 아래 codegen으로 함께 갱신한다.

## 3. 검증과 완료 경계

예측:

- 50.1% HP의 대상 BA는 기존 `zed_attack1`, stage 1, 추가 피해 0이다.
- 정확히 50.0% 이하 대상 BA는 `skinned_mesh_zed_attack_passive`, stage 2 EffectTrigger 정확히 1회, 일반 BA와 분리된 `현재 잃은 체력 × 0.10` 물리 피해를 낸다. 추가분은 치명타·흡혈·아이템 on-hit을 다시 발동하지 않는다.
- E의 본체/그림자 적중 집합과 피해 수치는 바뀌지 않고, 각각 반경 2.75의 검은 `zed_atkswipe.wmesh`가 한 번 보인다.
- R cast에는 붉은 `zed_crossswipe.wmesh`가 대상에 3초 붙는다. 현재 pop 피해가 방어력·관통과 방어 상태까지 적용한 뒤 lethal이면 머리 위 marker가 회전하고, heal/shield로 false가 되면 즉시 사라지며 pop stage 2에서도 반드시 제거된다.
- 잘못된 결과: stage 4가 R pop으로 재생됨, marker가 pop 뒤 남음, E damage가 두 번 적용됨, WMesh cue가 renderer pointer 누락으로 spawn 0건, 50% 초과 대상이 passive stage 2가 됨.

검증 명령:

```powershell
python Tools/LoLData/Build-LoLDefinitionPack.py --root .
python Tools/LoLData/Build-LoLDefinitionPack.py --root . --check
Client/Bin/Debug/SimLab.exe --zed-passive-r-only
git diff --check -- Data/LoL/ServerPrivate/Gameplay/SkillEffectGameplayDefs.json Shared/GameSim Client Data/LoL/FX/Champions/Zed Tools/LoLData Tools/SimLab/main.cpp
```

미검증:

- 사용자가 Client/Server를 실행 중이므로 이번 세션에서는 exe 실행·build·SimLab 실행을 하지 않는다.
- WMesh의 실제 카메라 투영 크기·머리 높이·검은색 alpha는 사용자의 인게임 눈검증 대상이다.

확인 필요:

- 패시브 `잃은 체력 10%`는 요청에 계수가 없어 둔 튜닝 기본값이다. 원하는 최종 계수가 있으면 JSON 한 값으로 바꾼다.
- `zed_atkswipe` 반경은 cooked bounds radius 1.0293을 기준으로 scale 2.75, `zed_base_r_hit_marker`는 bounds radius 32.15327을 기준으로 scale 0.045를 예측했다. 인게임 캡처에서만 최종 미세 조정한다.

30% ceiling과 외부 마감:

- 구현·정합성 70%, 실제 눈검증 30%로 고정한다. ceiling 산출물은 동일 대상에 `50.1% BA / 50.0% BA / E / R non-lethal / R lethal` 다섯 장의 캡처와 SimLab 한 줄 결과다.
- 외부 마감은 2026-07-18로 제안한다. 인프라 추가 조사보다 위 다섯 장을 먼저 결과물로 환전한다.
