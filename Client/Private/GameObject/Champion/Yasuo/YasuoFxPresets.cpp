#include "GameObject/Champion/Yasuo/YasuoFxPresets.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxCuePlayer.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "ECS/World.h"
#include <cmath>

namespace
{
    constexpr const char* kCueQSlash = "Yasuo.Q.Slash";
    constexpr const char* kCueQBuildUp = "Yasuo.Q.BuildUp";
    constexpr const char* kCueQTornado = "Yasuo.Q.Tornado";
    constexpr const char* kCueWWindWall = "Yasuo.W.WindWall";
    constexpr const char* kCueEDashTrail = "Yasuo.E.DashTrail";
    constexpr const char* kCueEQRing = "Yasuo.EQ.Ring";
    constexpr const char* kCueEQInnerWind = "Yasuo.EQ.InnerWind";
    constexpr const char* kCueRLandImpact = "Yasuo.R.LandImpact";
    constexpr const char* kCueRSwordGlow = "Yasuo.R.SwordGlow";

    constexpr const wchar_t* kPathQWind = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_q_wind.png";
    constexpr const wchar_t* kPathQBuildUp = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_q_build_up.png";
    constexpr const wchar_t* kPathTornadoMid = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_base_e_tonado_blend.png";
    constexpr const wchar_t* kPathTornadoGroundMis = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_q_wind.png";
    constexpr const wchar_t* kPathWDust = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_w_windwall_dust.png";
    constexpr const wchar_t* kPathEAfterImg = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_base_e_afterimage.png";
    constexpr const wchar_t* kPathEQInnerWind = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_base_e_inner_wind.png";
    constexpr const wchar_t* kPathEQGroundMis = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_q_wind.png";
    constexpr const wchar_t* kPathRSwordGlow = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_r_sword_glow.png";
    constexpr const wchar_t* kPathRLandDistort = L"Client/Bin/Resource/Texture/FX/Yasuo/color_yasuo_r_land_distort.png";

    constexpr const char* kPathTornadoFbx = "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_base_q_tornado_blade_cas.fbx";
    constexpr const char* kPathWWallFbx = "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_w_windwall_mesh.fbx";
    constexpr const char* kPathRSwordFbx = "Client/Bin/Resource/Texture/FX/Yasuo/fbx/yasuo_base_r_sword_wind2.fbx";

    bool_t HasCue(const char* pszCueName)
    {
        return CFxCuePlayer::FindCue(pszCueName).IsValid();
    }

    bool_t PlayCue(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
    {
        return CFxCuePlayer::Play(world, pszCueName, ctx) != NULL_ENTITY;
    }

    Vec3 ResolveEntityWorldPos(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return {};
    }
}

EntityID YasuoFx::SpawnQStraight(CWorld& world, const Vec3& vOrigin, const Vec3& vForward,
    f32_t fSpeed, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    cue.bOverrideVelocity = true;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    const EntityID cueId = CFxCuePlayer::Play(world, kCueQSlash, cue);
    if (cueId != NULL_ENTITY)
        return cueId;

    FxBillboardComponent fx{};
    fx.attachTo = NULL_ENTITY;
    fx.vWorldPos = { vOrigin.x, vOrigin.y + 1.0f, vOrigin.z };
    fx.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    fx.texturePath = kPathQWind;
    fx.fWidth = 1.5f;
    fx.fHeight = 0.6f;
    fx.fYaw = std::atan2f(vForward.x, vForward.z);
    fx.bBillboard = false;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.f, 1.f, 1.f, 1.f };
    fx.blendMode = eBlendPreset::Additive;
    fx.bBlockableByWindWall = true;
    return CFxSystem::Spawn(world, fx);
}

EntityID YasuoFx::SpawnQBuildUp(CWorld& world, EntityID owner, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = ResolveEntityWorldPos(world, owner);
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    const EntityID cueId = CFxCuePlayer::Play(world, kCueQBuildUp, cue);
    if (cueId != NULL_ENTITY)
        return cueId;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.5f, 0.f };
    fx.texturePath = kPathQBuildUp;
    fx.fWidth = 0.8f;
    fx.fHeight = 0.8f;
    fx.fLifetime = fLifetime;
    fx.bBillboard = true;
    fx.vColor = { 0.7f, 0.9f, 1.5f, 1.0f };
    fx.blendMode = eBlendPreset::Additive;
    return CFxSystem::Spawn(world, fx);
}

