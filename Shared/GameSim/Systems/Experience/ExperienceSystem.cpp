#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"

#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Core/Ecs/CoreComponents.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/GoldRewardDef.h"
#include "Shared/GameSim/Definitions/MinionCombatDef.h"
#include "Shared/GameSim/Feedback/GameplayFeedbackQueue.h"
#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <vector>

namespace
{
    // P1: 보상 정의 누락은 "킬이 아무것도 안 줌"으로만 나타난다 — 등록 누락을 가시화.
    void LogRewardMiss(const char* pWhat)
    {
        static u32_t s_rewardMissLogCount = 0;
        if (s_rewardMissLogCount >= 16u)
            return;
        char msg[112]{};
        sprintf_s(msg, "[Data] reward miss %s -> no gold/xp granted\n", pWhat);
        WintersOutputAIDebugStringA(msg);
        ++s_rewardMissLogCount;
    }

    constexpr f32_t kFallbackNearbyExperienceShareRadius = 20.f;

    eMinionRewardKind ResolveMinionRewardKind(u8_t roleType)
    {
        if (roleType == 1)
            return eMinionRewardKind::Ranged;
        if (roleType == 2)
            return eMinionRewardKind::Siege;
        if (roleType == 3)
            return eMinionRewardKind::Super;
        return eMinionRewardKind::Melee;
    }

    u32_t RoundRewardGold(f32_t amount)
    {
        if (amount <= 0.f)
            return 0u;
        return static_cast<u32_t>(amount + 0.5f);
    }

    f32_t ResolveExperienceShareRadius(const RewardDef& reward)
    {
        return (reward.experience.shareRadius > 0.f)
            ? reward.experience.shareRadius : kFallbackNearbyExperienceShareRadius;
    }

    bool_t IsLivingChampionRecipient(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<ChampionComponent>(entity) ||
            !world.HasComponent<ExperienceComponent>(entity))
            return false;

        if (world.HasComponent<HealthComponent>(entity))
        {
            const auto& health = world.GetComponent<HealthComponent>(entity);
            if (health.bIsDead || health.fCurrent <= 0.f)
                return false;
        }
        else if (world.GetComponent<ChampionComponent>(entity).hp <= 0.f)
            return false;
        return true;
    }

    bool_t TryResolveExperienceOrigin(CWorld& world, EntityID victim, Vec3& outOrigin)
    {
        if (victim == NULL_ENTITY ||
            !world.IsAlive(victim) ||
            !world.HasComponent<TransformComponent>(victim))
            return false;

        outOrigin = world.GetComponent<TransformComponent>(victim).GetPosition();
        return true;
    }

    bool_t TryResolveRewardTeam(CWorld& world, EntityID source, eTeam& outTeam)
    {
        if (source == NULL_ENTITY || !world.IsAlive(source))
            return false;

        if (world.HasComponent<ChampionComponent>(source))
        {
            outTeam = world.GetComponent<ChampionComponent>(source).team;
            return outTeam != eTeam::Neutral;
        }
        if (world.HasComponent<MinionComponent>(source))
        {
            outTeam = world.GetComponent<MinionComponent>(source).team;
            return outTeam != eTeam::Neutral;
        }
        if (world.HasComponent<MinionStateComponent>(source))
        {
            outTeam = world.GetComponent<MinionStateComponent>(source).team;
            return outTeam != eTeam::Neutral;
        }
        if (world.HasComponent<TurretComponent>(source))
        {
            outTeam = world.GetComponent<TurretComponent>(source).team;
            return outTeam != eTeam::Neutral;
        }
        if (world.HasComponent<StructureComponent>(source))
        {
            outTeam = world.GetComponent<StructureComponent>(source).team;
            return outTeam != eTeam::Neutral;
        }

        return false;
    }

    bool_t IsWithinExperienceRange(const Vec3& origin, const Vec3& position, f32_t radius)
    {
        const f32_t dx = position.x - origin.x;
        const f32_t dz = position.z - origin.z;
        return dx * dx + dz * dz <= radius * radius;
    }

    std::vector<EntityID> CollectNearbyExperienceRecipients(
        CWorld& world, eTeam rewardTeam, EntityID fallbackRecipient, EntityID victim, f32_t radius)
    {
        std::vector<EntityID> recipients;
        if (rewardTeam == eTeam::Neutral)
            return recipients;

        Vec3 origin{};
        if (!TryResolveExperienceOrigin(world, victim, origin))
        {
            if (IsLivingChampionRecipient(world, fallbackRecipient) &&
                world.GetComponent<ChampionComponent>(fallbackRecipient).team == rewardTeam)
            {
                recipients.push_back(fallbackRecipient);
            }
            return recipients;
        }

        world.ForEach<ChampionComponent, TransformComponent>(
            [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
            {
                if (entity == victim)
                    return;
                if (champion.team != rewardTeam)
                    return;
                if (!IsLivingChampionRecipient(world, entity))
                    return;
                if (!IsWithinExperienceRange(origin, transform.GetPosition(), radius))
                    return;
                recipients.push_back(entity);
            }
        );
        return recipients;
    }

    void GrantExperienceToRecipients(
        CWorld& world, const std::vector<EntityID>& recipients, f32_t amount)
    {
        for (EntityID recipient : recipients)
            CExperienceSystem::GrantExperience(world, recipient, amount);
    }

    void GrantMinionExperienceToRecipients(
        CWorld& world,
        const std::vector<EntityID>& recipients,
        const RewardDef& reward)
    {
        if (recipients.empty())
            return;

        const f32_t totalPool = recipients.size() == 1u
            ? reward.experience.nearbyXP
            : reward.experience.teamXP;
        const f32_t perRecipient =
            totalPool / static_cast<f32_t>(recipients.size());
        for (EntityID recipient : recipients)
            CExperienceSystem::GrantExperience(world, recipient, perRecipient);
    }
}

