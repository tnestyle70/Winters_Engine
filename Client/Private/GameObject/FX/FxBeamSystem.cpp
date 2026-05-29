#include "GameObject/FX/FxBeamSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "Renderer/PlaneRenderer.h"
#include "Renderer/RHIFxSpriteRenderer.h"
#include "Renderer/FxShaderConstants.h"
#include "Resource/Texture.h"
#include "RHI/RHITextureLoader.h"
#include "DynamicCamera.h"
#include "ProfilerAPI.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>
#include <vector>

using namespace DirectX;

namespace
{
    f32_t ComputeFadeAlpha(f32_t fElapsed, f32_t fLifetime,
        f32_t fFadeIn, f32_t fFadeOut)
    {
        f32_t fadeAlpha = 1.f;
        if (fFadeIn > 0.f && fElapsed < fFadeIn)
            fadeAlpha *= fElapsed / fFadeIn;
        if (fFadeOut > 0.f && fElapsed > (fLifetime - fFadeOut))
            fadeAlpha *= (fLifetime - fElapsed) / fFadeOut;
        return std::clamp(fadeAlpha, 0.f, 1.f);
    }

    Vec3 ResolveForwardFromYaw(f32_t fYaw)
    {
        return { std::sinf(fYaw), 0.f, std::cosf(fYaw) };
    }

    FxBeamComponent BuildBeamFromEmitter(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const Vec3& vWorldPos,
        EntityID attachTo)
    {
        const Vec3 fallbackForward = ResolveForwardFromYaw(emitter.fYaw);
        const f32_t fallbackLength = (emitter.fHeight > 0.f) ? emitter.fHeight : 1.f;
        const Vec3 endOffset =
            (emitter.vEndOffset.x != 0.f || emitter.vEndOffset.y != 0.f || emitter.vEndOffset.z != 0.f)
            ? emitter.vEndOffset
            : Vec3{ fallbackForward.x * fallbackLength,
                    fallbackForward.y * fallbackLength,
                    fallbackForward.z * fallbackLength };

        FxBeamComponent beam{};
        beam.hAsset = asset.handle;
        beam.iEmitterIndex = emitterIndex;
        beam.hStart = attachTo;
        beam.vStartWorldPos = { vWorldPos.x + emitter.vAttachOffset.x,
                                vWorldPos.y + emitter.vAttachOffset.y,
                                vWorldPos.z + emitter.vAttachOffset.z };
        beam.vEndWorldPos = { beam.vStartWorldPos.x + endOffset.x,
                              beam.vStartWorldPos.y + endOffset.y,
                              beam.vStartWorldPos.z + endOffset.z };
        beam.vStartOffset = emitter.vAttachOffset;
        beam.vEndOffset = { emitter.vAttachOffset.x + endOffset.x,
                            emitter.vAttachOffset.y + endOffset.y,
                            emitter.vAttachOffset.z + endOffset.z };
        beam.vVelocity = emitter.vVelocity;
        beam.SetTexturePath(emitter.strTexturePath);
        beam.fWidth = emitter.fWidth;
        beam.fLifetime = emitter.fLifetime;
        beam.fStartDelay = emitter.fStartDelay;
        beam.fFadeIn = emitter.fFadeIn;
        beam.fFadeOut = emitter.fFadeOut;
        beam.fUvScrollSpeed = emitter.fUvScrollV;
        beam.fUvScrollU = emitter.fUvScrollU;
        beam.fUvScrollV = emitter.fUvScrollV;

        beam.vColor = emitter.vColor;
        beam.blendMode = emitter.blendMode;
        beam.bBlockableByWindWall = emitter.bBlockableByWindWall;
        beam.fAlphaClip = emitter.fAlphaClip;
        beam.fErodeThreshold = emitter.fErodeThreshold;
        beam.SetMaterialFromDesc(emitter.material, emitter.depthMode);
        return beam;
    }

