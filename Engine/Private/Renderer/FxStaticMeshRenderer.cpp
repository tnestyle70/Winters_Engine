#include "Renderer/FxStaticMeshRenderer.h"

#include "RHI/RHITypes.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "RHI/DX11/BlendStateCache.h"
#include "RHI/IRHICommandList.h"
#include "Renderer/BlendTypes.h"
#include "Renderer/RHIFxMeshResource.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"

#include <d3dcompiler.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <array>
#include <cstring>
#include <unordered_map>
#include <wrl/client.h>

using namespace DirectX;

NS_BEGIN(Engine)

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

    bool_t CompileShader(const char* pSource, const char* pEntry, const char* pTarget, ID3DBlob** ppBlob)
    {
        Microsoft::WRL::ComPtr<ID3DBlob> pError;
        const HRESULT hr = D3DCompile(
            pSource,
            strlen(pSource),
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

    constexpr const char* kRHIFxMeshShader = R"(
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3 g_vCameraWorld;
    float g_fFxTime;
    float3 g_vLightDirWorld;
    float g_fLightIntensity;
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
Texture2D g_ErodeMap : register(t1);
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
    float3 vWorldPos : TEXCOORD1;
    float3 vWorldNormal : TEXCOORD2;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(float4(input.vPosition, 1.0f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vWorldPos = worldPos.xyz;
    output.vWorldNormal = normalize(mul(float4(input.vNormal, 0.0f), g_matWorld).xyz);
    output.vTexCoord = lerp(g_vUVRect.xy, g_vUVRect.zw, input.vTexCoord) + g_vUVScroll;
    return output;
}

float ComputeBrush(float4 texColor)
{
    return saturate(max(max(texColor.r, texColor.g), texColor.b));
}

float ComputeRim(float3 normal, float3 worldPos)
{
    float3 viewDir = normalize(g_vCameraWorld - worldPos);
    float rimBase = 1.0f - saturate(dot(normal, viewDir));
    return pow(rimBase, max(g_vStyleParams.y, 0.001f));
}

float3 ApplyFxStyle(float4 texColor, float3 normal, float3 worldPos)
{
    const float styleMode = g_vStyleParams.x;
    if (styleMode < 0.5f)
        return texColor.rgb * g_vTint.rgb;

    const float brush = pow(ComputeBrush(texColor), max(g_vStyleColorB.a, 0.001f));
    const float emission = max(g_vStyleColorA.a, 0.0f);
    const float rim = ComputeRim(normal, worldPos);
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;

    if (styleMode < 1.5f)
    {
        const float3 mainColor = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, brush);
        return mainColor * g_vTint.rgb * emission + rimRGB;
    }

    if (styleMode < 2.5f)
    {
        const float cellLow = g_vStyleParams.z;
        const float cellHigh = max(g_vStyleParams.w, cellLow + 0.001f);
        const float3 lightDir = normalize(-g_vLightDirWorld);
        const float nDotL = dot(normal, lightDir);
        const float cellLit = (nDotL > cellHigh) ? 1.0f : ((nDotL > cellLow) ? 0.55f : 0.18f);
        return texColor.rgb * g_vTint.rgb * cellLit * emission + rimRGB;
    }

    const float gradY = normal.y * 0.5f + 0.5f;
    const float3 gradRGB = lerp(g_vStyleColorB.rgb, g_vStyleColorA.rgb, saturate(gradY));
    return gradRGB * texColor.rgb * g_vTint.rgb * emission + rimRGB;
}

float4 ApplyMagicSurface(PS_INPUT input)
{
    const float elapsed = g_vTimeParams.x;
    const float age = saturate(g_vTimeParams.y);
    const float random = g_vTimeParams.z;

    float2 uvA = input.vTexCoord + g_vMagicScrollA.xy * elapsed + random * 0.11f;
    float2 uvB = input.vTexCoord * 1.7f + g_vMagicScrollA.zw * elapsed + random * 0.37f;
    float2 distortVec = float2(
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 1.3f).r * 2.0f - 1.0f,
        g_DiffuseMap.Sample(g_Sampler, uvB * 0.7f + 5.7f).r * 2.0f - 1.0f);
    uvA += distortVec * g_vMagicShape.w;

    const float nLow = g_DiffuseMap.Sample(g_Sampler, uvA).r;
    const float nHigh = g_DiffuseMap.Sample(g_Sampler, uvA * 2.7f + 0.5f).r;
    const float n = pow(saturate(nLow * 0.7f + nHigh * 0.3f), max(g_vMagicShape.x, 0.001f));
    const float2 fromCenter = input.vTexCoord - 0.5f;
    const float centerMask = pow(saturate(1.0f - length(fromCenter) * 2.0f), max(g_vMagicCore.x, 0.001f));
    const float edgeWidth = max(g_vMagicShape.y, 0.001f);
    const float dissolved = n - age * g_vMagicShape.z;
    clip(dissolved + edgeWidth);

    const float edgeMask = 1.0f - smoothstep(0.0f, edgeWidth, dissolved);
    const float coreMask = saturate(dissolved / edgeWidth);
    const float rim = ComputeRim(normalize(input.vWorldNormal), input.vWorldPos);
    const float3 coreRGB = g_vStyleColorA.rgb * coreMask * g_vMagicCore.y;
    const float3 edgeRGB = g_vStyleColorB.rgb * edgeMask * g_vMagicCore.z;
    const float3 rimRGB = g_vRimColor.rgb * rim * g_vRimColor.a;
    const float3 hue = lerp(float3(0.9f, 0.9f, 1.1f), float3(1.1f, 1.0f, 0.9f), saturate(random));
    const float alpha = saturate(coreMask + edgeMask) * centerMask * g_vTint.a;
    if (g_fAlphaClip > 0.0f)
        clip(alpha - g_fAlphaClip);
    return float4((coreRGB + edgeRGB + rimRGB) * hue * g_vTint.rgb * max(g_vStyleColorA.a, 0.0f) * alpha, alpha);
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = g_DiffuseMap.Sample(g_Sampler, input.vTexCoord);
    texColor.a *= g_vTint.a;
    const float styleMode = g_vStyleParams.x;

    if (g_fErodeThreshold > 0.0f)
    {
        float erodeNoise = g_ErodeMap.Sample(g_Sampler, input.vTexCoord).r;
        if (erodeNoise < g_fErodeThreshold)
            discard;
    }

    if (styleMode >= 3.5f && styleMode < 4.5f)
        return ApplyMagicSurface(input);

    if (g_fAlphaClip > 0.0f)
        clip(texColor.a - g_fAlphaClip);

    float3 normal = normalize(input.vWorldNormal);
    if (styleMode >= 0.5f)
    {
        const float brush = ComputeBrush(texColor);
        const float rim = ComputeRim(normal, input.vWorldPos);
        texColor.a *= saturate(brush + rim * 0.35f);
    }

    texColor.rgb = ApplyFxStyle(texColor, normal, input.vWorldPos);
    if (styleMode >= 0.5f)
        texColor.rgb *= texColor.a;
    return texColor;
}
)";
}

