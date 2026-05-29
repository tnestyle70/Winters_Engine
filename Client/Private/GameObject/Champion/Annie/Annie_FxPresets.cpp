#include "GameObject/Champion/Annie/Annie_FxPresets.h"

#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"

#include <cmath>

namespace
{
    constexpr const wchar_t* kPathBAFlashTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_ash.png";
    constexpr const wchar_t* kPathQFireballTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_flames.png";
    constexpr const wchar_t* kPathQFireballTrailTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_q_mis_trail.png";
    constexpr const wchar_t* kPathWConeTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_w_grounddecalfinal.png";
    constexpr const wchar_t* kPathEShieldTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_circle.png";
    constexpr const wchar_t* kPathEGlowTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_e_buf_glow2.png";
    constexpr const wchar_t* kPathRSummonTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_brazier_flame_temp_01.png";
    constexpr const wchar_t* kPathStunChargeTex =
        L"Client/Bin/Resource/Texture/Character/Annie/particles/annie_base_glow.png";
}

namespace Annie::Fx
{
    void SpawnBAFireFlash(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathBAFlashTex;
        fx.fWidth = 1.2f;
        fx.fHeight = 1.2f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.3f, 0.6f, 0.2f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.45f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnQFireball(CWorld& world, EntityID owner, EntityID target, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.2f, 0.f };
            fx.texturePath = kPathQFireballTex;
            fx.fWidth = 1.0f;
            fx.fHeight = 1.0f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime * 0.4f;
            fx.vColor = { 1.4f, 0.7f, 0.2f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.2f;
            CFxSystem::Spawn(world, fx);
        }

        if (target != NULL_ENTITY)
        {
            FxBillboardComponent fx{};
            fx.attachTo = target;
            fx.vAttachOffset = { 0.f, 1.0f, 0.f };
            fx.texturePath = kPathQFireballTrailTex;
            fx.fWidth = 1.6f;
            fx.fHeight = 1.6f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime;
            fx.vColor = { 1.4f, 0.5f, 0.15f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.5f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnWConeFire(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 0.05f, 0.f };
        fx.texturePath = kPathWConeTex;
        fx.fWidth = 4.0f;
        fx.fHeight = 4.0f;
        fx.bBillboard = false;
        if (dir.x * dir.x + dir.z * dir.z > 0.0001f)
            fx.fYaw = static_cast<f32_t>(std::atan2(dir.x, dir.z));
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.4f, 0.5f, 0.15f, 0.9f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnEShield(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.0f, 0.f };
            fx.texturePath = kPathEShieldTex;
            fx.fWidth = 2.2f;
            fx.fHeight = 2.2f;
            fx.bBillboard = true;
            fx.fLifetime = fDuration;
            fx.vColor = { 1.2f, 0.5f, 0.2f, 0.85f };
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.fFadeOut = fDuration * 0.3f;
            CFxSystem::Spawn(world, fx);
        }

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.1f, 0.f };
            fx.texturePath = kPathEGlowTex;
            fx.fWidth = 1.5f;
            fx.fHeight = 1.5f;
            fx.bBillboard = true;
            fx.fLifetime = fDuration;
            fx.vColor = { 1.3f, 0.7f, 0.3f, 0.7f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fDuration * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnRTibbersSummon(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        {
            FxBillboardComponent fx{};
            fx.vWorldPos = groundPos;
            fx.texturePath = kPathRSummonTex;
            fx.fWidth = 5.0f;
            fx.fHeight = 5.0f;
            fx.bBillboard = false;
            fx.fLifetime = fLifetime;
            fx.vColor = { 1.4f, 0.6f, 0.2f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.6f;
            CFxSystem::Spawn(world, fx);
        }

        {
            FxBillboardComponent fx{};
            fx.vWorldPos = { groundPos.x, groundPos.y + 1.5f, groundPos.z };
            fx.texturePath = kPathRSummonTex;
            fx.fWidth = 2.0f;
            fx.fHeight = 4.0f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime * 0.7f;
            fx.vColor = { 1.4f, 0.5f, 0.15f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnStunCharge(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 2.4f, 0.f };
        fx.texturePath = kPathStunChargeTex;
        fx.fWidth = 0.8f;
        fx.fHeight = 0.8f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.4f, 0.4f, 0.1f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }
}
