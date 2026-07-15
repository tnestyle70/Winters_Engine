#include "WintersPCH.h"

#include "Renderer/RHISceneRenderer.h"

#include "RHI/IRHICommandList.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHIShaderCompiler.h"
#include "ProfilerAPI.h"

#include <Windows.h>

#include <array>
#include <iterator>
#include <string>
#include <vector>

namespace
{
    struct SceneFrameConstants
    {
        DirectX::XMFLOAT4X4 viewProjection{};
    };

    struct SceneObjectConstants
    {
        DirectX::XMFLOAT4X4 world{};
        DirectX::XMFLOAT4 tint{ 1.f, 1.f, 1.f, 1.f };
    };

    constexpr u32_t kSceneConstantBufferSize = 256;

    constexpr char kSceneMeshShader[] = R"(
cbuffer SceneFrameConstants : register(b0)
{
    row_major float4x4 gViewProjection;
};

cbuffer SceneObjectConstants : register(b1)
{
    row_major float4x4 gWorld;
    float4 gTint;
};

Texture2D gAlbedo : register(t0);
SamplerState gAlbedoSampler : register(s0);

struct VSInputColor
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct VSInputStatic
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

PSInput VSMainColor(VSInputColor input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(worldPos, gViewProjection);
    output.color = input.color * gTint;
    output.uv = input.uv;
    return output;
}

PSInput VSMainStatic(VSInputStatic input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(worldPos, gViewProjection);

    float light = saturate(input.normal.y * 0.35f + 0.75f);
    output.color = float4(light.xxx, 1.0f) * gTint;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return gAlbedo.Sample(gAlbedoSampler, input.uv) * input.color;
}
)";

    RHITextureHandle CreateWhiteTexture(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return {};

        const u32_t whitePixel = 0xFFFFFFFFu;

        RHITextureDesc desc{};
        desc.width = 1;
        desc.height = 1;
        desc.format = eRHIFormat::R8G8B8A8_UNorm;
        desc.usageFlags = static_cast<u32_t>(eRHITextureUsage::ShaderResource);
        desc.debugName = "RHISceneRenderer.DefaultWhite";

        return pDevice->CreateTexture(desc, &whitePixel, sizeof(whitePixel));
    }

    RHISamplerHandle CreateDefaultSampler(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return {};

        RHISamplerDesc desc{};
        desc.filter = eRHIFilter::Linear;
        desc.addressU = eRHIAddressMode::Wrap;
        desc.addressV = eRHIAddressMode::Wrap;
        desc.addressW = eRHIAddressMode::Wrap;
        desc.debugName = "RHISceneRenderer.DefaultSampler";
        return pDevice->CreateSampler(desc);
    }

    RHIPipelineHandle CreateScenePipeline(
        IRHIDevice* pDevice,
        RHIShaderHandle vs,
        RHIShaderHandle ps,
        RHIBindGroupLayoutHandle layout,
        RHIInputElementDesc* pInputElements,
        u32_t inputElementCount,
        bool_t depthWrite,
        const char* debugName)
    {
        if (!pDevice)
            return {};

        RHIPipelineDesc pipelineDesc{};
        pipelineDesc.vsHandle = vs;
        pipelineDesc.psHandle = ps;
        pipelineDesc.inputElements = pInputElements;
        pipelineDesc.inputElementCount = inputElementCount;
        pipelineDesc.topology = eRHIPrimitiveTopology::TriangleList;
        pipelineDesc.blendMode = eRHIBlendMode::Opaque;
        pipelineDesc.cullMode = eRHICullMode::Back;
        pipelineDesc.depthWrite = depthWrite;
        pipelineDesc.depthOp = eRHIDepthOp::LessEqual;
        pipelineDesc.dsvFormat = eRHIFormat::D24_UNorm_S8_UInt;
        pipelineDesc.bindGroupLayouts[0] = layout;
        pipelineDesc.bindGroupLayoutCount = 1;
        pipelineDesc.rtvFormats[0] = eRHIFormat::R8G8B8A8_UNorm;
        pipelineDesc.rtvCount = 1;
        pipelineDesc.debugName = debugName;
        return pDevice->CreatePipeline(pipelineDesc);
    }
}

