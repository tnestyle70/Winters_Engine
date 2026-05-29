#pragma once

#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Core/World/World.h"

inline EntityID EnqueueReplicatedEvent(CWorld& world, const ReplicatedEventComponent& event)
{
    const EntityID entity = world.CreateEntity();
    world.AddComponent<ReplicatedEventComponent>(entity, event);
    return entity;
}
