#include "Shared/GameSim/Systems/Damage/DamageQueueSystem.h"

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/ChampionAssistCredit.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
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

    constexpr u64_t kAssistCreditWindowTicks = DeterministicTime::kTicksPerSecond * 10ull;

    ChampionScoreComponent& EnsureChampionScore(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<ChampionScoreComponent>(entity))
            world.AddComponent<ChampionScoreComponent>(entity, ChampionScoreComponent{});
        return world.GetComponent<ChampionScoreComponent>(entity);
    }

    MatchScoreComponent& EnsureMatchScore(CWorld& world)
    {
        MatchScoreComponent* pScore = nullptr;
        world.ForEach<MatchScoreComponent>(
            [&](EntityID, MatchScoreComponent& score)
            {
                if (!pScore)
                    pScore = &score;
            }
        );
        if (pScore)
            return *pScore;

        return world.AddComponent<MatchScoreComponent>(
            world.CreateEntity(),
            MatchScoreComponent{});
    }

    TeamScoreState* ResolveTeamScore(MatchScoreComponent& score, eTeam team)
    {
        if (team == eTeam::Blue)
            return &score.Blue;
        if (team == eTeam::Red)
            return &score.Red;
        return nullptr;
    }

    void RecordChampionAssistCredit(CWorld& world, const TickContext& tc,
        const DamageRequest& request)
    {
        if (request.source == NULL_ENTITY || request.target == NULL_ENTITY ||
            request.source == request.target ||
            !world.HasComponent<ChampionComponent>(request.source) ||
            !world.HasComponent<ChampionComponent>(request.target))
        {
            return;
        }

        const ChampionComponent& sourceChampion =
            world.GetComponent<ChampionComponent>(request.source);
        const ChampionComponent& targetChampion =
            world.GetComponent<ChampionComponent>(request.target);
        if (sourceChampion.team == targetChampion.team)
            return;

        if (!world.HasComponent<ChampionAssistCreditComponent>(request.target))
            world.AddComponent<ChampionAssistCreditComponent>(
                request.target,
                ChampionAssistCreditComponent{});

        ChampionAssistCreditComponent& credits =
            world.GetComponent<ChampionAssistCreditComponent>(request.target);

        ChampionAssistCreditComponent::Credit* pEmpty = nullptr;
        ChampionAssistCreditComponent::Credit* pOldest = &credits.Credits[0];
        for (ChampionAssistCreditComponent::Credit& credit : credits.Credits)
        {
            if (credit.SourceEntity == request.source)
            {
                credit.iLastDamageTick = tc.tickIndex;
                credit.iSourceTeam = static_cast<u8_t>(sourceChampion.team);
                return;
            }

            if (credit.SourceEntity == NULL_ENTITY && !pEmpty)
                pEmpty = &credit;
            if (credit.iLastDamageTick < pOldest->iLastDamageTick)
                pOldest = &credit;
        }

        ChampionAssistCreditComponent::Credit& slot = pEmpty ? *pEmpty : *pOldest;
        slot.SourceEntity = request.source;
        slot.iLastDamageTick = tc.tickIndex;
        slot.iSourceTeam = static_cast<u8_t>(sourceChampion.team);
    }

    void ApplyScoreForKill(CWorld& world, const TickContext& tc,
        const DamageRequest& request)
    {
        if (request.source == NULL_ENTITY || request.target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(request.source))
        {
            return;
        }

        const ChampionComponent& sourceChampion =
            world.GetComponent<ChampionComponent>(request.source);
        MatchScoreComponent& matchScore = EnsureMatchScore(world);
        TeamScoreState* pSourceTeamScore =
            ResolveTeamScore(matchScore, sourceChampion.team);

        if (world.HasComponent<ChampionComponent>(request.target))
        {
            ChampionScoreComponent& sourceScore =
                EnsureChampionScore(world, request.source);
            ChampionScoreComponent& targetScore =
                EnsureChampionScore(world, request.target);

            ++sourceScore.iKills;
            ++targetScore.iDeaths;

            if (pSourceTeamScore)
                ++pSourceTeamScore->iTotalKills;

            if (world.HasComponent<ChampionAssistCreditComponent>(request.target))
            {
                ChampionAssistCreditComponent& credits =
                    world.GetComponent<ChampionAssistCreditComponent>(request.target);

                for (ChampionAssistCreditComponent::Credit& credit : credits.Credits)
                {
                    if (credit.SourceEntity == NULL_ENTITY ||
                        credit.SourceEntity == request.source ||
                        !world.IsAlive(credit.SourceEntity) ||
                        !world.HasComponent<ChampionComponent>(credit.SourceEntity))
                    {
                        credit = {};
                        continue;
                    }

                    const ChampionComponent& assistChampion =
                        world.GetComponent<ChampionComponent>(credit.SourceEntity);
                    const bool_t bSameTeam = assistChampion.team == sourceChampion.team;
                    const bool_t bRecent =
                        tc.tickIndex >= credit.iLastDamageTick &&
                        tc.tickIndex - credit.iLastDamageTick <= kAssistCreditWindowTicks;
                    if (bSameTeam && bRecent)
                        ++EnsureChampionScore(world, credit.SourceEntity).iAssists;

                    credit = {};
                }
            }
            return;
        }

        if (world.HasComponent<StructureComponent>(request.target))
        {
            const StructureComponent& structure =
                world.GetComponent<StructureComponent>(request.target);
            if (structure.kind == static_cast<u32_t>(Winters::Map::eObjectKind::Structure_Turret) &&
                pSourceTeamScore)
            {
                ++pSourceTeamScore->iDestroyedTurrets;
            }
        }

        if (world.HasComponent<JungleComponent>(request.target) && pSourceTeamScore)
        {
            const JungleComponent& jungle =
                world.GetComponent<JungleComponent>(request.target);
            if (jungle.subKind == kJungleSubDragon)
                ++pSourceTeamScore->iDragons;
            else if (jungle.subKind == kJungleSubBaron)
                ++pSourceTeamScore->iBarons;
        }
    }

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

    void NotifyJungleAggroFromChampionDamage(CWorld& world, const DamageRequest& request)
    {
        if (request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY ||
            request.source == request.target ||
            !world.HasComponent<JungleComponent>(request.target) ||
            !world.HasComponent<ChampionComponent>(request.source))
        {
            return;
        }

        auto& ai = world.HasComponent<JungleAIComponent>(request.target)
            ? world.GetComponent<JungleAIComponent>(request.target)
            : world.AddComponent<JungleAIComponent>(request.target, JungleAIComponent{});

        ai.target = request.source;
        ai.bAggro = true;
    }

    bool_t TryMarkChampionDeathCredit(CWorld& world, EntityID target)
    {
        if (target != NULL_ENTITY &&
            world.HasComponent<ViegoSoulComponent>(target))
        {
            return false;
        }

        if (target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<RespawnComponent>(target))
        {
            return true;
        }

        RespawnComponent& respawn = world.GetComponent<RespawnComponent>(target);
        if (respawn.bDeathCredited)
            return false;

        respawn.bDeathCredited = true;
        return true;
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

            RecordChampionAssistCredit(world, tc, request);
            NotifyJungleAggroFromChampionDamage(world, request);
        }

        if (result.bKilled &&
            request.source != NULL_ENTITY &&
            request.target != NULL_ENTITY &&
            TryMarkChampionDeathCredit(world, request.target))
        {
            ApplyScoreForKill(world, tc, request);
            TryEnqueueKillFeedEvent(world, tc, request);
            ViegoGameSim::TrySpawnSoulForKill(world, tc, request.source, request.target);
            CExperienceSystem::GrantKillRewards(world, tc, request.source, request.target);
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
