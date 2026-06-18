#include "ECS/Systems/YoneSoulSpawnSystem.h"

#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/MeshGroupVisibilityComponent.h"
#include "ECS/Components/RenderComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/Champion/Yone/Yone_Components.h"
#include "GameObject/Champion/Yone/Yone_MeshGroups.h"
#include "Renderer/ModelRenderer.h"

#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "GameObject/FX/FxBeamSystem.h"
#include "GameObject/FX/FxSystem.h"

#include <Windows.h>
#include <utility>
#include <vector>

namespace
{
    constexpr const wchar_t* kYoneSoulOuterGlowTexture =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/aura_self.png";
    constexpr const wchar_t* kYoneSoulEyeTrailTexture =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/yone_base_e_blue_flame.png";

    Vec4 MakeYoneSoulBodyOverrideColor()
    {
        return Vec4{ 0.16f, 0.012f, 0.016f, 1.f };
    }

    Vec4 MakeYoneSoulOuterGlowColor()
    {
        return Vec4{ 1.35f, 0.01f, 0.07f, 0.38f };
    }

    EntityID SpawnYoneSoulOuterGlow(CWorld& world, EntityID soul, f32_t fDurationSec)
    {
        if (soul == NULL_ENTITY)
            return NULL_ENTITY;

        const f32_t fLifetime = fDurationSec > 0.1f ? fDurationSec : 0.1f;

        FxBillboardComponent fx{};
        fx.attachTo = soul;
        fx.vAttachOffset = { 0.f, 1.15f, 0.f };
        fx.texturePath = kYoneSoulOuterGlowTexture;
        fx.fWidth = 2.35f;
        fx.fHeight = 3.15f;
        fx.bBillboard = true;
        fx.renderType = eFxRenderType::Billboard;
        fx.fLifetime = fLifetime;
        fx.vColor = MakeYoneSoulOuterGlowColor();
        fx.blendMode = eBlendPreset::Additive;
        fx.depthMode = eFxDepthMode::DepthTestWriteOff;
        fx.fFadeIn = 0.12f;
        fx.fFadeOut = 0.35f;
        fx.fAlphaClip = 0.02f;
        fx.fUvScrollV = -0.08f;
        return CFxSystem::Spawn(world, fx);
    }

    EntityID SpawnYoneSoulEyeTrail(CWorld& world,
        EntityID owner,
        const char* pszBoneName,
        f32_t fDurationSec)
    {
        if (owner == NULL_ENTITY || !pszBoneName || pszBoneName[0] == 0)
            return NULL_ENTITY;

        const f32_t fLifetime = fDurationSec > 0.1f ? fDurationSec : 0.1f;
        Vec3 vFallbackPos{};
        if (world.HasComponent<TransformComponent>(owner))
        {
            const Vec3& p = world.GetComponent<TransformComponent>(owner).GetPosition();
            vFallbackPos = { p.x, p.y + 1.55f, p.z };
        }

        FxRibbonComponent ribbon{};
        ribbon.attachTo = owner;
        ribbon.vStartOffset = { 0.f, 1.55f, 0.f };
        ribbon.vEndOffset = { 0.f, 1.55f, -0.35f };
        ribbon.anchor.eAnchorType = eFxAnchorType::Bone;
        ribbon.anchor.eFallback = eFxAnchorFallback::Entity;
        ribbon.anchor.strAnchorName = pszBoneName;
        ribbon.anchor.vAnchorOffset = { 0.f, 0.f, 0.f };
        ribbon.SetTexturePath(kYoneSoulEyeTrailTexture);
        ribbon.fWidth = 0.18f;
        ribbon.fLifetime = fLifetime;
        ribbon.fFadeIn = 0.05f;
        ribbon.fFadeOut = 0.35f;
        ribbon.fUvScrollV = -0.35f;
        ribbon.fAlphaClip = 0.02f;
        ribbon.vColor = { 0.26f, 0.72f, 2.1f, 0.68f };
        ribbon.blendMode = eBlendPreset::Additive;
        ribbon.depthMode = eFxDepthMode::DepthTestWriteOff;
        ribbon.bHistoryTrail = true;
        ribbon.fTrailSampleInterval = 0.018f;
        ribbon.fTrailHeadWidthScale = 1.0f;
        ribbon.fTrailTailWidthScale = 0.12f;
        ribbon.fTrailHeadAlphaScale = 1.0f;
        ribbon.fTrailTailAlphaScale = 0.05f;

        constexpr u32_t kPointCount = 14u;
        for (u32_t i = 0; i < kPointCount; ++i)
        {
            ribbon.SetPoint(i, vFallbackPos);
            ribbon.pointAges[i] = static_cast<f32_t>(i) * 0.018f;
        }

        return CFxBeamSystem::Spawn(world, ribbon);
    }
}