struct CFxStaticMeshRenderer::Impl
{
    IRHIDevice* pDevice = nullptr;
    bool_t bRHIBackend = false;
    DX11Shader* pMeshShader = nullptr;
    DX11Pipeline* pMeshPipeline = nullptr;
    DX11Shader* pFxMeshShader = nullptr;
    DX11Pipeline* pFxMeshPipeline = nullptr;
    CBlendStateCache* pBlendCache = nullptr;

    DX11ConstantBuffer<CBPerFrame> cbPerFrame;
    DX11ConstantBuffer<CBPerObject> cbPerObject;
    DX11ConstantBuffer<CBFxParams> cbFxParams;

    std::unordered_map<std::string, std::shared_ptr<CModel>> mapModels;
    std::unordered_map<std::wstring, std::unique_ptr<CTexture>> mapTextures;
    std::unique_ptr<CRHIFxMeshResourceCache> pRHIMeshCache;

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDSSNoWrite = { nullptr };
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pDSSNoDepth = { nullptr };

    RHIShaderHandle hRHIVS{};
    RHIShaderHandle hRHIPS{};
    RHIBindGroupLayoutHandle hRHILayout{};
    RHIBindGroupHandle hRHIBindGroup{};
    RHIBufferHandle hRHICBPerFrame{};
    RHIBufferHandle hRHICBPerObject{};
    RHIBufferHandle hRHICBFxParams{};
    std::array<RHIPipelineHandle, static_cast<size_t>(eBlendPreset::Count)> hRHIPipelines{};
};

