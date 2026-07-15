#include "WintersPCH.h"

#include "Renderer/RHIFxSpriteRenderer.h"
#include "Renderer/FxShaderConstants.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <array>
#include <cstring>

namespace
{
    struct FxSpriteVertex
    {
        f32_t px, py, pz;
        f32_t nx, ny, nz;
        f32_t u, v;
        f32_t tx, ty, tz;
    };

    struct FxSpriteCBPerFrame
    {
        DirectX::XMFLOAT4X4 viewProjection;
    };

    struct FxSpriteCBPerObject
    {
        DirectX::XMFLOAT4X4 world;
    };

    constexpr const char* kFxSpriteShader = R"(
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

cbuffer CBFxParams : register(b2)
{
    float4 g_vTint;
    float4 g_vUVRect;
    float2 g_vUVScroll;
    float g_fAlphaClip;
    float g_fErodeThreshold;

    float4 g_vStyleColorA;
    float4 g_vStyleColorB;
    float4 g_vRimColor;
    float4 g_vStyleParams;
    float4 g_vTimeParams;
    float4 g_vMagicScrollA;
    float4 g_vMagicShape;
    float4 g_vMagicCore;
};

Texture2D g_DiffuseMap : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent : TANGENT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
    float2 vLocalUV : TEXCOORD1;
};

PS_INPUT VSMain(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(float4(input.vPosition, 1.0f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);

    float2 localUV = input.vTexCoord;
    float2 uv = lerp(g_vUVRect.xy, g_vUVRect.zw, localUV);
    uv += g_vUVScroll;

    output.vTexCoord = uv;
    output.vLocalUV = localUV;
    return output;
}

float ComputeBrush(float4 texColor)
{
    return saturate(max(max(texColor.r, texColor.g), texColor.b));
}

float ComputeSpriteRim(float2 localUV)
{
    float edge = min(min(localUV.x, 1.0f - localUV.x), min(localUV.y, 1.0f - localUV.y));
    return pow(saturate(1.0f - edge * 2.0f), max(g_vStyleParams.y, 0.001f));
}

float3 ApplyFxStyle(float4 texColor, float2 localUV)
{
    const float styleMode = g_vStyleParams.x;
    float3 baseColor = texColor.rgb * g_vTint.rgb;

    if (styleMode < 0.5f)
        return baseColor;

    const float rim = ComputeSpriteRim(localUV);
    const float brushContrast = max(g_vStyleColorB.a, 0.001f);
    const float brush = pow(ComputeBrush(texColor), brushContrast);
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;

    if (styleMode < 1.5f)
    {
        const float emissionIntensity = max(g_vStyleColorA.a, 0.0f);
        const float3 mainColor = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, brush);
        return mainColor * g_vTint.rgb * emissionIntensity + rimRGB;
    }

    const float cellLow = g_vStyleParams.z;
    const float cellHigh = max(g_vStyleParams.w, cellLow + 0.001f);
    const float luminance = dot(baseColor, float3(0.299f, 0.587f, 0.114f));
    const float cell = smoothstep(cellLow, cellHigh, luminance);
    const float3 stylized = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, cell);
    return stylized * g_vTint.rgb + rimRGB;
}

float ComputeRadialWipeMask(float2 localUV)
{
    const float2 fromCenter = localUV - 0.5f;
    const float phase = frac(
        atan2(-fromCenter.y, fromCenter.x) * 0.1591549431f + 1.0f);
    const float age = saturate(g_vTimeParams.y);
    const float feather = 0.008f;
    const float mask = smoothstep(age - feather, age + feather, phase);
    clip(mask - 0.001f);
    return mask;
}

