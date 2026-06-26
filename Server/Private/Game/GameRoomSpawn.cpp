#include "Game/GameRoom.h"

#include "GameRoomInternal.h"
#include "GameRoomSmokeRoster.h"

#include "Game/ServerAICommandProducer.h"
#include "Game/ServerMinionTuning.h"
#include "Game/WorldBootstrap.h"
#include "Server/Private/Game/Factory/ChampionSimComponentTable.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillLoadoutComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/WaypointPatrolComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Definitions/MinionCombatDef.h"
#include "Shared/GameSim/Definitions/StageData.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

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

    constexpr int32_t kStageChampionSpawnWalkableSearchRadius = 16;
    constexpr f32_t kChampionAIInitialDecisionDelaySec = 0.35f;

    VisibilityComponent BuildServerVisibleToAll()
    {
        VisibilityComponent visibility{};
        visibility.teamVisibilityMask = static_cast<u8_t>(
            (1u << TeamByte(eTeam::Blue)) |
            (1u << TeamByte(eTeam::Red)));
        return visibility;
    }
}

Vec3 CGameRoom::GetSpawnPositionForLobbySlot(const LobbySlotState& slot) const
{
    if (IsRedSylasSmokeDummySlot(slot))
        return GetRedSylasSmokeDummyPosition();

    Winters::Map::StageData stage{};
    std::wstring stagePath;
    if (LoadServerStageData(stage, stagePath))
    {
        Vec3 stageSpawn{};
        if (TryResolveStageFountainSpawn(
            stage,
            slot.slotId,
            static_cast<eTeam>(slot.team),
            stageSpawn))
        {
            Vec3 walkableStageSpawn = stageSpawn;
            if (TryResolveServerWalkablePosition(
                stageSpawn,
                kStageChampionSpawnWalkableSearchRadius,
                walkableStageSpawn))
            {
                return walkableStageSpawn;
            }
        }
    }

    const Vec3 fallbackSpawn = GetGameSimRosterSpawnPosition(slot.slotId, slot.team, slot.bBot);
    Vec3 walkableFallbackSpawn = fallbackSpawn;
    if (TryResolveServerWalkablePosition(
        fallbackSpawn,
        kStageChampionSpawnWalkableSearchRadius,
        walkableFallbackSpawn))
    {
        return walkableFallbackSpawn;
    }

    return fallbackSpawn;
}

void CGameRoom::SpawnChampionsFromLobby()
{
    LobbySlotState* pLobbySlots = GetLobbySlots();
    const u32_t lobbySlotCount = GetLobbySlotCount();
    for (u32_t i = 0; pLobbySlots && i < lobbySlotCount; ++i)
    {
        LobbySlotState& slot = pLobbySlots[i];
        if (!slot.bHuman && !slot.bBot)
            continue;
        if (slot.netId != NULL_NET_ENTITY)
            continue;

        SpawnChampionForLobbySlot(slot);
    }
}

