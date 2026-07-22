#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Game/CommandIngress.h"
#include "Game/LobbyAuthority.h"
#include "Game/SessionBinding.h"
#include "Shared/GameSim/Core/Determinism/DeterministicRng.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Definitions/PracticeMinionAttackDamagePolicy.h"
#include "Shared/GameSim/Definitions/SpawnLoadoutPolicyDef.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/Replay/ReplayFormat.h"
#include "Game/ServerMinionWaveRuntime.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <array>
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
    class CMapSurfaceSampler;
    class CNavGrid;
}
namespace GameplayTurret
{
    class CTurretAISystem;
}

class CSnapshotBuilder;
class CLagCompensation;
class CGameRoomIntegrationProbeAccess;
struct ChampionAIShadowPolicyArtifactV1;
//Replay
class CReplayRecorder;

class CGameRoom final : public IWalkableQuery
{
public:
    static std::unique_ptr<CGameRoom> Create(
        u32_t roomId,
        std::shared_ptr<const ChampionAIShadowPolicyArtifactV1> shadowPolicy = {});
    ~CGameRoom();

    void Start();
    void Stop();

    void OnCommandBatch(u32_t sessionId, const Shared::Schema::CommandBatch* batch);
    void EnqueueCommand(u32_t sessionId, const GameCommandWire& wire,
        u64_t acceptedTick, u64_t recvTimeMs, u64_t clientTimestampMs);
    void OnLobbyCommand(u32_t sessionId, const Shared::Schema::LobbyCommand* command);

    EntityID OnSessionJoin(u32_t sessionId, bool_t* pOutAccepted = nullptr);
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
    void SetCompletedGameSessionGenerationFloor(u64_t generation);
    u64_t GetCurrentTickIndex() const
    {
        return m_visibleTickIndex.load(std::memory_order_relaxed);
    }
    bool IsRunning() const { return m_bRunning.load(std::memory_order_relaxed); }

private:
    friend class CGameRoomIntegrationProbeAccess;
    CGameRoom(
        u32_t roomId,
        std::shared_ptr<const ChampionAIShadowPolicyArtifactV1> shadowPolicy = {});
    static u64_t ResolveServerGameTimeMs(u64_t iServerTick);

    void TickThread();
    void Tick();
    bool IsInGamePhase() const;

    //Replay
    void FinalizeReplayRecorder();

    // S035: 게임종료 후 마지막 세션 이탈 시 룸을 새 매치 대기 상태(SeatSelect)로 리셋.
    // m_stateMutex를 잡은 문맥에서만 호출한다.
    void ResetMatchStateLocked();
    bool_t TryResetCompletedMatchLocked();

    void Phase_DrainCommands(TickContext& tc);
    void Phase_ExecuteCommands(TickContext& tc);
    void Phase_ServerBotAI(TickContext& tc);
    void Phase_SimulationSystems(TickContext& tc);
    void Phase_ServerMinionWave(TickContext& tc);
    void Phase_ServerUnitAI(TickContext& tc);
    void Phase_ServerTurretAI(TickContext& tc);
    void Phase_ServerMinionDepenetration(TickContext& tc);
    void Phase_ServerProjectiles(TickContext& tc);
    void Phase_ServerDeathAndRespawn(TickContext& tc);
    void Phase_CheckGameEnd(TickContext& tc);
    void Phase_BroadcastEvents(TickContext& tc);
    void Phase_BroadcastSnapshot(TickContext& tc);
    void BroadcastEventPayload(const u8_t* payload, u32_t payloadSize, u32_t sequence);
    void RecordReplayCommand(
        u64_t tick,
        const PendingCommand& pending,
        Winters::Replay::eReplayCommandDomain domain,
        Winters::Replay::eReplayJournalOutcome outcome);
    void RecordPendingReplayCommand(
        u64_t tick,
        const GameCommand& command,
        Winters::Replay::eReplayJournalOutcome outcome);
    bool_t TryHandleTeamPing(
        const TickContext& tc,
        const GameCommand& cmd,
        bool_t& outAccepted);
    bool_t TryHandlePracticeControl(
        const TickContext& tc,
        const GameCommand& cmd,
        bool_t& outAccepted);
    bool_t TryHandleAIDebugControl(
        const TickContext& tc,
        const GameCommand& cmd,
        bool_t& outAccepted);
    void TickPracticeControls(const TickContext& tc);
    void ClearPracticeSpawns();
    void ClearPracticeMinionAttackDamageOverrides();
    bool_t CommitPendingPracticeControlChange(const TickContext& tc);
    void CancelPendingPracticeControlChange(u64_t tick);
    void TickPausedControlLane();
    void CaptureKeyframeIfDue(const TickContext& tc);
    void PerformPendingRewind();

    EntityID SpawnChampionForLobbySlot(LobbySlotState& slot);
    void ConfigureChampionControlRole(
        EntityID entity,
        const LobbySlotState& slot);
    void SpawnChampionsFromLobby();
    void InitializeServerSimSystems();
    void InitializeServerWalkableGrid(const Winters::Map::StageData* pStage, const wchar_t* pStagePath);
    bool_t CaptureServerTerrainNavGrid();
    u64_t ComputeServerStructureNavigationStateHash();
    void RefreshServerStructureNavigationIfNeeded();
    bool_t CarveServerStructuresOnNavGrid(bool_t bRebuildDerived = true);
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
        const Vec3& vPreferredForward,
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
    void AdvanceServerMinionWaypointPastPosition(
        MinionStateComponent& state,
        const Vec3& position,
        u8_t waypointLane,
        u32_t waypointCount) const;
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
    const std::shared_ptr<const ChampionAIShadowPolicyArtifactV1> m_pShadowPolicy;

