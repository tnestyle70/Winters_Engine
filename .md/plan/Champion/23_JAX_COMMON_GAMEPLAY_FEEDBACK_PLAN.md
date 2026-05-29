# Jax Common Gameplay Feedback Plan

Session - 상태 이상/회피 텍스트를 잭스 전용 cue가 아닌 공용 GameplayFeedback 서버 이벤트로 연결한다.

## Direction

- 서버 권위 흐름은 `Client Input -> GameCommand -> Server GameSim -> Snapshot/Event -> Client Visual`을 유지한다.
- 잭스가 텍스트 id를 소유하지 않는다.
- 서버 GameSim은 판정 결과만 공용 GameplayFeedback 이벤트로 발행한다.
- 클라이언트 `EventApplier`가 공용 feedback kind를 UI 문자열/색상으로 매핑하고 `UI_Manager` Font world text를 띄운다.
- Jax E, Yasuo airborne, Irelia stun 모두 같은 공용 world text 경로를 탄다.
- Runtime Jax animation 이상 없음은 수동 검증된 상태로 둔다.

## Plan-Rules Snapshot

```text
Session - 상태 이상/회피 텍스트를 잭스 전용 cue가 아닌 공용 GameplayFeedback 서버 이벤트로 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shared/GameSim/Feedback/GameplayFeedback.h

새 파일:
```

```cpp
#pragma once

#include "WintersTypes.h"

namespace GameplayFeedback
{
    enum class WorldTextFeedbackKind : u16_t
    {
        None = 0,
        Dodge = 1,
        Stun = 2,
        Airborne = 3,
    };

    inline constexpr u32_t kWorldTextEffectBase = 0x7f000000u;
    inline constexpr u32_t kWorldTextEffectMask = 0xffff0000u;

    inline constexpr u32_t BuildWorldTextEffectId(WorldTextFeedbackKind kind)
    {
        return kWorldTextEffectBase | static_cast<u32_t>(kind);
    }

    inline bool_t TryResolveWorldTextEffectId(
        u32_t effectId,
        WorldTextFeedbackKind& outKind)
    {
        if ((effectId & kWorldTextEffectMask) != kWorldTextEffectBase)
        {
            outKind = WorldTextFeedbackKind::None;
            return false;
        }

        const WorldTextFeedbackKind kind =
            static_cast<WorldTextFeedbackKind>(effectId & 0xffffu);
        switch (kind)
        {
        case WorldTextFeedbackKind::Dodge:
        case WorldTextFeedbackKind::Stun:
        case WorldTextFeedbackKind::Airborne:
            outKind = kind;
            return true;
        default:
            outKind = WorldTextFeedbackKind::None;
            return false;
        }
    }
}
```

```text
1-2. C:/Users/user/Desktop/Winters/Shared/GameSim/Feedback/GameplayFeedbackQueue.h

새 파일:
```

```cpp
#pragma once

#include "Shared/GameSim/Feedback/GameplayFeedback.h"

#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"

namespace GameplayFeedback
{
    inline Vec3 ResolveWorldTextPosition(
        CWorld& world,
        EntityID source,
        EntityID target)
    {
        if (target != NULL_ENTITY && world.HasComponent<TransformComponent>(target))
            return world.GetComponent<TransformComponent>(target).GetPosition();
        if (source != NULL_ENTITY && world.HasComponent<TransformComponent>(source))
            return world.GetComponent<TransformComponent>(source).GetPosition();
        return {};
    }

    inline EntityID EnqueueWorldTextFeedback(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        WorldTextFeedbackKind kind,
        u16_t durationMs = 700)
    {
        if (kind == WorldTextFeedbackKind::None)
            return NULL_ENTITY;

        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = source;
        event.targetEntity = target;
        event.effectId = BuildWorldTextEffectId(kind);
        event.durationMs = durationMs;
        event.startTick = tc.tickIndex;
        event.position = ResolveWorldTextPosition(world, source, target);

        return EnqueueReplicatedEvent(world, event);
    }
}
```

```text
1-3. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h

기존 코드:
```

```cpp
#include "ECS/Components/GameplayComponents.h"
```

```text
아래에 추가:
```

```cpp
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
```

```text
기존 코드:
```

```cpp
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
```

```text
아래로 교체:
```

