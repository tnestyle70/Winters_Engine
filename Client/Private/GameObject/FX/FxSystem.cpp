#include "GameObject/FX/FxSystem.h"
#include "ECS/CCommandBuffer.h"
#include "ECS/World.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "GameObject/FX/FxAnchorResolver.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxBeamSystem.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Renderer/PlaneRenderer.h"
#include "Renderer/RHIFxSpriteRenderer.h"
#include "Renderer/FxShaderConstants.h"
#include "Resource/Texture.h"
#include "RHI/RHITextureLoader.h"
#include "RHI/RHITypes.h"
#include "GameInstance.h"
#include "DynamicCamera.h"
#include "ProfilerAPI.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <utility>

namespace
{
    bool IsBillboardBackedType(eFxRenderType type)
    {
        return type == eFxRenderType::Billboard ||
            type == eFxRenderType::GroundDecal ||
            type == eFxRenderType::ShockwaveRing;
    }

    constexpr f32_t kFxRenderNearDistance = 8.f;
    constexpr f32_t kFxRenderMaxDistance = 70.f;

    bool_t TryQueryLocalTeam(CWorld& world, u8_t& outLocalTeam)
    {
        bool_t bFound = false;
        world.ForEach<LocalPlayerTag, SpatialAgentComponent>(
            std::function<void(EntityID, LocalPlayerTag&, SpatialAgentComponent&)>(
                [&](EntityID, LocalPlayerTag&, SpatialAgentComponent& agent)
                {
                    if (bFound)
                        return;
                    outLocalTeam = agent.team;
                    bFound = true;
                }));
        return bFound;
    }

    bool_t IsFxVisibleForLocal(
        CWorld& world,
        const FxBillboardComponent& fx,
        bool_t bHasLocalTeam,
        u8_t localTeam)
    {
        if (fx.visibilityPolicy == eFxVisibilityPolicy::Always)
            return true;

        if (!bHasLocalTeam || localTeam >= 8u)
            return false;

        const u8_t localTeamBit = static_cast<u8_t>(1u << localTeam);
        if (fx.visibilityPolicy == eFxVisibilityPolicy::TeamMask)
            return (fx.visibilityTeamMask & localTeamBit) != 0u;

        if (fx.attachTo == NULL_ENTITY || !world.IsAlive(fx.attachTo))
            return false;

        if (world.HasComponent<SpatialAgentComponent>(fx.attachTo) &&
            world.GetComponent<SpatialAgentComponent>(fx.attachTo).team == localTeam)
        {
            return true;
        }

        if (!world.HasComponent<VisibilityComponent>(fx.attachTo))
            return false;

        const auto& visibility =
            world.GetComponent<VisibilityComponent>(fx.attachTo);
        return (visibility.teamVisibilityMask & localTeamBit) != 0u;
    }

    bool_t IsFxRenderRelevant(
        const Vec3& vWorldPos,
        const Vec3& vCameraPos,
        const Vec3& vCameraForward,
        f32_t fBoundsRadius)
    {
        const f32_t dx = vWorldPos.x - vCameraPos.x;
        const f32_t dy = vWorldPos.y - vCameraPos.y;
        const f32_t dz = vWorldPos.z - vCameraPos.z;
        const f32_t distSq = dx * dx + dy * dy + dz * dz;

        const f32_t nearDist = kFxRenderNearDistance + fBoundsRadius;
        if (distSq <= nearDist * nearDist)
            return true;

        const f32_t maxDist = kFxRenderMaxDistance + fBoundsRadius;
        if (distSq > maxDist * maxDist)
            return false;

        const f32_t invLen = 1.f / std::sqrt((std::max)(distSq, 0.0001f));
        const f32_t dot =
            (dx * invLen) * vCameraForward.x +
            (dy * invLen) * vCameraForward.y +
            (dz * invLen) * vCameraForward.z;

        return dot > -0.25f;
    }

