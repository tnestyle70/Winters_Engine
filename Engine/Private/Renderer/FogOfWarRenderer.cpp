#include "WintersPCH.h"
#include "Renderer/FogOfWarRenderer.h"
#include "Core/Profiler/RenderFrameStats.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/RHITypes.h"
#include "WintersPaths.h"

#include <DirectXMath.h>
#include <d3d11.h>
#include <cmath>
#include <cstring>
#include <wrl/client.h>

namespace
{
    ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
    {
        return pDevice
            ? static_cast<ID3D11Device*>(pDevice->GetNativeHandle(eNativeHandleType::DX11Device))
            : nullptr;
    }

    ID3D11DeviceContext* GetNativeDX11Context(IRHIDevice* pDevice)
    {
        return pDevice
            ? static_cast<ID3D11DeviceContext*>(pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext))
            : nullptr;
    }

    struct FogWorldVertex
    {
        f32_t x, y, z;
        f32_t u, v;
    };

    struct FogWorldCB
    {
        DirectX::XMFLOAT4X4 matViewProj;
        DirectX::XMFLOAT4 vWorldRect;
        DirectX::XMFLOAT4 vFogParams;
        DirectX::XMFLOAT4 vUnexploredColor;
        DirectX::XMFLOAT4 vExploredColor;
    };

    bool_t CreateWorldOverlayResources(ID3D11Device* pNativeDevice,
        DX11Shader& shader,
        Microsoft::WRL::ComPtr<ID3D11Buffer>& pVB,
        Microsoft::WRL::ComPtr<ID3D11Buffer>& pIB,
        Microsoft::WRL::ComPtr<ID3D11Buffer>& pCB,
        Microsoft::WRL::ComPtr<ID3D11InputLayout>& pInputLayout,
        Microsoft::WRL::ComPtr<ID3D11SamplerState>& pSampler,
        Microsoft::WRL::ComPtr<ID3D11BlendState>& pBlend,
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState>& pDepth,
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>& pRasterizer)
    {
        wchar_t shaderPath[MAX_PATH] = {};
        if (!WintersResolveContentPath(L"Shaders/FogOfWarWorld.hlsl", shaderPath, MAX_PATH))
        {
            OutputDebugStringA("[FogOfWarRenderer] FogOfWarWorld.hlsl path resolve failed\n");
            return false;
        }

        if (!shader.Load(pNativeDevice, shaderPath, "VS", "PS"))
        {
            OutputDebugStringA("[FogOfWarRenderer] FogOfWarWorld shader load failed\n");
            return false;
        }

        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(f32_t) * 3, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        HRESULT hr = pNativeDevice->CreateInputLayout(
            layout,
            2,
            shader.GetVSBlob()->GetBufferPointer(),
            shader.GetVSBlob()->GetBufferSize(),
            pInputLayout.GetAddressOf());
        if (FAILED(hr))
            return false;

        D3D11_BUFFER_DESC vbDesc{};
        vbDesc.ByteWidth = sizeof(FogWorldVertex) * 4;
        vbDesc.Usage = D3D11_USAGE_DYNAMIC;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = pNativeDevice->CreateBuffer(&vbDesc, nullptr, pVB.GetAddressOf());
        if (FAILED(hr))
            return false;

        const u16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
        D3D11_BUFFER_DESC ibDesc{};
        ibDesc.ByteWidth = sizeof(indices);
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA ibInit{};
        ibInit.pSysMem = indices;
        hr = pNativeDevice->CreateBuffer(&ibDesc, &ibInit, pIB.GetAddressOf());
        if (FAILED(hr))
            return false;

        D3D11_BUFFER_DESC cbDesc{};
        cbDesc.ByteWidth = sizeof(FogWorldCB);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = pNativeDevice->CreateBuffer(&cbDesc, nullptr, pCB.GetAddressOf());
        if (FAILED(hr))
            return false;

        D3D11_SAMPLER_DESC samplerDesc{};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        hr = pNativeDevice->CreateSamplerState(&samplerDesc, pSampler.GetAddressOf());
        if (FAILED(hr))
            return false;

        D3D11_BLEND_DESC blendDesc{};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        hr = pNativeDevice->CreateBlendState(&blendDesc, pBlend.GetAddressOf());
        if (FAILED(hr))
            return false;

        D3D11_DEPTH_STENCIL_DESC depthDesc{};
        depthDesc.DepthEnable = FALSE;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        hr = pNativeDevice->CreateDepthStencilState(&depthDesc, pDepth.GetAddressOf());
        if (FAILED(hr))
            return false;

        D3D11_RASTERIZER_DESC rasterDesc{};
        rasterDesc.FillMode = D3D11_FILL_SOLID;
        rasterDesc.CullMode = D3D11_CULL_NONE;
        rasterDesc.FrontCounterClockwise = FALSE;
        rasterDesc.DepthClipEnable = TRUE;
        hr = pNativeDevice->CreateRasterizerState(&rasterDesc, pRasterizer.GetAddressOf());
        return SUCCEEDED(hr);
    }
}

