#include "GameObject/Champion/Irelia/IreliaFxPresets.h"
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/LegacyFxAdapter.h"
#include "GameObject/Champion/Irelia/IreliaBladeSystem.h"
#include "GameObject/Champion/Irelia/Irelia_Tuning.h"
#include "ECS/World.h"
#include <cmath>

namespace
{
    // ?? ?띿뒪泥?寃쎈줈 const (LoL 蹂몄껜 癒명떚由ъ뼹 PNG) ??
    constexpr const wchar_t* kPathQTrail =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_q_dark_trail.png";
    constexpr const wchar_t* kPathQMark =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_q_mark_pulse_erode.png";
    constexpr const wchar_t* kPathQLeadingEdge =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_q_leadingedge.png";
    constexpr const wchar_t* kPathWSpin =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_bladeimages_spin_02.png";
    constexpr const wchar_t* kPathWBlade =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_blade_erode.png";
    constexpr const wchar_t* kPathWShieldSoft =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_shield_soft_tex.png";
    constexpr const wchar_t* kPathWBlockMult =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_w_block_mult.png";
    constexpr const wchar_t* kPathWAmbientGlow =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_w_ambientglow.png";
    constexpr const wchar_t* kPathShards =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_shards.png";
    //  W2 release 2-layer ?⑹꽦 (blade_erode ??kPathWBlade 濡?SpawnWStage2Slash 媛 ?ъ슜)
    constexpr const wchar_t* kPathWSwipeBlades =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_swipe_blades.png";
    constexpr const wchar_t* kPathWMisGlow =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_w_mis_glow.png";
    constexpr const wchar_t* kPathRPulse = L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_r_pulse_mesh_tex.png";
    constexpr const wchar_t* kPathRTrail =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_temp_r_mis_trail_mult.png";
    constexpr const wchar_t* kPathRLead =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_r_mis_glow_lines.png";
    constexpr const wchar_t* kPathRDisarmRing =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_disarm_ring.png";
    constexpr const wchar_t* kPathRWallDecal =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_r_hit_wall_dissarm_muzzle.png";

    // ?? E mesh ??
    constexpr const char* kPathEBeamFbx = "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_beam.fbx";
    constexpr const wchar_t* kPathEBeamTex = L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_beam_mult.png";

    constexpr const wchar_t* kPathEGroundTyphoon =
        L"Client/Bin/Resource/Texture/FX/Irelia/render/irelia_base_temp_e_tar_blade_indicator_typhoon.png";
    constexpr const wchar_t* kPathEWarningSpark =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_warnig_spark.png";
    constexpr const wchar_t* kPathEStunBeam =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam.png";
    constexpr const wchar_t* kPathEStunBeamDark =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_stun_beam_dark.png";
    constexpr const wchar_t* kPathEStunTrail =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_e_blade_stun_trail.png";
    constexpr const wchar_t* kPathLensflareStreak =
        L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_lensflare_streak.png";

    // ?? R 遺梨꾧섦 移쇰궇 (E sword ? ?숈씪 mesh + 蹂몄껜 癒명떚由ъ뼹 ?띿뒪泥? ??
    constexpr const char*    kPathRBladeFbx = "Client/Bin/Resource/Texture/FX/Irelia/fbx/irelia_base_e_blade.fbx";
    constexpr const wchar_t* kPathRBladeTex = L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_blades_passive_4_texture.png";

    EntityID SpawnBillboardAsset(CWorld& world, const FxBillboardComponent& fx, const char* pszAssetName)
    {
        CFxAssetRegistry& registry = CFxSystem::GetAssetRegistry();
        const FxAssetHandle handle =
            LegacyFx::FxAssetFromBillboard(registry, fx, pszAssetName);
        return CFxSystem::SpawnFromAsset(world, handle, fx.vWorldPos, fx.attachTo);
    }

    EntityID SpawnRuntimeBillboard(CWorld& world, const FxBillboardComponent& fx)
    {
        return CFxSystem::Spawn(world, fx);
    }
}

