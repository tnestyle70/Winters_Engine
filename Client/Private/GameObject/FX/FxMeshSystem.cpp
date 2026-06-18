#include "GameObject/FX/FxMeshSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "GameObject/FX/FxAnchorResolver.h"
#include "Renderer/FxStaticMeshRenderer.h"
#include "DynamicCamera.h"
#include "ProfilerAPI.h"
#include <DirectXMath.h>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace
{
    constexpr f32_t kFxMeshRenderNearDistance = 8.f;
    constexpr f32_t kFxMeshRenderMaxDistance = 70.f;

    bool_t IsFxMeshRenderRelevant(
        const Vec3& vWorldPos,
        const Vec3& vCameraPos,
        const Vec3& vCameraForward,
        f32_t fBoundsRadius)
    {
        const f32_t dx = vWorldPos.x - vCameraPos.x;
        const f32_t dy = vWorldPos.y - vCameraPos.y;
        const f32_t dz = vWorldPos.z - vCameraPos.z;
        const f32_t distSq = dx * dx + dy * dy + dz * dz;

        const f32_t nearDist = kFxMeshRenderNearDistance + fBoundsRadius;
        if (distSq <= nearDist * nearDist)
            return true;

        const f32_t maxDist = kFxMeshRenderMaxDistance + fBoundsRadius;
        if (distSq > maxDist * maxDist)
            return false;

        const f32_t invLen = 1.f / std::sqrt((std::max)(distSq, 0.0001f));
        const f32_t dot =
            (dx * invLen) * vCameraForward.x +
            (dy * invLen) * vCameraForward.y +
            (dz * invLen) * vCameraForward.z;

        return dot > -0.25f;
    }
}

std::unique_ptr<CFxMeshSystem> CFxMeshSystem::Create(Engine::CFxStaticMeshRenderer* pRenderer)
{
    if (!pRenderer) return nullptr;
    auto p = std::unique_ptr<CFxMeshSystem>(new CFxMeshSystem());
    p->m_pRenderer = pRenderer;
    return p;
}

EntityID CFxMeshSystem::Spawn(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const FxMeshComponent& tmpl)
{
    if (!pRenderer || !tmpl.modelPath)
    {
        return NULL_ENTITY;
    }

    std::wstring texPath = tmpl.texturePath ? std::wstring(tmpl.texturePath) : std::wstring();
    std::wstring erodePath = tmpl.erodeTexturePath ? tmpl.erodeTexturePath : L"";
    if (!pRenderer->PreloadMesh(std::string(tmpl.modelPath), texPath, erodePath))
    {
        return NULL_ENTITY;
    }

    FxMeshComponent instance = tmpl;
    if (!instance.bMaterialReady)
        instance.RefreshMaterialFromLegacyFields();

    EntityID e = world.CreateEntity();
    world.AddComponent<FxMeshComponent>(e, instance);
    return e;
}

EntityID CFxMeshSystem::SpawnFromAsset(CWorld& world, Engine::CFxStaticMeshRenderer* pRenderer,
    const CFxAssetRegistry& registry, FxAssetHandle handle,
    const Vec3& vWorldPos, EntityID attachTo)
{
    const FxAsset* pAsset = registry.Find(handle);
    if (!pAsset || pAsset->emitters.empty())
        return NULL_ENTITY;

    EntityID firstEntity = NULL_ENTITY;
    for (u32_t i = 0; i < pAsset->emitters.size(); ++i)
    {
        const FxEmitterDesc& emitter = pAsset->emitters[i];
        if (emitter.renderType != eFxRenderType::MeshParticle)
            continue;

        FxMeshComponent mesh{};
        mesh.hAsset = handle;
        mesh.iEmitterIndex = i;
        mesh.vWorldPos = vWorldPos;
        mesh.attachTo = attachTo;
        mesh.vAttachOffset = emitter.vAttachOffset;
        mesh.anchor = emitter.anchor;
        mesh.lifecycle = emitter.lifecycle;
        mesh.vVelocity = emitter.vVelocity;
        mesh.vScale = emitter.vScale;
        mesh.vRotation = emitter.vRotation;
        mesh.fWorldYawSpinSpeed = emitter.fWorldYawSpinSpeed;
        mesh.SetModelPath(emitter.strModelPath);
        mesh.SetTexturePath(emitter.strTexturePath);
        mesh.SetErodeTexturePath(emitter.strErodeTexturePath);
        mesh.fLifetime = emitter.fLifetime;
        mesh.fStartDelay = emitter.fStartDelay;
        mesh.blendMode = emitter.blendMode;
        mesh.fFadeIn = emitter.fFadeIn;
        mesh.fFadeOut = emitter.fFadeOut;
        mesh.SetMaterialFromDesc(emitter.material, emitter.depthMode);
        mesh.bBlockableByWindWall = emitter.bBlockableByWindWall;

        const EntityID spawned = Spawn(world, pRenderer, mesh);
        if (firstEntity == NULL_ENTITY)
            firstEntity = spawned;
    }

    return firstEntity;
}