void CGameRoom::SpawnServerGameplayObjects()
{
    if (m_bGameplayObjectsSpawned)
        return;

    m_bGameplayObjectsSpawned = true;

    Winters::Map::StageData stage{};
    std::wstring stagePath;
    const bool_t bLoadedStage = LoadServerStageData(stage, stagePath);
    if (bLoadedStage)
    {
        CacheServerMinionWaypoints(stage);
        InitializeServerWalkableGrid(&stage, stagePath.c_str());

        u32_t spawnedStructures = 0;
        for (const auto& entry : stage.structures)
        {
            if (SpawnServerStructureFromStageEntry(entry) != NULL_ENTITY)
                ++spawnedStructures;
        }

        u32_t spawnedJungles = 0;
        for (const auto& entry : stage.jungles)
        {
            if (SpawnServerJungleFromStageEntry(entry) != NULL_ENTITY)
                ++spawnedJungles;
        }

        wchar_t stageMsg[512]{};
        swprintf_s(stageMsg,
            L"[GameRoom] Stage1 loaded for server sim: %ls structures=%u jungles=%u waypoints=%zu\n",
            stagePath.c_str(),
            spawnedStructures,
            spawnedJungles,
            stage.minionWaypoints.size());
        OutputServerAITraceW(stageMsg);

        if (spawnedStructures == 0)
        {
            OutputServerAITrace("[GameRoom] Stage has no server structures; using fallback structures\n");
        }
        else
        {
            CarveServerStructuresOnNavGrid();
            SanitizeServerMoversOnNavGrid();
            SanitizeServerWaypointPatrolsOnNavGrid();
            SanitizeServerMinionWaypointsOnNavGrid();
            RebuildServerMinionFlowFields();
            RefreshChampionAIGoals();
            m_serverMinionWaves.ResetWaveSchedule();

            char msg[160]{};
            sprintf_s(msg,
                "[GameRoom] Server gameplay objects spawned. entities=%u\n",
                m_world.GetEntityCount());
            OutputServerAITrace(msg);
            return;
        }
    }
    else
    {
        OutputServerAITrace("[GameRoom] Stage1.dat not found for server sim; using fallback objects\n");
        InitializeServerWalkableGrid(nullptr, nullptr);
    }

    const SpawnObjectDefinitionPack& objectDefs = ServerData::GetLoLSpawnObjectDefinitionPack();
    const auto fallbackStructures = CWorldBootstrap::BuildFallbackStructures(
        objectDefs,
        kStructureKindTurret,
        kStructureKindNexus,
        kLaneMid);
    for (const WorldBootstrapStructureSpawnRequest& request : fallbackStructures)
    {
        SpawnServerStructure(
            request.team,
            request.kind,
            request.tier,
            request.lane,
            request.position,
            request.rotation,
            request.maxHp,
            request.scale,
            request.bTurret,
            request.bNexus,
            request.bInhibitor);
    }

    CarveServerStructuresOnNavGrid();
    SanitizeServerMoversOnNavGrid();
    SanitizeServerWaypointPatrolsOnNavGrid();
    SanitizeServerMinionWaypointsOnNavGrid();
    RebuildServerMinionFlowFields();
    RefreshChampionAIGoals();
    m_serverMinionWaves.ResetWaveSchedule();

    char msg[160]{};
    sprintf_s(msg,
        "[GameRoom] Server gameplay objects spawned. entities=%u\n",
        m_world.GetEntityCount());
    OutputServerAITrace(msg);
}

EntityID CGameRoom::SpawnServerStructureFromStageEntry(
    const Winters::Map::StructureEntry& entry)
{
    WorldBootstrapStructureSpawnRequest request{};
    if (!CWorldBootstrap::TryBuildStageStructureRequest(
        ServerData::GetLoLSpawnObjectDefinitionPack(),
        entry,
        static_cast<u8_t>(entry.lane),
        kStructureKindTurret,
        kStructureKindNexus,
        kStructureKindInhibitor,
        request))
    {
        return NULL_ENTITY;
    }

    const u32_t kind = request.kind;
    const eTeam team = request.team;
    const Vec3 pos = request.position;
    const u8_t resolvedLane = ResolveServerStructureLane(team, kind, entry.tier, pos);
    if (entry.lane != resolvedLane)
    {
        char msg[256]{};
        sprintf_s(msg,
            "[GameRoom] structure lane remap team=%u kind=%u tier=%u stageLane=%u lane=%u pos=(%.2f,%.2f,%.2f)\n",
            static_cast<u32_t>(team),
            kind,
            entry.tier,
            entry.lane,
            static_cast<u32_t>(resolvedLane),
            pos.x,
            pos.y,
            pos.z);
        OutputServerAITrace(msg);
    }

    request.lane = resolvedLane;

    return SpawnServerStructure(
        request.team,
        request.kind,
        request.tier,
        request.lane,
        request.position,
        request.rotation,
        request.maxHp,
        request.scale,
        request.bTurret,
        request.bNexus,
        request.bInhibitor);
}

