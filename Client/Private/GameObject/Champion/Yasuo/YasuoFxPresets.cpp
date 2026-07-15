#include "GameObject/Champion/Yasuo/YasuoFxPresets.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "ECS/World.h"

#include <algorithm>

namespace
{
    constexpr const char* kCueQSlash = "Yasuo.Q.Slash";
    constexpr const char* kCueQBuildUp = "Yasuo.Q.BuildUp";
    constexpr const char* kCueQTornado = "Yasuo.Q.Tornado";
    constexpr const char* kCueWWindWall = "Yasuo.W.WindWall";
    constexpr const char* kCueEDashTrail = "Yasuo.E.DashTrail";
    constexpr const char* kCueEDashRing = "Yasuo.E.DashRing";
    constexpr const char* kCuePassiveShield = "Yasuo.Passive.Shield";
    constexpr const char* kCueEQRing = "Yasuo.EQ.Ring";
    constexpr const char* kCueEQInnerWind = "Yasuo.EQ.InnerWind";
    constexpr const char* kCueRLandImpact = "Yasuo.R.LandImpact";
    constexpr const char* kCueRSwordGlow = "Yasuo.R.SwordGlow";

    Vec3 ResolveEntityWorldPos(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return {};
    }
}

EntityID YasuoFx::SpawnQStraight(CWorld& world, const Vec3& vOrigin,
    const Vec3& vForward, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    return CFxCuePlayer::Play(world, kCueQSlash, cue);
}

EntityID YasuoFx::SpawnQBuildUp(CWorld& world, EntityID owner, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = ResolveEntityWorldPos(world, owner);
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    return CFxCuePlayer::Play(world, kCueQBuildUp, cue);
}

void YasuoFx::SpawnQTornado(CWorld& world,
    const Vec3& vOrigin, const Vec3& vForward,
    f32_t fSpeed, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    cue.bOverrideVelocity = true;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    CFxCuePlayer::Play(world, kCueQTornado, cue);
}

void YasuoFx::SpawnWWindWall(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vOrigin, const Vec3& vForward,
    f32_t fLifetime, f32_t fWidth, f32_t fHeight,
    f32_t fMeshScale, EntityID attachTo,
    std::vector<EntityID>* pSpawnedEntities)
{
    (void)fHeight;
    (void)fMeshScale;
    if (!pRenderer) return;

    FxCueContext cue{};
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.attachTo = attachTo;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    cue.bOverrideScaleMultiplier = true;
    cue.vScaleMultiplier = {
        (std::max)(0.1f, fWidth / 6.2f),
        1.f,
        1.f };
    cue.pFxMeshRenderer = pRenderer;
    CFxCuePlayer::PlayAll(
        world,
        kCueWWindWall,
        cue,
        pSpawnedEntities);
}

EntityID YasuoFx::SpawnEDashTrail(CWorld& world,
    Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID owner, const Vec3& vForward, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = ResolveEntityWorldPos(world, owner);
    cue.vForward = vForward;
    cue.pFxMeshRenderer = pRenderer;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    return CFxCuePlayer::Play(world, kCueEDashTrail, cue);
}

EntityID YasuoFx::SpawnEDashTargetRing(CWorld& world,
    EntityID target, f32_t fLifetime)
{
    if (target == NULL_ENTITY ||
        !world.HasComponent<TransformComponent>(target) ||
        fLifetime <= 0.f)
    {
        return NULL_ENTITY;
    }

    FxCueContext cue{};
    cue.attachTo = target;
    cue.vWorldPos = ResolveEntityWorldPos(world, target);
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    return CFxCuePlayer::Play(world, kCueEDashRing, cue);
}

void YasuoFx::SpawnPassiveShield(CWorld& world,
    Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID owner, f32_t fLifetime)
{
    if (!pRenderer || owner == NULL_ENTITY || fLifetime <= 0.f)
        return;

    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = ResolveEntityWorldPos(world, owner);
    cue.pFxMeshRenderer = pRenderer;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    CFxCuePlayer::PlayAll(world, kCuePassiveShield, cue, nullptr);
}

void YasuoFx::SpawnEQRing(CWorld& world, EntityID owner,
    const Vec3& vCenter, f32_t fLifetime, f32_t fRadius)
{
    (void)fRadius;

    FxCueContext ground{};
    ground.vWorldPos = vCenter;
    ground.bOverrideLifetime = true;
    ground.fLifetimeOverride = fLifetime;
    CFxCuePlayer::Play(world, kCueEQRing, ground);

    FxCueContext glow{};
    glow.attachTo = owner;
    glow.vWorldPos = ResolveEntityWorldPos(world, owner);
    glow.bOverrideLifetime = true;
    glow.fLifetimeOverride = fLifetime;
    CFxCuePlayer::Play(world, kCueEQInnerWind, glow);
}

void YasuoFx::SpawnRLastBreath(CWorld& world,
    const Vec3& vLandPos, EntityID owner, f32_t fLifetime)
{
    FxCueContext sword{};
    sword.attachTo = owner;
    sword.vWorldPos = ResolveEntityWorldPos(world, owner);
    sword.bOverrideLifetime = true;
    sword.fLifetimeOverride = fLifetime;
    CFxCuePlayer::Play(world, kCueRSwordGlow, sword);

    FxCueContext land{};
    land.vWorldPos = vLandPos;
    land.bOverrideLifetime = true;
    land.fLifetimeOverride = fLifetime;
    CFxCuePlayer::Play(world, kCueRLandImpact, land);
}
