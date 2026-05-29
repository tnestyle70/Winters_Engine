#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
// Viego Soul Spawn
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"

namespace
{
    constexpr u32_t kJungleSubBaron = 0u;
    constexpr u32_t kJungleSubDragon = 1u;

    eKillFeedObjectKind ResolveKillFeedObjectKind(CWorld& world, EntityID target)
    {
        if (world.HasComponent<ChampionComponent>(target))
            return eKillFeedObjectKind::Champion;

        if (world.HasComponent<JungleComponent>(target))
        {
            const JungleComponent& jungle = world.GetComponent<JungleComponent>(target);
            if (jungle.subKind == kJungleSubBaron)
                return eKillFeedObjectKind::Baron;
            if (jungle.subKind == kJungleSubDragon)
                return eKillFeedObjectKind::Dragon;
            return eKillFeedObjectKind::None;
        }

        if (!world.HasComponent<StructureComponent>(target))
            return eKillFeedObjectKind::None;

        const StructureComponent& structure = world.GetComponent<StructureComponent>(target);
        if (structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret))
            return eKillFeedObjectKind::Turret;
        if (structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Inhibitor))
            return eKillFeedObjectKind::Inhibitor;

        return eKillFeedObjectKind::None;
    }

    void TryEnqueueKillFeedEvent(CWorld& world, const TickContext& tc,
        const DamageRequest& request)
    {
        if (request.source == NULL_ENTITY || request.target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(request.source))
            return;

        const eKillFeedObjectKind objectKind =
            ResolveKillFeedObjectKind(world, request.target);
        if (objectKind == eKillFeedObjectKind::None)
            return;

        const ChampionComponent& sourceChampion =
            world.GetComponent<ChampionComponent>(request.source);

        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::KillFeed;
        event.sourceEntity = request.source;
        event.targetEntity = request.target;
        event.sourceChampion = sourceChampion.id;
        event.sourceTeam = static_cast<u8_t>(sourceChampion.team);
        event.killFeedObjectKind = objectKind;
        event.startTick = tc.tickIndex;

        if (world.HasComponent<ChampionComponent>(request.target))
        {
            const ChampionComponent& targetChampion =
                world.GetComponent<ChampionComponent>(request.target);
            event.targetChampion = targetChampion.id;
            event.targetTeam = static_cast<u8_t>(targetChampion.team);
        }
        else if (world.HasComponent<StructureComponent>(request.target))
        {
            const StructureComponent& structure =
                world.GetComponent<StructureComponent>(request.target);
            event.targetTeam = static_cast<u8_t>(structure.team);
        }
        else if (world.HasComponent<JungleComponent>(request.target))
        {
            event.targetTeam = static_cast<u8_t>(eTeam::Neutral);
        }

        EnqueueReplicatedEvent(world, event);
    }
}

void CDamageQueueSystem::Execute(CWorld& world, const TickContext& tc)
{
    const auto requests = DeterministicEntityIterator<DamageRequestComponent>::CollectSorted(world);
    for (EntityID entity : requests)
    {
        if (!world.IsAlive(entity) || !world.HasComponent<DamageRequestComponent>(entity))
            continue;

        const DamageRequest request = world.GetComponent<DamageRequestComponent>(entity);
        const DamageResult result = ApplyDamageRequest(world, tc, request);
        if (result.finalAmount > 0.f && request.target != NULL_ENTITY)
        {
            ReplicatedEventComponent event{};
            event.kind = eReplicatedEventKind::Damage;
            event.sourceEntity = request.source;
            event.targetEntity = request.target;
            event.amount = result.finalAmount;
            event.damageType = request.type;
            event.bWasCrit = result.bWasCrit;
            event.bKilled = result.bKilled;
            event.skillId = request.skillId;
            event.flags = static_cast<u16_t>(request.flags & 0xffffu);
            event.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(world, event);
        }

        if (result.bKilled &&
            request.source != NULL_ENTITY &&
            request.target != NULL_ENTITY)
        {
            TryEnqueueKillFeedEvent(world, tc, request);
            ViegoGameSim::TrySpawnSoulForKill(world, tc, request.source, request.target);
            CExperienceSystem::GrantKillRewards(world, request.source, request.target);
        }

        if (result.finalAmount > 0.f &&
            request.source != NULL_ENTITY &&
            request.target != NULL_ENTITY &&
            world.HasComponent<ChampionComponent>(request.source) &&
            world.HasComponent<ChampionComponent>(request.target))
        {
            TowerAggroNotifyComponent notify{};
            notify.attackerEntity = request.source;
            notify.victimEntity = request.target;
            notify.priorityDuration = 2.0f;

            const EntityID notifyEntity = world.CreateEntity();
            world.AddComponent<TowerAggroNotifyComponent>(notifyEntity, notify);
        }

        world.DestroyEntity(entity);
    }
}