struct CRHISceneRenderer::Impl
{
    struct DrawSlot
    {
        RHIBufferHandle hObjectConstants{};
        RHIBindGroupHandle hBindGroup{};
    };

    IRHIDevice* pDevice = nullptr;
    RHIShaderHandle hColorVS{};
    RHIShaderHandle hColorPS{};
    RHIShaderHandle hStaticVS{};
    RHIShaderHandle hStaticPS{};
    RHIBindGroupLayoutHandle hLayout{};
    RHIPipelineHandle hColorPipeline{};
    RHIPipelineHandle hColorNoDepthPipeline{};
    RHIPipelineHandle hStaticPipeline{};
    RHIPipelineHandle hStaticNoDepthPipeline{};
    RHIBufferHandle hFrameConstants{};
    RHITextureHandle hDefaultTexture{};
    RHISamplerHandle hDefaultSampler{};
    std::vector<DrawSlot> drawSlots{};

    bool_t EnsureDrawSlots(u32_t count)
    {
        if (!pDevice)
            return false;

        while (drawSlots.size() < count)
        {
            RHIBufferDesc objectDesc{};
            objectDesc.sizeBytes = kSceneConstantBufferSize;
            objectDesc.usage = eRHIBufferUsage::Constant;
            objectDesc.memoryUsage = eRHIMemoryUsage::Dynamic;
            objectDesc.dynamic = true;
            objectDesc.debugName = "RHISceneRenderer.ObjectConstants";

            DrawSlot slot{};
            slot.hObjectConstants = pDevice->CreateBuffer(objectDesc, nullptr);
            if (!slot.hObjectConstants.IsValid())
                return false;

            RHIBindGroupResource resources[4]{};
            resources[0].slot = 0;
            resources[0].type = eRHIBindingType::ConstantBuffer;
            resources[0].bufferHandle = hFrameConstants;
            resources[1].slot = 1;
            resources[1].type = eRHIBindingType::ConstantBuffer;
            resources[1].bufferHandle = slot.hObjectConstants;
            resources[2].slot = 0;
            resources[2].type = eRHIBindingType::ShaderResource;
            resources[2].textureHandle = hDefaultTexture;
            resources[3].slot = 0;
            resources[3].type = eRHIBindingType::Sampler;
            resources[3].samplerHandle = hDefaultSampler;

            RHIBindGroupDesc bindGroupDesc{};
            bindGroupDesc.layoutHandle = hLayout;
            bindGroupDesc.resources = resources;
            bindGroupDesc.resourceCount = 4;
            bindGroupDesc.debugName = "RHISceneRenderer.DrawSlot";
            slot.hBindGroup = pDevice->CreateBindGroup(bindGroupDesc);
            if (!slot.hBindGroup.IsValid())
            {
                pDevice->DestroyBuffer(slot.hObjectConstants);
                return false;
            }

            drawSlots.push_back(slot);
        }

        return true;
    }
};

CRHISceneRenderer::~CRHISceneRenderer()
{
    Shutdown();
}

