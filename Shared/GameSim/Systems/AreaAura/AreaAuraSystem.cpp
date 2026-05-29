#include "Shared/GameSim/Systems/AreaAura/AreaAuraSystem.h"
#include "Shared/GameSim/Components/AreaAuraComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <vector>

namespace
{
    bool_t IsAliveTarget(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return false;

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    bool_t IsInsideArea(CWorld& world, const AreaAuraComponent& aura, EntityID target)
    {
        if (!world.HasComponent<TransformComponent>(target))
            return false;

        const Vec3 pos = world.GetComponent<TransformComponent>(target).GetPosition();
        switch (aura.shape)
        {
        case eAreaAuraShape::Circle:
            return WintersMath::DistanceSqXZ(pos, aura.vCenter) <= aura.fRadius * aura.fRadius;
        case eAreaAuraShape::Cone:
        case eAreaAuraShape::Box:
        default:
            return false;
        }
    }

    void ApplyAuraStatus(CWorld& world, const AreaAuraComponent& aura, EntityID target)
    {
        if (!aura.bApplyStatus || !IsAliveTarget(world, target) || !IsInsideArea(world, aura, target))
            return;

        StatusEffectApplyDesc desc = aura.status;
        if (desc.sourceEntity == NULL_ENTITY)
            desc.sourceEntity = aura.owner;
        GameplayStatus::ApplyStatusEffect(world, target, desc);
    }
}

void CAreaAuraSystem::Execute(CWorld& world, const TickContext& tc)
{
    std::vector<EntityID> expired;
    const auto entities = DeterministicEntityIterator<AreaAuraComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        if (!world.IsAlive(entity) || !world.HasComponent<AreaAuraComponent>(entity))
            continue;

        AreaAuraComponent& aura = world.GetComponent<AreaAuraComponent>(entity);
        aura.fRemainingSec -= tc.fDt;
        if (aura.fRemainingSec <= 0.f)
        {
            expired.push_back(entity);
            continue;
        }

        aura.fTickAccumulatorSec += tc.fDt;
        if (aura.fTickIntervalSec > 0.f &&
            aura.fTickAccumulatorSec < aura.fTickIntervalSec)
        {
            continue;
        }
        aura.fTickAccumulatorSec = 0.f;

        if (aura.applyMode == eAreaAuraApplyMode::OwnerOnly)
            ApplyAuraStatus(world, aura, aura.owner);
    }

    for (EntityID entity : expired)
    {
        if (!world.IsAlive(entity) || !world.HasComponent<AreaAuraComponent>(entity))
            continue;

        if (world.GetComponent<AreaAuraComponent>(entity).bDestroyOnExpire)
            world.DestroyEntity(entity);
        else
            world.RemoveComponent<AreaAuraComponent>(entity);
    }
}