std::unique_ptr<CYoneSoulSpawnSystem> CYoneSoulSpawnSystem::Create()
{
    return std::make_unique<CYoneSoulSpawnSystem>();
}

void CYoneSoulSpawnSystem::Execute(CWorld& world, float fTimeDelta)
{
    std::vector<std::pair<EntityID, YoneSoulRequestComponent>> requests;
    world.ForEach<YoneSoulRequestComponent>(
        [&](EntityID owner, YoneSoulRequestComponent& req)
        {
            requests.emplace_back(owner, req);
        });

    for (const auto& item : requests)
    {
        const EntityID owner = item.first;
        const auto& req = item.second;

        if (!world.IsAlive(owner))
            continue;

        if (req.action == eYoneSoulRequestAction::Spawn)
            SpawnSoul(world, owner);
        else
            DespawnSoul(world, owner, true);

        if (world.HasComponent<YoneSoulRequestComponent>(owner))
            world.RemoveComponent<YoneSoulRequestComponent>(owner);
    }

    std::vector<EntityID> expiredOwners;
    world.ForEach<YoneStateComponent>(
        [&](EntityID owner, YoneStateComponent& state)
        {
            if (!state.bEActive)
                return;

            state.fETimer -= static_cast<f32_t>(fTimeDelta);
            if (state.fETimer <= 0.f)
                expiredOwners.push_back(owner);
        });

    for (EntityID owner : expiredOwners)
        DespawnSoul(world, owner, true);
}

