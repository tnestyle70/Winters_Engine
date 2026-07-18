#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/SkillChargeStateComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/CombatActionComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace
{
    constexpr f32_t kStatusExpiryEpsilonSec = 0.00001f;
    GameplayStateComponent& EnsureGameplayState(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<GameplayStateComponent>(entity))
            world.AddComponent<GameplayStateComponent>(entity, GameplayStateComponent{});
        return world.GetComponent<GameplayStateComponent>(entity);
    }

    StatusEffectComponent& EnsureStatusEffects(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<StatusEffectComponent>(entity))
            world.AddComponent<StatusEffectComponent>(entity, StatusEffectComponent{});
        return world.GetComponent<StatusEffectComponent>(entity);
    }

    bool_t IsSameStack(const StatusEffectInstance& effect, const StatusEffectApplyDesc& desc)
    {
        if (desc.stackGroup != 0u)
        {
            return effect.stackGroup == desc.stackGroup &&
                effect.effectId == desc.effectId &&
                effect.sourceEntity == desc.sourceEntity;
        }
        return effect.effectId == desc.effectId &&
            effect.sourceEntity == desc.sourceEntity;
    }

    bool_t IsCrowdControlStatus(const StatusEffectApplyDesc& desc)
    {
        constexpr u32_t kCrowdControlFlags =
            kGameplayStateStunnedFlag |
            kGameplayStateSlowedFlag |
            kGameplayStateDisarmedFlag |
            kGameplayStateAirborneFlag;
        return (desc.stateFlags & kCrowdControlFlags) != 0u;
    }

    void CompactExpired(StatusEffectComponent& effects)
    {
        u8_t write = 0;
        for (u8_t read = 0; read < effects.count; ++read)
        {
            if (effects.active[read].effectId == eStatusEffectId::None ||
                effects.active[read].fRemainingSec <= kStatusExpiryEpsilonSec)
            {
                continue;
            }

            if (write != read)
                effects.active[write] = effects.active[read];
            ++write;
        }

        for (u8_t i = write; i < effects.count; ++i)
            effects.active[i] = StatusEffectInstance{};
        effects.count = write;
    }

    bool_t UpsertEffect(StatusEffectComponent& effects, const StatusEffectApplyDesc& desc)
    {
        if (desc.stackPolicy != eStatusStackPolicy::AddIndependent)
        {
            for (u8_t i = 0; i < effects.count; ++i)
            {
                StatusEffectInstance& effect = effects.active[i];
                if (!IsSameStack(effect, desc))
                    continue;

                effect.effectId = desc.effectId;
                effect.stackPolicy = desc.stackPolicy;
                effect.sourceEntity = desc.sourceEntity;
                effect.stackGroup = desc.stackGroup;
                effect.stateFlags = desc.stateFlags;
                effect.fMoveSpeedMul = desc.fMoveSpeedMul;
                if (desc.stackPolicy == eStatusStackPolicy::KeepLongest)
                    effect.fRemainingSec = (std::max)(effect.fRemainingSec, desc.fDurationSec);
                else
                    effect.fRemainingSec = desc.fDurationSec;
                return true;
            }
        }

        StatusEffectInstance next{};
        next.effectId = desc.effectId;
        next.stackPolicy = desc.stackPolicy;
        next.sourceEntity = desc.sourceEntity;
        next.stackGroup = desc.stackGroup;
        next.stateFlags = desc.stateFlags;
        next.fRemainingSec = desc.fDurationSec;
        next.fMoveSpeedMul = desc.fMoveSpeedMul;

        if (effects.count < kMaxStatusEffectInstances)
        {
            effects.active[effects.count++] = next;
            return true;
        }

        // Silently evicting a live effect can detach state ownership from a
        // forced motion, carry lock, or channel. Preserve existing contracts
        // and let the caller observe deterministic capacity rejection.
        return false;
    }

    void PushUnique(std::vector<EntityID>& entities, EntityID entity)
    {
        if (entity == NULL_ENTITY)
            return;
        if (std::find(entities.begin(), entities.end(), entity) == entities.end())
            entities.push_back(entity);
    }

    bool_t ApplyStatusEffectInternal(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            desc.effectId == eStatusEffectId::None ||
            desc.fDurationSec <= 0.f)
        {
            return false;
        }
        if (IsCrowdControlStatus(desc) &&
            !GameplayStateQuery::CanReceiveCrowdControl(
                world,
                desc.sourceEntity,
                target))
        {
            return false;
        }

        StatusEffectComponent& effects = EnsureStatusEffects(world, target);
        if (desc.effectId == eStatusEffectId::GenericAirborne)
        {
            // Forced motion is single-owner per entity. A newer airborne therefore
            // replaces the complete previous airborne contract (state + motion)
            // instead of leaving independent timers behind with no matching arc.
            for (u8_t i = 0; i < effects.count; ++i)
            {
                if (effects.active[i].effectId == eStatusEffectId::GenericAirborne)
                    effects.active[i] = StatusEffectInstance{};
            }
            CompactExpired(effects);
        }
        if (!UpsertEffect(effects, desc))
            return false;
        GameplayStatus::RebuildGameplayState(world, target);
        return true;
    }

    void InterruptActionsForCrowdControl(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc,
        const TickContext& tc)
    {
        constexpr u32_t kInterruptingFlags =
            kGameplayStateStunnedFlag |
            kGameplayStateAirborneFlag |
            kGameplayStateInvulnerableFlag;
        if ((desc.stateFlags & kInterruptingFlags) == 0u)
            return;

        if (world.HasComponent<CombatActionComponent>(target))
            world.RemoveComponent<CombatActionComponent>(target);
        if (world.HasComponent<AttackChaseComponent>(target))
            world.RemoveComponent<AttackChaseComponent>(target);
        if (world.HasComponent<RecallComponent>(target))
            world.RemoveComponent<RecallComponent>(target);
        if (world.HasComponent<SkillChargeStateComponent>(target))
        {
            const SkillChargeStateComponent charge =
                world.GetComponent<SkillChargeStateComponent>(target);
            if (world.HasComponent<SkillStateComponent>(target) && charge.localSlot < 5u)
            {
                auto& slot =
                    world.GetComponent<SkillStateComponent>(target).slots[charge.localSlot];
                slot.currentStage = 0u;
                slot.stageWindow = 0.f;
            }
            world.RemoveComponent<SkillChargeStateComponent>(target);
        }

        if (world.HasComponent<ActionStateComponent>(target))
        {
            auto& action = world.GetComponent<ActionStateComponent>(target);
            action.actionId = static_cast<u16_t>(eActionStateId::None);
            action.startTick = tc.tickIndex;
            action.lockEndTick = tc.tickIndex;
            ++action.sequence;
            action.sourceChampion = eChampion::NONE;
            action.sourceSlot = 0u;
            action.stage = 1u;
            action.movePolicy = eSkillActionMovePolicy::Allow;
        }
        SetPoseState(world, target, ePoseStateId::Idle, tc.tickIndex, true);
    }
}

