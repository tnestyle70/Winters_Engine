#include "GameObject/Champion/Kalista/KalistaFxPresets.h"
#include "GameObject/Champion/Kalista/Kalista_Tuning.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"
#include "WintersMath.h"
#include <cmath>

namespace
{
    constexpr const char* kPathQSpearFbx =
        "Client/Bin/Resource/Texture/FX/Kalista/fbx/kalista_base_q_mis_spear.fbx";
    constexpr const char* kPathESpearFbx =
        "Client/Bin/Resource/Texture/FX/Kalista/fbx/kalista_base_e_spear_hold.fbx";
    constexpr const wchar_t* kPathQSpearTex =
        L"Client/Bin/Resource/Texture/FX/Kalista/kalista_base_q_mis_glow_color.png";
    constexpr const wchar_t* kPathESpearTex =
        L"Client/Bin/Resource/Texture/FX/Kalista/kalista_base_e_spear_glow.png";
    constexpr const wchar_t* kPathERendWispAtlasTex =
        L"Client/Bin/Resource/Texture/Character/Kalista/particles/kalista_base_q_precast_wisps.png";
}

EntityID KalistaFx::SpawnQSpear(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vOrigin, const Vec3& vForward,
    f32_t fSpeed, f32_t fLifetime, f32_t fScale)
{
    return SpawnQSpear(world, pRenderer, vOrigin, vForward, fSpeed, fLifetime,
        Vec3{ fScale, fScale, fScale });
}

EntityID KalistaFx::SpawnQSpear(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const Vec3& vOrigin, const Vec3& vForward,
    f32_t fSpeed, f32_t fLifetime, const Vec3& vScale)
{
    if (!pRenderer)
        return NULL_ENTITY;

    FxMeshComponent mesh{};
    mesh.vWorldPos = vOrigin;
    mesh.vVelocity = { vForward.x * fSpeed, 0.f, vForward.z * fSpeed };
    mesh.vRotation = { 0.f, std::atan2f(vForward.x, vForward.z) + WintersMath::kPi, 0.f };
    mesh.vScale = vScale;
    mesh.modelPath = kPathQSpearFbx;
    mesh.texturePath = kPathQSpearTex;
    mesh.vColor = { 0.5f, 1.0f, 1.5f, 1.0f };
    mesh.blendMode = eBlendPreset::AlphaBlend;
    mesh.fLifetime = fLifetime;
    mesh.bDepthWrite = false;

    return CFxMeshSystem::Spawn(world, pRenderer, mesh);
}

EntityID KalistaFx::SpawnESpearStuck(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    EntityID target, f32_t fScale)
{
    if (!pRenderer || target == NULL_ENTITY)
        return NULL_ENTITY;

    //매 hit 마다 다른 각도/위치 (사실적 누적 효과)
    const f32_t randYaw = (static_cast<f32_t>(rand()) / RAND_MAX) * 6.2832f;       // 0 ~ 2π
    const f32_t randTiltX = (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f;   // ±0.3 rad
    const f32_t randTiltZ = (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f;

    const f32_t randOffsetX = (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f;
    const f32_t randOffsetY = 1.0f + (static_cast<f32_t>(rand()) / RAND_MAX) * 1.0f;  // 1.0 ~ 2.0
    const f32_t randOffsetZ = (static_cast<f32_t>(rand()) / RAND_MAX - 0.5f) * 0.6f;

    FxMeshComponent mesh{};
    mesh.attachTo = target;
    mesh.vAttachOffset = { randOffsetX, randOffsetY, randOffsetZ };   // 위치 분산
    mesh.vRotation = { 1.5708f + randTiltX, randYaw, randTiltZ }; // 각도 분산
    mesh.vScale = { fScale, fScale, fScale };
    mesh.modelPath = kPathESpearFbx;
    mesh.texturePath = kPathESpearTex;
    mesh.vColor = { 0.5f, 1.0f, 1.5f, 1.0f };
    mesh.blendMode = eBlendPreset::AlphaBlend;
    mesh.fLifetime = 30.f;
    mesh.bDepthWrite = false;
    return CFxMeshSystem::Spawn(world, pRenderer, mesh);
}

void KalistaFx::SpawnEExplode(CWorld& world, EntityID target, f32_t fLifetime)
{
    if (target == NULL_ENTITY)
        return;

    const Kalista::KalistaTuning& tuning = Kalista::GetTuning();
    const f32_t size = (tuning.eRendWispSize < 0.1f) ? 0.1f : tuning.eRendWispSize;
    const f32_t lifetime = (fLifetime < 0.03f) ? 0.03f : fLifetime;

    FxBillboardComponent fx{};
    fx.attachTo = target;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathERendWispAtlasTex;
    fx.fWidth = size;
    fx.fHeight = size;
    fx.bBillboard = true;
    fx.fLifetime = lifetime;
    fx.vColor = { 0.65f, 1.25f, 1.55f, 1.0f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeIn = 0.03f;
    fx.fFadeOut = lifetime * 0.25f;
    fx.iAtlasCols = 3;
    fx.iAtlasRows = 3;
    fx.iAtlasFrameCount = 9;
    fx.fAtlasFps = tuning.eRendWispAtlasFps;
    fx.bAtlasLoop = false;
    CFxSystem::Spawn(world, fx);
}