EntityID IreliaFx::SpawnQTrail(CWorld& world, EntityID owner, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();

    FxBillboardComponent qfx{};
    qfx.attachTo = owner;
    // Q dash trail is a short atlas sprite attached near the body.
    qfx.vAttachOffset = { 0.f, t.qTrailYOffset, 0.f };
    qfx.texturePath = kPathQTrail;
    qfx.fWidth = t.qTrailWidth;
    qfx.fHeight = t.qTrailHeight;
    qfx.fLifetime = (fLifetime > t.qTrailLifetimeMax) ? t.qTrailLifetimeMax : fLifetime;
    qfx.bBillboard = true;
    qfx.vColor = t.qTrailColor;
    qfx.blendMode = eBlendPreset::Additive;
    qfx.iAtlasCols = 4;
    qfx.iAtlasRows = 1;
    qfx.iAtlasFrameCount = 4;
    qfx.fAtlasFps = t.qTrailAtlasFps;
    qfx.bAtlasLoop = false;
    
    const EntityID trailId = SpawnBillboardAsset(world, qfx, "Irelia_Q_Trail");

    FxBillboardComponent lead = qfx;
    lead.texturePath = kPathQLeadingEdge;
    lead.fWidth = t.qTrailWidth * 1.25f;
    lead.fHeight = t.qTrailHeight * 2.0f;
    lead.fFadeIn = 0.02f;
    lead.fFadeOut = 0.18f;
    lead.vColor = { t.qTrailColor.x, t.qTrailColor.y, t.qTrailColor.z, t.qTrailColor.w * 0.9f };
    lead.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, lead);

    return trailId;
}

EntityID IreliaFx::SpawnQMark(CWorld& world, EntityID target, f32_t fLifeTime)
{
    FxBillboardComponent mark{};
    mark.attachTo = target;
    //?ㅽ봽???쇰?濡?以꾩엫 - ?먯옉 諛섏쁺
    mark.vAttachOffset = { 0.f, 1.f, 0.f };
    mark.texturePath = kPathQMark;
    //?ш린 ?뺣?
    mark.fWidth = 1.8f;
    mark.fHeight = 1.8f;
    mark.fLifetime = fLifeTime;
    mark.bBillboard = true;

    return SpawnBillboardAsset(world, mark, "Irelia_Q_Mark");
}

IreliaFx::IreliaWHoldFxIds IreliaFx::SpawnWSpinLayers(CWorld& world, EntityID owner, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();

    FxBillboardComponent wfx{};
    wfx.attachTo = owner;
    wfx.vAttachOffset = { 0.f, 1.f, 0.f };
    wfx.texturePath = kPathWSpin;
    wfx.fWidth = 2.f;
    wfx.fHeight = 3.f;
    wfx.fLifetime = fLifetime;
    wfx.fFadeIn = 0.06f;
    wfx.fFadeOut = 0.25f;
    wfx.bBillboard = true;
    wfx.vColor = { 0.95f, 1.05f, 1.7f, 0.85f };
    wfx.blendMode = eBlendPreset::Additive;
    wfx.iAtlasCols = 2;
    wfx.iAtlasRows = 2;
    wfx.iAtlasFrameCount = 4;
    wfx.fAtlasFps = 12.f;
    wfx.bAtlasLoop = true;

    IreliaWHoldFxIds ids{};
    ids.spinFxID = SpawnRuntimeBillboard(world, wfx);

    FxBillboardComponent shield = wfx;
    shield.texturePath = kPathWShieldSoft;
    shield.fWidth = t.fWHoldShieldSize;
    shield.fHeight = t.fWHoldShieldSize;
    shield.fStartDelay = 0.03f;
    shield.vColor = t.vWHoldShieldColor;
    shield.iAtlasCols = 1;
    shield.iAtlasRows = 1;
    shield.iAtlasFrameCount = 1;
    shield.fAtlasFps = 0.f;
    ids.shieldFxID = SpawnRuntimeBillboard(world, shield);

    FxBillboardComponent glow = wfx;
    glow.texturePath = kPathWAmbientGlow;
    glow.fWidth = t.fWHoldGlowSize;
    glow.fHeight = t.fWHoldGlowSize;
    glow.fFadeIn = 0.12f;
    glow.fFadeOut = 0.35f;
    glow.fStartDelay = 0.06f;
    glow.vColor = t.vWHoldGlowColor;
    glow.iAtlasCols = 1;
    glow.iAtlasRows = 1;
    glow.iAtlasFrameCount = 1;
    glow.fAtlasFps = 0.f;
    ids.glowFxID = SpawnRuntimeBillboard(world, glow);

    FxBillboardComponent block = shield;
    block.texturePath = kPathWBlockMult;
    block.fWidth = t.fWHoldShieldSize * 0.85f;
    block.fHeight = t.fWHoldShieldSize * 0.85f;
    block.fStartDelay = 0.09f;
    block.fFadeIn = 0.05f;
    block.fFadeOut = 0.30f;
    block.vColor = { t.vWHoldShieldColor.x, t.vWHoldShieldColor.y, t.vWHoldShieldColor.z,
        t.vWHoldShieldColor.w * 0.75f };
    ids.blockFxID = SpawnRuntimeBillboard(world, block);

    return ids;
}