```cpp
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	bool_t ApplyStatusEffectWithFeedback(CWorld& world, const TickContext& tc,
		EntityID target, const StatusEffectApplyDesc& desc,
		GameplayFeedback::WorldTextFeedbackKind feedbackKind);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
```

```text
1-4. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp

기존 코드:
```

```cpp
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Core/World/World.h"
```

```text
아래로 교체:
```

```cpp
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Core/World/World.h"
```

```text
기존 코드:
```

```cpp
    void PushUnique(std::vector<EntityID>& entities, EntityID entity)
    {
        if (entity == NULL_ENTITY)
            return;
        if (std::find(entities.begin(), entities.end(), entity) == entities.end())
            entities.push_back(entity);
    }
}
```

```text
아래로 교체:
```

```cpp
    void PushUnique(std::vector<EntityID>& entities, EntityID entity)
    {
        if (entity == NULL_ENTITY)
            return;
        if (std::find(entities.begin(), entities.end(), entity) == entities.end())
            entities.push_back(entity);
    }

    bool_t TryApplyStatusEffect(CWorld& world, EntityID target, const StatusEffectApplyDesc& desc)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            desc.effectId == eStatusEffectId::None ||
            desc.fDurationSec <= 0.f)
        {
            return false;
        }

        StatusEffectComponent& effects = EnsureStatusEffects(world, target);
        UpsertEffect(effects, desc);
        GameplayStatus::RebuildGameplayState(world, target);
        return true;
    }
}
```

```text
기존 코드:
```

```cpp
    void ApplyStatusEffect(CWorld& world, EntityID target, const StatusEffectApplyDesc& desc)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            desc.effectId == eStatusEffectId::None ||
            desc.fDurationSec <= 0.f)
        {
            return;
        }

        StatusEffectComponent& effects = EnsureStatusEffects(world, target);
        UpsertEffect(effects, desc);
        RebuildGameplayState(world, target);
    }

    void TickStatusEffects(CWorld& world, const TickContext& tc)
```

```text
아래로 교체:
```

```cpp
    void ApplyStatusEffect(CWorld& world, EntityID target, const StatusEffectApplyDesc& desc)
    {
        (void)TryApplyStatusEffect(world, target, desc);
    }

    bool_t ApplyStatusEffectWithFeedback(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        const StatusEffectApplyDesc& desc,
        GameplayFeedback::WorldTextFeedbackKind feedbackKind)
    {
        if (!TryApplyStatusEffect(world, target, desc))
            return false;

        GameplayFeedback::EnqueueWorldTextFeedback(
            world,
            tc,
            desc.sourceEntity,
            target,
            feedbackKind);
        return true;
    }

    void TickStatusEffects(CWorld& world, const TickContext& tc)
```

```text
1-5. C:/Users/user/Desktop/Winters/Shared/GameSim/Components/JaxSimComponent.h

기존 코드:
```

```cpp
    f32_t counterRadius = 2.2f;
    f32_t counterDamage = 60.f;

    bool_t bUltActive = false;
```

```text
아래로 교체:
```

```cpp
    f32_t counterRadius = 2.2f;
    f32_t counterDamage = 60.f;
    f32_t counterStunDurationSec = 1.f;

    bool_t bUltActive = false;
```

```text
1-6. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.h

기존 코드:
```

```cpp
    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        f32_t baseDamage);
}
```

```text
아래로 교체:
```

```cpp
    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        f32_t baseDamage);

    bool_t TryDodgeBasicAttack(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target);
}
```

```text
1-7. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.cpp

기존 코드:
```

```cpp
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

```text
아래로 교체:
```

```cpp
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

```text
기존 코드:
```

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

```text
아래로 교체:
```

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
```

```text
기존 코드:
```

```cpp
    void EnqueueCircleDamage(
        CWorld& world,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& origin,
        f32_t radius,
        f32_t amount,
        u8_t slot)
    {
        const f32_t radiusSq = radius * radius;
        std::vector<EntityID> targets;
        targets.reserve(8);

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == source || champion.team == sourceTeam)
                        return;

                    const Vec3 pos = transform.GetPosition();
                    const f32_t dx = pos.x - origin.x;
                    const f32_t dz = pos.z - origin.z;
                    if (dx * dx + dz * dz <= radiusSq)
                        targets.push_back(entity);
                }));

        for (EntityID target : targets)
            EnqueuePhysicalDamage(world, source, target, sourceTeam, amount, slot, 1);
    }
