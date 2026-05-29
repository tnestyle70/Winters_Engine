#include "GameObject/Champion/Jax/Jax_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxSystem.h"
#include "WintersMath.h"

namespace
{
    constexpr const char* kCueBAHit = "Jax.BA.Hit";
    constexpr const char* kCueQLeap = "Jax.Q.Leap";
    constexpr const char* kCueWEmpower = "Jax.W.Empower";
    constexpr const char* kCueECounter = "Jax.E.CounterStrike";
    constexpr const char* kCueRAura = "Jax.R.Aura";
    constexpr const char* kCueRThirdHit = "Jax.R.ThirdHit";

    constexpr const wchar_t* kPathBaHitFlashTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_hit_flash.png";
    constexpr const wchar_t* kPathBaBoltsTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_bolts_tar.png";
    constexpr const wchar_t* kPathQTrailTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_q_smoketrail.png";
    constexpr const wchar_t* kPathQBallTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_q_ball.png";
    constexpr const wchar_t* kPathWGlowTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_w_mis_01_ribbon.png";
    constexpr const wchar_t* kPathWSparkTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_ba_sparks.png";
    constexpr const wchar_t* kPathEGroundTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_e_ground.png";
    constexpr const wchar_t* kPathEFlameTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_e_dome_flames.png";
    constexpr const wchar_t* kPathESparkTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_e_spark.png";
    constexpr const wchar_t* kPathRAuraTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_r_buf_glow.png";
    constexpr const wchar_t* kPathRBuffOuterTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_r_buf_outer.png";
    constexpr const wchar_t* kPathRAOETex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_r_wave02_01.png";
    constexpr const wchar_t* kPathRFlashTex =
        L"Client/Bin/Resource/Texture/Character/Jax/particles/jax_base_r_bigglow02.png";

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

    bool_t PlayJaxCue(CWorld& world, const char* pszCueName, EntityID attachTo,
        const Vec3& worldPos, const Vec3& forward,
        Engine::CFxStaticMeshRenderer* pRenderer = nullptr)
    {
        FxCueContext fx{};
        fx.vWorldPos = worldPos;
        fx.vForward = forward;
        fx.attachTo = attachTo;
        fx.pFxMeshRenderer = pRenderer;
        return CFxCuePlayer::PlayAll(world, pszCueName, fx, nullptr) != NULL_ENTITY;
    }

    EntityID SpawnBillboard(CWorld& world, EntityID attachTo, const Vec3& worldPos,
        const Vec3& attachOffset, const wchar_t* texturePath,
        f32_t fWidth, f32_t fHeight, f32_t fLifetime,
        const Vec4& color, bool_t bBillboard, eBlendPreset blend,
        f32_t fFadeIn = 0.f, f32_t fFadeOut = 0.f, f32_t fStartDelay = 0.f,
        f32_t fYaw = 0.f)
    {
        FxBillboardComponent fx{};
        fx.vWorldPos = worldPos;
        fx.attachTo = attachTo;
        fx.vAttachOffset = attachOffset;
        fx.texturePath = texturePath;
        fx.fWidth = fWidth;
        fx.fHeight = fHeight;
        fx.fYaw = fYaw;
        fx.fLifetime = fLifetime;
        fx.fFadeIn = fFadeIn;
        fx.fFadeOut = fFadeOut;
        fx.fStartDelay = fStartDelay;
        fx.bBillboard = bBillboard;
        fx.vColor = color;
        fx.blendMode = blend;
        fx.depthMode = eFxDepthMode::DepthTestWriteOff;
        fx.fAlphaClip = 0.02f;
        return CFxSystem::Spawn(world, fx);
    }
}

namespace Jax::Fx
{
    void SpawnBAHitFlash(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;

        const Vec3 pos = ResolveEntityWorldPos(world, target);
        if (PlayJaxCue(world, kCueBAHit, target, pos, { 0.f, 0.f, 1.f }))
            return;

        SpawnBillboard(world, target, pos, { 0.f, 1.05f, 0.f },
            kPathBaHitFlashTex, 1.55f, 1.55f, fLifetime,
            { 1.55f, 0.85f, 0.28f, 1.f }, true, eBlendPreset::Additive,
            0.01f, fLifetime * 0.55f);

        SpawnBillboard(world, target, pos, { 0.f, 1.18f, 0.f },
            kPathBaBoltsTex, 1.35f, 1.35f, fLifetime * 0.85f,
            { 1.8f, 0.58f, 0.18f, 0.95f }, true, eBlendPreset::Additive,
            0.01f, fLifetime * 0.45f, 0.03f);
    }