namespace GameplayStatus
{
    bool_t TryApplyStatusEffect(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc)
    {
        if (FioraGameSim::TryParryCrowdControl(world, target, desc, nullptr))
            return false;
        return ApplyStatusEffectInternal(world, target, desc);
    }

    bool_t TryApplyStatusEffect(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc,
        const TickContext& tc)
    {
        if (FioraGameSim::TryParryCrowdControl(world, target, desc, &tc))
            return false;
        if (!ApplyStatusEffectInternal(world, target, desc))
            return false;

        InterruptActionsForCrowdControl(world, target, desc, tc);

        const GameplayFeedback::WorldTextFeedbackKind feedbackKind =
            GameplayFeedback::ResolveStatusFeedbackKind(desc.effectId, desc.stateFlags);
        (void)GameplayFeedback::EnqueueWorldTextFeedback(
            world,
            tc,
            desc.sourceEntity,
            target,
            feedbackKind);
        return true;
    }

    void ApplyStatusEffect(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc)
    {
        (void)TryApplyStatusEffect(world, target, desc);
    }

    void ApplyStatusEffect(
        CWorld& world,
        EntityID target,
        const StatusEffectApplyDesc& desc,
        const TickContext& tc)
    {
        (void)TryApplyStatusEffect(world, target, desc, tc);
    }

