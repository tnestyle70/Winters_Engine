Session - DX11 완성 프레임을 LDR 후처리 체인에 태워 passthrough를 먼저 고정하고 색보정·비네트·quarter-resolution bloom을 F1에서 독립 튜닝한다.

1. 반영해야 하는 코드

1-1. C:/Users/user/Desktop/Winters/Shaders/PostFx/PostFx.hlsl

새 파일:

```hlsl
Texture2D<float4> g_SceneColor : register(t0);
Texture2D<float4> g_BloomColor : register(t1);
SamplerState g_LinearClamp : register(s0);

cbuffer CBPostFx : register(b0)
{
    float2 g_vSourceTexelSize;
    float2 g_vBlurDirection;

    float g_fGamma;
    float g_fSaturation;
    float g_fGradeStrength;
    float g_fVignetteStrength;

    float3 g_vTint;
    float g_fVignetteInner;

    float g_fVignetteOuter;
    float g_fBloomThreshold;
    float g_fBloomIntensity;
    float g_fBloomSoftKnee;
};

struct VS_OUTPUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
};

VS_OUTPUT VS_Fullscreen(uint uVertexID : SV_VertexID)
{
    VS_OUTPUT output;
    const float2 vertex = float2((uVertexID << 1) & 2, uVertexID & 2);
    output.vPosition = float4(
        vertex.x * 2.f - 1.f,
        1.f - vertex.y * 2.f,
        0.f,
        1.f);
    output.vTexCoord = vertex;
    return output;
}

float3 ExtractBloom(float3 color)
{
    const float brightness = max(max(color.r, color.g), color.b);
    const float threshold = max(g_fBloomThreshold, 0.f);
    const float knee = max(threshold * g_fBloomSoftKnee, 0.0001f);
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.f, 2.f * knee);
    soft = soft * soft / (4.f * knee + 0.0001f);
    const float contribution = max(brightness - threshold, soft) /
        max(brightness, 0.0001f);
    return color * contribution;
}

float4 PS_BloomExtract(VS_OUTPUT input) : SV_TARGET
{
    const float2 offset = g_vSourceTexelSize * 0.5f;
    const float3 color = (
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2(-offset.x, -offset.y), 0.f).rgb +
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2( offset.x, -offset.y), 0.f).rgb +
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2(-offset.x,  offset.y), 0.f).rgb +
        g_SceneColor.SampleLevel(g_LinearClamp, input.vTexCoord + float2( offset.x,  offset.y), 0.f).rgb) * 0.25f;
    return float4(ExtractBloom(color), 1.f);
}

float4 PS_BloomBlur(VS_OUTPUT input) : SV_TARGET
{
    static const float kWeights[5] = {
        0.2270270270f,
        0.1945945946f,
        0.1216216216f,
        0.0540540541f,
        0.0162162162f
    };

    float3 color = g_SceneColor.SampleLevel(
        g_LinearClamp,
        input.vTexCoord,
        0.f).rgb * kWeights[0];

    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        const float2 offset = g_vBlurDirection * g_vSourceTexelSize * (float)i;
        color += g_SceneColor.SampleLevel(
            g_LinearClamp,
            input.vTexCoord + offset,
            0.f).rgb * kWeights[i];
        color += g_SceneColor.SampleLevel(
            g_LinearClamp,
            input.vTexCoord - offset,
            0.f).rgb * kWeights[i];
    }

    return float4(color, 1.f);
}

float4 LoadSceneColor(float4 vPosition)
{
    uint uWidth = 0;
    uint uHeight = 0;
    g_SceneColor.GetDimensions(uWidth, uHeight);
    const uint2 maxPixel = uint2(max(uWidth, 1u) - 1u, max(uHeight, 1u) - 1u);
    const uint2 pixel = min(uint2(vPosition.xy), maxPixel);
    return g_SceneColor.Load(int3(pixel, 0));
}

float4 PS_Composite(VS_OUTPUT input) : SV_TARGET
{
    const float4 source = LoadSceneColor(input.vPosition);
    const bool bUseBloom = g_fBloomIntensity > 0.0001f;
    const bool bUseGrade = g_fGradeStrength > 0.0001f;
    if (!bUseBloom && !bUseGrade)
        return source;

    float3 color = source.rgb;
    if (bUseBloom)
    {
        color += g_BloomColor.SampleLevel(
            g_LinearClamp,
            input.vTexCoord,
            0.f).rgb * g_fBloomIntensity;
    }

    if (bUseGrade)
    {
        float3 graded = color;
        const float gamma = max(g_fGamma, 0.001f);
        if (abs(gamma - 1.f) > 0.0001f)
            graded = pow(max(graded, 0.f), 1.f / gamma);

        const float luminance = dot(graded, float3(0.299f, 0.587f, 0.114f));
        graded = lerp(luminance.xxx, graded, g_fSaturation);
        graded *= g_vTint;

        const float distanceFromCenter = length(input.vTexCoord - 0.5f);
        const float vignette = 1.f - g_fVignetteStrength * smoothstep(
            g_fVignetteInner,
            g_fVignetteOuter,
            distanceFromCenter);
        graded *= vignette;
        color = lerp(color, graded, saturate(g_fGradeStrength));
    }

    return float4(saturate(color), source.a);
}
```