float4 ApplyMagicSurface(float2 uv, float2 localUV)
{
    const float elapsed = g_vTimeParams.x;
    const float age = saturate(g_vTimeParams.y);
    const float random = g_vTimeParams.z;

    float2 uvA = uv + g_vMagicScrollA.xy * elapsed + random * 0.11f;
    float2 uvB = uv * 1.7f + g_vMagicScrollA.zw * elapsed + random * 0.37f;

    float2 distortVec = float2(
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 1.3f).r * 2.0f - 1.0f,
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 5.7f).r * 2.0f - 1.0f
    );
    uvA += distortVec * g_vMagicShape.w;

    const float nLow = g_DiffuseMap.Sample(g_Sampler, uvA).r;
    const float nHigh = g_DiffuseMap.Sample(g_Sampler, uvA * 2.7f + 0.5f).r;
    const float n = pow(saturate(nLow * 0.7f + nHigh * 0.3f), max(g_vMagicShape.x, 0.001f));

    const float2 fromCenter = localUV - 0.5f;
    const float centerMask = pow(saturate(1.0f - length(fromCenter) * 2.0f), max(g_vMagicCore.x, 0.001f));

    const float edgeWidth = max(g_vMagicShape.y, 0.001f);
    const float dissolved = n - age * g_vMagicShape.z;
    clip(dissolved + edgeWidth);

    const float edgeMask = 1.0f - smoothstep(0.0f, edgeWidth, dissolved);
    const float coreMask = saturate(dissolved / edgeWidth);
    const float rim = ComputeSpriteRim(localUV);

    const float3 coreRGB = g_vStyleColorA.rgb * coreMask * g_vMagicCore.y;
    const float3 edgeRGB = g_vStyleColorB.rgb * edgeMask * g_vMagicCore.z;
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;
    const float3 hue = lerp(float3(0.9f, 0.9f, 1.1f),
        float3(1.1f, 1.0f, 0.9f), saturate(random));

    float alpha = saturate(coreMask + edgeMask) * centerMask * g_vTint.a;
    if (g_fAlphaClip > 0.0f)
        clip(alpha - g_fAlphaClip);

    const float emissionIntensity = max(g_vStyleColorA.a, 0.0f);
    float3 finalRGB = (coreRGB + edgeRGB + rimRGB) * hue * g_vTint.rgb * emissionIntensity;
    finalRGB *= alpha;
    return float4(finalRGB, alpha);
}

float4 PSMain(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    texColor.a *= g_vTint.a;

    if (g_fErodeThreshold > 0.0f)
    {
        float erodeMask = saturate(dot(texColor.rgb, float3(0.299f, 0.587f, 0.114f)));
        clip(erodeMask - g_fErodeThreshold);
    }

    if (g_fAlphaClip > 0.0f)
        clip(texColor.a - g_fAlphaClip);

    const float styleMode = g_vStyleParams.x;
    if (styleMode >= 3.5f && styleMode < 4.5f)
        return ApplyMagicSurface(input.vTexCoord, input.vLocalUV);
    if (styleMode >= 4.5f && styleMode < 5.5f)
    {
        const float radialMask = ComputeRadialWipeMask(input.vLocalUV);
        texColor.rgb *= g_vTint.rgb * radialMask;
        texColor.a *= radialMask;
        return texColor;
    }

    texColor.rgb = ApplyFxStyle(texColor, input.vLocalUV);
    return texColor;
}
)";

    bool_t CompileShader(const char* pSource, const char* pEntry, const char* pTarget, ID3DBlob** ppBlob)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> pError;
        const HRESULT hr = D3DCompile(
            pSource,
            std::strlen(pSource),
            nullptr,
            nullptr,
            nullptr,
            pEntry,
            pTarget,
            D3DCOMPILE_ENABLE_STRICTNESS,
            0,
            ppBlob,
            &pError);

        if (FAILED(hr))
        {
            if (pError)
                OutputDebugStringA(static_cast<const char*>(pError->GetBufferPointer()));
            return false;
        }

        return true;
    }

    eRHIBlendMode ToRHIBlend(eBlendPreset blend)
    {
        switch (blend)
        {
        case eBlendPreset::Opaque: return eRHIBlendMode::Opaque;
        case eBlendPreset::PremultipliedAlpha: return eRHIBlendMode::Premultiplied;
        case eBlendPreset::Additive: return eRHIBlendMode::Additive;
        case eBlendPreset::AlphaBlend:
        default:
            return eRHIBlendMode::AlphaBlend;
        }
    }
}