EntityID CGameRoom::SpawnServerJungleFromStageEntry(
    const Winters::Map::JungleEntry& entry)
{
    WorldBootstrapJungleSpawnRequest request{};
    if (!CWorldBootstrap::TryBuildStageJungleRequest(
        ServerData::GetLoLSpawnObjectDefinitionPack(),
        entry,
        request))
    {
        return NULL_ENTITY;
    }

    const EntityID entity = m_world.CreateEntity();

    TransformComponent transform{};
    transform.SetPosition(request.position);
    transform.SetRotation(request.rotation);
    transform.SetScale(request.scale);
    m_world.AddComponent<TransformComponent>(entity, transform);

    const JungleCampGameDef& jungleDef = request.combat;
    const f32_t maxHp = jungleDef.maxHp;

    JungleComponent jungle{};
    jungle.subKind = request.subKind;
    jungle.campId = request.campId;
    jungle.hp = maxHp;
    jungle.maxHp = maxHp;
    m_world.AddComponent<JungleComponent>(entity, jungle);
    m_world.AddComponent<JungleMonsterTag>(entity);

    HealthComponent health{};
    health.fCurrent = maxHp;
    health.fMaximum = maxHp;
    m_world.AddComponent<HealthComponent>(entity, health);

    const f32_t attackDamage = jungleDef.attackDamage;
    const f32_t attackCooldown = jungleDef.attackCooldown;
    const f32_t attackSpeed = 1.f / attackCooldown;

    StatComponent stat{};
    stat.championId = eChampion::NONE;
    stat.level = 1;
    stat.hpMax = maxHp;
    stat.baseAd = attackDamage;
    stat.ad = attackDamage;
    stat.baseArmor = jungleDef.baseArmor;
    stat.armor = stat.baseArmor;
    stat.baseMr = jungleDef.baseMr;
    stat.mr = stat.baseMr;
    stat.baseAttackSpeed = attackSpeed;
    stat.attackSpeedRatio = attackSpeed;
    stat.attackSpeed = attackSpeed;
    stat.attackRange = jungleDef.attackRange;
    stat.moveSpeed = jungleDef.moveSpeed;
    stat.bDirty = false;
    m_world.AddComponent<StatComponent>(entity, stat);

    m_world.AddComponent<SkillStateComponent>(entity, SkillStateComponent{});
    m_world.AddComponent<JungleAIComponent>(entity, JungleAIComponent{});

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::NeutralUnit;
    spatial.team = TeamByte(eTeam::Neutral);
    spatial.radius = jungleDef.radius;
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 2.0f, spatial.radius };
    collider.vOffset = { 0.f, 1.0f, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = 10.f;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);

    SetPoseState(m_world, entity, ePoseStateId::Idle, 0, true);

    const NetEntityId netId = m_entityMap.IssueNew(entity);
    NetEntityIdComponent netEntity{};
    netEntity.netId = netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    return entity;
}

EntityID CGameRoom::SpawnServerStructure(eTeam team, u32_t kind, u32_t tier, u32_t lane,
    const Vec3& pos, const Vec3& rotation, f32_t maxHp, f32_t scale,
    bool_t bTurret, bool_t bNexus, bool_t bInhibitor)
{
    const EntityID entity = m_world.CreateEntity();

    TransformComponent transform{};
    transform.SetPosition(pos);
    transform.SetRotation(rotation);
    transform.SetScale(scale > 0.f ? scale : 1.f);
    m_world.AddComponent<TransformComponent>(entity, transform);

    StructureComponent structure{};
    structure.team = team;
    structure.kind = kind;
    structure.tier = tier;
    structure.lane = lane;
    structure.hp = maxHp;
    structure.maxHp = maxHp;
    m_world.AddComponent<StructureComponent>(entity, structure);

    HealthComponent health{};
    health.fCurrent = maxHp;
    health.fMaximum = maxHp;
    m_world.AddComponent<HealthComponent>(entity, health);

    if (bTurret)
    {
        TurretComponent turret{};
        turret.team = team;
        turret.hp = maxHp;
        turret.maxHp = maxHp;
        turret.tier = static_cast<u8_t>(tier);
        turret.laneType = static_cast<u8_t>(lane);
        m_world.AddComponent<TurretComponent>(entity, turret);

        const TurretAIGameDef& turretAI = ServerData::GetLoLSpawnObjectDefinitionPack().structure.turretAI;
        TurretAIComponent ai{};
        ai.attackRange = turretAI.attackRange;
        ai.attackCooldownMax = turretAI.attackCooldownMax;
        ai.attackDamage = (tier == static_cast<u32_t>(Winters::Map::eTurretTier::Nexus))
            ? turretAI.nexusAttackDamage
            : turretAI.attackDamage;
        ai.projectileSpeed = turretAI.projectileSpeed;
        m_world.AddComponent<TurretAIComponent>(entity, ai);
    }

    if (bNexus)
        m_world.AddComponent<NexusTag>(entity);
    if (bInhibitor)
        m_world.AddComponent<InhibitorTag>(entity);

    SpatialAgentComponent spatial{};
    if (bTurret)
        spatial.kind = eSpatialKind::Structure;
    else if (bInhibitor)
        spatial.kind = eSpatialKind::Objective;
    else
        spatial.kind = eSpatialKind::Core;
    spatial.team = TeamByte(team);
    spatial.radius = ResolveStageStructureRadius(kind, tier);
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    ColliderComponent collider{};
    const TurretAIGameDef& structureDef = ServerData::GetLoLSpawnObjectDefinitionPack().structure.turretAI;
    collider.vHalfExtents = { spatial.radius, structureDef.bodyHeight, spatial.radius };
    collider.vOffset = { 0.f, structureDef.bodyOffsetY, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = bTurret ? structureDef.turretSightRange : structureDef.structureSightRange;
    vision.bTrueSight = bTurret;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);

    SetPoseState(m_world, entity, ePoseStateId::Idle, 0, true);

    const NetEntityId netId = m_entityMap.IssueNew(entity);
    NetEntityIdComponent netEntity{};
    netEntity.netId = netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    return entity;
}

