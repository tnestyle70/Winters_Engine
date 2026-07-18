#include "Shared/GameSim/Spawn/ChampionGameplayAssembly.h"

#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"
#include "Shared/GameSim/Definitions/SkillTypes.h"
#include "Shared/GameSim/Definitions/WardDefinitions.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>

namespace
{
    void AssignDefaultBotSkillRanks(SkillRankComponent& ranks, u8_t championLevel)
    {
        ranks = SkillRankComponent{};
        CSkillRankSystem::SyncPointsForLevel(ranks, championLevel);

        static constexpr u8_t kLevelOrder[] =
        {
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
            static_cast<u8_t>(eSkillSlot::Q),
            static_cast<u8_t>(eSkillSlot::W),
            static_cast<u8_t>(eSkillSlot::E),
            static_cast<u8_t>(eSkillSlot::R),
        };

        const u8_t count = std::min<u8_t>(
            championLevel,
            static_cast<u8_t>(sizeof(kLevelOrder) / sizeof(kLevelOrder[0])));
        for (u8_t i = 0; i < count && ranks.pointsAvailable > 0u; ++i)
        {
            CSkillRankSystem::TryLevelSkill(
                ranks,
                static_cast<u8_t>(i + 1u),
                kLevelOrder[i]);
        }
    }
}

EntityID ChampionGameplayAssembly::Build(CWorld& world, const ChampionAssemblyContext& ctx)
{
    if (!ctx.pDef)
        return NULL_ENTITY;

    const EntityHandle entityHandle = world.CreateEntityHandle();
    const EntityID entity = entityHandle.GetIndex();

    TransformComponent transform{};
    transform.SetPosition(ctx.spawnPos);
    world.AddComponent<TransformComponent>(entity, transform);

    StatComponent stat{};
    ChampionDefinitionComponent identity{};
    identity.championDefId = ctx.pDef->id;
    world.AddComponent<ChampionDefinitionComponent>(entity, identity);

    SkillLoadoutComponent loadout{};
    for (u8_t skillSlot = 0u; skillSlot < kChampionSkillSlotCount; ++skillSlot)
        loadout.skills[skillSlot] = ctx.pDef->skillLoadout[skillSlot];
    world.AddComponent<SkillLoadoutComponent>(entity, loadout);

    stat = CStatSystem::BuildBaseStats(
        ctx.pDef->stats,
        ctx.pDef->legacyChampion,
        ctx.loadout.startLevel);
    if (ctx.maxHpOverride > 0.f)
        stat.hpMax = ctx.maxHpOverride;
    world.AddComponent<StatComponent>(entity, stat);

    HealthComponent health{};
    health.fCurrent = stat.hpMax;
    health.fMaximum = stat.hpMax;
    health.bIsDead = false;
    world.AddComponent<HealthComponent>(entity, health);

    RespawnComponent respawn{};
    respawn.spawnPos = ctx.spawnPos;
    respawn.respawnDelay = ctx.loadout.respawnDelaySec;
    world.AddComponent<RespawnComponent>(entity, respawn);

    SkillStateComponent skillState{};
    world.AddComponent<SkillStateComponent>(entity, skillState);

    CExperienceSystem::InitializeChampionExperience(world, entity, stat.level);

    SkillRankComponent skillRank{};
    if (ctx.bAssignBotSkillRanks)
        AssignDefaultBotSkillRanks(skillRank, stat.level);
    else
        CSkillRankSystem::SyncPointsForLevel(skillRank, stat.level);
    world.AddComponent<SkillRankComponent>(entity, skillRank);

    GoldComponent gold{};
    gold.amount = ctx.loadout.startGold;
    world.AddComponent<GoldComponent>(entity, gold);

    InventoryComponent inventory{};
    constexpr u8_t kDefaultWardSlot = 3u;
    inventory.itemIds[kDefaultWardSlot] = kTrinketWardItemId;
    inventory.count = static_cast<u8_t>(kDefaultWardSlot + 1u);
    if (ctx.champion == eChampion::KALISTA)
    {
        // 칼리스타 서약(Oathsworn) 아이템은 스폰 시 고정 슬롯에 시딩된다 —
        // KalistaBondSystem 이 이 슬롯을 서약 활성 조건으로 읽는다.
        inventory.itemIds[kKalistaOathswornInventorySlot] =
            kKalistaOathswornItemId;
        inventory.count = InventoryComponent::kMaxSlots;
    }
    world.AddComponent<InventoryComponent>(entity, inventory);

    RuneLoadoutComponent runeLoadout{};
    runeLoadout.eRunes[0] = ctx.loadout.startRune;
    runeLoadout.iCount = ctx.loadout.startRuneCount;
    world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
    world.AddComponent<RuneRuntimeComponent>(entity, RuneRuntimeComponent{});

    ChampionScoreComponent score{};
    world.AddComponent<ChampionScoreComponent>(entity, score);

    SummonerSpellStateComponent summonerSpellState{};
    world.AddComponent<SummonerSpellStateComponent>(entity, summonerSpellState);

    ChampionComponent champion{};
    champion.id = ctx.champion;
    champion.team = static_cast<eTeam>(ctx.team);
    champion.hp = health.fCurrent;
    champion.maxHp = health.fMaximum;
    champion.mana = stat.manaMax;
    champion.maxMana = stat.manaMax;
    champion.moveSpeed = stat.moveSpeed;
    champion.level = stat.level;
    world.AddComponent<ChampionComponent>(entity, champion);

    return entity;
}
