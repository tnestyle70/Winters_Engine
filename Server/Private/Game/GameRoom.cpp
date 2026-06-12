#include "Game/GameRoom.h"

#include "Game/ServerMinionTuning.h"
#include "Game/SnapshotBuilder.h"
#include "Game/ReplayRecorder.h"
#include "GameRoomSmokeRoster.h"
#include "GameRoomInternal.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionScore.h"
#include "Shared/GameSim/Components/MatchScore.h"
#include "Shared/GameSim/Components/GoldComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/JungleAIComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/WaypointPatrolComponent.h"
#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"
#include "Shared/GameSim/Champions/Ashe/AsheGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Irelia/IreliaGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"
#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"
#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"
#include "Shared/GameSim/Champions/MasterYi/MasterYiGameSim.h"
#include "Shared/GameSim/Champions/Riven/RivenGameSim.h"
#include "Shared/GameSim/Champions/Sylas/SylasGameSim.h"
#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Definitions/StageData.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"

#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Security/LagCompensation.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/Experience/ExperienceSystem.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"
#include "Shared/GameSim/Systems/Stat/StatSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

#include "Shared/Network/PacketEnvelope.h"
#include "Shared/Schemas/Generated/cpp/Hello_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyCommand_generated.h"
#include "Shared/Schemas/Generated/cpp/LobbyState_generated.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "ECS/Systems/TurretAISystem.h"
#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/MapWalkableBaker.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Navigation/Pathfinder.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <flatbuffers/flatbuffers.h>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
    void EnsureMatchScoreEntity(CWorld& world)
    {
        bool_t bFound = false;
        world.ForEach<MatchScoreComponent>(
            [&](EntityID, MatchScoreComponent&)
            {
                bFound = true;
            }
        );
        if (!bFound)
            world.AddComponent<MatchScoreComponent>(
                world.CreateEntity(),
                MatchScoreComponent{}
            );
    }

    constexpr u16_t kTurretProjectileKind = 100;
    constexpr f32_t kServerMinionRangedProjectileTargetHeight = 0.85f;
    bool_t IsServerMinionRangedProjectileKind(eProjectileKind kind)
    {
        return kind == eProjectileKind::MinionRangedBasicBlue ||
            kind == eProjectileKind::MinionRangedBasicRed;
    }

    f32_t ResolveCombatRadius(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<SpatialAgentComponent>(entity))
            return std::max(0.2f, world.GetComponent<SpatialAgentComponent>(entity).radius);
        if (world.HasComponent<StructureComponent>(entity))
            return 1.5f;
        if (world.HasComponent<MinionComponent>(entity) ||
            world.HasComponent<MinionStateComponent>(entity))
            return 0.45f;
        return 0.65f;
    }

    EntityID FindSkillProjectileHitTarget(
        CWorld& world,
        const SkillProjectileComponent& projectile,
        const Vec3& start,
        const Vec3& end,
        Vec3& outHitPos)
    {
        EntityID bestTarget = NULL_ENTITY;
        f32_t bestT = 1.f;

        world.ForEach<HealthComponent, TransformComponent>(
            std::function<void(EntityID, HealthComponent&, TransformComponent&)>(
                [&](EntityID entity, HealthComponent& health, TransformComponent& transform)
                {
                    const bool_t bYasuoTornado =
                        projectile.kind == eProjectileKind::Tornado;
                    if (entity == projectile.sourceEntity ||
                        !world.IsAlive(entity) ||
                        health.bIsDead ||
                        health.fCurrent <= 0.f)
                    {
                        return;
                    }

                    eTeam targetTeam = eTeam::Neutral;
                    if (!TryResolveCombatTeam(world, entity, targetTeam))
                        return;
                    if (targetTeam == projectile.sourceTeam &&
                        targetTeam != eTeam::Neutral)
                    {
                        return;
                    }
                    if (!GameplayStateQuery::CanReceiveProjectileHit(
                        world,
                        projectile.sourceEntity,
                        entity))
                    {
                        return;
                    }

                    const Vec3 targetPos = transform.GetPosition();
                    f32_t t = 0.f;
                    const f32_t distSq = WintersMath::DistanceSqPointToSegmentXZ(
                        targetPos,
                        start,
                        end,
                        &t,
                        std::numeric_limits<f32_t>::epsilon());
                    const f32_t projectileRadius = bYasuoTornado
                        ? std::max(projectile.hitRadius, 2.25f)
                        : projectile.hitRadius;
                    const f32_t radius = projectileRadius + ResolveCombatRadius(world, entity);
                    if (distSq <= radius * radius && t <= bestT)
                    {
                        bestTarget = entity;
                        bestT = t;
                        outHitPos = Vec3{
                            start.x + (end.x - start.x) * t,
                            targetPos.y + 1.0f,
                            start.z + (end.z - start.z) * t
                        };
                    }
                }));

        return bestTarget;
    }

    void LogSkillProjectileEvent(
        const char* state,
        EntityID projectileEntity,
        const SkillProjectileComponent& projectile,
        EntityID targetEntity,
        const Vec3& pos)
    {
        static u32_t s_logCount = 0;
        if (s_logCount >= 128u)
            return;

        char msg[256]{};
        sprintf_s(msg,
            "[SkillProjectile] %s kind=%u source=%u projectile=%u target=%u pos=(%.2f,%.2f,%.2f) traveled=%.2f\n",
            state ? state : "-",
            static_cast<u32_t>(projectile.kind),
            static_cast<u32_t>(projectile.sourceEntity),
            static_cast<u32_t>(projectileEntity),
            static_cast<u32_t>(targetEntity),
            pos.x,
            pos.y,
            pos.z,
            projectile.traveledDistance);
        OutputServerAITrace(msg);
        ++s_logCount;
    }
}