EntityID CGameRoom::SpawnServerMinion(eTeam team, u8_t roleType, u8_t lane, const Vec3& pos)
{
    const EntityID entity = m_world.CreateEntity();

    Vec3 spawnPos = pos;
    (void)TryResolveServerWalkablePosition(pos, 16, spawnPos);

    TransformComponent transform{};
    transform.SetPosition(spawnPos);
    transform.SetScale(0.006f);
    m_world.AddComponent<TransformComponent>(entity, transform);

    MinionStateComponent state{};
    state.current = MinionStateComponent::LaneMove;
    state.currentWaypoint = ResolveServerMinionStartWaypoint(team, lane, spawnPos);
    state.team = team;
    state.type = roleType;
    state.lane = lane;
    const MinionCombatDef combat = ServerData::GetLoLSpawnObjectDefinitionPack().ResolveMinion(roleType);
    state.moveSpeed = combat.moveSpeed;
    state.attackRange = combat.attackRange;
    state.sightRange = combat.sightRange;
    state.attackDamage = combat.attackDamage;
    state.attackCooldownMax = combat.attackCooldownMax;
    state.targetScanInterval = ServerMinionTuning::kTargetScanIntervalSec;
    const u32_t scanBucket =
        (static_cast<u32_t>(entity) * 1103515245u +
            static_cast<u32_t>(lane) * 2246822519u +
            static_cast<u32_t>(roleType) * 3266489917u) %
        ServerMinionTuning::kTargetScanStaggerBuckets;
    state.targetScanCooldown =
        state.targetScanInterval *
        (static_cast<f32_t>(scanBucket) /
            static_cast<f32_t>(ServerMinionTuning::kTargetScanStaggerBuckets));
    m_world.AddComponent<MinionStateComponent>(entity, state);

    const f32_t maxHp = combat.maxHp;
    HealthComponent health{};
    health.fCurrent = maxHp;
    health.fMaximum = maxHp;
    m_world.AddComponent<HealthComponent>(entity, health);

    MinionComponent minion{};
    minion.team = team;
    minion.laneType = lane;
    minion.roleType = roleType;
    minion.hp = maxHp;
    minion.maxHp = maxHp;
    m_world.AddComponent<MinionComponent>(entity, minion);

    VelocityComponent velocity{};
    m_world.AddComponent<VelocityComponent>(entity, velocity);

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Unit;
    spatial.team = TeamByte(team);
    spatial.radius = 0.5f;
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, 1.0f, spatial.radius };
    collider.vOffset = { 0.f, 0.5f, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = state.sightRange;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);

    SetPoseState(m_world, entity, ePoseStateId::Run, 0, true);

    const NetEntityId netId = m_entityMap.IssueNew(entity);
    NetEntityIdComponent netEntity{};
    netEntity.netId = netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    return entity;
}