std::unique_ptr<CRHISceneRenderer> CRHISceneRenderer::Create(IRHIDevice* pDevice)
{
    auto pRenderer = std::unique_ptr<CRHISceneRenderer>(new CRHISceneRenderer());
    if (!pRenderer || !pDevice)
        return nullptr;

    pRenderer->m_pImpl = new Impl();
    Impl& impl = *pRenderer->m_pImpl;
    impl.pDevice = pDevice;

    const bool_t useSM51 = pDevice->GetBackend() == eRHIBackend::DX12;
    const char* pVSTarget = useSM51 ? "vs_5_1" : "vs_5_0";
    const char* pPSTarget = useSM51 ? "ps_5_1" : "ps_5_0";

    std::vector<u8_t> colorVsBytecode;
    std::vector<u8_t> colorPsBytecode;
    std::vector<u8_t> staticVsBytecode;
    std::vector<u8_t> staticPsBytecode;
    std::string errors;

    if (!RHI_CompileHlslShader(kSceneMeshShader, "VSMainColor", pVSTarget, colorVsBytecode, &errors) ||
        !RHI_CompileHlslShader(kSceneMeshShader, "PSMain", pPSTarget, colorPsBytecode, &errors) ||
        !RHI_CompileHlslShader(kSceneMeshShader, "VSMainStatic", pVSTarget, staticVsBytecode, &errors) ||
        !RHI_CompileHlslShader(kSceneMeshShader, "PSMain", pPSTarget, staticPsBytecode, &errors))
    {
        OutputDebugStringA(errors.c_str());
        pRenderer->Shutdown();
        return nullptr;
    }

    impl.hColorVS = pDevice->CreateShader(
        eRHIShaderStage::Vertex,
        colorVsBytecode.data(),
        static_cast<u32_t>(colorVsBytecode.size()),
        "RHISceneRenderer.ColorMesh.VS");
    impl.hColorPS = pDevice->CreateShader(
        eRHIShaderStage::Pixel,
        colorPsBytecode.data(),
        static_cast<u32_t>(colorPsBytecode.size()),
        "RHISceneRenderer.ColorMesh.PS");
    impl.hStaticVS = pDevice->CreateShader(
        eRHIShaderStage::Vertex,
        staticVsBytecode.data(),
        static_cast<u32_t>(staticVsBytecode.size()),
        "RHISceneRenderer.StaticMesh.VS");
    impl.hStaticPS = pDevice->CreateShader(
        eRHIShaderStage::Pixel,
        staticPsBytecode.data(),
        static_cast<u32_t>(staticPsBytecode.size()),
        "RHISceneRenderer.StaticMesh.PS");

    RHIBindingSlot bindingSlots[4]{};
    bindingSlots[0].slot = 0;
    bindingSlots[0].type = eRHIBindingType::ConstantBuffer;
    bindingSlots[0].visibility = eRHIShaderVisibility::Vertex;
    bindingSlots[1].slot = 1;
    bindingSlots[1].type = eRHIBindingType::ConstantBuffer;
    bindingSlots[1].visibility = eRHIShaderVisibility::All;
    bindingSlots[2].slot = 0;
    bindingSlots[2].type = eRHIBindingType::ShaderResource;
    bindingSlots[2].visibility = eRHIShaderVisibility::Pixel;
    bindingSlots[3].slot = 0;
    bindingSlots[3].type = eRHIBindingType::Sampler;
    bindingSlots[3].visibility = eRHIShaderVisibility::Pixel;

    RHIBindGroupLayoutDesc layoutDesc{};
    layoutDesc.slots = bindingSlots;
    layoutDesc.slotCount = 4;
    layoutDesc.debugName = "RHISceneRenderer.StaticMesh.Layout";
    impl.hLayout = pDevice->CreateBindGroupLayout(layoutDesc);

    RHIInputElementDesc colorInputElements[] =
    {
        { "POSITION", 0, eRHIFormat::R32G32B32_Float, 0, 0 },
        { "COLOR", 0, eRHIFormat::R32G32B32A32_Float, 12, 0 },
        { "TEXCOORD", 0, eRHIFormat::R32G32_Float, 28, 0 },
    };
    RHIInputElementDesc staticInputElements[] =
    {
        { "POSITION", 0, eRHIFormat::R32G32B32_Float, 0, 0 },
        { "NORMAL", 0, eRHIFormat::R32G32B32_Float, 12, 0 },
        { "TEXCOORD", 0, eRHIFormat::R32G32_Float, 24, 0 },
    };

    impl.hColorPipeline = CreateScenePipeline(
        pDevice,
        impl.hColorVS,
        impl.hColorPS,
        impl.hLayout,
        colorInputElements,
        static_cast<u32_t>(std::size(colorInputElements)),
        true,
        "RHISceneRenderer.ColorMesh.Pipeline");
    impl.hColorNoDepthPipeline = CreateScenePipeline(
        pDevice,
        impl.hColorVS,
        impl.hColorPS,
        impl.hLayout,
        colorInputElements,
        static_cast<u32_t>(std::size(colorInputElements)),
        false,
        "RHISceneRenderer.ColorMesh.NoDepthPipeline");
    impl.hStaticPipeline = CreateScenePipeline(
        pDevice,
        impl.hStaticVS,
        impl.hStaticPS,
        impl.hLayout,
        staticInputElements,
        static_cast<u32_t>(std::size(staticInputElements)),
        true,
        "RHISceneRenderer.StaticMesh.Pipeline");
    impl.hStaticNoDepthPipeline = CreateScenePipeline(
        pDevice,
        impl.hStaticVS,
        impl.hStaticPS,
        impl.hLayout,
        staticInputElements,
        static_cast<u32_t>(std::size(staticInputElements)),
        false,
        "RHISceneRenderer.StaticMesh.NoDepthPipeline");

    RHIBufferDesc frameDesc{};
    frameDesc.sizeBytes = kSceneConstantBufferSize;
    frameDesc.usage = eRHIBufferUsage::Constant;
    frameDesc.memoryUsage = eRHIMemoryUsage::Dynamic;
    frameDesc.dynamic = true;
    frameDesc.debugName = "RHISceneRenderer.FrameConstants";
    impl.hFrameConstants = pDevice->CreateBuffer(frameDesc, nullptr);
    impl.hDefaultTexture = CreateWhiteTexture(pDevice);
    impl.hDefaultSampler = CreateDefaultSampler(pDevice);

    if (!pRenderer->IsReady())
    {
        pRenderer->Shutdown();
        return nullptr;
    }

    return pRenderer;
}