```

```text
아래로 교체:
```

```cpp
    void EnqueueMagicDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        f32_t amount,
        u8_t slot,
        u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Magic;
        request.flatAmount = amount;
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::JAX) << 8) | slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    std::vector<EntityID> CollectEnemyChampionsInRadius(
        CWorld& world,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& origin,
        f32_t radius)
    {
        const f32_t radiusSq = radius * radius;
        std::vector<EntityID> targets;
        targets.reserve(8);

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == source || champion.team == sourceTeam)
                        return;

                    const Vec3 pos = transform.GetPosition();
                    const f32_t dx = pos.x - origin.x;
                    const f32_t dz = pos.z - origin.z;
                    if (dx * dx + dz * dz <= radiusSq)
                        targets.push_back(entity);
                }));

        return targets;
    }

    void ApplyCounterStrikeStun(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        f32_t durationSec)
    {
        StatusEffectApplyDesc stun{};
        stun.effectId = eStatusEffectId::JaxCounterStrike;
        stun.stackPolicy = eStatusStackPolicy::RefreshDuration;
        stun.sourceEntity = source;
        stun.stackGroup =
            static_cast<u16_t>((static_cast<u32_t>(eChampion::JAX) << 8) |
                static_cast<u8_t>(eSkillSlot::E));
        stun.stateFlags =
            kGameplayStateStunnedFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        stun.fDurationSec = durationSec;

        GameplayStatus::ApplyStatusEffectWithFeedback(
            world,
            tc,
            target,
            stun,
            GameplayFeedback::WorldTextFeedbackKind::Stun);
    }

    void EndCounterStrike(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        JaxSimComponent& state)
    {
        if (!state.bCounterStrikeActive)
            return;

        state.bCounterStrikeActive = false;
        state.counterTimerSec = 0.f;

        if (!world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<ChampionComponent>(caster))
            return;

        const auto& champion = world.GetComponent<ChampionComponent>(caster);
        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        const auto targets = CollectEnemyChampionsInRadius(
            world,
            caster,
            champion.team,
            origin,
            state.counterRadius);

        for (EntityID target : targets)
        {
            EnqueuePhysicalDamage(
                world,
                caster,
                target,
                champion.team,
                state.counterDamage,
                static_cast<u8_t>(eSkillSlot::E),
                1);

            ApplyCounterStrikeStun(
                world,
                tc,
                caster,
                target,
                state.counterStunDurationSec);
        }
    }
```

```text
기존 코드:
```

```cpp
    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        JaxSimComponent& state = EnsureJaxState(*ctx.pWorld, ctx.casterEntity);
        state.bCounterStrikeActive = true;
        state.counterTimerSec = state.counterDurationSec;
        ClearMove(*ctx.pWorld, ctx.casterEntity);
        std::cout << "[JaxSim] E counter start caster=" << ctx.casterEntity << "\n";
    }
```

```text
아래로 교체:
```

```cpp
    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        CWorld& world = *ctx.pWorld;
        JaxSimComponent& state = EnsureJaxState(world, ctx.casterEntity);
        const bool_t bEndRequest = ctx.pCommand && ctx.pCommand->itemId == 2u;

        if (bEndRequest)
        {
            if (ctx.pTickCtx)
                EndCounterStrike(world, *ctx.pTickCtx, ctx.casterEntity, state);
            else
            {
                state.bCounterStrikeActive = false;
                state.counterTimerSec = 0.f;
            }
            return;
        }

        state.bCounterStrikeActive = true;
        state.counterTimerSec = state.counterDurationSec;
        ClearMove(world, ctx.casterEntity);
        std::cout << "[JaxSim] E counter start caster=" << ctx.casterEntity << "\n";
    }
```

```text
기존 코드:
```

```cpp
                    if (state.bCounterStrikeActive)
                    {
                        state.counterTimerSec = std::max(0.f, state.counterTimerSec - tc.fDt);
                        if (state.counterTimerSec <= 0.f)
                        {
                            state.bCounterStrikeActive = false;
                            if (world.HasComponent<TransformComponent>(entity) &&
                                world.HasComponent<ChampionComponent>(entity))
                            {
                                const auto& champion = world.GetComponent<ChampionComponent>(entity);
                                const Vec3 origin = world.GetComponent<TransformComponent>(entity).GetPosition();
                                EnqueueCircleDamage(
                                    world,
                                    entity,
                                    champion.team,
                                    origin,
                                    state.counterRadius,
                                    state.counterDamage,
                                    static_cast<u8_t>(eSkillSlot::E));
                            }
                        }
                    }