1-2. C:/Users/user/Desktop/Winters/Engine/Public/Renderer/PostFxPass.h

새 파일:

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"

#include <memory>

namespace Engine
{
    struct WINTERS_ENGINE PostFxParams
    {
        f32_t fGamma = 1.f;
        f32_t fSaturation = 1.f;
        f32_t fGradeStrength = 0.f;
        Vec3 vTint{ 1.f, 1.f, 1.f };

        f32_t fVignetteStrength = 0.f;
        f32_t fVignetteInner = 0.35f;
        f32_t fVignetteOuter = 0.75f;

        bool_t bBloomEnabled = false;
        f32_t fBloomThreshold = 0.85f;
        f32_t fBloomIntensity = 0.6f;
        f32_t fBloomSoftKnee = 0.1f;
    };

    class WINTERS_ENGINE CPostFxPass final
    {
    public:
        ~CPostFxPass();

        CPostFxPass(const CPostFxPass&) = delete;
        CPostFxPass& operator=(const CPostFxPass&) = delete;
        CPostFxPass(CPostFxPass&&) noexcept = default;
        CPostFxPass& operator=(CPostFxPass&&) noexcept = default;

        static std::unique_ptr<CPostFxPass> Create(
            IRHIDevice* pDevice,
            u32_t width,
            u32_t height);

        void Execute(IRHIDevice* pDevice);

        bool_t GetEnabled() const { return m_bEnabled; }
        void SetEnabled(bool_t bEnabled) { m_bEnabled = bEnabled; }

        PostFxParams GetParams() const { return m_Params; }
        void SetParams(const PostFxParams& params);

    private:
        CPostFxPass();
        bool_t Initialize(IRHIDevice* pDevice, u32_t width, u32_t height);

        struct Impl;
        std::unique_ptr<Impl> m_pImpl;

        bool_t m_bEnabled = false;
        PostFxParams m_Params{};
    };
}
```

1-3. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/PostFxPass.cpp

새 파일:

```cpp
#include "Renderer/PostFxPass.h"

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
```

1-4. C:/Users/user/Desktop/Winters/Client/Public/Scene/Scene_InGame.h

기존 코드:

```cpp
#include "Renderer/NormalPass.h"
#include "Renderer/SSAOPass.h"
#include "Renderer/FogOfWarRenderer.h"
```

아래로 교체:

```cpp
#include "Renderer/NormalPass.h"
#include "Renderer/SSAOPass.h"
#include "Renderer/PostFxPass.h"
#include "Renderer/FogOfWarRenderer.h"
```

기존 코드:

```cpp
    void SetSSAOThicknessHeuristic(f32_t v)
    {
        if (m_pSSAOPass) m_pSSAOPass->SetThicknessHeuristic(v);
    }

    //Irelia - Q Dash Tuner Approach
```

아래로 교체:

```cpp
    void SetSSAOThicknessHeuristic(f32_t v)
    {
        if (m_pSSAOPass) m_pSSAOPass->SetThicknessHeuristic(v);
    }

    bool_t IsPostFxAvailable() const { return m_pPostFxPass != nullptr; }
    bool_t GetPostFxEnabled() const
    {
        return m_pPostFxPass ? m_pPostFxPass->GetEnabled() : false;
    }
    void SetPostFxEnabled(bool_t bEnabled)
    {
        if (m_pPostFxPass) m_pPostFxPass->SetEnabled(bEnabled);
    }
    Engine::PostFxParams GetPostFxParams() const
    {
        return m_pPostFxPass
            ? m_pPostFxPass->GetParams()
            : Engine::PostFxParams{};
    }
    void SetPostFxParams(const Engine::PostFxParams& params)
    {
        if (m_pPostFxPass) m_pPostFxPass->SetParams(params);
    }

    //Irelia - Q Dash Tuner Approach
