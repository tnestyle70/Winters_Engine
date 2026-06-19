#include "Renderer/ModelRenderer.h"
#include "WintersPaths.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "Renderer/CMaterialPBR.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"
#include "Resource/ResourceCache.h"
#include "Framework/CEngineApp.h"
#include "GameInstance.h"
#include "ProfilerAPI.h"
#include "Resource/Skeleton.h"
#include "Resource/Animation.h"
#include "Resource/Animator.h"

#include <cmath>
#include <cstdio>

using namespace Engine;

namespace
{
    constexpr f32_t kYawTraceHalfTurnTolerance = 0.35f;

    // GPU bone palette: structured buffer SRV (t8) so 512+ bone Elden Ring
    // rigs skin in one draw. Replaces the old 256/512 cbuffer palette.
    constexpr u32_t kMaxGPUBones = 1024;

    struct BoneMatrixSRVBuffer
    {
        ID3D11Buffer* pBuffer = nullptr;
        ID3D11ShaderResourceView* pSRV = nullptr;

        [[nodiscard]] bool Create(ID3D11Device* pDevice)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = sizeof(DirectX::XMFLOAT4X4) * kMaxGPUBones;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = sizeof(DirectX::XMFLOAT4X4);
            if (FAILED(pDevice->CreateBuffer(&desc, nullptr, &pBuffer)))
                return false;

            D3D11_SHADER_RESOURCE_VIEW_DESC srv{};
            srv.Format = DXGI_FORMAT_UNKNOWN;
            srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            srv.Buffer.FirstElement = 0;
            srv.Buffer.NumElements = kMaxGPUBones;
            return SUCCEEDED(pDevice->CreateShaderResourceView(pBuffer, &srv, &pSRV));
        }

        void Update(ID3D11DeviceContext* pContext, const std::vector<DirectX::XMFLOAT4X4>& matrices)
        {
            if (!pBuffer)
                return;
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(pContext->Map(pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                const u32_t count = min((u32_t)matrices.size(), kMaxGPUBones);
                memcpy(mapped.pData, matrices.data(), count * sizeof(DirectX::XMFLOAT4X4));
                pContext->Unmap(pBuffer, 0);
            }
        }

        void BindVS(ID3D11DeviceContext* pContext, UINT slot) const
        {
            pContext->VSSetShaderResources(slot, 1, &pSRV);
        }

        void Release()
        {
            if (pSRV) { pSRV->Release(); pSRV = nullptr; }
            if (pBuffer) { pBuffer->Release(); pBuffer = nullptr; }
        }
    };

    ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return nullptr;
        return static_cast<ID3D11Device*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
    }

    ID3D11DeviceContext* GetNativeDX11Context(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return nullptr;
        return static_cast<ID3D11DeviceContext*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
    }

    bool_t IsYawTraceHalfTurn(f32_t yawDelta)
    {
        return std::fabs(std::fabs(WintersMath::NormalizeRadians(yawDelta)) -
            WintersMath::kPi) <= kYawTraceHalfTurnTolerance;
    }

    bool_t ShouldLogAnimationName(const string& animName)
    {
        return animName.find("minion_") == string::npos;
    }

    void LogMissingAnimationName(const string& keyword)
    {
        if (keyword.find("minion_") != string::npos)
            return;

        static u32_t s_missingAnimationLogCount = 0;
        if (s_missingAnimationLogCount >= 128u)
            return;

        OutputDebugStringA(("[ModelRenderer] Missing animation keyword: "
            + keyword + "\n").c_str());
        ++s_missingAnimationLogCount;
    }

}

struct ModelRenderer::Impl
{
    // 공유 셰이더 / 파이프라인 (소유 X — CEngineApp 이 소유)
    DX11Shader*                         pSharedMeshShader = nullptr;
    DX11Pipeline*                       pSharedMeshPipeline = nullptr;
    DX11Shader*                         pSharedSkinnedShader = nullptr;
    DX11Pipeline*                       pSharedSkinnedPipeline = nullptr;

    // 인스턴스별 상수 버퍼 (데이터가 프레임/오브젝트마다 다르므로 인스턴스별)
    DX11ConstantBuffer<CBPerFrame>      cbPerFrame;
    DX11ConstantBuffer<CBPerObject>     cbPerObject;
    BoneMatrixSRVBuffer                 bonesSRV;
    unique_ptr<CMaterialPBR>            pMaterialPBR;

    // 공유 모델 데이터 (ResourceCache 경유)
    shared_ptr<CModel>                  pSharedModel;

