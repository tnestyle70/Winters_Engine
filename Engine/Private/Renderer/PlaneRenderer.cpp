#include "Renderer/PlaneRenderer.h"

#include "RHI/RHITypes.h"
#include "RHI/DX11/BlendStateCache.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "Resource/Texture.h"
#include "RHI/DX11/DX11ConstantBuffer.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstring>

namespace
{
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

    struct VtxMesh
    {
        f32_t px, py, pz;
        f32_t nx, ny, nz;
        f32_t u, v;
        f32_t tx, ty, tz;
    };

    struct PlaneCBPerFrame
    {
        DirectX::XMFLOAT4X4 VP;
    };

    struct PlaneCBPerObject
    {
        DirectX::XMFLOAT4X4 World;
    };
}

struct CPlaneRenderer::Impl
{
    Microsoft::WRL::ComPtr<ID3D11Buffer> pVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pIB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pCBPerFrame;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pCBPerObject;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pCBFxParams;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDSSNoWrite;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDSSNoDepth;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> pRSTwoSided;

    DX11Shader* pSharedShader = nullptr;
    DX11Pipeline* pSharedPipeline = nullptr;
};

CPlaneRenderer::CPlaneRenderer()
    : m_pImpl(std::make_unique<Impl>())
{
    DirectX::XMStoreFloat4x4(
        reinterpret_cast<DirectX::XMFLOAT4X4*>(&m_World.m),
        DirectX::XMMatrixIdentity());
    ResetFxParams();
}

CPlaneRenderer::~CPlaneRenderer() = default;

std::unique_ptr<CPlaneRenderer> CPlaneRenderer::Create(
    IRHIDevice* pDevice,
    DX11Shader* pMeshShader,
    DX11Pipeline* pMeshPipeline)
{
    auto pRenderer = std::unique_ptr<CPlaneRenderer>(new CPlaneRenderer());
    if (FAILED(pRenderer->Initialize(pDevice, pMeshShader, pMeshPipeline)))
        return nullptr;
    return pRenderer;
}

void CPlaneRenderer::SetTexture(Engine::CTexture* pTex)
{
    m_pTexture = pTex;
}

void CPlaneRenderer::SetWorld(const Mat4& world)
{
    m_World = world;
}

void CPlaneRenderer::SetBlendCache(CBlendStateCache* pCache, eBlendPreset ePreset)
{
    m_pBlendCache = pCache;
    m_eBlend = ePreset;
}

void CPlaneRenderer::SetDepthMode(eFxDepthMode eMode)
{
    m_eDepthMode = eMode;
}

HRESULT CPlaneRenderer::Initialize(IRHIDevice* pDevice,
    DX11Shader* pMeshShader,
    DX11Pipeline* pMeshPipeline)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    if (!pNativeDevice || !pMeshShader || !pMeshPipeline)
        return E_INVALIDARG;

    m_pImpl->pSharedShader = pMeshShader;
    m_pImpl->pSharedPipeline = pMeshPipeline;

    const VtxMesh verts[4] = {
        { -0.5f, 0.f, -0.5f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f },
        {  0.5f, 0.f, -0.5f, 0.f, 1.f, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f },
        {  0.5f, 0.f,  0.5f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 0.f },
        { -0.5f, 0.f,  0.5f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f },
    };
    const uint16_t idx[6] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(verts);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbInit = { verts, 0, 0 };
    if (FAILED(pNativeDevice->CreateBuffer(&vbDesc, &vbInit, m_pImpl->pVB.GetAddressOf())))
        return E_FAIL;

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(idx);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA ibInit = { idx, 0, 0 };
    if (FAILED(pNativeDevice->CreateBuffer(&ibDesc, &ibInit, m_pImpl->pIB.GetAddressOf())))
        return E_FAIL;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = sizeof(PlaneCBPerFrame);
    if (FAILED(pNativeDevice->CreateBuffer(&cbDesc, nullptr, m_pImpl->pCBPerFrame.GetAddressOf())))
        return E_FAIL;

    cbDesc.ByteWidth = sizeof(PlaneCBPerObject);
    if (FAILED(pNativeDevice->CreateBuffer(&cbDesc, nullptr, m_pImpl->pCBPerObject.GetAddressOf())))
        return E_FAIL;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dsd.StencilEnable = FALSE;
    if (FAILED(pNativeDevice->CreateDepthStencilState(&dsd, m_pImpl->pDSSNoWrite.GetAddressOf())))
        return E_FAIL;

    D3D11_DEPTH_STENCIL_DESC noDepthDsd = {};
    noDepthDsd.DepthEnable = FALSE;
    noDepthDsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    noDepthDsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    noDepthDsd.StencilEnable = FALSE;
    if (FAILED(pNativeDevice->CreateDepthStencilState(&noDepthDsd, m_pImpl->pDSSNoDepth.GetAddressOf())))
        return E_FAIL;

    D3D11_RASTERIZER_DESC rsd = {};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.FrontCounterClockwise = FALSE;
    rsd.DepthClipEnable = TRUE;
    if (FAILED(pNativeDevice->CreateRasterizerState(&rsd, m_pImpl->pRSTwoSided.GetAddressOf())))
        return E_FAIL;

    cbDesc.ByteWidth = sizeof(CBFxParams);
    if (FAILED(pNativeDevice->CreateBuffer(&cbDesc, nullptr, m_pImpl->pCBFxParams.GetAddressOf())))
        return E_FAIL;

    return S_OK;
}

