#include "GameObject/Champion/Zed/ZedFxPresets.h"

#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "GameObject/ChampionDef.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxSystem.h"
#include "Renderer/ModelRenderer.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr const wchar_t* kPathQRazorTex =
        L"Texture/Character/Zed/particles/zed_shuriken_tx.png";
    constexpr const wchar_t* kPathWShadowTex =
        L"Texture/Character/Zed/particles/zed_base_w_team_indicator_blue.png";
    constexpr const wchar_t* kPathWShadowWispsTex =
        L"Texture/Character/Zed/particles/zed_shadowwisps.png";
    constexpr const wchar_t* kPathESlashTex =
        L"Texture/Character/Zed/particles/zed_e_slash.png";
    constexpr const wchar_t* kPathRMarkTex =
        L"Texture/Character/Zed/particles/zed_r_marker.png";
    Vec4 MakeShadowCloneOverrideColor()
    {
        return Vec4{ 0.015f, 0.017f, 0.024f, 1.f };
    }

    struct ShadowCloneVisual
    {
        EntityID owner = NULL_ENTITY;
        u8_t sourceSlot = 0u;
        std::unique_ptr<ModelRenderer> pRenderer{};
        f32_t fRemainingSec = 0.f;
        bool_t bSwapAvailable = true;
    };

    std::unordered_map<EntityID, ShadowCloneVisual> s_shadowClones;
    std::unordered_map<u64_t, EntityID> s_shadowCloneByOwnerAndSlot;

    u64_t BuildShadowCloneKey(EntityID owner, u8_t sourceSlot)
    {
        return (static_cast<u64_t>(owner) << 8u) | static_cast<u64_t>(sourceSlot);
    }

    Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return Vec3{};
    }

    void DestroyTrackedShadowClone(CWorld& world, EntityID shadowEntity)
    {
        const auto it = s_shadowClones.find(shadowEntity);
        if (it != s_shadowClones.end())
        {
            if (it->second.owner != NULL_ENTITY)
            {
                s_shadowCloneByOwnerAndSlot.erase(
                    BuildShadowCloneKey(it->second.owner, it->second.sourceSlot));
            }
            s_shadowClones.erase(it);
        }

        if (shadowEntity != NULL_ENTITY && world.IsAlive(shadowEntity))
            world.DestroyEntity(shadowEntity);
    }
}

