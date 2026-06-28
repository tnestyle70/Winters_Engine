#include "Server/Private/Game/Factory/ServerChampionEntityFactory.h"

#include "Game/GameRoom.h"
#include "Server/Private/Game/GameRoomInternal.h"
#include "Server/Private/Game/GameRoomSmokeRoster.h"
#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/World.h"

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
            CSkillRankSystem::TryLevelSkill(ranks, kLevelOrder[i]);
    }
}

VisibilityComponent BuildServerVisibleToAll()
{
    VisibilityComponent visibility{};
    visibility.teamVisibilityMask = static_cast<u8_t>(
        (1u << TeamByte(eTeam::Blue)) |
        (1u << TeamByte(eTeam::Red)));
    return visibility;
}

EntityID ServerEntityFactory::BuildChampionEntity(
    CWorld& world, const LobbySlotState& slot, const Vec3& spawnPos)
{
    const EntityHandle entityHandle = world.CreateEntityHandle();
    const EntityID entity = entityHandle.GetIndex();

    TransformComponent transform{};
    transform.SetPosition(spawnPos);
    world.AddComponent<TransformComponent>(entity, transform);

    const SpawnObjectDefinitionPack& objectDefs = ServerData::GetLoLSpawnObjectDefinitionPack();
    const SpawnLoadoutPolicyDef& spawnPolicy = objectDefs.spawnLoadout;
    const GameplayDefinitionPack& definitions = ServerData::GetLoLGameplayDefinitionPack();
    const ChampionGameplayDef* championDef = definitions.FindChampion(slot.champion);

    StatComponent stat{};
    f32_t spatialRadius = 0.75f;
    f32_t sightRange = 19.f;
    if (championDef)
    {
        ChampionDefinitionComponent identity{};
        identity.championDefId = championDef->id;
        world.AddComponent<ChampionDefinitionComponent>(entity, identity);

        SkillLoadoutComponent loadout{};
        for (u8_t skillSlot = 0u; skillSlot < kChampionSkillSlotCount; ++skillSlot)
            loadout.skills[skillSlot] = championDef->skillLoadout[skillSlot];
        world.AddComponent<SkillLoadoutComponent>(entity, loadout);

        stat = CStatSystem::BuildBaseStats(
            championDef->stats,
            championDef->legacyChampion,
            spawnPolicy.startLevel);
        spatialRadius = championDef->stats.spatialRadius;
        sightRange = championDef->stats.sightRange;
    }
    else
    {
        const ChampionStatsDef legacyStats =
            CChampionStatsRegistry::Instance().Resolve(slot.champion);
        stat = CStatSystem::BuildBaseStats(legacyStats, spawnPolicy.startLevel);
        spatialRadius = legacyStats.spatialRadius;
        sightRange = legacyStats.sightRange;
    }
    stat.hpMax = ResolveServerChampionMaxHpForSlot(slot, stat.hpMax);
    world.AddComponent<StatComponent>(entity, stat);

    HealthComponent health{};
    health.fCurrent = stat.hpMax;
    health.fMaximum = stat.hpMax;
    health.bIsDead = false;
    world.AddComponent<HealthComponent>(entity, health);

    RespawnComponent respawn{};
    respawn.spawnPos = spawnPos;
    respawn.respawnDelay = spawnPolicy.respawnDelaySec;
    world.AddComponent<RespawnComponent>(entity, respawn);

    SkillStateComponent skillState{};
    world.AddComponent<SkillStateComponent>(entity, skillState);

    CExperienceSystem::InitializeChampionExperience(world, entity, stat.level);

    SkillRankComponent skillRank{};
    if (slot.bBot && !slot.bDummy)
        AssignDefaultBotSkillRanks(skillRank, stat.level);
    else
        CSkillRankSystem::SyncPointsForLevel(skillRank, stat.level);
    world.AddComponent<SkillRankComponent>(entity, skillRank);

    GoldComponent gold{};
    gold.amount = spawnPolicy.startGold;
    world.AddComponent<GoldComponent>(entity, gold);

    InventoryComponent inventory{};
    world.AddComponent<InventoryComponent>(entity, inventory);

    RuneLoadoutComponent runeLoadout{};
    runeLoadout.eRunes[0] = spawnPolicy.startRune;
    runeLoadout.iCount = spawnPolicy.startRuneCount;
    world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
    world.AddComponent<RuneRuntimeComponent>(entity, RuneRuntimeComponent{});

    ChampionScoreComponent score{};
    world.AddComponent<ChampionScoreComponent>(entity, score);

    SummonerSpellStateComponent summonerSpellState{};
    world.AddComponent<SummonerSpellStateComponent>(entity, summonerSpellState);

    ChampionComponent champion{};
    champion.id = slot.champion;
    champion.team = static_cast<eTeam>(slot.team);
    champion.hp = health.fCurrent;
    champion.maxHp = health.fMaximum;
    champion.mana = stat.manaMax;
    champion.maxMana = stat.manaMax;
    champion.moveSpeed = stat.moveSpeed;
    champion.level = stat.level;
    world.AddComponent<ChampionComponent>(entity, champion);

    AttachChampionSimComponents(world, entity, slot.champion);

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Character;
    spatial.team = slot.team;
    spatial.radius = spatialRadius;
    world.AddComponent<SpatialAgentComponent>(entity, spatial);

    const ChampionColliderProfileDef& colliderProfile = objectDefs.championCollider;
    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, colliderProfile.bodyHeight, spatial.radius };
    collider.vOffset = { 0.f, colliderProfile.bodyOffsetY, 0.f };
    collider.bIsTrigger = false;
    world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = sightRange;
    world.AddComponent<VisionSourceComponent>(entity, vision);
    world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    world.AddComponent<TargetableTag>(entity);

    return entity;
}