struct CFogOfWarRenderer::Impl
{
    IRHIDevice* pDevice = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRV;
    // Impl means implementation details kept out of the public renderer header.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pMinimapOverlayTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pMinimapOverlaySRV;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pWorldVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pWorldIB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pWorldCB;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> pWorldInputLayout;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> pWorldSampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> pWorldBlend;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pWorldDepth;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> pWorldRasterizer;
    DX11Shader worldShader;
    u32_t uDim = 0;
    bool_t bWorldOverlayReady = false;
    bool_t bWorldOverlayDrawLogged = false;
};

CFogOfWarRenderer::~CFogOfWarRenderer() = default;

std::unique_ptr<CFogOfWarRenderer> CFogOfWarRenderer::Create(IRHIDevice* pDevice, u32_t dim)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    if (!pNativeDevice || dim == 0)
        return nullptr;

    std::unique_ptr<CFogOfWarRenderer> p(new CFogOfWarRenderer());
    p->m_pImpl = std::make_unique<Impl>();
    p->m_pImpl->pDevice = pDevice;
    p->m_pImpl->uDim = dim;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = dim;
    desc.Height = dim;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = pNativeDevice->CreateTexture2D(&desc, nullptr, p->m_pImpl->pTexture.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("[FogOfWarRenderer] CreateTexture2D failed\n");
        return nullptr;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = pNativeDevice->CreateShaderResourceView(
        p->m_pImpl->pTexture.Get(), &srvDesc, p->m_pImpl->pSRV.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugStringA("[FogOfWarRenderer] CreateShaderResourceView failed\n");
        return nullptr;
    }

    p->m_pImpl->bWorldOverlayReady = CreateWorldOverlayResources(
        pNativeDevice,
        p->m_pImpl->worldShader,
        p->m_pImpl->pWorldVB,
        p->m_pImpl->pWorldIB,
        p->m_pImpl->pWorldCB,
        p->m_pImpl->pWorldInputLayout,
        p->m_pImpl->pWorldSampler,
        p->m_pImpl->pWorldBlend,
        p->m_pImpl->pWorldDepth,
        p->m_pImpl->pWorldRasterizer);
    if (!p->m_pImpl->bWorldOverlayReady)
        OutputDebugStringA("[FogOfWarRenderer] world overlay resources failed\n");

    D3D11_TEXTURE2D_DESC overlayDesc = desc;
    overlayDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    overlayDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = pNativeDevice->CreateTexture2D(
        &overlayDesc, nullptr, p->m_pImpl->pMinimapOverlayTexture.GetAddressOf());

    if (SUCCEEDED(hr))
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC overlaySrvDesc{};
        overlaySrvDesc.Format = overlayDesc.Format;
        overlaySrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        overlaySrvDesc.Texture2D.MipLevels = 1;
        hr = pNativeDevice->CreateShaderResourceView(
            p->m_pImpl->pMinimapOverlayTexture.Get(),
            &overlaySrvDesc,
            p->m_pImpl->pMinimapOverlaySRV.GetAddressOf());
    }

    if (FAILED(hr))
        OutputDebugStringA("[FogOfWarRenderer] minimap overlay SRV create failed\n");

    return p;
}