namespace
{
    bool_t InitializeRHIFxMesh(CFxStaticMeshRenderer::Impl* pImpl)
    {
        if (!pImpl || !pImpl->pDevice || pImpl->pDevice->GetBackend() != eRHIBackend::DX12)
            return false;

        Microsoft::WRL::ComPtr<ID3DBlob> pVS;
        Microsoft::WRL::ComPtr<ID3DBlob> pPS;
        if (!CompileShader(kRHIFxMeshShader, "VS", "vs_5_0", &pVS))
            return false;
        if (!CompileShader(kRHIFxMeshShader, "PS", "ps_5_0", &pPS))
            return false;

        pImpl->hRHIVS = pImpl->pDevice->CreateShader(
            eRHIShaderStage::Vertex,
            pVS->GetBufferPointer(),
            static_cast<u32_t>(pVS->GetBufferSize()),
            "RHIFxMeshVS");
        pImpl->hRHIPS = pImpl->pDevice->CreateShader(
            eRHIShaderStage::Pixel,
            pPS->GetBufferPointer(),
            static_cast<u32_t>(pPS->GetBufferSize()),
            "RHIFxMeshPS");
        if (!pImpl->hRHIVS.IsValid() || !pImpl->hRHIPS.IsValid())
            return false;

        RHIBindingSlot slots[] = {
            { 0, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::Vertex },
            { 1, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::Vertex },
            { 2, eRHIBindingType::ConstantBuffer, eRHIShaderVisibility::All },
            { 0, eRHIBindingType::ShaderResource, eRHIShaderVisibility::Pixel },
            { 1, eRHIBindingType::ShaderResource, eRHIShaderVisibility::Pixel },
        };
        RHIBindGroupLayoutDesc layoutDesc{};
        layoutDesc.slots = slots;
        layoutDesc.slotCount = 5;
        layoutDesc.debugName = "RHIFxMeshLayout";
        pImpl->hRHILayout = pImpl->pDevice->CreateBindGroupLayout(layoutDesc);
        if (!pImpl->hRHILayout.IsValid())
            return false;

        RHIBindGroupDesc bindDesc{};
        bindDesc.layoutHandle = pImpl->hRHILayout;
        bindDesc.debugName = "RHIFxMeshBindGroup";
        pImpl->hRHIBindGroup = pImpl->pDevice->CreateBindGroup(bindDesc);
        if (!pImpl->hRHIBindGroup.IsValid())
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
            pipelineDesc.vsHandle = pImpl->hRHIVS;
            pipelineDesc.psHandle = pImpl->hRHIPS;
            pipelineDesc.bindGroupLayouts[0] = pImpl->hRHILayout;
            pipelineDesc.bindGroupLayoutCount = 1;
            pipelineDesc.inputElements = kInputElements;
            pipelineDesc.inputElementCount = static_cast<u32_t>(std::size(kInputElements));
            pipelineDesc.topology = eRHIPrimitiveTopology::TriangleList;
            pipelineDesc.blendMode = ToRHIBlend(static_cast<eBlendPreset>(i));
            pipelineDesc.cullMode = eRHICullMode::None;
            pipelineDesc.depthOp = eRHIDepthOp::Always;
            pipelineDesc.depthWrite = false;
            pipelineDesc.rtvFormats[0] = eRHIFormat::R8G8B8A8_UNorm;
            pipelineDesc.rtvCount = 1;
            pipelineDesc.dsvFormat = eRHIFormat::Unknown;
            pipelineDesc.debugName = "RHIFxMeshPipeline";
            pImpl->hRHIPipelines[i] = pImpl->pDevice->CreatePipeline(pipelineDesc);
            if (!pImpl->hRHIPipelines[i].IsValid())
                return false;
        }

        auto createConstantBuffer = [pImpl](u32_t sizeBytes, const char* pDebugName)
        {
            RHIBufferDesc desc{};
            desc.sizeBytes = sizeBytes;
            desc.usage = eRHIBufferUsage::Constant;
            desc.memoryUsage = eRHIMemoryUsage::Dynamic;
            desc.dynamic = true;
            desc.debugName = pDebugName;
            return pImpl->pDevice->CreateBuffer(desc, nullptr);
        };

        pImpl->hRHICBPerFrame = createConstantBuffer(sizeof(CBPerFrame), "RHIFxMeshCBPerFrame");
        pImpl->hRHICBPerObject = createConstantBuffer(sizeof(CBPerObject), "RHIFxMeshCBPerObject");
        pImpl->hRHICBFxParams = createConstantBuffer(sizeof(CBFxParams), "RHIFxMeshCBFxParams");
        pImpl->pRHIMeshCache = CRHIFxMeshResourceCache::Create(pImpl->pDevice);