std::unique_ptr<CGameRoom> CGameRoom::Create(u32_t roomId)
{
    auto room = std::unique_ptr<CGameRoom>(new CGameRoom(roomId));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    room->m_pLagCompensation = std::make_unique<CLagCompensation>();
    room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
    room->InitializeServerSimSystems();
    return room;
}

CGameRoom::CGameRoom(u32_t roomId)
    : m_roomId(roomId)
{
    InitializeLobbySlots();
}

CGameRoom::~CGameRoom()
{
    Stop();
}

void CGameRoom::Start()
{
    if (m_bRunning.exchange(true))
        return;

    m_tickThread = std::thread(&CGameRoom::TickThread, this);
}

void CGameRoom::Stop()
{
    const bool_t bWasRunning = m_bRunning.exchange(false);
    if (bWasRunning && m_tickThread.joinable())
        m_tickThread.join();

    FinalizeReplayRecorder();
}

void CGameRoom::Phase_ServerDeathAndRespawn(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto entities = DeterministicEntityIterator<RespawnComponent>::CollectSorted(m_world);
    for (EntityID entity : entities)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<RespawnComponent>(entity) ||
            !m_world.HasComponent<HealthComponent>(entity) ||
            !m_world.HasComponent<ChampionComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& respawn = m_world.GetComponent<RespawnComponent>(entity);
        auto& health = m_world.GetComponent<HealthComponent>(entity);
        auto& champion = m_world.GetComponent<ChampionComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);

        const bool_t bDead = health.bIsDead || health.fCurrent <= 0.f;
        if (!bDead)
        {
            if (respawn.bPending)
            {
                respawn.bPending = false;
                respawn.respawnTimer = 0.f;
                if (!m_world.HasComponent<TargetableTag>(entity))
                    m_world.AddComponent<TargetableTag>(entity);
            }
            continue;
        }

        health.fCurrent = 0.f;
        health.bIsDead = true;
        champion.hp = 0.f;
        champion.maxHp = health.fMaximum;

        if (!respawn.bPending)
        {
            respawn.bPending = true;
            respawn.respawnTimer = respawn.respawnDelay > 0.f
                ? respawn.respawnDelay
                : kDefaultChampionRespawnDelaySec;

            if (m_world.HasComponent<TargetableTag>(entity))
                m_world.RemoveComponent<TargetableTag>(entity);

            if (m_world.HasComponent<MoveTargetComponent>(entity))
            {
                auto& moveTarget = m_world.GetComponent<MoveTargetComponent>(entity);
                moveTarget.bHasTarget = false;
                moveTarget.pathCount = 0;
                moveTarget.pathIndex = 0;
            }

            if (m_world.HasComponent<SkillStateComponent>(entity))
            {
                auto& skillState = m_world.GetComponent<SkillStateComponent>(entity);
                for (u8_t i = 0; i < 5; ++i)
                {
                    skillState.slots[i].currentStage = 0;
                    skillState.slots[i].stageWindow = 0.f;
                }
            }

            if (m_world.HasComponent<ChampionAIComponent>(entity))
            {
                auto& ai = m_world.GetComponent<ChampionAIComponent>(entity);
                ai.state = eChampionAIState::Dead;
                ai.lastAction = eChampionAIAction::Retreat;
                ai.lockedChampion = NULL_ENTITY;
                ai.targetMinion = NULL_ENTITY;
                ai.targetStructure = NULL_ENTITY;
                ai.alliedWave = NULL_ENTITY;
                ai.comboTarget = NULL_ENTITY;
                ai.comboStep = 0u;
                ai.bWaveJoined = false;
                ai.bStructureWaveTanking = false;
                ai.bInsideEnemyTurretDanger = false;
            }

            StartReplicatedAnimation(m_world, entity, eNetAnimId::Death, tc);

            static u32_t s_deathLogCount = 0;
            if (s_deathLogCount < 64u)
            {
                char msg[224]{};
                sprintf_s(msg,
                    "[Respawn] death tick=%llu entity=%u champion=%u team=%u respawn=%.2f\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(entity),
                    static_cast<u32_t>(champion.id),
                    static_cast<u32_t>(champion.team),
                    respawn.respawnTimer);
                OutputServerAITrace(msg);
                ++s_deathLogCount;
            }
        }

        if (respawn.respawnTimer > 0.f)
        {
            respawn.respawnTimer -= tc.fDt;
            if (respawn.respawnTimer > 0.f)
                continue;
        }

        health.fCurrent = health.fMaximum;
        health.bIsDead = false;
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
        transform.SetPosition(respawn.spawnPos);

        if (!m_world.HasComponent<TargetableTag>(entity))
            m_world.AddComponent<TargetableTag>(entity);
        if (m_world.HasComponent<MoveTargetComponent>(entity))
        {
            auto& moveTarget = m_world.GetComponent<MoveTargetComponent>(entity);
            moveTarget.bHasTarget = false;
            moveTarget.pathCount = 0;
            moveTarget.pathIndex = 0;
        }

        if (m_world.HasComponent<ChampionAIComponent>(entity))
        {
            auto& ai = m_world.GetComponent<ChampionAIComponent>(entity);
            ai.state = eChampionAIState::MoveToOuterTurret;
            ai.lastAction = eChampionAIAction::MoveToSafeAnchor;
            ai.lockedChampion = NULL_ENTITY;
            ai.targetMinion = NULL_ENTITY;
            ai.targetStructure = NULL_ENTITY;
            ai.alliedWave = NULL_ENTITY;
            ai.comboTarget = NULL_ENTITY;
            ai.comboStep = 0u;
            ai.bWaveJoined = false;
            ai.bStructureWaveTanking = false;
            ai.bInsideEnemyTurretDanger = false;
            ai.decisionTimer = 0.25f;
        }

        respawn.bPending = false;
        respawn.respawnTimer = 0.f;

        if (champion.id == eChampion::SYLAS &&
            m_world.HasComponent<PracticeDummyTag>(entity) &&
            m_world.HasComponent<WaypointPatrolComponent>(entity))
        {
            auto& patrol = m_world.GetComponent<WaypointPatrolComponent>(entity);
            patrol.currentIndex = 1;
            patrol.direction = 1;
            patrol.bActive = true;
        }

        StartReplicatedAnimation(m_world, entity, eNetAnimId::Idle, tc);

        static u32_t s_respawnLogCount = 0;
        if (s_respawnLogCount < 64u)
        {
            char msg[256]{};
            sprintf_s(msg,
                "[Respawn] revive tick=%llu entity=%u champion=%u team=%u pos=(%.2f,%.2f,%.2f) hp=%.2f\n",
                static_cast<unsigned long long>(tc.tickIndex),
                static_cast<u32_t>(entity),
                static_cast<u32_t>(champion.id),
                static_cast<u32_t>(champion.team),
                respawn.spawnPos.x,
                respawn.spawnPos.y,
                respawn.spawnPos.z,
                health.fCurrent);
            OutputServerAITrace(msg);
            ++s_respawnLogCount;
        }
    }
}

