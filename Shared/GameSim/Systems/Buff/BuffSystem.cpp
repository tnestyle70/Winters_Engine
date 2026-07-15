#include "Shared/GameSim/Systems/Buff/BuffSystem.h"

#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
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

bool_t CBuffSystem::PruneExpiredTickBuffs(
    CWorld& world,
    const TickContext& tc)
{
    bool_t bAnyChanged = false;
    const auto entities =
        DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& component = world.GetComponent<BuffComponent>(entity);
        bool_t bChanged = false;
        u8_t uWrite = 0u;
        for (u8_t uRead = 0u;
            uRead < component.count && uRead < BuffComponent::kMaxBuffs;
            ++uRead)
        {
            const BuffInstance buff = component.buffs[uRead];
            if (buff.uExpireTick != 0u &&
                tc.tickIndex >= buff.uExpireTick)
            {
                bChanged = true;
                continue;
            }
            component.buffs[uWrite++] = buff;
        }

        if (uWrite != component.count)
        {
            for (u8_t i = uWrite;
                i < component.count && i < BuffComponent::kMaxBuffs;
                ++i)
            {
                component.buffs[i] = {};
            }
            component.count = uWrite;
        }

        if (bChanged && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
        bAnyChanged = bAnyChanged || bChanged;
    }
    return bAnyChanged;
}

void CBuffSystem::AdvanceDurationsAfterStat(
    CWorld& world,
    const TickContext& tc)
{
    const auto entities =
        DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& component = world.GetComponent<BuffComponent>(entity);
        bool_t bChanged = false;
        u8_t uWrite = 0u;
        for (u8_t uRead = 0u;
            uRead < component.count && uRead < BuffComponent::kMaxBuffs;
            ++uRead)
        {
            BuffInstance buff = component.buffs[uRead];
            bool_t bKeep = true;
            if (buff.uExpireTick != 0u)
            {
                const u64_t uRemainingTicks = buff.uExpireTick > tc.tickIndex
                    ? buff.uExpireTick - tc.tickIndex
                    : 0u;
                buff.fDurationRemaining =
                    static_cast<f32_t>(uRemainingTicks) *
                    DeterministicTime::kFixedDt;
                bKeep = uRemainingTicks != 0u;
            }
            else
            {
                if (buff.fDurationRemaining > 0.f)
                    buff.fDurationRemaining -= tc.fDt;
                bKeep = buff.fDurationRemaining > 0.f;
            }

            if (bKeep)
                component.buffs[uWrite++] = buff;
            else
                bChanged = true;
        }

        if (uWrite != component.count)
        {
            for (u8_t i = uWrite;
                i < component.count && i < BuffComponent::kMaxBuffs;
                ++i)
            {
                component.buffs[i] = {};
            }
            component.count = uWrite;
        }

        if (bChanged && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
    }
}
