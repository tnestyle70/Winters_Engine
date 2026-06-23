#pragma once

#include "ECS/Entity.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameContext.h"
#include "Game/CommandIngress.h"
#include "Game/LobbyAuthority.h"
#include "Game/SessionBinding.h"
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
class CLagCompensation;
//Replay
class CReplayRecorder;

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
    bool IsInGamePhase() const;

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
    Vec3 ResolveChampionAILaneGoal(eTeam team, u8_t lane) const;
    Vec3 ResolveChampionAISafeAnchor(eTeam team, u8_t lane);
    void RefreshChampionAIGoals();

    void InitializeLobbyAuthority();
    void ApplyLobbyAuthorityResult(const LobbyAuthorityResult& result);
    void TraceLobbyMessageLocked() const;
    void BroadcastLobbyStateLocked();
    void BroadcastGameStartLocked();
    void SendGameStartToSessionLocked(u32_t sessionId);
    void SendHelloToSessionLocked(u32_t sessionId, NetEntityId netId, eChampion champion, u8_t team);
    bool TryAttachSessionToDisconnectedHumanSlot(u32_t sessionId, EntityID& outEntity, LobbyAuthorityResult& outResult);
    Vec3 GetSpawnPositionForLobbySlot(const LobbySlotState& slot) const;
    LobbySlotState* GetLobbySlots();
    const LobbySlotState* GetLobbySlots() const;
    u32_t GetLobbySlotCount() const;

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
    std::unique_ptr<CLobbyAuthority> m_pLobbyAuthority;
    //Replay
    std::unique_ptr<CReplayRecorder> m_pReplayRecorder;
    bool_t m_bReplayFinalized = false;

    std::unique_ptr<Engine::CSpatialHashSystem> m_pSpatialSystem;
    std::unique_ptr<Engine::CTurretAISystem> m_pTurretAI;
    std::unique_ptr<Engine::CMapSurfaceSampler> m_pMapSurfaceSampler;
    std::unique_ptr<Engine::CNavGrid> m_pNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pPathNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pMinionLaneNavGrid;

    CCommandIngress m_commandIngress;
    std::vector<GameCommand> m_pendingExecCommands;

    std::vector<u32_t> m_sessionIds;
    CSessionBinding m_sessionBinding;
    std::unordered_map<EntityID, u32_t> m_lastBroadcastActionSeq;
    std::unordered_map<u32_t, u32_t> m_lastSimCommandSeqBySession;

    bool_t m_bGameplayObjectsSpawned = false;
    CServerMinionWaveRuntime m_serverMinionWaves{};
};
