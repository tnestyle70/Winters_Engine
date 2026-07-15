#include "Security/LagCompensation.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

void CLagCompensation::RecordHistory(CWorld& world, u64_t tickIndex)
{
    m_latestTick = tickIndex;

    for (auto it = m_history.begin(); it != m_history.end();)
    {
        if (!world.IsAlive(it->first) || !world.HasComponent<TransformComponent>(it->first))
            it = m_history.erase(it);
        else
            ++it;
    }

    const auto entities = DeterministicEntityIterator<TransformComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        const EntityGeneration generation = world.GetEntityGeneration(entity);
        LagCompensatedEntityState state{};
        state.vPosition = world.GetComponent<TransformComponent>(entity).GetLocalPosition();

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& hp = world.GetComponent<HealthComponent>(entity);
            state.fHp = hp.fCurrent;
            state.bIsDead = hp.bIsDead || hp.fCurrent <= 0.f;
        }

        auto& frames = m_history[entity];
        if (!frames.empty() && frames.back().generation != generation)
            frames.clear();

        frames.push_back({ tickIndex, generation, state });
        while (!frames.empty() && tickIndex > frames.front().tickIndex + kMaxRewindTicks)
            frames.pop_front();
    }
}

bool_t CLagCompensation::TryGetHistoricalStateAtTick(
    EntityHandle hEntity,
    u64_t uExpectedTick,
    LagCompensatedEntityState& outState) const
{
    if (!hEntity.IsValid() || uExpectedTick > m_latestTick)
        return false;

    const auto it = m_history.find(hEntity.GetIndex());
    if (it == m_history.end())
        return false;

    for (auto rit = it->second.rbegin(); rit != it->second.rend(); ++rit)
    {
        if (rit->generation != hEntity.GetGeneration())
            continue;
        if (rit->tickIndex == uExpectedTick)
        {
            outState = rit->state;
            return true;
        }
        if (rit->tickIndex < uExpectedTick)
            break;
    }

    return false;
}
