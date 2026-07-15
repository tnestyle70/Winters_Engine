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
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/Shield/ShieldSystem.h"
// Viego Soul Spawn
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Definitions/MapDataFormats.h"

namespace
{
    constexpr u32_t kJungleSubBaron = 0u;
    constexpr u32_t kJungleSubDragon = 1u;

    constexpr u64_t kAssistCreditWindowTicks = DeterministicTime::kTicksPerSecond * 10ull;

    u64_t ResolveAssistCreditWindowTicks(const TickContext& tc)
    {
        if (const EconomyGameplayDef* pEconomy =
            tc.pDefinitions ? tc.pDefinitions->FindEconomy() : nullptr)
        {
            return static_cast<u64_t>(
                pEconomy->assistCreditWindowSec *
                static_cast<f32_t>(DeterministicTime::kTicksPerSecond) + 0.5f);
        }
        return kAssistCreditWindowTicks;
    }

    bool_t IsYasuoPassiveShieldReady(CWorld& world, EntityID target)
    {
        if (target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<YasuoStateComponent>(target))
        {
            return false;
        }

        const ChampionComponent& champion =
            world.GetComponent<ChampionComponent>(target);
        const YasuoStateComponent& state =
            world.GetComponent<YasuoStateComponent>(target);
        return champion.id == eChampion::YASUO &&
            state.fPassiveShieldRemaining <= 0.f &&
            state.fPassiveFlowMax > 0.f &&
            state.fPassiveFlow >= state.fPassiveFlowMax;
    }

    void EnqueueYasuoPassiveShieldVisual(
        CWorld& world,
        const TickContext& tc,
        EntityID target)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.effectId = MakeGameplayHookId(
            eChampion::YASUO,
            GameplayHookVariant::Passive_Trigger);
        event.sourceEntity = target;
        event.targetEntity = target;
        event.sourceChampion = eChampion::YASUO;
        event.slot = static_cast<u8_t>(eSkillSlot::W);
        event.flags = static_cast<u16_t>(eSkillSlot::W);
        event.rank = 1u;
        event.startTick = tc.tickIndex;
        event.durationMs = 3000u;
        if (world.HasComponent<ChampionComponent>(target))
        {
            event.sourceTeam = static_cast<u8_t>(
                world.GetComponent<ChampionComponent>(target).team);
        }
        EnqueueReplicatedEvent(world, event);
    }

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

            // 퍼스트 블러드: 양 팀 통산 킬 0에서의 첫 챔피언 킬에만 지급.
            const bool_t bFirstBlood =
                matchScore.Blue.iTotalKills == 0u && matchScore.Red.iTotalKills == 0u;

            ++sourceScore.iKills;
            ++targetScore.iDeaths;

            if (pSourceTeamScore)
                ++pSourceTeamScore->iTotalKills;

            if (const RewardDef* pChampionReward =
                CRewardRegistry::Instance().FindReward(eRewardSourceKind::Champion))
            {
                if (bFirstBlood)
                {
                    (void)CExperienceSystem::GrantGold(
                        world, request.source,
                        pChampionReward->gold.firstBloodBonusGold);
                }
            }

            if (world.HasComponent<ChampionAssistCreditComponent>(request.target))
            {
                const u64_t assistWindowTicks = ResolveAssistCreditWindowTicks(tc);
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
                        tc.tickIndex - credit.iLastDamageTick <= assistWindowTicks;
                    if (bSameTeam && bRecent)
                    {
                        ++EnsureChampionScore(world, credit.SourceEntity).iAssists;
                        if (const RewardDef* pChampionReward =
                            CRewardRegistry::Instance().FindReward(eRewardSourceKind::Champion))
                        {
                            (void)CExperienceSystem::GrantGold(
                                world, credit.SourceEntity,
                                pChampionReward->gold.assistGold);
                        }
                    }

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
        if (request.source == NULL_ENTITY || request.target == NULL_ENTITY)
            return;

        const eKillFeedObjectKind objectKind =
            ResolveKillFeedObjectKind(world, request.target);
        if (objectKind == eKillFeedObjectKind::None)
            return;

        // 챔피언/정글 킬은 챔피언 막타만 공지한다.
        // 구조물 파괴는 미니언 막타가 일상 경로이므로 킬러가 챔피언이 아니어도 발행한다
        // (클라이언트 파괴 배너/연출이 이 이벤트 하나에 걸려 있다).
        const bool_t bChampionSource =
            world.HasComponent<ChampionComponent>(request.source);
        const bool_t bStructureKill =
            objectKind == eKillFeedObjectKind::Turret ||
            objectKind == eKillFeedObjectKind::Inhibitor;
        if (!bChampionSource && !bStructureKill)
            return;

        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::KillFeed;
        event.sourceEntity = request.source;
        event.targetEntity = request.target;
        if (bChampionSource)
        {
            const ChampionComponent& sourceChampion =
                world.GetComponent<ChampionComponent>(request.source);
            event.sourceChampion = sourceChampion.id;
            event.sourceTeam = static_cast<u8_t>(sourceChampion.team);
        }
        else
        {
            event.sourceTeam = static_cast<u8_t>(
                GameplayStateQuery::ResolveEntityTeam(world, request.source));
        }
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
    CShieldSystem::Execute(world, tc);

    const auto requests = DeterministicEntityIterator<DamageRequestComponent>::CollectSorted(world);
    for (EntityID entity : requests)
    {
        if (!world.IsAlive(entity) || !world.HasComponent<DamageRequestComponent>(entity))
            continue;

        const DamageRequest request = world.GetComponent<DamageRequestComponent>(entity);
        const bool_t bYasuoPassiveShieldReady =
            IsYasuoPassiveShieldReady(world, request.target);
        const DamageResult result = ApplyDamageRequest(world, tc, request);
        if (bYasuoPassiveShieldReady && result.bWasShielded)
            EnqueueYasuoPassiveShieldVisual(world, tc, request.target);

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