EntityID IreliaFx::SpawnWSpin(CWorld& world, EntityID owner, f32_t fLifetime)
{
    FxBillboardComponent wfx{};
    wfx.attachTo = owner;
    //?ㅽ봽??
    wfx.vAttachOffset = { 0.f, 1.f, 0.f };
    wfx.texturePath = kPathWSpin;
    //?ш린 ?ㅼ?!
    wfx.fWidth = 2.f;
    wfx.fHeight = 3.f;
    wfx.fLifetime = fLifetime;
    wfx.bBillboard = true;
    wfx.iAtlasCols = 2;
    wfx.iAtlasRows = 2;
    wfx.iAtlasFrameCount = 4;
    wfx.fAtlasFps = 12.f;
    wfx.bAtlasLoop = true;
    return SpawnBillboardAsset(world, wfx, "Irelia_W_Spin");
}

void IreliaFx::SpawnWReleaseLayers(CWorld& world, EntityID owner, f32_t fLifetime, f32_t fSize,
    const Vec4& vBladesColor, const Vec4& vGlowColor,
    const Vec3& vAttachOffset)
{
    // ??DIAG (寃利????쒓굅 ?덉젙) ??W2 release ?몄텧 ?뺤씤
    {
        char dbg[256];
        sprintf_s(dbg,
            "[WRel] Spawn owner=%u life=%.2f size=%.2f off=(%.2f,%.2f,%.2f) blades=(%.2f,%.2f,%.2f,%.2f) glow=(%.2f,%.2f,%.2f,%.2f)\n",
            owner, fLifetime, fSize,
            vAttachOffset.x, vAttachOffset.y, vAttachOffset.z,
            vBladesColor.x, vBladesColor.y, vBladesColor.z, vBladesColor.w,
            vGlowColor.x,   vGlowColor.y,   vGlowColor.z,   vGlowColor.w);
    }

    // Layer 1: swipe_blades (RGBA 移쇰궇 ?ㅻ（?? AlphaBlend)
    FxBillboardComponent base{};
    base.attachTo      = owner;
    base.vAttachOffset = vAttachOffset;
    base.texturePath   = kPathWSwipeBlades;
    base.fWidth        = fSize;
    base.fHeight       = fSize;
    base.fLifetime     = fLifetime;
    base.bBillboard    = true;
    base.vColor        = vBladesColor;
    base.blendMode     = eBlendPreset::AlphaBlend;
    SpawnBillboardAsset(world, base, "Irelia_W_Release_Blades");

    // Layer 2: mis_glow (luminance, Additive 釉붾（ tint)
    FxBillboardComponent glow = base;
    glow.texturePath = kPathWMisGlow;
    glow.vColor      = vGlowColor;
    glow.blendMode   = eBlendPreset::Additive;
    SpawnBillboardAsset(world, glow, "Irelia_W_Release_Glow");

    FxBillboardComponent after = base;
    after.fStartDelay = 0.05f;
    after.fLifetime = fLifetime * 0.65f;
    after.fWidth = fSize * 1.15f;
    after.fHeight = fSize * 1.15f;
    after.fFadeOut = after.fLifetime * 0.75f;
    after.vColor = { vBladesColor.x, vBladesColor.y, vBladesColor.z, vBladesColor.w * 0.45f };
    SpawnRuntimeBillboard(world, after);

    FxBillboardComponent shard = base;
    shard.texturePath = kPathShards;
    shard.fStartDelay = 0.08f;
    shard.fLifetime = 0.28f;
    shard.fWidth = fSize * 0.7f;
    shard.fHeight = fSize * 0.7f;
    shard.fFadeOut = 0.22f;
    shard.vColor = vGlowColor;
    shard.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, shard);

    // blade_erode ??SpawnWStage2Slash 媛 forward ?щ옒?쒕줈 2??spawn ???ш린???쒖쇅.
}