void CFogOfWarRenderer::UpdateTexture(const u8_t* pData, u32_t dim)
{
    if (!m_pImpl || !pData || dim != m_pImpl->uDim)
        return;

    ID3D11DeviceContext* pContext = GetNativeDX11Context(m_pImpl->pDevice);
    if (!pContext)
        return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = pContext->Map(m_pImpl->pTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return;

    u8_t* pDst = reinterpret_cast<u8_t*>(mapped.pData);
    for (u32_t y = 0; y < dim; ++y)
        std::memcpy(pDst + y * mapped.RowPitch, pData + y * dim, dim);

    pContext->Unmap(m_pImpl->pTexture.Get(), 0);

    // R8 stores one normalized 8-bit visibility value per fog texel.
    if (m_pImpl->pMinimapOverlayTexture)
    {
        D3D11_MAPPED_SUBRESOURCE overlayMapped{};
        if (SUCCEEDED(pContext->Map(m_pImpl->pMinimapOverlayTexture.Get(),
            0, D3D11_MAP_WRITE_DISCARD, 0, &overlayMapped)))
        {
            // Convert the single-channel fog data into an RGBA minimap overlay.
            for (u32_t y = 0; y < dim; ++y)
            {
                u8_t* pRow = reinterpret_cast<u8_t*>(overlayMapped.pData) + y * overlayMapped.RowPitch;
                for (u32_t x = 0; x < dim; ++x)
                {
                    const u8_t value = pData[y * dim + x];
                    const u8_t alpha = value >= 250 ? 0 : (value >= 127 ? 112 : 214);
                    // BGRA bytes: near-black fog color plus alpha from visibility.
                    pRow[x * 4 + 0] = 3;
                    pRow[x * 4 + 1] = 7;
                    pRow[x * 4 + 2] = 8;
                    pRow[x * 4 + 3] = alpha;
                }
            }
            // Unmap uploads the CPU-written overlay bytes so the SRV can be sampled by UI rendering.
            pContext->Unmap(m_pImpl->pMinimapOverlayTexture.Get(), 0);
        }
    }
}

void* CFogOfWarRenderer::GetNativeSRV() const
{
    return m_pImpl ? static_cast<void*>(m_pImpl->pSRV.Get()) : nullptr;
}

void* CFogOfWarRenderer::GetMinimapOverlaySRV() const
{
    return m_pImpl ? static_cast<void*>(m_pImpl->pMinimapOverlaySRV.Get()) : nullptr;
}


void CFogOfWarRenderer::RenderWorldOverlay(IRHIDevice* pDevice,
    const Mat4& matViewProj,
    const Vec2& vWorldAtUv00,
    const Vec2& vWorldAtUv10,
    const Vec2& vWorldAtUv01,
    f32_t fYOffset)
{
    if (!m_pImpl || !m_pImpl->bWorldOverlayReady)
        return;

    ID3D11DeviceContext* pContext = GetNativeDX11Context(pDevice ? pDevice : m_pImpl->pDevice);
    if (!pContext)
        return;

    const f32_t ux = vWorldAtUv10.x - vWorldAtUv00.x;
    const f32_t uz = vWorldAtUv10.y - vWorldAtUv00.y;
    const f32_t vx = vWorldAtUv01.x - vWorldAtUv00.x;
    const f32_t vz = vWorldAtUv01.y - vWorldAtUv00.y;
    if (std::fabs(ux * vz - uz * vx) <= 0.0001f)
        return;

    const Vec2 vWorldAtUv11{
        vWorldAtUv10.x + vWorldAtUv01.x - vWorldAtUv00.x,
        vWorldAtUv10.y + vWorldAtUv01.y - vWorldAtUv00.y
    };

    const FogWorldVertex vertices[4] =
    {
        { vWorldAtUv00.x, fYOffset, vWorldAtUv00.y, 0.f, 0.f },
        { vWorldAtUv10.x, fYOffset, vWorldAtUv10.y, 1.f, 0.f },
        { vWorldAtUv11.x, fYOffset, vWorldAtUv11.y, 1.f, 1.f },
        { vWorldAtUv01.x, fYOffset, vWorldAtUv01.y, 0.f, 1.f },
    };

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = pContext->Map(m_pImpl->pWorldVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return;
    std::memcpy(mapped.pData, vertices, sizeof(vertices));
    pContext->Unmap(m_pImpl->pWorldVB.Get(), 0);

    hr = pContext->Map(m_pImpl->pWorldCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return;

    FogWorldCB cb{};
    cb.matViewProj = matViewProj.m;
    cb.vWorldRect = DirectX::XMFLOAT4(0.f, 0.f, 1.f, 1.f);
    cb.vFogParams = DirectX::XMFLOAT4(0.64f, 0.28f, 0.50f, 0.f);
    cb.vUnexploredColor = DirectX::XMFLOAT4(0.026f, 0.038f, 0.035f, 1.f);
    cb.vExploredColor = DirectX::XMFLOAT4(0.070f, 0.088f, 0.076f, 1.f);
    std::memcpy(mapped.pData, &cb, sizeof(cb));
    pContext->Unmap(m_pImpl->pWorldCB.Get(), 0);

    using Microsoft::WRL::ComPtr;

    ComPtr<ID3D11BlendState> pPrevBlend;
    FLOAT prevBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    UINT prevSampleMask = 0;
    ComPtr<ID3D11DepthStencilState> pPrevDepth;
    UINT prevStencilRef = 0;
    ComPtr<ID3D11RasterizerState> pPrevRasterizer;
    ComPtr<ID3D11InputLayout> pPrevInputLayout;
    D3D11_PRIMITIVE_TOPOLOGY prevTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ComPtr<ID3D11Buffer> pPrevVB;
    UINT prevStride = 0;
    UINT prevOffset = 0;
    ComPtr<ID3D11Buffer> pPrevIB;
    DXGI_FORMAT prevIndexFormat = DXGI_FORMAT_UNKNOWN;
    UINT prevIndexOffset = 0;
    ComPtr<ID3D11VertexShader> pPrevVS;
    ComPtr<ID3D11PixelShader> pPrevPS;
    ComPtr<ID3D11Buffer> pPrevVSCB;
    ComPtr<ID3D11Buffer> pPrevPSCB;
    ComPtr<ID3D11ShaderResourceView> pPrevSRV;
    ComPtr<ID3D11SamplerState> pPrevSampler;

    pContext->OMGetBlendState(pPrevBlend.GetAddressOf(), prevBlendFactor, &prevSampleMask);
    pContext->OMGetDepthStencilState(pPrevDepth.GetAddressOf(), &prevStencilRef);
    pContext->RSGetState(pPrevRasterizer.GetAddressOf());
    pContext->IAGetInputLayout(pPrevInputLayout.GetAddressOf());
    pContext->IAGetPrimitiveTopology(&prevTopology);
    pContext->IAGetVertexBuffers(0, 1, pPrevVB.GetAddressOf(), &prevStride, &prevOffset);
    pContext->IAGetIndexBuffer(pPrevIB.GetAddressOf(), &prevIndexFormat, &prevIndexOffset);
    pContext->VSGetShader(pPrevVS.GetAddressOf(), nullptr, nullptr);
    pContext->PSGetShader(pPrevPS.GetAddressOf(), nullptr, nullptr);
    pContext->VSGetConstantBuffers(0, 1, pPrevVSCB.GetAddressOf());
    pContext->PSGetConstantBuffers(0, 1, pPrevPSCB.GetAddressOf());
    pContext->PSGetShaderResources(0, 1, pPrevSRV.GetAddressOf());
    pContext->PSGetSamplers(0, 1, pPrevSampler.GetAddressOf());

    const FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    pContext->OMSetBlendState(m_pImpl->pWorldBlend.Get(), blendFactor, 0xFFFFFFFF);
    pContext->OMSetDepthStencilState(m_pImpl->pWorldDepth.Get(), 0);
    pContext->RSSetState(m_pImpl->pWorldRasterizer.Get());
    pContext->IASetInputLayout(m_pImpl->pWorldInputLayout.Get());

    UINT stride = sizeof(FogWorldVertex);
    UINT offset = 0;
    ID3D11Buffer* pVB = m_pImpl->pWorldVB.Get();
    pContext->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
    pContext->IASetIndexBuffer(m_pImpl->pWorldIB.Get(), DXGI_FORMAT_R16_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_pImpl->worldShader.Bind(pContext);

    ID3D11Buffer* pCB = m_pImpl->pWorldCB.Get();
    pContext->VSSetConstantBuffers(0, 1, &pCB);
    pContext->PSSetConstantBuffers(0, 1, &pCB);

    ID3D11ShaderResourceView* pSRV = m_pImpl->pSRV.Get();
    ID3D11SamplerState* pSampler = m_pImpl->pWorldSampler.Get();
    pContext->PSSetShaderResources(0, 1, &pSRV);
    pContext->PSSetSamplers(0, 1, &pSampler);

    RenderFrameStats::AddDraw(6u);
    pContext->DrawIndexed(6, 0, 0);

    if (!m_pImpl->bWorldOverlayDrawLogged)
    {
        OutputDebugStringA("[FogOfWarRenderer] world overlay draw submitted\n");
        m_pImpl->bWorldOverlayDrawLogged = true;
    }

    ID3D11ShaderResourceView* pRestoreSRV = pPrevSRV.Get();
    ID3D11SamplerState* pRestoreSampler = pPrevSampler.Get();
    ID3D11Buffer* pRestoreVSCB = pPrevVSCB.Get();
    ID3D11Buffer* pRestorePSCB = pPrevPSCB.Get();
    ID3D11Buffer* pRestoreVB = pPrevVB.Get();

    pContext->PSSetShaderResources(0, 1, &pRestoreSRV);
    pContext->PSSetSamplers(0, 1, &pRestoreSampler);
    pContext->VSSetConstantBuffers(0, 1, &pRestoreVSCB);
    pContext->PSSetConstantBuffers(0, 1, &pRestorePSCB);
    pContext->VSSetShader(pPrevVS.Get(), nullptr, 0);
    pContext->PSSetShader(pPrevPS.Get(), nullptr, 0);
    pContext->IASetInputLayout(pPrevInputLayout.Get());
    pContext->IASetVertexBuffers(0, 1, &pRestoreVB, &prevStride, &prevOffset);
    pContext->IASetIndexBuffer(pPrevIB.Get(), prevIndexFormat, prevIndexOffset);
    pContext->IASetPrimitiveTopology(prevTopology);
    pContext->RSSetState(pPrevRasterizer.Get());
    pContext->OMSetDepthStencilState(pPrevDepth.Get(), prevStencilRef);
    pContext->OMSetBlendState(pPrevBlend.Get(), prevBlendFactor, prevSampleMask);
}