    // 인스턴스별 애니메이션 상태 (Skeleton 만 공유, 시간은 개별)
    unique_ptr<CAnimator>               pInstanceAnimator;

    // 수동 텍스처 오버라이드 (인스턴스별)
    unique_ptr<CTexture>                pManualTexture;
    ID3D11ShaderResourceView*           pAmbientOcclusionSRV = nullptr;

    bool_t                              bUsePBR = false;
    bool_t                              bSkinnedReady = false;
    bool_t                              bForceStaticMeshPath = false;
    bool_t                              bReady = false;
    bool_t                              bMaterialOverrideEnabled = false;
    Vec4                                vMaterialOverrideColor{ 0.f, 0.f, 0.f, 0.f };
    Vec4                                vMaterialOverrideParams{ 0.f, 0.f, 0.f, 0.f };
    Mat4                                matWorld{};
    bool_t                              bHasWorldMatrix = false;
    bool_t                              bYawTraceEnabled = false;
    bool_t                              bYawTraceHasPrevWorldYaw = false;
    u64_t                               yawTraceSnapshotTick = 0;
    u32_t                               yawTraceEntity = 0;
    u32_t                               yawTraceChampion = 0;
    u32_t                               yawTraceCommandSeq = 0;
    f32_t                               yawTraceExpectedYaw = 0.f;
    f32_t                               yawTracePrevWorldYaw = 0.f;
    Vec3                                yawTraceExpectedForward{};

    // AABB (ImGui 디버깅 / 피킹)
    Vec3 m_vLocalAABBMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vec3 m_vLocalAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool m_bAABBValid = false;
};

ModelRenderer::ModelRenderer() : m_pImpl(new Impl()) {}
ModelRenderer::~ModelRenderer() { Shutdown(); delete m_pImpl; }

bool ModelRenderer::Initialize(const string& strFbxPath, const wchar_t* pHlslPath)
{
    auto& app = CEngineApp::Get();
    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    const std::wstring shaderPath = (pHlslPath != nullptr) ? pHlslPath : L"";

    if (!pDevice || !pNativeDevice)
        return false;

    m_pImpl->bUsePBR = (shaderPath.find(L"PBR") != std::wstring::npos);

    // ── 공유 Mesh3D 참조 ─────────────────────────────────────
    m_pImpl->pSharedMeshShader   = m_pImpl->bUsePBR ? app.GetMeshPBRShader() : app.GetMeshShader();
    m_pImpl->pSharedMeshPipeline = m_pImpl->bUsePBR ? app.GetMeshPBRPipeline() : app.GetMeshPipeline();

    if (!m_pImpl->pSharedMeshShader || !m_pImpl->pSharedMeshPipeline)
    {
        OutputDebugStringA("[ModelRenderer] Shared Mesh shader/pipeline NULL — CEngineApp 초기화 확인\n");
        return false;
    }

    // 상수 버퍼 (인스턴스별)
    if (!m_pImpl->cbPerFrame.Create(pNativeDevice))
        return false;
    if (!m_pImpl->cbPerObject.Create(pNativeDevice))
        return false;
    if (m_pImpl->bUsePBR)
    {
        m_pImpl->pMaterialPBR = CMaterialPBR::Create(pDevice);
        if (!m_pImpl->pMaterialPBR)
            return false;
    }

    // ── 모델 — ResourceCache 경유 공유 로드 ──────────────────
    m_pImpl->pSharedModel = app.GetResourceCache().LoadModel(pDevice, strFbxPath);
    if (!m_pImpl->pSharedModel)
        return false;
    if (m_pImpl->pSharedModel->HasValidAABB())
    {
        m_pImpl->m_vLocalAABBMin = m_pImpl->pSharedModel->GetLocalAABBMin();
        m_pImpl->m_vLocalAABBMax = m_pImpl->pSharedModel->GetLocalAABBMax();
        m_pImpl->m_bAABBValid = true;
    }

    // ── 인스턴스별 Animator + 스키닝 파이프라인 ──────────────
    if (m_pImpl->pSharedModel->HasSkeleton())
    {
        auto* pSkel = m_pImpl->pSharedModel->GetSkeleton();
        if (pSkel)
        {
            m_pImpl->pInstanceAnimator = CAnimator::Create(pSkel);
            // 첫 애니메이션 자동 재생
            if (m_pImpl->pSharedModel->GetAnimationCount() > 0)
            {
                auto* pFirst = m_pImpl->pSharedModel->GetAnimation(0);
                if (pFirst && m_pImpl->pInstanceAnimator)
                    m_pImpl->pInstanceAnimator->PlayAnimation(pFirst);
            }
        }

        // 공유 Skinned3D 참조 + 인스턴스 cbBones 만 생성
        m_pImpl->pSharedSkinnedShader   = m_pImpl->bUsePBR ? app.GetSkinnedPBRShader() : app.GetSkinnedShader();
        m_pImpl->pSharedSkinnedPipeline = m_pImpl->bUsePBR ? app.GetSkinnedPBRPipeline() : app.GetSkinnedPipeline();
        if (!m_pImpl->pSharedSkinnedShader || !m_pImpl->pSharedSkinnedPipeline ||
            !m_pImpl->bonesSRV.Create(pNativeDevice))
        {
            OutputDebugStringA("[ModelRenderer] Shared skinned shader/pipeline missing\n");
            return false;
        }
        m_pImpl->bSkinnedReady = true;
    }

    m_pImpl->bReady = true;
    return true;
}