void CFxMeshSystem::Update(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("FxMesh::Update");

    std::vector<EntityID> vecDelete;

    world.ForEach<FxMeshComponent>(
        std::function<void(EntityID, FxMeshComponent&)>(
            [&](EntityID e, FxMeshComponent& m)
            {
                f32_t fEffectiveDelta = fTimeDelta;
                if (m.fStartDelay > 0.f)
                {
                    m.fStartDelay -= fTimeDelta;
                    if (m.fStartDelay > 0.f)
                        return;

                    fEffectiveDelta = -m.fStartDelay;
                    m.fStartDelay = 0.f;
                }

                m.fElapsed += fEffectiveDelta;

                //E 移쇰궇 ?뚯븘媛?꾨줉
                if (m.fWorldYawSpinSpeed != 0.f)
                {
                    m.fWorldYawSpin += m.fWorldYawSpinSpeed * fEffectiveDelta;
                    if (m.fWorldYawSpin > XM_2PI || m.fWorldYawSpin < -XM_2PI)
                        m.fWorldYawSpin = std::fmod(m.fWorldYawSpin, XM_2PI);
                }

                Vec3 vResolvedAnchor{};
                if (FxAnchor::TryResolveWorldPosition(
                    world,
                    m.attachTo,
                    m.anchor,
                    m.vAttachOffset,
                    m.vWorldPos,
                    vResolvedAnchor))
                {
                    m.vWorldPos = vResolvedAnchor;
                    m.bAnchorResolvedLastFrame = true;
                }
                else
                {
                    m.bAnchorResolvedLastFrame = false;
                    m.vWorldPos.x += m.vVelocity.x * fEffectiveDelta;
                    m.vWorldPos.y += m.vVelocity.y * fEffectiveDelta;
                    m.vWorldPos.z += m.vVelocity.z * fEffectiveDelta;
                }

                if (m.bPendingDelete || m.fElapsed >= m.fLifetime)
                    vecDelete.push_back(e);
            }));

    for (EntityID e : vecDelete) world.DestroyEntity(e);
}