    void SpawnQLeapTrail(CWorld& world, EntityID owner, const Vec3& dir,
        f32_t fLifetime, Engine::CFxStaticMeshRenderer* pRenderer)
    {
        if (owner == NULL_ENTITY) return;

        const Vec3 pos = ResolveEntityWorldPos(world, owner);
        const Vec3 forward = ResolveForward(world, owner, dir);
        if (PlayJaxCue(world, kCueQLeap, owner, pos, forward, pRenderer))
            return;

        SpawnBillboard(world, owner, pos, { 0.f, 1.0f, 0.f },
            kPathQTrailTex, 2.2f, 1.35f, fLifetime,
            { 1.15f, 0.62f, 0.22f, 0.72f }, true, eBlendPreset::Additive,
            0.02f, fLifetime * 0.65f);

        SpawnBillboard(world, owner, pos, { 0.f, 1.15f, 0.f },
            kPathQBallTex, 1.3f, 1.3f, fLifetime * 0.75f,
            { 1.55f, 0.95f, 0.32f, 0.82f }, true, eBlendPreset::Additive,
            0.01f, fLifetime * 0.45f);
    }

    void SpawnWEmpowerGlow(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;

        const Vec3 pos = ResolveEntityWorldPos(world, owner);
        if (PlayJaxCue(world, kCueWEmpower, owner, pos, { 0.f, 0.f, 1.f }))
            return;

        SpawnBillboard(world, owner, pos, { 0.f, 1.25f, 0.f },
            kPathWGlowTex, 1.85f, 1.85f, fDuration,
            { 1.65f, 0.62f, 0.20f, 0.90f }, true, eBlendPreset::Additive,
            0.04f, fDuration * 0.25f);

        SpawnBillboard(world, owner, pos, { 0.f, 1.35f, 0.f },
            kPathWSparkTex, 1.35f, 1.35f, fDuration * 0.45f,
            { 1.85f, 0.80f, 0.24f, 1.0f }, true, eBlendPreset::Additive,
            0.02f, fDuration * 0.30f);
    }

    void SpawnECounterSlash(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer)
    {
        if (owner == NULL_ENTITY) return;

        const Vec3 pos = ResolveEntityWorldPos(world, owner);
        if (PlayJaxCue(world, kCueECounter, owner, pos, { 0.f, 0.f, 1.f }, pRenderer))
            return;

        SpawnBillboard(world, owner, pos, { 0.f, 0.05f, 0.f },
            kPathEGroundTex, 4.6f, 4.6f, fLifetime,
            { 0.45f, 0.78f, 1.35f, 0.55f }, false, eBlendPreset::Additive,
            0.06f, fLifetime * 0.30f);

        SpawnBillboard(world, owner, pos, { 0.f, 1.0f, 0.f },
            kPathEFlameTex, 3.8f, 3.8f, fLifetime,
            { 1.45f, 0.58f, 0.18f, 0.82f }, true, eBlendPreset::Additive,
            0.03f, fLifetime * 0.35f);

        SpawnBillboard(world, owner, pos, { 0.f, 1.2f, 0.f },
            kPathESparkTex, 2.2f, 2.2f, 0.34f,
            { 1.85f, 0.88f, 0.30f, 1.0f }, true, eBlendPreset::Additive,
            0.01f, 0.24f, fLifetime * 0.78f);
    }

    void SpawnRBuffAura(CWorld& world, EntityID owner, f32_t fDuration,
        Engine::CFxStaticMeshRenderer* pRenderer)
    {
        if (owner == NULL_ENTITY) return;

        const Vec3 pos = ResolveEntityWorldPos(world, owner);
        if (PlayJaxCue(world, kCueRAura, owner, pos, { 0.f, 0.f, 1.f }, pRenderer))
            return;

        SpawnBillboard(world, owner, pos, { 0.f, 1.25f, 0.f },
            kPathRAuraTex, 2.6f, 2.6f, fDuration,
            { 1.45f, 1.02f, 0.42f, 0.78f }, true, eBlendPreset::Additive,
            0.06f, fDuration * 0.20f);

        SpawnBillboard(world, owner, pos, { 0.f, 1.25f, 0.f },
            kPathRBuffOuterTex, 3.4f, 3.4f, fDuration,
            { 1.25f, 0.58f, 0.95f, 0.52f }, true, eBlendPreset::Additive,
            0.10f, fDuration * 0.20f, 0.06f);
    }

    void SpawnRThirdAttackAOE(CWorld& world, EntityID owner, f32_t fLifetime,
        Engine::CFxStaticMeshRenderer* pRenderer)
    {
        if (owner == NULL_ENTITY) return;

        const Vec3 pos = ResolveEntityWorldPos(world, owner);
        if (PlayJaxCue(world, kCueRThirdHit, owner, pos, { 0.f, 0.f, 1.f }, pRenderer))
            return;

        SpawnBillboard(world, owner, pos, { 0.f, 0.05f, 0.f },
            kPathRAOETex, 4.2f, 4.2f, fLifetime,
            { 1.55f, 0.82f, 0.25f, 0.82f }, false, eBlendPreset::Additive,
            0.01f, fLifetime * 0.55f);

        SpawnBillboard(world, owner, pos, { 0.f, 1.3f, 0.f },
            kPathRFlashTex, 2.4f, 2.4f, fLifetime * 0.65f,
            { 1.8f, 1.2f, 0.48f, 0.95f }, true, eBlendPreset::Additive,
            0.01f, fLifetime * 0.45f);
    }
}
