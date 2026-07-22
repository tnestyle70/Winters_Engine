#pragma once
#include "Defines.h"
#include "ECS/Entity.h"

#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>

class CWorld;
class EntityIdMap;
class CEventApplier;
struct Vec3;

struct SnapshotTimelineState
{
    u64_t timelineEpoch = 0;
    u64_t branchId = 0;
    u64_t toolRevision = 0;
    bool_t simPaused = false;
    f32_t simSpeedMul = 1.f;
};

class CSnapshotApplier final
{
public:
    static std::unique_ptr<CSnapshotApplier> Create();

    using OnNewEntityFn = std::function<EntityID(u32_t netId, u8_t championId, u8_t team)>;
    using OnAuthoritativeSnapshotFn = std::function<void(u64_t serverTick,
        u64_t serverTimeMs, u32_t lastAckedCommandSeq, u32_t yourNetId)>;
    using OnCommandResultFn = std::function<void(
        u64_t serverTick,
        u32_t commandSequence,
        u8_t state,
        u16_t reason,
        u8_t authoritativeSkillSlot,
        u8_t authoritativeSkillStage,
        u64_t stageWindowEndTick)>;
    using OnTimelineRebaseFn = std::function<void(
        const SnapshotTimelineState& previous,
        const SnapshotTimelineState& next,
        u64_t serverTick)>;
    using OnChampionVisualChangedFn = std::function<void(EntityID entity, u8_t championId, u8_t team)>;
    using OnRemoveEntityFn = std::function<void(EntityID entity)>;

    void SetOnChampionVisualChangedCallback(OnChampionVisualChangedFn fn) { m_onChampionVisualChanged = std::move(fn); }
    void SetOnRemoveEntityCallback(OnRemoveEntityFn fn) { m_onRemoveEntity = std::move(fn); }

    void SetOnNewEntityCallback(OnNewEntityFn fn) { m_onNewEntity = std::move(fn); }
    void SetOnAuthoritativeSnapshot(OnAuthoritativeSnapshotFn fn) { m_onAuthoritativeSnapshot = std::move(fn); }
    void SetOnCommandResult(OnCommandResultFn fn) { m_onCommandResult = std::move(fn); }
    void SetOnTimelineRebase(OnTimelineRebaseFn fn) { m_onTimelineRebase = std::move(fn); }
    void SetEventApplier(CEventApplier* pEventApplier)
    {
        m_pEventApplier = pEventApplier;
    }
    void ProtectLocalMoveYaw(u32_t netId, u32_t commandSeq, f32_t yaw);
    bool_t GetLocalMoveYawProtectionDebug(
        u32_t& outNetId,
        u32_t& outCommandSeq,
        f32_t& outYaw) const;

    void OnHello(
        CWorld& world,
        EntityIdMap& entityMap,
        const u8_t* payload,
        u32_t len,
        u32_t* outMyNetId,
        u32_t* outMySessionId);

    void OnSnapshot(
        CWorld& world,
        EntityIdMap& entityMap,
        const u8_t* payload,
        u32_t len);

    void ResetForReplaySeek(
        CWorld& world,
        EntityIdMap& entityMap,
        u64_t targetTick);

    u64_t GetLastAppliedServerTick() const { return m_lastServerTick; }
    u64_t GetLastAppliedSnapshotTick() const { return m_lastSnapshotTick; }
    u32_t GetLastHelloNetId() const { return m_lastHelloNetId; }
    u32_t GetLastSnapshotNetId() const { return m_lastSnapshotNetId; }
    u32_t GetLocalNetId() const { return m_localNetId; }
    u32_t GetLastAckedCommandSequence() const
    {
        return m_lastAckedCommandSequence;
    }
    const SnapshotTimelineState& GetTimelineState() const { return m_timelineState; }

    static bool_t ShouldRebaseTimeline(
        bool_t hasPrevious,
        const SnapshotTimelineState& previous,
        const SnapshotTimelineState& next)
    {
        return hasPrevious &&
            (previous.timelineEpoch != next.timelineEpoch ||
                previous.branchId != next.branchId);
    }

private:
    CSnapshotApplier() = default;

    void ClearRemoteEntitiesForTimelineRebase(
        CWorld& world,
        EntityIdMap& entityMap,
        u32_t localNetId);

    EntityID EnsureEntity(
        CWorld& world,
        EntityIdMap& entityMap,
        u32_t netId,
        u8_t entityKind,
        u8_t legacyChampionValue,
        u16_t subtype,
        const Vec3& vPos,
        u8_t team);

    OnNewEntityFn m_onNewEntity;
    OnAuthoritativeSnapshotFn m_onAuthoritativeSnapshot;
    OnCommandResultFn m_onCommandResult;
    OnTimelineRebaseFn m_onTimelineRebase;
    u64_t m_lastServerTick = 0;
    u64_t m_lastSnapshotTick = 0;
    u32_t m_localNetId = 0;
    u32_t m_lastHelloNetId = 0;
    u32_t m_lastSnapshotNetId = 0;
    u32_t m_lastAckedCommandSequence = 0u;
    // M6: Hello 에 실려 온 서버 활성 정의 팩 해시/런타임 리로드 revision (표시/기록용).
    u32_t m_serverGameplayPackHash = 0u;
    u32_t m_serverGameplayPackRevision = 0u;
    std::unordered_set<u32_t> m_seenNetIds;
    std::unordered_set<u32_t> m_ezrealPassiveNetIds;
    CEventApplier* m_pEventApplier = nullptr;
    SnapshotTimelineState m_timelineState{};
    bool_t m_bHasTimelineState = false;

    //Viego soul change
    OnChampionVisualChangedFn m_onChampionVisualChanged;
    OnRemoveEntityFn m_onRemoveEntity;

    struct LocalMoveYawProtection
    {
        bool_t bActive = false;
        u32_t netId = 0;
        u32_t commandSeq = 0;
        u16_t protectedSnapshotCount = 0;
        u16_t ackedProtectedSnapshotCount = 0;
        f32_t yaw = 0.f;
    };
    LocalMoveYawProtection m_localMoveYawProtection{};
};