void ZedFx::SpawnQRazor(CWorld& world, EntityID owner, const Vec3& dir, f32_t fLifetime)
{
    if (owner == NULL_ENTITY)
        return;

    const Vec3 ownerPos = ResolveEntityPosition(world, owner);
    const Vec3 forward = WintersMath::NormalizeXZ(dir);

    FxBillboardComponent fx{};
    fx.vWorldPos = {
        ownerPos.x + forward.x * 0.8f,
        ownerPos.y + 1.25f,
        ownerPos.z + forward.z * 0.8f
    };
    fx.vVelocity = { forward.x * 22.f, 0.f, forward.z * 22.f };
    fx.texturePath = kPathQRazorTex;
    fx.fWidth = 1.2f;
    fx.fHeight = 1.2f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.f, 1.f, 1.f, 1.f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.4f;
    fx.bBlockableByWindWall = true;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnWShadow(CWorld& world, EntityID owner, const Vec3& groundPos, f32_t fDuration)
{
    if (owner == NULL_ENTITY)
        return;

    const Vec3 ownerPos = ResolveEntityPosition(world, owner);
    const bool_t bHasGroundPos =
        (std::fabs(groundPos.x) + std::fabs(groundPos.y) + std::fabs(groundPos.z)) > 0.001f;
    const Vec3 spawnPos = bHasGroundPos ? groundPos : ownerPos;

    FxBillboardComponent ground{};
    ground.vWorldPos = { spawnPos.x, spawnPos.y + 0.06f, spawnPos.z };
    ground.texturePath = kPathWShadowTex;
    ground.renderType = eFxRenderType::GroundDecal;
    ground.fWidth = 3.8f;
    ground.fHeight = 3.8f;
    ground.bBillboard = false;
    ground.fLifetime = fDuration;
    ground.vColor = { 1.f, 1.f, 1.f, 0.85f };
    ground.blendMode = eBlendPreset::AlphaBlend;
    ground.fFadeIn = 0.05f;
    ground.fFadeOut = 0.25f;
    CFxSystem::Spawn(world, ground);

    FxBillboardComponent wisps{};
    wisps.vWorldPos = { spawnPos.x, spawnPos.y + 1.2f, spawnPos.z };
    wisps.texturePath = kPathWShadowWispsTex;
    wisps.fWidth = 2.0f;
    wisps.fHeight = 2.0f;
    wisps.bBillboard = true;
    wisps.fLifetime = fDuration;
    wisps.vColor = { 0.55f, 0.65f, 1.f, 0.95f };
    wisps.blendMode = eBlendPreset::Additive;
    wisps.fFadeIn = 0.08f;
    wisps.fFadeOut = 0.35f;
    CFxSystem::Spawn(world, wisps);
}

void ZedFx::SpawnESlash(CWorld& world, EntityID owner, f32_t fLifetime)
{
    if (owner == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = owner;
    fx.vAttachOffset = { 0.f, 1.0f, 0.f };
    fx.texturePath = kPathESlashTex;
    fx.fWidth = 2.5f;
    fx.fHeight = 2.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 1.0f, 0.4f, 0.4f, 1.0f };
    fx.blendMode = eBlendPreset::AlphaBlend;
    fx.fFadeOut = fLifetime * 0.3f;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnRMark(CWorld& world, EntityID target, f32_t fLifetime)
{
    if (target == NULL_ENTITY)
        return;

    FxBillboardComponent fx{};
    fx.attachTo = target;
    fx.vAttachOffset = { 0.f, 2.5f, 0.f };
    fx.texturePath = kPathRMarkTex;
    fx.fWidth = 1.5f;
    fx.fHeight = 1.5f;
    fx.bBillboard = true;
    fx.fLifetime = fLifetime;
    fx.vColor = { 0.9f, 0.2f, 0.3f, 1.0f };
    fx.blendMode = eBlendPreset::Additive;
    fx.fFadeOut = fLifetime * 0.5f;
    CFxSystem::Spawn(world, fx);
}

void ZedFx::SpawnShadowCloneModel(
    CWorld& world,
    EntityID owner,
    u8_t sourceSlot,
    const Vec3& groundPos,
    const Vec3& direction,
    f32_t fDuration)
{
    if (owner == NULL_ENTITY)
        return;

    const u64_t cloneKey = BuildShadowCloneKey(owner, sourceSlot);
    const auto oldIt = s_shadowCloneByOwnerAndSlot.find(cloneKey);
    if (oldIt != s_shadowCloneByOwnerAndSlot.end())
        DestroyTrackedShadowClone(world, oldIt->second);

    const ChampionDef* pDef = FindChampionDef(eChampion::ZED);
    if (!pDef || !pDef->fbxPath)
        return;

    std::unique_ptr<ModelRenderer> pRenderer = std::make_unique<ModelRenderer>();
    if (!pRenderer->Initialize(pDef->fbxPath, pDef->shaderPath))
        return;

    if (pDef->defaultTexturePath)
        pRenderer->LoadTextureForAllMeshes(pDef->defaultTexturePath);

    for (u32_t i = 0; i < kChampionTextureSlotMax; ++i)
    {
        if (pDef->texturePath[i])
            pRenderer->LoadMeshTexture(i, pDef->texturePath[i]);
    }

    if (pDef->animPrefix && pDef->idleAnimKey)
        pRenderer->PlayAnimationByName(
            std::string(pDef->animPrefix) + pDef->idleAnimKey,
            true);

    pRenderer->SetMaterialOverrideColor(MakeShadowCloneOverrideColor(), true);

    const EntityID shadowEntity = world.CreateEntity();
    TransformComponent transform{};
    transform.SetPosition(groundPos);
    transform.SetScale(pDef->spawnScale);

    const Vec3 forward = WintersMath::NormalizeXZ(direction, Vec3{ 0.f, 0.f, 1.f }, 0.0001f);
    transform.SetRotation(Vec3{
        0.f,
        ResolveChampionVisualYawNear(eChampion::ZED, forward, 0.f),
        0.f });
    world.AddComponent<TransformComponent>(shadowEntity, transform);

    RenderComponent render{};
    render.pRenderer = pRenderer.get();
    render.bVisible = true;
    render.bAnimated = true;
    render.bSceneManaged = false;
    world.AddComponent<RenderComponent>(shadowEntity, render);

    ShadowCloneVisual visual{};
    visual.owner = owner;
    visual.sourceSlot = sourceSlot;
    visual.pRenderer = std::move(pRenderer);
    visual.fRemainingSec = fDuration;
    s_shadowClones[shadowEntity] = std::move(visual);
    s_shadowCloneByOwnerAndSlot[cloneKey] = shadowEntity;
}

bool_t ZedFx::CanSwapToShadowClone(CWorld& world, EntityID owner, u8_t sourceSlot)
{
    const auto ownerIt = s_shadowCloneByOwnerAndSlot.find(
        BuildShadowCloneKey(owner, sourceSlot));
    if (ownerIt == s_shadowCloneByOwnerAndSlot.end())
        return false;

    const EntityID shadowEntity = ownerIt->second;
    const auto shadowIt = s_shadowClones.find(shadowEntity);
    return shadowEntity != NULL_ENTITY &&
        world.IsAlive(shadowEntity) &&
        world.HasComponent<TransformComponent>(shadowEntity) &&
        shadowIt != s_shadowClones.end() &&
        shadowIt->second.bSwapAvailable;
}

bool_t ZedFx::MoveShadowCloneModel(
    CWorld& world,
    EntityID owner,
    u8_t sourceSlot,
    const Vec3& groundPos,
    const Vec3& direction)
{
    const auto it = s_shadowCloneByOwnerAndSlot.find(
        BuildShadowCloneKey(owner, sourceSlot));
    if (it == s_shadowCloneByOwnerAndSlot.end())
        return false;

    const EntityID shadowEntity = it->second;
    if (shadowEntity == NULL_ENTITY ||
        !world.IsAlive(shadowEntity) ||
        !world.HasComponent<TransformComponent>(shadowEntity))
    {
        return false;
    }

    TransformComponent& transform = world.GetComponent<TransformComponent>(shadowEntity);
    transform.SetPosition(groundPos);
    const Vec3 forward = WintersMath::NormalizeXZ(direction, Vec3{ 0.f, 0.f, 1.f }, 0.0001f);
    transform.SetRotation(Vec3{
        transform.GetRotation().x,
        ResolveChampionVisualYawNear(eChampion::ZED, forward, transform.GetRotation().y),
        transform.GetRotation().z });

    const auto shadowIt = s_shadowClones.find(shadowEntity);
    if (shadowIt != s_shadowClones.end())
        shadowIt->second.bSwapAvailable = false;
    return true;
}

void ZedFx::TickShadowCloneModels(CWorld& world, f32_t fDeltaTime)
{
    std::vector<EntityID> expired;
    expired.reserve(s_shadowClones.size());

    for (auto& pair : s_shadowClones)
    {
        const EntityID entity = pair.first;
        ShadowCloneVisual& visual = pair.second;
        if (!world.IsAlive(entity) || !world.IsAlive(visual.owner))
        {
            expired.push_back(entity);
            continue;
        }

        visual.fRemainingSec -= fDeltaTime;
        if (visual.fRemainingSec <= 0.f)
            expired.push_back(entity);
    }

    for (EntityID entity : expired)
        DestroyTrackedShadowClone(world, entity);
}