    FxRibbonComponent BuildRibbonFromEmitter(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const Vec3& vWorldPos,
        EntityID attachTo)
    {
        FxRibbonComponent ribbon{};
        ribbon.hAsset = asset.handle;
        ribbon.iEmitterIndex = emitterIndex;
        ribbon.attachTo = attachTo;
        ribbon.vStartOffset = emitter.vAttachOffset;
        ribbon.vEndOffset = emitter.vEndOffset;
        ribbon.vVelocity = emitter.vVelocity;
        ribbon.SetTexturePath(emitter.strTexturePath);
        ribbon.fWidth = emitter.fWidth;
        ribbon.fLifetime = emitter.fLifetime;
        ribbon.fStartDelay = emitter.fStartDelay;
        ribbon.fFadeIn = emitter.fFadeIn;
        ribbon.fFadeOut = emitter.fFadeOut;
        ribbon.fUvScrollU = emitter.fUvScrollU;
        ribbon.fUvScrollV = emitter.fUvScrollV;

        ribbon.vColor = emitter.vColor;
        ribbon.blendMode = emitter.blendMode;
        ribbon.bBlockableByWindWall = emitter.bBlockableByWindWall;
        ribbon.fAlphaClip = emitter.fAlphaClip;
        ribbon.fErodeThreshold = emitter.fErodeThreshold;
        ribbon.SetMaterialFromDesc(emitter.material, emitter.depthMode);

        const Vec3 fallbackForward = ResolveForwardFromYaw(emitter.fYaw);
        const f32_t fallbackLength = (emitter.fHeight > 0.f) ? emitter.fHeight : 1.f;
        Vec3 endOffset = emitter.vEndOffset;
        if (endOffset.x == 0.f && endOffset.y == 0.f && endOffset.z == 0.f)
        {
            endOffset = { fallbackForward.x * fallbackLength,
                          fallbackForward.y * fallbackLength,
                          fallbackForward.z * fallbackLength };
            ribbon.vEndOffset = { ribbon.vStartOffset.x + endOffset.x,
                                  ribbon.vStartOffset.y + endOffset.y,
                                  ribbon.vStartOffset.z + endOffset.z };
        }

        const u32_t pointCount = std::clamp(emitter.iRibbonPointCount, 2u, FX_RIBBON_MAX_POINTS);
        for (u32_t i = 0; i < pointCount; ++i)
        {
            const f32_t t = (pointCount > 1)
                ? static_cast<f32_t>(i) / static_cast<f32_t>(pointCount - 1)
                : 0.f;
            const Vec3 offset{
                emitter.vAttachOffset.x + endOffset.x * t,
                emitter.vAttachOffset.y + endOffset.y * t,
                emitter.vAttachOffset.z + endOffset.z * t
            };
            ribbon.SetPoint(i, { vWorldPos.x + offset.x,
                                 vWorldPos.y + offset.y,
                                 vWorldPos.z + offset.z });
        }

        return ribbon;
    }
}

std::unique_ptr<CFxBeamSystem> CFxBeamSystem::Create(
    IRHIDevice* pDevice,
    DX11Shader* pShader,
    DX11Pipeline* pPipeline,
    CBlendStateCache* pBlendCache)
{
    if (!pDevice)
        return nullptr;

    auto p = std::unique_ptr<CFxBeamSystem>(new CFxBeamSystem());
    p->m_pDevice = pDevice;
    p->m_pBlendCache = pBlendCache;

    if (pDevice->GetBackend() == eRHIBackend::DX12)
    {
        p->m_pRHISprite = CRHIFxSpriteRenderer::Create(pDevice);
        return p->m_pRHISprite ? std::move(p) : nullptr;
    }

    if (!pShader || !pPipeline || !pBlendCache)
        return nullptr;

    p->m_pPlane = CPlaneRenderer::Create(pDevice, pShader, pPipeline);
    if (!p->m_pPlane)
        return nullptr;

    p->m_pPlane->SetBlendCache(pBlendCache, eBlendPreset::Additive);
    return p;
}

EntityID CFxBeamSystem::Spawn(CWorld& world, const FxBeamComponent& tmpl)
{
    EntityID e = world.CreateEntity();

    FxBeamComponent instance = tmpl;
    if (!instance.bMaterialReady)
        instance.RefreshMaterialFromLegacyFields();

    world.AddComponent<FxBeamComponent>(e, instance);
    return e;
}

EntityID CFxBeamSystem::Spawn(CWorld& world, const FxRibbonComponent& tmpl)
{
    EntityID e = world.CreateEntity();

    FxRibbonComponent instance = tmpl;
    if (!instance.bMaterialReady)
        instance.RefreshMaterialFromLegacyFields();

    world.AddComponent<FxRibbonComponent>(e, instance);
    return e;
}

EntityID CFxBeamSystem::SpawnFromAsset(CWorld& world, const CFxAssetRegistry& registry,
    FxAssetHandle handle, const Vec3& vWorldPos, EntityID attachTo)
{
    const FxAsset* pAsset = registry.Find(handle);
    if (!pAsset)
        return NULL_ENTITY;

    return SpawnFromAsset(world, *pAsset, vWorldPos, attachTo);
}