EntityID IreliaFx::SpawnEBlade(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vGround, EntityID owner, f32_t fScale, const Vec3& vRotation,
    f32_t fWorldYawSpinSpeed)
{
    // IreliaBladeSystem ??湲곗〈 SpawnPlaced 洹몃?濡??꾩엫 (caller ?⑥닚??紐⑹쟻)
    return CIreliaBladeSystem::SpawnPlaced(world, pRenderer, vGround, owner,
        fScale, vRotation, fWorldYawSpinSpeed);
}

EntityID IreliaFx::SpawnEBeam(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vMid, f32_t fYaw, f32_t fLength,
    f32_t fGirth, f32_t fBaseScale, f32_t fAxisScale)
{
    FxMeshComponent beam{};
    beam.vWorldPos   = vMid;
    beam.vRotation   = { 0.f, fYaw, 0.f };
    beam.modelPath   = kPathEBeamFbx;
    beam.texturePath = kPathEBeamTex;
    beam.vScale      = { fBaseScale * fGirth,
                         fBaseScale * fGirth,
                         fBaseScale * fLength * fAxisScale };
    beam.vColor      = { 0.45f, 0.55f, 1.8f, 0.85f };
    beam.blendMode   = eBlendPreset::Additive;
    beam.bDepthWrite = false;
    beam.fFadeIn     = 0.02f;
    beam.fFadeOut    = 0.28f;
    beam.fLifetime   = 0.46f;
    return CFxMeshSystem::Spawn(world, pRenderer, beam);
}

void IreliaFx::SpawnWStage2Slash(CWorld& world, EntityID owner, const Vec3& vForward)
{
    FxBillboardComponent sfx1{};
    sfx1.attachTo      = owner;
    sfx1.vAttachOffset = { vForward.x * 2.0f, 1.0f, vForward.z * 2.0f };
    sfx1.vVelocity     = { 0.f, 0.f, 0.f };
    sfx1.texturePath   = kPathWBlade;
    sfx1.fWidth        = 4.0f;
    sfx1.fHeight       = 2.0f;
    sfx1.fLifetime     = 0.4f;
    sfx1.bBillboard    = true;
    SpawnBillboardAsset(world, sfx1, "Irelia_W_Stage2_Slash_A");

    FxBillboardComponent sfx2 = sfx1;
    sfx2.vAttachOffset.y += 0.2f;
    SpawnBillboardAsset(world, sfx2, "Irelia_W_Stage2_Slash_B");
}

EntityID IreliaFx::SpawnStunMark(CWorld& world, EntityID target, f32_t fDuration)
{
    return SpawnQMark(world, target, fDuration);
}

