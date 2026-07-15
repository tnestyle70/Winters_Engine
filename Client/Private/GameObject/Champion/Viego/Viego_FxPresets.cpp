#include "GameObject/Champion/Viego/Viego_FxPresets.h"

#include "GameObject/FX/FxCuePlayer.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"

#include <unordered_map>
#include <vector>

namespace
{
    constexpr const char* kCueBAHit = "Viego.BA.Hit";
    constexpr const char* kCueQSlash = "Viego.Q.Slash";
    constexpr const char* kCueWChargeGlow = "Viego.W.ChargeGlow";
    constexpr const char* kCueWCast = "Viego.W.Cast";
    constexpr const char* kCueWMissile = "Viego.W.Missile";
    constexpr const char* kCueEMist = "Viego.E.Mist";
    constexpr const char* kCueEBodyGlow = "Viego.E.BodyGlow";
    constexpr const char* kCueRImpact = "Viego.R.Impact";
    constexpr const char* kCueSoulIdle = "Viego.Soul.Idle";

    std::unordered_map<EntityID, std::vector<EntityID>> g_WChargeGlowEntities;
    std::unordered_map<EntityID, std::vector<EntityID>> g_SoulIdleEntities;

    Vec3 ResolvePosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return Vec3{};
    }

    Vec3 NormalizeOrForward(const Vec3& dir)
    {
        const Vec3 n = WintersMath::NormalizeXZ(dir);
        if (n.x != 0.f || n.z != 0.f)
            return n;

        return Vec3{ 0.f, 0.f, 1.f };
    }

    Vec3 ResolveDirectionBetween(const Vec3& from, const Vec3& to, const Vec3& fallback)
    {
        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{ to.x - from.x, 0.f, to.z - from.z });
        if (dir.x != 0.f || dir.z != 0.f)
            return dir;

        return NormalizeOrForward(fallback);
    }

    Vec3 OffsetForward(const Vec3& origin, const Vec3& forward, f32_t fDistance, f32_t fUp = 0.f)
    {
        const Vec3 n = NormalizeOrForward(forward);
        return {
            origin.x + n.x * fDistance,
            origin.y + fUp,
            origin.z + n.z * fDistance
        };
    }

    FxCueContext MakeWorldCue(const Vec3& origin, const Vec3& forward,
        Engine::CFxStaticMeshRenderer* pRenderer, f32_t fLifetime)
    {
        FxCueContext cue{};
        cue.vWorldPos = origin;
        cue.vForward = NormalizeOrForward(forward);
        cue.pFxMeshRenderer = pRenderer;
        cue.bOverrideLifetime = true;
        cue.fLifetimeOverride = fLifetime;
        return cue;
    }

    FxCueContext MakeAttachedCue(EntityID owner, const Vec3& worldPos,
        Engine::CFxStaticMeshRenderer* pRenderer, f32_t fLifetime)
    {
        FxCueContext cue{};
        cue.attachTo = owner;
        cue.vWorldPos = worldPos;
        cue.pFxMeshRenderer = pRenderer;
        cue.bOverrideLifetime = true;
        cue.fLifetimeOverride = fLifetime;
        return cue;
    }

    void DestroyLiveEntities(CWorld& world, const std::vector<EntityID>& entities)
    {
        for (const EntityID entity : entities)
        {
            if (entity != NULL_ENTITY && world.IsAlive(entity))
                world.DestroyEntity(entity);
        }
    }
}

namespace Viego::Fx
{
    void SpawnBAHit(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, EntityID target, f32_t fLifetime)
    {
        const Vec3 ownerPos = ResolvePosition(world, owner);
        const Vec3 targetPos = target != NULL_ENTITY ? ResolvePosition(world, target) : ownerPos;
        const Vec3 dir = ResolveDirectionBetween(ownerPos, targetPos, Vec3{ 0.f, 0.f, 1.f });

        FxCueContext cue = MakeWorldCue(targetPos, dir, pRenderer, fLifetime);
        CFxCuePlayer::Play(world, kCueBAHit, cue);
    }

    void SpawnQSlash(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        const Vec3 start = owner != NULL_ENTITY ? ResolvePosition(world, owner) : origin;
        const Vec3 forward = NormalizeOrForward(dir);

        FxCueContext cue = MakeWorldCue(start, forward, pRenderer, fLifetime);
        cue.bOverrideEndWorldPos = true;
        cue.vEndWorldPos = OffsetForward(start, forward, 5.8f);
        CFxCuePlayer::Play(world, kCueQSlash, cue);
    }

