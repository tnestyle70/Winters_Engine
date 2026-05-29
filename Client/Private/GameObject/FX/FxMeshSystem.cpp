#include "GameObject/FX/FxMeshSystem.h"
#include "ECS/World.h"
#include "ECS/Components/TransformComponent.h"
#include "Renderer/FxStaticMeshRenderer.h"
#include "DynamicCamera.h"
#include "ProfilerAPI.h"
#include <DirectXMath.h>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace DirectX;

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
        ::OutputDebugStringA("[CFxMeshSystem::Spawn] renderer/modelPath null - abort\n");
        return NULL_ENTITY;
    }

    std::wstring texPath = tmpl.texturePath ? std::wstring(tmpl.texturePath) : std::wstring();
    std::wstring erodePath = tmpl.erodeTexturePath ? tmpl.erodeTexturePath : L"";
    if (!pRenderer->PreloadMesh(std::string(tmpl.modelPath), texPath, erodePath))
    {
        ::OutputDebugStringA(("[CFxMeshSystem::Spawn] PreloadMesh fail - abort spawn for "
            + std::string(tmpl.modelPath) + "\n").c_str());
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

                //E 칼날 돌아가도록
                if (m.fWorldYawSpinSpeed != 0.f)
                {
                    m.fWorldYawSpin += m.fWorldYawSpinSpeed * fEffectiveDelta;
                    if (m.fWorldYawSpin > XM_2PI || m.fWorldYawSpin < -XM_2PI)
                        m.fWorldYawSpin = std::fmod(m.fWorldYawSpin, XM_2PI);
                }

                if (m.attachTo != NULL_ENTITY
                    && world.HasComponent<TransformComponent>(m.attachTo))
                {
                    const Vec3& tp = world.GetComponent<TransformComponent>(m.attachTo).m_LocalPosition;
                    m.vWorldPos = { tp.x + m.vAttachOffset.x,
                                    tp.y + m.vAttachOffset.y,
                                    tp.z + m.vAttachOffset.z };
                }
                else
                {
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

    m_pRenderer->BeginFrame(pCamera->GetViewProjection(), pCamera->GetEye());

    world.ForEach<FxMeshComponent>(
        std::function<void(EntityID, FxMeshComponent&)>(
            [&](EntityID /*e*/, FxMeshComponent& m)
            {
                if (m.bPendingDelete || m.fLifetime <= 0.f) return;
                if (m.fStartDelay > 0.f) return;
                if (!m.modelPath) return;

                // World = S * RX * RY * RZ * T (row-vector, mul(v,M) HLSL convention).
                const XMMATRIX mS = XMMatrixScaling(m.vScale.x, m.vScale.y, m.vScale.z);
                const XMMATRIX mRX = XMMatrixRotationX(m.vRotation.x);
                const XMMATRIX mRY = XMMatrixRotationY(m.vRotation.y);
                const XMMATRIX mRZ = XMMatrixRotationZ(m.vRotation.z);
                const XMMATRIX mT = XMMatrixTranslation(m.vWorldPos.x, m.vWorldPos.y, m.vWorldPos.z);
                //칼날 회전 시키기
                const XMMATRIX mRBase = mRX * mRY * mRZ;
                const XMMATRIX mRSpin = XMMatrixRotationY(m.fWorldYawSpin);
                const XMMATRIX mWorld = mS * mRBase * mRSpin * mT;

                Mat4 world;
                XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&world.m), mWorld);

                // Fade alpha 계산 (FxSystem 과 동일)
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

                // FxMeshDrawParams 채우기 (Bug D — namespace Engine 안)
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

                m_pRenderer->DrawMesh(m.modelPath, params);   // ★ const char* + POD
            }));

    m_pRenderer->EndFrame();
}