void IreliaFx::SpawnRBladeFan(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vHitPos, const Vec3& vForward,
    int32_t iCount, f32_t fSpreadRad, f32_t fPlaceDist,
    f32_t fLifetime, f32_t fScale, const Vec3& vRotation,
    bool bTriangle, f32_t fTipBoost, f32_t fSideShrink)
{
    if (!pRenderer || iCount <= 0)
        return;

    {
        const f32_t fwdYaw = std::atan2f(vForward.x, vForward.z);
        const f32_t shrinkClamp = (fSideShrink < 0.f) ? 0.f
            : (fSideShrink > 0.9f ? 0.9f : fSideShrink);

        const f32_t spreadDuration = 0.28f;
        const f32_t maxStartDelay = 0.14f;
        const f32_t startRadius = 0.35f;

        auto BuildBlade = [&](const Vec3& pos, const Vec3& velocity,
            f32_t lifetime, f32_t startDelay, f32_t fadeIn, f32_t fadeOut,
            f32_t bladeYaw, f32_t bladeScale) -> FxMeshComponent
        {
            FxMeshComponent blade{};
            blade.vWorldPos = pos;
            blade.vVelocity = velocity;
            blade.vRotation = { vRotation.x, vRotation.y + bladeYaw, vRotation.z };
            blade.vScale = { bladeScale, bladeScale, bladeScale };
            blade.modelPath = kPathRBladeFbx;
            blade.texturePath = kPathRBladeTex;
            blade.vColor = { 0.7f, 0.9f, 1.45f, 1.0f };
            blade.blendMode = eBlendPreset::AlphaBlend;
            blade.fLifetime = lifetime;
            blade.fStartDelay = startDelay;
            blade.fFadeIn = fadeIn;
            blade.fFadeOut = fadeOut;
            blade.fAlphaClip = 0.f;
            blade.bDepthWrite = false;
            blade.RefreshMaterialFromLegacyFields();
            return blade;
        };

        for (i32_t i = 0; i < iCount; ++i)
        {
            const f32_t t = (iCount == 1)
                ? 0.5f
                : static_cast<f32_t>(i) / static_cast<f32_t>(iCount - 1);
            const f32_t sideRatio = std::fabs(2.f * t - 1.f);
            const f32_t angleOffset = (-fSpreadRad * 0.5f) + fSpreadRad * t;

            const f32_t bladeYaw = fwdYaw + angleOffset;
            const Vec3 bladeDir{ std::sinf(bladeYaw), 0.f, std::cosf(bladeYaw) };

            f32_t bladeScale = fScale;
            f32_t fwdPush = 0.f;
            if (bTriangle)
            {
                fwdPush = fTipBoost * (1.f - sideRatio);
                bladeScale = fScale * (1.f - shrinkClamp * sideRatio);
            }

            const Vec3 startPos{
                vHitPos.x + bladeDir.x * startRadius + vForward.x * (0.2f + fwdPush * 0.25f),
                vHitPos.y,
                vHitPos.z + bladeDir.z * startRadius + vForward.z * (0.2f + fwdPush * 0.25f)
            };

            const Vec3 finalPos{
                vHitPos.x + bladeDir.x * fPlaceDist + vForward.x * fwdPush,
                vHitPos.y,
                vHitPos.z + bladeDir.z * fPlaceDist + vForward.z * fwdPush
            };

            const f32_t startDelay = sideRatio * maxStartDelay;
            const Vec3 velocity{
                (finalPos.x - startPos.x) / spreadDuration,
                (finalPos.y - startPos.y) / spreadDuration,
                (finalPos.z - startPos.z) / spreadDuration
            };

            FxMeshComponent movingBlade = BuildBlade(
                startPos,
                velocity,
                spreadDuration + 0.08f,
                startDelay,
                0.025f,
                0.08f,
                bladeYaw,
                bladeScale);
            CFxMeshSystem::Spawn(world, pRenderer, movingBlade);

            const f32_t settleDelay = startDelay + spreadDuration * 0.85f;
            const f32_t settleLifetime = (fLifetime > settleDelay)
                ? (fLifetime - settleDelay)
                : 0.1f;

            FxMeshComponent settledBlade = BuildBlade(
                finalPos,
                { 0.f, 0.f, 0.f },
                settleLifetime,
                settleDelay,
                0.05f,
                0.35f,
                bladeYaw,
                bladeScale);
            CFxMeshSystem::Spawn(world, pRenderer, settledBlade);
        }

        return;
    }

}

IreliaFx::IreliaEPlacedFxIds IreliaFx::SpawnEPlacedLayers(CWorld& world, const Vec3& vBladePos, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const Vec3 ground{ vBladePos.x, vBladePos.y - 2.95f + t.eGroundYOffset, vBladePos.z };

    FxBillboardComponent glow{};
    glow.vWorldPos = ground;
    glow.texturePath = kPathEGroundTyphoon;
    glow.fWidth = t.eGroundGlowSize;
    glow.fHeight = t.eGroundGlowSize;
    glow.fLifetime = fLifetime;
    glow.fFadeIn = 0.08f;
    glow.fFadeOut = 0.25f;
    glow.bBillboard = false;
    glow.vColor = t.eGroundGlowColor;
    glow.blendMode = eBlendPreset::AlphaBlend;
    glow.fAlphaClip = 0.01f;

    IreliaEPlacedFxIds ids{};
    ids.groundGlowFxID = SpawnBillboardAsset(world, glow, "Irelia_E_Ground_Typhoon");

    FxBillboardComponent core = glow;
    core.fWidth = t.eGroundCoreSize;
    core.fHeight = t.eGroundCoreSize;
    core.vColor = t.eGroundCoreColor;
    core.blendMode = eBlendPreset::AlphaBlend;
    core.fAlphaClip = 0.01f;
    ids.groundCoreFxID = SpawnBillboardAsset(world, core, "Irelia_E_Ground_Core");

    return ids;
}