    std::unique_ptr<ICommandExecutor> m_pExecutor;
    std::unique_ptr<CSnapshotBuilder> m_pSnapBuilder;
    std::unique_ptr<CLagCompensation> m_pLagCompensation;
    std::unique_ptr<CLobbyAuthority> m_pLobbyAuthority;
    //Replay
    std::unique_ptr<CReplayRecorder> m_pReplayRecorder;
    bool_t m_bReplayFinalized = false;
    // S030: 넥서스 파괴 게임 종료 latch — 종료 이벤트 1회 브로드캐스트 + 리플레이 발행 보증.
    bool_t m_bGameEnded = false;

    std::unique_ptr<Engine::CSpatialHashSystem> m_pSpatialSystem;
    std::unique_ptr<GameplayTurret::CTurretAISystem> m_pTurretAI;
    std::unique_ptr<Engine::CMapSurfaceSampler> m_pMapSurfaceSampler;
    std::unique_ptr<Engine::CNavGrid> m_pTerrainNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pPathNavGrid;
    std::unique_ptr<Engine::CNavGrid> m_pMinionLaneNavGrid;
    u64_t m_serverStructureNavigationStateHash = 0ull;
    u64_t m_serverPathNavGridBuildCount = 0ull;
    u64_t m_serverStructureNavigationRefreshCount = 0ull;

    CCommandIngress m_commandIngress;
    std::vector<GameCommand> m_pendingExecCommands;
    std::unordered_map<u64_t, PendingCommand> m_pendingReplayCommands;
    bool_t m_bPracticeModeEnabled = false;
    std::array<f32_t, SpawnLoadoutPolicyDef::kRespawnLevelCount>
        m_PracticeRespawnSecondsByLevel{};
    PracticeMinionAttackDamagePolicy m_PracticeMinionAttackDamage{};
    std::vector<EntityID> m_PracticeSpawnedEntities;

    enum class PracticeControlChangeKind : u8_t
    {
        None = 0u,
        TakeRosterChampion,
        ReplaceControlledChampion,
    };

    struct PendingPracticeControlChange
    {
        PracticeControlChangeKind eKind = PracticeControlChangeKind::None;
        u32_t uSessionId = 0u;
        NetEntityId uSourceNetId = NULL_NET_ENTITY;
        NetEntityId uTargetNetId = NULL_NET_ENTITY;
        eChampion eChampionId = eChampion::END;
        GameCommand tCommand{};
    };

    PendingPracticeControlChange m_PendingPracticeControlChange{};

    // Simulation time control (designer/practice; sim dt stays kFixedDt)
    bool_t m_bSimPaused = false;
    u32_t m_simStepBudget = 0;
    std::atomic<f32_t> m_simSpeedMul{ 1.f };
    u64_t m_toolRevision = 0;
    u64_t m_timelineEpoch = 1;
    u64_t m_timelineBranchId = 1;
    u64_t m_lastReplaySnapshotTick = ~0ull;
    u64_t m_lastReplayToolRevision = ~0ull;

    // Chrono Break: 주기 키프레임 링(1초 간격, 90초 창) + 지연 되감기 요청(틱 경계에서 수행)
    struct RoomKeyframe
    {
        u64_t tick = 0;
        std::vector<u8_t> simBytes;
        CServerMinionWaveRuntime::WaveState waveState{};
        f32_t turretActivationAccum = 0.f;
        bool_t bPracticeModeEnabled = false;
        std::array<f32_t, SpawnLoadoutPolicyDef::kRespawnLevelCount>
            practiceRespawnSecondsByLevel{};
        std::array<f32_t, PracticeMinionAttackDamagePolicy::kRoleCount>
            practiceMinionAttackDamageByRole{};
    };
    std::vector<RoomKeyframe> m_keyframes;
    u64_t m_pendingRewindToTick = 0;

    std::vector<u32_t> m_sessionIds;
    CSessionBinding m_sessionBinding;

    struct AuthenticatedMatchParticipant
    {
        u32_t sessionId = 0u;
        u8_t team = 0xFFu;
        NetEntityId perspectiveNetId = NULL_NET_ENTITY;
    };
    std::string m_matchID;
    std::string m_gameSessionID;
	u64_t m_gameSessionGeneration = 0u;
	u64_t m_lastCompletedGameSessionGeneration = 0u;
    std::string m_pendingReadyMatchID;
    std::string m_pendingReadyGameSessionID;
    std::unordered_map<u32_t, std::string> m_userIDBySession;
    std::unordered_map<std::string, AuthenticatedMatchParticipant>
        m_authenticatedParticipants;
    u8_t m_winningTeam = 0xFFu;
    std::unordered_map<EntityID, u32_t> m_lastBroadcastActionSeq;
    std::unordered_map<u32_t, u32_t> m_lastSimCommandSeqBySession;
    std::unordered_map<u32_t, std::array<SkillCommandFeedback, 5u>>
        m_lastCommandFeedbackBySession;

    bool_t m_bGameplayObjectsSpawned = false;
    CServerMinionWaveRuntime m_serverMinionWaves{};
    std::unordered_map<EntityID, Vec3> m_serverMinionTickStartPositions{};
};
