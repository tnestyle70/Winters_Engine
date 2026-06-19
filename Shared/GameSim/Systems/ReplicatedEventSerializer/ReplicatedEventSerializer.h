#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Components/ReplicatedActionComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Core/World/World.h"

#include <flatbuffers/flatbuffers.h>

namespace SharedSim
{
    struct SerializedReplicatedEvent
    {
        flatbuffers::DetachedBuffer payload{};
        NetEntityId projectileNetToUnbind = NULL_NET_ENTITY;
        bool_t bValid = false;
        bool_t bUnbindProjectileAfterSend = false;
    };

    class CReplicatedEventSerializer final
    {
    public:
        static bool_t Build(
            CWorld& world,
            EntityIdMap& entityMap,
            const ReplicatedEventComponent& event,
            u64_t serverTick,
            SerializedReplicatedEvent& out);

        static bool_t BuildActionStart(
            NetEntityId netId,
            const ReplicatedActionComponent& action,
            u64_t serverTick,
            SerializedReplicatedEvent& out);

    private:
        CReplicatedEventSerializer() = delete;
    };
}
