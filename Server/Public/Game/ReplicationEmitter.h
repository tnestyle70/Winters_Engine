#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Systems/ReplicatedEventSerializer/ReplicatedEventSerializer.h"
#include "WintersTypes.h"

#include <unordered_map>
#include <vector>

class CWorld;
class EntityIdMap;

class CReplicationEmitter final
{
public:
    static std::vector<SharedSim::SerializedReplicatedEvent> CollectActionStartEvents(
        CWorld& world,
        EntityIdMap& entityMap,
        u64_t serverTick,
        std::unordered_map<EntityID, u32_t>& lastBroadcastActionSeq);

    static std::vector<EntityID> CollectReplicatedEventEntities(CWorld& world);

    static bool_t TryBuildReplicatedEvent(
        CWorld& world,
        EntityIdMap& entityMap,
        EntityID eventEntity,
        u64_t serverTick,
        SharedSim::SerializedReplicatedEvent& outSerialized);

private:
    CReplicationEmitter() = delete;
};
