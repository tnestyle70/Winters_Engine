Session - Gameplay feedback world text를 Irelia R 느려짐, Jax E 회피/E2 기절, Yasuo Q3 공중에뜸으로 연결한다.

1. 반영해야 하는 코드

1-1. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Feedback/GameplayFeedback.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

namespace GameplayFeedback
{
    enum class WorldTextFeedbackKind : u16_t
    {
        None = 0,
        Dodge = 1,
        Slow = 2,
        Stun = 3,
        Airborne = 4,
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
        case WorldTextFeedbackKind::Slow:
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

1-2. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Feedback/GameplayFeedbackQueue.h

새 파일:

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

1-3. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h

기존 코드:

```cpp
#include "ECS/Components/GameplayComponents.h"
```

아래에 추가:

```cpp
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
```

기존 코드:

```cpp
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
```

아래로 교체:

```cpp
	void ApplyStatusEffect(CWorld& world, EntityID target,
		const StatusEffectApplyDesc& desc);
	bool_t ApplyStatusEffectWithFeedback(CWorld& world, const TickContext& tc,
		EntityID target, const StatusEffectApplyDesc& desc,
		GameplayFeedback::WorldTextFeedbackKind feedbackKind);
	void TickStatusEffects(CWorld& world, const TickContext& tc);
```

1-4. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.cpp

기존 코드:

```cpp
#include "ECS/World.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
```

아래로 교체:

```cpp
#include "ECS/World.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
```

기존 코드:

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

아래로 교체:

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

기존 코드:

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

아래로 교체:

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

1-5. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Components/JaxSimComponent.h

기존 코드:

```cpp
    f32_t counterRadius = 2.2f;
    f32_t counterDamage = 60.f;

    bool_t bUltActive = false;
```

아래로 교체:

```cpp
    f32_t counterRadius = 2.2f;
    f32_t counterDamage = 60.f;
    f32_t counterStunDurationSec = 1.f;

    bool_t bUltActive = false;
```

1-6. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.h

기존 코드:

```cpp
    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        f32_t baseDamage);
}
```

아래로 교체:

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

1-7. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Jax/JaxGameSim.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

기존 코드:

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"
```

기존 코드:

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

아래로 교체:

```cpp
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

기존 코드:

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

아래로 교체:

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

기존 코드:

```cpp
        return damage;
    }

    void Tick(CWorld& world, const TickContext& tc)
```

아래로 교체:

```cpp
        return damage;
    }

    bool_t TryDodgeBasicAttack(
        CWorld& world,
        const TickContext& tc,
        EntityID attacker,
        EntityID target)
    {
        if (attacker == NULL_ENTITY ||
            target == NULL_ENTITY ||
            attacker == target ||
            !world.IsAlive(attacker) ||
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

기존 코드:

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

아래로 교체:

```cpp
                    if (state.bCounterStrikeActive)
                    {
                        state.counterTimerSec = std::max(0.f, state.counterTimerSec - tc.fDt);
                        if (state.counterTimerSec <= 0.f)
                            EndCounterStrike(world, tc, entity, state);
                    }
```

1-8. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Systems/Combat/CombatActionSystem.cpp

기존 코드:

```cpp
        const eTeam sourceTeam = ResolveTeam(world, source);
        const eTeam targetTeam = ResolveTeam(world, target);
        if (sourceTeam == targetTeam && sourceTeam != eTeam::Neutral)
            return false;

