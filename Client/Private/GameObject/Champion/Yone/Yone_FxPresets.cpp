#include "GameObject/Champion/Yone/Yone_FxPresets.h"
#include "ECS/World.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "WintersMath.h"

namespace
{
    constexpr const char* kCueBasicAttackImpact = "Yone.BasicAttack.Impact";
    constexpr const char* kCueMortalSteel = "Yone.Q.MortalSteel";
    constexpr const char* kCueSpiritCleave = "Yone.W.SpiritCleave";
    constexpr const char* kCueSoulOut = "Yone.E.SoulOut";
    constexpr const char* kCueSoulReturn = "Yone.E.SoulReturn";
    constexpr const char* kCueFateSealed = "Yone.R.FateSealed";

    Vec3 NormalizeForward(const Vec3& vForward)
    {
        Vec3 vDir = WintersMath::NormalizeXZOrZero(vForward);
        if (WintersMath::LengthSq(vDir) <= 0.0001f)
            vDir = { 0.f, 0.f, 1.f };

        return vDir;
    }

    void PlayCue(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
    {
        CFxCuePlayer::Play(world, pszCueName, ctx);
    }

    FxCueContext MakeCue(EntityID attachTo, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& vOrigin, const Vec3& vForward)
    {
        FxCueContext cue{};
        cue.attachTo = attachTo;
        cue.vWorldPos = vOrigin;
        cue.vForward = NormalizeForward(vForward);
        cue.pFxMeshRenderer = pRenderer;
        return cue;
    }
}

void YoneFx::SpawnBasicAttackImpact(CWorld& world, EntityID target,
    const Vec3& vHitPos, const Vec3& vForward)
{
    PlayCue(world, kCueBasicAttackImpact,
        MakeCue(target, nullptr, vHitPos, vForward));
}

void YoneFx::SpawnMortalSteel(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    (void)caster;

    const Vec3 vDir = NormalizeForward(vForward);
    FxCueContext cue = MakeCue(NULL_ENTITY, pRenderer, vOrigin, vDir);
    cue.vEndWorldPos = { vOrigin.x + vDir.x * 4.9f, vOrigin.y, vOrigin.z + vDir.z * 4.9f };
    cue.bOverrideEndWorldPos = true;
    PlayCue(world, kCueMortalSteel, cue);
}

void YoneFx::SpawnSpiritCleave(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    PlayCue(world, kCueSpiritCleave,
        MakeCue(caster, pRenderer, vOrigin, vForward));
}

void YoneFx::SpawnSoulOut(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    PlayCue(world, kCueSoulOut,
        MakeCue(caster, pRenderer, vOrigin, vForward));
}

void YoneFx::SpawnSoulReturn(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    PlayCue(world, kCueSoulReturn,
        MakeCue(caster, pRenderer, vOrigin, vForward));
}

void YoneFx::SpawnFateSealed(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, EntityID target, const Vec3& vOrigin, const Vec3& vForward)
{
    (void)caster;
    (void)target;

    const Vec3 vDir = NormalizeForward(vForward);
    FxCueContext cue = MakeCue(NULL_ENTITY, pRenderer, vOrigin, vDir);
    cue.vEndWorldPos = { vOrigin.x + vDir.x * 10.5f, vOrigin.y, vOrigin.z + vDir.z * 10.5f };
    cue.bOverrideEndWorldPos = true;
    PlayCue(world, kCueFateSealed, cue);
}