void CYoneSoulSpawnSystem::SpawnSoul(CWorld& world, EntityID owner)
{
    if (!world.HasComponent<YoneStateComponent>(owner))
        return;
    if (!world.HasComponent<TransformComponent>(owner))
        return;

    auto& state = world.GetComponent<YoneStateComponent>(owner);
    if (state.soulEntity != NULL_ENTITY && world.IsAlive(state.soulEntity))
        return;

    Vec3 spawnPos = world.GetComponent<TransformComponent>(owner).GetPosition();
    f32_t fSoulDurationSec = state.fEDurationSec;
    if (world.HasComponent<YoneSoulRequestComponent>(owner))
    {
        const auto& req = world.GetComponent<YoneSoulRequestComponent>(owner);
        spawnPos = req.vSpawnPosition;
        if (req.fDurationSec > 0.f)
            fSoulDurationSec = req.fDurationSec;
    }

    auto pRenderer = std::make_unique<ModelRenderer>();
	if (!pRenderer->Initialize("Client/Bin/Resource/Texture/Character/Yone/yone.fbx",
		L"Shaders/Mesh3D.hlsl"))
	{
		return;
	}

    pRenderer->LoadTextureForAllMeshes(
        L"Client/Bin/Resource/Texture/Character/Yone/yone_base_tx_cm.png");
    pRenderer->LoadMeshTexture(1,
        L"Client/Bin/Resource/Texture/Character/Yone/yone_base_swords_tx_cm.png");
    pRenderer->LoadMeshTexture(2,
        L"Client/Bin/Resource/Texture/Character/Yone/yone_base_swords_tx_cm.png");
    pRenderer->LoadMeshTexture(3,
        L"Client/Bin/Resource/Texture/Character/Yone/yone_base_props_tx_cm.png");
    pRenderer->PlayAnimationByName("yone_idle1", true);
    pRenderer->SetMaterialOverrideColor(MakeYoneSoulBodyOverrideColor(), true);

    const EntityID soul = world.CreateEntity();

    TransformComponent tf = world.GetComponent<TransformComponent>(owner);
    tf.SetPosition(spawnPos);
    world.AddComponent<TransformComponent>(soul, tf);

    RenderComponent rc{};
    rc.pRenderer = pRenderer.get();
    rc.bVisible = true;
    rc.bAnimated = true;
    rc.bSceneManaged = false;
    world.AddComponent<RenderComponent>(soul, rc);

    ChampionComponent cc{};
    if (world.HasComponent<ChampionComponent>(owner))
        cc = world.GetComponent<ChampionComponent>(owner);
    cc.id = eChampion::YONE;
    world.AddComponent<ChampionComponent>(soul, cc);

    HealthComponent hp{};
    if (world.HasComponent<HealthComponent>(owner))
        hp = world.GetComponent<HealthComponent>(owner);
    world.AddComponent<HealthComponent>(soul, hp);

    MeshGroupVisibilityComponent visibility{};
    visibility.mask = Yone::MeshGroups::MaskBaseDefault();
    visibility.bEnabled = true;
    world.AddComponent<MeshGroupVisibilityComponent>(soul, visibility);

    state.soulEntity = soul;
    state.soulGlowEntity = SpawnYoneSoulOuterGlow(world, soul, fSoulDurationSec);
    state.soulEyeTrailLeftEntity =
        SpawnYoneSoulEyeTrail(world, owner, "L_Eye", fSoulDurationSec);
    state.soulEyeTrailRightEntity =
        SpawnYoneSoulEyeTrail(world, owner, "R_Eye", fSoulDurationSec);
	state.bEActive = true;
	m_mapSoulRenderers[soul] = std::move(pRenderer);
}

void CYoneSoulSpawnSystem::DespawnSoul(CWorld& world, EntityID owner, bool_t bReturnBody)
{
    (void)bReturnBody;

    if (!world.HasComponent<YoneStateComponent>(owner))
        return;

    auto& state = world.GetComponent<YoneStateComponent>(owner);
    const EntityID soul = state.soulEntity;

    if (state.soulGlowEntity != NULL_ENTITY && world.IsAlive(state.soulGlowEntity))
        world.DestroyEntity(state.soulGlowEntity);
    state.soulGlowEntity = NULL_ENTITY;

    if (state.soulEyeTrailLeftEntity != NULL_ENTITY &&
        world.IsAlive(state.soulEyeTrailLeftEntity))
    {
        world.DestroyEntity(state.soulEyeTrailLeftEntity);
    }
    state.soulEyeTrailLeftEntity = NULL_ENTITY;

    if (state.soulEyeTrailRightEntity != NULL_ENTITY &&
        world.IsAlive(state.soulEyeTrailRightEntity))
    {
        world.DestroyEntity(state.soulEyeTrailRightEntity);
    }
    state.soulEyeTrailRightEntity = NULL_ENTITY;

    if (soul != NULL_ENTITY)
    {
        if (world.IsAlive(soul))
            world.DestroyEntity(soul);
        m_mapSoulRenderers.erase(soul);
    }

    if (world.HasComponent<MeshGroupVisibilityComponent>(owner))
    {
        auto& visibility = world.GetComponent<MeshGroupVisibilityComponent>(owner);
        visibility.mask = Yone::MeshGroups::MaskBaseDefault();
        visibility.bEnabled = true;
    }

    state.soulEntity = NULL_ENTITY;
	state.bEActive = false;
	state.fETimer = 0.f;
}
