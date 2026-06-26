#include "GameObject/Champion/Fiora/Fiora_FxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    constexpr const wchar_t* kPathBaHitSparkTex =
        L"Texture/Character/Fiora/particles/fiora_base_e_hit_spark_yellow.png";
    constexpr const wchar_t* kPathQSlashTex =
        L"Texture/Character/Fiora/particles/fiora_base_q_slash.png";
    constexpr const wchar_t* kPathQGlowTex =
        L"Texture/Character/Fiora/particles/fiora_base_q_swordglow.png";
    constexpr const wchar_t* kPathWParryTex =
        L"Texture/Character/Fiora/particles/fiora_base_w_block_glow.png";
    constexpr const wchar_t* kPathWFlashTex =
        L"Texture/Character/Fiora/particles/fiora_base_w_block_flash.png";
    constexpr const wchar_t* kPathEBuffTex =
        L"Texture/Character/Fiora/particles/fiora_base_e_buff_mult_yellow.png";
    constexpr const wchar_t* kPathRMarkTex =
        L"Texture/Character/Fiora/particles/fiora_base_r_crest_glow.png";
    constexpr const wchar_t* kPathRHealTex =
        L"Texture/Character/Fiora/particles/fiora_base_r_healzone.png";
}

namespace Fiora::Fx
{
    void SpawnBAHitSpark(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathBaHitSparkTex;
        fx.fWidth = 1.4f; fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.1f, 0.95f, 0.45f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.45f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnQSlash(CWorld& world, EntityID owner, const Vec3&, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.0f, 0.f };
            fx.texturePath = kPathQSlashTex;
            fx.fWidth = 2.4f; fx.fHeight = 1.6f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime;
            fx.vColor = { 0.95f, 0.85f, 0.55f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.45f;
            CFxSystem::Spawn(world, fx);
        }
        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.1f, 0.f };
            fx.texturePath = kPathQGlowTex;
            fx.fWidth = 1.4f; fx.fHeight = 1.4f;
            fx.bBillboard = true;
            fx.fLifetime = fLifetime * 0.6f;
            fx.vColor = { 1.0f, 0.95f, 0.7f, 1.f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fLifetime * 0.3f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnWParryActive(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathWParryTex;
        fx.fWidth = 2.0f; fx.fHeight = 2.0f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.1f, 0.9f, 0.4f, 0.85f };
        fx.blendMode = eBlendPreset::AlphaBlend;
        fx.fFadeOut = fDuration * 0.35f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnWBlockFlash(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.2f, 0.f };
        fx.texturePath = kPathWFlashTex;
        fx.fWidth = 2.6f; fx.fHeight = 2.6f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.4f, 1.2f, 0.6f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.55f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnEBladeworkBuff(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.3f, 0.f };
        fx.texturePath = kPathEBuffTex;
        fx.fWidth = 1.6f; fx.fHeight = 1.6f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.0f, 0.85f, 0.3f, 0.9f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fDuration * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRMark(CWorld& world, EntityID target, f32_t fDuration)
    {
        if (target == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 2.5f, 0.f };
        fx.texturePath = kPathRMarkTex;
        fx.fWidth = 1.0f; fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fDuration;
        fx.vColor = { 1.2f, 0.9f, 0.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fDuration * 0.3f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRHealZone(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;
        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 0.05f, 0.f };
        fx.texturePath = kPathRHealTex;
        fx.fWidth = 6.0f; fx.fHeight = 6.0f;
        fx.bBillboard = false;
        fx.fYaw = 0.f;
        fx.fLifetime = fDuration;
        fx.vColor = { 0.85f, 1.0f, 0.55f, 0.7f };
        fx.blendMode = eBlendPreset::AlphaBlend;
        fx.fFadeOut = fDuration * 0.5f;
        CFxSystem::Spawn(world, fx);
    }
}