bool ModelRenderer::PrewarmModel(const std::string& strFbxPath)
{
    auto& app = CEngineApp::Get();

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();

    if (!pDevice)
        return false;

    return app.GetResourceCache().LoadModel(pDevice, strFbxPath) != nullptr;
}

void ModelRenderer::SetYawTraceContext(
    u64_t snapshotTick,
    u32_t entity,
    u32_t champion,
    u32_t commandSeq,
    f32_t expectedYaw,
    const Vec3& expectedForward)
{
    m_pImpl->bYawTraceEnabled = true;
    m_pImpl->yawTraceSnapshotTick = snapshotTick;
    m_pImpl->yawTraceEntity = entity;
    m_pImpl->yawTraceChampion = champion;
    m_pImpl->yawTraceCommandSeq = commandSeq;
    m_pImpl->yawTraceExpectedYaw = expectedYaw;
    m_pImpl->yawTraceExpectedForward =
        WintersMath::NormalizeXZ(expectedForward, Vec3{}, 0.0001f);
}

void ModelRenderer::ClearYawTraceContext()
{
    m_pImpl->bYawTraceEnabled = false;
    m_pImpl->bYawTraceHasPrevWorldYaw = false;
    m_pImpl->yawTraceSnapshotTick = 0;
    m_pImpl->yawTraceEntity = 0;
    m_pImpl->yawTraceChampion = 0;
    m_pImpl->yawTraceCommandSeq = 0;
    m_pImpl->yawTraceExpectedYaw = 0.f;
    m_pImpl->yawTracePrevWorldYaw = 0.f;
    m_pImpl->yawTraceExpectedForward = {};
}

void ModelRenderer::SetForceStaticMeshPath(bool_t bEnabled)
{
    if (!m_pImpl)
        return;

    m_pImpl->bForceStaticMeshPath = bEnabled;
}