u8_t CGameRoom::ResolveServerStructureLane(
    eTeam team,
    u32_t kind,
    u32_t tier,
    const Vec3& pos) const
{
    if (kind == kStructureKindNexus ||
        (kind == kStructureKindTurret &&
            tier == static_cast<u32_t>(Winters::Map::eTurretTier::Nexus)))
    {
        return static_cast<u8_t>(kLaneBase);
    }

    static constexpr u8_t kPhysicalLanes[] =
    {
        static_cast<u8_t>(kLaneTop),
        static_cast<u8_t>(kLaneMid),
        static_cast<u8_t>(kLaneBot),
    };

    u8_t bestLane = static_cast<u8_t>(kLaneMid);
    f32_t bestScore = std::numeric_limits<f32_t>::max();

    for (u8_t lane : kPhysicalLanes)
    {
        const u8_t waypointLane = ResolveServerWaypointLane(team, lane);
        const u32_t waypointCount = GetServerMinionWaypointCount(team, waypointLane);
        if (waypointCount == 0u)
            continue;

        f32_t score = std::numeric_limits<f32_t>::max();
        if (waypointCount >= 2u)
        {
            for (u32_t i = 1u; i < waypointCount; ++i)
            {
                f32_t t = 0.f;
                score = std::min(score, WintersMath::DistanceSqPointToSegmentXZ(
                    pos,
                    GetServerMinionWaypoint(team, waypointLane, i - 1u),
                    GetServerMinionWaypoint(team, waypointLane, i),
                    &t,
                    std::numeric_limits<f32_t>::epsilon()));
            }
        }
        else
        {
            score = WintersMath::DistanceSqXZ(pos, GetServerMinionWaypoint(team, waypointLane, 0u));
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestLane = lane;
        }
    }

    return bestLane;
}