void CGameRoom::InitializeServerSimSystems()
{
    m_world.Initialize_Spatial(LoLSpatialGridDesc());
    EnsureMatchScoreEntity(m_world);
    m_pSpatialSystem = Engine::CSpatialHashSystem::Create();
    m_pTurretAI = Engine::CTurretAISystem::Create();

    RegisterDefaultChampionSkillScalingTables();

    AnnieGameSim::RegisterHooks();
    AsheGameSim::RegisterHooks();
    FioraGameSim::RegisterHooks();
    IreliaGameSim::RegisterHooks();
    JaxGameSim::RegisterHooks();
    KalistaGameSim::RegisterHooks();
    LeeSinGameSim::RegisterHooks();
    KindredGameSim::RegisterHooks();
    MasterYiGameSim::RegisterHooks();
    RivenGameSim::RegisterHooks();
    SylasGameSim::RegisterHooks();
    ViegoGameSim::RegisterHooks();
    YoneGameSim::RegisterHooks();
    YasuoGameSim::RegisterHooks();
    ZedGameSim::RegisterHooks();
}

void CGameRoom::Phase_ServerTurretAI(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    if (m_pSpatialSystem)
        m_pSpatialSystem->Execute(m_world, tc.fDt);
    if (m_pTurretAI)
        m_pTurretAI->Execute(m_world, tc.fDt);
}