void ModelRenderer::UpdateTransform(const Mat4& matWorld)
{
    if (!m_pImpl->bReady) return;
    m_pImpl->matWorld = matWorld;
    m_pImpl->bHasWorldMatrix = true;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    auto* pContext = GetNativeDX11Context(pDevice);
    if (!pContext)
        return;

    CBPerObject data{};
    data.world = matWorld.m;
    const DirectX::XMMATRIX worldMatrix = matWorld.ToXMMATRIX();
    DirectX::XMStoreFloat4x4(&data.worldInvTranspose,
        DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, worldMatrix)));
    data.materialOverrideColor = m_pImpl->bMaterialOverrideEnabled
        ? m_pImpl->vMaterialOverrideColor.ToXMFLOAT4()
        : DirectX::XMFLOAT4{ 0.f, 0.f, 0.f, 0.f };
    data.vMaterialOverrideParams = m_pImpl->vMaterialOverrideParams.ToXMFLOAT4();
    m_pImpl->cbPerObject.Update(pContext, data);

    if (m_pImpl->bYawTraceEnabled)
    {
        static u32_t s_modelYawTraceCount = 0;
        if (s_modelYawTraceCount < 512u)
        {
            const Vec3 worldForward = WintersMath::NormalizeXZ(
                matWorld.TransformDirection(Vec3{ 0.f, 0.f, 1.f }),
                Vec3{},
                0.0001f);
            const Vec3 worldRight = WintersMath::NormalizeXZ(
                matWorld.TransformDirection(Vec3{ 1.f, 0.f, 0.f }),
                Vec3{},
                0.0001f);
            const Vec3 worldBack = WintersMath::NormalizeXZ(
                matWorld.TransformDirection(Vec3{ 0.f, 0.f, -1.f }),
                Vec3{},
                0.0001f);
            const f32_t worldYaw = WintersMath::YawFromDirectionXZ(
                worldForward,
                0.f,
                Vec3{},
                0.0001f);
            const f32_t worldDelta = m_pImpl->bYawTraceHasPrevWorldYaw
                ? WintersMath::NormalizeRadians(worldYaw - m_pImpl->yawTracePrevWorldYaw)
                : 0.f;
            const f32_t expectedDelta =
                WintersMath::NormalizeRadians(worldYaw - m_pImpl->yawTraceExpectedYaw);
            const f32_t worldVsExpectedDot =
                worldForward.x * m_pImpl->yawTraceExpectedForward.x +
                worldForward.z * m_pImpl->yawTraceExpectedForward.z;
            const f32_t rightVsExpectedDot =
                worldRight.x * m_pImpl->yawTraceExpectedForward.x +
                worldRight.z * m_pImpl->yawTraceExpectedForward.z;
            const f32_t backVsExpectedDot =
                worldBack.x * m_pImpl->yawTraceExpectedForward.x +
                worldBack.z * m_pImpl->yawTraceExpectedForward.z;
            const bool_t bHalfTurn = IsYawTraceHalfTurn(worldDelta);
            const bool_t bOpposesExpected = worldVsExpectedDot < -0.75f;

            const CAnimation* pCurrentAnimation =
                m_pImpl->pInstanceAnimator
                    ? m_pImpl->pInstanceAnimator->GetCurrentAnimation()
                    : nullptr;
            const char* pAnimName = pCurrentAnimation
                ? pCurrentAnimation->GetName().c_str()
                : "";
            const f32_t animTime = m_pImpl->pInstanceAnimator
                ? static_cast<f32_t>(m_pImpl->pInstanceAnimator->GetCurrentTime())
                : 0.f;
            const bool_t bAnimPlaying = m_pImpl->pInstanceAnimator
                ? m_pImpl->pInstanceAnimator->IsPlaying()
                : false;

            Vec3 rootForward{};
            f32_t rootYaw = 0.f;
            if (m_pImpl->pInstanceAnimator &&
                !m_pImpl->pInstanceAnimator->GetFinalBoneMatrices().empty())
            {
                const Mat4 rootMatrix(
                    m_pImpl->pInstanceAnimator->GetFinalBoneMatrices()[0]);
                rootForward = WintersMath::NormalizeXZ(
                    rootMatrix.TransformDirection(Vec3{ 0.f, 0.f, 1.f }),
                    Vec3{},
                    0.0001f);
                rootYaw = WintersMath::YawFromDirectionXZ(
                    rootForward,
                    0.f,
                    Vec3{},
                    0.0001f);
            }
        }
    }
}

void ModelRenderer::UpdateCamera(const Mat4& matViewProj)
{
    UpdateCamera(matViewProj, Vec3{});
}

void ModelRenderer::UpdateCamera(const Mat4& matViewProj, const Vec3& vCameraWorld)
{
    if (!m_pImpl->bReady) return;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    auto* pContext = GetNativeDX11Context(pDevice);
    if (!pContext)
        return;

    CBPerFrame data{};
    data.viewProjection = matViewProj.m;
    data.cameraWorld = vCameraWorld.ToXMFLOAT3();
    data.lightDirWorld = { -0.45f, -1.0f, 0.30f };
    data.lightIntensity = 1.65f;
    data.lightColor = { 1.0f, 0.98f, 0.92f };
    data.pointLightCount = 2u;
    data.pointLights[0] = { { -4.0f, 5.0f,  1.5f }, 9.0f, { 0.95f, 0.44f, 0.34f }, 1.15f };
    data.pointLights[1] = { {  4.0f, 5.0f, -1.5f }, 9.0f, { 0.34f, 0.52f, 1.00f }, 1.15f };
    data.pointLights[2] = { { 12.0f, 5.0f,  2.0f }, 12.0f, { 0.40f, 1.00f, 0.55f }, 0.0f };
    data.pointLights[3] = { { 20.0f, 5.0f,  0.0f }, 12.0f, { 1.00f, 0.85f, 0.30f }, 0.0f };
    data.screenSize = {
        static_cast<f32_t>(CEngineApp::Get().GetWindow().GetWidth()),
        static_cast<f32_t>(CEngineApp::Get().GetWindow().GetHeight())
    };
    m_pImpl->cbPerFrame.Update(pContext, data);
}

void ModelRenderer::Render()
{
    const VisibilityMask mask = MakeAllVisibleMask();
    RenderWithVisibility(mask);
}