    FxBillboardComponent BuildBillboardFromEmitter(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const Vec3& vWorldPos,
        EntityID attachTo)
    {
        FxBillboardComponent fx{};
        fx.hAsset = asset.handle;
        fx.iEmitterIndex = emitterIndex;
        fx.renderType = emitter.renderType;
        fx.vWorldPos = vWorldPos;
        fx.attachTo = attachTo;
        fx.vAttachOffset = emitter.vAttachOffset;
        fx.anchor = emitter.anchor;
        fx.lifecycle = emitter.lifecycle;
        fx.vVelocity = emitter.vVelocity;
        fx.SetTexturePath(emitter.strTexturePath);
        fx.fWidth = emitter.fWidth;
        fx.fHeight = emitter.fHeight;
        fx.fYaw = emitter.fYaw;
        fx.fStartRadius = emitter.fStartRadius;
        fx.fEndRadius = emitter.fEndRadius;
        fx.fThickness = emitter.fThickness;
        fx.fGrowDuration = emitter.fGrowDuration;
        fx.vColor = emitter.vColor;
        fx.fFadeIn = emitter.fFadeIn;
        fx.fFadeOut = emitter.fFadeOut;
        fx.iAtlasCols = emitter.iAtlasCols;
        fx.iAtlasRows = emitter.iAtlasRows;
        fx.iAtlasFrameCount = emitter.iAtlasFrameCount;
        fx.fAtlasFps = emitter.fAtlasFps;
        fx.bAtlasLoop = emitter.bAtlasLoop;
        fx.fUvScrollU = emitter.fUvScrollU;
        fx.fUvScrollV = emitter.fUvScrollV;

        fx.fAlphaClip = emitter.fAlphaClip;
        fx.fErodeThreshold = emitter.fErodeThreshold;
        fx.SetMaterialFromDesc(emitter.material, emitter.depthMode);

        fx.blendMode = emitter.blendMode;
        fx.fLifetime = emitter.fLifetime;
        fx.fStartDelay = emitter.fStartDelay;
        fx.bBillboard = emitter.bBillboard && emitter.renderType == eFxRenderType::Billboard;
        fx.bBlockableByWindWall = emitter.bBlockableByWindWall;
        return fx;
    }
}

std::unique_ptr<CFxSystem> CFxSystem::Create(
    IRHIDevice* pDevice, CBlendStateCache* pBlendCache)
{
    if (!pDevice)
        return nullptr;

    auto p = std::unique_ptr<CFxSystem>(new CFxSystem());
    p->m_pDevice = pDevice;
    p->m_pBlendCache = pBlendCache;

    if (pDevice->GetBackend() == eRHIBackend::DX12)
    {
        p->m_pRHISprite = CRHIFxSpriteRenderer::Create(pDevice);
        return p->m_pRHISprite ? std::move(p) : nullptr;
    }

    if (!pBlendCache)
        return nullptr;

    Engine::CGameInstance* pGI = Engine::CGameInstance::Get();
    if (!pGI)
        return nullptr;

    DX11Shader* pShader = pGI->Get_FxSpriteShader();
    DX11Pipeline* pPipeline = pGI->Get_FxSpritePipeline();
    if (!pShader || !pPipeline)
        return nullptr;

    p->m_pPlane = CPlaneRenderer::Create(pDevice, pShader, pPipeline);
    if (!p->m_pPlane) return nullptr;
    p->m_pPlane->SetBlendCache(pBlendCache, eBlendPreset::AlphaBlend);
    return p;
}

EntityID CFxSystem::Spawn(CWorld& world, const FxBillboardComponent& tmpl)
{
    EntityID e = world.CreateEntity();

    FxBillboardComponent instance = tmpl;
    if (!instance.bMaterialReady)
        instance.RefreshMaterialFromLegacyFields();

    world.AddComponent<FxBillboardComponent>(e, instance);
    return e;
}