void CPlaneRenderer::Render(IRHIDevice* pDevice, const Mat4& matViewProj)
{
    ID3D11DeviceContext* pContext = GetNativeDX11Context(pDevice);

    if (!pContext || !m_pImpl->pSharedShader || !m_pImpl->pSharedPipeline)
        return;

    using Microsoft::WRL::ComPtr;

    const bool bAlpha = (m_pBlendCache != nullptr);
    ComPtr<ID3D11BlendState> pPrevBS;
    FLOAT prevBF[4] = { 0, 0, 0, 0 };
    UINT prevMask = 0;
    ComPtr<ID3D11DepthStencilState> pPrevDSS;
    UINT prevStencil = 0;
    ComPtr<ID3D11RasterizerState> pPrevRS;

    if (bAlpha)
    {
        pContext->OMGetBlendState(pPrevBS.GetAddressOf(), prevBF, &prevMask);
        pContext->OMGetDepthStencilState(pPrevDSS.GetAddressOf(), &prevStencil);
        pContext->RSGetState(pPrevRS.GetAddressOf());
        m_pBlendCache->Bind(pContext, m_eBlend);

        ID3D11DepthStencilState* pDepthState = m_pImpl->pDSSNoWrite.Get();
        if (m_eDepthMode == eFxDepthMode::OverlayNoDepth && m_pImpl->pDSSNoDepth)
            pDepthState = m_pImpl->pDSSNoDepth.Get();

        pContext->OMSetDepthStencilState(pDepthState, 0);
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(pContext->Map(m_pImpl->pCBPerFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        reinterpret_cast<PlaneCBPerFrame*>(mapped.pData)->VP = matViewProj.m;
        pContext->Unmap(m_pImpl->pCBPerFrame.Get(), 0);
    }

    if (SUCCEEDED(pContext->Map(m_pImpl->pCBPerObject.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        reinterpret_cast<PlaneCBPerObject*>(mapped.pData)->World = m_World.m;
        pContext->Unmap(m_pImpl->pCBPerObject.Get(), 0);
    }

    m_pImpl->pSharedShader->Bind(pContext);
    m_pImpl->pSharedPipeline->Bind(pContext);

    if (bAlpha && m_pImpl->pRSTwoSided)
        pContext->RSSetState(m_pImpl->pRSTwoSided.Get());

    ID3D11Buffer* pCB[2] = { m_pImpl->pCBPerFrame.Get(), m_pImpl->pCBPerObject.Get() };
    pContext->VSSetConstantBuffers(0, 2, pCB);

    D3D11_MAPPED_SUBRESOURCE mappedFx = {};
    if (SUCCEEDED(pContext->Map(m_pImpl->pCBFxParams.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedFx)))
    {
        const CBFxParams data = m_FxParams;
        memcpy(mappedFx.pData, &data, sizeof(CBFxParams));
        pContext->Unmap(m_pImpl->pCBFxParams.Get(), 0);
    }

    ID3D11Buffer* pFxCB = m_pImpl->pCBFxParams.Get();
    pContext->VSSetConstantBuffers(2, 1, &pFxCB);
    pContext->PSSetConstantBuffers(2, 1, &pFxCB);

    if (m_pTexture)
        m_pTexture->Bind(pDevice, 0);

    UINT stride = sizeof(VtxMesh);
    UINT offset = 0;
    ID3D11Buffer* pVertexBuffer = m_pImpl->pVB.Get();
    pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);
    pContext->IASetIndexBuffer(m_pImpl->pIB.Get(), DXGI_FORMAT_R16_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->DrawIndexed(6, 0, 0);

    m_pImpl->pSharedShader->Unbind(pContext);

    if (bAlpha)
    {
        pContext->OMSetBlendState(pPrevBS.Get(), prevBF, prevMask);
        pContext->OMSetDepthStencilState(pPrevDSS.Get(), prevStencil);
        pContext->RSSetState(pPrevRS.Get());
    }
}

void CPlaneRenderer::SetFxParams(const CBFxParams& params)
{
    m_FxParams = params;
    m_vFxTint = { params.vTint.x, params.vTint.y, params.vTint.z, params.vTint.w };
    m_vFxUVRect = { params.vUVRect.x, params.vUVRect.y, params.vUVRect.z, params.vUVRect.w };
    m_vFxUVScroll = { params.vUVScroll.x, params.vUVScroll.y };
    m_fFxAlphaClip = params.fAlphaClip;
    m_fFxErodeThreshold = params.fErodeThreshold;
}

void CPlaneRenderer::SetFxParams(const Vec4& vTint,
    const Vec4& vUVRect,
    const Vec2& vUVScroll,
    f32_t fAlphaClip,
    f32_t fErodeThreshold)
{
    m_vFxTint = vTint;
    m_vFxUVRect = vUVRect;
    m_vFxUVScroll = vUVScroll;
    m_fFxAlphaClip = fAlphaClip;
    m_fFxErodeThreshold = fErodeThreshold;

    m_FxParams = MakeFxParamsFromMaterial(
        FxMaterialDesc{},
        vTint,
        vUVRect,
        vUVScroll,
        0.f,
        0.f);
    m_FxParams.fAlphaClip = fAlphaClip;
    m_FxParams.fErodeThreshold = fErodeThreshold;
}

void CPlaneRenderer::ResetFxParams()
{
    m_vFxTint = { 1.f, 1.f, 1.f, 1.f };
    m_vFxUVRect = { 0.f, 0.f, 1.f, 1.f };
    m_vFxUVScroll = { 0.f, 0.f };
    m_fFxAlphaClip = 0.05f;
    m_fFxErodeThreshold = 0.f;
    m_FxParams = MakeFxParamsFromMaterial(
        FxMaterialDesc{},
        m_vFxTint,
        m_vFxUVRect,
        m_vFxUVScroll,
        0.f,
        0.f);
    m_FxParams.fAlphaClip = m_fFxAlphaClip;
    m_FxParams.fErodeThreshold = m_fFxErodeThreshold;
}
