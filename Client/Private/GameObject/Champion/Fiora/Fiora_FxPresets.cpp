#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "WintersMath.h"

namespace
{
    constexpr const char* kCueBAHit = "Fiora.BA.Hit";
    constexpr const char* kCueQCast = "Fiora.Q.Cast";
    constexpr const char* kCueWCast = "Fiora.W.Cast";
    constexpr const char* kCueECast = "Fiora.E.Buff";
    constexpr const char* kCueRMark = "Fiora.R.Mark";
    constexpr const char* kCueRHeal = "Fiora.R.Heal";

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

    void PlayAttached(CWorld& world, const char* cueName, EntityID owner,
        const Vec3& direction, f32_t lifetime)
    {
        if (owner == NULL_ENTITY)
            return;

        FxCueContext cue{};
        cue.attachTo = owner;
        cue.vWorldPos = ResolvePosition(world, owner);
        cue.vForward = ResolveForward(direction);
        if (lifetime > 0.f)
        {
            cue.bOverrideLifetime = true;
            cue.fLifetimeOverride = lifetime;
        }
        CFxCuePlayer::PlayAll(world, cueName, cue, nullptr);
    }
}

namespace Fiora::Fx
{
    void SpawnBAHitSpark(CWorld& world, EntityID target, f32_t fLifetime)
    {
        PlayAttached(world, kCueBAHit, target, {}, fLifetime);
    }

    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime)
    {
        PlayAttached(world, kCueQCast, owner, dir, fLifetime);
    }

    void SpawnWParryActive(CWorld& world, EntityID owner, f32_t fDuration)
    {
        PlayAttached(world, kCueWCast, owner, {}, fDuration);
    }

    void SpawnWBlockFlash(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        PlayAttached(world, kCueWCast, owner, {}, fLifetime);
    }

    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t fDuration)
    {
        PlayAttached(world, kCueECast, owner, {}, fDuration);
    }

    void SpawnRMark(CWorld& world, EntityID target, f32_t fDuration)
    {
        PlayAttached(world, kCueRMark, target, {}, fDuration);
    }

    void SpawnRHealZone(CWorld& world, EntityID owner, f32_t fDuration)
    {
        PlayAttached(world, kCueRHeal, owner, {}, fDuration);
    }
}
