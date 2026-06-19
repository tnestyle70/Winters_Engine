#pragma once

#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

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

    void SetFxMeshRenderer(Engine::CFxStaticMeshRenderer* pRenderer)
    {
        m_pFxMeshRenderer = pRenderer;
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
        u64_t serverTick);
    void ApplyProjectileHit(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::ProjectileHitEvent* ev);
    void ApplyEffectTrigger(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::EffectTriggerEvent* ev);
    void ApplyDamage(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::DamageEvent* ev);
    void ApplyKillFeed(
        CWorld& world,
        EntityIdMap& entityMap,
        const Shared::Schema::KillFeedEvent* ev,
        u64_t serverTick);

    void PlayReplicatedActionVisual(
        CWorld& world,
        EntityID entity,
        u16_t actionId,
        u8_t actionStage);
    void SpawnBillboard(CWorld& world, const Vec3& pos, const Vec3& velocity,
        const wchar_t* texturePath, f32_t width, f32_t height, f32_t lifetime,
        EntityID attachTo = NULL_ENTITY);

    std::unordered_map<NetEntityId, u32_t> m_lastActionSeq;
    std::unordered_set<u64_t> m_seenEffectCueKeys;
    std::unordered_set<u64_t> m_seenProjectileCueKeys;
    std::unordered_set<u64_t> m_seenKillFeedKeys;
    Engine::CFxStaticMeshRenderer* m_pFxMeshRenderer = nullptr;
};