```

기존 코드:

```cpp
    std::unique_ptr<Engine::CNormalPass> m_pNormalPass;
    std::unique_ptr<Engine::CSSAOPass> m_pSSAOPass;
    std::unique_ptr<CFogOfWarRenderer> m_pFogOfWarRenderer;
```

아래로 교체:

```cpp
    std::unique_ptr<Engine::CNormalPass> m_pNormalPass;
    std::unique_ptr<Engine::CSSAOPass> m_pSSAOPass;
    std::unique_ptr<Engine::CPostFxPass> m_pPostFxPass;
    std::unique_ptr<CFogOfWarRenderer> m_pFogOfWarRenderer;
```

1-5. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameLifecycle.cpp

기존 코드:

```cpp
            m_pWhiteTexture = CTexture::CreateDefault(pRhiDevice);
            m_pNormalPass = Engine::CNormalPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            m_pSSAOPass = Engine::CSSAOPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            if (m_pSSAOPass)
            {
                m_pSSAOPass->SetEnabled(false);
                m_pSSAOPass->SetRadius(1.1f);
                m_pSSAOPass->SetIntensity(1.25f);
            }
            m_pFogOfWarRenderer = CFogOfWarRenderer::Create(
                pRhiDevice, Engine::CVisionSystem::FOW_TEX_DIM);
```

아래로 교체:

```cpp
            m_pWhiteTexture = CTexture::CreateDefault(pRhiDevice);
            m_pNormalPass = Engine::CNormalPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            m_pSSAOPass = Engine::CSSAOPass::Create(pRhiDevice, g_iWinSizeX, g_iWinSizeY);
            if (m_pSSAOPass)
            {
                m_pSSAOPass->SetEnabled(false);
                m_pSSAOPass->SetRadius(1.1f);
                m_pSSAOPass->SetIntensity(1.25f);
            }
            m_pPostFxPass = Engine::CPostFxPass::Create(
                pRhiDevice,
                g_iWinSizeX,
                g_iWinSizeY);
            if (m_pPostFxPass)
                m_pPostFxPass->SetEnabled(false);
            m_pFogOfWarRenderer = CFogOfWarRenderer::Create(
                pRhiDevice, Engine::CVisionSystem::FOW_TEX_DIM);
```

기존 코드:

```cpp
    m_pSSAOPass.reset();
    m_pNormalPass.reset();
    m_pFogOfWarRenderer.reset();
```

아래로 교체:

```cpp
    m_pPostFxPass.reset();
    m_pSSAOPass.reset();
    m_pNormalPass.reset();
    m_pFogOfWarRenderer.reset();
```

1-6. C:/Users/user/Desktop/Winters/Client/Private/Scene/Scene_InGameRender.cpp

기존 코드:

```cpp
    if (m_pFxMeshSystem && m_pCamera)
        m_pFxMeshSystem->Render(m_World, m_pCamera.get());
    if (m_pFxBeamSystem && m_pCamera)
        m_pFxBeamSystem->Render(m_World, m_pCamera.get());
    if (m_pFxSystem && m_pCamera)
        m_pFxSystem->Render(m_World, m_pCamera.get());

    {
        WINTERS_PROFILE_SCOPE("UIOverlay::Render");
        CGameInstance::Get()->UI_Render_Overlay(vp);
    }
```

아래로 교체:

```cpp
    if (m_pFxMeshSystem && m_pCamera)
        m_pFxMeshSystem->Render(m_World, m_pCamera.get());
    if (m_pFxBeamSystem && m_pCamera)
        m_pFxBeamSystem->Render(m_World, m_pCamera.get());
    if (m_pFxSystem && m_pCamera)
        m_pFxSystem->Render(m_World, m_pCamera.get());

    if (bUseDX11RHI && m_pPostFxPass && m_pPostFxPass->GetEnabled())
    {
        WINTERS_PROFILE_SCOPE("Render::PostFx");
        const Engine::PostFxParams params = m_pPostFxPass->GetParams();
        WINTERS_PROFILE_COUNT("PostFx::BloomEnabled", params.bBloomEnabled ? 1u : 0u);
        m_pPostFxPass->Execute(pDevice);
    }

    {
        WINTERS_PROFILE_SCOPE("UIOverlay::Render");
        CGameInstance::Get()->UI_Render_Overlay(vp);
    }
