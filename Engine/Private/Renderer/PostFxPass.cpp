#include "Renderer/PostFxPass.h"

#include "Core/Profiler/RenderFrameStats.h"
#include "RHI/RHITypes.h"
#include "WintersPaths.h"

#include <algorithm>
#include <cstring>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

using namespace Engine;
using Microsoft::WRL::ComPtr;

namespace
{
    struct CBPostFxData
    {
        f32_t fSourceTexelSize[2];
        f32_t fBlurDirection[2];

        f32_t fGamma;
        f32_t fSaturation;
        f32_t fGradeStrength;
        f32_t fVignetteStrength;

        f32_t fTint[3];
        f32_t fVignetteInner;

        f32_t fVignetteOuter;
        f32_t fBloomThreshold;
        f32_t fBloomIntensity;
        f32_t fBloomSoftKnee;
    };
    static_assert(sizeof(CBPostFxData) == 64u);

    ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
    {
        if (!pDevice || pDevice->GetBackend() != eRHIBackend::DX11)
            return nullptr;
        return static_cast<ID3D11Device*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
    }

    ID3D11DeviceContext* GetNativeDX11Context(IRHIDevice* pDevice)
    {
        if (!pDevice || pDevice->GetBackend() != eRHIBackend::DX11)
            return nullptr;
        return static_cast<ID3D11DeviceContext*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext));
    }

    IDXGISwapChain* GetNativeDX11SwapChain(IRHIDevice* pDevice)
    {
        if (!pDevice || pDevice->GetBackend() != eRHIBackend::DX11)
            return nullptr;
        return static_cast<IDXGISwapChain*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11SwapChain));
    }

    bool_t CompileShaderBlob(
        const wchar_t* pPath,
        const char* pEntry,
        const char* pProfile,
        ComPtr<ID3DBlob>& outBlob)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        ComPtr<ID3DBlob> pErrors;
        const HRESULT hr = D3DCompileFromFile(
            pPath,
            nullptr,
            D3D_COMPILE_STANDARD_FILE_INCLUDE,
            pEntry,
            pProfile,
            flags,
            0,
            outBlob.ReleaseAndGetAddressOf(),
            pErrors.ReleaseAndGetAddressOf());

        if (FAILED(hr))
        {
            OutputDebugStringA("[PostFxPass] shader compile failed\n");
            if (pErrors)
            {
                OutputDebugStringA(
                    static_cast<const char*>(pErrors->GetBufferPointer()));
            }
            outBlob.Reset();
            return false;
        }
        return true;
    }

    PostFxParams ClampPostFxParams(PostFxParams params)
    {
        params.fGamma = std::clamp(params.fGamma, 0.5f, 2.5f);
        params.fSaturation = std::clamp(params.fSaturation, 0.f, 2.f);
        params.fGradeStrength = std::clamp(params.fGradeStrength, 0.f, 1.f);
        params.vTint.x = std::clamp(params.vTint.x, 0.f, 2.f);
        params.vTint.y = std::clamp(params.vTint.y, 0.f, 2.f);
        params.vTint.z = std::clamp(params.vTint.z, 0.f, 2.f);
        params.fVignetteStrength = std::clamp(params.fVignetteStrength, 0.f, 1.f);
        params.fVignetteInner = std::clamp(params.fVignetteInner, 0.f, 0.95f);
        params.fVignetteOuter = std::clamp(
            params.fVignetteOuter,
            params.fVignetteInner + 0.01f,
            1.f);
        params.fBloomThreshold = std::clamp(params.fBloomThreshold, 0.f, 1.f);
        params.fBloomIntensity = std::clamp(params.fBloomIntensity, 0.f, 2.f);
        params.fBloomSoftKnee = std::clamp(params.fBloomSoftKnee, 0.f, 1.f);
        return params;
    }

    struct DX11PipelineStateGuard final
    {
        explicit DX11PipelineStateGuard(ID3D11DeviceContext* pInContext)
            : pContext(pInContext)
        {
            if (!pContext)
                return;

            ID3D11RenderTargetView* pRawRenderTargets[
                D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
            pContext->OMGetRenderTargets(
                D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,
                pRawRenderTargets,
                pDepthStencil.ReleaseAndGetAddressOf());
            for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            {
                pRenderTargets[i].Attach(pRawRenderTargets[i]);
                if (pRawRenderTargets[i])
                    renderTargetCount = i + 1;
            }

            viewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            pContext->RSGetViewports(viewportCount > 0 ? &viewportCount : nullptr, viewports);

            pContext->IAGetInputLayout(pInputLayout.ReleaseAndGetAddressOf());
            pContext->IAGetPrimitiveTopology(&topology);
            ID3D11Buffer* pRawVertexBuffer = nullptr;
            pContext->IAGetVertexBuffers(
                0,
                1,
                &pRawVertexBuffer,
                &vertexStride,
                &vertexOffset);
            pVertexBuffer.Attach(pRawVertexBuffer);

            pContext->VSGetShader(pVertexShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
            pContext->HSGetShader(pHullShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
            pContext->DSGetShader(pDomainShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
            pContext->GSGetShader(pGeometryShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
            pContext->PSGetShader(pPixelShader.ReleaseAndGetAddressOf(), nullptr, nullptr);

            ID3D11Buffer* pRawConstantBuffer = nullptr;
            pContext->PSGetConstantBuffers(0, 1, &pRawConstantBuffer);
            pPixelConstantBuffer.Attach(pRawConstantBuffer);

            ID3D11ShaderResourceView* pRawSRVs[2] = { nullptr, nullptr };
            pContext->PSGetShaderResources(0, 2, pRawSRVs);
            pPixelSRV0.Attach(pRawSRVs[0]);
            pPixelSRV1.Attach(pRawSRVs[1]);

            ID3D11SamplerState* pRawSampler = nullptr;
            pContext->PSGetSamplers(0, 1, &pRawSampler);
            pPixelSampler.Attach(pRawSampler);

            pContext->OMGetBlendState(
                pBlendState.ReleaseAndGetAddressOf(),
                blendFactor,
                &sampleMask);
            pContext->OMGetDepthStencilState(
                pDepthStencilState.ReleaseAndGetAddressOf(),
                &stencilRef);
            pContext->RSGetState(pRasterizerState.ReleaseAndGetAddressOf());
        }

        ~DX11PipelineStateGuard()
        {
            if (!pContext)
                return;

            ID3D11RenderTargetView* pRawRenderTargets[
                D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
            for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
                pRawRenderTargets[i] = pRenderTargets[i].Get();
            pContext->OMSetRenderTargets(
                renderTargetCount,
                renderTargetCount > 0 ? pRawRenderTargets : nullptr,
                pDepthStencil.Get());
            if (viewportCount > 0)
                pContext->RSSetViewports(viewportCount, viewports);

            ID3D11Buffer* pRawVertexBuffer = pVertexBuffer.Get();
            pContext->IASetVertexBuffers(
                0,
                1,
                &pRawVertexBuffer,
                &vertexStride,
                &vertexOffset);
            pContext->IASetInputLayout(pInputLayout.Get());
            pContext->IASetPrimitiveTopology(topology);

            pContext->VSSetShader(pVertexShader.Get(), nullptr, 0);
            pContext->HSSetShader(pHullShader.Get(), nullptr, 0);
            pContext->DSSetShader(pDomainShader.Get(), nullptr, 0);
            pContext->GSSetShader(pGeometryShader.Get(), nullptr, 0);
            pContext->PSSetShader(pPixelShader.Get(), nullptr, 0);

            ID3D11Buffer* pRawConstantBuffer = pPixelConstantBuffer.Get();
            pContext->PSSetConstantBuffers(0, 1, &pRawConstantBuffer);

            ID3D11ShaderResourceView* pRawSRVs[2] = {
                pPixelSRV0.Get(),
                pPixelSRV1.Get()
            };
            pContext->PSSetShaderResources(0, 2, pRawSRVs);

            ID3D11SamplerState* pRawSampler = pPixelSampler.Get();
            pContext->PSSetSamplers(0, 1, &pRawSampler);

            pContext->OMSetBlendState(pBlendState.Get(), blendFactor, sampleMask);
            pContext->OMSetDepthStencilState(pDepthStencilState.Get(), stencilRef);
            pContext->RSSetState(pRasterizerState.Get());
        }

        ID3D11DeviceContext* pContext = nullptr;
        ComPtr<ID3D11RenderTargetView> pRenderTargets[
            D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
        UINT renderTargetCount = 0;
        ComPtr<ID3D11DepthStencilView> pDepthStencil;
        D3D11_VIEWPORT viewports[
            D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        UINT viewportCount = 0;

        ComPtr<ID3D11InputLayout> pInputLayout;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        ComPtr<ID3D11Buffer> pVertexBuffer;
        UINT vertexStride = 0;
        UINT vertexOffset = 0;

        ComPtr<ID3D11VertexShader> pVertexShader;
        ComPtr<ID3D11HullShader> pHullShader;
        ComPtr<ID3D11DomainShader> pDomainShader;
        ComPtr<ID3D11GeometryShader> pGeometryShader;
        ComPtr<ID3D11PixelShader> pPixelShader;
        ComPtr<ID3D11Buffer> pPixelConstantBuffer;
        ComPtr<ID3D11ShaderResourceView> pPixelSRV0;
        ComPtr<ID3D11ShaderResourceView> pPixelSRV1;
        ComPtr<ID3D11SamplerState> pPixelSampler;
        ComPtr<ID3D11BlendState> pBlendState;
        FLOAT blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
        UINT sampleMask = 0xffffffffu;
        ComPtr<ID3D11DepthStencilState> pDepthStencilState;
        UINT stencilRef = 0;
        ComPtr<ID3D11RasterizerState> pRasterizerState;
    };
}

struct CPostFxPass::Impl
{
    bool_t EnsureFrameResources(
        ID3D11Device* pDevice,
        const D3D11_TEXTURE2D_DESC& sourceDesc)
    {
        if (!pDevice || sourceDesc.Width == 0 || sourceDesc.Height == 0)
            return false;
        if (sourceDesc.MipLevels != 1u ||
            sourceDesc.ArraySize != 1u ||
            sourceDesc.SampleDesc.Count != 1u ||
            sourceDesc.SampleDesc.Quality != 0u)
        {
            OutputDebugStringA(
                "[PostFxPass] expected a single-mip, single-slice, non-MSAA backbuffer\n");
            return false;
        }

        const u32_t nextBloomWidth = std::max(1u, (sourceDesc.Width + 3u) / 4u);
        const u32_t nextBloomHeight = std::max(1u, (sourceDesc.Height + 3u) / 4u);
        const bool_t bResourcesMatch =
            pSceneTexture && pSceneSRV &&
            pBloomPingTexture && pBloomPingRTV && pBloomPingSRV &&
            pBloomPongTexture && pBloomPongRTV && pBloomPongSRV &&
            width == sourceDesc.Width &&
            height == sourceDesc.Height &&
            format == sourceDesc.Format &&
            bloomWidth == nextBloomWidth &&
            bloomHeight == nextBloomHeight;
        if (bResourcesMatch)
            return true;

        pSceneSRV.Reset();
        pSceneTexture.Reset();
        pBloomPingSRV.Reset();
        pBloomPingRTV.Reset();
        pBloomPingTexture.Reset();
        pBloomPongSRV.Reset();
        pBloomPongRTV.Reset();
        pBloomPongTexture.Reset();

        D3D11_TEXTURE2D_DESC sceneDesc = sourceDesc;
        sceneDesc.Usage = D3D11_USAGE_DEFAULT;
        sceneDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        sceneDesc.CPUAccessFlags = 0;
        sceneDesc.MiscFlags = 0;
        if (FAILED(pDevice->CreateTexture2D(
            &sceneDesc,
            nullptr,
            pSceneTexture.ReleaseAndGetAddressOf())))
        {
            return false;
        }
        if (FAILED(pDevice->CreateShaderResourceView(
            pSceneTexture.Get(),
            nullptr,
            pSceneSRV.ReleaseAndGetAddressOf())))
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC bloomDesc{};
        bloomDesc.Width = nextBloomWidth;
        bloomDesc.Height = nextBloomHeight;
        bloomDesc.MipLevels = 1;
        bloomDesc.ArraySize = 1;
        bloomDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        bloomDesc.SampleDesc.Count = 1;
        bloomDesc.Usage = D3D11_USAGE_DEFAULT;
        bloomDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        auto CreateBloomTarget = [&](ComPtr<ID3D11Texture2D>& pTexture,
            ComPtr<ID3D11RenderTargetView>& pRTV,
            ComPtr<ID3D11ShaderResourceView>& pSRV) -> bool_t
        {
            if (FAILED(pDevice->CreateTexture2D(
                &bloomDesc,
                nullptr,
                pTexture.ReleaseAndGetAddressOf())))
            {
                return false;
            }
            if (FAILED(pDevice->CreateRenderTargetView(
                pTexture.Get(),
                nullptr,
                pRTV.ReleaseAndGetAddressOf())))
            {
                return false;
            }
            return SUCCEEDED(pDevice->CreateShaderResourceView(
                pTexture.Get(),
                nullptr,
                pSRV.ReleaseAndGetAddressOf()));
        };

        if (!CreateBloomTarget(pBloomPingTexture, pBloomPingRTV, pBloomPingSRV) ||
            !CreateBloomTarget(pBloomPongTexture, pBloomPongRTV, pBloomPongSRV))
        {
            return false;
        }

        width = sourceDesc.Width;
        height = sourceDesc.Height;
        format = sourceDesc.Format;
        bloomWidth = nextBloomWidth;
        bloomHeight = nextBloomHeight;

        bloomViewport = {};
        bloomViewport.Width = static_cast<FLOAT>(bloomWidth);
        bloomViewport.Height = static_cast<FLOAT>(bloomHeight);
        bloomViewport.MinDepth = 0.f;
        bloomViewport.MaxDepth = 1.f;
        return true;
    }

    u32_t width = 0;
    u32_t height = 0;
    u32_t bloomWidth = 0;
    u32_t bloomHeight = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    ComPtr<ID3D11Texture2D> pSceneTexture;
    ComPtr<ID3D11ShaderResourceView> pSceneSRV;

    ComPtr<ID3D11Texture2D> pBloomPingTexture;
    ComPtr<ID3D11RenderTargetView> pBloomPingRTV;
    ComPtr<ID3D11ShaderResourceView> pBloomPingSRV;
    ComPtr<ID3D11Texture2D> pBloomPongTexture;
    ComPtr<ID3D11RenderTargetView> pBloomPongRTV;
    ComPtr<ID3D11ShaderResourceView> pBloomPongSRV;

    ComPtr<ID3D11VertexShader> pFullscreenVS;
    ComPtr<ID3D11PixelShader> pBloomExtractPS;
    ComPtr<ID3D11PixelShader> pBloomBlurPS;
    ComPtr<ID3D11PixelShader> pCompositePS;
    ComPtr<ID3D11Buffer> pPostFxCB;
    ComPtr<ID3D11SamplerState> pLinearClampSampler;
    ComPtr<ID3D11BlendState> pOpaqueBlendState;
    ComPtr<ID3D11DepthStencilState> pDepthDisabledState;
    ComPtr<ID3D11RasterizerState> pCullNoneState;
    D3D11_VIEWPORT bloomViewport{};
};

CPostFxPass::CPostFxPass()
    : m_pImpl(std::make_unique<Impl>())
{
}

CPostFxPass::~CPostFxPass() = default;

std::unique_ptr<CPostFxPass> CPostFxPass::Create(
    IRHIDevice* pDevice,
    u32_t width,
    u32_t height)
{
    auto pPass = std::unique_ptr<CPostFxPass>(new CPostFxPass());
    if (!pPass->Initialize(pDevice, width, height))
        return nullptr;
    return pPass;
}

bool_t CPostFxPass::Initialize(IRHIDevice* pDevice, u32_t width, u32_t height)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    IDXGISwapChain* pSwapChain = GetNativeDX11SwapChain(pDevice);
    if (!pNativeDevice || !pSwapChain || width == 0 || height == 0)
        return false;

    wchar_t shaderPath[MAX_PATH]{};
    if (!WintersResolveContentPath(
        L"Shaders/PostFx/PostFx.hlsl",
        shaderPath,
        MAX_PATH))
    {
        return false;
    }

    ComPtr<ID3DBlob> pFullscreenVSBlob;
    ComPtr<ID3DBlob> pBloomExtractPSBlob;
    ComPtr<ID3DBlob> pBloomBlurPSBlob;
    ComPtr<ID3DBlob> pCompositePSBlob;
    if (!CompileShaderBlob(
            shaderPath,
            "VS_Fullscreen",
            "vs_5_0",
            pFullscreenVSBlob) ||
        !CompileShaderBlob(
            shaderPath,
            "PS_BloomExtract",
            "ps_5_0",
            pBloomExtractPSBlob) ||
        !CompileShaderBlob(
            shaderPath,
            "PS_BloomBlur",
            "ps_5_0",
            pBloomBlurPSBlob) ||
        !CompileShaderBlob(
            shaderPath,
            "PS_Composite",
            "ps_5_0",
            pCompositePSBlob))
    {
        return false;
    }

    if (FAILED(pNativeDevice->CreateVertexShader(
            pFullscreenVSBlob->GetBufferPointer(),
            pFullscreenVSBlob->GetBufferSize(),
            nullptr,
            m_pImpl->pFullscreenVS.ReleaseAndGetAddressOf())) ||
        FAILED(pNativeDevice->CreatePixelShader(
            pBloomExtractPSBlob->GetBufferPointer(),
            pBloomExtractPSBlob->GetBufferSize(),
            nullptr,
            m_pImpl->pBloomExtractPS.ReleaseAndGetAddressOf())) ||
        FAILED(pNativeDevice->CreatePixelShader(
            pBloomBlurPSBlob->GetBufferPointer(),
            pBloomBlurPSBlob->GetBufferSize(),
            nullptr,
            m_pImpl->pBloomBlurPS.ReleaseAndGetAddressOf())) ||
        FAILED(pNativeDevice->CreatePixelShader(
            pCompositePSBlob->GetBufferPointer(),
            pCompositePSBlob->GetBufferSize(),
            nullptr,
            m_pImpl->pCompositePS.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(CBPostFxData);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(pNativeDevice->CreateBuffer(
        &cbDesc,
        nullptr,
        m_pImpl->pPostFxCB.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(pNativeDevice->CreateSamplerState(
        &samplerDesc,
        m_pImpl->pLinearClampSampler.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(pNativeDevice->CreateBlendState(
        &blendDesc,
        m_pImpl->pOpaqueBlendState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depthDesc.StencilEnable = FALSE;
    if (FAILED(pNativeDevice->CreateDepthStencilState(
        &depthDesc,
        m_pImpl->pDepthDisabledState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    if (FAILED(pNativeDevice->CreateRasterizerState(
        &rasterDesc,
        m_pImpl->pCullNoneState.ReleaseAndGetAddressOf())))
    {
        return false;
    }

    ComPtr<ID3D11Texture2D> pBackBuffer;
    if (FAILED(pSwapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(pBackBuffer.ReleaseAndGetAddressOf()))))
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC backBufferDesc{};
    pBackBuffer->GetDesc(&backBufferDesc);
    if (backBufferDesc.Width != width || backBufferDesc.Height != height)
    {
        OutputDebugStringA(
            "[PostFxPass] requested size differs from the current backbuffer; using backbuffer size\n");
    }
    return m_pImpl->EnsureFrameResources(pNativeDevice, backBufferDesc);
}

void CPostFxPass::SetParams(const PostFxParams& params)
{
    m_Params = ClampPostFxParams(params);
}

void CPostFxPass::Execute(IRHIDevice* pDevice)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    ID3D11DeviceContext* pContext = GetNativeDX11Context(pDevice);
    if (!m_bEnabled || !pNativeDevice || !pContext || !m_pImpl)
        return;

    DX11PipelineStateGuard stateGuard(pContext);
    if (!stateGuard.pRenderTargets[0] || stateGuard.viewportCount == 0)
        return;

    ComPtr<ID3D11Resource> pSourceResource;
    stateGuard.pRenderTargets[0]->GetResource(
        pSourceResource.ReleaseAndGetAddressOf());
    ComPtr<ID3D11Texture2D> pSourceTexture;
    if (FAILED(pSourceResource.As(&pSourceTexture)) || !pSourceTexture)
        return;

    D3D11_TEXTURE2D_DESC sourceDesc{};
    pSourceTexture->GetDesc(&sourceDesc);
    if (!m_pImpl->EnsureFrameResources(pNativeDevice, sourceDesc))
        return;

    ID3D11ShaderResourceView* pNullSRVs[2] = { nullptr, nullptr };
    pContext->PSSetShaderResources(0, 2, pNullSRVs);
    pContext->OMSetRenderTargets(0, nullptr, nullptr);
    pContext->CopyResource(m_pImpl->pSceneTexture.Get(), pSourceTexture.Get());

    pContext->IASetInputLayout(nullptr);
    ID3D11Buffer* pNullVertexBuffer = nullptr;
    UINT zero = 0;
    pContext->IASetVertexBuffers(0, 1, &pNullVertexBuffer, &zero, &zero);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->VSSetShader(m_pImpl->pFullscreenVS.Get(), nullptr, 0);
    pContext->HSSetShader(nullptr, nullptr, 0);
    pContext->DSSetShader(nullptr, nullptr, 0);
    pContext->GSSetShader(nullptr, nullptr, 0);
    pContext->OMSetBlendState(
        m_pImpl->pOpaqueBlendState.Get(),
        nullptr,
        0xffffffffu);
    pContext->OMSetDepthStencilState(m_pImpl->pDepthDisabledState.Get(), 0);
    pContext->RSSetState(m_pImpl->pCullNoneState.Get());

    ID3D11SamplerState* pSampler = m_pImpl->pLinearClampSampler.Get();
    pContext->PSSetSamplers(0, 1, &pSampler);
    ID3D11Buffer* pConstantBuffer = m_pImpl->pPostFxCB.Get();
    pContext->PSSetConstantBuffers(0, 1, &pConstantBuffer);

    const PostFxParams params = ClampPostFxParams(m_Params);
    auto UpdateConstants = [&](f32_t fTexelX,
        f32_t fTexelY,
        f32_t fDirectionX,
        f32_t fDirectionY) -> bool_t
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(pContext->Map(
            m_pImpl->pPostFxCB.Get(),
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &mapped)))
        {
            return false;
        }

        CBPostFxData data{};
        data.fSourceTexelSize[0] = fTexelX;
        data.fSourceTexelSize[1] = fTexelY;
        data.fBlurDirection[0] = fDirectionX;
        data.fBlurDirection[1] = fDirectionY;
        data.fGamma = params.fGamma;
        data.fSaturation = params.fSaturation;
        data.fGradeStrength = params.fGradeStrength;
        data.fVignetteStrength = params.fVignetteStrength;
        data.fTint[0] = params.vTint.x;
        data.fTint[1] = params.vTint.y;
        data.fTint[2] = params.vTint.z;
        data.fVignetteInner = params.fVignetteInner;
        data.fVignetteOuter = params.fVignetteOuter;
        data.fBloomThreshold = params.fBloomThreshold;
        data.fBloomIntensity = params.bBloomEnabled
            ? params.fBloomIntensity
            : 0.f;
        data.fBloomSoftKnee = params.fBloomSoftKnee;
        std::memcpy(mapped.pData, &data, sizeof(data));
        pContext->Unmap(m_pImpl->pPostFxCB.Get(), 0);
        return true;
    };

    auto DrawFullscreen = [&](ID3D11RenderTargetView* pTarget,
        const D3D11_VIEWPORT& viewport,
        ID3D11PixelShader* pPixelShader,
        ID3D11ShaderResourceView* pSource0,
        ID3D11ShaderResourceView* pSource1)
    {
        pContext->PSSetShaderResources(0, 2, pNullSRVs);
        pContext->OMSetRenderTargets(1, &pTarget, nullptr);
        pContext->RSSetViewports(1, &viewport);
        ID3D11ShaderResourceView* pSources[2] = { pSource0, pSource1 };
        pContext->PSSetShaderResources(0, 2, pSources);
        pContext->PSSetShader(pPixelShader, nullptr, 0);
        RenderFrameStats::AddDraw(3u);
        pContext->Draw(3, 0);
        pContext->PSSetShaderResources(0, 2, pNullSRVs);
    };

    if (params.bBloomEnabled && params.fBloomIntensity > 0.0001f)
    {
        if (!UpdateConstants(
            1.f / static_cast<f32_t>(m_pImpl->width),
            1.f / static_cast<f32_t>(m_pImpl->height),
            0.f,
            0.f))
        {
            return;
        }
        DrawFullscreen(
            m_pImpl->pBloomPingRTV.Get(),
            m_pImpl->bloomViewport,
            m_pImpl->pBloomExtractPS.Get(),
            m_pImpl->pSceneSRV.Get(),
            nullptr);

        if (!UpdateConstants(
            1.f / static_cast<f32_t>(m_pImpl->bloomWidth),
            1.f / static_cast<f32_t>(m_pImpl->bloomHeight),
            1.f,
            0.f))
        {
            return;
        }
        DrawFullscreen(
            m_pImpl->pBloomPongRTV.Get(),
            m_pImpl->bloomViewport,
            m_pImpl->pBloomBlurPS.Get(),
            m_pImpl->pBloomPingSRV.Get(),
            nullptr);

        if (!UpdateConstants(
            1.f / static_cast<f32_t>(m_pImpl->bloomWidth),
            1.f / static_cast<f32_t>(m_pImpl->bloomHeight),
            0.f,
            1.f))
        {
            return;
        }
        DrawFullscreen(
            m_pImpl->pBloomPingRTV.Get(),
            m_pImpl->bloomViewport,
            m_pImpl->pBloomBlurPS.Get(),
            m_pImpl->pBloomPongSRV.Get(),
            nullptr);
    }

    if (!UpdateConstants(
        1.f / static_cast<f32_t>(m_pImpl->width),
        1.f / static_cast<f32_t>(m_pImpl->height),
        0.f,
        0.f))
    {
        return;
    }
    DrawFullscreen(
        stateGuard.pRenderTargets[0].Get(),
        stateGuard.viewports[0],
        m_pImpl->pCompositePS.Get(),
        m_pImpl->pSceneSRV.Get(),
        params.bBloomEnabled && params.fBloomIntensity > 0.0001f
            ? m_pImpl->pBloomPingSRV.Get()
            : nullptr);
}
