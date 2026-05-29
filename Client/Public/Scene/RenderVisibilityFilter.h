#pragma once

#include "Defines.h"
#include "ECS/Entity.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Shared/GameSim/Components/ReplicatedStateComponent.h"
#include "Shared/GameSim/Definitions/SnapshotStateFlags.h"

namespace UI
{
    inline u8_t QueryLocalTeam(CWorld& world)
    {
        u8_t localTeam = 0;
        bool_t bFound = false;

        world.ForEach<LocalPlayerTag, SpatialAgentComponent>(
            function<void(EntityID, LocalPlayerTag&, SpatialAgentComponent&)>(
                [&](EntityID, LocalPlayerTag&, SpatialAgentComponent& agent)
                {
                    if (bFound)
                        return;
                    localTeam = agent.team;
                    bFound = true;
                }));

        return localTeam;
    }

    inline bool_t IsRenderableForLocal(CWorld& world, EntityID entity, u8_t localTeam)
    {
        if (entity == NULL_ENTITY)
            return true;

        if (!world.HasComponent<SpatialAgentComponent>(entity))
            return true;

        const SpatialAgentComponent& agent = world.GetComponent<SpatialAgentComponent>(entity);
        const bool_t bInvisible =
            world.HasComponent<ReplicatedStateComponent>(entity) &&
            (world.GetComponent<ReplicatedStateComponent>(entity).stateFlags &
                kSnapshotStateInvisibleFlag) != 0u;
        if (bInvisible &&
            world.HasComponent<ChampionComponent>(entity) &&
            world.GetComponent<ChampionComponent>(entity).id == eChampion::ZED)
        {
            return false;
        }
        if (agent.team == localTeam)
            return true;

        if (bInvisible)
            return false;

#if defined(_DEBUG) && !defined(WINTERS_DISABLE_AI_VISIBILITY_DEBUG)
        return true;
#else
        if (!world.HasComponent<VisibilityComponent>(entity))
            return true;

        if (localTeam >= 8)
            return true;

        const VisibilityComponent& visibility = world.GetComponent<VisibilityComponent>(entity);
        return (visibility.teamVisibilityMask & static_cast<u8_t>(1u << localTeam)) != 0;
#endif
    }
}
