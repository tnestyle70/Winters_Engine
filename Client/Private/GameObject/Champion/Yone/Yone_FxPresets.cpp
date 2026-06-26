#include "GameObject/Champion/Yone/Yone_FxPresets.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxSystem.h"
#include <cmath>

namespace
{
    constexpr const char* kCueBasicAttackImpact = "Yone.BasicAttack.Impact";
    constexpr const char* kCueMortalSteel = "Yone.Q.MortalSteel";
    constexpr const char* kCueSpiritCleave = "Yone.W.SpiritCleave";
    constexpr const char* kCueSoulOut = "Yone.E.SoulOut";
    constexpr const char* kCueSoulReturn = "Yone.E.SoulReturn";
    constexpr const char* kCueFateSealed = "Yone.R.FateSealed";

    constexpr const wchar_t* kFallbackImpact =
        L"Texture/Character/Yone/particles/yone_basic_attack_impact_slash.png";
    constexpr const wchar_t* kFallbackQSword =
        L"Texture/Character/Yone/particles/yone_q_sword.png";
    constexpr const wchar_t* kFallbackWArc =
        L"Texture/Character/Yone/particles/yone_base_w_groundswipe_01.png";
    constexpr const wchar_t* kFallbackETether =
        L"Texture/Character/Yone/particles/yone_base_e_timer_sumi-e_erode.png";
    constexpr const wchar_t* kFallbackRGround =
        L"Texture/Character/Yone/particles/yone_base_r_ground.png";

    Vec3 NormalizeForward(const Vec3& vForward)
    {
        Vec3 vDir = WintersMath::NormalizeXZOrZero(vForward);
        if (WintersMath::LengthSq(vDir) <= 0.0001f)
            vDir = { 0.f, 0.f, 1.f };

        return vDir;
    }

    Vec3 ResolveEntityWorldPos(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return {};
    }

    bool_t PlayCue(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
    {
        return CFxCuePlayer::Play(world, pszCueName, ctx) != NULL_ENTITY;
    }

    void SpawnFallbackBillboard(CWorld& world, EntityID attachTo, const Vec3& vPos,
        const Vec3& vForward, const wchar_t* pszTexture,
        f32_t fWidth, f32_t fHeight, f32_t fLifetime, const Vec4& vColor,
        bool_t bBillboard)
    {
        const Vec3 vDir = NormalizeForward(vForward);

        FxBillboardComponent fx{};
        fx.attachTo = attachTo;
        fx.vWorldPos = { vPos.x, vPos.y + 0.9f, vPos.z };
        fx.texturePath = pszTexture;
        fx.fWidth = fWidth;
        fx.fHeight = fHeight;
        fx.fYaw = std::atan2f(vDir.x, vDir.z);
        fx.bBillboard = bBillboard;
        fx.renderType = bBillboard ? eFxRenderType::Billboard : eFxRenderType::GroundDecal;
        fx.fLifetime = fLifetime;
        fx.fFadeOut = fLifetime * 0.45f;
        fx.vColor = vColor;
        fx.blendMode = eBlendPreset::Additive;
        CFxSystem::Spawn(world, fx);
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

void YoneFx::SpawnBasicAttackImpact(CWorld& world, EntityID target, const Vec3& vHitPos, const Vec3& vForward)
{
    FxCueContext cue = MakeCue(target, nullptr, vHitPos, vForward);
    if (PlayCue(world, kCueBasicAttackImpact, cue))
        return;

    SpawnFallbackBillboard(world, target, vHitPos, vForward, kFallbackImpact, 1.35f, 1.35f, 0.26f,
        { 1.0f, 0.28f, 0.34f, 0.88f }, true);
}

void YoneFx::SpawnMortalSteel(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    const Vec3 vDir = NormalizeForward(vForward);
    FxCueContext cue = MakeCue(NULL_ENTITY, pRenderer, vOrigin, vDir);
    cue.vEndWorldPos = { vOrigin.x + vDir.x * 4.9f, vOrigin.y, vOrigin.z + vDir.z * 4.9f };
    cue.bOverrideEndWorldPos = true;
    if (PlayCue(world, kCueMortalSteel, cue))
        return;

    SpawnFallbackBillboard(world, caster, ResolveEntityWorldPos(world, caster), vDir, kFallbackQSword, 2.4f, 0.7f,
        0.34f, { 0.45f, 1.0f, 1.25f, 0.92f }, false);
}

void YoneFx::SpawnSpiritCleave(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    FxCueContext cue = MakeCue(caster, pRenderer, vOrigin, vForward);
    if (PlayCue(world, kCueSpiritCleave, cue))
        return;

    SpawnFallbackBillboard(world, caster, vOrigin, vForward, kFallbackWArc, 4.4f, 2.2f, 0.55f,
        { 1.0f, 0.12f, 0.26f, 0.74f }, false);
}

void YoneFx::SpawnSoulOut(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    FxCueContext cue = MakeCue(caster, pRenderer, vOrigin, vForward);
    if (PlayCue(world, kCueSoulOut, cue))
        return;

    SpawnFallbackBillboard(world, caster, vOrigin, vForward, kFallbackETether, 1.7f, 1.7f, 0.62f,
        { 0.35f, 0.9f, 1.25f, 0.82f }, true);
}

void YoneFx::SpawnSoulReturn(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, const Vec3& vOrigin, const Vec3& vForward)
{
    FxCueContext cue = MakeCue(caster, pRenderer, vOrigin, vForward);
    if (PlayCue(world, kCueSoulReturn, cue))
        return;

    SpawnFallbackBillboard(world, caster, vOrigin, vForward, kFallbackETether, 1.9f, 1.9f, 0.7f,
        { 1.0f, 0.08f, 0.18f, 0.82f }, true);
}

void YoneFx::SpawnFateSealed(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID caster, EntityID target, const Vec3& vOrigin, const Vec3& vForward)
{
    (void)caster;

    const Vec3 vDir = NormalizeForward(vForward);
    FxCueContext cue = MakeCue(NULL_ENTITY, pRenderer, vOrigin, vDir);
    cue.vEndWorldPos = { vOrigin.x + vDir.x * 10.5f, vOrigin.y, vOrigin.z + vDir.z * 10.5f };
    cue.bOverrideEndWorldPos = true;
    if (PlayCue(world, kCueFateSealed, cue))
        return;

    SpawnFallbackBillboard(world, target, ResolveEntityWorldPos(world, target), vDir, kFallbackImpact, 2.4f, 2.4f,
        0.34f, { 1.0f, 0.1f, 0.24f, 0.92f }, true);
    SpawnFallbackBillboard(world, NULL_ENTITY, vOrigin, vDir, kFallbackRGround, 8.0f, 2.5f,
        0.75f, { 1.0f, 0.04f, 0.12f, 0.58f }, false);
}