```

```text
아래로 교체:
```

```cpp
                    if (state.bCounterStrikeActive)
                    {
                        state.counterTimerSec = std::max(0.f, state.counterTimerSec - tc.fDt);
                        if (state.counterTimerSec <= 0.f)
                            EndCounterStrike(world, tc, entity, state);
                    }
```

```text
기존 코드:
```

```cpp
    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID /*target*/,
        eTeam /*casterTeam*/,
        f32_t baseDamage)
```

```text
아래로 교체:
```

```cpp
    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        f32_t baseDamage)
```

```text
기존 코드:
```

```cpp
        if (state.bEmpowerActive)
        {
            damage += state.empowerBonusDamage;
            state.bEmpowerActive = false;
            state.empowerTimerSec = 0.f;
        }
```

```text
아래로 교체:
```

```cpp
        if (state.bEmpowerActive)
        {
            EnqueueMagicDamage(
                world,
                caster,
                target,
                casterTeam,
                state.empowerBonusDamage,
                static_cast<u8_t>(eSkillSlot::W),
                1);

            state.bEmpowerActive = false;
            state.empowerTimerSec = 0.f;
        }
```

```text
기존 코드:
```

```cpp
        return damage;
    }

    void Tick(CWorld& world, const TickContext& tc)
```

```text
아래로 교체:
```

```cpp
        return damage;
    }

    bool_t TryDodgeBasicAttack(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            !world.HasComponent<JaxSimComponent>(target))
        {
            return false;
        }

        if (world.HasComponent<ChampionComponent>(target) &&
            world.GetComponent<ChampionComponent>(target).id != eChampion::JAX)
        {
            return false;
        }

        const JaxSimComponent& state = world.GetComponent<JaxSimComponent>(target);
        if (!state.bCounterStrikeActive || state.counterTimerSec <= 0.f)
            return false;

        GameplayFeedback::EnqueueWorldTextFeedback(
            world,
            tc,
            attacker,
            target,
            GameplayFeedback::WorldTextFeedbackKind::Dodge);
        return true;
    }

    void Tick(CWorld& world, const TickContext& tc)
```

```text
1-8. C:/Users/user/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

기존 코드:
```

```cpp
        const eTeam sourceTeam = ResolveTeam(world, source);
        const eTeam targetTeam = ResolveTeam(world, target);
        if (sourceTeam == targetTeam && sourceTeam != eTeam::Neutral)
            return false;

        const f32_t damage =
            ResolveBasicAttackDamage(world, source, target, sourceTeam);
```

```text
아래로 교체:
```

```cpp
        const eTeam sourceTeam = ResolveTeam(world, source);
        const eTeam targetTeam = ResolveTeam(world, target);
        if (sourceTeam == targetTeam && sourceTeam != eTeam::Neutral)
            return false;

        if (JaxGameSim::TryDodgeBasicAttack(world, tc, source, target))
            return true;

        const f32_t damage =
            ResolveBasicAttackDamage(world, source, target, sourceTeam);
```

```text
1-9. C:/Users/user/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

기존 코드:
```

```cpp
        { eChampion::JAX, 0, 1, 1.00f, 1.00f },
        { eChampion::JAX, 1, 1, 0.60f, 1.00f },
        { eChampion::JAX, 2, 1, 0.50f, 1.00f },
        { eChampion::JAX, 3, 1, 0.70f, 1.00f },
        { eChampion::JAX, 4, 1, 0.60f, 1.00f },
```

```text
아래로 교체:
```

```cpp
        { eChampion::JAX, 0, 1, 1.00f, 1.00f },
        { eChampion::JAX, 1, 1, 0.60f, 1.00f },
        { eChampion::JAX, 2, 1, 0.50f, 1.00f },
        { eChampion::JAX, 3, 1, 0.70f, 1.00f },
        { eChampion::JAX, 3, 2, 0.45f, 1.00f },
        { eChampion::JAX, 4, 1, 0.60f, 1.00f },
