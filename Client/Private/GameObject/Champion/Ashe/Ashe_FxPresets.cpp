#include "GameObject/Champion/Ashe/Ashe_FxPresets.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

namespace
{
    constexpr const wchar_t* kPathArrowGlowTex =
        L"Texture/Character/Ashe/particles/ashe_base_ba_glow.png";
    constexpr const wchar_t* kPathArrowTrailTex =
        L"Texture/Character/Ashe/particles/ashe_base_ba_mist_trail.png";
    constexpr const wchar_t* kPathFrostHitTex =
        L"Texture/Character/Ashe/particles/ashe_base_ba_color-rampdownfrost.png";
    constexpr const wchar_t* kPathQBuffTex =
        L"Texture/Character/Ashe/particles/ashe_base_q_buf.png";
    constexpr const wchar_t* kPathQReadyTex =
        L"Texture/Character/Ashe/particles/ashe_base_q_ready_brightsparks.png";
    constexpr const wchar_t* kPathQDiffuseTex =
        L"Texture/Character/Ashe/particles/ashe_base_q_buff_diffuse.png";
    constexpr const wchar_t* kPathWArrowTex =
        L"Texture/Character/Ashe/particles/ashe_base_aa_arrowtext.png";
    constexpr const wchar_t* kPathWMuzzleTex =
        L"Texture/Character/Ashe/particles/ashe_base_q_bow_sparks.png";
    constexpr const wchar_t* kPathEHawkTex =
        L"Texture/Character/Ashe/particles/ashe_base_e_textureowl.png";
    constexpr const wchar_t* kPathRChargeTex =
        L"Texture/Character/Ashe/particles/ashe_base_q_ready_brightsparks_star.png";
    constexpr const wchar_t* kPathRArrowTex =
        L"Texture/Character/Ashe/particles/ashe_base_q_mis_star.png";
    constexpr const wchar_t* kPathRStunTex =
        L"Texture/Character/Ashe/particles/ashe_base_ba_ashe_teal_sparkle.png";

    constexpr const char* kFbxBAArrow =
        "Texture/Character/Ashe/particles/fbx/ashe_base_aa_arrow.fbx";
    constexpr const char* kFbxQBuffSphere =
        "Texture/Character/Ashe/particles/fbx/ashe_base_q_buf_attack_sphere.fbx";
    constexpr const char* kFbxWVolleySwirl =
        "Texture/Character/Ashe/particles/fbx/ashe_base_w_swirlmesh03.fbx";
    constexpr const char* kFbxRArrow =
        "Texture/Character/Ashe/particles/fbx/ashe_base_r_arrow.fbx";

    constexpr f32_t kArrowMeshYawOffset = -1.57079632679f;
    const Vec3 kBAArrowMeshScale{ 0.021f, 0.021f, 0.021f };