void CExperienceSystem::InitializeChampionExperience(CWorld& world, EntityID entity, u8_t level)
{
    if (entity == NULL_ENTITY || !world.IsAlive(entity))
        return;

    ExperienceComponent xp{};
    xp.level = (level > 0) ? level : 1;
    xp.requiredForNextLevel = CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(xp.level);

    if (world.HasComponent<ExperienceComponent>(entity))
        world.GetComponent<ExperienceComponent>(entity) = xp;
    else
        world.AddComponent<ExperienceComponent>(entity, xp);
}

void CExperienceSystem::GrantExperience(CWorld& world, EntityID entity, f32_t amount)
{
    if (entity == NULL_ENTITY ||
        amount <= 0.f ||
        !world.IsAlive(entity) ||
        !world.HasComponent<ExperienceComponent>(entity))
    {
        return;
    }

    auto& xp = world.GetComponent<ExperienceComponent>(entity);
    xp.current += amount;
    xp.total += amount;

    bool_t bLeveled = false;
    while (xp.level < ChampionExperienceCurveDef::kMaxChampionLevel)
    {
        if (xp.requiredForNextLevel <= 0.f)
            xp.requiredForNextLevel =
                CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(xp.level);
        if (xp.requiredForNextLevel <= 0.f || xp.current < xp.requiredForNextLevel)
            break;

        xp.current -= xp.requiredForNextLevel;
        ++xp.level;
        xp.requiredForNextLevel =
            CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(xp.level);
        bLeveled = true;
    }

    if (xp.level >= ChampionExperienceCurveDef::kMaxChampionLevel)
    {
        xp.current = 0.f;
        xp.requiredForNextLevel = 0.f;
    }

    if (world.HasComponent<ChampionComponent>(entity))
        world.GetComponent<ChampionComponent>(entity).level = xp.level;

    if (world.HasComponent<StatComponent>(entity))
    {
        auto& stat = world.GetComponent<StatComponent>(entity);
        if (stat.level != xp.level)
        {
            stat.level = xp.level;
            stat.bDirty = true;
        }
    }
    if (bLeveled && world.HasComponent<SkillRankComponent>(entity))
        CSkillRankSystem::SyncPointsForLevel(world.GetComponent<SkillRankComponent>(entity), xp.level);
}