        return pImpl->hRHICBPerFrame.IsValid() &&
            pImpl->hRHICBPerObject.IsValid() &&
            pImpl->hRHICBFxParams.IsValid() &&
            pImpl->pRHIMeshCache != nullptr;
    }

    void DrawRHIFxMesh(CFxStaticMeshRenderer::Impl* pImpl,
        const char* pFbxPath,
        const FxMeshDrawParams& params)
    {
        if (!pImpl || !pImpl->pDevice || !pImpl->pRHIMeshCache || !pFbxPath)
            return;

        const std::string strFbxPath(pFbxPath);
        const std::wstring strTexturePath = params.pTexturePath ? params.pTexturePath : L"";
        const std::wstring strErodeTexturePath = params.pErodeTexturePath ? params.pErodeTexturePath : L"";

        RHIFxMeshResource* pMesh = pImpl->pRHIMeshCache->Find(
            strFbxPath,
            strTexturePath,
            strErodeTexturePath);
        if (!pMesh)
            pMesh = pImpl->pRHIMeshCache->Find(strFbxPath);
        if (!pMesh)
            return;

        IRHICommandList* pCommandList = pImpl->pDevice->GetFrameCommandList();
        if (!pCommandList)
            return;

        CBPerObject objData{};
        objData.world = params.matWorld.m;

        CBFxParams fxData{};
        fxData.vTint = { params.vTint.x, params.vTint.y, params.vTint.z, params.vTint.w };
        fxData.vUVRect = { params.vUVRect.x, params.vUVRect.y, params.vUVRect.z, params.vUVRect.w };
        fxData.vUVScroll = { params.vUVScroll.x, params.vUVScroll.y };
        fxData.fAlphaClip = params.fAlphaClip;
        fxData.fErodeThreshold = params.fErodeThreshold;
        fxData.vStyleColorA = { params.vStyleColorA.x, params.vStyleColorA.y,
            params.vStyleColorA.z, params.vStyleColorA.w };
        fxData.vStyleColorB = { params.vStyleColorB.x, params.vStyleColorB.y,
            params.vStyleColorB.z, params.vStyleColorB.w };
        fxData.vRimColor = { params.vRimColor.x, params.vRimColor.y,
            params.vRimColor.z, params.vRimColor.w };
        fxData.vStyleParams = { params.vStyleParams.x, params.vStyleParams.y,
            params.vStyleParams.z, params.vStyleParams.w };
        fxData.vTimeParams = { params.vTimeParams.x, params.vTimeParams.y,
            params.vTimeParams.z, params.vTimeParams.w };
        fxData.vMagicScrollA = { params.vMagicScrollA.x, params.vMagicScrollA.y,
            params.vMagicScrollA.z, params.vMagicScrollA.w };
        fxData.vMagicShape = { params.vMagicShape.x, params.vMagicShape.y,
            params.vMagicShape.z, params.vMagicShape.w };
        fxData.vMagicCore = { params.vMagicCore.x, params.vMagicCore.y,
            params.vMagicCore.z, params.vMagicCore.w };

        pCommandList->UpdateBuffer(pImpl->hRHICBPerObject, &objData, sizeof(objData));
        pCommandList->UpdateBuffer(pImpl->hRHICBFxParams, &fxData, sizeof(fxData));

        const RHITextureHandle hDefaultTexture = pImpl->pRHIMeshCache->GetDefaultTexture();
        RHITextureHandle hDiffuseTexture = pMesh->hDiffuseTexture.IsValid()
            ? pMesh->hDiffuseTexture
            : hDefaultTexture;
        RHITextureHandle hErodeTexture = pMesh->hErodeTexture.IsValid()
            ? pMesh->hErodeTexture
            : hDefaultTexture;

        RHIBindGroupResource resources[] = {
            { 0, eRHIBindingType::ConstantBuffer, pImpl->hRHICBPerFrame, {}, {} },
            { 1, eRHIBindingType::ConstantBuffer, pImpl->hRHICBPerObject, {}, {} },
            { 2, eRHIBindingType::ConstantBuffer, pImpl->hRHICBFxParams, {}, {} },
            { 0, eRHIBindingType::ShaderResource, {}, hDiffuseTexture, {} },
            { 1, eRHIBindingType::ShaderResource, {}, hErodeTexture, {} },
        };
        pImpl->pDevice->UpdateBindGroup(pImpl->hRHIBindGroup, resources, 5);

        const u32_t blendIndex = params.iBlendPreset < static_cast<u32_t>(eBlendPreset::Count)
            ? params.iBlendPreset
            : static_cast<u32_t>(eBlendPreset::AlphaBlend);
        pCommandList->SetPipeline(pImpl->hRHIPipelines[blendIndex]);
        pCommandList->SetBindGroup(0, pImpl->hRHIBindGroup);

        for (const RHIFxMeshPart& part : pMesh->parts)
        {
            pCommandList->SetVertexBuffer(0, part.hVertexBuffer, part.vertexStride, 0);
            pCommandList->SetIndexBuffer(part.hIndexBuffer, 0, eRHIFormat::R32_UInt);
            pCommandList->DrawIndexed(part.indexCount, 1, 0, 0, 0);
        }
    }
}