void ModelRenderer::RenderFrustumCulled(const Mat4& matViewProj)
{
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel)
        return;

    if (!m_pImpl->bHasWorldMatrix)
    {
        Render();
        return;
    }

    const Mat4 matLocalToClip(
        m_pImpl->matWorld.ToXMMATRIX() * matViewProj.ToXMMATRIX());
    bool_t bAnyVisible = true;
    VisibilityMask mask{};
    {
        WINTERS_PROFILE_SCOPE("ModelRenderer::BuildClipVisibilityMask");
        mask = m_pImpl->pSharedModel->BuildClipVisibilityMask(matLocalToClip, &bAnyVisible);
    }
    if (!bAnyVisible)
        return;

    RenderWithVisibility(mask);
}

void ModelRenderer::RenderWithVisibility(const VisibilityMask& mask)
{
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel) return;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    auto* pContext = GetNativeDX11Context(pDevice);
    if (!pContext)
        return;
    ID3D11ShaderResourceView* pAmbientOcclusionSRV = m_pImpl->pAmbientOcclusionSRV;

    // ── 스키닝 렌더링 ──
    const bool_t bUseSkinnedPath =
        !m_pImpl->bForceStaticMeshPath &&
        m_pImpl->bSkinnedReady &&
        m_pImpl->pSharedModel->HasSkeleton();

    if (bUseSkinnedPath)
    {
        WINTERS_PROFILE_SCOPE("ModelRenderer::RenderSkinned");

        if (m_pImpl->pInstanceAnimator)
        {
            m_pImpl->bonesSRV.Update(pContext,
                m_pImpl->pInstanceAnimator->GetFinalBoneMatrices());
        }

        m_pImpl->pSharedSkinnedShader->Bind(pContext);
        m_pImpl->pSharedSkinnedPipeline->Bind(pContext);
        if (m_pImpl->bUsePBR)
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
            if (m_pImpl->pMaterialPBR)
                m_pImpl->pMaterialPBR->Bind(pDevice);
        }
        else
        {
            m_pImpl->cbPerFrame.Bind(pContext, 0);
        }
        if (pAmbientOcclusionSRV)
            pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);

        m_pImpl->cbPerObject.Bind(pContext, 1);
        m_pImpl->bonesSRV.BindVS(pContext, 8);

        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);
        if (pAmbientOcclusionSRV)
        {
            ID3D11ShaderResourceView* pNullSRV = nullptr;
            pContext->PSSetShaderResources(5, 1, &pNullSRV);
        }
        m_pImpl->pSharedSkinnedShader->Unbind(pContext);
        return;
    }

    // ── 정적 메시 렌더링 ──
    WINTERS_PROFILE_SCOPE("ModelRenderer::RenderStatic");

    m_pImpl->pSharedMeshShader->Bind(pContext);
    m_pImpl->pSharedMeshPipeline->Bind(pContext);
    if (m_pImpl->bUsePBR)
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
        if (m_pImpl->pMaterialPBR)
            m_pImpl->pMaterialPBR->Bind(pDevice);
    }
    else
    {
        m_pImpl->cbPerFrame.Bind(pContext, 0);
    }
    if (pAmbientOcclusionSRV)
        pContext->PSSetShaderResources(5, 1, &pAmbientOcclusionSRV);

    m_pImpl->cbPerObject.Bind(pContext, 1);

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);
    if (pAmbientOcclusionSRV)
    {
        ID3D11ShaderResourceView* pNullSRV = nullptr;
        pContext->PSSetShaderResources(5, 1, &pNullSRV);
    }
    m_pImpl->pSharedMeshShader->Unbind(pContext);
}

void ModelRenderer::RenderNormalPass(DX11Shader* pMeshShader,
    DX11Pipeline* pMeshPipeline,
    DX11Shader* pSkinnedShader,
    DX11Pipeline* pSkinnedPipeline)
{
    const VisibilityMask mask = MakeAllVisibleMask();
    RenderNormalPassWithVisibility(pMeshShader, pMeshPipeline, pSkinnedShader, pSkinnedPipeline, mask);
}

