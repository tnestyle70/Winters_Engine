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
#include "GameObject/FX/FxSystem.h"

#include <Windows.h>
#include <utility>
#include <vector>

namespace
{
    constexpr const wchar_t* kYoneSoulOuterGlowTexture =
        L"Client/Bin/Resource/Texture/Character/Yone/particles/aura_self.png";

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
        OutputDebugStringA("[YoneSoulSpawnSystem] soul renderer init failed\n");
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
    state.bEActive = true;
    m_mapSoulRenderers[soul] = std::move(pRenderer);

    OutputDebugStringA("[YoneSoulSpawnSystem] soul entity spawned\n");
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

    OutputDebugStringA("[YoneSoulSpawnSystem] soul entity despawned\n");
}