CFxStaticMeshRenderer::~CFxStaticMeshRenderer()
{
    Shutdown();
    delete m_pImpl;
    m_pImpl = nullptr;
}

std::unique_ptr<CFxStaticMeshRenderer> CFxStaticMeshRenderer::Create(
    IRHIDevice* pDevice,
    DX11Shader* pMeshShader,
    DX11Pipeline* pMeshPipeline,
    DX11Shader* pFxMeshShader,
    DX11Pipeline* pFxMeshPipeline,
    CBlendStateCache* pBlendCache)
{
    if (!pDevice)
        return nullptr;

    auto pInstance = unique_ptr<CFxStaticMeshRenderer>(new CFxStaticMeshRenderer());
    if (!pInstance)
        return nullptr;

    pInstance->m_pImpl = new Impl();
    pInstance->m_pImpl->pDevice = pDevice;
    pInstance->m_pImpl->bRHIBackend = pDevice->GetBackend() == eRHIBackend::DX12;

    if (pInstance->m_pImpl->bRHIBackend)
    {
        if (!InitializeRHIFxMesh(pInstance->m_pImpl))
        {
            ::OutputDebugStringA("[CFxStaticMeshRenderer] DX12 RHI mesh renderer init failed.\n");
            return nullptr;
        }

        ::OutputDebugStringA("[CFxStaticMeshRenderer] DX12 RHI mesh renderer ready.\n");
        return pInstance;
    }

    if (!pMeshShader || !pMeshPipeline || !pFxMeshShader || !pFxMeshPipeline || !pBlendCache)
        return nullptr;

    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    if (!pNativeDevice)
        return nullptr;

    pInstance->m_pImpl->pMeshShader = pMeshShader;
    pInstance->m_pImpl->pMeshPipeline = pMeshPipeline;
    pInstance->m_pImpl->pFxMeshShader = pFxMeshShader;
    pInstance->m_pImpl->pFxMeshPipeline = pFxMeshPipeline;
    pInstance->m_pImpl->pBlendCache = pBlendCache;

    if (!pInstance->m_pImpl->cbPerFrame.Create(pNativeDevice))
        return nullptr;
    if (!pInstance->m_pImpl->cbPerObject.Create(pNativeDevice))
        return nullptr;
    if (!pInstance->m_pImpl->cbFxParams.Create(pNativeDevice))
        return nullptr;

    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dsd.StencilEnable = FALSE;
    if (FAILED(pNativeDevice->CreateDepthStencilState(&dsd,
        pInstance->m_pImpl->pDSSNoWrite.GetAddressOf())))
    {
        return nullptr;
    }

    D3D11_DEPTH_STENCIL_DESC noDepthDsd = {};
    noDepthDsd.DepthEnable = FALSE;
    noDepthDsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    noDepthDsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    noDepthDsd.StencilEnable = FALSE;
    if (FAILED(pNativeDevice->CreateDepthStencilState(&noDepthDsd,
        pInstance->m_pImpl->pDSSNoDepth.GetAddressOf())))
    {
        return nullptr;
    }

    return pInstance;
}

bool CFxStaticMeshRenderer::PreloadMesh(const std::string& strFbxPath, const std::wstring& strTexturePath)
{
    return PreloadMeshInternal(strFbxPath, strTexturePath, L"", false);
}

bool CFxStaticMeshRenderer::PreloadMesh(const std::string& strFbxPath,
    const std::wstring& strTexturePath, const std::wstring& strErodeTexturePath)
{
    return PreloadMeshInternal(
        strFbxPath,
        strTexturePath,
        strErodeTexturePath,
        false);
}

