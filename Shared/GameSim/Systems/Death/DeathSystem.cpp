#include "Shared/GameSim/Systems/Death/DeathSystem.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillChargeStateComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include "Shared/GameSim/Core/World/World.h"

void CDeathSystem::Execute(CWorld& world, const TickContext& tc)
{
    (void)tc;

    const auto entities = DeterministicEntityIterator<HealthComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        auto& health = world.GetComponent<HealthComponent>(entity);
        const bool_t shouldBeDead = health.fCurrent <= 0.f;

        if (shouldBeDead)
        {
            health.fCurrent = 0.f;

            if (!health.bIsDead)
            {
                health.bIsDead = true;

                if (world.HasComponent<MoveTargetComponent>(entity))
                    world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;

                if (world.HasComponent<SkillStateComponent>(entity))
                {
                    auto& skillState = world.GetComponent<SkillStateComponent>(entity);
                    for (u8_t i = 0; i < 5; ++i)
                        skillState.slots[i].currentStage = 0;
                }
                if (world.HasComponent<SkillChargeStateComponent>(entity))
                    world.RemoveComponent<SkillChargeStateComponent>(entity);

                if (world.HasComponent<StructureComponent>(entity) &&
                    world.HasComponent<TargetableTag>(entity))
                {
                    world.RemoveComponent<TargetableTag>(entity);
                }
            }
        }
        else if (health.bIsDead)
        {
            health.bIsDead = false;

            if (world.HasComponent<StructureComponent>(entity) &&
                !world.HasComponent<TargetableTag>(entity))
            {
                world.AddComponent<TargetableTag>(entity);
            }
        }
    }
}