void IreliaFx::SpawnECloseLayers(CWorld& world, const Vec3& vStart, const Vec3& vEnd, f32_t fLifetime)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();
    const f32_t dx = vEnd.x - vStart.x;
    const f32_t dz = vEnd.z - vStart.z;
    const f32_t dist = std::sqrtf(dx * dx + dz * dz);
    const f32_t len = (dist > 0.1f) ? dist : 0.1f;
    const f32_t yaw = std::atan2f(dx, dz);
    const Vec3 dir{ dx / len, 0.f, dz / len };
    const Vec3 right{ dir.z, 0.f, -dir.x };
    const Vec3 mid{
        (vStart.x + vEnd.x) * 0.5f,
        (vStart.y + vEnd.y) * 0.5f,
        (vStart.z + vEnd.z) * 0.5f
    };
    const f32_t groundY = mid.y - 2.95f + t.eGroundYOffset;

    FxBillboardComponent dark{};
    dark.vWorldPos = { mid.x, groundY, mid.z };
    dark.texturePath = kPathEStunBeamDark;
    dark.fWidth = t.eCloseBeamWidth * t.fECloseDarkWidthMul;
    dark.fHeight = len;
    dark.fYaw = yaw;
    dark.fLifetime = fLifetime;
    dark.fFadeIn = 0.03f;
    dark.fFadeOut = 0.25f;
    dark.bBillboard = false;
    dark.vColor = { t.eCloseBeamColor.x, t.eCloseBeamColor.y, t.eCloseBeamColor.z,
        t.eCloseBeamColor.w * 0.55f };
    dark.blendMode = eBlendPreset::Additive;
    SpawnBillboardAsset(world, dark, "Irelia_E_Close_Beam_Dark");

    FxBillboardComponent sideRail = dark;
    sideRail.fWidth = t.eCloseBeamWidth;
    sideRail.vColor = { 0.18f, 0.32f, 1.35f, 0.48f };
    sideRail.vWorldPos = {
        mid.x + right.x * 0.32f,
        groundY + 0.01f,
        mid.z + right.z * 0.32f
    };
    SpawnRuntimeBillboard(world, sideRail);
    sideRail.vWorldPos = {
        mid.x - right.x * 0.32f,
        groundY + 0.02f,
        mid.z - right.z * 0.32f
    };
    sideRail.vColor.w *= 0.88f;
    SpawnRuntimeBillboard(world, sideRail);

    FxBillboardComponent beam = dark;
    beam.texturePath = kPathEStunBeam;
    beam.fWidth = t.eCloseBeamWidth;
    beam.vColor = t.eCloseBeamColor;
    SpawnBillboardAsset(world, beam, "Irelia_E_Close_Beam");

    FxBillboardComponent core = beam;
    core.texturePath = kPathEStunTrail;
    core.fWidth = t.eCloseBeamWidth * t.fECloseCoreWidthMul;
    core.fLifetime = 0.32f;
    core.fFadeIn = 0.02f;
    core.fFadeOut = 0.22f;
    core.fStartDelay = 0.02f;
    core.vColor = { t.eCloseBeamColor.x * 1.25f, t.eCloseBeamColor.y * 1.25f,
        t.eCloseBeamColor.z * 1.25f, t.eCloseBeamColor.w };
    SpawnRuntimeBillboard(world, core);

    FxBillboardComponent spark{};
    spark.vWorldPos = mid;
    spark.texturePath = kPathEWarningSpark;
    spark.fWidth = t.eCloseSparkSize;
    spark.fHeight = t.eCloseSparkSize;
    spark.fLifetime = fLifetime;
    spark.fFadeIn = 0.02f;
    spark.fFadeOut = 0.28f;
    spark.bBillboard = true;
    spark.vColor = t.eCloseSparkColor;
    spark.blendMode = eBlendPreset::Additive;
    SpawnBillboardAsset(world, spark, "Irelia_E_Close_Spark_Mid");

    FxBillboardComponent edgeSpark = spark;
    edgeSpark.fWidth = t.eCloseSparkSize * 0.7f;
    edgeSpark.fHeight = t.eCloseSparkSize * 0.7f;
    edgeSpark.vWorldPos = vStart;
    SpawnBillboardAsset(world, edgeSpark, "Irelia_E_Close_Spark_A");
    edgeSpark.vWorldPos = vEnd;
    SpawnBillboardAsset(world, edgeSpark, "Irelia_E_Close_Spark_B");

    FxBillboardComponent flash = spark;
    flash.fWidth = t.eCloseSparkSize * 1.45f;
    flash.fHeight = t.eCloseSparkSize * 1.45f;
    flash.fLifetime = 0.18f;
    flash.fFadeIn = 0.01f;
    flash.fFadeOut = 0.16f;
    flash.fStartDelay = 0.08f;
    flash.vColor = t.vECloseFlashColor;
    SpawnRuntimeBillboard(world, flash);

    FxBillboardComponent splitFlash = flash;
    splitFlash.texturePath = kPathLensflareStreak;
    splitFlash.fWidth = t.eCloseSparkSize * 0.24f;
    splitFlash.fHeight = t.eCloseSparkSize * 1.24f;
    splitFlash.fLifetime = 0.22f;
    splitFlash.fStartDelay = 0.12f;
    splitFlash.vWorldPos = {
        vStart.x + (vEnd.x - vStart.x) * 0.34f,
        mid.y + 0.95f,
        vStart.z + (vEnd.z - vStart.z) * 0.34f
    };
    SpawnRuntimeBillboard(world, splitFlash);
    splitFlash.fStartDelay = 0.16f;
    splitFlash.vColor.w *= 0.82f;
    splitFlash.vWorldPos = {
        vStart.x + (vEnd.x - vStart.x) * 0.68f,
        mid.y + 0.90f,
        vStart.z + (vEnd.z - vStart.z) * 0.68f
    };
    SpawnRuntimeBillboard(world, splitFlash);

    FxBillboardComponent afterglow = dark;
    afterglow.fWidth = t.eCloseBeamWidth * t.fECloseAfterglowWidthMul;
    afterglow.fLifetime = fLifetime + 0.2f;
    afterglow.fFadeIn = 0.08f;
    afterglow.fFadeOut = 0.45f;
    afterglow.fStartDelay = 0.12f;
    afterglow.vColor = t.vECloseAfterglowColor;
    SpawnRuntimeBillboard(world, afterglow);

    f32_t fStreakLifetime = t.fECloseStreakLifetime;
    if (fStreakLifetime < 0.05f)
        fStreakLifetime = 0.05f;

    FxBillboardComponent streak{};
    streak.vWorldPos = { vStart.x, groundY + 0.03f, vStart.z };
    streak.vVelocity = { dir.x * len / fStreakLifetime, 0.f, dir.z * len / fStreakLifetime };
    streak.texturePath = kPathLensflareStreak;
    streak.fWidth = t.fECloseStreakWidth;
    streak.fHeight = len * 0.35f;
    streak.fYaw = yaw;
    streak.fLifetime = fStreakLifetime;
    streak.fFadeIn = 0.02f;
    streak.fFadeOut = 0.12f;
    streak.bBillboard = false;
    streak.vColor = t.vECloseFlashColor;
    streak.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, streak);

    FxBillboardComponent reverseStreak = streak;
    reverseStreak.vWorldPos = { vEnd.x, groundY + 0.04f, vEnd.z };
    reverseStreak.vVelocity = { -dir.x * len / fStreakLifetime, 0.f, -dir.z * len / fStreakLifetime };
    reverseStreak.fStartDelay = 0.06f;
    reverseStreak.vColor = { t.vECloseFlashColor.x, t.vECloseFlashColor.y,
        t.vECloseFlashColor.z, t.vECloseFlashColor.w * 0.7f };
    SpawnRuntimeBillboard(world, reverseStreak);
}

