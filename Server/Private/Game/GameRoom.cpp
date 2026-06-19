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
    InitializeLobbyAuthority();
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

bool CGameRoom::IsInGamePhase() const
{
    return m_pLobbyAuthority &&
        m_pLobbyAuthority->GetPhase() == eRoomPhase::InGame;
}

void CGameRoom::Phase_ServerDeathAndRespawn(TickContext& tc)
{
    if (!IsInGamePhase())
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

            StartReplicatedAction(m_world, entity, eActionStateId::DeathStart, tc);
            SetReplicatedPose(m_world, entity, ePoseStateId::Dead, tc);

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

        SetReplicatedPose(m_world, entity, ePoseStateId::Idle, tc);

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
