#include "ECS/World.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "GameObject/ChampionDef.h"
#include "Shared/GameSim/Definitions/ChampionStatsDef.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/Damage/Damage.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/PendingHit/PendingHitSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include <type_traits>

namespace
{
    constexpr bool CloseEnough(f32_t lhs, f32_t rhs, f32_t epsilon)
    {
        return (lhs > rhs ? lhs - rhs : rhs - lhs) <= epsilon;
    }

    constexpr f32_t TestLevel18Ad()
    {
        return 60.f + 17.f * 3.5f;
    }

    constexpr f32_t TestArmorDamage()
    {
        return 100.f * (100.f / (100.f + 30.f));
    }

    static_assert(DeterministicTime::kTicksPerSecond == 30);
    static_assert(DeterministicTime::kFixedDt > 0.f);
    static_assert(CloseEnough(TestLevel18Ad(), 119.5f, 0.001f));
    static_assert(CloseEnough(TestArmorDamage(), 76.92308f, 0.001f));
    static_assert(std::is_trivially_copyable_v<StatComponent>);
    static_assert(std::is_standard_layout_v<StatComponent>);
    static_assert(std::is_trivially_copyable_v<DamageRequest>);
    static_assert(std::is_standard_layout_v<DamageRequest>);
    static_assert(std::is_trivially_copyable_v<SkillRankComponent>);
    static_assert(std::is_standard_layout_v<SkillRankComponent>);

    void CompileSim2MilestonePaths()
    {
        CWorld world;

        ChampionStatsDef stats{};
        stats.championId = eChampion::EZREAL;
        stats.baseHp = 600.f;
        stats.hpPerLevel = 100.f;
        stats.baseAd = 60.f;
        stats.adPerLevel = 3.5f;
        stats.baseArmor = 30.f;
        stats.armorPerLevel = 4.f;
        CChampionStatsRegistry::Instance().Add(eChampion::EZREAL, stats);

        EntityID source = world.CreateEntity();
        EntityID target = world.CreateEntity();

        ChampionComponent sourceChampion{};
        sourceChampion.id = eChampion::EZREAL;
        sourceChampion.team = eTeam::Blue;
        world.AddComponent<ChampionComponent>(source, sourceChampion);

        ChampionComponent targetChampion{};
        targetChampion.id = eChampion::EZREAL;
        targetChampion.team = eTeam::Red;
        targetChampion.hp = 1000.f;
        targetChampion.maxHp = 1000.f;
        world.AddComponent<ChampionComponent>(target, targetChampion);
        world.AddComponent<HealthComponent>(target, HealthComponent{ 1000.f, 1000.f, false });

        StatComponent sourceStat{};
        sourceStat.championId = eChampion::EZREAL;
        sourceStat.level = 18;
        sourceStat.bDirty = true;
        world.AddComponent<StatComponent>(source, sourceStat);

        BuffComponent buffs{};
        BuffInstance conqueror{};
        conqueror.buffDefId = 1;
        conqueror.source = source;
        conqueror.fDurationRemaining = 5.f;
        conqueror.stackCount = 5;
        conqueror.flatAdPerStack = 2.f;
        CBuffSystem::AddOrRefresh(buffs, conqueror);
        world.AddComponent<BuffComponent>(source, buffs);
        CStatSystem::Execute(world);

        StatComponent targetStat{};
        targetStat.championId = eChampion::EZREAL;
        targetStat.level = 1;
        targetStat.armor = 30.f;
        targetStat.bDirty = false;
        world.AddComponent<StatComponent>(target, targetStat);

        SkillRankComponent ranks{};
        CSkillRankSystem::SyncPointsForLevel(ranks, 18);
        for (u8_t i = 0; i < 5; ++i)
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 1u);
        for (u8_t i = 0; i < 5; ++i)
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 2u);
        for (u8_t i = 0; i < 5; ++i)
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 3u);
        for (u8_t i = 0; i < 3; ++i)
            (void)CSkillRankSystem::TryLevelSkill(ranks, 18u, 4u);

        TickContext tick{};
        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = eTeam::Blue;
        request.type = eDamageType::Physical;
        request.skillId = 1201;
        request.rank = 1;
        (void)ApplyDamageRequest(world, tick, request);
    }
}
