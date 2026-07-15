#include "Game/GameRoom.h"

#include "Game/ServerMinionTuning.h"
#include "Game/SnapshotBuilder.h"
#include "Game/ReplayRecorder.h"
#include "GameRoomSmokeRoster.h"
#include "GameRoomInternal.h"
#include "Network/PacketDispatcher.h"
#include "Network/Session.h"
#include "Network/Session_Manager.h"
#include "Server/Private/Data/LoLGameplayDefinitionPack.h"
#include "Server/Private/Data/RuntimeGameplayDefinitionOverlay.h"
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
#include "Shared/GameSim/Champions/Ezreal/EzrealGameSim.h"
#include "Shared/GameSim/Champions/Garen/GarenGameSim.h"
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
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/GameSim/Definitions/StageData.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Shared/GameSim/Registries/Reward/RewardRegistry.h"

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
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "ECS/SpatialIndex.h"
#include "ECS/Systems/SpatialHashSystem.h"
#include "Shared/GameSim/Systems/Turret/TurretAISystem.h"
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

    void ResetChampionAIForLifecycle(
        ChampionAIComponent& ai,
        eChampionAIState state,
        eChampionAIAction action,
        f32_t decisionDelay)
    {
        ai.activeLane = ai.lane;
        ai.state = state;
        ai.lastAction = action;
        ai.intent = eChampionAIIntent::FarmMinion;
        ai.retreatGoal = ai.safeAnchor;

        ai.lockedChampion = NULL_ENTITY;
        ai.targetMinion = NULL_ENTITY;
        ai.targetStructure = NULL_ENTITY;
        ai.alliedWave = NULL_ENTITY;
        ai.comboTarget = NULL_ENTITY;
        ai.lowHpEnemyChampion = NULL_ENTITY;
        ai.diveTarget = NULL_ENTITY;
        ai.lastSeenEnemyChampion = NULL_ENTITY;
        ai.lastSeenEnemyChampionPos = {};
        ai.lastSeenEnemyChampionTick = 0u;
        ai.divePhase = eChampionAIDivePhase::None;
        ai.comboStep = 0u;
        ai.diveExtraBACount = 0u;

        ai.decisionTimer = decisionDelay;
        ai.intentHoldTimer = 0.f;
        ai.fPostComboBATimer = 0.f;
        ai.fDiveExtraBATimer = 0.f;
        ai.fSkillCastCooldownTimer = 0.f;

        ai.fRetreatDecisionScore = 0.f;
        ai.fChampionDecisionScore = 0.f;
        ai.fFarmDecisionScore = 0.f;
        ai.fStructureDecisionScore = 0.f;
        ai.fDecisionSelfHpRatio = 1.f;
        ai.fDecisionEnemyHpRatio = 1.f;
        ai.fDecisionEnemyDistance = 999.f;
        ai.fDecisionAttackRange = 1.5f;
        ai.fDecisionTurretDanger = 0.f;
        ai.fDecisionLowHpEnemyRatio = 1.f;
        ai.fDecisionLowHpEnemyDistance = 999.f;
        ai.fDecisionChampionScanRange = 0.f;
        ai.fDecisionDiveScanRange = 0.f;
        ai.fDecisionFlashRange = 0.f;

        ai.debugLastCommandKind = 0u;
        ai.debugLastCommandSlot = 0u;
        ai.debugLastCommandTarget = NULL_ENTITY;
        ai.debugLastCommandPos = {};
        ai.debugLastBlockReason = eChampionAIDecisionBlockReason::None;
        for (ChampionAIDecisionTraceEntry& entry : ai.debugDecisionTrace)
            entry = {};
        ai.debugDecisionTraceHead = 0u;
        ai.debugDecisionTraceCount = 0u;
        ai.debugAvailableActionMask = 0u;
        ai.debugAvailableSkillMask = 0u;
        ai.debugControlMode = eChampionAIDebugControlMode::Observe;
        ai.debugForcedAction = eChampionAIAction::FollowWave;
        ai.debugForcedSkillSlot = 0u;
        ai.debugForcedDecisionCount = 0u;
        ai.bDebugForceAction = false;

        ai.bWaveJoined = false;
        ai.bStructureWaveTanking = false;
        ai.bInsideEnemyTurretDanger = false;
        ai.bCanAttackChampion = false;
        ai.bPostComboBAAllowed = false;
        ai.bMidDefenseActive = false;
    }
}

