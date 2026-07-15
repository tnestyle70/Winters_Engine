#include "GameObject/Champion/Jax/Jax_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "WintersMath.h"

namespace
{
    constexpr const char* kCueBAHit = "Jax.BA.Hit";
    constexpr const char* kCueQLeap = "Jax.Q.Leap";
    constexpr const char* kCueWEmpower = "Jax.W.Empower";
    constexpr const char* kCueECounterGuard = "Jax.E.CounterGuard";
    constexpr const char* kCueECounterRelease = "Jax.E.CounterRelease";
    constexpr const char* kCueRAura = "Jax.R.Aura";
    constexpr const char* kCueRThirdHit = "Jax.R.ThirdHit";

    Vec3 ResolveEntityWorldPos(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return {};
    }

    Vec3 ResolveForward(CWorld& world, EntityID owner, const Vec3& dir)
    {
        const Vec3 commandDir = WintersMath::NormalizeXZOrZero(dir);
        if (commandDir.x != 0.f || commandDir.z != 0.f)
            return commandDir;

        if (owner != NULL_ENTITY && world.HasComponent<TransformComponent>(owner))
        {
            const f32_t yaw = world.GetComponent<TransformComponent>(owner).GetRotation().y;
            return WintersMath::DirectionFromYawXZ(yaw);
        }

        return { 0.f, 0.f, 1.f };
    }

    void PlayJaxCue(CWorld& world, const char* pszCueName, EntityID attachTo,
        const Vec3& worldPos, const Vec3& forward,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr)
    {
        FxCueContext fx{};
        fx.vWorldPos = worldPos;
        fx.vForward = forward;
        fx.attachTo = attachTo;
        fx.pFxMeshRenderer = pRenderer;
        CFxCuePlayer::PlayAll(world, pszCueName, fx, nullptr);
    }
}

namespace Jax::Fx
{
    void SpawnBAHitFlash(CWorld& world, EntityID target, f32_t fLifetime)
    {
        (void)fLifetime;
        if (target == NULL_ENTITY) return;

        PlayJaxCue(world, kCueBAHit, target,
            ResolveEntityWorldPos(world, target), { 0.f, 0.f, 1.f });
    }

    void SpawnQLeapTrail(CWorld& world, EntityID owner, const Vec3& dir,
        f32_t fLifetime, Engine::CFxStaticMeshRenderer* pRenderer)
    {
        (void)fLifetime;
        if (owner == NULL_ENTITY) return;

        PlayJaxCue(world, kCueQLeap, owner,
            ResolveEntityWorldPos(world, owner), ResolveForward(world, owner, dir), pRenderer);
    }

    void SpawnWEmpowerGlow(CWorld& world, EntityID owner, f32_t fDuration)
    {
        (void)fDuration;
        if (owner == NULL_ENTITY) return;

        PlayJaxCue(world, kCueWEmpower, owner,
            ResolveEntityWorldPos(world, owner), { 0.f, 0.f, 1.f });
    }

    void SpawnECounterGuard(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer)
    {
        (void)fLifetime;
        if (owner == NULL_ENTITY) return;

        PlayJaxCue(world, kCueECounterGuard, owner,
            ResolveEntityWorldPos(world, owner), { 0.f, 0.f, 1.f }, pRenderer);
    }

    void SpawnECounterRelease(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer)
    {
        (void)fLifetime;
        if (owner == NULL_ENTITY) return;

        PlayJaxCue(world, kCueECounterRelease, owner,
            ResolveEntityWorldPos(world, owner), { 0.f, 0.f, 1.f }, pRenderer);
    }

    void SpawnRBuffAura(CWorld& world, EntityID owner, f32_t fDuration,
        Engine::CFxStaticMeshRenderer* pRenderer)
    {
        (void)fDuration;
        if (owner == NULL_ENTITY) return;

        PlayJaxCue(world, kCueRAura, owner,
            ResolveEntityWorldPos(world, owner), { 0.f, 0.f, 1.f }, pRenderer);
    }

    void SpawnRThirdAttackAOE(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer)
    {
        (void)fLifetime;
        if (owner == NULL_ENTITY) return;

        PlayJaxCue(world, kCueRThirdHit, owner,
            ResolveEntityWorldPos(world, owner), { 0.f, 0.f, 1.f }, pRenderer);
    }
}