```

```text
기존 코드:
```

```cpp
        { eChampion::IRELIA, 2, 2, 4.00f },
        { eChampion::IRELIA, 3, 2, 3.50f },
        { eChampion::VIEGO, 2, 2, 4.f},
        { eChampion::ZED, 2, 2, 5.f },
```

```text
아래로 교체:
```

```cpp
        { eChampion::IRELIA, 2, 2, 4.00f },
        { eChampion::IRELIA, 3, 2, 3.50f },
        { eChampion::JAX, 3, 2, 2.00f },
        { eChampion::VIEGO, 2, 2, 4.f},
        { eChampion::ZED, 2, 2, 5.f },
```

```text
1-10. C:/Users/user/Desktop/Winters/Client/Private/GameObject/Champion/Jax/Jax_Registration.cpp

기존 코드:
```

```cpp
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 0.6f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell3_attack1";
                s.lockDurationSec = 0.7f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.castFrame = 6.f; s.recoveryFrame = 14.f; s.animPlaySpeed = 1.f;
                s.castFrameHookId = kJax_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 3, s);
            }
```

```text
아래로 교체:
```

```cpp
            {
                SkillDef s{};
                s.champ = eChampion::JAX; s.slot = 3;
                s.targetMode = eTargetMode::Self;
                s.cooldownSec = 0.6f; s.rangeMax = 0.f; s.manaCost = 0.f;
                s.animKey = "spell3_attack1";
                s.lockDurationSec = 0.7f; s.bOneShot = true;
                s.rotate = eRotateMode::None;
                s.stageCount = 2;
                s.stage2TargetMode = eTargetMode::Self;
                s.stage2AnimKey = "spell3_attack2";
                s.stage2LockSec = 0.45f;
                s.stage2Rotate = eRotateMode::None;
                s.stageWindowSec = 2.0f;
                s.castFrame = 6.f; s.recoveryFrame = 14.f;
                s.stage2CastFrame = 1.f; s.stage2RecoveryFrame = 8.f;
                s.animPlaySpeed = 1.f; s.stage2PlaySpeed = 1.f;
                s.castFrameHookId = kJax_E_Cast;
                CSkillRegistry::Instance().Add(eChampion::JAX, 3, s);
            }
```

```text
1-11. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

기존 코드:
```

```cpp
                    StunComponent stun{};
                    stun.fRemaining = 0.75f;
                    stun.sourceEntity = ctx.casterEntity;

                    if (world.HasComponent<StunComponent>(target))
                        world.GetComponent<StunComponent>(target) = stun;
                    else
                        world.AddComponent<StunComponent>(target, stun);

                    EnqueuePhysicalDamage(
```

```text
아래로 교체:
```

```cpp
                    StatusEffectApplyDesc stun{};
                    stun.effectId = eStatusEffectId::GenericStun;
                    stun.stackPolicy = eStatusStackPolicy::RefreshDuration;
                    stun.sourceEntity = ctx.casterEntity;
                    stun.stackGroup =
                        static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) | 3u);
                    stun.stateFlags =
                        kGameplayStateStunnedFlag |
                        kGameplayStateCannotMoveFlag |
                        kGameplayStateCannotAttackFlag |
                        kGameplayStateCannotCastFlag;
                    stun.fDurationSec = 0.75f;

                    if (ctx.pTickCtx)
                    {
                        GameplayStatus::ApplyStatusEffectWithFeedback(
                            world,
                            *ctx.pTickCtx,
                            target,
                            stun,
                            GameplayFeedback::WorldTextFeedbackKind::Stun);
                    }
                    else
                    {
                        GameplayStatus::ApplyStatusEffect(world, target, stun);
                    }

                    EnqueuePhysicalDamage(
```

```text
1-12. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.h

기존 코드:
```

```cpp
    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius);
    void ApplyTornadoAirborne(CWorld& world, EntityID source, EntityID target);
```

```text
아래로 교체:
```

```cpp
    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius);
    void ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
```

```text
1-13. C:/Users/user/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

기존 코드:
```

```cpp
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

```text
아래로 교체:
```

```cpp
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

```text
기존 코드:
```

```cpp
        ApplyAirborne(world, ctx.casterEntity, target, kYasuoRHoldAirborneSec);
        const bool_t bPlaced = PlaceCasterForUltimate(world, ctx.pTickCtx, ctx.casterEntity, target);