bool CFxStaticMeshRenderer::PreloadMeshStrict(
    const std::string& strFbxPath,
    const std::wstring& strTexturePath)
{
    return PreloadMeshInternal(strFbxPath, strTexturePath, L"", true);
}

bool CFxStaticMeshRenderer::PreloadMeshInternal(
    const std::string& strFbxPath,
    const std::wstring& strTexturePath,
    const std::wstring& strErodeTexturePath,
    bool_t bRequireDiffuseTexture)
{
    if (!m_pImpl)
        return false;

    if (m_pImpl->mapModels.find(strFbxPath) != m_pImpl->mapModels.end())
    {
        return !bRequireDiffuseTexture ||
            strTexturePath.empty() ||
            m_pImpl->mapTextures.find(strTexturePath) != m_pImpl->mapTextures.end();
    }

    if (m_pImpl->bRHIBackend)
    {
        if (!m_pImpl->pRHIMeshCache)
            return false;

        RHIFxMeshResource* pResource = m_pImpl->pRHIMeshCache->LoadOrGet(
            strFbxPath,
            strTexturePath,
            strErodeTexturePath);
        if (!pResource)
            return false;

        if (bRequireDiffuseTexture && !strTexturePath.empty())
        {
            const RHITextureHandle hDefault =
                m_pImpl->pRHIMeshCache->GetDefaultTexture();
            if (!pResource->hDiffuseTexture.IsValid() ||
                pResource->hDiffuseTexture.value == hDefault.value)
            {
                return false;
            }
        }
        return true;
    }
    std::unique_ptr<CModel> pOwned = CModel::Create(m_pImpl->pDevice, strFbxPath);
    if (!pOwned)
    {
        ::OutputDebugStringA(("[CFxStaticMeshRenderer] Model load fail: " + strFbxPath + "\n").c_str());
        return false;
    }

    if (pOwned->HasSkeleton())
    {
        ::OutputDebugStringA(("[CFxStaticMeshRenderer] Skinned FBX rejected: " + strFbxPath + "\n").c_str());
        return false;
    }

    if (!strTexturePath.empty())
    {
        auto itTex = m_pImpl->mapTextures.find(strTexturePath);
        CTexture* pRawTex = nullptr;
        if (itTex == m_pImpl->mapTextures.end())
        {
            auto pNewTex = CTexture::Create(m_pImpl->pDevice, strTexturePath, eTexSamplerMode::Clamp);
            if (!pNewTex)
            {
                ::OutputDebugStringW((L"[CFxStaticMeshRenderer] Texture load fail: " + strTexturePath + L"\n").c_str());
            }
            else
            {
                pRawTex = pNewTex.get();
                m_pImpl->mapTextures.emplace(strTexturePath, std::move(pNewTex));
            }
        }
        else
        {
            pRawTex = itTex->second.get();
        }

        if (!pRawTex && bRequireDiffuseTexture)
            return false;

        if (pRawTex)
        {
            const u32_t meshCount = pOwned->GetMeshCount();
            for (u32_t i = 0; i < meshCount; ++i)
                pOwned->SetMeshTexture(i, pRawTex);
        }
    }

    std::shared_ptr<CModel> pShared(pOwned.release());
    m_pImpl->mapModels.emplace(strFbxPath, pShared);
    return true;
}

