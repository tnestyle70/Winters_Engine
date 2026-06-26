#include "GameObject/Champion/Garen/GarenFxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"
#include <cmath>

namespace
{
    // ★ v2: 실제 가렌 추출 구조 — particles/fbx/, particles/ 하위
    constexpr const char* kPathESpinFbx =
        "Texture/FX/Garen/particles/fbx/garen_base_e_spin.fbx";
    constexpr const char* kPathWShieldFbx =
        "Texture/FX/Garen/particles/fbx/garen_base_w_shield.fbx";
    constexpr const char* kPathRSwordFbx =
        "Texture/FX/Garen/particles/fbx/garen_base_r_sword_plane.fbx";

    // 1차는 가렌 base aura/ball 텍스처 사용 — 정확한 머티리얼 매핑은 RenderDoc 검증 후 확정
    constexpr const wchar_t* kPathQTrailTex =
        L"Texture/FX/Garen/particles/garen_aura_self.png";
    constexpr const wchar_t* kPathWShieldTex =
        L"Texture/FX/Garen/particles/garen_aura_self_02.png";
    constexpr const wchar_t* kPathRSwordTex =
        L"Texture/FX/Garen/particles/garen_ball01.png";
}

void GarenFx::SpawnQTrail(CWorld& world, EntityID owner, f32_t fLifetime)
{
    if (owner == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.5f, 0.f };
    fx.texturePath = kPathQTrailTex;
    fx.fWidth = 1.5f;
    fx.fHeight = 1.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.0f, 0.9f, 0.4f, 1.0f };  // 데마시아 황금
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.4f;

    CFxSystem::Spawn(world, fx);
}

void GarenFx::SpawnWShield(CWorld & world, EntityID owner, f32_t fDuration)
{
    if (owner == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathWShieldTex;
    fx.fWidth = 2.5f;
    fx.fHeight = 2.5f;
    fx.bBillboard = true;
    fx.fLifetime = fDuration;
    fx.vColor = { 0.8f, 0.7f, 0.3f, 0.7f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fDuration * 0.3f;
    CFxSystem::Spawn(world, fx);
}

EntityID GarenFx::SpawnESpinBlade(CWorld & world, Engine::CFxStaticMeshRenderer * pRenderer, EntityID owner, f32_t fDuration)
{
    if (owner == NULL_ENTITY)
        return NULL_ENTITY;

    if (pRenderer)
    {
        FxMeshComponent fxmesh{};
        fxmesh.attachTo = owner;
        fxmesh.vAttachOffset = { 0.f, 1.0f, 0.f };
        fxmesh.vScale = { 0.018f, 0.018f, 0.018f };
        fxmesh.fWorldYawSpinSpeed = 14.f;
        fxmesh.modelPath = kPathESpinFbx;
        fxmesh.texturePath = kPathQTrailTex;
        fxmesh.fLifetime = fDuration;
        fxmesh.vColor = { 1.4f, 1.1f, 0.35f, 0.9f };
        fxmesh.blendMode = eBlendPreset::Additive;
        fxmesh.bDepthWrite = false;
        fxmesh.fFadeIn = 0.05f;
        fxmesh.fFadeOut = 0.2f;

        const EntityID meshId = CFxMeshSystem::Spawn(world, pRenderer, fxmesh);
        if (meshId != NULL_ENTITY)
            return meshId;
    }

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathQTrailTex;
    fx.fWidth = 3.0f;
    fx.fHeight = 3.0f;
    fx.bBillboard = true;
    fx.fLifetime = fDuration;
    fx.vColor = { 1.4f, 1.1f, 0.35f, 0.75f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeIn = 0.05f;
    fx.fFadeOut = 0.2f;
    return CFxSystem::Spawn(world, fx);
}

void GarenFx::SpawnRSword(CWorld& world, EntityID target, f32_t fLifetime)
{
    if (target == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = target;
    fx.vAttachOffset = { 0.f, 3.0f, 0.f };
    fx.texturePath = kPathRSwordTex;
    fx.fWidth = 1.5f;
    fx.fHeight = 3.0f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.5f, 1.3f, 0.5f, 1.0f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.5f;
    CFxSystem::Spawn(world, fx);
}
