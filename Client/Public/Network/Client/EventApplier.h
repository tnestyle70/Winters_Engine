#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "Network/Client/PresentationMutation.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace Shared::Schema
{
    struct ActionStartEvent;
    struct DamageEvent;
    struct EffectTriggerEvent;
    struct ProjectileHitEvent;
    struct ProjectileSpawnEvent;
    struct KillFeedEvent;
}

class CEventApplier final
{
public:
    static std::unique_ptr<CEventApplier> Create();

    void OnEvent(
        CWorld& world,
        EntityIdMap& entityMap,
        const u8_t* payload,
        u32_t len);

    void RebaseTimeline(CWorld& world, EntityIdMap& entityMap);

    void SetFxMeshRenderer(Engine::CFxStaticMeshRenderer* pRenderer)
    {
        m_pFxMeshRenderer = pRenderer;
    }

    bool_t RetryCurrentActionVisual(
        CWorld& world,
        EntityID entity,
        u16_t expectedActionId);

    void BeginSnapshotReconciliation(
        u64_t uServerTick,
        bool_t bFullSnapshot);
    void UpsertProjectileSnapshot(
        CWorld& world,
        EntityIdMap& entityMap,
        NetEntityId uProjectileNet,
        u16_t uProjectileKind,
        const Vec3& vPosition,
        const Vec3& vDirection,
        f32_t fSpeed,
        f32_t fMaxDistance,
        f32_t fTraveledDistance);
    void UpsertEzrealFluxSnapshot(
        CWorld& world,
        EntityIdMap& entityMap,
        NetEntityId uSourceNet,
        NetEntityId uTargetNet,
        u64_t uExpireTick);
    void UpsertYasuoWindWallSnapshot(
        CWorld& world,
        NetEntityId uSourceNet,
        u64_t uSpawnTick,
        const Vec3& vCenter,
        const Vec3& vDirection,
        f32_t fHalfLength,
        f32_t fHalfThickness,
        u64_t uExpireTick);
    void ReconcileObjectiveSnapshot(
        CWorld& world,
        NetEntityId uNetId,
        EntityID entity,
        u32_t objectiveStateFlags);
    void EndSnapshotReconciliation(CWorld& world, EntityIdMap& entityMap);

    // S030: 서버 kGlobalGameEndEffect 수신 latch — 씬이 폴링 후 1회 소비한다.
    bool_t ConsumeGameEndEvent(u8_t& outWinningTeam)
    {
        if (!m_bGameEndPending)
            return false;
        m_bGameEndPending = false;
        outWinningTeam = m_uGameEndWinningTeam;
        return true;
    }

private:
    CEventApplier() = default;

    void ApplyActionStart(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ActionStartEvent* ev);
    void ApplyProjectileSpawn(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ProjectileSpawnEvent* ev,
        u64_t serverTick,
        u32_t uEventOrdinal);
    void ApplyProjectileHit(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ProjectileHitEvent* ev,
        u64_t serverTick,
        u32_t uEventOrdinal);
    void ApplyEffectTrigger(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::EffectTriggerEvent* ev,
        u64_t uServerTick,
        u32_t uEventOrdinal);
    void ApplyDamage(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::DamageEvent* ev);
    void ApplyKillFeed(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::KillFeedEvent* ev,
        u64_t serverTick);
    void DestroyProjectileVisuals(CWorld& world, NetEntityId projectileNet);
    void DestroyEzrealFluxVisuals(CWorld& world, u64_t relationKey);
    void DestroyYasuoWindWallVisuals(CWorld& world, u64_t wallKey);
    void DestroyObjectiveVisuals(CWorld& world, u64_t objectiveKey);

    EntityID EnsureProjectilePresentation(
        CWorld& world,
        EntityIdMap& entityMap,
        NetEntityId uProjectileNet,
        u16_t uProjectileKind,
        const Vec3& vPosition,
        const Vec3& vDirection,
        f32_t fSpeed,
        f32_t fRemainingDistance);

    bool_t PlayReplicatedActionVisual(
        CWorld& world,
        EntityID entity,
        u16_t actionId,
        u8_t actionStage);

    std::unordered_map<NetEntityId, u32_t> m_lastActionSeq;
    std::unordered_map<NetEntityId, std::vector<EntityID>> m_projectileVisualEntities;
    std::unordered_map<u64_t, std::vector<EntityID>> m_ezrealFluxVisualEntities;
    std::unordered_map<u64_t, u64_t> m_ezrealFluxExpireTicks;
    std::unordered_map<u64_t, EntityID> m_yasuoWindWallAnchors;
    std::unordered_map<u64_t, std::vector<EntityID>> m_yasuoWindWallVisualEntities;
    std::unordered_map<u64_t, u64_t> m_yasuoWindWallExpireTicks;
    std::vector<EntityHandle> m_timelineVisualEntities;
    std::unordered_set<u64_t> m_seenEffectCueKeys;
    std::unordered_set<u64_t> m_seenKillFeedKeys;
    std::unordered_set<NetEntityId> m_snapshotProjectileNetIds;
    std::unordered_set<u64_t> m_snapshotEzrealFluxKeys;
    std::unordered_set<u64_t> m_snapshotYasuoWindWallKeys;
    std::unordered_set<u64_t> m_snapshotObjectiveKeys;
    std::unordered_map<u64_t, std::vector<EntityID>> m_objectiveVisualEntities;
    std::unordered_set<u64_t> m_seenProjectileHitCueKeys;
    std::unordered_map<NetEntityId, PresentationMutationStamp>
        m_projectileMutationStamps;
    std::unordered_map<u64_t, PresentationMutationStamp>
        m_ezrealFluxMutationStamps;
    u64_t m_reconcileServerTick = 0u;
    bool_t m_bReconcileFullSnapshot = false;
    bool_t m_bGameEndPending = false;
    u8_t m_uGameEndWinningTeam = 0u;
    Engine::CFxStaticMeshRenderer* m_pFxMeshRenderer = nullptr;
};