```

1-7. C:/Users/user/Desktop/Winters/Client/Private/UI/RenderDebug.cpp

기존 코드:

```cpp
        ImGui::SetNextWindowSize(ImVec2(340.f, 460.f), ImGuiCond_FirstUseEver);
```

아래로 교체:

```cpp
        ImGui::SetNextWindowSize(ImVec2(360.f, 680.f), ImGuiCond_FirstUseEver);
```

기존 코드:

```cpp
        ImGui::EndDisabled();

        ImGui::SeparatorText("Debug Draw");
```

아래로 교체:

```cpp
        ImGui::EndDisabled();

        ImGui::SeparatorText("PostFx (DX11 LDR)");
        const bool bPostFxAvailable = pScene->IsPostFxAvailable();
        if (!bPostFxAvailable)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.45f, 0.25f, 1.0f),
                "PostFx unavailable on this backend");
        }

        ImGui::BeginDisabled(!bPostFxAvailable);
        {
            bool bPostFxEnabled = pScene->GetPostFxEnabled();
            if (ImGui::Checkbox("PostFx", &bPostFxEnabled))
                pScene->SetPostFxEnabled(bPostFxEnabled);

            Engine::PostFxParams params = pScene->GetPostFxParams();
            bool bChanged = false;
            bChanged |= ImGui::SliderFloat(
                "Grade strength",
                &params.fGradeStrength,
                0.f,
                1.f,
                "%.2f");
            bChanged |= ImGui::SliderFloat(
                "Gamma (1 = neutral)",
                &params.fGamma,
                0.5f,
                2.5f,
                "%.2f");
            bChanged |= ImGui::SliderFloat(
                "Saturation",
                &params.fSaturation,
                0.f,
                2.f,
                "%.2f");
            bChanged |= ImGui::ColorEdit3("Tint", &params.vTint.x);
            bChanged |= ImGui::SliderFloat(
                "Vignette strength",
                &params.fVignetteStrength,
                0.f,
                1.f,
                "%.2f");
            bChanged |= ImGui::SliderFloat(
                "Vignette inner",
                &params.fVignetteInner,
                0.f,
                0.95f,
                "%.2f");
            bChanged |= ImGui::SliderFloat(
                "Vignette outer",
                &params.fVignetteOuter,
                0.01f,
                1.f,
                "%.2f");

            bChanged |= ImGui::Checkbox("LDR bloom", &params.bBloomEnabled);
            bChanged |= ImGui::SliderFloat(
                "Bloom threshold",
                &params.fBloomThreshold,
                0.f,
                1.f,
                "%.2f");
            bChanged |= ImGui::SliderFloat(
                "Bloom intensity",
                &params.fBloomIntensity,
                0.f,
                2.f,
                "%.2f");
            bChanged |= ImGui::SliderFloat(
                "Bloom soft knee",
                &params.fBloomSoftKnee,
                0.f,
                1.f,
                "%.2f");

            if (bChanged)
                pScene->SetPostFxParams(params);

            if (ImGui::Button("Passthrough"))
            {
                pScene->SetPostFxParams(Engine::PostFxParams{});
                pScene->SetPostFxEnabled(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("LoL Subtle"))
            {
                Engine::PostFxParams subtle{};
                subtle.fGamma = 1.f;
                subtle.fSaturation = 1.1f;
                subtle.fGradeStrength = 0.65f;
                subtle.vTint = { 1.02f, 1.f, 0.98f };
                subtle.fVignetteStrength = 0.16f;
                subtle.fVignetteInner = 0.36f;
                subtle.fVignetteOuter = 0.74f;
                subtle.bBloomEnabled = true;
                subtle.fBloomThreshold = 0.88f;
                subtle.fBloomIntensity = 0.45f;
                subtle.fBloomSoftKnee = 0.12f;
                pScene->SetPostFxParams(subtle);
                pScene->SetPostFxEnabled(true);
            }

            ImGui::TextWrapped(
                "LDR bloom spreads near-white pixels only; HDR emissive values above 1.0 are already clipped.");
        }
        ImGui::EndDisabled();

        ImGui::SeparatorText("Debug Draw");
```

2. 검증

미검증:
- 이번 세션은 Claude Handoff와 현재 DX11/Client/Engine 코드를 조사해 적용 계획만 작성했다.
- C++/HLSL/프로젝트 파일은 수정하지 않았고 빌드·셰이더 컴파일·런타임 캡처도 수행하지 않았다.

적용 전 충돌 확인:
- 현재 `Client/Public/Scene/Scene_InGame.h`, `Client/Private/Scene/Scene_InGameLifecycle.cpp`, `Client/Private/Scene/Scene_InGameRender.cpp`, `Engine/Include/Engine.vcxproj`가 다른 Claude 세션 변경과 겹친다. 그 세션이 commit 또는 Handoff 상태가 된 뒤 fresh `git status`와 이 문서 1-4~1-6의 기존 코드 anchor를 다시 확인한다.
- `.md/collab/ACTIVE_WORK_PACKETS.md`에서 기존 `Engine/**` 및 `Scene_InGame*` read-only packet이 종료됐는지 확인하고, 새 PostFx packet이 `Engine/Public/Renderer/PostFxPass.h`, `Engine/Private/Renderer/PostFxPass.cpp`, `Engine/Include/Engine.vcxproj`, `Engine/Include/Engine.vcxproj.filters`, `Client/Public/Scene/Scene_InGame.h`, `Client/Private/Scene/Scene_InGameLifecycle.cpp`, `Client/Private/Scene/Scene_InGameRender.cpp`, `Client/Private/UI/RenderDebug.cpp`, `Shaders/PostFx/PostFx.hlsl`를 예약한 뒤 구현한다.
- `EngineSDK/inc/**`는 직접 수정하지 않는다.

프로젝트/배포 확인:
- 새 `PostFxPass.h/.cpp`가 `Engine/Include/Engine.vcxproj`와 `.filters`의 NormalPass/SSAOPass 인접 renderer pass 그룹에 포함되는지 확인한다. `.md/계획서작성규칙.md`에 따라 XML 교체 블록은 이 계획에 넣지 않는다.
- `cmake/WintersEngine.cmake`는 `Engine/Private/*.cpp`와 `Engine/Public/*.h`를 `GLOB_RECURSE CONFIGURE_DEPENDS`로 수집하므로 새 빌드 membership 코드는 추가하지 않는다. IDE source group만 필요하면 PostFx 그룹 추가를 별도 정리한다.
- Engine 빌드 뒤 `UpdateLib.bat`으로 `Engine/Public/Renderer/PostFxPass.h`를 `EngineSDK/inc/Renderer/PostFxPass.h`에 동기화한다.
- Client PostBuild가 루트 `Shaders/**`를 재귀 복사하므로 별도 copy rule은 추가하지 않는다. 빌드 뒤 아래 두 파일의 hash가 같아야 한다.

```powershell
Get-FileHash Shaders/PostFx/PostFx.hlsl
Get-FileHash Client/Bin/Debug/Shaders/PostFx/PostFx.hlsl
```

셰이더 문법 검증:

```powershell
$fxc = Get-ChildItem `
    -Path "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\fxc.exe" |
    Sort-Object { [version]$_.Directory.Parent.Name } -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $fxc) { throw 'Windows SDK x64 fxc.exe was not found.' }

& $fxc /nologo /T vs_5_0 /E VS_Fullscreen /Fo "$env:TEMP/PostFxVS.cso" Shaders/PostFx/PostFx.hlsl
if ($LASTEXITCODE -ne 0) { throw 'VS_Fullscreen compile failed.' }
& $fxc /nologo /T ps_5_0 /E PS_BloomExtract /Fo "$env:TEMP/PostFxExtract.cso" Shaders/PostFx/PostFx.hlsl
if ($LASTEXITCODE -ne 0) { throw 'PS_BloomExtract compile failed.' }
& $fxc /nologo /T ps_5_0 /E PS_BloomBlur /Fo "$env:TEMP/PostFxBlur.cso" Shaders/PostFx/PostFx.hlsl
if ($LASTEXITCODE -ne 0) { throw 'PS_BloomBlur compile failed.' }
& $fxc /nologo /T ps_5_0 /E PS_Composite /Fo "$env:TEMP/PostFxComposite.cso" Shaders/PostFx/PostFx.hlsl
if ($LASTEXITCODE -ne 0) { throw 'PS_Composite compile failed.' }
```

빌드/자동 검증:

```powershell
git diff --check
MSBuild Engine/Include/Engine.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
.\UpdateLib.bat
MSBuild Client/Include/Client.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m:1 /nr:false /v:minimal
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Run-S17RhiValidation.ps1 -ReportPath .md/build/2026-07-14_POSTFX_RHI_VALIDATION.md
```

게이트 순서:
- G0: PostFx 기본 OFF로 normal F5와 `--rhi-scene-only` 프로세스가 기존처럼 살아 있는지 확인한다. `Run-S17RhiValidation.ps1`은 runtime 생존성 게이트이며 픽셀 동일성을 증명한다고 기록하지 않는다.
- G1: F1에서 `Passthrough`를 누른 상태로 동일 seed·동일 카메라·동일 tick의 OFF/ON 캡처를 비교한다. `Texture2D.Load(SV_Position.xy)` 경로에서 RGB와 alpha가 동일하고 D3D11 debug layer에 RTV/SRV hazard가 없어야 다음 게이트로 간다.
- G2: grade strength만 켜 gamma, saturation, tint, vignette를 한 항목씩 A/B한다. 현재 Mesh/Skinned가 이미 shader 내부 gamma를 적용하므로 `fGamma=1.0`을 기준값으로 유지하고 2.2를 기본값으로 소성하지 않는다.
- G3: bloom만 켜 threshold/soft-knee/intensity를 조절한다. 맵의 흰색 albedo, FOW 경계, debug line이 FX보다 먼저 과발광하면 실패다.

수동 확인:
- PostFx 실행 위치가 FxMesh/FxBeam/FxSprite 뒤이면서 `UI_Render_Overlay`, minimap, Viego/death screen overlay, Scene ImGui, cursor보다 앞인지 캡처로 확인한다. HUD·미니맵·F1·커서는 색보정/비네트/블룸 영향을 받지 않아야 한다.
- normal F5 roster, map, champion, minion, structure, snapshot, UI, FOW, FX를 숨기지 않은 상태에서 Annie Q, Yasuo Q3, Irelia R처럼 밝은 FX와 밝은 맵 지형을 함께 비교한다.
- `--rhi-scene-only`는 같은 DX11 백버퍼이므로 PostFx ON에서도 실행되어야 한다. DX12에서는 `CPostFxPass::Create`가 null이고 F1에 unavailable로 표시되어야 한다.
- 현재 swapchain은 sample count 1이다. 미래 MSAA 백버퍼에서는 silent copy하지 않고 `ResolveSubresource` 경로가 추가될 때까지 pass가 안전하게 skip되는지 확인한다.

성능 게이트:
- 1280x720에서 full-resolution R8 scene copy 1장과 quarter-resolution R16G16B16A16 ping/pong 2장의 추가 VRAM은 약 4.4 MiB를 기준으로 확인한다.
- normal F5 profiler JSON에서 `Render::PostFx` CPU submission p95는 0.25ms 이하, `GPU::FrameUs`의 OFF 대비 증가 p95는 1.0ms 이하를 1차 ceiling으로 둔다.
- `PostFx::BloomEnabled` counter와 동일 구간 캡처를 함께 남긴다. instrumentation만 추가한 것을 품질 개선이나 최적화 완료로 계산하지 않는다.

HDR 후속 승인 게이트:
- 이 계획의 bloom은 `R8G8B8A8_UNORM` 백버퍼에서 이미 clamp된 near-white 픽셀을 번지는 LDR 아트 필터다. FX emissive의 1.0 초과 에너지를 보존하는 HDR bloom이라고 부르지 않는다.
- LDR A/B가 작품 캡처에서 실제 퀄리티 상승으로 환전된 뒤에만 `R16G16B16A16_FLOAT SceneColor -> quarter bloom -> tone-map/gamma 1회 -> UNORM backbuffer` 컷오버 계획을 작성한다. 그때 `Mesh3D.hlsl`, `Skinned3D.hlsl`, PBR/Fog/FX 출력의 색공간과 인라인 gamma 제거를 같은 세션에서 다룬다.

천장 예산 30%:
- 구현·검증 시간의 최소 30%를 Annie Q, Yasuo Q3, Irelia R의 동일 카메라 before/after 영상과 profiler JSON으로 남긴다. passthrough 인프라 완성만으로 세션을 닫지 않고, 최소 한 스킬의 공개 가능한 품질 비교 컷으로 환전한다.