std::unique_ptr<CGameRoom> CGameRoom::Create(
    u32_t roomId,
    std::shared_ptr<const ChampionAIShadowPolicyArtifactV1> shadowPolicy)
{
    auto room = std::unique_ptr<CGameRoom>(
        new CGameRoom(roomId, std::move(shadowPolicy)));
    room->m_pExecutor = CDefaultCommandExecutor::Create();
    room->m_pSnapBuilder = CSnapshotBuilder::Create();
    room->m_pLagCompensation = std::make_unique<CLagCompensation>();
    room->m_pReplayRecorder = CReplayRecorder::Create(roomId, 30);
    room->InitializeServerSimSystems();
    return room;
}

CGameRoom::CGameRoom(
    u32_t roomId,
    std::shared_ptr<const ChampionAIShadowPolicyArtifactV1> shadowPolicy)
    : m_roomId(roomId)
    , m_pShadowPolicy(std::move(shadowPolicy))
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
            respawn.bDeathCredited = false;
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
            const SpawnLoadoutPolicyDef& spawnPolicy =
                ServerData::GetActiveLoLSpawnObjectDefinitionPack().spawnLoadout;
            respawn.bPending = true;
            respawn.respawnDelay = spawnPolicy.respawnDelaySec;
            respawn.respawnTimer = spawnPolicy.respawnDelaySec;

            GameplayStatus::ClearStatusEffects(m_world, entity);

            if (m_world.HasComponent<TargetableTag>(entity))
                m_world.RemoveComponent<TargetableTag>(entity);

            if (m_world.HasComponent<MoveTargetComponent>(entity))
            {
                auto& moveTarget = m_world.GetComponent<MoveTargetComponent>(entity);
                moveTarget.bHasTarget = false;
                moveTarget.pathCount = 0;
                moveTarget.pathIndex = 0;
                moveTarget.blockedMoveTicks = 0;
                moveTarget.bestMoveDistance = -1.f;
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
                ResetChampionAIForLifecycle(
                    ai,
                    eChampionAIState::Dead,
                    eChampionAIAction::Retreat,
                    0.f);
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
        champion.mana = champion.maxMana;
        transform.SetPosition(respawn.spawnPos);
        PositionDiscontinuityComponent& discontinuity =
            m_world.HasComponent<PositionDiscontinuityComponent>(entity)
                ? m_world.GetComponent<PositionDiscontinuityComponent>(entity)
                : m_world.AddComponent<PositionDiscontinuityComponent>(
                    entity,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = tc.tickIndex;

        if (!m_world.HasComponent<TargetableTag>(entity))
            m_world.AddComponent<TargetableTag>(entity);
        if (m_world.HasComponent<MoveTargetComponent>(entity))
        {
            auto& moveTarget = m_world.GetComponent<MoveTargetComponent>(entity);
            moveTarget.bHasTarget = false;
            moveTarget.pathCount = 0;
            moveTarget.pathIndex = 0;
            moveTarget.blockedMoveTicks = 0;
            moveTarget.bestMoveDistance = -1.f;
        }

        if (m_world.HasComponent<ChampionAIComponent>(entity))
        {
            auto& ai = m_world.GetComponent<ChampionAIComponent>(entity);
            ResetChampionAIForLifecycle(
                ai,
                eChampionAIState::MoveToOuterTurret,
                eChampionAIAction::MoveToSafeAnchor,
                0.25f);
        }

        respawn.bPending = false;
        respawn.respawnTimer = 0.f;
        respawn.bDeathCredited = false;

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
    m_world.Initialize_Spatial(DefaultSpatialGridDesc());
    EnsureMatchScoreEntity(m_world);
    m_pSpatialSystem = Engine::CSpatialHashSystem::Create();
    m_pTurretAI = GameplayTurret::CTurretAISystem::Create();

    RegisterDefaultChampionSkillScalingTables();

    // 활성 정의 팩의 경제 값으로 보상/XP 레지스트리 재적재 (팩 미장착 시 ctor 기본값 유지).
    if (const EconomyGameplayDef* pEconomy =
        ServerData::GetActiveLoLGameplayDefinitionPack().FindEconomy())
    {
        CRewardRegistry::Instance().LoadFromEconomyDef(*pEconomy);
    }

    // 활성 정의 팩의 아이템 값으로 아이템 레지스트리 재적재 (팩 미장착 시 컴파일 기본 표 유지).
    std::size_t itemDefCount = 0u;
    if (const ItemDef* pItemDefs =
        ServerData::GetActiveLoLGameplayDefinitionPack().FindItems(itemDefCount))
    {
        CItemRegistry::Instance().LoadFromItemDefs(pItemDefs, itemDefCount);
    }

    AnnieGameSim::RegisterHooks();
    AsheGameSim::RegisterHooks();
    FioraGameSim::RegisterHooks();
    EzrealGameSim::RegisterHooks();
    GarenGameSim::RegisterHooks();
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

void CGameRoom::ResetMatchStateLocked()
{
    // 월드 통째 교체 — 파괴된 구조물/미니언/챔피언/이벤트 엔티티 전부 소멸.
    // 스테이지/내비/웨이포인트/구조물 스폰은 SpawnServerGameplayObjects가 자기완결이라
    // m_bGameplayObjectsSpawned 가드만 풀면 다음 매치 시작(bBeginLoading) 때 재구축된다.
    m_world = CWorld{};
    m_world.Initialize_Spatial(DefaultSpatialGridDesc());
    EnsureMatchScoreEntity(m_world);

    m_entityMap = EntityIdMap{};
    m_rng = DeterministicRng{ 0xC0FFEEull };
    m_tickIndex = 0;
    m_visibleTickIndex.store(0, std::memory_order_relaxed);

    m_pExecutor = CDefaultCommandExecutor::Create();
    m_pSnapBuilder = CSnapshotBuilder::Create();
    m_pLagCompensation = std::make_unique<CLagCompensation>();
    m_pReplayRecorder = CReplayRecorder::Create(m_roomId, 30);
    m_bReplayFinalized = false;
    m_bGameEnded = false;

    m_pSpatialSystem = Engine::CSpatialHashSystem::Create();
    m_pTurretAI = GameplayTurret::CTurretAISystem::Create();

    m_commandIngress.Clear();
    m_pendingExecCommands.clear();
    m_pendingReplayCommands.clear();
    m_bPracticeModeEnabled = false;
    m_PracticeSpawnedEntities.clear();
    m_PendingPracticeControlChange = {};

    m_bSimPaused = false;
    m_simStepBudget = 0;
    m_simSpeedMul.store(1.f, std::memory_order_relaxed);
    m_timelineEpoch = 1;
    m_timelineBranchId = 1;
    m_lastReplaySnapshotTick = ~0ull;
    m_lastReplayToolRevision = ~0ull;
    m_keyframes.clear();
    m_pendingRewindToTick = 0;

    m_sessionBinding = CSessionBinding{};
    m_lastBroadcastActionSeq.clear();
    m_lastSimCommandSeqBySession.clear();

    m_bGameplayObjectsSpawned = false;
    m_serverMinionWaves.Clear();

    // 로비를 SeatSelect부터 다시 — 다음 접속은 첫 게임과 동일 경로를 탄다.
    InitializeLobbyAuthority();

    OutputServerAITrace("[GameRoom] Match reset after game end; lobby back to SeatSelect\n");
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
