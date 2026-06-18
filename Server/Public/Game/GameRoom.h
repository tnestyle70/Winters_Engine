#pragma once

#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameContext.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Game/ServerMinionWaveRuntime.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace flatbuffers { class DetachedBuffer; }
namespace Shared::Schema { struct CommandBatch; struct LobbyCommand; }
namespace Winters::Map
{
    struct StageData;
    struct StructureEntry;
    struct JungleEntry;
}
namespace Engine
{
    class CSpatialHashSystem;
    class CTurretAISystem;
    class CMapSurfaceSampler;
    class CNavGrid;
}

class CSnapshotBuilder;
struct ReplicatedEventComponent;
class CLagCompensation;
//Replay
class CReplayRecorder;

struct PendingCommand
{
    u32_t sessionId = 0;
    u32_t sequenceNum = 0;
    GameCommandWire wire{};
    u64_t acceptedTick = 0;
    u64_t recvTimeMs = 0;
    u64_t clientTimestampMs = 0;
};

enum class eRoomPhase : u8_t
{
    SeatSelect,
    ChampionSelect,
    Loading,
    InGame,
};

struct LobbySlotState
{
    u8_t slotId = kInvalidGameRosterSlot;
    u8_t team = 0;
    bool_t bHuman = false;
    bool_t bBot = false;
    u32_t sessionId = 0;
    NetEntityId netId = NULL_NET_ENTITY;
    eChampion champion = eChampion::END;
    u8_t botDifficulty = 2;
    u8_t botLane = kGameRosterDefaultBotLane;
    bool_t bReady = false;
    bool_t bLocked = false;
    //Sylas 더미
    bool_t bDummy = false;
};

class CGameRoom final : public IWalkableQuery
{
public:
    static std::unique_ptr<CGameRoom> Create(u32_t roomId);
    ~CGameRoom();

    void Start();
    void Stop();

    void OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch);
    void EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
        u64_t acceptedTick, u64_t recvTimeMs, u64_t clientTimestampMs);
    void OnLobbyCommand(u32_t sessionId, const Shared::Schema::LobbyCommand* command);

    EntityID OnSessionJoin(u32_t sessionId);
    void OnSessionLeave(u32_t sessionId);
    bool DebugSetHealthByNetId(NetEntityId netId, f32_t value);

    bool_t IsWalkableXZ(const Vec3& pos) const override;
    bool_t SegmentWalkableXZ(const Vec3& from, const Vec3& to, f32_t radiusWorld = 0.f) const override;
    bool_t TryClampMoveSegmentXZ(const Vec3& vFrom, const Vec3& vDesired, f32_t fRadiusWorld, Vec3& vOutPosition) const override;
    bool_t TryResolveMoveTarget(const Vec3& from, const Vec3& rawTarget, Vec3& outTarget) const override;
    bool_t TryBuildMovePath(
        const Vec3& from,
        const Vec3& rawTarget,
        Vec3* pOutWaypoints,
        u16_t maxWaypoints,
        u16_t& outWaypointCount,
        Vec3& outTarget) const override;
    bool_t TrySampleHeight(f32_t x, f32_t z, f32_t& outY) const override;

    u32_t GetRoomId() const { return m_roomId; }
    u64_t GetCurrentTickIndex() const
    {
        return m_visibleTickIndex.load(std::memory_order_relaxed);
    }
    bool IsRunning() const { return m_bRunning.load(std::memory_order_relaxed); }