void ModelRenderer::RenderNormalPassFrustumCulled(DX11Shader* pMeshShader,
    DX11Pipeline* pMeshPipeline,
    DX11Shader* pSkinnedShader,
    DX11Pipeline* pSkinnedPipeline,
    const Mat4& matViewProj)
{
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel)
        return;

    if (!m_pImpl->bHasWorldMatrix)
    {
        RenderNormalPass(pMeshShader, pMeshPipeline, pSkinnedShader, pSkinnedPipeline);
        return;
    }

    const Mat4 matLocalToClip(
        m_pImpl->matWorld.ToXMMATRIX() * matViewProj.ToXMMATRIX());
    bool_t bAnyVisible = true;
    VisibilityMask mask{};
    {
        WINTERS_PROFILE_SCOPE("ModelRenderer::BuildClipVisibilityMask");
        mask = m_pImpl->pSharedModel->BuildClipVisibilityMask(matLocalToClip, &bAnyVisible);
    }
    if (!bAnyVisible)
        return;

    RenderNormalPassWithVisibility(
        pMeshShader,
        pMeshPipeline,
        pSkinnedShader,
        pSkinnedPipeline,
        mask);
}

void ModelRenderer::RenderNormalPassWithVisibility(DX11Shader* pMeshShader,
    DX11Pipeline* pMeshPipeline,
    DX11Shader* pSkinnedShader,
    DX11Pipeline* pSkinnedPipeline,
    const VisibilityMask& mask)
{
    if (!m_pImpl->bReady || !m_pImpl->pSharedModel)
        return;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    auto* pContext = GetNativeDX11Context(pDevice);
    if (!pContext)
        return;

    const bool_t bUseSkinnedPath =
        !m_pImpl->bForceStaticMeshPath &&
        m_pImpl->bSkinnedReady &&
        m_pImpl->pSharedModel->HasSkeleton();

    if (bUseSkinnedPath)
    {
        WINTERS_PROFILE_SCOPE("ModelRenderer::RenderSkinned");

        if (!pSkinnedShader || !pSkinnedPipeline)
            return;

        if (m_pImpl->pInstanceAnimator)
        {
            m_pImpl->bonesSRV.Update(pContext,
                m_pImpl->pInstanceAnimator->GetFinalBoneMatrices());
        }

        pSkinnedShader->Bind(pContext);
        pSkinnedPipeline->Bind(pContext);
        m_pImpl->cbPerFrame.BindVS(pContext, 0);
        m_pImpl->cbPerObject.BindVS(pContext, 1);
        m_pImpl->bonesSRV.BindVS(pContext, 8);
        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);
        pSkinnedShader->Unbind(pContext);
        return;
    }

    if (!pMeshShader || !pMeshPipeline)
        return;

    WINTERS_PROFILE_SCOPE("ModelRenderer::RenderStatic");

    pMeshShader->Bind(pContext);
    pMeshPipeline->Bind(pContext);
    m_pImpl->cbPerFrame.BindVS(pContext, 0);
    m_pImpl->cbPerObject.BindVS(pContext, 1);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImpl->pSharedModel->RenderWithMask(pDevice, mask);
    pMeshShader->Unbind(pContext);
}

bool ModelRenderer::LoadTexture(const wstring& strPath)
{
    if (!m_pImpl) return false;

    IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
    m_pImpl->pManualTexture = CTexture::Create(
        pDevice,
        strPath,
        eTexSamplerMode::Wrap,
        eTexColorSpace::ShaderLocalSRGB);

    if (m_pImpl->pManualTexture)
    {
        // ⚠️ 공유 CModel 의 Override 는 모든 인스턴스에 영향.
        //    인스턴스별 텍스처 오버라이드가 필요해지면 Phase 3+ 리팩터링.
        if (m_pImpl->pSharedModel)
            m_pImpl->pSharedModel->SetOverrideTexture(m_pImpl->pManualTexture.get());

        OutputDebugStringA("[ModelRenderer] Texture loaded OK\n");
        return true;
    }
    OutputDebugStringA("[ModelRenderer] Texture load FAILED\n");
    return false;
}

void ModelRenderer::Update(f32_t fDeltaTime)
{
    if (!m_pImpl->bReady) return;

    if (m_pImpl->pInstanceAnimator)
        m_pImpl->pInstanceAnimator->Update(fDeltaTime);
}

void ModelRenderer::PlayAnimation(uint32 iIndex)
{
    if (!m_pImpl->pSharedModel || !m_pImpl->pInstanceAnimator) return;

    auto* pAnim = m_pImpl->pSharedModel->GetAnimation(iIndex);
    if (pAnim)
        m_pImpl->pInstanceAnimator->PlayAnimation(pAnim);
}

void ModelRenderer::PlayAnimationByName(const string& strKeyword)
{
    PlayAnimationByNameAdvanced(strKeyword, true, false, 1.f);
}

