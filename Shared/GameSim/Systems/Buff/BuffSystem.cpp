#include "Shared/GameSim/Systems/Buff/BuffSystem.h"

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

bool CBuffSystem::AddOrRefresh(BuffComponent& component, const BuffInstance& instance)
{
    for (u8_t i = 0; i < component.count && i < BuffComponent::kMaxBuffs; ++i)
    {
        BuffInstance& existing = component.buffs[i];
        if (existing.buffDefId == instance.buffDefId && existing.source == instance.source)
        {
            existing = instance;
            return true;
        }
    }

    if (component.count >= BuffComponent::kMaxBuffs)
        return false;

    component.buffs[component.count++] = instance;
    return true;
}

void CBuffSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& component = world.GetComponent<BuffComponent>(entity);
        bool_t changed = false;

        u8_t write = 0;
        for (u8_t read = 0; read < component.count && read < BuffComponent::kMaxBuffs; ++read)
        {
            BuffInstance buff = component.buffs[read];
            if (buff.fDurationRemaining > 0.f)
                buff.fDurationRemaining -= tc.fDt;

            if (buff.fDurationRemaining > 0.f)
            {
                component.buffs[write++] = buff;
            }
            else
            {
                changed = true;
            }
        }

        if (write != component.count)
        {
            for (u8_t i = write; i < component.count && i < BuffComponent::kMaxBuffs; ++i)
                component.buffs[i] = {};
            component.count = write;
        }

        if (changed && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
    }
}