EntityID IreliaFx::SpawnRPulse(CWorld& world, const Vec3& vOrigin, const Vec3& vForward, f32_t fSpeed,
    f32_t fLifetime, f32_t fWidth, f32_t fHeight, f32_t fYOffset, f32_t fFwdOffset, f32_t fYawOffset)
{
    const Irelia::IreliaTuning& t = Irelia::GetTuning();

    FxBillboardComponent rfx{};
    rfx.attachTo = NULL_ENTITY;
    // Offset/size are exposed through Irelia Live Tuning.
    rfx.vWorldPos = { vOrigin.x + vForward.x * fFwdOffset,
                        vOrigin.y + fYOffset,
                        vOrigin.z + vForward.z * fFwdOffset };
    // Speed is passed from IreliaTuning::waveSpeed.
    rfx.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    rfx.texturePath = kPathRPulse;  
    rfx.fWidth = fWidth;
    rfx.fHeight = fHeight;
    rfx.fYaw = std::atan2f(vForward.x, vForward.z) + fYawOffset;
    rfx.bBillboard = false;     // 吏硫??뚯쟾 紐⑤뱶
    rfx.fLifetime = fLifetime;
    rfx.fFadeOut = fLifetime * 0.35f;
    rfx.vColor = { 1.0f, 1.08f, 1.55f, 0.9f };
    rfx.blendMode = eBlendPreset::Additive;

    const EntityID pulseId = SpawnBillboardAsset(world, rfx, "Irelia_R_Pulse");

    FxBillboardComponent trail = rfx;
    trail.texturePath = kPathRTrail;
    trail.fWidth = fWidth * t.fRTrailWidthMul;
    trail.fHeight = fHeight * t.fRTrailHeightMul;
    trail.fFadeIn = 0.02f;
    trail.fFadeOut = fLifetime * 0.55f;
    trail.vColor = t.vRTrailColor;
    SpawnRuntimeBillboard(world, trail);

    FxBillboardComponent lead = rfx;
    lead.texturePath = kPathRLead;
    lead.fWidth = fWidth * 0.85f;
    lead.fHeight = fHeight * 0.65f;
    lead.fLifetime = fLifetime * 0.6f;
    lead.fFadeIn = 0.01f;
    lead.fFadeOut = fLifetime * 0.35f;
    lead.vColor = t.vRLeadColor;
    SpawnRuntimeBillboard(world, lead);

    return pulseId;
}