        const f32_t damage =
            ResolveBasicAttackDamage(world, source, target, sourceTeam);
```

아래로 교체:

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

1-9. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Definitions/ChampionRuntimeDefaults.cpp

기존 코드:

```cpp
        { eChampion::JAX, 0, 1, 1.00f, 1.00f },
        { eChampion::JAX, 1, 1, 0.60f, 1.00f },
        { eChampion::JAX, 2, 1, 0.50f, 1.00f },
        { eChampion::JAX, 3, 1, 0.70f, 1.00f },
        { eChampion::JAX, 4, 1, 0.60f, 1.00f },
```

아래로 교체:

```cpp
        { eChampion::JAX, 0, 1, 1.00f, 1.00f },
        { eChampion::JAX, 1, 1, 0.60f, 1.00f },
        { eChampion::JAX, 2, 1, 0.50f, 1.00f },
        { eChampion::JAX, 3, 1, 0.70f, 1.00f },
        { eChampion::JAX, 3, 2, 0.45f, 1.00f },
        { eChampion::JAX, 4, 1, 0.60f, 1.00f },
```

기존 코드:

```cpp
        { eChampion::IRELIA, 2, 2, 4.00f },
        { eChampion::IRELIA, 3, 2, 3.50f },
        { eChampion::VIEGO, 2, 2, 4.f},
```

아래로 교체:

```cpp
        { eChampion::IRELIA, 2, 2, 4.00f },
        { eChampion::IRELIA, 3, 2, 3.50f },
        { eChampion::JAX, 3, 2, 2.00f },
        { eChampion::VIEGO, 2, 2, 4.f},
```

1-10. C:/Users/tnest/Desktop/Winters/Client/Private/GameObject/Champion/Jax/Jax_Registration.cpp

기존 코드:

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

아래로 교체:

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

1-11. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Irelia/IreliaGameSim.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

삭제할 코드:

```cpp
    constexpr f32_t kIreliaRHitStunSec = 0.3f;
```

삭제할 범위:
`void ApplyStun(CWorld& world, EntityID target, EntityID source, f32_t duration)` 줄부터
`void ApplySlow(CWorld& world, EntityID target, EntityID source, f32_t duration, f32_t moveSpeedMul)` 바로 위까지 삭제.
`void ApplySlow...` 줄은 남긴다.

기존 코드:

```cpp
    void ApplySlow(CWorld& world, EntityID target, EntityID source, f32_t duration, f32_t moveSpeedMul)
    {
        if (!IsAliveChampion(world, target))
            return;

        SlowComponent slow{};
        slow.fRemaining = duration;
        slow.fMoveSpeedMul = moveSpeedMul;
        slow.sourceEntity = source;
        if (world.HasComponent<SlowComponent>(target))
            world.GetComponent<SlowComponent>(target) = slow;
        else
            world.AddComponent<SlowComponent>(target, slow);

        GameplayStatus::RebuildGameplayState(world, target);
    }
```

아래로 교체:

```cpp
    StatusEffectApplyDesc MakeIreliaRSlowDesc(
        EntityID source,
        f32_t duration,
        f32_t moveSpeedMul)
    {
        StatusEffectApplyDesc slow{};
        slow.effectId = eStatusEffectId::GenericSlow;
        slow.stackPolicy = eStatusStackPolicy::RefreshDuration;
        slow.sourceEntity = source;
        slow.stackGroup =
            static_cast<u16_t>((static_cast<u32_t>(eChampion::IRELIA) << 8) |
                static_cast<u8_t>(eSkillSlot::R));
        slow.stateFlags = kGameplayStateSlowedFlag;
        slow.fDurationSec = duration;
        slow.fMoveSpeedMul = moveSpeedMul;
        return slow;
    }

    bool_t ApplySlow(CWorld& world, EntityID target, EntityID source, f32_t duration, f32_t moveSpeedMul)
    {
        if (!IsAliveChampion(world, target))
            return false;

        GameplayStatus::ApplyStatusEffect(
            world,
            target,
            MakeIreliaRSlowDesc(source, duration, moveSpeedMul));
        return true;
    }

    bool_t ApplySlowWithFeedback(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        EntityID source,
        f32_t duration,
        f32_t moveSpeedMul)
    {
        if (!IsAliveChampion(world, target))
            return false;

        return GameplayStatus::ApplyStatusEffectWithFeedback(
            world,
            tc,
            target,
            MakeIreliaRSlowDesc(source, duration, moveSpeedMul),
            GameplayFeedback::WorldTextFeedbackKind::Slow);
    }
```

기존 코드:

```cpp
                ApplyDisarm(world, hitTarget, caster, kIreliaRDisarmSec);
                ApplyStun(world, hitTarget, caster, kIreliaRHitStunSec);

