#include "WintersPCH.h"

#include "Renderer/UIRenderer.h"
#include "WintersMath.h"

#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cmath>
#include <vector>
#include <cstring>

namespace
{
    using Microsoft::WRL::ComPtr;

    struct UIVertex
    {
        f32_t px = 0.f;
        f32_t py = 0.f;
        f32_t u = 0.f;
        f32_t v = 0.f;
        f32_t r = 1.f;
        f32_t g = 1.f;
        f32_t b = 1.f;
        f32_t a = 1.f;
    };

    struct UICBFrame
    {
        f32_t screenSize[2] = { 1.f, 1.f };
        f32_t padding[2] = {};
    };

    constexpr u32_t kMaxUIVertices = 65536;

    constexpr const char* kUIShader = R"(
cbuffer CBFrame : register(b0)
{
    float2 g_vScreenSize;
    float2 g_vPadding;
};

Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_INPUT
{
    float2 vPosition : POSITION;
    float2 vTexCoord : TEXCOORD0;
    float4 vColor : COLOR0;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
    float4 vColor : COLOR0;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    float2 ndc;
    ndc.x = (input.vPosition.x / max(g_vScreenSize.x, 1.0f)) * 2.0f - 1.0f;
    ndc.y = 1.0f - (input.vPosition.y / max(g_vScreenSize.y, 1.0f)) * 2.0f;
    output.vPosition = float4(ndc, 0.0f, 1.0f);
    output.vTexCoord = input.vTexCoord;
    output.vColor = input.vColor;
    return output;
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    return texColor * input.vColor;
}
)";

    bool_t CompileUIShader(const char* pEntry, const char* pTarget, ID3DBlob** ppBlob)
    {
        ComPtr<ID3DBlob> pError;
        const HRESULT hr = D3DCompile(
            kUIShader,
            std::strlen(kUIShader),
            nullptr,
            nullptr,
            nullptr,
            pEntry,
            pTarget,
            D3DCOMPILE_ENABLE_STRICTNESS,
            0,
            ppBlob,
            pError.GetAddressOf());

        if (FAILED(hr))
        {
            if (pError)
                OutputDebugStringA(static_cast<const char*>(pError->GetBufferPointer()));
            return false;
        }

        return true;
    }

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
}

struct CUIRenderer::Impl
{
    IRHIDevice* pDevice = nullptr;
    ID3D11Device* pNativeDevice = nullptr;
    ID3D11DeviceContext* pContext = nullptr;

    ComPtr<ID3D11VertexShader> pVS;
    ComPtr<ID3D11PixelShader> pPS;
    ComPtr<ID3D11InputLayout> pInputLayout;
    ComPtr<ID3D11Buffer> pVertexBuffer;
    ComPtr<ID3D11Buffer> pCBFrame;
    ComPtr<ID3D11BlendState> pBlendState;
    ComPtr<ID3D11DepthStencilState> pDepthState;
    ComPtr<ID3D11RasterizerState> pRasterizerState;
    ComPtr<ID3D11SamplerState> pLinearSamplerState;
    ComPtr<ID3D11SamplerState> pPointSamplerState;
    ComPtr<ID3D11Texture2D> pWhiteTexture;
    ComPtr<ID3D11ShaderResourceView> pWhiteSRV;

    std::vector<UIVertex> vertices;
    ID3D11ShaderResourceView* pCurrentSRV = nullptr;
    bool_t bInFrame = false;
    bool_t bReady = false;

    ComPtr<ID3D11InputLayout> pPrevInputLayout;
    D3D11_PRIMITIVE_TOPOLOGY prevTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ComPtr<ID3D11Buffer> pPrevVB;
    UINT prevStride = 0;
    UINT prevOffset = 0;
    ComPtr<ID3D11VertexShader> pPrevVS;
    ComPtr<ID3D11PixelShader> pPrevPS;
    ComPtr<ID3D11Buffer> pPrevVSCB0;
    ComPtr<ID3D11ShaderResourceView> pPrevPSSRV0;
    ComPtr<ID3D11SamplerState> pPrevPSSampler0;
    ComPtr<ID3D11BlendState> pPrevBlendState;
    FLOAT prevBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    UINT prevSampleMask = 0xffffffffu;
    ComPtr<ID3D11DepthStencilState> pPrevDepthState;
    UINT prevStencilRef = 0;
    ComPtr<ID3D11RasterizerState> pPrevRasterizerState;