```

```text
아래로 교체:
```

```cpp
        ApplyAirborne(world, ctx.casterEntity, target, kYasuoRHoldAirborneSec);
        if (ctx.pTickCtx)
        {
            GameplayFeedback::EnqueueWorldTextFeedback(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target,
                GameplayFeedback::WorldTextFeedbackKind::Airborne);
        }
        const bool_t bPlaced = PlaceCasterForUltimate(world, ctx.pTickCtx, ctx.casterEntity, target);
```

```text
기존 코드:
```

```cpp
    void ApplyTornadoAirborne(CWorld& world, EntityID source, EntityID target)
    {
        ApplyAirborne(world, source, target, kYasuoAirborneDurationSec);
        std::cout << "[YasuoSim] airborne target=" << target
```

```text
아래로 교체:
```

```cpp
    void ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
    {
        ApplyAirborne(world, source, target, kYasuoAirborneDurationSec);
        GameplayFeedback::EnqueueWorldTextFeedback(
            world,
            tc,
            source,
            target,
            GameplayFeedback::WorldTextFeedbackKind::Airborne);
        std::cout << "[YasuoSim] airborne target=" << target
```

```text
1-14. C:/Users/user/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:
```

```cpp
            if (projectile.kind == eProjectileKind::Tornado)
                YasuoGameSim::ApplyTornadoAirborne(m_world, projectile.sourceEntity, target);
```

```text
아래로 교체:
```

```cpp
            if (projectile.kind == eProjectileKind::Tornado)
                YasuoGameSim::ApplyTornadoAirborne(m_world, tc, projectile.sourceEntity, target);
```

```text
1-15. C:/Users/user/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:
```

```cpp
    void    Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
```

```text
아래로 교체:
```

```cpp
    void    Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void    Push_WorldText(const Vec3& vWorldPos, const char* pText, u8_t iTextStyle);
    void Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
```

```text
기존 코드:
```

```cpp
        u8_t iDamageType = 0;
        bool_t bWasCrit = false;
        bool_t bKilled = false;
    };
```

```text
아래로 교체:
```

```cpp
        u8_t iDamageType = 0;
        bool_t bWasCrit = false;
        bool_t bKilled = false;
        std::string strText;
        u8_t iTextStyle = 0;
    };
```

```text
1-16. C:/Users/user/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:
```

```cpp
    void UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    //Kill, Slay, Destroy Log
```

```text
아래로 교체:
```

```cpp
    void UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void UI_Push_WorldText(const Vec3& vWorldPos, const char* pText, u8_t iTextStyle);
    //Kill, Slay, Destroy Log
```

```text
1-17. C:/Users/user/Desktop/Winters/Engine/Private/GameInstance.cpp

기존 코드:
```

```cpp
void CGameInstance::UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
	u8_t iDamageType, bool_t bWasCrit, bool_t bKilled)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_DamageNumber(vWorldPos, fAmount, iDamageType, bWasCrit, bKilled);
}
```

```text
아래에 추가:
```

```cpp
void CGameInstance::UI_Push_WorldText(const Vec3& vWorldPos,
	const char* pText, u8_t iTextStyle)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_WorldText(vWorldPos, pText, iTextStyle);
}
```

```text
1-18. C:/Users/user/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:
```

```cpp
    ImU32 UI_DamageColor(u8_t iDamageType, bool_t bWasCrit, bool_t bKilled, f32_t alpha)
    {
        if (bKilled)
            return UI_ColorWithAlpha(255, 82, 74, alpha);
        if (bWasCrit)
            return UI_ColorWithAlpha(255, 170, 58, alpha);

        switch (iDamageType)
        {
        case 1:
            return UI_ColorWithAlpha(126, 202, 255, alpha);
        case 2:
            return UI_ColorWithAlpha(248, 248, 248, alpha);
        default:
            return UI_ColorWithAlpha(255, 229, 180, alpha);
        }
    }
```

```text
아래에 추가:
```

```cpp
    ImU32 UI_StatusTextColor(u8_t iTextStyle, f32_t alpha)
    {
        switch (iTextStyle)
        {
        case 1:
            return UI_ColorWithAlpha(152, 214, 255, alpha);
        case 2:
            return UI_ColorWithAlpha(255, 221, 92, alpha);
        case 3:
            return UI_ColorWithAlpha(190, 236, 255, alpha);
        default:
            return UI_ColorWithAlpha(245, 245, 245, alpha);
        }
    }