CFxAssetRegistry& CFxSystem::GetAssetRegistry()
{
    return *Engine::CGameInstance::Get()->Get_FxAssetRegistry();
}

FxAssetHandle CFxSystem::RegisterAsset(FxAsset asset)
{
    return GetAssetRegistry().RegisterOrReplaceByName(std::move(asset));
}

EntityID CFxSystem::SpawnFromAsset(CWorld& world, FxAssetHandle handle,
    const Vec3& vWorldPos, EntityID attachTo)
{
    const FxAsset* pAsset = GetAssetRegistry().Find(handle);
    if (!pAsset)
        return NULL_ENTITY;

    return SpawnFromAsset(world, *pAsset, vWorldPos, attachTo);
}

EntityID CFxSystem::SpawnFromAsset(CWorld& world, const FxAsset& asset,
    const Vec3& vWorldPos, EntityID attachTo)
{
    if (asset.emitters.empty())
        return NULL_ENTITY;

    EntityID firstEntity = NULL_ENTITY;
    for (u32_t i = 0; i < asset.emitters.size(); ++i)
    {
        const FxEmitterDesc& emitter = asset.emitters[i];
        if (!IsBillboardBackedType(emitter.renderType))
            continue;

        FxBillboardComponent fx =
            BuildBillboardFromEmitter(asset, emitter, i, vWorldPos, attachTo);
        const EntityID spawned = Spawn(world, fx);
        if (firstEntity == NULL_ENTITY)
            firstEntity = spawned;
    }

    const EntityID beamEntity =
        CFxBeamSystem::SpawnFromAsset(world, asset, vWorldPos, attachTo);
    if (firstEntity == NULL_ENTITY)
        firstEntity = beamEntity;

    return firstEntity;
}

void CFxSystem::DeferSpawnFromAsset(CCommandBuffer& commandBuffer,
    FxAssetHandle handle,
    const Vec3& vWorldPos,
    EntityID attachTo)
{
    commandBuffer.DeferCommand(
        [handle, vWorldPos, attachTo](CWorld& world)
        {
            CFxSystem::SpawnFromAsset(world, handle, vWorldPos, attachTo);
        });
}

bool_t CFxSystem::PreloadTextureResource(const wchar_t* wszPath)
{
    if (!wszPath || !wszPath[0] || !m_pDevice)
        return false;

    if (m_pDevice->GetBackend() == eRHIBackend::DX12)
        return GetOrLoadRHITexture(wszPath).IsValid();

    return GetOrLoadTexture(wszPath) != nullptr;
}

void CFxSystem::Update(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("Fx::Update");

    std::vector<EntityID> vecDelete;
    //ForEach
    world.ForEach<FxBillboardComponent>(
        std::function<void(EntityID, FxBillboardComponent&)>(
            [&](EntityID e, FxBillboardComponent& fx)
            {
                f32_t fEffectiveDelta = fTimeDelta;
                if (fx.fStartDelay > 0.f)
                {
                    fx.fStartDelay -= fTimeDelta;
                    if (fx.fStartDelay > 0.f)
                        return;

                    fEffectiveDelta = -fx.fStartDelay;
                    fx.fStartDelay = 0.f;
                }

                fx.fElapsed += fEffectiveDelta;

                Vec3 vResolvedAnchor{};
                if (FxAnchor::TryResolveWorldPosition(
                    world,
                    fx.attachTo,
                    fx.anchor,
                    fx.vAttachOffset,
                    fx.vWorldPos,
                    vResolvedAnchor))
                {
                    fx.vWorldPos = vResolvedAnchor;
                    fx.bAnchorResolvedLastFrame = true;
                }
                else
                {
                    fx.bAnchorResolvedLastFrame = false;
                    if (fx.bDestroyWhenAttachInvalid && fx.attachTo != NULL_ENTITY)
                    {
                        vecDelete.push_back(e);
                        return;
                    }
                    // [Phase T-8r] ?ъ궗泥?紐⑤뱶 ??vVelocity (m/s) ?곸슜
                    fx.vWorldPos.x += fx.vVelocity.x * fEffectiveDelta;
                    fx.vWorldPos.y += fx.vVelocity.y * fEffectiveDelta;
                    fx.vWorldPos.z += fx.vVelocity.z * fEffectiveDelta;
                }

                if (fx.bPendingDelete || fx.fElapsed >= fx.fLifetime)
                    vecDelete.push_back(e);
            }));

    for (EntityID e : vecDelete)
        world.DestroyEntity(e);
}