    void Flush()
    {
        if (!bInFrame || vertices.empty() || !pContext || !pCurrentSRV)
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT hr = pContext->Map(pVertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (FAILED(hr))
        {
            vertices.clear();
            return;
        }

        const size_t byteCount = vertices.size() * sizeof(UIVertex);
        std::memcpy(mapped.pData, vertices.data(), byteCount);
        pContext->Unmap(pVertexBuffer.Get(), 0);

        ID3D11ShaderResourceView* pSRV = pCurrentSRV;
        pContext->PSSetShaderResources(0, 1, &pSRV);
        pContext->Draw(static_cast<UINT>(vertices.size()), 0);
        vertices.clear();
    }

    void SaveState()
    {
        pContext->IAGetInputLayout(pPrevInputLayout.GetAddressOf());
        pContext->IAGetPrimitiveTopology(&prevTopology);

        ID3D11Buffer* pVB = nullptr;
        pContext->IAGetVertexBuffers(0, 1, &pVB, &prevStride, &prevOffset);
        pPrevVB.Attach(pVB);

        pContext->VSGetShader(pPrevVS.GetAddressOf(), nullptr, nullptr);
        pContext->PSGetShader(pPrevPS.GetAddressOf(), nullptr, nullptr);

        ID3D11Buffer* pCB = nullptr;
        pContext->VSGetConstantBuffers(0, 1, &pCB);
        pPrevVSCB0.Attach(pCB);

        ID3D11ShaderResourceView* pSRV = nullptr;
        pContext->PSGetShaderResources(0, 1, &pSRV);
        pPrevPSSRV0.Attach(pSRV);

        ID3D11SamplerState* pSampler = nullptr;
        pContext->PSGetSamplers(0, 1, &pSampler);
        pPrevPSSampler0.Attach(pSampler);

        pContext->OMGetBlendState(pPrevBlendState.GetAddressOf(), prevBlendFactor, &prevSampleMask);
        pContext->OMGetDepthStencilState(pPrevDepthState.GetAddressOf(), &prevStencilRef);
        pContext->RSGetState(pPrevRasterizerState.GetAddressOf());
    }

    void RestoreState()
    {
        ID3D11Buffer* pVB = pPrevVB.Get();
        pContext->IASetVertexBuffers(0, 1, &pVB, &prevStride, &prevOffset);
        pContext->IASetInputLayout(pPrevInputLayout.Get());
        pContext->IASetPrimitiveTopology(prevTopology);
        pContext->VSSetShader(pPrevVS.Get(), nullptr, 0);
        pContext->PSSetShader(pPrevPS.Get(), nullptr, 0);

        ID3D11Buffer* pCB = pPrevVSCB0.Get();
        pContext->VSSetConstantBuffers(0, 1, &pCB);

        ID3D11ShaderResourceView* pSRV = pPrevPSSRV0.Get();
        pContext->PSSetShaderResources(0, 1, &pSRV);

        ID3D11SamplerState* pSampler = pPrevPSSampler0.Get();
        pContext->PSSetSamplers(0, 1, &pSampler);

        pContext->OMSetBlendState(pPrevBlendState.Get(), prevBlendFactor, prevSampleMask);
        pContext->OMSetDepthStencilState(pPrevDepthState.Get(), prevStencilRef);
        pContext->RSSetState(pPrevRasterizerState.Get());

        pPrevInputLayout.Reset();
        pPrevVB.Reset();
        pPrevVS.Reset();
        pPrevPS.Reset();
        pPrevVSCB0.Reset();
        pPrevPSSRV0.Reset();
        pPrevPSSampler0.Reset();
        pPrevBlendState.Reset();
        pPrevDepthState.Reset();
        pPrevRasterizerState.Reset();
    }
};

CUIRenderer::CUIRenderer()
    : m_pImpl(std::make_unique<Impl>())
{
}

CUIRenderer::~CUIRenderer()
{
    Shutdown();
}

std::unique_ptr<CUIRenderer> CUIRenderer::Create(IRHIDevice* pDevice)
{
    auto pRenderer = std::unique_ptr<CUIRenderer>(new CUIRenderer());
    if (!pRenderer->Initialize(pDevice))
        return nullptr;

    return pRenderer;
}

bool_t CUIRenderer::Initialize(IRHIDevice* pDevice)
{
    if (!m_pImpl || !pDevice || pDevice->GetBackend() != eRHIBackend::DX11)
        return false;

    m_pImpl->pDevice = pDevice;
    m_pImpl->pNativeDevice = GetNativeDX11Device(pDevice);
    m_pImpl->pContext = GetNativeDX11Context(pDevice);
    if (!m_pImpl->pNativeDevice || !m_pImpl->pContext)
        return false;

    ComPtr<ID3DBlob> pVSBlob;
    ComPtr<ID3DBlob> pPSBlob;
    if (!CompileUIShader("VSMain", "vs_5_0", pVSBlob.GetAddressOf()))
        return false;
    if (!CompileUIShader("PSMain", "ps_5_0", pPSBlob.GetAddressOf()))
        return false;

    if (FAILED(m_pImpl->pNativeDevice->CreateVertexShader(
        pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(),
        nullptr,
        m_pImpl->pVS.GetAddressOf())))
        return false;

    if (FAILED(m_pImpl->pNativeDevice->CreatePixelShader(
        pPSBlob->GetBufferPointer(),
        pPSBlob->GetBufferSize(),
        nullptr,
        m_pImpl->pPS.GetAddressOf())))
        return false;

    static constexpr D3D11_INPUT_ELEMENT_DESC kInputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    if (FAILED(m_pImpl->pNativeDevice->CreateInputLayout(
        kInputElements,
        static_cast<UINT>(_countof(kInputElements)),
        pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(),
        m_pImpl->pInputLayout.GetAddressOf())))
        return false;

    D3D11_BUFFER_DESC vbDesc{};
    vbDesc.ByteWidth = sizeof(UIVertex) * kMaxUIVertices;
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(m_pImpl->pNativeDevice->CreateBuffer(&vbDesc, nullptr,
        m_pImpl->pVertexBuffer.GetAddressOf())))
        return false;

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(UICBFrame);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(m_pImpl->pNativeDevice->CreateBuffer(&cbDesc, nullptr,
        m_pImpl->pCBFrame.GetAddressOf())))
        return false;

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(m_pImpl->pNativeDevice->CreateBlendState(&blendDesc,
        m_pImpl->pBlendState.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(m_pImpl->pNativeDevice->CreateDepthStencilState(&depthDesc,
        m_pImpl->pDepthState.GetAddressOf())))
        return false;

    D3D11_RASTERIZER_DESC rsDesc{};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.DepthClipEnable = TRUE;
    if (FAILED(m_pImpl->pNativeDevice->CreateRasterizerState(&rsDesc,
        m_pImpl->pRasterizerState.GetAddressOf())))
        return false;

    const auto createSampler = [this](D3D11_FILTER filter, ComPtr<ID3D11SamplerState>& outSampler) -> bool_t
    {
        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = filter;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0.f;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        return SUCCEEDED(m_pImpl->pNativeDevice->CreateSamplerState(
            &samplerDesc,
            outSampler.GetAddressOf()));
    };

    if (!createSampler(D3D11_FILTER_MIN_MAG_MIP_LINEAR, m_pImpl->pLinearSamplerState))
        return false;
    if (!createSampler(D3D11_FILTER_MIN_MAG_MIP_POINT, m_pImpl->pPointSamplerState))
        return false;

    const u32_t whitePixel = 0xffffffffu;
    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = 1;
    texDesc.Height = 1;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA texInit{};
    texInit.pSysMem = &whitePixel;
    texInit.SysMemPitch = sizeof(whitePixel);
    if (FAILED(m_pImpl->pNativeDevice->CreateTexture2D(&texDesc, &texInit,
        m_pImpl->pWhiteTexture.GetAddressOf())))
        return false;

    if (FAILED(m_pImpl->pNativeDevice->CreateShaderResourceView(
        m_pImpl->pWhiteTexture.Get(),
        nullptr,
        m_pImpl->pWhiteSRV.GetAddressOf())))
        return false;

    m_pImpl->vertices.reserve(4096);
    m_pImpl->bReady = true;
    return true;
}