    void SpawnLineProjectileVisual(CWorld& world, EntityID owner,
        const wchar_t* path, f32_t fLifetime,
        f32_t fWidth, f32_t fHeight, const Vec4& color)
    {
        if (owner == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = path;
        fx.fWidth = fWidth;
        fx.fHeight = fHeight;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = color;
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnMeshVisual(
        CWorld& world,
        Engine::CFxStaticMeshRenderer* pRenderer,
        const char* modelPath,
        const wchar_t* texturePath,
        const Vec3& pos,
        const Vec3& dir,
        const Vec3& scale,
        f32_t lifetime,
        const Vec4& color,
        f32_t speed = 0.f,
        f32_t spinSpeed = 0.f,
        f32_t yawOffset = 0.f)
    {
        if (!pRenderer || !modelPath)
            return;

        const Vec3 n = WintersMath::NormalizeXZ(dir);
        FxMeshComponent mesh{};
        mesh.vWorldPos = pos;
        mesh.vVelocity = { n.x * speed, 0.f, n.z * speed };
        mesh.vRotation = { 0.f, WintersMath::YawFromDirectionXZ(n) + yawOffset, 0.f };
        mesh.vScale = scale;
        mesh.modelPath = modelPath;
        mesh.texturePath = texturePath;
        mesh.vColor = color;
        mesh.blendMode = eBlendPreset::Additive;
        mesh.fLifetime = lifetime;
        mesh.fFadeOut = lifetime * 0.4f;
        mesh.fWorldYawSpinSpeed = spinSpeed;
        mesh.bDepthWrite = false;
        mesh.bBlockableByWindWall = true;
        CFxMeshSystem::Spawn(world, pRenderer, mesh);
    }
}

namespace Ashe::Fx
{
    void SpawnBAArrow(CWorld& world, EntityID owner, const Vec3&, const Vec3&, f32_t fLifetime)
    {
        SpawnLineProjectileVisual(world, owner, kPathArrowGlowTex,
            fLifetime, 0.8f, 0.8f, { 0.6f, 0.95f, 1.2f, 1.f });
        SpawnLineProjectileVisual(world, owner, kPathArrowTrailTex,
            fLifetime * 0.5f, 1.4f, 0.6f, { 0.7f, 1.0f, 1.3f, 0.85f });
    }

    void SpawnBAArrowMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        const Vec3 n = WintersMath::NormalizeXZ(dir);
        SpawnMeshVisual(
            world,
            pRenderer,
            kFbxBAArrow,
            kPathArrowGlowTex,
            { origin.x + n.x * 0.8f, origin.y + 1.0f, origin.z + n.z * 0.8f },
            n,
            kBAArrowMeshScale,
            fLifetime,
            { 0.65f, 1.1f, 1.35f, 0.9f },
            18.f,
            0.f,
            kArrowMeshYawOffset);
    }

    void SpawnFrostHit(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.0f, 0.f };
        fx.texturePath = kPathFrostHitTex;
        fx.fWidth = 1.0f;
        fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.5f, 0.85f, 1.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnQBuffActive(CWorld& world, EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY) return;

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 1.2f, 0.f };
            fx.texturePath = kPathQBuffTex;
            fx.fWidth = 1.8f;
            fx.fHeight = 1.8f;
            fx.bBillboard = true;
            fx.fLifetime = fDuration;
            fx.vColor = { 0.9f, 1.1f, 1.4f, 0.85f };
            fx.blendMode = eBlendPreset::Additive;
            fx.fFadeOut = fDuration * 0.3f;
            CFxSystem::Spawn(world, fx);
        }

        {
            FxBillboardComponent fx{};
            fx.attachTo = owner;
            fx.vAttachOffset = { 0.f, 0.05f, 0.f };
            fx.texturePath = kPathQDiffuseTex;
            fx.fWidth = 2.0f;
            fx.fHeight = 2.0f;
            fx.bBillboard = false;
            fx.fLifetime = fDuration;
            fx.vColor = { 0.6f, 0.9f, 1.2f, 0.6f };
            fx.blendMode = eBlendPreset::AlphaBlend;
            fx.fFadeOut = fDuration * 0.4f;
            CFxSystem::Spawn(world, fx);
        }
    }

    void SpawnQBuffMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        EntityID owner, f32_t fDuration)
    {
        if (owner == NULL_ENTITY)
            return;

        if (pRenderer)
        {
            FxMeshComponent mesh{};
            mesh.attachTo = owner;
            mesh.vAttachOffset = { 0.f, 1.2f, 0.f };
            mesh.vScale = { 0.018f, 0.018f, 0.018f };
            mesh.modelPath = kFbxQBuffSphere;
            mesh.texturePath = kPathQBuffTex;
            mesh.vColor = { 0.65f, 1.15f, 1.45f, 0.75f };
            mesh.blendMode = eBlendPreset::Additive;
            mesh.fLifetime = fDuration;
            mesh.fFadeOut = fDuration * 0.3f;
            mesh.fWorldYawSpinSpeed = 4.f;
            mesh.bDepthWrite = false;
            CFxMeshSystem::Spawn(world, pRenderer, mesh);
        }
    }

    void SpawnQReadySparks(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.5f, 0.f };
        fx.texturePath = kPathQReadyTex;
        fx.fWidth = 1.4f;
        fx.fHeight = 1.4f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.2f, 1.3f, 1.4f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnWVolleyArrow(CWorld& world, EntityID owner, const Vec3&, const Vec3&, f32_t fLifetime)
    {
        SpawnLineProjectileVisual(world, owner, kPathWArrowTex,
            fLifetime, 1.0f, 0.6f, { 0.8f, 1.0f, 1.3f, 1.f });
    }

    void SpawnWVolleyMuzzle(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.4f, 0.f };
        fx.texturePath = kPathWMuzzleTex;
        fx.fWidth = 2.0f;
        fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.0f, 1.2f, 1.4f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnWVolleyMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        const Vec3 n = WintersMath::NormalizeXZ(dir);
        SpawnMeshVisual(
            world,
            pRenderer,
            kFbxWVolleySwirl,
            kPathWMuzzleTex,
            { origin.x + n.x * 1.0f, origin.y + 1.2f, origin.z + n.z * 1.0f },
            n,
            { 0.018f, 0.018f, 0.018f },
            fLifetime,
            { 0.75f, 1.15f, 1.45f, 0.85f },
            0.f,
            8.f);
    }

    void SpawnEHawkshot(CWorld& world, const Vec3& start, const Vec3&, f32_t fLifetime)
    {
        FxBillboardComponent fx{};
        fx.attachTo = NULL_ENTITY;
        fx.vWorldPos = { start.x, start.y + 3.0f, start.z };
        fx.vAttachOffset = { 0.f, 0.f, 0.f };
        fx.texturePath = kPathEHawkTex;
        fx.fWidth = 1.4f;
        fx.fHeight = 1.0f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.8f, 1.1f, 1.3f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRCrystalCharge(CWorld& world, EntityID owner, f32_t fLifetime)
    {
        if (owner == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = owner;
        fx.vAttachOffset = { 0.f, 1.5f, 0.f };
        fx.texturePath = kPathRChargeTex;
        fx.fWidth = 1.6f;
        fx.fHeight = 1.6f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 1.3f, 1.4f, 1.5f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.5f;
        CFxSystem::Spawn(world, fx);
    }

    void SpawnRCrystalArrow(CWorld& world, EntityID owner, const Vec3&, const Vec3&, f32_t fLifetime)
    {
        SpawnLineProjectileVisual(world, owner, kPathRArrowTex,
            fLifetime, 1.6f, 0.8f, { 1.0f, 1.3f, 1.5f, 1.f });
    }

    void SpawnRCrystalArrowMesh(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
        const Vec3& origin, const Vec3& dir, f32_t fLifetime)
    {
        const Vec3 n = WintersMath::NormalizeXZ(dir);
        SpawnMeshVisual(
            world,
            pRenderer,
            kFbxRArrow,
            kPathRArrowTex,
            { origin.x + n.x * 1.2f, origin.y + 1.1f, origin.z + n.z * 1.2f },
            n,
            { 0.018f, 0.018f, 0.018f },
            fLifetime,
            { 0.75f, 1.25f, 1.55f, 0.95f },
            22.f,
            0.f,
            kArrowMeshYawOffset);
    }

    void SpawnRStunFrost(CWorld& world, EntityID target, f32_t fLifetime)
    {
        if (target == NULL_ENTITY) return;

        FxBillboardComponent fx{};
        fx.attachTo = target;
        fx.vAttachOffset = { 0.f, 1.5f, 0.f };
        fx.texturePath = kPathRStunTex;
        fx.fWidth = 1.8f;
        fx.fHeight = 1.8f;
        fx.bBillboard = true;
        fx.fLifetime = fLifetime;
        fx.vColor = { 0.7f, 1.2f, 1.5f, 1.f };
        fx.blendMode = eBlendPreset::Additive;
        fx.fFadeOut = fLifetime * 0.4f;
        CFxSystem::Spawn(world, fx);
    }
}
