#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
    constexpr const char* kCueQCast = "Fiora.Q.Cast";
    constexpr const char* kCueWCast = "Fiora.W.Cast";
    constexpr const char* kCueWParrySuccess = "Fiora.W.ParrySuccess";
    constexpr const char* kCueWRelease = "Fiora.W.Release";
    constexpr const char* kCueECast = "Fiora.E.Buff";
    constexpr const char* kCueEHit = "Fiora.E.Hit";
    constexpr const char* kCuePassiveVital = "Fiora.Passive.Vital";
    constexpr const char* kCueRVital = "Fiora.R.Vital";
    constexpr const char* kCueVitalPop = "Fiora.Vital.Pop";
    constexpr const char* kCueRRing = "Fiora.R.Ring";
    constexpr const char* kCueRHeal = "Fiora.R.Heal";

    constexpr f32_t kVitalPopGap = 1.6f;
    constexpr f32_t kVitalHeight = 1.20f;

    using VisualHandles = std::vector<EntityHandle>;

    struct OwnerVisualEntry
    {
        EntityHandle hOwner = NULL_ENTITY_HANDLE;
        VisualHandles vecHandles;
    };

    struct VitalVisualEntry
    {
        EntityHandle hOwner = NULL_ENTITY_HANDLE;
        EntityHandle hTarget = NULL_ENTITY_HANDLE;
        Fiora::Fx::eVitalVisualKind eKind = Fiora::Fx::eVitalVisualKind::Passive;
        u8_t iDirection = 0u;
        VisualHandles vecHandles;
    };

    std::vector<OwnerVisualEntry> s_vecWParryVisuals;
    std::vector<OwnerVisualEntry> s_vecEVisuals;
    std::vector<OwnerVisualEntry> s_vecRRingVisuals;
    std::vector<VitalVisualEntry> s_vecVitalVisuals;

    Vec3 ResolvePosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();
        return {};
    }

    Vec3 ResolveForward(const Vec3& direction)
    {
        const Vec3 forward = WintersMath::NormalizeXZOrZero(direction);
        return (forward.x != 0.f || forward.z != 0.f)
            ? forward
            : Vec3{ 0.f, 0.f, 1.f };
    }

    u8_t ResolveCardinalDirection(const Vec3& direction)
    {
        const Vec3 outward = ResolveForward(direction);
        if (std::abs(outward.x) >= std::abs(outward.z))
            return outward.x >= 0.f ? 0u : 1u;
        return outward.z >= 0.f ? 2u : 3u;
    }

    Vec3 ResolveCardinalOutward(u8_t direction)
    {
        switch (direction & 3u)
        {
        case 0u: return { 1.f, 0.f, 0.f };
        case 1u: return { -1.f, 0.f, 0.f };
        case 2u: return { 0.f, 0.f, 1.f };
        default: return { 0.f, 0.f, -1.f };
        }
    }

    EntityHandle ResolveHandle(CWorld& world, EntityID entity)
    {
        return entity != NULL_ENTITY && world.IsAlive(entity)
            ? world.GetEntityHandle(entity)
            : NULL_ENTITY_HANDLE;
    }

    void DestroyHandles(CWorld& world, VisualHandles& handles)
    {
        for (const EntityHandle handle : handles)
        {
            if (world.IsAlive(handle))
                world.DestroyEntity(handle);
        }
        handles.clear();
    }

    VisualHandles CaptureHandles(CWorld& world, const std::vector<EntityID>& entities)
    {
        VisualHandles handles;
        handles.reserve(entities.size());
        for (const EntityID entity : entities)
        {
            if (entity != NULL_ENTITY && world.IsAlive(entity))
                handles.push_back(world.GetEntityHandle(entity));
        }
        return handles;
    }

    void ClearOwnerVisual(
        CWorld& world,
        std::vector<OwnerVisualEntry>& entries,
        EntityID owner)
    {
        const EntityHandle ownerHandle = ResolveHandle(world, owner);
        entries.erase(
            std::remove_if(
                entries.begin(),
                entries.end(),
                [&](OwnerVisualEntry& entry)
                {
                    if (entry.hOwner != ownerHandle)
                        return false;
                    DestroyHandles(world, entry.vecHandles);
                    return true;
                }),
            entries.end());
    }

    void StoreOwnerVisual(
        CWorld& world,
        std::vector<OwnerVisualEntry>& entries,
        EntityID owner,
        const std::vector<EntityID>& spawned)
    {
        VisualHandles handles = CaptureHandles(world, spawned);
        if (handles.empty())
            return;

        OwnerVisualEntry entry{};
        entry.hOwner = ResolveHandle(world, owner);
        entry.vecHandles = std::move(handles);
        entries.push_back(std::move(entry));
    }

    std::vector<EntityID> PlayAttached(
        CWorld& world,
        const char* cueName,
        EntityID attachTo,
        const Vec3& direction,
        f32_t lifetime)
    {
        std::vector<EntityID> spawned;
        if (attachTo == NULL_ENTITY || !world.IsAlive(attachTo))
            return spawned;

        FxCueContext cue{};
        cue.attachTo = attachTo;
        cue.vWorldPos = ResolvePosition(world, attachTo);
        cue.vForward = ResolveForward(direction);
        if (lifetime > 0.f)
        {
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = lifetime;
        }
        CFxCuePlayer::PlayAll(world, cueName, cue, &spawned);
        return spawned;
    }

    std::vector<EntityID> PlayWorld(
        CWorld& world,
        const char* cueName,
        const Vec3& position,
        const Vec3& direction,
        f32_t lifetime)
    {
        FxCueContext cue{};
        cue.vWorldPos = position;
        cue.vForward = ResolveForward(direction);
        if (lifetime > 0.f)
        {
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = lifetime;
        }

        std::vector<EntityID> spawned;
        CFxCuePlayer::PlayAll(world, cueName, cue, &spawned);
        return spawned;
    }

    void ApplyVitalOffset(CWorld& world, const std::vector<EntityID>& spawned, u8_t direction)
    {
        const Vec3 outward = ResolveCardinalOutward(direction);
        const Vec3 offset{
            outward.x * kVitalPopGap,
            kVitalHeight,
            outward.z * kVitalPopGap
        };

        for (const EntityID entity : spawned)
        {
            if (!world.HasComponent<FxBillboardComponent>(entity))
                continue;

            FxBillboardComponent& fx = world.GetComponent<FxBillboardComponent>(entity);
            fx.vAttachOffset = offset;
            fx.anchor.vAnchorOffset = offset;
        }
    }

    bool_t IsVitalKeyMatch(
        const VitalVisualEntry& entry,
        EntityHandle owner,
        EntityHandle target,
        Fiora::Fx::eVitalVisualKind kind,
        u8_t direction)
    {
        return entry.hOwner == owner &&
            entry.hTarget == target &&
            entry.eKind == kind &&
            entry.iDirection == direction;
    }

    void ClearVitalKey(
        CWorld& world,
        EntityID owner,
        EntityID target,
        Fiora::Fx::eVitalVisualKind kind,
        u8_t direction)
    {
        const EntityHandle ownerHandle = ResolveHandle(world, owner);
        const EntityHandle targetHandle = ResolveHandle(world, target);
        s_vecVitalVisuals.erase(
            std::remove_if(
                s_vecVitalVisuals.begin(),
                s_vecVitalVisuals.end(),
                [&](VitalVisualEntry& entry)
                {
                    if (!IsVitalKeyMatch(
                            entry,
                            ownerHandle,
                            targetHandle,
                            kind,
                            direction))
                    {
                        return false;
                    }
                    DestroyHandles(world, entry.vecHandles);
                    return true;
                }),
            s_vecVitalVisuals.end());
    }

    void SpawnVitalPop(
        CWorld& world,
        EntityID target,
        const Vec3& outward,
        f32_t lifetime)
    {
        const u8_t direction = ResolveCardinalDirection(outward);
        const std::vector<EntityID> spawned =
            PlayAttached(world, kCueVitalPop, target, outward, lifetime);
        ApplyVitalOffset(world, spawned, direction);
    }
}