bool_t CRHISceneRenderer::IsReady() const
{
    return m_pImpl &&
        m_pImpl->pDevice &&
        m_pImpl->hColorVS.IsValid() &&
        m_pImpl->hColorPS.IsValid() &&
        m_pImpl->hStaticVS.IsValid() &&
        m_pImpl->hStaticPS.IsValid() &&
        m_pImpl->hLayout.IsValid() &&
        m_pImpl->hColorPipeline.IsValid() &&
        m_pImpl->hColorNoDepthPipeline.IsValid() &&
        m_pImpl->hStaticPipeline.IsValid() &&
        m_pImpl->hStaticNoDepthPipeline.IsValid() &&
        m_pImpl->hFrameConstants.IsValid() &&
        m_pImpl->hDefaultTexture.IsValid() &&
        m_pImpl->hDefaultSampler.IsValid();
}

void CRHISceneRenderer::Render(IRHIDevice* pDevice, const RenderWorldSnapshot& snapshot)
{
    WINTERS_PROFILE_SCOPE("RHISceneRenderer::Render");
    if (!IsReady() || !pDevice || pDevice != m_pImpl->pDevice)
        return;

    IRHICommandList* pCommandList = pDevice->GetFrameCommandList();
    if (!pCommandList)
        return;

    const u32_t meshCount = static_cast<u32_t>(snapshot.meshes.size());
    WINTERS_PROFILE_COUNT("RHI::SceneMeshCandidates", meshCount);
    if (meshCount == 0 || !m_pImpl->EnsureDrawSlots(meshCount))
        return;

    SceneFrameConstants frameData{};
    frameData.viewProjection = snapshot.view.matViewProjection.m;
    pCommandList->UpdateBuffer(
        m_pImpl->hFrameConstants,
        &frameData,
        static_cast<u32_t>(sizeof(frameData)));

    u32_t submittedDraws = 0;
    u64_t submittedIndices = 0;
    for (u32_t i = 0; i < meshCount; ++i)
    {
        const RenderMeshItem& item = snapshot.meshes[i];
        const RHIMeshSlice& mesh = item.mesh;
        if (!mesh.hVertexBuffer.IsValid() ||
            !mesh.hIndexBuffer.IsValid() ||
            mesh.vertexStride == 0 ||
            mesh.indexCount == 0)
        {
            continue;
        }

        RHIPipelineHandle hPipeline{};
        if (mesh.vertexLayout == eRenderVertexLayout::PositionNormalUv)
        {
            hPipeline = item.bDepthWrite
                ? m_pImpl->hStaticPipeline
                : m_pImpl->hStaticNoDepthPipeline;
        }
        else
        {
            hPipeline = item.bDepthWrite
                ? m_pImpl->hColorPipeline
                : m_pImpl->hColorNoDepthPipeline;
        }
        if (!hPipeline.IsValid())
            continue;

        Impl::DrawSlot& slot = m_pImpl->drawSlots[i];
        SceneObjectConstants objectData{};
        objectData.world = item.matWorld.m;
        objectData.tint = item.vTint.ToXMFLOAT4();
        pCommandList->UpdateBuffer(
            slot.hObjectConstants,
            &objectData,
            static_cast<u32_t>(sizeof(objectData)));

        RHIBindGroupResource resources[4]{};
        resources[0].slot = 0;
        resources[0].type = eRHIBindingType::ConstantBuffer;
        resources[0].bufferHandle = m_pImpl->hFrameConstants;
        resources[1].slot = 1;
        resources[1].type = eRHIBindingType::ConstantBuffer;
        resources[1].bufferHandle = slot.hObjectConstants;
        resources[2].slot = 0;
        resources[2].type = eRHIBindingType::ShaderResource;
        resources[2].textureHandle = item.hAlbedoTexture.IsValid()
            ? item.hAlbedoTexture
            : m_pImpl->hDefaultTexture;
        resources[3].slot = 0;
        resources[3].type = eRHIBindingType::Sampler;
        resources[3].samplerHandle = item.hSampler.IsValid()
            ? item.hSampler
            : m_pImpl->hDefaultSampler;

        pDevice->UpdateBindGroup(
            slot.hBindGroup,
            resources,
            static_cast<u32_t>(std::size(resources)));
        pCommandList->SetPipeline(hPipeline);
        pCommandList->SetBindGroup(0, slot.hBindGroup);
        pCommandList->SetVertexBuffer(0, mesh.hVertexBuffer, mesh.vertexStride, 0);
        pCommandList->SetIndexBuffer(mesh.hIndexBuffer, 0, eRHIFormat::R32_UInt);
        pCommandList->DrawIndexed(
            mesh.indexCount,
            1,
            mesh.firstIndex,
            mesh.baseVertex,
            0);
        ++submittedDraws;
        submittedIndices += mesh.indexCount;
    }

    WINTERS_PROFILE_COUNT("RHI::SceneDrawCalls", submittedDraws);
    WINTERS_PROFILE_COUNT("RHI::SceneSubmittedIndices", submittedIndices);
}