void CFxStaticMeshRenderer::BeginFrame(const Mat4& matViewProj,
    const Vec3& vCameraWorld)
{
    if (!m_pImpl)
        return;

    if (m_pImpl->bRHIBackend)
    {
        CBPerFrame frameData{};
        frameData.viewProjection = matViewProj.m;
        frameData.cameraWorld = { vCameraWorld.x, vCameraWorld.y, vCameraWorld.z };
        frameData.lightDirWorld = { -0.35f, -0.65f, 0.68f };
        frameData.lightIntensity = 1.f;
        frameData.lightColor = { 1.f, 1.f, 1.f };
        if (IRHICommandList* pCommandList = m_pImpl->pDevice->GetFrameCommandList())
        {
            pCommandList->UpdateBuffer(
                m_pImpl->hRHICBPerFrame,
                &frameData,
                sizeof(frameData));
        }
        return;
    }

    if (!m_pImpl->pFxMeshShader || !m_pImpl->pFxMeshPipeline)
        return;

    ID3D11DeviceContext* pContext = GetNativeDX11Context(m_pImpl->pDevice);
    if (!pContext)
        return;

    m_pImpl->pFxMeshShader->Bind(pContext);
    m_pImpl->pFxMeshPipeline->Bind(pContext);

    CBPerFrame frameData = {};
    frameData.viewProjection = matViewProj.m;
    frameData.cameraWorld = { vCameraWorld.x, vCameraWorld.y, vCameraWorld.z };
    frameData.lightDirWorld = { -0.35f, -0.65f, 0.68f };
    frameData.lightIntensity = 1.f;
    frameData.lightColor = { 1.f, 1.f, 1.f };
    m_pImpl->cbPerFrame.Update(pContext, frameData);
    m_pImpl->cbPerFrame.BindVS(pContext, 0);
    m_pImpl->cbPerFrame.BindPS(pContext, 0);

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void CFxStaticMeshRenderer::DrawMesh(const char* pFbxPath, const FxMeshDrawParams& params)
{
    if (!m_pImpl || !pFbxPath)
        return;

    if (m_pImpl->bRHIBackend)
    {
        DrawRHIFxMesh(m_pImpl, pFbxPath, params);
        return;
    }

    auto it = m_pImpl->mapModels.find(std::string(pFbxPath));
    if (it == m_pImpl->mapModels.end())
        return;

    ID3D11DeviceContext* pContext = GetNativeDX11Context(m_pImpl->pDevice);
    if (!pContext)
        return;

    CBPerObject objData = {};
    objData.world = params.matWorld.m;
    m_pImpl->cbPerObject.Update(pContext, objData);
    m_pImpl->cbPerObject.BindVS(pContext, 1);

    CBFxParams fxData = {};
    fxData.vTint = { params.vTint.x, params.vTint.y, params.vTint.z, params.vTint.w };
    fxData.vUVRect = { params.vUVRect.x, params.vUVRect.y, params.vUVRect.z, params.vUVRect.w };
    fxData.vUVScroll = { params.vUVScroll.x, params.vUVScroll.y };
    fxData.fAlphaClip = params.fAlphaClip;
    fxData.fErodeThreshold = params.fErodeThreshold;
    fxData.vStyleColorA = { params.vStyleColorA.x, params.vStyleColorA.y,
        params.vStyleColorA.z, params.vStyleColorA.w };
    fxData.vStyleColorB = { params.vStyleColorB.x, params.vStyleColorB.y,
        params.vStyleColorB.z, params.vStyleColorB.w };
    fxData.vRimColor = { params.vRimColor.x, params.vRimColor.y,
        params.vRimColor.z, params.vRimColor.w };
    fxData.vStyleParams = { params.vStyleParams.x, params.vStyleParams.y,
        params.vStyleParams.z, params.vStyleParams.w };
    fxData.vTimeParams = { params.vTimeParams.x, params.vTimeParams.y,
        params.vTimeParams.z, params.vTimeParams.w };
    fxData.vMagicScrollA = { params.vMagicScrollA.x, params.vMagicScrollA.y,
        params.vMagicScrollA.z, params.vMagicScrollA.w };
    fxData.vMagicShape = { params.vMagicShape.x, params.vMagicShape.y,
        params.vMagicShape.z, params.vMagicShape.w };
    fxData.vMagicCore = { params.vMagicCore.x, params.vMagicCore.y,
        params.vMagicCore.z, params.vMagicCore.w };
    m_pImpl->cbFxParams.Update(pContext, fxData);
    m_pImpl->cbFxParams.BindVS(pContext, 2);
    m_pImpl->cbFxParams.BindPS(pContext, 2);

    Microsoft::WRL::ComPtr<ID3D11BlendState> pPrevBS;
    FLOAT prevBlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    UINT prevSampleMask = 0xFFFFFFFFu;
    if (m_pImpl->pBlendCache)
    {
        pContext->OMGetBlendState(pPrevBS.GetAddressOf(), prevBlendFactor, &prevSampleMask);

        const u32_t blendCount = static_cast<u32_t>(eBlendPreset::Count);
        const eBlendPreset blend = params.iBlendPreset < blendCount
            ? static_cast<eBlendPreset>(params.iBlendPreset)
            : eBlendPreset::AlphaBlend;
        m_pImpl->pBlendCache->Bind(pContext, blend);
    }

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> pPrevDSS;
    UINT prevStencil = 0;
    ID3D11DepthStencilState* pOverrideDSS = nullptr;

    if (params.depthMode == eFxDepthMode::OverlayNoDepth && m_pImpl->pDSSNoDepth)
    {
        pOverrideDSS = m_pImpl->pDSSNoDepth.Get();
    }
    else if (!params.bDepthWrite && m_pImpl->pDSSNoWrite)
    {
        pOverrideDSS = m_pImpl->pDSSNoWrite.Get();
    }

    if (pOverrideDSS)
    {
        pContext->OMGetDepthStencilState(pPrevDSS.GetAddressOf(), &prevStencil);
        pContext->OMSetDepthStencilState(pOverrideDSS, 0);
    }

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    it->second->Render(m_pImpl->pDevice);

    if (pOverrideDSS)
        pContext->OMSetDepthStencilState(pPrevDSS.Get(), prevStencil);

    if (m_pImpl->pBlendCache)
        pContext->OMSetBlendState(pPrevBS.Get(), prevBlendFactor, prevSampleMask);
}

void CFxStaticMeshRenderer::DrawMesh(const char* pFbxPath, const Mat4& matWorld)
{
    if (!m_pImpl || !pFbxPath)
        return;

    if (m_pImpl->bRHIBackend)
    {
        FxMeshDrawParams params{};
        params.matWorld = matWorld;
        DrawRHIFxMesh(m_pImpl, pFbxPath, params);
        return;
    }

    auto it = m_pImpl->mapModels.find(std::string(pFbxPath));
    if (it == m_pImpl->mapModels.end())
        return;

    ID3D11DeviceContext* pContext = GetNativeDX11Context(m_pImpl->pDevice);
    if (!pContext)
        return;

    CBPerObject objData = {};
    objData.world = matWorld.m;
    m_pImpl->cbPerObject.Update(pContext, objData);
    m_pImpl->cbPerObject.BindVS(pContext, 1);

    it->second->Render(m_pImpl->pDevice);
}

void CFxStaticMeshRenderer::EndFrame()
{
    if (!m_pImpl || !m_pImpl->pFxMeshShader)
        return;

    if (m_pImpl->bRHIBackend)
        return;

    ID3D11DeviceContext* pContext = GetNativeDX11Context(m_pImpl->pDevice);
    if (!pContext)
        return;

    m_pImpl->pFxMeshShader->Unbind(pContext);
}

void CFxStaticMeshRenderer::Shutdown()
{
    if (!m_pImpl)
        return;

    if (m_pImpl->pDevice && m_pImpl->bRHIBackend)
    {
        IRHIDevice* pDevice = m_pImpl->pDevice;

        if (m_pImpl->hRHIBindGroup.IsValid())
            pDevice->DestroyBindGroup(m_pImpl->hRHIBindGroup);
        m_pImpl->hRHIBindGroup = {};

        for (RHIPipelineHandle& hPipeline : m_pImpl->hRHIPipelines)
        {
            if (hPipeline.IsValid())
                pDevice->DestroyPipeline(hPipeline);
            hPipeline = {};
        }

        if (m_pImpl->hRHILayout.IsValid())
            pDevice->DestroyBindGroupLayout(m_pImpl->hRHILayout);
        m_pImpl->hRHILayout = {};

        if (m_pImpl->pRHIMeshCache)
        {
            m_pImpl->pRHIMeshCache->Shutdown();
            m_pImpl->pRHIMeshCache.reset();
        }

        if (m_pImpl->hRHICBFxParams.IsValid())
            pDevice->DestroyBuffer(m_pImpl->hRHICBFxParams);
        if (m_pImpl->hRHICBPerObject.IsValid())
            pDevice->DestroyBuffer(m_pImpl->hRHICBPerObject);
        if (m_pImpl->hRHICBPerFrame.IsValid())
            pDevice->DestroyBuffer(m_pImpl->hRHICBPerFrame);
        if (m_pImpl->hRHIPS.IsValid())
            pDevice->DestroyShader(m_pImpl->hRHIPS);
        if (m_pImpl->hRHIVS.IsValid())
            pDevice->DestroyShader(m_pImpl->hRHIVS);

        m_pImpl->hRHICBFxParams = {};
        m_pImpl->hRHICBPerObject = {};
        m_pImpl->hRHICBPerFrame = {};
        m_pImpl->hRHIPS = {};
        m_pImpl->hRHIVS = {};
    }

    m_pImpl->mapModels.clear();
    m_pImpl->mapTextures.clear();
    m_pImpl->cbFxParams.Release();
    m_pImpl->cbPerObject.Release();
    m_pImpl->cbPerFrame.Release();
}

NS_END
