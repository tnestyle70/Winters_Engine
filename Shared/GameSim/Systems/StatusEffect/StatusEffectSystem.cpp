#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

#include "ECS/World.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <functional>
#include <vector>

namespace
{
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
            return effect.stackGroup == desc.stackGroup;
        return effect.effectId == desc.effectId &&
            effect.sourceEntity == desc.sourceEntity;
    }

    void CompactExpired(StatusEffectComponent& effects)
    {
        u8_t write = 0;
        for (u8_t read = 0; read < effects.count; ++read)
        {
            if (effects.active[read].effectId == eStatusEffectId::None ||
                effects.active[read].fRemainingSec <= 0.f)
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

    void UpsertEffect(StatusEffectComponent& effects, const StatusEffectApplyDesc& desc)
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
                return;
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
            return;
        }

        u8_t replaceIndex = 0;
        for (u8_t i = 1; i < effects.count; ++i)
        {
            if (effects.active[i].fRemainingSec < effects.active[replaceIndex].fRemainingSec)
                replaceIndex = i;
        }
        effects.active[replaceIndex] = next;
    }

    void PushUnique(std::vector<EntityID>& entities, EntityID entity)
    {
        if (entity == NULL_ENTITY)
            return;
        if (std::find(entities.begin(), entities.end(), entity) == entities.end())
            entities.push_back(entity);
    }
}

namespace GameplayStatus
{
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