void ModelRenderer::PlayAnimationByName(const string& strKeyword, bool bLoop)
{
    PlayAnimationByNameAdvanced(strKeyword, bLoop, false, 1.f);
}

bool ModelRenderer::HasAnimationByName(const string& strKeyword) const
{
    return m_pImpl &&
        m_pImpl->pSharedModel &&
        m_pImpl->pSharedModel->FindAnimationIndex(strKeyword) >= 0;
}

f32_t ModelRenderer::GetAnimationDurationSecondsByName(const string& strKeyword) const
{
    if (!m_pImpl || !m_pImpl->pSharedModel)
        return 0.f;

    const i32_t idx = m_pImpl->pSharedModel->FindAnimationIndex(strKeyword);
    if (idx < 0)
        return 0.f;

    const auto* pAnim = m_pImpl->pSharedModel->GetAnimation((u32_t)idx);
    if (!pAnim || pAnim->GetTicksPerSecond() <= 0.0)
        return 0.f;

    return static_cast<f32_t>(pAnim->GetDuration() / pAnim->GetTicksPerSecond());
}

bool ModelRenderer::PlayAnimationByNameAdvanced(
    const string& strKeyword,
    bool bLoop,
    bool_t bReverse,
    f32_t fPlaySpeed)
{
    if (!m_pImpl->pSharedModel || !m_pImpl->pInstanceAnimator)
        return false;

    const i32_t idx = m_pImpl->pSharedModel->FindAnimationIndex(strKeyword);
    if (idx < 0)
    {
        LogMissingAnimationName(strKeyword);
        return false;
    }

    auto* pAnim = m_pImpl->pSharedModel->GetAnimation((u32_t)idx);
    if (!pAnim)
        return false;

    f32_t speed = (fPlaySpeed < 0.f) ? -fPlaySpeed : fPlaySpeed;
    if (speed < 0.01f)
        speed = 1.f;
    if (bReverse)
        speed = -speed;

    const f64_t startTime = bReverse ? pAnim->GetDuration() : 0.0;
    m_pImpl->pInstanceAnimator->PlayAnimation(pAnim, bLoop, startTime, speed);

    if (ShouldLogAnimationName(pAnim->GetName()))
    {
        OutputDebugStringA((string("[ModelRenderer] Playing (loop=")
            + (bLoop ? "true" : "false")
            + ", reverse=" + (bReverse ? "true" : "false") + "): "
            + pAnim->GetName() + "\n").c_str());
    }

    return true;
}

bool ModelRenderer::HasSkeleton() const
{
    return m_pImpl && m_pImpl->pSharedModel && m_pImpl->pSharedModel->HasSkeleton();
}

bool ModelRenderer::TryResolveBoneWorldPosition(const string& strBoneName,
    const Mat4& matEntityWorld,
    const Vec3& vLocalOffset,
    Vec3& vOutWorldPos) const
{
    if (strBoneName.empty() || !m_pImpl || !m_pImpl->pInstanceAnimator)
        return false;

    XMFLOAT4X4 matBoneGlobal{};
    if (!m_pImpl->pInstanceAnimator->TryGetBoneGlobalTransform(strBoneName, matBoneGlobal))
        return false;

    const Mat4 matBoneLocal(matBoneGlobal);
    vOutWorldPos = (matBoneLocal * matEntityWorld).TransformPoint(vLocalOffset);
    return true;
}

bool ModelRenderer::UsesPBR() const
{
    return m_pImpl ? m_pImpl->bUsePBR : false;
}

void ModelRenderer::SetAmbientOcclusionSRV(void* pNativeSRV)
{
    if (!m_pImpl)
        return;
    m_pImpl->pAmbientOcclusionSRV = static_cast<ID3D11ShaderResourceView*>(pNativeSRV);
}

void ModelRenderer::SetMaterialOverrideColor(const Vec4& color, bool_t bEnabled)
{
    if (!m_pImpl)
        return;

    m_pImpl->vMaterialOverrideColor = color;
    m_pImpl->bMaterialOverrideEnabled = bEnabled;
}

void ModelRenderer::ClearMaterialOverrideColor()
{
    if (!m_pImpl)
        return;

    m_pImpl->vMaterialOverrideColor = Vec4{ 0.f, 0.f, 0.f, 0.f };
    m_pImpl->bMaterialOverrideEnabled = false;
}

void ModelRenderer::SetHoverOutline(const Vec4& color, f32_t fIntensity)
{
    if (!m_pImpl)
        return;

    m_pImpl->vMaterialOverrideParams = Vec4{
        color.x,
        color.y,
        color.z,
        (fIntensity < 0.f) ? 0.f : fIntensity
    };
}