```

```text
기존 코드:
```

```cpp
void CUI_Manager::Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
    u8_t iDamageType, bool_t bWasCrit, bool_t bKilled)
{
    if (fAmount <= 0.f)
        return;

    if (m_DamageFloaters.size() >= 128)
        m_DamageFloaters.erase(m_DamageFloaters.begin());

    DamageFloater floater{};
    floater.vWorldPos = vWorldPos;
    floater.fAmount = fAmount;
    floater.fLifetime = bKilled ? (m_fDamageFloaterLife + 0.25f) : m_fDamageFloaterLife;
    floater.fRisePixels = bWasCrit ? (m_fDamageFloaterRise + 12.f) : m_fDamageFloaterRise;
    floater.iDamageType = iDamageType;
    floater.bWasCrit = bWasCrit;
    floater.bKilled = bKilled;

    const u32_t seed =
        static_cast<u32_t>(m_DamageFloaters.size() * 37u) ^
        static_cast<u32_t>(fAmount * 17.f);
    floater.fXJitter = static_cast<f32_t>(static_cast<i32_t>(seed % 17u) - 8) * 1.5f;

    m_DamageFloaters.push_back(floater);
}
```

```text
아래에 추가:
```

```cpp
void CUI_Manager::Push_WorldText(const Vec3& vWorldPos,
    const char* pText, u8_t iTextStyle)
{
    if (!pText || pText[0] == '\0')
        return;

    if (m_DamageFloaters.size() >= 128)
        m_DamageFloaters.erase(m_DamageFloaters.begin());

    DamageFloater floater{};
    floater.vWorldPos = vWorldPos;
    floater.fLifetime = m_fDamageFloaterLife;
    floater.fRisePixels = m_fDamageFloaterRise;
    floater.strText = pText;
    floater.iTextStyle = iTextStyle;

    const u32_t seed =
        static_cast<u32_t>(m_DamageFloaters.size() * 37u) ^
        static_cast<u32_t>(pText[0] * 17u);
    floater.fXJitter =
        static_cast<f32_t>(static_cast<i32_t>(seed % 17u) - 8) * 1.5f;

    m_DamageFloaters.push_back(floater);
}
```

```text
기존 코드:
```

```cpp
        char text[32]{};
        std::snprintf(text, sizeof(text), "%.0f", floater.fAmount);

        const f32_t fontSize = floater.bWasCrit ? 26.f : 20.f;
        const ImVec2 textSize = pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
        const ImVec2 pos(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f);
        const ImU32 color = UI_DamageColor(
            floater.iDamageType,
            floater.bWasCrit,
            floater.bKilled,
            alpha);

        UI_DrawOutlinedText(pDraw, pFont, fontSize, pos, color, text);
```

```text
아래로 교체:
```

```cpp
        char numberText[32]{};
        const bool_t bWorldText = !floater.strText.empty();
        const char* pText = floater.strText.c_str();
        if (!bWorldText)
        {
            std::snprintf(numberText, sizeof(numberText), "%.0f", floater.fAmount);
            pText = numberText;
        }

        const f32_t fontSize = bWorldText ? 22.f : (floater.bWasCrit ? 26.f : 20.f);
        const ImVec2 textSize = pFont->CalcTextSizeA(fontSize, FLT_MAX, 0.f, pText);
        const ImVec2 pos(screen.x - textSize.x * 0.5f, screen.y - textSize.y * 0.5f);
        const ImU32 color = bWorldText
            ? UI_StatusTextColor(floater.iTextStyle, alpha)
            : UI_DamageColor(
                floater.iDamageType,
                floater.bWasCrit,
                floater.bKilled,
                alpha);

        UI_DrawOutlinedText(pDraw, pFont, fontSize, pos, color, pText);
```

```text
1-19. C:/Users/user/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:
```

```cpp
#include "Renderer/ModelRenderer.h"
#include "Shared/GameSim/Components/HealthComponent.h"
```

```text
아래로 교체:
```

```cpp
#include "Renderer/ModelRenderer.h"
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
#include "Shared/GameSim/Components/HealthComponent.h"
```

```text
기존 코드:
```

```cpp
    const char* ResolveEffectTriggerCue(eChampion hookChampion, u8_t slot)
    {
        if (hookChampion != eChampion::KINDRED)
            return nullptr;

        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::BasicAttack:
            return "Kindred.BA.Hit";
        case eSkillSlot::Q:
            return "Kindred.Q.Arrow";
        case eSkillSlot::W:
            return "Kindred.W.Zone";
        case eSkillSlot::E:
            return "Kindred.E.Mark";
        case eSkillSlot::R:
            return "Kindred.R.Zone";
        default:
            return nullptr;
        }
    }