struct CRHIFxSpriteRenderer::Impl
{
    IRHIDevice* pDevice = nullptr;
    RHIShaderHandle hVS{};
    RHIShaderHandle hPS{};
    RHIBindGroupLayoutHandle hLayout{};
    RHIBindGroupHandle hBindGroup{};
    RHIBufferHandle hVertexBuffer{};
    RHIBufferHandle hCBPerFrame{};
    RHIBufferHandle hCBPerObject{};
    RHIBufferHandle hCBFxParams{};
    std::array<RHIPipelineHandle, static_cast<size_t>(eBlendPreset::Count)> hPipelines{};
};

CRHIFxSpriteRenderer::CRHIFxSpriteRenderer()
    : m_pImpl(std::make_unique<Impl>())
{
}

CRHIFxSpriteRenderer::~CRHIFxSpriteRenderer()
{
    Shutdown();
}

std::unique_ptr<CRHIFxSpriteRenderer> CRHIFxSpriteRenderer::Create(IRHIDevice* pDevice)
{
    auto pRenderer = std::unique_ptr<CRHIFxSpriteRenderer>(new CRHIFxSpriteRenderer());
    if (!pRenderer->Initialize(pDevice))
        return nullptr;

    return pRenderer;
}

bool_t CRHIFxSpriteRenderer::Initialize(IRHIDevice* pDevice)
{
    if (!pDevice || pDevice->GetBackend() != eRHIBackend::DX12)
        return false;

    m_pImpl->pDevice = pDevice;

    Microsoft::WRL::ComPtr<ID3DBlob> pVS;
    Microsoft::WRL::ComPtr<ID3DBlob> pPS;
    if (!CompileShader(kFxSpriteShader, "VSMain", "vs_5_0", &pVS))
        return false;
    if (!CompileShader(kFxSpriteShader, "PSMain", "ps_5_0", &pPS))
        return false;

    m_pImpl->hVS = pDevice->CreateShader(
        eRHIShaderStage::Vertex,
        pVS->GetBufferPointer(),
        static_cast<u32_t>(pVS->GetBufferSize()),
        "RHIFxSpriteVS");
    m_pImpl->hPS = pDevice->CreateShader(
        eRHIShaderStage::Pixel,
        pPS->GetBufferPointer(),
        static_cast<u32_t>(pPS->GetBufferSize()),
        "RHIFxSpritePS");
    if (!m_pImpl->hVS.IsValid() || !m_pImpl->hPS.IsValid())
        return false;

    RHIBindingSlot slots[] = {
        { 0, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::Vertex },
        { 1, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::Vertex },
        { 2, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::All },
        { 0, eRHIBindingType::ShaderResource, eRHIShaderVisibility::Pixel },
    };
    RHIBindGroupLayoutDesc layoutDesc{};
    layoutDesc.slots = slots;
    layoutDesc.slotCount = 4;
    layoutDesc.debugName = "RHIFxSpriteLayout";
    m_pImpl->hLayout = pDevice->CreateBindGroupLayout(layoutDesc);
    if (!m_pImpl->hLayout.IsValid())
        return false;

    RHIBindGroupDesc bindDesc{};
    bindDesc.layoutHandle = m_pImpl->hLayout;
    bindDesc.debugName = "RHIFxSpriteBindGroup";
    m_pImpl->hBindGroup = pDevice->CreateBindGroup(bindDesc);
    if (!m_pImpl->hBindGroup.IsValid())
        return false;

    static constexpr RHIInputElementDesc kInputElements[] = {
        { "POSITION", 0, eRHIFormat::R32G32B32_Float, 0, 0 },
        { "NORMAL", 0, eRHIFormat::R32G32B32_Float, 12, 0 },
        { "TEXCOORD", 0, eRHIFormat::R32G32_Float, 24, 0 },
        { "TANGENT", 0, eRHIFormat::R32G32B32_Float, 32, 0 },
    };

    for (u32_t i = 0; i < static_cast<u32_t>(eBlendPreset::Count); ++i)
    {
        RHIPipelineDesc pipelineDesc{};
        pipelineDesc.vsHandle = m_pImpl->hVS;
        pipelineDesc.psHandle = m_pImpl->hPS;
        pipelineDesc.bindGroupLayouts[0] = m_pImpl->hLayout;
        pipelineDesc.bindGroupLayoutCount = 1;
        pipelineDesc.inputElements = kInputElements;
        pipelineDesc.inputElementCount = 4;
        pipelineDesc.topology = eRHIPrimitiveTopology::TriangleList;
        pipelineDesc.blendMode = ToRHIBlend(static_cast<eBlendPreset>(i));
        pipelineDesc.cullMode = eRHICullMode::None;
        pipelineDesc.depthOp = eRHIDepthOp::Always;
        pipelineDesc.depthWrite = false;
        pipelineDesc.rtvFormats[0] = eRHIFormat::R8G8B8A8_UNorm;
        pipelineDesc.rtvCount = 1;
        pipelineDesc.dsvFormat = eRHIFormat::Unknown;
        pipelineDesc.debugName = "RHIFxSpritePipeline";
        m_pImpl->hPipelines[i] = pDevice->CreatePipeline(pipelineDesc);
        if (!m_pImpl->hPipelines[i].IsValid())
            return false;
    }

    static constexpr FxSpriteVertex kVertices[] = {
        { -0.5f, 0.f, -0.5f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f },
        {  0.5f, 0.f, -0.5f, 0.f, 1.f, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f },
        {  0.5f, 0.f,  0.5f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 0.f },
        { -0.5f, 0.f, -0.5f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f },
        {  0.5f, 0.f,  0.5f, 0.f, 1.f, 0.f, 1.f, 0.f, 1.f, 0.f, 0.f },
        { -0.5f, 0.f,  0.5f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f },
    };

    RHIBufferDesc vbDesc{};
    vbDesc.sizeBytes = sizeof(kVertices);
    vbDesc.usage = eRHIBufferUsage::Vertex;
    vbDesc.memoryUsage = eRHIMemoryUsage::Dynamic;
    vbDesc.dynamic = true;
    vbDesc.debugName = "RHIFxSpriteVB";
    m_pImpl->hVertexBuffer = pDevice->CreateBuffer(vbDesc, kVertices);
    if (!m_pImpl->hVertexBuffer.IsValid())
        return false;

    auto createConstantBuffer = [pDevice](u32_t sizeBytes, const char* debugName)
    {
        RHIBufferDesc desc{};
        desc.sizeBytes = sizeBytes;
        desc.usage = eRHIBufferUsage::Constant;
        desc.memoryUsage = eRHIMemoryUsage::Dynamic;
        desc.dynamic = true;
        desc.debugName = debugName;
        return pDevice->CreateBuffer(desc, nullptr);
    };

    m_pImpl->hCBPerFrame = createConstantBuffer(sizeof(FxSpriteCBPerFrame), "RHIFxSpriteCBPerFrame");
    m_pImpl->hCBPerObject = createConstantBuffer(sizeof(FxSpriteCBPerObject), "RHIFxSpriteCBPerObject");
    m_pImpl->hCBFxParams = createConstantBuffer(sizeof(CBFxParams), "RHIFxSpriteCBFxParams");

    return m_pImpl->hCBPerFrame.IsValid() &&
        m_pImpl->hCBPerObject.IsValid() &&
        m_pImpl->hCBFxParams.IsValid();
}