void CFxSystem::Render(CWorld& world, const CDynamicCamera* pCamera)
{
    WINTERS_PROFILE_SCOPE("Fx::Render");

    if (!pCamera) return;

    const bool_t bUseRHI = m_pDevice && m_pDevice->GetBackend() == eRHIBackend::DX12;
    if ((bUseRHI && !m_pRHISprite) || (!bUseRHI && !m_pPlane))
        return;

    const Mat4 matVP = pCamera->GetViewProjection();
    const Vec3 vCamRight = pCamera->GetRight();
    const Vec3 vCamUp = pCamera->GetUp();
    const Vec3 vCamFwd = pCamera->GetForward();
    const Vec3 vCamPos = pCamera->GetEye();
    u8_t localTeam = 0u;
    const bool_t bHasLocalTeam = TryQueryLocalTeam(world, localTeam);

    uint64_t drawCount = 0;
    uint64_t cullSkippedCount = 0;

    world.ForEach<FxBillboardComponent>(
        std::function<void(EntityID, FxBillboardComponent&)>(
            [&](EntityID /*e*/, FxBillboardComponent& fx)
            {
                if (fx.bPendingDelete || fx.fLifetime <= 0.f) return;
                if (fx.fStartDelay > 0.f) return;
                if (!fx.texturePath) return;
                if (!IsFxVisibleForLocal(world, fx, bHasLocalTeam, localTeam)) return;

                const f32_t boundsRadius = (std::max)(fx.fWidth, fx.fHeight) * 0.75f;
                if (!IsFxRenderRelevant(fx.vWorldPos, vCamPos, vCamFwd, boundsRadius))
                {
                    ++cullSkippedCount;
                    return;
                }

                Engine::CTexture* pTex = nullptr;
                RHITextureHandle hRHITexture{};
                if (bUseRHI)
                {
                    hRHITexture = GetOrLoadRHITexture(fx.texturePath);
                    if (!hRHITexture.IsValid()) return;
                }
                else
                {
                    pTex = GetOrLoadTexture(fx.texturePath);
                    if (!pTex) return;
                }

                // ??  ?섏씠???뚰뙆 怨꾩궛 ??
                f32_t fadeAlpha = 1.f;
                if (fx.fFadeIn > 0.f && fx.fElapsed < fx.fFadeIn)
                    fadeAlpha *= fx.fElapsed / fx.fFadeIn;
                if (fx.fFadeOut > 0.f && fx.fElapsed > (fx.fLifetime - fx.fFadeOut))
                    fadeAlpha *= (fx.fLifetime - fx.fElapsed) / fx.fFadeOut;
                fadeAlpha = std::clamp(fadeAlpha, 0.f, 1.f);

                f32_t drawWidth = fx.fWidth;
                f32_t drawHeight = fx.fHeight;
                if (fx.renderType == eFxRenderType::GroundDecal && fx.fGrowDuration > 0.f)
                {
                    const f32_t grow = std::clamp(fx.fElapsed / fx.fGrowDuration, 0.f, 1.f);
                    drawWidth *= grow;
                    drawHeight *= grow;
                }
                else if (fx.renderType == eFxRenderType::ShockwaveRing)
                {
                    const f32_t t = std::clamp(fx.fElapsed / fx.fLifetime, 0.f, 1.f);
                    const f32_t startRadius = (fx.fStartRadius > 0.f) ? fx.fStartRadius : (fx.fWidth * 0.5f);
                    const f32_t endRadius = (fx.fEndRadius > 0.f) ? fx.fEndRadius : (fx.fHeight * 0.5f);
                    const f32_t radius = startRadius + (endRadius - startRadius) * t;
                    const f32_t diameter = radius * 2.f;
                    drawWidth = diameter;
                    drawHeight = diameter;
                }

                // ??  ?꾪??쇱뒪 frame index ??UV rect ??
                Vec4 uvRect = { 0.f, 0.f, 1.f, 1.f };
                const u32_t atlasCols = (fx.iAtlasCols > 0) ? fx.iAtlasCols : 1;
                const u32_t atlasRows = (fx.iAtlasRows > 0) ? fx.iAtlasRows : 1;
                const u32_t atlasMaxFrames = atlasCols * atlasRows;
                const u32_t requestedFrames = (fx.iAtlasFrameCount > 0) ? fx.iAtlasFrameCount : 1;
                const u32_t atlasFrameCount =
                    (requestedFrames < atlasMaxFrames) ? requestedFrames : atlasMaxFrames;
                if (atlasFrameCount > 1)
                {
                    u32_t frame = 0u;
                    if (fx.fAtlasFps > 0.f)
                    {
                        frame = static_cast<u32_t>(fx.fElapsed * fx.fAtlasFps);
                        if (fx.bAtlasLoop) frame %= atlasFrameCount;
                        else               frame = (frame < atlasFrameCount) ? frame : (atlasFrameCount - 1);
                    }

                    const u32_t col = frame % atlasCols;
                    const u32_t row = frame / atlasCols;
                    const f32_t u0 = static_cast<f32_t>(col) / atlasCols;
                    const f32_t v0 = static_cast<f32_t>(row) / atlasRows;
                    const f32_t u1 = static_cast<f32_t>(col + 1) / atlasCols;
                    const f32_t v1 = static_cast<f32_t>(row + 1) / atlasRows;
                    uvRect = { u0, v0, u1, v1 };
                }

                // ?? UV ?ㅽ겕濡??꾩쟻 ??
                const Vec2 uvScroll = { fx.fUvScrollU * fx.fElapsed,
                                        fx.fUvScrollV * fx.fElapsed };

                // ?? Tint ? fadeAlpha ?⑹꽦 ??
                const Vec4 tint = { fx.vColor.x, fx.vColor.y, fx.vColor.z,
                                    fx.vColor.w * fadeAlpha };

                // ?? PlaneRenderer ??FxParams ?명똿 ??
                // ?? Blend ?곸슜 ??
                if (!bUseRHI)
                    m_pPlane->SetBlendCache(m_pBlendCache, fx.blendMode);



                using namespace DirectX;
                XMMATRIX mWorld;

                if (fx.bBillboard)
                {
                    // 移대찓??諛붾씪蹂대뒗 荑쇰뱶 ??quad(XZ plane) 瑜?(Right, -Fwd, Up) 異뺤쑝濡??뚯쟾 ???ㅼ???
                    const XMVECTOR vR = XMVectorSet(vCamRight.x, vCamRight.y, vCamRight.z, 0.f);
                    const XMVECTOR vU = XMVectorSet(vCamUp.x, vCamUp.y, vCamUp.z, 0.f);
                    const XMVECTOR vN = XMVectorSet(-vCamFwd.x, -vCamFwd.y, -vCamFwd.z, 0.f);

                    XMMATRIX mRot = XMMatrixIdentity();
                    mRot.r[0] = XMVectorScale(vR, drawWidth);
                    mRot.r[1] = vN;
                    mRot.r[2] = XMVectorScale(vU, drawHeight);
                    mRot.r[3] = XMVectorSet(fx.vWorldPos.x, fx.vWorldPos.y, fx.vWorldPos.z, 1.f);
                    mWorld = mRot;
                }
                else
                {
                    // Non-billboard quad. Callers own any Y offset for ground decals.
                    const XMMATRIX mS = XMMatrixScaling(drawWidth, 1.f, drawHeight);
                    const XMMATRIX mR = XMMatrixRotationY(fx.fYaw);
                    const XMMATRIX mT = XMMatrixTranslation(fx.vWorldPos.x, fx.vWorldPos.y, fx.vWorldPos.z);
                    mWorld = mS * mR * mT;
                }

                Mat4 world;
                XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&world.m), mWorld);

                if (!fx.bMaterialReady)
                    fx.RefreshMaterialFromLegacyFields();

                FxMaterialDesc drawMaterial = fx.material;
                drawMaterial.vTint = tint;
                drawMaterial.vUVRect = uvRect;
                drawMaterial.vUVScroll = uvScroll;

                const f32_t fNormalizedAge =
                    (fx.fLifetime > 0.f) ? std::clamp(fx.fElapsed / fx.fLifetime, 0.f, 1.f) : 0.f;

                const CBFxParams fxParams = MakeFxParamsFromMaterial(
                    drawMaterial,
                    drawMaterial.vTint,
                    drawMaterial.vUVRect,
                    drawMaterial.vUVScroll,
                    fx.fElapsed,
                    fNormalizedAge);

                if (bUseRHI)
                {
                    m_pRHISprite->Draw(
                        m_pDevice,
                        hRHITexture,
                        world,
                        matVP,
                        fxParams,
                        fx.blendMode);
                }
                else
                {
                    m_pPlane->SetFxParams(fxParams);
                    // Runtime FX should not be clipped by terrain depth; world actors keep normal depth.
                    m_pPlane->SetDepthMode(eFxDepthMode::OverlayNoDepth);
                    m_pPlane->SetTexture(pTex);
                    m_pPlane->SetWorld(world);
                    m_pPlane->Render(m_pDevice, matVP);

                    // Reset shared plane state after FX rendering.
                    m_pPlane->ResetFxParams();
                    m_pPlane->SetBlendCache(m_pBlendCache, eBlendPreset::AlphaBlend);
                }

                ++drawCount;
            }));

    WINTERS_PROFILE_COUNT("Fx::Drawn", drawCount);
    WINTERS_PROFILE_COUNT("Fx::CullSkipped", cullSkippedCount);
}