EntityID CFxBeamSystem::SpawnFromAsset(CWorld& world, const FxAsset& asset,
    const Vec3& vWorldPos, EntityID attachTo)
{
    EntityID firstEntity = NULL_ENTITY;

    for (u32_t i = 0; i < asset.emitters.size(); ++i)
    {
        const FxEmitterDesc& emitter = asset.emitters[i];
        EntityID spawned = NULL_ENTITY;
        if (emitter.renderType == eFxRenderType::Beam)
        {
            spawned = Spawn(world,
                BuildBeamFromEmitter(asset, emitter, i, vWorldPos, attachTo));
        }
        else if (emitter.renderType == eFxRenderType::Ribbon)
        {
            spawned = Spawn(world,
                BuildRibbonFromEmitter(asset, emitter, i, vWorldPos, attachTo));
        }

        if (firstEntity == NULL_ENTITY && spawned != NULL_ENTITY)
            firstEntity = spawned;
    }

    return firstEntity;
}

void CFxBeamSystem::Update(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("FxBeam::Update");

    std::vector<EntityID> vecDelete;
    world.ForEach<FxBeamComponent>(
        std::function<void(EntityID, FxBeamComponent&)>(
            [&](EntityID e, FxBeamComponent& beam)
            {
                beam.fElapsed += fTimeDelta;

                if (beam.hStart != NULL_ENTITY
                    && world.HasComponent<TransformComponent>(beam.hStart))
                {
                    const Vec3& p = world.GetComponent<TransformComponent>(beam.hStart).m_LocalPosition;
                    beam.vStartWorldPos = { p.x + beam.vStartOffset.x,
                                            p.y + beam.vStartOffset.y,
                                            p.z + beam.vStartOffset.z };
                }
                else
                {
                    beam.vStartWorldPos.x += beam.vVelocity.x * fTimeDelta;
                    beam.vStartWorldPos.y += beam.vVelocity.y * fTimeDelta;
                    beam.vStartWorldPos.z += beam.vVelocity.z * fTimeDelta;
                }

                if (beam.hEnd != NULL_ENTITY
                    && world.HasComponent<TransformComponent>(beam.hEnd))
                {
                    const Vec3& p = world.GetComponent<TransformComponent>(beam.hEnd).m_LocalPosition;
                    beam.vEndWorldPos = { p.x + beam.vEndOffset.x,
                                          p.y + beam.vEndOffset.y,
                                          p.z + beam.vEndOffset.z };
                }
                else if (beam.hStart != NULL_ENTITY
                    && world.HasComponent<TransformComponent>(beam.hStart))
                {
                    const Vec3& p = world.GetComponent<TransformComponent>(beam.hStart).m_LocalPosition;
                    beam.vEndWorldPos = { p.x + beam.vEndOffset.x,
                                          p.y + beam.vEndOffset.y,
                                          p.z + beam.vEndOffset.z };
                }
                else
                {
                    beam.vEndWorldPos.x += beam.vVelocity.x * fTimeDelta;
                    beam.vEndWorldPos.y += beam.vVelocity.y * fTimeDelta;
                    beam.vEndWorldPos.z += beam.vVelocity.z * fTimeDelta;
                }

                if (beam.bPendingDelete || beam.fElapsed >= beam.fStartDelay + beam.fLifetime)
                    vecDelete.push_back(e);
            }));

    world.ForEach<FxRibbonComponent>(
        std::function<void(EntityID, FxRibbonComponent&)>(
            [&](EntityID e, FxRibbonComponent& ribbon)
            {
                ribbon.fElapsed += fTimeDelta;

                if (ribbon.attachTo != NULL_ENTITY
                    && world.HasComponent<TransformComponent>(ribbon.attachTo))
                {
                    const Vec3& p = world.GetComponent<TransformComponent>(ribbon.attachTo).m_LocalPosition;
                    ribbon.SetPoint(0, { p.x + ribbon.vStartOffset.x,
                                         p.y + ribbon.vStartOffset.y,
                                         p.z + ribbon.vStartOffset.z });
                    ribbon.SetPoint(1, { p.x + ribbon.vEndOffset.x,
                                         p.y + ribbon.vEndOffset.y,
                                         p.z + ribbon.vEndOffset.z });
                }
                else
                {
                    for (u32_t i = 0; i < ribbon.iPointCount; ++i)
                    {
                        ribbon.points[i].x += ribbon.vVelocity.x * fTimeDelta;
                        ribbon.points[i].y += ribbon.vVelocity.y * fTimeDelta;
                        ribbon.points[i].z += ribbon.vVelocity.z * fTimeDelta;
                    }
                }

                if (ribbon.bPendingDelete || ribbon.fElapsed >= ribbon.fStartDelay + ribbon.fLifetime)
                    vecDelete.push_back(e);
            }));

    for (EntityID e : vecDelete)
        world.DestroyEntity(e);
}