void CGameRoom::Phase_ServerProjectiles(TickContext& tc)
{
    if (m_roomPhase != eRoomPhase::InGame)
        return;

    const auto projectiles =
        DeterministicEntityIterator<TurretProjectileComponent>::CollectSorted(m_world);

    for (EntityID entity : projectiles)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<TurretProjectileComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& projectile = m_world.GetComponent<TurretProjectileComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);
        const Vec3 pos = projectile.currentPos;
        const NetEntityId currentProjectileNet = m_entityMap.ToNet(entity);
        const bool_t bTargetAlive =
            projectile.targetEntity != NULL_ENTITY &&
            m_world.IsAlive(projectile.targetEntity) &&
            m_world.HasComponent<TransformComponent>(projectile.targetEntity) &&
            IsAliveHealth(m_world, projectile.targetEntity) &&
            GameplayStateQuery::CanReceiveProjectileHit(
                m_world,
                projectile.sourceEntity,
                projectile.targetEntity);

        if (!bTargetAlive)
        {
            if (currentProjectileNet != NULL_NET_ENTITY)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = kTurretProjectileKind;
                hit.position = pos;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
            }

            m_world.DestroyEntity(entity);
            continue;
        }

        if (currentProjectileNet == NULL_NET_ENTITY)
        {
            const NetEntityId projectileNet = m_entityMap.IssueNew(entity);
            NetEntityIdComponent net{};
            net.netId = projectileNet;
            if (!m_world.HasComponent<NetEntityIdComponent>(entity))
                m_world.AddComponent<NetEntityIdComponent>(entity, net);

            Vec3 dir{ 0.f, 0.f, 1.f };
            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
            dir = NormalizeXZOrForward(
                Vec3{ targetPos.x - pos.x, 0.f, targetPos.z - pos.z },
                eTeam::Neutral);

            ReplicatedEventComponent spawn{};
            spawn.kind = eReplicatedEventKind::ProjectileSpawn;
            spawn.sourceEntity = projectile.sourceEntity;
            spawn.targetEntity = projectile.targetEntity;
            spawn.projectileEntity = entity;
            spawn.projectileKind = kTurretProjectileKind;
            spawn.position = pos;
            spawn.direction = dir;
            spawn.speed = projectile.speed;
            spawn.maxDistance = 48.f;
            spawn.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, spawn);

            static u32_t s_turretProjectileLogCount = 0;
            if (s_turretProjectileLogCount < 64u)
            {
                char msg[192]{};
                sprintf_s(msg,
                    "[TurretAI] projectile tick=%llu source=%u target=%u projectile=%u pos=(%.2f,%.2f,%.2f)\n",
                    static_cast<unsigned long long>(tc.tickIndex),
                    static_cast<u32_t>(projectile.sourceEntity),
                    static_cast<u32_t>(projectile.targetEntity),
                    static_cast<u32_t>(entity),
                    pos.x,
                    pos.y,
                    pos.z);
                OutputServerAITrace(msg);
                ++s_turretProjectileLogCount;
            }
        }

        const Vec3 targetPos = m_world.GetComponent<TransformComponent>(
            projectile.targetEntity).GetPosition();
        const Vec3 targetAim{ targetPos.x, targetPos.y + 1.2f, targetPos.z };
        const Vec3 delta{
            targetAim.x - pos.x,
            targetAim.y - pos.y,
            targetAim.z - pos.z
        };
        const f32_t distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        const f32_t hitRadiusSq = projectile.hitRadius * projectile.hitRadius;

        if (distSq <= hitRadiusSq)
        {
            eTeam sourceTeam = eTeam::Neutral;
            (void)TryResolveCombatTeam(m_world, projectile.sourceEntity, sourceTeam);

            DamageRequest request{};
            request.source = projectile.sourceEntity;
            request.target = projectile.targetEntity;
            request.sourceTeam = sourceTeam;
            request.type = eDamageType::Physical;
            request.flatAmount = projectile.damage;
            request.skillId = kTurretProjectileKind;
            EnqueueDamageRequest(m_world, request);

            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.targetEntity = projectile.targetEntity;
            hit.projectileEntity = entity;
            hit.projectileKind = kTurretProjectileKind;
            hit.position = targetAim;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t dist = std::sqrt(distSq);
        if (dist <= std::numeric_limits<f32_t>::epsilon())
        {
            m_world.DestroyEntity(entity);
            continue;
        }

        const f32_t step = projectile.speed * tc.fDt;
        const f32_t t = (step >= dist) ? 1.f : (step / dist);
        Vec3 next{
            pos.x + delta.x * t,
            pos.y + delta.y * t,
            pos.z + delta.z * t
        };
        projectile.currentPos = next;
        transform.SetPosition(next);
    }

    const auto skillProjectiles =
        DeterministicEntityIterator<SkillProjectileComponent>::CollectSorted(m_world);

    for (EntityID entity : skillProjectiles)
    {
        if (!m_world.IsAlive(entity) ||
            !m_world.HasComponent<SkillProjectileComponent>(entity) ||
            !m_world.HasComponent<TransformComponent>(entity))
        {
            continue;
        }

        auto& projectile = m_world.GetComponent<SkillProjectileComponent>(entity);
        auto& transform = m_world.GetComponent<TransformComponent>(entity);

        if (!projectile.bSpawned)
        {
            const NetEntityId projectileNet = m_entityMap.IssueNew(entity);
            NetEntityIdComponent net{};
            net.netId = projectileNet;
            if (!m_world.HasComponent<NetEntityIdComponent>(entity))
                m_world.AddComponent<NetEntityIdComponent>(entity, net);

            projectile.bSpawned = true;

            ReplicatedEventComponent spawn{};
            spawn.kind = eReplicatedEventKind::ProjectileSpawn;
            spawn.sourceEntity = projectile.sourceEntity;
            spawn.targetEntity = projectile.targetEntity;
            spawn.projectileEntity = entity;
            spawn.projectileKind = static_cast<u16_t>(projectile.kind);
            spawn.position = projectile.currentPos;
            spawn.direction = projectile.direction;
            spawn.speed = projectile.speed;
            spawn.maxDistance = projectile.maxDistance;
            spawn.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, spawn);
            if (!IsServerMinionRangedProjectileKind(projectile.kind))
                LogSkillProjectileEvent(
                    "spawn",
                    entity,
                    projectile,
                    projectile.targetEntity,
                    projectile.currentPos);
            continue;
        }

        if (!IsAliveHealth(m_world, projectile.sourceEntity) ||
            projectile.speed <= 0.f ||
            projectile.maxDistance <= 0.f)
        {
            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.projectileEntity = entity;
            hit.projectileKind = static_cast<u16_t>(projectile.kind);
            hit.position = projectile.currentPos;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            m_world.DestroyEntity(entity);
            continue;
        }

        if (projectile.targetEntity != NULL_ENTITY)
        {
            const bool_t bTargetAlive =
                m_world.IsAlive(projectile.targetEntity) &&
                m_world.HasComponent<TransformComponent>(projectile.targetEntity) &&
                IsAliveHealth(m_world, projectile.targetEntity) &&
                GameplayStateQuery::CanReceiveProjectileHit(
                    m_world,
                    projectile.sourceEntity,
                    projectile.targetEntity);

            if (!bTargetAlive)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = projectile.currentPos;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const Vec3 targetPos =
                m_world.GetComponent<TransformComponent>(projectile.targetEntity).GetPosition();
            const Vec3 targetAim{
                targetPos.x,
                targetPos.y + kServerMinionRangedProjectileTargetHeight,
                targetPos.z
            };
            const Vec3 delta{
                targetAim.x - projectile.currentPos.x,
                targetAim.y - projectile.currentPos.y,
                targetAim.z - projectile.currentPos.z
            };
            const f32_t distSq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            const f32_t hitRadiusSq = projectile.hitRadius * projectile.hitRadius;

            if (distSq <= hitRadiusSq)
            {
                DamageRequest request{};
                request.source = projectile.sourceEntity;
                request.target = projectile.targetEntity;
                request.sourceTeam = projectile.sourceTeam;
                request.type = eDamageType::Physical;
                request.flatAmount = projectile.damage;
                request.skillId = projectile.skillId;
                request.rank = projectile.rank;
                request.flags = DamageFlag_OnHit;
                EnqueueDamageRequest(m_world, request);

                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = projectile.targetEntity;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = targetAim;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t dist = std::sqrt(distSq);
            if (dist <= std::numeric_limits<f32_t>::epsilon())
            {
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
            if (remaining <= 0.f)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = projectile.currentPos;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
                continue;
            }

            const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
            const f32_t actualStep = (step >= dist) ? dist : step;
            const f32_t t = actualStep / dist;
            const Vec3 next{
                projectile.currentPos.x + delta.x * t,
                projectile.currentPos.y + delta.y * t,
                projectile.currentPos.z + delta.z * t
            };
            projectile.currentPos = next;
            projectile.traveledDistance += actualStep;
            transform.SetPosition(next);

            if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
            {
                ReplicatedEventComponent hit{};
                hit.kind = eReplicatedEventKind::ProjectileHit;
                hit.sourceEntity = projectile.sourceEntity;
                hit.targetEntity = NULL_ENTITY;
                hit.projectileEntity = entity;
                hit.projectileKind = static_cast<u16_t>(projectile.kind);
                hit.position = next;
                hit.bDestroyed = true;
                hit.startTick = tc.tickIndex;
                EnqueueReplicatedEvent(m_world, hit);
                m_world.DestroyEntity(entity);
            }
            continue;
        }

        const Vec3 start = projectile.currentPos;
        const f32_t remaining = projectile.maxDistance - projectile.traveledDistance;
        const f32_t step = std::min(projectile.speed * tc.fDt, remaining);
        const Vec3 end{
            start.x + projectile.direction.x * step,
            start.y + projectile.direction.y * step,
            start.z + projectile.direction.z * step
        };

        Vec3 hitPos = end;
        const EntityID target = FindSkillProjectileHitTarget(
            m_world,
            projectile,
            start,
            end,
            hitPos);

        if (target != NULL_ENTITY)
        {
            if (projectile.kind == eProjectileKind::Tornado)
                YasuoGameSim::ApplyTornadoAirborne(m_world, tc, projectile.sourceEntity, target);
            if (projectile.kind == eProjectileKind::LeeSinQ)
                LeeSinGameSim::ApplySonicWaveMark(m_world, projectile.sourceEntity, target);
            if (projectile.kind == eProjectileKind::SylasChain)
                SylasGameSim::ApplyChainHit(m_world, tc, projectile.sourceEntity, target);
            if (projectile.bApplyOnHitStatus)
                GameplayStatus::ApplyStatusEffect(m_world, target, projectile.onHitStatus, tc);

            DamageRequest request{};
            request.source = projectile.sourceEntity;
            request.target = target;
            request.sourceTeam = projectile.sourceTeam;
            request.type = projectile.kind == eProjectileKind::SylasChain
                ? eDamageType::Magic
                : eDamageType::Physical;
            request.flatAmount = projectile.damage;
            request.skillId = projectile.skillId;
            request.rank = projectile.rank;
            request.flags = DamageFlag_OnHit;
            EnqueueDamageRequest(m_world, request);

            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.targetEntity = target;
            hit.projectileEntity = entity;
            hit.projectileKind = static_cast<u16_t>(projectile.kind);
            hit.position = hitPos;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            LogSkillProjectileEvent("hit", entity, projectile, target, hitPos);
            m_world.DestroyEntity(entity);
            continue;
        }

        projectile.currentPos = end;
        projectile.traveledDistance += step;
        transform.SetPosition(end);

        if (projectile.traveledDistance >= projectile.maxDistance - 0.001f)
        {
            ReplicatedEventComponent hit{};
            hit.kind = eReplicatedEventKind::ProjectileHit;
            hit.sourceEntity = projectile.sourceEntity;
            hit.projectileEntity = entity;
            hit.projectileKind = static_cast<u16_t>(projectile.kind);
            hit.position = end;
            hit.bDestroyed = true;
            hit.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(m_world, hit);
            LogSkillProjectileEvent("expire", entity, projectile, NULL_ENTITY, end);
            m_world.DestroyEntity(entity);
        }
    }
}

u64_t CGameRoom::ResolveServerGameTimeMs(u64_t iServerTick)
{
    return (iServerTick * 1000ull) / DeterministicTime::kTicksPerSecond;
}

bool CGameRoom::DebugSetHealthByNetId(NetEntityId netId, f32_t value)
{
    std::lock_guard stateLock(m_stateMutex);

    const EntityID entity = m_entityMap.FromNet(netId);
    if (entity == NULL_ENTITY || !m_world.HasComponent<HealthComponent>(entity))
        return false;

    auto& health = m_world.GetComponent<HealthComponent>(entity);
    health.fCurrent = value;
    health.bIsDead = (health.fCurrent <= 0.f);

    if (m_world.HasComponent<ChampionComponent>(entity))
    {
        auto& champion = m_world.GetComponent<ChampionComponent>(entity);
        champion.hp = health.fCurrent;
    }

    char msg[160]{};
    sprintf_s(msg,
        "[GameRoom] Debug hp netId=%u entity=%u value=%.2f\n",
        netId,
        static_cast<u32_t>(entity),
        value);
    OutputServerAITrace(msg);
    return true;
}