void IreliaFx::SpawnRHitLayers(CWorld& world, const Vec3& vHitPos, const Vec3& vForward, f32_t fLifetime)
{
    const f32_t yaw = std::atan2f(vForward.x, vForward.z);
    const Vec3 ground{ vHitPos.x, vHitPos.y + 0.03f, vHitPos.z };

    FxBillboardComponent ring{};
    ring.vWorldPos = ground;
    ring.texturePath = kPathRDisarmRing;
    ring.fWidth = 3.4f;
    ring.fHeight = 3.4f;
    ring.fYaw = yaw;
    ring.fLifetime = fLifetime;
    ring.fFadeIn = 0.03f;
    ring.fFadeOut = 0.45f;
    ring.bBillboard = false;
    ring.vColor = { 0.7f, 0.98f, 1.8f, 0.55f };
    ring.blendMode = eBlendPreset::Additive;
    SpawnRuntimeBillboard(world, ring);

    FxBillboardComponent wall = ring;
    wall.texturePath = kPathRWallDecal;
    wall.fWidth = 5.5f;
    wall.fHeight = 2.2f;
    wall.fStartDelay = 0.08f;
    wall.fLifetime = fLifetime + 0.4f;
    wall.fFadeOut = 0.6f;
    wall.vColor = { 0.6f, 0.9f, 1.55f, 0.45f };
    SpawnRuntimeBillboard(world, wall);

    FxBillboardComponent flash = ring;
    flash.texturePath = L"Client/Bin/Resource/Texture/FX/Irelia/irelia_base_r_hit_flash.png";
    flash.fWidth = 2.6f;
    flash.fHeight = 2.6f;
    flash.fLifetime = 0.22f;
    flash.fFadeIn = 0.01f;
    flash.fFadeOut = 0.18f;
    flash.bBillboard = true;
    flash.vColor = { 1.3f, 1.28f, 2.05f, 0.95f };
    SpawnRuntimeBillboard(world, flash);
}