void ModelRenderer::ClearHoverOutline()
{
    if (!m_pImpl)
        return;

    m_pImpl->vMaterialOverrideParams = Vec4{ 0.f, 0.f, 0.f, 0.f };
}

uint32 ModelRenderer::GetAnimationCount() const
{
    if (m_pImpl && m_pImpl->pSharedModel)
        return m_pImpl->pSharedModel->GetAnimationCount();
    return 0;
}

// [Phase T] 프레임 이벤트 감지용 접근자
const CAnimator* ModelRenderer::GetAnimator() const
{
    return (m_pImpl ? m_pImpl->pInstanceAnimator.get() : nullptr);
}

Engine::CAnimator* ModelRenderer::GetAnimator()
{
    //이거 시그니처 안 맞는데 이유 뭐임? 애초에 Animator 쪽도 안 맞았음 루트 맞음?
    return m_pImpl ? m_pImpl->pInstanceAnimator.get() : nullptr;
}

bool ModelRenderer::HasValidAABB() const
{
    return m_pImpl ? m_pImpl->m_bAABBValid : false;
}

Vec3 ModelRenderer::GetLocalAABBMin() const
{
    return (m_pImpl && m_pImpl->m_bAABBValid) ? m_pImpl->m_vLocalAABBMin : Vec3{ 0.f, 0.f, 0.f };
}

Vec3 ModelRenderer::GetLocalAABBMax() const
{
    return (m_pImpl && m_pImpl->m_bAABBValid) ? m_pImpl->m_vLocalAABBMax : Vec3{ 0.f, 0.f, 0.f };
}

void ModelRenderer::Shutdown()
{
    if (m_pImpl)
    {
        // 인스턴스 소유 리소스만 해제
        m_pImpl->pSharedModel.reset();        // shared_ptr 참조 감소 (ResourceCache 가 여전히 보유)
        m_pImpl->pInstanceAnimator.reset();
        m_pImpl->pMaterialPBR.reset();
        m_pImpl->pManualTexture.reset();

        m_pImpl->cbPerFrame.Release();
        m_pImpl->cbPerObject.Release();
        m_pImpl->bonesSRV.Release();

        // 공유 셰이더/파이프라인 포인터는 nullptr 만 (소유권 없음, CEngineApp 이 해제)
        m_pImpl->pSharedMeshShader = nullptr;
        m_pImpl->pSharedMeshPipeline = nullptr;
        m_pImpl->pSharedSkinnedShader = nullptr;
        m_pImpl->pSharedSkinnedPipeline = nullptr;

        m_pImpl->bUsePBR = false;
        m_pImpl->bSkinnedReady = false;
        m_pImpl->bForceStaticMeshPath = false;
        m_pImpl->bReady = false;
        m_pImpl->bHasWorldMatrix = false;
        m_pImpl->bMaterialOverrideEnabled = false;
        m_pImpl->vMaterialOverrideColor = Vec4{ 0.f, 0.f, 0.f, 0.f };
        m_pImpl->vMaterialOverrideParams = Vec4{ 0.f, 0.f, 0.f, 0.f };
        m_pImpl->m_vLocalAABBMin = { FLT_MAX, FLT_MAX, FLT_MAX };
        m_pImpl->m_vLocalAABBMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        m_pImpl->m_bAABBValid = false;
    }
}

bool ModelRenderer::LoadMeshTexture(uint32_t iMeshIndex, const wstring& strPath)
{
    if (!m_pImpl || !m_pImpl->pSharedModel) return false;

    CTexture* pTex = CEngineApp::Get().GetResourceCache().LoadTexture(
        strPath,
        eTexColorSpace::ShaderLocalSRGB);
    if (!pTex) return false;

    m_pImpl->pSharedModel->SetMeshTexture(iMeshIndex, pTex);
    OutputDebugStringA(("[ModelRenderer] Mesh " + to_string(iMeshIndex) + " texture set\n").c_str());
    return true;
}

void ModelRenderer::LoadTextureForAllMeshes(const wstring& strPath)
{
    if (!m_pImpl || !m_pImpl->pSharedModel) return;

    u32_t meshCount = m_pImpl->pSharedModel->GetMeshCount();
    for (u32_t i = 0; i < meshCount; ++i)
        LoadMeshTexture(i, strPath);
}

uint32 ModelRenderer::GetMeshCount() const
{
    if (m_pImpl && m_pImpl->pSharedModel)
        return m_pImpl->pSharedModel->GetMeshCount();
    return 0;
}