```

```text
아래에 추가:
```

```cpp
    const char* ResolveWorldTextFeedbackText(GameplayFeedback::WorldTextFeedbackKind kind)
    {
        switch (kind)
        {
        case GameplayFeedback::WorldTextFeedbackKind::Dodge:
            return "\xED\x9A\x8C\xED\x94\xBC!";
        case GameplayFeedback::WorldTextFeedbackKind::Stun:
            return "\xEA\xB8\xB0\xEC\xA0\x88!";
        case GameplayFeedback::WorldTextFeedbackKind::Airborne:
            return "\xEA\xB3\xB5\xEC\xA4\x91!";
        default:
            return "";
        }
    }

    u8_t ResolveWorldTextFeedbackStyle(GameplayFeedback::WorldTextFeedbackKind kind)
    {
        switch (kind)
        {
        case GameplayFeedback::WorldTextFeedbackKind::Dodge:
            return 1u;
        case GameplayFeedback::WorldTextFeedbackKind::Stun:
            return 2u;
        case GameplayFeedback::WorldTextFeedbackKind::Airborne:
            return 3u;
        default:
            return 0u;
        }
    }

    bool_t TryPushWorldTextFeedback(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::EffectTriggerEvent* ev,
        const Vec3& fallbackPos)
    {
        if (!ev)
            return false;

        GameplayFeedback::WorldTextFeedbackKind kind{};
        if (!GameplayFeedback::TryResolveWorldTextEffectId(ev->effectId(), kind))
            return false;

        const char* pText = ResolveWorldTextFeedbackText(kind);
        if (!pText || pText[0] == '\0')
            return true;

        EntityID target = ev->targetNet() != NULL_NET_ENTITY
            ? entityMap.FromNet(ev->targetNet())
            : NULL_ENTITY;

        Vec3 textPos = fallbackPos;
        if (target != NULL_ENTITY && world.HasComponent<TransformComponent>(target))
            textPos = world.GetComponent<TransformComponent>(target).GetPosition();
        textPos.y += 2.35f;

        CGameInstance::Get()->UI_Push_WorldText(
            textPos,
            pText,
            ResolveWorldTextFeedbackStyle(kind));
        return true;
    }
```

```text
기존 코드:
```

```cpp
    const EntityID source = ev->sourceNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->sourceNet())
        : NULL_ENTITY;

    if (eventSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
```

```text
아래로 교체:
```

```cpp
    const EntityID source = ev->sourceNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->sourceNet())
        : NULL_ENTITY;

    if (TryPushWorldTextFeedback(world, entityMap, ev, pos))
        return;

    if (eventSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
```

```text
2. 검증

미검증:
- 코드 미반영, 빌드 미검증.
- Jax W가 기본 BA 물리 데미지와 별도 마법 데미지를 모두 내는지 미검증.
- Jax E 지속 중 BA 회피 시 데미지/BA hit FX 없이 "회피!"만 뜨는지 미검증.
- Jax E 재입력/타임아웃 시 범위 피해, 기절 상태, "기절!" 텍스트가 같이 적용되는지 미검증.
- Irelia E 기절과 Yasuo airborne가 같은 GameplayFeedback world text 경로를 타는지 미검증.

수동 확인:
- Runtime Jax animation 이상 없음 확인됨.

검증 명령:
```

```powershell
git diff --check
msbuild Server\Include\Server.vcxproj /p:Configuration=Debug /p:Platform=x64
msbuild Client\Include\Client.vcxproj /p:Configuration=Debug /p:Platform=x64
```

```text
확인 필요:
- 새 header가 VS 프로젝트 탐색기에 보여야 하면 `.vcxproj.filters` 등록 여부만 별도 검토.
- `EffectTriggerEvent`를 공용 feedback 운반체로 재사용하는 1차안이다. 별도 FlatBuffer `GameplayFeedbackEvent`가 필요해지면 schema/generated 코드까지 확장하는 2차 계획으로 분리.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
```

