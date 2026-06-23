#include "Game/ReplicationEmitter.h"

#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

#include <functional>
#include <utility>

namespace
{
    struct PendingActionEvent
    {
        NetEntityId netId = NULL_NET_ENTITY;
        ReplicatedActionComponent action{};
    };
}

std::vector<SharedSim::SerializedReplicatedEvent> CReplicationEmitter::CollectActionStartEvents(
    CWorld& world,
    EntityIdMap& entityMap,
    u64_t serverTick,
    std::unordered_map<EntityID, u32_t>& lastBroadcastActionSeq)
{
    std::vector<PendingActionEvent> pendingEvents;
    world.ForEach<ReplicatedActionComponent>(
        std::function<void(EntityID, ReplicatedActionComponent&)>(
            [&](EntityID entity, ReplicatedActionComponent& action)
            {
                if (action.sequence == 0)
                    return;

                const NetEntityId netId = entityMap.ToNet(entity);
                if (netId == NULL_NET_ENTITY)
                    return;

                u32_t& lastSeq = lastBroadcastActionSeq[entity];
                if (lastSeq == action.sequence)
                    return;

                lastSeq = action.sequence;
                pendingEvents.push_back(PendingActionEvent{ netId, action });
            }));

    std::vector<SharedSim::SerializedReplicatedEvent> serializedEvents;
    serializedEvents.reserve(pendingEvents.size());
    for (const PendingActionEvent& pending : pendingEvents)
    {
        SharedSim::SerializedReplicatedEvent serialized{};
        if (SharedSim::CReplicatedEventSerializer::BuildActionStart(
            pending.netId,
            pending.action,
            serverTick,
            serialized))
        {
            serializedEvents.push_back(std::move(serialized));
        }
    }

    return serializedEvents;
}

std::vector<EntityID> CReplicationEmitter::CollectReplicatedEventEntities(CWorld& world)
{
    std::vector<EntityID> result;
    const auto replicatedEvents =
        DeterministicEntityIterator<ReplicatedEventComponent>::CollectSorted(world);

    result.reserve(replicatedEvents.size());
    for (EntityID entity : replicatedEvents)
    {
        if (!world.IsAlive(entity) ||
            !world.HasComponent<ReplicatedEventComponent>(entity))
        {
            continue;
        }

        result.push_back(entity);
    }

    return result;
}

bool_t CReplicationEmitter::TryBuildReplicatedEvent(
    CWorld& world,
    EntityIdMap& entityMap,
    EntityID eventEntity,
    u64_t serverTick,
    SharedSim::SerializedReplicatedEvent& outSerialized)
{
    if (!world.IsAlive(eventEntity) ||
        !world.HasComponent<ReplicatedEventComponent>(eventEntity))
    {
        return false;
    }

    const ReplicatedEventComponent event =
        world.GetComponent<ReplicatedEventComponent>(eventEntity);

    return SharedSim::CReplicatedEventSerializer::Build(
        world,
        entityMap,
        event,
        serverTick,
        outSerialized);
}