void YasuoFx::SpawnQTornado(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vOrigin, const Vec3& vForward,
    f32_t fSpeed, f32_t fLifetime, f32_t fScale,
    const Vec4& vColor)
{
    if (!pRenderer) return;

    FxCueContext cue{};
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    cue.bOverrideVelocity = true;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    cue.pFxMeshRenderer = pRenderer;
    if (PlayCue(world, kCueQTornado, cue))
        return;

    FxMeshComponent mesh{};
    mesh.vWorldPos = { vOrigin.x, vOrigin.y + 1.0f, vOrigin.z };
    mesh.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    mesh.vRotation = { 0.f, std::atan2f(vForward.x, vForward.z), 0.f };
    mesh.vScale = { fScale, fScale, fScale };
    mesh.modelPath = kPathTornadoFbx;
    mesh.texturePath = kPathTornadoMid;
    mesh.vColor = vColor;
    mesh.blendMode = eBlendPreset::Additive;
    mesh.fLifetime = fLifetime;
    mesh.fWorldYawSpinSpeed = 12.566f;
    mesh.bBlockableByWindWall = true;
    CFxMeshSystem::Spawn(world, pRenderer, mesh);

    FxBillboardComponent ground{};
    ground.vWorldPos = { vOrigin.x, vOrigin.y + 0.05f, vOrigin.z };
    ground.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    ground.texturePath = kPathTornadoGroundMis;
    ground.fWidth = 2.5f;
    ground.fHeight = 2.5f;
    ground.fYaw = std::atan2f(vForward.x, vForward.z);
    ground.bBillboard = false;
    ground.fLifetime = fLifetime;
    ground.vColor = vColor;
    ground.blendMode = eBlendPreset::Additive;
    ground.bBlockableByWindWall = true;
    CFxSystem::Spawn(world, ground);
}

void YasuoFx::SpawnWWindWall(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vOrigin, const Vec3& vForward,
    f32_t fLifetime, f32_t fWidth, f32_t fHeight,
    f32_t fMeshScale)
{
    if (!pRenderer) return;

    FxCueContext cue{};
    cue.vWorldPos = vOrigin;
    cue.vForward = vForward;
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    cue.pFxMeshRenderer = pRenderer;
    if (PlayCue(world, kCueWWindWall, cue))
        return;

    const f32_t fwdYaw = std::atan2f(vForward.x, vForward.z);
    const Vec3 vWallPos{ vOrigin.x + vForward.x * 1.5f, vOrigin.y, vOrigin.z + vForward.z * 1.5f };

    FxMeshComponent wall{};
    wall.vWorldPos = vWallPos;
    wall.vRotation = { 0.f, fwdYaw, 0.f };
    wall.vScale = { fWidth * fMeshScale, fHeight * 4.f * fMeshScale, 0.3f * fMeshScale };
    wall.modelPath = kPathWWallFbx;
    wall.texturePath = kPathWDust;
    wall.vColor = { 0.8f, 0.95f, 1.2f, 1.0f };
    wall.blendMode = eBlendPreset::Additive;
    wall.fLifetime = fLifetime;
    wall.bDepthWrite = false;
    CFxMeshSystem::Spawn(world, pRenderer, wall);

    FxBillboardComponent dust{};
    dust.vWorldPos = { vWallPos.x, vWallPos.y + 0.1f, vWallPos.z };
    dust.texturePath = kPathWDust;
    dust.fWidth = fWidth;
    dust.fHeight = 0.8f;
    dust.fYaw = fwdYaw;
    dust.bBillboard = false;
    dust.fLifetime = fLifetime;
    dust.vColor = { 0.8f, 0.95f, 1.2f, 0.6f };
    dust.blendMode = eBlendPreset::Additive;
    CFxSystem::Spawn(world, dust);
}