void CRHIFxSpriteRenderer::Draw(IRHIDevice* pDevice,
    RHITextureHandle hTexture,
    const Mat4& matWorld,
    const Mat4& matViewProjection,
    const Vec4& vTint,
    const Vec4& vUVRect,
    const Vec2& vUVScroll,
    f32_t fAlphaClip,
    f32_t fErodeThreshold,
    eBlendPreset eBlend)
{
    CBFxParams fxParams = MakeFxParamsFromMaterial(
        FxMaterialDesc{},
        vTint,
        vUVRect,
        vUVScroll,
        0.f,
        0.f);
    fxParams.fAlphaClip = fAlphaClip;
    fxParams.fErodeThreshold = fErodeThreshold;

    Draw(pDevice, hTexture, matWorld, matViewProjection, fxParams, eBlend);
}

void CRHIFxSpriteRenderer::Draw(IRHIDevice* pDevice,
    RHITextureHandle hTexture,
    const Mat4& matWorld,
    const Mat4& matViewProjection,
    const CBFxParams& fxParams,
    eBlendPreset eBlend)
{
    if (!m_pImpl || !pDevice || !hTexture.IsValid())
        return;

    IRHICommandList* pCommandList = pDevice->GetFrameCommandList();
    if (!pCommandList)
        return;

    FxSpriteCBPerFrame perFrame{};
    perFrame.viewProjection = matViewProjection.m;
    FxSpriteCBPerObject perObject{};
    perObject.world = matWorld.m;

    pCommandList->UpdateBuffer(m_pImpl->hCBPerFrame, &perFrame, sizeof(perFrame));
    pCommandList->UpdateBuffer(m_pImpl->hCBPerObject, &perObject, sizeof(perObject));
    pCommandList->UpdateBuffer(m_pImpl->hCBFxParams, &fxParams, sizeof(fxParams));

    RHIBindGroupResource resources[] = {
        { 0, eRHIBindingType::ConstantBuffer, m_pImpl->hCBPerFrame, {}, {} },
        { 1, eRHIBindingType::ConstantBuffer, m_pImpl->hCBPerObject, {}, {} },
        { 2, eRHIBindingType::ConstantBuffer, m_pImpl->hCBFxParams, {}, {} },
        { 0, eRHIBindingType::ShaderResource, {}, hTexture, {} },
    };
    pDevice->UpdateBindGroup(m_pImpl->hBindGroup, resources, 4);

    const u32_t blendIndex = static_cast<u32_t>(eBlend);
    const u32_t fallbackIndex = static_cast<u32_t>(eBlendPreset::AlphaBlend);
    RHIPipelineHandle hPipeline =
        blendIndex < static_cast<u32_t>(eBlendPreset::Count)
            ? m_pImpl->hPipelines[blendIndex]
            : m_pImpl->hPipelines[fallbackIndex];

    pCommandList->SetPipeline(hPipeline);
    pCommandList->SetBindGroup(0, m_pImpl->hBindGroup);
    pCommandList->SetVertexBuffer(0, m_pImpl->hVertexBuffer, sizeof(FxSpriteVertex), 0);
    pCommandList->Draw(6, 1, 0, 0);
}