Engine::CTexture* CFxSystem::GetOrLoadTexture(const wchar_t* wszPath)
{
    if (!wszPath) return nullptr;
    std::wstring key(wszPath);

    auto it = m_TextureCache.find(key);
    if (it != m_TextureCache.end()) return it->second.get();

    auto p = Engine::CTexture::Create(m_pDevice, key, Engine::eTexSamplerMode::Clamp);
    if (!p)
    {
        return nullptr;
    }
    Engine::CTexture* raw = p.get();
    m_TextureCache.emplace(std::move(key), std::move(p));
    return raw;
}

RHITextureHandle CFxSystem::GetOrLoadRHITexture(const wchar_t* wszPath)
{
    if (!wszPath || !m_pDevice)
        return {};

    std::wstring key(wszPath);
    auto it = m_RHITextureCache.find(key);
    if (it != m_RHITextureCache.end())
        return it->second;

    RHITextureHandle handle = RHI_CreateTextureFromFile(m_pDevice, key.c_str(), "FxSystemRHITexture");
    if (!handle.IsValid())
    {
        return {};
    }

    m_RHITextureCache.emplace(std::move(key), handle);
    return handle;
}

void CFxSystem::Shutdown()
{
    if (m_pDevice)
    {
        for (auto& pair : m_RHITextureCache)
        {
            if (pair.second.IsValid())
                m_pDevice->DestroyTexture(pair.second);
        }
    }
    m_RHITextureCache.clear();
    m_pRHISprite.reset();
    m_TextureCache.clear();
    m_pPlane.reset();
}