EntityID YasuoFx::SpawnEDashTrail(CWorld& world, EntityID owner, f32_t fLifetime)
{
    FxCueContext cue{};
    cue.attachTo = owner;
    cue.vWorldPos = ResolveEntityWorldPos(world, owner);
    cue.bOverrideLifetime = true;
    cue.fLifetimeOverride = fLifetime;
    const EntityID cueId = CFxCuePlayer::Play(world, kCueEDashTrail, cue);
    if (cueId != NULL_ENTITY)
        return cueId;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathEAfterImg;
    fx.fWidth = 1.4f;
    fx.fHeight = 1.6f;
    fx.fLifetime = fLifetime;
    fx.bBillboard = true;
    fx.vColor = { 0.6f, 0.85f, 1.3f, 1.0f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.5f;
    return CFxSystem::Spawn(world, fx);
}

void YasuoFx::SpawnEQRing(CWorld& world, EntityID owner,
    const Vec3& vCenter, f32_t fLifetime, f32_t fRadius)
{
    if (HasCue(kCueEQRing) && HasCue(kCueEQInnerWind))
    {
        FxCueContext ground{};
        ground.vWorldPos = vCenter;
        ground.bOverrideLifetime = true;
        ground.fLifetimeOverride = fLifetime;
        PlayCue(world, kCueEQRing, ground);

        FxCueContext glow{};
        glow.attachTo = owner;
        glow.vWorldPos = ResolveEntityWorldPos(world, owner);
        glow.bOverrideLifetime = true;
        glow.fLifetimeOverride = fLifetime;
        PlayCue(world, kCueEQInnerWind, glow);
        return;
    }

    FxBillboardComponent ground{};
    ground.vWorldPos = { vCenter.x, vCenter.y + 0.05f, vCenter.z };
    ground.texturePath = kPathEQGroundMis;
    ground.fWidth = fRadius * 2.f;
    ground.fHeight = fRadius * 2.f;
    ground.bBillboard = false;
    ground.fLifetime = fLifetime;
    ground.vColor = { 0.7f, 0.9f, 1.4f, 1.0f };
    ground.blendMode = eBlendPreset::Additive;
    CFxSystem::Spawn(world, ground);

    FxBillboardComponent glow{};
    glow.attachTo = owner;
    glow.vAttachOffset = { 0.f, 1.0f, 0.f };
    glow.texturePath = kPathEQInnerWind;
    glow.fWidth = fRadius * 1.5f;
    glow.fHeight = fRadius * 1.5f;
    glow.bBillboard = true;
    glow.fLifetime = fLifetime;
    glow.vColor = { 0.8f, 1.0f, 1.5f, 1.0f };
    glow.blendMode = eBlendPreset::Additive;
    glow.fFadeOut = fLifetime * 0.5f;
    CFxSystem::Spawn(world, glow);
}

void YasuoFx::SpawnRLastBreath(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vLandPos, EntityID owner, f32_t fLifetime)
{
    const bool_t bCanPlayLandCue = HasCue(kCueRLandImpact);
    const bool_t bCanPlaySwordCue = !pRenderer || HasCue(kCueRSwordGlow);
    if (bCanPlayLandCue && bCanPlaySwordCue)
    {
        if (pRenderer)
        {
            FxCueContext sword{};
            sword.attachTo = owner;
            sword.vWorldPos = ResolveEntityWorldPos(world, owner);
            sword.pFxMeshRenderer = pRenderer;
            sword.bOverrideLifetime = true;
            sword.fLifetimeOverride = fLifetime;
            PlayCue(world, kCueRSwordGlow, sword);
        }

        FxCueContext land{};
        land.vWorldPos = vLandPos;
        land.bOverrideLifetime = true;
        land.fLifetimeOverride = fLifetime;
        PlayCue(world, kCueRLandImpact, land);
        return;
    }

    if (pRenderer)
    {
        FxMeshComponent sword{};
        sword.attachTo = owner;
        sword.vAttachOffset = { 0.f, 1.5f, 0.f };
        sword.vScale = { 0.015f, 0.015f, 0.015f };
        sword.modelPath = kPathRSwordFbx;
        sword.texturePath = kPathRSwordGlow;
        sword.vColor = { 1.4f, 1.2f, 0.9f, 1.f };
        sword.blendMode = eBlendPreset::Additive;
        sword.fLifetime = fLifetime;
        sword.bDepthWrite = false;
        sword.fWorldYawSpinSpeed = 8.0f;
        CFxMeshSystem::Spawn(world, pRenderer, sword);
    }

    FxBillboardComponent land{};
    land.vWorldPos = { vLandPos.x, vLandPos.y + 0.05f, vLandPos.z };
    land.texturePath = kPathRLandDistort;
    land.fWidth = 3.0f;
    land.fHeight = 3.0f;
    land.bBillboard = false;
    land.fLifetime = fLifetime;
    land.vColor = { 1.4f, 1.2f, 0.9f, 1.f };
    land.blendMode = eBlendPreset::Additive;
    CFxSystem::Spawn(world, land);
}