void CFxMeshSystem::Render(CWorld& world, const CDynamicCamera* pCamera)
{
    WINTERS_PROFILE_SCOPE("FxMesh::Render");
    if (!pCamera || !m_pRenderer) return;

    const Vec3 vCamPos = pCamera->GetEye();
    const Vec3 vCamFwd = pCamera->GetForward();
    uint64_t drawCount = 0;
    uint64_t cullSkippedCount = 0;

    m_pRenderer->BeginFrame(pCamera->GetViewProjection(), vCamPos);

    world.ForEach<FxMeshComponent>(
        std::function<void(EntityID, FxMeshComponent&)>(
            [&](EntityID /*e*/, FxMeshComponent& m)
            {
                if (m.bPendingDelete || m.fLifetime <= 0.f) return;
                if (m.fStartDelay > 0.f) return;
                if (!m.modelPath) return;

                const f32_t boundsRadius =
                    (std::max)((std::max)(std::abs(m.vScale.x), std::abs(m.vScale.y)), std::abs(m.vScale.z));
                if (!IsFxMeshRenderRelevant(m.vWorldPos, vCamPos, vCamFwd, boundsRadius))
                {
                    ++cullSkippedCount;
                    return;
                }

                // World = S * RX * RY * RZ * T (row-vector, mul(v,M) HLSL convention).
                const XMMATRIX mS = XMMatrixScaling(m.vScale.x, m.vScale.y, m.vScale.z);
                const XMMATRIX mRX = XMMatrixRotationX(m.vRotation.x);
                const XMMATRIX mRY = XMMatrixRotationY(m.vRotation.y);
                const XMMATRIX mRZ = XMMatrixRotationZ(m.vRotation.z);
                const XMMATRIX mT = XMMatrixTranslation(m.vWorldPos.x, m.vWorldPos.y, m.vWorldPos.z);
                //移쇰궇 ?뚯쟾 ?쒗궎湲?
                const XMMATRIX mRBase = mRX * mRY * mRZ;
                const XMMATRIX mRSpin = XMMatrixRotationY(m.fWorldYawSpin);
                const XMMATRIX mWorld = mS * mRBase * mRSpin * mT;

                Mat4 world;
                XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&world.m), mWorld);

                // Fade alpha 怨꾩궛 (FxSystem 怨??숈씪)
                f32_t fadeAlpha = 1.f;
                if (m.fFadeIn > 0.f && m.fElapsed < m.fFadeIn)
                    fadeAlpha *= m.fElapsed / m.fFadeIn;
                if (m.fFadeOut > 0.f && m.fElapsed > (m.fLifetime - m.fFadeOut))
                    fadeAlpha *= (m.fLifetime - m.fElapsed) / m.fFadeOut;
                fadeAlpha = std::clamp(fadeAlpha, 0.f, 1.f);
                const f32_t normalizedAge = (m.fLifetime > 0.f)
                    ? std::clamp(m.fElapsed / m.fLifetime, 0.f, 1.f)
                    : 1.f;
                if (!m.bMaterialReady)
                    m.RefreshMaterialFromLegacyFields();

                // FxMeshDrawParams 梨꾩슦湲?(Bug D ??namespace Engine ??
                Engine::FxMeshDrawParams params;
                params.matWorld = world;
                params.pTexturePath = m.texturePath;
                params.pErodeTexturePath = m.erodeTexturePath;
                const FxMaterialDesc& material = m.material;
                params.vTint = {
                    material.vTint.x,
                    material.vTint.y,
                    material.vTint.z,
                    material.vTint.w * fadeAlpha
                };
                params.vUVRect = material.vUVRect;
                params.vUVScroll = {
                    material.vUVScroll.x * m.fElapsed,
                    material.vUVScroll.y * m.fElapsed
                };
                params.fAlphaClip = material.fAlphaClip;
                params.fErodeThreshold = material.fErodeThreshold;
                params.vStyleColorA = material.vStyleColorA;
                params.vStyleColorB = material.vStyleColorB;
                params.vRimColor = material.vRimColor;
                params.vStyleParams = {
                    static_cast<f32_t>(material.iStyleMode),
                    material.fRimPower,
                    material.fCellLow,
                    material.fCellHigh
                };
                params.vTimeParams = {
                    m.fElapsed,
                    normalizedAge,
                    material.fMaterialRandom,
                    0.f
                };
                params.vMagicScrollA = material.vMagicScrollA;
                params.vMagicShape = material.vMagicShape;
                params.vMagicCore = material.vMagicCore;
                params.iBlendPreset = static_cast<u32_t>(m.blendMode);
                // FX meshes follow the same overlay rule as sprite FX; champions still render elsewhere.
                params.depthMode = eFxDepthMode::OverlayNoDepth;
                params.bDepthWrite = false;

                m_pRenderer->DrawMesh(m.modelPath, params);
                ++drawCount;
            }));

    m_pRenderer->EndFrame();

    WINTERS_PROFILE_COUNT("FxMesh::Drawn", drawCount);
    WINTERS_PROFILE_COUNT("FxMesh::CullSkipped", cullSkippedCount);
}
