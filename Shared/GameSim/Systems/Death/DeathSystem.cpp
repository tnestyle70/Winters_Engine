#include "Shared/GameSim/Systems/Death/DeathSystem.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillChargeStateComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>

void CDeathSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<HealthComponent>::CollectSorted(world);

    for (EntityID entity : entities)
    {
        auto& health = world.GetComponent<HealthComponent>(entity);
        const bool_t shouldBeDead = health.fCurrent <= 0.f;

        if (shouldBeDead)
        {
            health.fCurrent = 0.f;

            if (world.HasComponent<InhibitorTag>(entity) &&
                world.HasComponent<InhibitorRespawnComponent>(entity))
            {
                auto& respawn = world.GetComponent<InhibitorRespawnComponent>(entity);
                if (!respawn.bRespawnPending)
                {
                    respawn.bRespawnPending = true;
                    respawn.fRespawnTimerSec = (std::max)(
                        0.f,
                        respawn.fRespawnDelaySec);
                }
                else
                {
                    respawn.fRespawnTimerSec = (std::max)(
                        0.f,
                        respawn.fRespawnTimerSec - tc.fDt);
                }

                if (respawn.fRespawnTimerSec <= 0.f && health.fMaximum > 0.f)
                {
                    health.fCurrent = health.fMaximum;
                    health.bIsDead = false;
                    if (world.HasComponent<StructureComponent>(entity))
                    {
                        auto& structure = world.GetComponent<StructureComponent>(entity);
                        structure.hp = health.fCurrent;
                        structure.maxHp = health.fMaximum;
                    }
                    respawn.bRespawnPending = false;
                    if (!world.HasComponent<TargetableTag>(entity))
                        world.AddComponent<TargetableTag>(entity);
                    continue;
                }
            }

            const bool_t bFirstDeathTransition = !health.bIsDead;
            health.bIsDead = true;

            if (bFirstDeathTransition)
            {
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
            }

            if (world.HasComponent<StructureComponent>(entity) &&
                world.HasComponent<TargetableTag>(entity))
            {
                world.RemoveComponent<TargetableTag>(entity);
            }
            continue;
        }

        if (world.HasComponent<InhibitorRespawnComponent>(entity))
        {
            auto& respawn = world.GetComponent<InhibitorRespawnComponent>(entity);
            respawn.bRespawnPending = false;
            respawn.fRespawnTimerSec = 0.f;
            if (world.HasComponent<StructureComponent>(entity))
            {
                auto& structure = world.GetComponent<StructureComponent>(entity);
                structure.hp = health.fCurrent;
                structure.maxHp = health.fMaximum;
            }
        }

        if (health.bIsDead)
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