EntityID CGameRoom::SpawnChampionForLobbySlot(LobbySlotState& slot)
{
    const EntityHandle entityHandle = m_world.CreateEntityHandle();
    const EntityID entity = entityHandle.GetIndex();

    const Vec3 spawnPos = GetSpawnPositionForLobbySlot(slot);

    TransformComponent transform{};
    transform.SetPosition(spawnPos);
    m_world.AddComponent<TransformComponent>(entity, transform);

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
        m_world.AddComponent<ChampionDefinitionComponent>(entity, identity);

        SkillLoadoutComponent loadout{};
        for (u8_t skillSlot = 0u; skillSlot < kChampionSkillSlotCount; ++skillSlot)
            loadout.skills[skillSlot] = championDef->skillLoadout[skillSlot];
        m_world.AddComponent<SkillLoadoutComponent>(entity, loadout);

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
    m_world.AddComponent<StatComponent>(entity, stat);

    HealthComponent health{};
    health.fCurrent = stat.hpMax;
    health.fMaximum = stat.hpMax;
    health.bIsDead = false;
    m_world.AddComponent<HealthComponent>(entity, health);

    RespawnComponent respawn{};
    respawn.spawnPos = spawnPos;
    respawn.respawnDelay = spawnPolicy.respawnDelaySec;
    m_world.AddComponent<RespawnComponent>(entity, respawn);

    SkillStateComponent skillState{};
    m_world.AddComponent<SkillStateComponent>(entity, skillState);

    CExperienceSystem::InitializeChampionExperience(m_world, entity, stat.level);

    SkillRankComponent skillRank{};
    if (slot.bBot && !slot.bDummy)
        AssignDefaultBotSkillRanks(skillRank, stat.level);
    else
        CSkillRankSystem::SyncPointsForLevel(skillRank, stat.level);
    m_world.AddComponent<SkillRankComponent>(entity, skillRank);

    GoldComponent gold{};
    gold.amount = spawnPolicy.startGold;
    m_world.AddComponent<GoldComponent>(entity, gold);

    InventoryComponent inventory{};
    m_world.AddComponent<InventoryComponent>(entity, inventory);

    RuneLoadoutComponent runeLoadout{};
    runeLoadout.eRunes[0] = spawnPolicy.startRune;
    runeLoadout.iCount = spawnPolicy.startRuneCount;
    m_world.AddComponent<RuneLoadoutComponent>(entity, runeLoadout);
    m_world.AddComponent<RuneRuntimeComponent>(entity, RuneRuntimeComponent{});

    ChampionScoreComponent score{};
    m_world.AddComponent<ChampionScoreComponent>(entity, score);

    SummonerSpellStateComponent summonerSpellState{};
    m_world.AddComponent<SummonerSpellStateComponent>(entity, summonerSpellState);

    ChampionComponent champion{};
    champion.id = slot.champion;
    champion.team = static_cast<eTeam>(slot.team);
    champion.hp = health.fCurrent;
    champion.maxHp = health.fMaximum;
    champion.mana = stat.manaMax;
    champion.maxMana = stat.manaMax;
    champion.moveSpeed = stat.moveSpeed;
    champion.level = stat.level;
    m_world.AddComponent<ChampionComponent>(entity, champion);

    AttachChampionSimComponents(m_world, entity, slot.champion);

    SpatialAgentComponent spatial{};
    spatial.kind = eSpatialKind::Character;
    spatial.team = slot.team;
    spatial.radius = spatialRadius;
    m_world.AddComponent<SpatialAgentComponent>(entity, spatial);

    const ChampionColliderProfileDef& colliderProfile = objectDefs.championCollider;
    ColliderComponent collider{};
    collider.vHalfExtents = { spatial.radius, colliderProfile.bodyHeight, spatial.radius };
    collider.vOffset = { 0.f, colliderProfile.bodyOffsetY, 0.f };
    collider.bIsTrigger = false;
    m_world.AddComponent<ColliderComponent>(entity, collider);

    VisionSourceComponent vision{};
    vision.sightRange = sightRange;
    m_world.AddComponent<VisionSourceComponent>(entity, vision);
    m_world.AddComponent<VisibilityComponent>(entity, BuildServerVisibleToAll());
    m_world.AddComponent<TargetableTag>(entity);
    if (slot.bDummy)
        m_world.AddComponent<PracticeDummyTag>(entity);
    if (IsRedSylasSmokeDummySlot(slot))
    {
        WaypointPatrolComponent patrol{};
        patrol.pointCount = GetRedSylasSmokePatrolPointCount();
        for (u8_t i = 0; i < patrol.pointCount; ++i)
            patrol.points[i] = GetRedSylasSmokePatrolPoint(i);
        patrol.currentIndex = 1;
        patrol.direction = 1;
        patrol.mode = eWaypointPatrolMode::PingPong;
        patrol.arriveRadius = 0.35f;
        patrol.bActive = true;
        m_world.AddComponent<WaypointPatrolComponent>(entity, patrol);
    }

    if (slot.bBot && !slot.bDummy)
    {
        ChampionAIComponent ai{};
        const ChampionAIProfile& profile = GetChampionAIProfile(slot.champion);
        ai.champion = slot.champion;
        ai.team = static_cast<eTeam>(slot.team);
        ai.difficulty = slot.botDifficulty;
        ai.lane = CServerAICommandProducer::ResolveInitialBotLane(
            slot,
            GetGameSimRosterLane(slot.slotId));
        ai.decisionTimer = kChampionAIInitialDecisionDelaySec;
        ai.retreatGoal = spawnPos;
        ai.championScanRange = profile.championScanRange;
        ai.minionScanRange = profile.minionScanRange;
        ai.structureScanRange = profile.structureScanRange;
        ai.leashRange = profile.leashRange;
        ai.retreatHpRatio = profile.retreatHpRatio;
        ai.reengageHpRatio = profile.reengageHpRatio;

        const u8_t waypointLane = ResolveServerWaypointLane(ai.team, ai.lane);
        const u32_t waypointCount = GetServerMinionWaypointCount(ai.team, waypointLane);
        ai.laneGoal = waypointCount > 0u
            ? GetServerMinionWaypoint(ai.team, waypointLane, waypointCount / 2u)
            : GetGameSimLaneGatherPosition(ai.lane, slot.team);
        ai.safeAnchor = ResolveChampionAISafeAnchor(ai.team, ai.lane);
        ai.retreatGoal = ai.safeAnchor;

        m_world.AddComponent<ChampionAIComponent>(entity, ai);
    }

    SetPoseState(m_world, entity, ePoseStateId::Idle, 0, true);

    slot.netId = m_entityMap.IssueNew(entity);

    NetEntityIdComponent netEntity{};
    netEntity.netId = slot.netId;
    m_world.AddComponent<NetEntityIdComponent>(entity, netEntity);

    if (slot.bHuman && slot.sessionId != 0)
        m_sessionBinding.Bind(slot.sessionId, entity);

    char spawnMsg[320]{};
    sprintf_s(spawnMsg,
        "[GameRoom] SpawnLobby slot=%u team=%u human=%u bot=%u dummy=%u champ=%u netId=%u entity=%u pos=(%.2f,%.2f,%.2f) aiDelay=%.2f\n",
        static_cast<u32_t>(slot.slotId),
        static_cast<u32_t>(slot.team),
        slot.bHuman ? 1u : 0u,
        slot.bBot ? 1u : 0u,
        slot.bDummy ? 1u : 0u,
        static_cast<u32_t>(slot.champion),
        slot.netId,
        static_cast<u32_t>(entity),
        spawnPos.x,
        spawnPos.y,
        spawnPos.z,
        (slot.bBot && !slot.bDummy) ? kChampionAIInitialDecisionDelaySec : 0.f);
    OutputServerAITrace(spawnMsg);

    return entity;
}