bool_t CUIRenderer::IsReady() const
{
    return m_pImpl && m_pImpl->bReady;
}

void CUIRenderer::Begin(u32_t iScreenWidth, u32_t iScreenHeight, eUISamplerMode eSamplerMode)
{
    if (!IsReady() || m_pImpl->bInFrame)
        return;

    m_pImpl->bInFrame = true;
    m_pImpl->pCurrentSRV = nullptr;
    m_pImpl->vertices.clear();
    if (m_pImpl->vertices.capacity() < 4096u)
        m_pImpl->vertices.reserve(4096u);
    m_pImpl->SaveState();

    UICBFrame cb{};
    cb.screenSize[0] = static_cast<f32_t>(iScreenWidth);
    cb.screenSize[1] = static_cast<f32_t>(iScreenHeight);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(m_pImpl->pContext->Map(m_pImpl->pCBFrame.Get(), 0,
        D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        std::memcpy(mapped.pData, &cb, sizeof(cb));
        m_pImpl->pContext->Unmap(m_pImpl->pCBFrame.Get(), 0);
    }

    UINT stride = sizeof(UIVertex);
    UINT offset = 0;
    ID3D11Buffer* pVB = m_pImpl->pVertexBuffer.Get();
    ID3D11Buffer* pCB = m_pImpl->pCBFrame.Get();
    ID3D11SamplerState* pSampler = (eSamplerMode == eUISamplerMode::PointClamp)
        ? m_pImpl->pPointSamplerState.Get()
        : m_pImpl->pLinearSamplerState.Get();

    m_pImpl->pContext->IASetInputLayout(m_pImpl->pInputLayout.Get());
    m_pImpl->pContext->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
    m_pImpl->pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pImpl->pContext->VSSetShader(m_pImpl->pVS.Get(), nullptr, 0);
    m_pImpl->pContext->PSSetShader(m_pImpl->pPS.Get(), nullptr, 0);
    m_pImpl->pContext->VSSetConstantBuffers(0, 1, &pCB);
    m_pImpl->pContext->PSSetSamplers(0, 1, &pSampler);
    m_pImpl->pContext->OMSetBlendState(m_pImpl->pBlendState.Get(), nullptr, 0xffffffffu);
    m_pImpl->pContext->OMSetDepthStencilState(m_pImpl->pDepthState.Get(), 0);
    m_pImpl->pContext->RSSetState(m_pImpl->pRasterizerState.Get());
}

void CUIRenderer::End()
{
    if (!IsReady() || !m_pImpl->bInFrame)
        return;

    m_pImpl->Flush();
    m_pImpl->pCurrentSRV = nullptr;
    m_pImpl->bInFrame = false;
    m_pImpl->RestoreState();
}

void CUIRenderer::ReserveQuads(u32_t iQuadCount)
{
    if (!IsReady())
        return;

    u32_t iVertexCount = iQuadCount * 6u;
    if (iVertexCount > kMaxUIVertices)
        iVertexCount = kMaxUIVertices;

    if (m_pImpl->vertices.capacity() < iVertexCount)
        m_pImpl->vertices.reserve(iVertexCount);
}

void CUIRenderer::DrawImage(void* pTextureSRV,
    f32_t fX, f32_t fY, f32_t fW, f32_t fH,
    const Vec4& vUVRect,
    const Vec4& vColor)
{
    if (!IsReady() || !m_pImpl->bInFrame || fW <= 0.f || fH <= 0.f)
        return;

    ID3D11ShaderResourceView* pSRV = pTextureSRV
        ? static_cast<ID3D11ShaderResourceView*>(pTextureSRV)
        : m_pImpl->pWhiteSRV.Get();
    if (!pSRV)
        return;

    if (m_pImpl->pCurrentSRV != pSRV ||
        m_pImpl->vertices.size() + 6 > kMaxUIVertices)
    {
        m_pImpl->Flush();
        m_pImpl->pCurrentSRV = pSRV;
    }

    const f32_t x0 = fX;
    const f32_t y0 = fY;
    const f32_t x1 = fX + fW;
    const f32_t y1 = fY + fH;
    const f32_t u0 = vUVRect.x;
    const f32_t v0 = vUVRect.y;
    const f32_t u1 = vUVRect.z;
    const f32_t v1 = vUVRect.w;

    const size_t base = m_pImpl->vertices.size();
    m_pImpl->vertices.resize(base + 6u);
    UIVertex* pVtx = m_pImpl->vertices.data() + base;
    pVtx[0] = { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[1] = { x1, y0, u1, v0, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[2] = { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[3] = { x0, y0, u0, v0, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[4] = { x1, y1, u1, v1, vColor.x, vColor.y, vColor.z, vColor.w };
    pVtx[5] = { x0, y1, u0, v1, vColor.x, vColor.y, vColor.z, vColor.w };
}

void CUIRenderer::DrawImageCircle(void* pTextureSRV,
    f32_t fX, f32_t fY, f32_t fW, f32_t fH,
    const Vec4& vUVRect,
    const Vec4& vColor,
    u32_t iSegmentCount)
{
    if (!IsReady() || !m_pImpl->bInFrame || fW <= 0.f || fH <= 0.f)
        return;

    ID3D11ShaderResourceView* pSRV = pTextureSRV
        ? static_cast<ID3D11ShaderResourceView*>(pTextureSRV)
        : m_pImpl->pWhiteSRV.Get();
    if (!pSRV)
        return;

    if (iSegmentCount < 12u)
        iSegmentCount = 12u;
    if (iSegmentCount > 96u)
        iSegmentCount = 96u;

    const u32_t vertexCount = iSegmentCount * 3u;
    if (m_pImpl->pCurrentSRV != pSRV ||
        m_pImpl->vertices.size() + vertexCount > kMaxUIVertices)
    {
        m_pImpl->Flush();
        m_pImpl->pCurrentSRV = pSRV;
    }

    const f32_t cx = fX + fW * 0.5f;
    const f32_t cy = fY + fH * 0.5f;
    const f32_t rx = fW * 0.5f;
    const f32_t ry = fH * 0.5f;
    const f32_t u0 = vUVRect.x;
    const f32_t v0 = vUVRect.y;
    const f32_t u1 = vUVRect.z;
    const f32_t v1 = vUVRect.w;
    const f32_t uc = (u0 + u1) * 0.5f;
    const f32_t vc = (v0 + v1) * 0.5f;
    const f32_t ur = (u1 - u0) * 0.5f;
    const f32_t vr = (v1 - v0) * 0.5f;

    auto makeVertex = [&](f32_t px, f32_t py, f32_t u, f32_t v) -> UIVertex
    {
        return { px, py, u, v, vColor.x, vColor.y, vColor.z, vColor.w };
    };

    for (u32_t i = 0; i < iSegmentCount; ++i)
    {
        const f32_t a0 =
            (static_cast<f32_t>(i) / static_cast<f32_t>(iSegmentCount)) * WintersMath::kTwoPi;
        const f32_t a1 =
            (static_cast<f32_t>(i + 1u) / static_cast<f32_t>(iSegmentCount)) * WintersMath::kTwoPi;
        const f32_t c0 = static_cast<f32_t>(std::cos(a0));
        const f32_t s0 = static_cast<f32_t>(std::sin(a0));
        const f32_t c1 = static_cast<f32_t>(std::cos(a1));
        const f32_t s1 = static_cast<f32_t>(std::sin(a1));

        m_pImpl->vertices.push_back(makeVertex(cx, cy, uc, vc));
        m_pImpl->vertices.push_back(makeVertex(cx + c0 * rx, cy + s0 * ry, uc + c0 * ur, vc + s0 * vr));
        m_pImpl->vertices.push_back(makeVertex(cx + c1 * rx, cy + s1 * ry, uc + c1 * ur, vc + s1 * vr));
    }
}

void CUIRenderer::Shutdown()
{
    if (!m_pImpl)
        return;

    m_pImpl->vertices.clear();
    m_pImpl->pCurrentSRV = nullptr;
    m_pImpl->bInFrame = false;
    m_pImpl->bReady = false;
    m_pImpl->pDevice = nullptr;
    m_pImpl->pNativeDevice = nullptr;
    m_pImpl->pContext = nullptr;
}