    bool_t RemoveStatusEffect(
        CWorld& world,
        EntityID target,
        eStatusEffectId effectId,
        EntityID source)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            effectId == eStatusEffectId::None ||
            !world.HasComponent<StatusEffectComponent>(target))
        {
            return false;
        }

        StatusEffectComponent& effects = world.GetComponent<StatusEffectComponent>(target);
        bool_t removed = false;
        for (u8_t i = 0; i < effects.count; ++i)
        {
            StatusEffectInstance& effect = effects.active[i];
            if (effect.effectId != effectId || effect.sourceEntity != source)
                continue;
            effect = StatusEffectInstance{};
            removed = true;
        }
        if (!removed)
            return false;

        CompactExpired(effects);
        if (effectId == eStatusEffectId::GenericAirborne &&
            world.HasComponent<ForcedMotionComponent>(target))
        {
            const ForcedMotionComponent motion =
                world.GetComponent<ForcedMotionComponent>(target);
            if (motion.sourceEntity == source)
            {
                if (world.HasComponent<TransformComponent>(target))
                    world.GetComponent<TransformComponent>(target).SetPosition(motion.end);
                world.RemoveComponent<ForcedMotionComponent>(target);
            }
        }
        RebuildGameplayState(world, target);
        return true;
    }

    bool_t StartAirborneMotion(
        CWorld& world,
        EntityID target,
        EntityID source,
        const Vec3& landingPosition,
        f32_t durationSec,
        f32_t arcHeight,
        bool_t bGatherToLanding)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            durationSec <= 0.f ||
            !world.HasComponent<TransformComponent>(target) ||
            !GameplayStateQuery::CanReceiveCrowdControl(world, source, target))
        {
            return false;
        }

        const Vec3 currentPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        ForcedMotionComponent motion{};
        motion.kind = bGatherToLanding
            ? eForcedMotionKind::GatherAirborneArc
            : eForcedMotionKind::AirborneArc;
        motion.sourceEntity = source;
        motion.start = currentPosition;
        motion.end = landingPosition;
        if (!bGatherToLanding)
        {
            motion.end.x = currentPosition.x;
            motion.end.z = currentPosition.z;
        }
        motion.fDurationSec = durationSec;
        motion.fArcHeight = (std::max)(0.f, arcHeight);

        if (world.HasComponent<ForcedMotionComponent>(target))
            world.GetComponent<ForcedMotionComponent>(target) = motion;
        else
            world.AddComponent<ForcedMotionComponent>(target, motion);
        return true;
    }

    void TickStatusEffects(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> affected;
        const auto statusEntities =
            DeterministicEntityIterator<StatusEffectComponent>::CollectSorted(world);
        for (EntityID entity : statusEntities)
        {
            if (!world.IsAlive(entity) || !world.HasComponent<StatusEffectComponent>(entity))
                continue;

            StatusEffectComponent& effects = world.GetComponent<StatusEffectComponent>(entity);
            for (u8_t i = 0; i < effects.count; ++i)
                effects.active[i].fRemainingSec -= tc.fDt;
            CompactExpired(effects);
            PushUnique(affected, entity);
        }

        std::vector<EntityID> removeStun;
        std::vector<EntityID> removeSlow;
        std::vector<EntityID> removeDisarm;

        world.ForEach<StunComponent>(
            std::function<void(EntityID, StunComponent&)>(
                [&](EntityID entity, StunComponent& stun)
                {
                    stun.fRemaining -= tc.fDt;
                    PushUnique(affected, entity);
                    if (stun.fRemaining <= 0.f)
                        removeStun.push_back(entity);
                }));
        world.ForEach<SlowComponent>(
            std::function<void(EntityID, SlowComponent&)>(
                [&](EntityID entity, SlowComponent& slow)
                {
                    slow.fRemaining -= tc.fDt;
                    PushUnique(affected, entity);
                    if (slow.fRemaining <= 0.f)
                        removeSlow.push_back(entity);
                }));
        world.ForEach<DisarmComponent>(
            std::function<void(EntityID, DisarmComponent&)>(
                [&](EntityID entity, DisarmComponent& disarm)
                {
                    disarm.fRemaining -= tc.fDt;
                    PushUnique(affected, entity);
                    if (disarm.fRemaining <= 0.f)
                        removeDisarm.push_back(entity);
                }));

        for (EntityID entity : removeStun)
            world.RemoveComponent<StunComponent>(entity);
        for (EntityID entity : removeSlow)
            world.RemoveComponent<SlowComponent>(entity);
        for (EntityID entity : removeDisarm)
            world.RemoveComponent<DisarmComponent>(entity);

        std::sort(affected.begin(), affected.end());
        affected.erase(std::unique(affected.begin(), affected.end()), affected.end());
        for (EntityID entity : affected)
        {
            if (world.IsAlive(entity))
                RebuildGameplayState(world, entity);
        }
    }

    void TickForcedMotions(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finished;
        const auto entities =
            DeterministicEntityIterator<ForcedMotionComponent>::CollectSorted(world);
        finished.reserve(entities.size());

        for (EntityID entity : entities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<ForcedMotionComponent>(entity))
            {
                continue;
            }
            if (!world.HasComponent<TransformComponent>(entity))
            {
                finished.push_back(entity);
                continue;
            }

            ForcedMotionComponent& motion =
                world.GetComponent<ForcedMotionComponent>(entity);
            motion.fElapsedSec += tc.fDt;
            const f32_t duration = (std::max)(0.0001f, motion.fDurationSec);
            const f32_t t = (std::clamp)(motion.fElapsedSec / duration, 0.f, 1.f);
            const f32_t arc =
                std::sin(t * WintersMath::kPi) * (std::max)(0.f, motion.fArcHeight);

            Vec3 position{};
            if (motion.kind == eForcedMotionKind::GatherAirborneArc)
            {
                position.x = motion.start.x + (motion.end.x - motion.start.x) * t;
                position.z = motion.start.z + (motion.end.z - motion.start.z) * t;
            }
            else
            {
                position.x = motion.start.x;
                position.z = motion.start.z;
            }
            position.y =
                motion.start.y + (motion.end.y - motion.start.y) * t + arc;
            world.GetComponent<TransformComponent>(entity).SetPosition(position);

            if (t >= 1.f || motion.kind == eForcedMotionKind::None)
            {
                world.GetComponent<TransformComponent>(entity).SetPosition(motion.end);
                finished.push_back(entity);
            }
        }

        for (EntityID entity : finished)
        {
            if (world.IsAlive(entity) && world.HasComponent<ForcedMotionComponent>(entity))
                world.RemoveComponent<ForcedMotionComponent>(entity);
        }
    }

    void ClearStatusEffects(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return;

        if (world.HasComponent<ForcedMotionComponent>(entity))
        {
            const ForcedMotionComponent motion =
                world.GetComponent<ForcedMotionComponent>(entity);
            if (world.HasComponent<TransformComponent>(entity))
                world.GetComponent<TransformComponent>(entity).SetPosition(motion.end);
            world.RemoveComponent<ForcedMotionComponent>(entity);
        }
        if (world.HasComponent<StatusEffectComponent>(entity))
            world.RemoveComponent<StatusEffectComponent>(entity);
        if (world.HasComponent<StunComponent>(entity))
            world.RemoveComponent<StunComponent>(entity);
        if (world.HasComponent<SlowComponent>(entity))
            world.RemoveComponent<SlowComponent>(entity);
        if (world.HasComponent<DisarmComponent>(entity))
            world.RemoveComponent<DisarmComponent>(entity);
        RebuildGameplayState(world, entity);
    }

    void CleanseCrowdControlEffects(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return;

        if (world.HasComponent<StatusEffectComponent>(entity))
        {
            StatusEffectComponent& effects =
                world.GetComponent<StatusEffectComponent>(entity);
            constexpr u32_t kCleanseFlags =
                kGameplayStateStunnedFlag |
                kGameplayStateSlowedFlag |
                kGameplayStateDisarmedFlag;
            u8_t write = 0u;
            for (u8_t read = 0u; read < effects.count; ++read)
            {
                const StatusEffectInstance& effect = effects.active[read];
                const bool_t bAirborne =
                    (effect.stateFlags & kGameplayStateAirborneFlag) != 0u;
                const bool_t bCleanse =
                    !bAirborne && (effect.stateFlags & kCleanseFlags) != 0u;
                if (bCleanse)
                    continue;
                if (write != read)
                    effects.active[write] = effect;
                ++write;
            }
            for (u8_t index = write; index < effects.count; ++index)
                effects.active[index] = StatusEffectInstance{};
            effects.count = write;
        }

        if (world.HasComponent<StunComponent>(entity))
            world.RemoveComponent<StunComponent>(entity);
        if (world.HasComponent<SlowComponent>(entity))
            world.RemoveComponent<SlowComponent>(entity);
        if (world.HasComponent<DisarmComponent>(entity))
            world.RemoveComponent<DisarmComponent>(entity);
        RebuildGameplayState(world, entity);
    }

    void RebuildGameplayState(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return;

        GameplayStateComponent state{};
        if (world.HasComponent<StatusEffectComponent>(entity))
        {
            const StatusEffectComponent& effects =
                world.GetComponent<StatusEffectComponent>(entity);
            for (u8_t i = 0; i < effects.count; ++i)
            {
                const StatusEffectInstance& effect = effects.active[i];
                if (effect.effectId == eStatusEffectId::None ||
                    effect.fRemainingSec <= 0.f)
                {
                    continue;
                }

                state.stateFlags |= effect.stateFlags;
                if (effect.fMoveSpeedMul > 0.f)
                    state.fMoveSpeedMul = (std::min)(state.fMoveSpeedMul, effect.fMoveSpeedMul);
            }
        }

        if (world.HasComponent<StunComponent>(entity))
        {
            state.stateFlags |=
                kGameplayStateStunnedFlag |
                kGameplayStateCannotMoveFlag |
                kGameplayStateCannotAttackFlag |
                kGameplayStateCannotCastFlag;
        }
        if (world.HasComponent<SlowComponent>(entity))
        {
            const SlowComponent& slow = world.GetComponent<SlowComponent>(entity);
            state.stateFlags |= kGameplayStateSlowedFlag;
            if (slow.fMoveSpeedMul > 0.f)
                state.fMoveSpeedMul = (std::min)(state.fMoveSpeedMul, slow.fMoveSpeedMul);
        }
        if (world.HasComponent<DisarmComponent>(entity))
        {
            state.stateFlags |=
                kGameplayStateDisarmedFlag |
                kGameplayStateCannotAttackFlag;
        }

        EnsureGameplayState(world, entity) = state;
    }
}