void CExperienceSystem::GrantChampionLevels(
    CWorld& world,
    EntityID entity,
    u8_t levels)
{
    if (entity == NULL_ENTITY || levels == 0u || !world.IsAlive(entity) ||
        !world.HasComponent<ExperienceComponent>(entity))
    {
        return;
    }

    auto& xp = world.GetComponent<ExperienceComponent>(entity);
    const u8_t oldLevel = xp.level;
    xp.level = static_cast<u8_t>((std::min)(
        static_cast<u32_t>(ChampionExperienceCurveDef::kMaxChampionLevel),
        static_cast<u32_t>(xp.level) + static_cast<u32_t>(levels)));
    xp.requiredForNextLevel = xp.level < ChampionExperienceCurveDef::kMaxChampionLevel
        ? CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(xp.level)
        : 0.f;
    if (xp.level >= ChampionExperienceCurveDef::kMaxChampionLevel)
        xp.current = 0.f;

    if (ChampionComponent* champion = world.TryGetComponent<ChampionComponent>(entity))
        champion->level = xp.level;
    if (StatComponent* stat = world.TryGetComponent<StatComponent>(entity))
    {
        stat->level = xp.level;
        stat->bDirty = stat->bDirty || oldLevel != xp.level;
    }
    if (oldLevel != xp.level && world.HasComponent<SkillRankComponent>(entity))
        CSkillRankSystem::SyncPointsForLevel(world.GetComponent<SkillRankComponent>(entity), xp.level);
}

void CExperienceSystem::GrantKillRewards(CWorld& world, const TickContext& tc,
    EntityID killer, EntityID victim)
{
    if (killer == NULL_ENTITY ||
        victim == NULL_ENTITY ||
        killer == victim ||
        !world.IsAlive(killer) ||
        !world.IsAlive(victim))
    {
        return;
    }

    eTeam rewardTeam = eTeam::Neutral;
    if (!TryResolveRewardTeam(world, killer, rewardTeam))
        return;

    if (world.HasComponent<MinionComponent>(victim))
    {
        const auto& minion = world.GetComponent<MinionComponent>(victim);
        // Champion summons use the minion runtime for movement/targeting but
        // must not inherit lane-minion gold or experience rewards.
        if (minion.roleType == kGameSimMinionRoleTibbers)
            return;
        const RewardDef* reward = CRewardRegistry::Instance().FindReward(
            eRewardSourceKind::Minion,
            static_cast<u8_t>(ResolveMinionRewardKind(minion.roleType)));
        if (!reward)
        {
            LogRewardMiss("minion");
            return;
        }

        const u32_t goldAmount = GrantGold(world, killer, reward->gold.killerGold);
        (void)GameplayFeedback::EnqueueGoldRewardFeedback(world, tc, killer, victim, goldAmount);

        const std::vector<EntityID> recipients = CollectNearbyExperienceRecipients(
            world, rewardTeam, killer, victim, ResolveExperienceShareRadius(*reward));
        GrantMinionExperienceToRecipients(world, recipients, *reward);

        return;
    }

    if (world.HasComponent<TurretAIComponent>(victim))
    {
        const RewardDef* reward = CRewardRegistry::Instance().FindReward(eRewardSourceKind::Turret);
        if (!reward)
        {
            LogRewardMiss("turret-kill");
            return;
        }

        const bool_t bChampionKiller =
            world.HasComponent<ChampionComponent>(killer);
        if (bChampionKiller)
        {
            const u32_t goldAmount =
                GrantGold(world, killer, reward->gold.killerGold);
            (void)GameplayFeedback::EnqueueGoldRewardFeedback(
                world, tc, killer, victim, goldAmount);
        }

        world.ForEach<ChampionComponent>(
            [&](EntityID entity, ChampionComponent& champion)
            {
                if (champion.team != rewardTeam ||
                    (bChampionKiller && entity == killer))
                {
                    return;
                }

                const u32_t goldAmount =
                    GrantGold(world, entity, reward->gold.teamGold);
                (void)GameplayFeedback::EnqueueGoldRewardFeedback(
                    world, tc, entity, victim, goldAmount);
            });
        return;
    }

    if (world.HasComponent<JungleComponent>(victim))
    {
        const auto& jungle = world.GetComponent<JungleComponent>(victim);
        const EconomyGameplayDef fallbackEconomy{};
        const EconomyGameplayDef* economy = tc.pDefinitions
            ? tc.pDefinitions->FindEconomy() : nullptr;
        if (!economy)
            economy = &fallbackEconomy;

        if (jungle.subKind == 0u || jungle.subKind == 1u)
        {
            const eObjectiveBuffKind buffKind = jungle.subKind == 0u
                ? eObjectiveBuffKind::Baron : eObjectiveBuffKind::Elder;
            const auto recipients =
                DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
            for (EntityID entity : recipients)
            {
                const ChampionComponent& champion =
                    world.GetComponent<ChampionComponent>(entity);
                if (champion.team != rewardTeam ||
                    world.HasComponent<PracticeSpawnedTag>(entity) ||
                    !world.HasComponent<GoldComponent>(entity) ||
                    !world.HasComponent<ExperienceComponent>(entity))
                {
                    continue;
                }

                const u32_t goldAmount = GrantGold(
                    world, entity, economy->objectives.teamGoldPerChampion);
                (void)GameplayFeedback::EnqueueGoldRewardFeedback(
                    world, tc, entity, victim, goldAmount);
                GrantChampionLevels(world, entity, economy->objectives.teamLevelGrant);
                if (IsLivingChampionRecipient(world, entity))
                    CBuffSystem::AddOrRefreshObjectiveBuff(world, entity, buffKind, tc);
            }
            return;
        }

        const u32_t goldAmount = GrantGold(world, killer, economy->jungle.smallCampGold);
        (void)GameplayFeedback::EnqueueGoldRewardFeedback(world, tc, killer, victim, goldAmount);
        GrantExperience(world, killer, economy->jungle.smallCampXP);
        if (IsLivingChampionRecipient(world, killer))
        {
            if (jungle.subKind == 2u)
                CBuffSystem::AddOrRefreshObjectiveBuff(world, killer, eObjectiveBuffKind::Blue, tc);
            else if (jungle.subKind == 3u)
                CBuffSystem::AddOrRefreshObjectiveBuff(world, killer, eObjectiveBuffKind::Red, tc);
        }

        return;
    }

    if (world.HasComponent<ChampionComponent>(victim))
    {
        const RewardDef* reward = CRewardRegistry::Instance().FindReward(eRewardSourceKind::Champion);
        if (!reward)
        {
            LogRewardMiss("champion-kill");
            return;
        }

        const u32_t goldAmount = GrantGold(world, killer, reward->gold.killerGold);
        (void)GameplayFeedback::EnqueueGoldRewardFeedback(world, tc, killer, victim, goldAmount);

        const std::vector<EntityID> recipients = CollectNearbyExperienceRecipients(
            world, rewardTeam, killer, victim, ResolveExperienceShareRadius(*reward));

        GrantExperienceToRecipients(world, recipients, ResolveChampionKillExperience(world, victim));
    }
}

