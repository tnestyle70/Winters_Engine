#pragma once
#include "Defines.h"
#include "ECS/Entity.h"

#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>

class CWorld;
class EntityIdMap;
struct Vec3;

class CSnapshotApplier final
{
public:
    static std::unique_ptr<CSnapshotApplier> Create();

    using OnNewEntityFn = std::function<EntityID(u32_t netId, u8_t championId, u8_t team)>;
    using OnAuthoritativeSnapshotFn = std::function<void(u64_t serverTick,
        u64_t serverTimeMs, u32_t lastAckedCommandSeq, u32_t yourNetId)>;
    using OnChampionVisualChangedFn = std::function<void(EntityID entity, u8_t championId, u8_t team)>;
    using OnRemoveEntityFn = std::function<void(EntityID entity)>;

    void SetOnChampionVisualChangedCallback(OnChampionVisualChangedFn fn) { m_onChampionVisualChanged = std::move(fn); }
    void SetOnRemoveEntityCallback(OnRemoveEntityFn fn) { m_onRemoveEntity = std::move(fn); }

    void SetOnNewEntityCallback(OnNewEntityFn fn) { m_onNewEntity = std::move(fn); }
    void SetOnAuthoritativeSnapshot(OnAuthoritativeSnapshotFn fn) { m_onAuthoritativeSnapshot = std::move(fn); }
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

    u64_t GetLastAppliedServerTick() const { return m_lastServerTick; }

private:
    CSnapshotApplier() = default;

    EntityID EnsureEntity(
        CWorld& world,
        EntityIdMap& entityMap,
        u32_t netId,
        u8_t entityKind,
        u8_t championId,
        u16_t subtype,
        const Vec3& vPos,
        u8_t team);

    OnNewEntityFn m_onNewEntity;
    OnAuthoritativeSnapshotFn m_onAuthoritativeSnapshot;
    u64_t m_lastServerTick = 0;
    u32_t m_localNetId = 0;
    std::unordered_set<u32_t> m_seenNetIds;

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