void CFxBeamSystem::Render(CWorld& world, const CDynamicCamera* pCamera)
{
    WINTERS_PROFILE_SCOPE("FxBeam::Render");

    if (!pCamera)
        return;

    const bool_t bUseRHI = m_pDevice && m_pDevice->GetBackend() == eRHIBackend::DX12;
    if ((bUseRHI && !m_pRHISprite) || (!bUseRHI && !m_pPlane))
        return;

    const Mat4 matVP = pCamera->GetViewProjection();

    world.ForEach<FxBeamComponent>(
        std::function<void(EntityID, FxBeamComponent&)>(
            [&](EntityID, FxBeamComponent& beam)
            {
                if (beam.bPendingDelete || beam.fLifetime <= 0.f || !beam.texturePath)
                    return;
                if (beam.fElapsed < beam.fStartDelay)
                    return;

                if (!beam.bMaterialReady)
                    beam.RefreshMaterialFromLegacyFields();

                const f32_t fAge = beam.fElapsed - beam.fStartDelay;
                const f32_t alpha = ComputeFadeAlpha(
                    fAge, beam.fLifetime, beam.fFadeIn, beam.fFadeOut);
                const Vec4 tint = { beam.vColor.x, beam.vColor.y, beam.vColor.z,
                                    beam.vColor.w * alpha };
                const Vec2 uvScroll = { beam.fUvScrollU * fAge,
                                        (beam.fUvScrollV + beam.fUvScrollSpeed) * fAge };
                FxMaterialDesc drawMaterial = beam.material;
                drawMaterial.vTint = tint;
                drawMaterial.vUVRect = { 0.f, 0.f, 1.f, 1.f };
                drawMaterial.vUVScroll = uvScroll;

                const f32_t fNormalizedAge =
                    (beam.fLifetime > 0.f) ? std::clamp(fAge / beam.fLifetime, 0.f, 1.f) : 0.f;

                const CBFxParams fxParams = MakeFxParamsFromMaterial(
                    drawMaterial,
                    drawMaterial.vTint,
                    drawMaterial.vUVRect,
                    drawMaterial.vUVScroll,
                    fAge,
                    fNormalizedAge);

                DrawSegment(
                    beam.vStartWorldPos,
                    beam.vEndWorldPos,
                    beam.fWidth,
                    beam.texturePath,
                    beam.blendMode,
                    eFxDepthMode::OverlayNoDepth,
                    fxParams,
                    matVP);
            }));

    world.ForEach<FxRibbonComponent>(
        std::function<void(EntityID, FxRibbonComponent&)>(
            [&](EntityID, FxRibbonComponent& ribbon)
            {
                if (ribbon.bPendingDelete || ribbon.fLifetime <= 0.f ||
                    !ribbon.texturePath || ribbon.iPointCount < 2)
                    return;
                if (ribbon.fElapsed < ribbon.fStartDelay)
                    return;

                if (!ribbon.bMaterialReady)
                    ribbon.RefreshMaterialFromLegacyFields();

                const f32_t fAge = ribbon.fElapsed - ribbon.fStartDelay;
                const f32_t alpha = ComputeFadeAlpha(
                    fAge, ribbon.fLifetime, ribbon.fFadeIn, ribbon.fFadeOut);
                const Vec4 tint = { ribbon.vColor.x, ribbon.vColor.y, ribbon.vColor.z,
                                    ribbon.vColor.w * alpha };
                const Vec2 uvScroll = { ribbon.fUvScrollU * fAge,
                                        ribbon.fUvScrollV * fAge };

                FxMaterialDesc drawMaterial = ribbon.material;
                drawMaterial.vTint = tint;
                drawMaterial.vUVRect = { 0.f, 0.f, 1.f, 1.f };
                drawMaterial.vUVScroll = uvScroll;

                const f32_t fNormalizedAge =
                    (ribbon.fLifetime > 0.f) ? std::clamp(fAge / ribbon.fLifetime, 0.f, 1.f) : 0.f;

                const CBFxParams fxParams = MakeFxParamsFromMaterial(
                    drawMaterial,
                    drawMaterial.vTint,
                    drawMaterial.vUVRect,
                    drawMaterial.vUVScroll,
                    fAge,
                    fNormalizedAge);

                for (u32_t i = 0; i + 1 < ribbon.iPointCount; ++i)
                {
                    DrawSegment(
                        ribbon.points[i],
                        ribbon.points[i + 1],
                        ribbon.fWidth,
                        ribbon.texturePath,
                        ribbon.blendMode,
                        eFxDepthMode::OverlayNoDepth,
                        fxParams,
                        matVP);
                }
            }));

    if (!bUseRHI)
    {
        m_pPlane->ResetFxParams();
        m_pPlane->SetBlendCache(m_pBlendCache, eBlendPreset::AlphaBlend);
    }
}