f32_t CExperienceSystem::ResolveChampionKillExperience(CWorld& world, EntityID victim)
{
    const RewardDef* reward = CRewardRegistry::Instance().FindReward(eRewardSourceKind::Champion);
    if (!reward)
    {
        LogRewardMiss("champion-kill-xp");
        return 0.f;
    }

    u8_t victimLevel = 1;
    f32_t nextLevelXp = 0.f;
    if (world.HasComponent<ExperienceComponent>(victim))
    {
        const auto& xp = world.GetComponent<ExperienceComponent>(victim);
        victimLevel = xp.level;
        nextLevelXp = xp.requiredForNextLevel;
    }
    else if (world.HasComponent<ChampionComponent>(victim))
    {
        victimLevel = world.GetComponent<ChampionComponent>(victim).level;
    }

    if (nextLevelXp <= 0.f)
        nextLevelXp = CRewardRegistry::Instance().GetRequiredExperienceForNextLevel(victimLevel);

    return (std::max)(0.f, nextLevelXp * reward->experience.victimNextLevelXPFactor);
}

u32_t CExperienceSystem::GrantGold(CWorld& world, EntityID entity, f32_t amount)
{
    if (entity == NULL_ENTITY ||
        amount <= 0.f ||
        !world.IsAlive(entity) ||
        !world.HasComponent<GoldComponent>(entity))
    {
        return 0u;
    }

    const u32_t goldAmount = RoundRewardGold(amount);
    if (goldAmount == 0u)
        return 0u;

    world.GetComponent<GoldComponent>(entity).amount += goldAmount;
    return goldAmount;
}