namespace Fiora::Fx
{
    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t lifetime)
    {
        PlayAttached(world, kCueQCast, owner, dir, lifetime);
    }

    void SpawnWParryActive(CWorld& world, EntityID owner, const Vec3& dir, f32_t duration)
    {
        ClearOwnerVisual(world, s_vecWParryVisuals, owner);
        const std::vector<EntityID> spawned =
            PlayAttached(world, kCueWCast, owner, dir, duration);
        StoreOwnerVisual(world, s_vecWParryVisuals, owner, spawned);
    }

    void SpawnWParrySuccess(CWorld& world, EntityID owner, const Vec3& dir, f32_t duration)
    {
        ClearOwnerVisual(world, s_vecWParryVisuals, owner);
        const std::vector<EntityID> spawned =
            PlayAttached(world, kCueWParrySuccess, owner, dir, duration);
        StoreOwnerVisual(world, s_vecWParryVisuals, owner, spawned);
    }

    void SpawnWRelease(CWorld& world, EntityID owner, const Vec3& dir, f32_t lifetime)
    {
        ClearOwnerVisual(world, s_vecWParryVisuals, owner);
        PlayAttached(world, kCueWRelease, owner, dir, lifetime);
    }

    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t duration)
    {
        ClearOwnerVisual(world, s_vecEVisuals, owner);
        const std::vector<EntityID> spawned =
            PlayAttached(world, kCueECast, owner, {}, duration);
        StoreOwnerVisual(world, s_vecEVisuals, owner, spawned);
    }

    void StopEBladeworkBuff(CWorld& world, EntityID owner)
    {
        ClearOwnerVisual(world, s_vecEVisuals, owner);
    }

    void SpawnEHitSpark(CWorld& world, EntityID target, f32_t lifetime)
    {
        PlayAttached(world, kCueEHit, target, {}, lifetime);
    }

    void SpawnVital(
        CWorld& world,
        EntityID owner,
        EntityID target,
        const Vec3& outward,
        eVitalVisualKind kind,
        f32_t duration)
    {
        if (owner == NULL_ENTITY || target == NULL_ENTITY)
            return;

        const u8_t direction = ResolveCardinalDirection(outward);
        ClearVitalKey(world, owner, target, kind, direction);

        const f32_t resolvedDuration = duration > 0.f ? duration : 8.f;
        const char* cueName = kind == eVitalVisualKind::Passive
            ? kCuePassiveVital
            : kCueRVital;
        const std::vector<EntityID> spawned =
            PlayAttached(world, cueName, target, outward, resolvedDuration);
        VisualHandles handles = CaptureHandles(world, spawned);
        if (handles.empty())
            return;

        VitalVisualEntry entry{};
        entry.hOwner = ResolveHandle(world, owner);
        entry.hTarget = ResolveHandle(world, target);
        entry.eKind = kind;
        entry.iDirection = direction;
        entry.vecHandles = std::move(handles);
        s_vecVitalVisuals.push_back(std::move(entry));
    }

    void ConsumeVital(
        CWorld& world,
        EntityID owner,
        EntityID target,
        const Vec3& outward,
        eVitalVisualKind kind)
    {
        const u8_t direction = ResolveCardinalDirection(outward);
        ClearVitalKey(world, owner, target, kind, direction);
        SpawnVitalPop(world, target, outward, 0.45f);
    }

    void ClearVitals(CWorld& world, EntityID owner, eVitalVisualKind kind)
    {
        const EntityHandle ownerHandle = ResolveHandle(world, owner);
        s_vecVitalVisuals.erase(
            std::remove_if(
                s_vecVitalVisuals.begin(),
                s_vecVitalVisuals.end(),
                [&](VitalVisualEntry& entry)
                {
                    if (entry.hOwner != ownerHandle || entry.eKind != kind)
                        return false;
                    DestroyHandles(world, entry.vecHandles);
                    return true;
                }),
            s_vecVitalVisuals.end());
    }

    void SpawnRRing(
        CWorld& world,
        EntityID owner,
        EntityID target,
        const Vec3& center,
        f32_t duration)
    {
        ClearOwnerVisual(world, s_vecRRingVisuals, owner);
        ClearVitals(world, owner, eVitalVisualKind::GrandChallenge);

        const f32_t resolvedDuration = duration > 0.f ? duration : 8.f;
        std::vector<EntityID> spawned;
        if (target != NULL_ENTITY && world.IsAlive(target))
            spawned = PlayAttached(world, kCueRRing, target, {}, resolvedDuration);
        else
            spawned = PlayWorld(world, kCueRRing, center, {}, resolvedDuration);
        StoreOwnerVisual(world, s_vecRRingVisuals, owner, spawned);
    }

    void ClearRPresentation(CWorld& world, EntityID owner)
    {
        ClearOwnerVisual(world, s_vecRRingVisuals, owner);
        ClearVitals(world, owner, eVitalVisualKind::GrandChallenge);
    }

    void SpawnRHealZone(CWorld& world, const Vec3& center, f32_t duration)
    {
        PlayWorld(world, kCueRHeal, center, {}, duration > 0.f ? duration : 5.f);
    }
}
