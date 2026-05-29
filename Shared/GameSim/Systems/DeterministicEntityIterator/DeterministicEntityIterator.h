#pragma once

#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <functional>
#include <vector>

template<typename TComponent>
class DeterministicEntityIterator
{
public:
    static std::vector<EntityID> CollectSorted(CWorld& world)
    {
        std::vector<EntityID> entities;
        world.ForEach<TComponent>(
            std::function<void(EntityID, TComponent&)>(
                [&](EntityID entity, TComponent&)
                {
                    entities.push_back(entity);
                }));

        std::sort(entities.begin(), entities.end());
        return entities;
    }
};