private:
    CGameRoom(u32_t roomId);
    static u64_t ResolveServerGameTimeMs(u64_t iServerTick);

    void TickThread();
    void Tick();

    //Replay
    void FinalizeReplayRecorder();

    void Phase_DrainCommands(TickContext& tc);
    void Phase_ExecuteCommands(TickContext& tc);
    void Phase_ServerBotAI(TickContext& tc);
    void Phase_SimulationSystems(TickContext& tc);
    void Phase_ServerMinionWave(TickContext& tc);
    void Phase_ServerMinionAI(TickContext& tc);
    void Phase_ServerTurretAI(TickContext& tc);
    void Phase_ServerMinionDepenetration(TickContext& tc);
    void Phase_ServerProjectiles(TickContext& tc);
    void Phase_ServerDeathAndRespawn(TickContext& tc);
    void Phase_BroadcastEvents(TickContext& tc);
    void Phase_BroadcastSnapshot(TickContext& tc);
    void BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence);
    void BroadcastReplicatedEvent(const ReplicatedEventComponent& event, TickContext& tc);

    EntityID SpawnChampionForLobbySlot(LobbySlotState& slot);
    void SpawnChampionsFromLobby();
    void InitializeServerSimSystems();
    void InitializeServerWalkableGrid(const Winters::Map::StageData* pStage, const wchar_t* pStagePath);
    void CarveServerStructuresOnNavGrid();
    void BuildServerPathNavGrid();
    bool_t LoadServerStageData(Winters::Map::StageData& outStage, std::wstring& outPath) const;
    void CacheServerMinionWaypoints(const Winters::Map::StageData& stage);
    void RebuildServerMinionFlowFields();
    void SpawnServerGameplayObjects();
    EntityID SpawnServerStructureFromStageEntry(const Winters::Map::StructureEntry& entry);
    EntityID SpawnServerJungleFromStageEntry(const Winters::Map::JungleEntry& entry);
    EntityID SpawnServerStructure(eTeam team, u32_t kind, u32_t tier, u32_t lane,
        const Vec3& pos, const Vec3& rotation, f32_t maxHp, f32_t scale,
        bool_t bTurret, bool_t bNexus, bool_t bInhibitor);
    EntityID SpawnServerMinion(eTeam team, u8_t roleType, u8_t lane, const Vec3& pos);

    //Minion Move&Spawn Logic Change
    bool_t TryResolveServerWalkablePosition(const Vec3& vRawPos,
        int32_t maxRadius, Vec3& vOutPos) const;
    void SanitizeServerMoversOnNavGrid();
    void SanitizeServerWaypointPatrolsOnNavGrid();
    void SanitizeServerMinionWaypointsOnNavGrid();
    u32_t ResolveServerMinionStartWaypoint(eTeam team, u8_t lane, const Vec3& vSpawnPos) const;
    bool_t TryMoveServerMinionToward(
        EntityID entity,
        MinionStateComponent& state,
        TransformComponent& transform,
        const Vec3& vTarget,
        f32_t fArriveRadius,
        TickContext& tc,
        u32_t& PathBuildBudget,
        bool_t& outMoved,
        MinionStateComponent::State moveState);
    bool_t TryMoveServerMinionByFlowFields(
        EntityID entity,
        MinionStateComponent& state,
        TransformComponent& transform,
        const Vec3& vLaneTarget,
        TickContext& tc,
        bool_t& outMoved);
    bool_t TryResolveMinionMoveStep(
        EntityID entity,
        const Vec3& vPos,
        const Vec3& vDesiredDir,
        f32_t fStep,
        const TickContext& tc,
        Vec3& vOutNext);
    bool_t TryResolveMinionDepenetrationStep(
        EntityID entity,
        const Vec3& vPos,
        f32_t fStep,
        const TickContext& tc,
        Vec3& vOutNext);
    bool_t TryBuildServerMinionMovePath(
        const Vec3& vStart,
        const Vec3& vGoal,
        Vec3* pOutWaypoints,
        u16_t maxWayPoints,
        u16_t& outCount,
        Vec3& outResolvedGoal) const;

    u32_t GetServerMinionWaypointCount(eTeam team, u8_t lane) const;
    Vec3 GetServerMinionWaypoint(eTeam team, u8_t lane, u32_t index) const;
    u8_t ResolveServerStructureLane(eTeam team, u32_t kind, u32_t tier, const Vec3& pos) const;
    u8_t ResolveInitialBotLane(const LobbySlotState& slot) const;
    Vec3 ResolveChampionAILaneGoal(eTeam team, u8_t lane) const;
    Vec3 ResolveChampionAISafeAnchor(eTeam team, u8_t lane);
    void RefreshChampionAIGoals();

    void InitializeLobbySlots();
    u8_t FindFirstEmptyLobbySlot(u32_t beginSlot, u32_t endSlot) const;
    void CompactLobbyTeamSlotsLocked(u32_t beginSlot, u32_t endSlot);
    void OnLobbyJoin(u32_t sessionId);
    bool TryJoinSlot(u32_t sessionId, u8_t slotId);
    bool TryLeaveSlot(u32_t sessionId);
    bool TryPickChampion(u32_t sessionId, eChampion champion);
    bool TrySetBotChampion(u32_t sessionId, u8_t slotId, eChampion champion);
    bool TrySetBotDifficulty(u32_t sessionId, u8_t slotId, u8_t difficulty);
    bool TrySetBotLane(u32_t sessionId, u8_t slotId, u8_t lane);
    bool TryAdvanceToChampionSelect(u32_t sessionId);
    bool TryStartGame(u32_t sessionId);

    bool TrySetReady(u32_t sessionId, bool_t bReady);
    bool TryStopReplay(u32_t sessionId);
    bool AreAllActiveHumanSlotsReady() const;
    void BeginInGameLocked(u32_t sessionId);

    bool CanEditBots(u32_t sessionId) const;
    void SetLobbyMessageLocked(const std::string& message);
    void SetLobbyMessageLocked(const char* message);
    void BroadcastLobbyStateLocked();
    void BroadcastGameStartLocked();
    void SendGameStartToSessionLocked(u32_t sessionId);
    void SendHelloToSessionLocked(u32_t sessionId, NetEntityId netId, eChampion champion, u8_t team);
    bool TryAttachSessionToDisconnectedHumanSlot(u32_t sessionId, EntityID& outEntity);
    Vec3 GetSpawnPositionForLobbySlot(const LobbySlotState& slot) const;
    EntityID ResolveControlledEntityForSession(u32_t sessionId);

    u32_t m_roomId = 0;
    std::atomic<bool> m_bRunning{ false };
    std::thread m_tickThread;

    std::mutex m_stateMutex;
    CWorld m_world;
    EntityIdMap m_entityMap;
    DeterministicRng m_rng{ 0xC0FFEEull };
    u64_t m_tickIndex = 0;
    std::atomic<u64_t> m_visibleTickIndex{ 0 };

    std::unique_ptr<ICommandExecutor> m_pExecutor;
    std::unique_ptr<CSnapshotBuilder> m_pSnapBuilder;
    std::unique_ptr<CLagCompensation> m_pLagCompensation;
    //Replay
    std::unique_ptr<CReplayRecorder> m_pReplayRecorder;
    bool_t m_bReplayFinalized = false;

    std::unique_ptr<Engine::CSpatialHashSystem> m_pSpatialSystem;
    std::unique_ptr<Engine::CTurretAISystem> m_pTurretAI;
    std::unique_ptr<Engine::CMapSurfaceSampler> m_pMapSurfaceSampler;
    std::unique_ptr<Engine::CNavGrid> m_pNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pPathNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pMinionLaneNavGrid;

    std::mutex m_pendingMutex;
    std::vector<PendingCommand> m_pendingCommands;
    std::vector<GameCommand> m_pendingExecCommands;

    std::vector<u32_t> m_sessionIds;
    std::unordered_map<u32_t, EntityID> m_sessionToEntity;
    std::unordered_map<u32_t, u8_t> m_sessionToSlot;
    std::unordered_map<EntityID, u32_t> m_lastBroadcastActionSeq;
    std::unordered_map<u32_t, u32_t> m_lastSimCommandSeqBySession;

    eRoomPhase m_roomPhase = eRoomPhase::SeatSelect;
    u32_t m_hostSessionId = 0;
    u32_t m_lobbyRevision = 0;
    std::string m_strLastLobbyMessage;
    bool_t m_bAllPlayersCanEditBots = true;
    bool_t m_bGameplayObjectsSpawned = false;
    CServerMinionWaveRuntime m_serverMinionWaves{};
    LobbySlotState m_lobbySlots[kGameRosterSlotCount]{};
};