    void SpawnWMissile(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        StopWChargeGlow(world, owner);

        const Vec3 start = owner != NULL_ENTITY ? ResolvePosition(world, owner) : origin;
        const Vec3 forward = NormalizeOrForward(dir);

        FxCueContext castCue = MakeWorldCue(start, forward, pRenderer, fLifetime);
        castCue.bOverrideEndWorldPos = true;
        castCue.vEndWorldPos = OffsetForward(start, forward, 2.3f);
        CFxCuePlayer::Play(world, kCueWCast, castCue);

        FxCueContext missileCue = MakeWorldCue(OffsetForward(start, forward, 0.7f, 0.85f),
            forward, pRenderer, fLifetime);
        missileCue.vVelocity = { forward.x * 3.6f, 0.f, forward.z * 3.6f };
        missileCue.bOverrideVelocity = true;
        missileCue.bOverrideEndWorldPos = true;
        missileCue.vEndWorldPos = OffsetForward(start, forward, 2.4f, 0.85f);
        CFxCuePlayer::Play(world, kCueWMissile, missileCue);
    }

    void StopWChargeGlow(CWorld& world, EntityID owner)
    {
        if (owner == NULL_ENTITY)
            return;

        const auto it = g_WChargeGlowEntities.find(owner);
        if (it == g_WChargeGlowEntities.end())
            return;

        DestroyLiveEntities(world, it->second);
        g_WChargeGlowEntities.erase(it);
    }

    void SpawnWChargeGlow(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir)
    {
        StopWChargeGlow(world, owner);

        const Vec3 start = owner != NULL_ENTITY ? ResolvePosition(world, owner) : origin;
        const Vec3 forward = NormalizeOrForward(dir);

        FxCueContext cue{};
        cue.vWorldPos = start;
        cue.vForward = forward;
        cue.pFxMeshRenderer = pRenderer;

        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(world, kCueWChargeGlow, cue, &spawned);

        if (owner != NULL_ENTITY && !spawned.empty())
            g_WChargeGlowEntities[owner] = spawned;
    }

    void SpawnEMist(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fDuration)
    {
        const Vec3 start = owner != NULL_ENTITY ? ResolvePosition(world, owner) : origin;
        const Vec3 forward = NormalizeOrForward(dir);

        FxCueContext mistCue = MakeWorldCue(start, forward, pRenderer, fDuration);
        mistCue.bOverrideEndWorldPos = true;
        mistCue.vEndWorldPos = OffsetForward(start, forward, 6.0f);
        CFxCuePlayer::Play(world, kCueEMist, mistCue);

        FxCueContext glowCue = MakeAttachedCue(owner, start, pRenderer, fDuration);
        glowCue.vForward = forward;
        CFxCuePlayer::Play(world, kCueEBodyGlow, glowCue);
    }

    void SpawnRImpact(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        const Vec3 start = owner != NULL_ENTITY ? ResolvePosition(world, owner) : origin;
        const Vec3 forward = NormalizeOrForward(dir);
        const Vec3 impact = OffsetForward(start, forward, 5.0f);

        FxCueContext cue = MakeWorldCue(impact, forward, pRenderer, fLifetime);
        cue.bOverrideEndWorldPos = true;
        cue.vEndWorldPos = OffsetForward(impact, forward, 2.2f);
        CFxCuePlayer::Play(world, kCueRImpact, cue);
    }

    void SpawnSoulIdle(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        StopSoulIdle(world, owner);

        const Vec3 ownerPos = ResolvePosition(world, owner);
        FxCueContext cue = MakeAttachedCue(owner, ownerPos, nullptr, fLifetime);
        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(world, kCueSoulIdle, cue, &spawned);
        if (owner != NULL_ENTITY && !spawned.empty())
            g_SoulIdleEntities[owner] = std::move(spawned);
    }

    void StopSoulIdle(CWorld& world, EntityID owner)
    {
        if (owner == NULL_ENTITY)
            return;

        const auto it = g_SoulIdleEntities.find(owner);
        if (it == g_SoulIdleEntities.end())
            return;

        DestroyLiveEntities(world, it->second);
        g_SoulIdleEntities.erase(it);
    }

    void StopAllSoulIdle(CWorld& world)
    {
        for (const auto& entry : g_SoulIdleEntities)
            DestroyLiveEntities(world, entry.second);
        g_SoulIdleEntities.clear();
    }
}