void CRHISceneRenderer::Shutdown()
{
    if (!m_pImpl)
        return;

    IRHIDevice* pDevice = m_pImpl->pDevice;
    if (pDevice)
    {
        for (Impl::DrawSlot& slot : m_pImpl->drawSlots)
        {
            if (slot.hBindGroup.IsValid())
                pDevice->DestroyBindGroup(slot.hBindGroup);
            if (slot.hObjectConstants.IsValid())
                pDevice->DestroyBuffer(slot.hObjectConstants);
        }

        if (m_pImpl->hDefaultSampler.IsValid())
            pDevice->DestroySampler(m_pImpl->hDefaultSampler);
        if (m_pImpl->hDefaultTexture.IsValid())
            pDevice->DestroyTexture(m_pImpl->hDefaultTexture);
        if (m_pImpl->hFrameConstants.IsValid())
            pDevice->DestroyBuffer(m_pImpl->hFrameConstants);
        if (m_pImpl->hStaticNoDepthPipeline.IsValid())
            pDevice->DestroyPipeline(m_pImpl->hStaticNoDepthPipeline);
        if (m_pImpl->hStaticPipeline.IsValid())
            pDevice->DestroyPipeline(m_pImpl->hStaticPipeline);
        if (m_pImpl->hColorNoDepthPipeline.IsValid())
            pDevice->DestroyPipeline(m_pImpl->hColorNoDepthPipeline);
        if (m_pImpl->hColorPipeline.IsValid())
            pDevice->DestroyPipeline(m_pImpl->hColorPipeline);
        if (m_pImpl->hLayout.IsValid())
            pDevice->DestroyBindGroupLayout(m_pImpl->hLayout);
        if (m_pImpl->hStaticPS.IsValid())
            pDevice->DestroyShader(m_pImpl->hStaticPS);
        if (m_pImpl->hStaticVS.IsValid())
            pDevice->DestroyShader(m_pImpl->hStaticVS);
        if (m_pImpl->hColorPS.IsValid())
            pDevice->DestroyShader(m_pImpl->hColorPS);
        if (m_pImpl->hColorVS.IsValid())
            pDevice->DestroyShader(m_pImpl->hColorVS);
    }

    m_pImpl->drawSlots.clear();
    m_pImpl->pDevice = nullptr;
    delete m_pImpl;
    m_pImpl = nullptr;
}