void CRHIFxSpriteRenderer::Shutdown()
{
    if (!m_pImpl || !m_pImpl->pDevice)
        return;

    IRHIDevice* pDevice = m_pImpl->pDevice;
    for (RHIPipelineHandle& hPipeline : m_pImpl->hPipelines)
    {
        if (hPipeline.IsValid())
            pDevice->DestroyPipeline(hPipeline);
        hPipeline = {};
    }

    if (m_pImpl->hBindGroup.IsValid())
        pDevice->DestroyBindGroup(m_pImpl->hBindGroup);
    if (m_pImpl->hLayout.IsValid())
        pDevice->DestroyBindGroupLayout(m_pImpl->hLayout);
    if (m_pImpl->hVertexBuffer.IsValid())
        pDevice->DestroyBuffer(m_pImpl->hVertexBuffer);
    if (m_pImpl->hCBPerFrame.IsValid())
        pDevice->DestroyBuffer(m_pImpl->hCBPerFrame);
    if (m_pImpl->hCBPerObject.IsValid())
        pDevice->DestroyBuffer(m_pImpl->hCBPerObject);
    if (m_pImpl->hCBFxParams.IsValid())
        pDevice->DestroyBuffer(m_pImpl->hCBFxParams);
    if (m_pImpl->hPS.IsValid())
        pDevice->DestroyShader(m_pImpl->hPS);
    if (m_pImpl->hVS.IsValid())
        pDevice->DestroyShader(m_pImpl->hVS);

    m_pImpl->hBindGroup = {};
    m_pImpl->hLayout = {};
    m_pImpl->hVertexBuffer = {};
    m_pImpl->hCBPerFrame = {};
    m_pImpl->hCBPerObject = {};
    m_pImpl->hCBFxParams = {};
    m_pImpl->hPS = {};
    m_pImpl->hVS = {};
    m_pImpl->pDevice = nullptr;
}
