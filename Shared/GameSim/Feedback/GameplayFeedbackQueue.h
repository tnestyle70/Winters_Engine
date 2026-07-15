#pragma once

#include "Shared/GameSim/Feedback/GameplayFeedback.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
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

    inline EntityID EnqueueGoldRewardFeedback(
        CWorld& world,
        const TickContext& tc,
        EntityID recipient,
        EntityID victim,
        u32_t goldAmount,
        u16_t durationMs = 900)
    {
        if (goldAmount == 0u)
            return NULL_ENTITY;

        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = recipient;
        event.targetEntity = victim;
        event.effectId = BuildWorldTextEffectId(WorldTextFeedbackKind::Gold);
        event.durationMs = durationMs;
        event.flags = PackWorldTextGoldAmount(goldAmount);
        event.startTick = tc.tickIndex;
        event.position = ResolveWorldTextPosition(world, recipient, victim);

        return EnqueueReplicatedEvent(world, event);
    }
}