Engine::CTexture* CFxBeamSystem::GetOrLoadTexture(const wchar_t* wszPath)
{
    if (!wszPath)
        return nullptr;

    std::wstring key(wszPath);
    auto it = m_TextureCache.find(key);
    if (it != m_TextureCache.end())
        return it->second.get();

    auto p = Engine::CTexture::Create(m_pDevice, key, Engine::eTexSamplerMode::Clamp);
    if (!p)
    {
        ::OutputDebugStringW((L"[FxBeamSystem] Texture load fail: " + key + L"\n").c_str());
        return nullptr;
    }

    Engine::CTexture* raw = p.get();
    m_TextureCache.emplace(std::move(key), std::move(p));
    return raw;
}

RHITextureHandle CFxBeamSystem::GetOrLoadRHITexture(const wchar_t* wszPath)
{
    if (!wszPath || !m_pDevice)
        return {};

    std::wstring key(wszPath);
    auto it = m_RHITextureCache.find(key);
    if (it != m_RHITextureCache.end())
        return it->second;

    RHITextureHandle handle = RHI_CreateTextureFromFile(m_pDevice, key.c_str(), "FxBeamSystemRHITexture");
    if (!handle.IsValid())
    {
        ::OutputDebugStringW((L"[FxBeamSystem] RHI texture load fail: " + key + L"\n").c_str());
        return {};
    }

    m_RHITextureCache.emplace(std::move(key), handle);
    return handle;
}

void CFxBeamSystem::DrawSegment(const Vec3& vStart, const Vec3& vEnd, f32_t fWidth,
    const wchar_t* wszTexturePath, eBlendPreset blendMode,
    eFxDepthMode depthMode,
    const CBFxParams& fxParams,
    const Mat4& matVP)
{
    const bool_t bUseRHI = m_pDevice && m_pDevice->GetBackend() == eRHIBackend::DX12;
    Engine::CTexture* pTex = nullptr;
    RHITextureHandle hRHITexture{};

    if (bUseRHI)
    {
        hRHITexture = GetOrLoadRHITexture(wszTexturePath);
        if (!hRHITexture.IsValid())
            return;
    }
    else
    {
        pTex = GetOrLoadTexture(wszTexturePath);
        if (!pTex)
            return;
    }

    if ((bUseRHI && !m_pRHISprite) || (!bUseRHI && !m_pPlane))
        return;

    const f32_t dx = vEnd.x - vStart.x;
    const f32_t dz = vEnd.z - vStart.z;
    const f32_t length = std::sqrt(dx * dx + dz * dz);
    if (length <= 0.001f)
        return;

    const Vec3 mid{
        (vStart.x + vEnd.x) * 0.5f,
        (vStart.y + vEnd.y) * 0.5f,
        (vStart.z + vEnd.z) * 0.5f
    };
    const f32_t yaw = std::atan2f(dx, dz);

    const XMMATRIX mS = XMMatrixScaling(fWidth, 1.f, length);
    const XMMATRIX mR = XMMatrixRotationY(yaw);
    const XMMATRIX mT = XMMatrixTranslation(mid.x, mid.y, mid.z);
    const XMMATRIX mWorld = mS * mR * mT;

    Mat4 world;
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&world.m), mWorld);

    if (bUseRHI)
    {
        m_pRHISprite->Draw(
            m_pDevice,
            hRHITexture,
            world,
            matVP,
            fxParams,
            blendMode);
    }
    else
    {
        m_pPlane->SetFxParams(fxParams);
        m_pPlane->SetBlendCache(m_pBlendCache, blendMode);
        m_pPlane->SetDepthMode(depthMode);
        m_pPlane->SetTexture(pTex);
        m_pPlane->SetWorld(world);
        m_pPlane->Render(m_pDevice, matVP);
    }
}

void CFxBeamSystem::Shutdown()
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