                EmitIreliaREffect(world,
```

아래로 교체:

```cpp
                ApplyDisarm(world, hitTarget, caster, kIreliaRDisarmSec);
                ApplySlowWithFeedback(
                    world,
                    tc,
                    hitTarget,
                    caster,
                    kIreliaRWallSlowSec,
                    kIreliaRWallSlowMul);

                EmitIreliaREffect(world,
```

기존 코드:

```cpp
                    TrackTarget(state.rWallTargets, state.rWallTargetCount, target);
                    ApplySlow(world, target, caster, kIreliaRWallSlowSec, kIreliaRWallSlowMul);
                    ApplyDisarm(world, target, caster, kIreliaRDisarmSec);
```

아래로 교체:

```cpp
                    TrackTarget(state.rWallTargets, state.rWallTargetCount, target);
                    ApplySlowWithFeedback(
                        world,
                        tc,
                        target,
                        caster,
                        kIreliaRWallSlowSec,
                        kIreliaRWallSlowMul);
                    ApplyDisarm(world, target, caster, kIreliaRDisarmSec);
```

1-12. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.h

기존 코드:

```cpp
    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius);
    void ApplyTornadoAirborne(CWorld& world, EntityID source, EntityID target);
```

아래로 교체:

```cpp
    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius);
    bool_t ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target);
```

1-13. C:/Users/tnest/Desktop/Winters/Shared/GameSim/Champions/Yasuo/YasuoGameSim.cpp

기존 코드:

```cpp
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

아래로 교체:

```cpp
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
```

기존 코드:

```cpp
    void ApplyAirborne(CWorld& world, EntityID source, EntityID target, f32_t durationSec)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }

        auto& transform = world.GetComponent<TransformComponent>(target);
        const Vec3 pos = transform.GetPosition();

        if (world.HasComponent<YasuoAirborneComponent>(target))
        {
            auto& airborne = world.GetComponent<YasuoAirborneComponent>(target);
            airborne.sourceEntity = source;
            const f32_t remaining = airborne.durationSec - airborne.elapsedSec;
            if (remaining < durationSec)
                airborne.durationSec = airborne.elapsedSec + durationSec;
            airborne.lift = kYasuoAirborneLift;
        }
        else
        {
            YasuoAirborneComponent airborne{};
            airborne.sourceEntity = source;
            airborne.baseY = pos.y;
            airborne.durationSec = durationSec;
            airborne.lift = kYasuoAirborneLift;
            world.AddComponent<YasuoAirborneComponent>(target, airborne);
        }

        StunComponent stun{};
        stun.fRemaining = durationSec;
        stun.sourceEntity = source;
        if (world.HasComponent<StunComponent>(target))
            world.GetComponent<StunComponent>(target) = stun;
        else
            world.AddComponent<StunComponent>(target, stun);

        if (world.HasComponent<MoveTargetComponent>(target))
            world.GetComponent<MoveTargetComponent>(target).bHasTarget = false;
    }
```

아래로 교체:

```cpp
    bool_t ApplyAirborne(CWorld& world, EntityID source, EntityID target, f32_t durationSec)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        auto& transform = world.GetComponent<TransformComponent>(target);
        const Vec3 pos = transform.GetPosition();

        if (world.HasComponent<YasuoAirborneComponent>(target))
        {
            auto& airborne = world.GetComponent<YasuoAirborneComponent>(target);
            airborne.sourceEntity = source;
            const f32_t remaining = airborne.durationSec - airborne.elapsedSec;
            if (remaining < durationSec)
                airborne.durationSec = airborne.elapsedSec + durationSec;
            airborne.lift = kYasuoAirborneLift;
        }
        else
        {
            YasuoAirborneComponent airborne{};
            airborne.sourceEntity = source;
            airborne.baseY = pos.y;
            airborne.durationSec = durationSec;
            airborne.lift = kYasuoAirborneLift;
            world.AddComponent<YasuoAirborneComponent>(target, airborne);
        }

        StunComponent stun{};
        stun.fRemaining = durationSec;
        stun.sourceEntity = source;
        if (world.HasComponent<StunComponent>(target))
            world.GetComponent<StunComponent>(target) = stun;
        else
            world.AddComponent<StunComponent>(target, stun);

        if (world.HasComponent<MoveTargetComponent>(target))
            world.GetComponent<MoveTargetComponent>(target).bHasTarget = false;

        return true;
    }
```

기존 코드:

```cpp
    void ApplyTornadoAirborne(CWorld& world, EntityID source, EntityID target)
    {
        ApplyAirborne(world, source, target, kYasuoAirborneDurationSec);
        std::cout << "[YasuoSim] airborne target=" << target
            << " source=" << source
            << " duration=" << kYasuoAirborneDurationSec << "\n";
    }
```

아래로 교체:

```cpp
    bool_t ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
    {
        if (!ApplyAirborne(world, source, target, kYasuoAirborneDurationSec))
            return false;

        GameplayFeedback::EnqueueWorldTextFeedback(
            world,
            tc,
            source,
            target,
            GameplayFeedback::WorldTextFeedbackKind::Airborne);

        std::cout << "[YasuoSim] airborne target=" << target
            << " source=" << source
            << " duration=" << kYasuoAirborneDurationSec << "\n";
        return true;
    }
```

1-14. C:/Users/tnest/Desktop/Winters/Server/Private/Game/GameRoom.cpp

기존 코드:

```cpp
            if (projectile.kind == eProjectileKind::Tornado)
                YasuoGameSim::ApplyTornadoAirborne(m_world, projectile.sourceEntity, target);
```

아래로 교체:

```cpp
            if (projectile.kind == eProjectileKind::Tornado)
                YasuoGameSim::ApplyTornadoAirborne(m_world, tc, projectile.sourceEntity, target);
```

1-15. C:/Users/tnest/Desktop/Winters/Engine/Public/Manager/UI/UI_Manager.h

기존 코드:

```cpp
    void    Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
```

아래로 교체:

```cpp
    void    Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void    Push_WorldText(const Vec3& vWorldPos, const char* pText, u8_t iTextStyle);
    void Push_KillFeedBanner(eChampion eSourceChampion, eChampion eTargetChampion,
```

기존 코드:

```cpp
        u8_t iDamageType = 0;
        bool_t bWasCrit = false;
        bool_t bKilled = false;
    };
```

아래로 교체:

```cpp
        u8_t iDamageType = 0;
        bool_t bWasCrit = false;
        bool_t bKilled = false;
        std::string strText;
        u8_t iTextStyle = 0;
    };
```

1-16. C:/Users/tnest/Desktop/Winters/Engine/Include/GameInstance.h

기존 코드:

```cpp
    void UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    //Kill, Slay, Destroy Log
```

아래로 교체:

```cpp
    void UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
        u8_t iDamageType, bool_t bWasCrit, bool_t bKilled);
    void UI_Push_WorldText(const Vec3& vWorldPos, const char* pText, u8_t iTextStyle);
    //Kill, Slay, Destroy Log
```

1-17. C:/Users/tnest/Desktop/Winters/Engine/Private/GameInstance.cpp

기존 코드:

```cpp
void CGameInstance::UI_Push_DamageNumber(const Vec3& vWorldPos, f32_t fAmount,
	u8_t iDamageType, bool_t bWasCrit, bool_t bKilled)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_DamageNumber(vWorldPos, fAmount, iDamageType, bWasCrit, bKilled);
}
```

아래에 추가:

```cpp
void CGameInstance::UI_Push_WorldText(const Vec3& vWorldPos,
	const char* pText, u8_t iTextStyle)
{
	if (m_pUI_Manager)
		m_pUI_Manager->Push_WorldText(vWorldPos, pText, iTextStyle);
}
```

1-18. C:/Users/tnest/Desktop/Winters/Engine/Private/Manager/UI/UI_Manager.cpp

기존 코드:

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

아래에 추가:

```cpp
    ImU32 UI_StatusTextColor(u8_t iTextStyle, f32_t alpha)
    {
        switch (iTextStyle)
        {
        case 1:
            return UI_ColorWithAlpha(152, 214, 255, alpha);
        case 2:
            return UI_ColorWithAlpha(126, 202, 255, alpha);
        case 3:
            return UI_ColorWithAlpha(255, 221, 92, alpha);
        case 4:
            return UI_ColorWithAlpha(190, 236, 255, alpha);
        default:
            return UI_ColorWithAlpha(245, 245, 245, alpha);
        }
    }
```

기존 코드:

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

아래에 추가:

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

기존 코드:

```cpp
    ImFont* pFont = FindUIFont("hud");
    if (!pFont)
        return;

    for (const DamageFloater& floater : m_DamageFloaters)
    {
        ImVec2 screen{};
        if (!UI_WorldToScreen(mVP, floater.vWorldPos, screen, m_iWinSizeX, m_iWinSizeY))
            continue;

        const f32_t t = UI_Clamp01(floater.fAge / floater.fLifetime);
        const f32_t alpha = 1.f - t;
        screen.x += floater.fXJitter;
        screen.y -= floater.fRisePixels * t;

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
    }
```

아래로 교체:

```cpp
    ImFont* pDamageFont = FindUIFont("hud");
    ImFont* pWorldTextFont = FindUIFont("fallback");
    if (!pDamageFont && !pWorldTextFont)
        return;

    for (const DamageFloater& floater : m_DamageFloaters)
    {
        ImVec2 screen{};
        if (!UI_WorldToScreen(mVP, floater.vWorldPos, screen, m_iWinSizeX, m_iWinSizeY))
            continue;

        const f32_t t = UI_Clamp01(floater.fAge / floater.fLifetime);
        const f32_t alpha = 1.f - t;
        screen.x += floater.fXJitter;
        screen.y -= floater.fRisePixels * t;

        char numberText[32]{};
        const bool_t bWorldText = !floater.strText.empty();
        const char* pText = floater.strText.c_str();
        if (!bWorldText)
        {
            std::snprintf(numberText, sizeof(numberText), "%.0f", floater.fAmount);
            pText = numberText;
        }

        ImFont* pFont = bWorldText
            ? (pWorldTextFont ? pWorldTextFont : pDamageFont)
            : (pDamageFont ? pDamageFont : pWorldTextFont);
        if (!pFont)
            continue;

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
    }
```

1-19. C:/Users/tnest/Desktop/Winters/Client/Private/Network/Client/EventApplier.cpp

기존 코드:

```cpp
#include "Renderer/ModelRenderer.h"
#include "Shared/GameSim/Components/HealthComponent.h"
```

아래로 교체:

```cpp
#include "Renderer/ModelRenderer.h"
#include "Shared/GameSim/Feedback/GameplayFeedback.h"
#include "Shared/GameSim/Components/HealthComponent.h"
```

기존 코드:

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

아래에 추가:

```cpp
    const char* ResolveWorldTextFeedbackText(GameplayFeedback::WorldTextFeedbackKind kind)
    {
        switch (kind)
        {
        case GameplayFeedback::WorldTextFeedbackKind::Dodge:
            return "\xED\x9A\x8C\xED\x94\xBC";
        case GameplayFeedback::WorldTextFeedbackKind::Slow:
            return "\xEB\x8A\x90\xEB\xA0\xA4\xEC\xA7\x90";
        case GameplayFeedback::WorldTextFeedbackKind::Stun:
            return "\xEA\xB8\xB0\xEC\xA0\x88";
        case GameplayFeedback::WorldTextFeedbackKind::Airborne:
            return "\xEA\xB3\xB5\xEC\xA4\x91\xEC\x97\x90\xEB\x9C\xB8";
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
        case GameplayFeedback::WorldTextFeedbackKind::Slow:
            return 2u;
        case GameplayFeedback::WorldTextFeedbackKind::Stun:
            return 3u;
        case GameplayFeedback::WorldTextFeedbackKind::Airborne:
            return 4u;
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

기존 코드:

```cpp
    const EntityID source = ev->sourceNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->sourceNet())
        : NULL_ENTITY;

    if (eventSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
```

아래로 교체:

```cpp
    const EntityID source = ev->sourceNet() != NULL_NET_ENTITY
        ? entityMap.FromNet(ev->sourceNet())
        : NULL_ENTITY;

    if (TryPushWorldTextFeedback(world, entityMap, ev, pos))
        return;

    if (eventSlot == static_cast<u8_t>(eSkillSlot::BasicAttack) &&
```

2. 검증

미검증:
- 코드 미반영, 빌드 미검증.
- Irelia R hit target과 wall 진입 target에 "느려짐" 텍스트가 1회씩 뜨는지 미검증.
- Jax E 지속 중 적 미니언/챔피언 기본 공격이 데미지와 BA hit FX 없이 "회피"만 띄우는지 미검증.
- Jax E2 재입력 또는 타임아웃 시 범위 피해, 기절 상태, "기절" 텍스트가 같이 적용되는지 미검증.
- Yasuo Q3 tornado hit에서 실제 airborne 적용 성공 시에만 "공중에뜸" 텍스트가 뜨는지 미검증.
- 한글 world text가 fallback Korean font로 깨지지 않고 렌더되는지 미검증.

검증 명령:
```powershell
git diff --check
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64
msbuild Winters.sln /p:Configuration=Release /p:Platform=x64
cmake --build --preset engine-debug
cmake --build --preset engine-release
```

확인 필요:
- `.claude/gotchas.md`가 현재 checkout에 없어 `/plan-rules` gotchas 검토는 수행하지 못했다.
- 새 header가 VS 프로젝트 탐색기에 보여야 하면 `.vcxproj.filters` 등록 여부만 별도 확인.

후속 동기화:
- Engine public header 변경 후 `UpdateLib.bat` 실행 필요.
