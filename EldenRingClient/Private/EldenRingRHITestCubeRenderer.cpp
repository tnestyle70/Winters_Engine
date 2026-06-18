#include "EldenRingRHITestCubeRenderer.h"

#include "RHI/Geometry/CubeGeometry.h"
#include "RHI/IRHICommandList.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHIShaderCompiler.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <array>
#include <string>
#include <vector>

namespace
{
    struct TestVertex
    {
        float position[3];
        float color[4];
        float uv[2];
    };

    struct CameraConstants
    {
        DirectX::XMFLOAT4X4 worldViewProjection{};
    };

    using CubeVertexArray = std::array<TestVertex, CubeGeometry::VERTEX_COUNT>;
    using CubeIndexArray = std::array<u32_t, CubeGeometry::INDEX_COUNT>;

    constexpr u32_t kCameraConstantBufferSize = 256;

    // register space를 쓰지 않아 SM5.0(DX11)/SM5.1(DX12) 양쪽에서 컴파일된다 (기본 space0).
    constexpr char kCubeShader[] = R"(
cbuffer CameraConstants : register(b0)
{
    row_major float4x4 gWorldViewProjection;
};

Texture2D gAlbedo : register(t0);
SamplerState gAlbedoSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), gWorldViewProjection);
    output.color = input.color;
    output.uv = input.uv;
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return gAlbedo.Sample(gAlbedoSampler, input.uv) * input.color;
}
)";

    CubeIndexArray MakeCubeIndices()
    {
        CubeIndexArray indices{};

        for (size_t i = 0; i < indices.size(); ++i)
            indices[i] = static_cast<u32_t>(CubeGeometry::Indices[i]);

        return indices;
    }

    CubeVertexArray MakeCubeVertices()
    {
        CubeVertexArray vertices{};

        // CubeGeometry는 면마다 4개 꼭짓점을 같은 코너 순서(BL, TL, TR, BR)로 나열한다.
        static constexpr float kCornerUVs[4][2] =
        {
            { 0.f, 1.f },
            { 0.f, 0.f },
            { 1.f, 0.f },
            { 1.f, 1.f },
        };

        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const CubeGeometry::Vertex3D& src = CubeGeometry::Vertices[i];

            vertices[i].position[0] = src.position[0];
            vertices[i].position[1] = src.position[1];
            vertices[i].position[2] = src.position[2];

            vertices[i].color[0] = src.color[0];
            vertices[i].color[1] = src.color[1];
            vertices[i].color[2] = src.color[2];
            vertices[i].color[3] = src.color[3];

            vertices[i].uv[0] = kCornerUVs[i % 4][0];
            vertices[i].uv[1] = kCornerUVs[i % 4][1];
        }

        return vertices;
    }

    std::vector<u32_t> MakeCheckerTexels(u32_t size)
    {
        std::vector<u32_t> texels(static_cast<size_t>(size) * size);

        for (u32_t y = 0; y < size; ++y)
        {
            for (u32_t x = 0; x < size; ++x)
            {
                const bool_t bWhite = (((x / 8u) + (y / 8u)) % 2u) == 0u;
                texels[static_cast<size_t>(y) * size + x] = bWhite ? 0xFFFFFFFFu : 0xFF303030u;
            }
        }

        return texels;
    }
}

CRHITestCubeRenderer::~CRHITestCubeRenderer()
{
    Shutdown();
}

std::unique_ptr<CRHITestCubeRenderer> CRHITestCubeRenderer::Create(IRHIDevice* pDevice)
{
    std::unique_ptr<CRHITestCubeRenderer> pRenderer(new CRHITestCubeRenderer());

    if (!pRenderer->Initialize(pDevice))
        return nullptr;

    return pRenderer;
}

bool CRHITestCubeRenderer::IsReady() const
{
    return m_pDevice &&
        m_hVertexBuffer.IsValid() &&
        m_hIndexBuffer.IsValid() &&
        m_hCameraConstantBuffer.IsValid() &&
        m_hCheckerTexture.IsValid() &&
        m_hCheckerSampler.IsValid() &&
        m_hCameraBindGroupLayout.IsValid() &&
        m_hCameraBindGroup.IsValid() &&
        m_hVertexShader.IsValid() &&
        m_hPixelShader.IsValid() &&
        m_hPipeline.IsValid();
}

bool CRHITestCubeRenderer::Initialize(IRHIDevice* pDevice)
{
    if (!pDevice)
        return false;

    m_pDevice = pDevice;

    std::vector<u8_t> vertexShaderBytecode;
    std::vector<u8_t> pixelShaderBytecode;
    std::string errors;

    // DX11은 SM5.0까지만 받으므로 backend에 맞춰 타깃을 고른다.
    const bool_t bUseSM51 = pDevice->GetBackend() == eRHIBackend::DX12;
    const char* pVSTarget = bUseSM51 ? "vs_5_1" : "vs_5_0";
    const char* pPSTarget = bUseSM51 ? "ps_5_1" : "ps_5_0";

    if (!RHI_CompileHlslShader(kCubeShader, "VSMain", pVSTarget, vertexShaderBytecode, &errors))
    {
        OutputDebugStringA(errors.c_str());
        return false;
    }

    if (!RHI_CompileHlslShader(kCubeShader, "PSMain", pPSTarget, pixelShaderBytecode, &errors))
    {
        OutputDebugStringA(errors.c_str());
        return false;
    }

    m_hVertexShader = pDevice->CreateShader(
        eRHIShaderStage::Vertex,
        vertexShaderBytecode.data(),
        static_cast<u32_t>(vertexShaderBytecode.size()),
        "EldenRing.RHI.TestCube.VS");

    m_hPixelShader = pDevice->CreateShader(
        eRHIShaderStage::Pixel,
        pixelShaderBytecode.data(),
        static_cast<u32_t>(pixelShaderBytecode.size()),
        "EldenRing.RHI.TestCube.PS");

    const CubeVertexArray vertices = MakeCubeVertices();

    RHIBufferDesc vertexBufferDesc{};
    vertexBufferDesc.sizeBytes = static_cast<u32_t>(sizeof(TestVertex) * vertices.size());
    vertexBufferDesc.usage = eRHIBufferUsage::Vertex;
    vertexBufferDesc.memoryUsage = eRHIMemoryUsage::Default;
    vertexBufferDesc.dynamic = false;
    vertexBufferDesc.debugName = "EldenRing.RHI.TestCube.VertexBuffer";
    m_hVertexBuffer = pDevice->CreateBuffer(vertexBufferDesc, vertices.data());

    const CubeIndexArray indices = MakeCubeIndices();

    RHIBufferDesc indexBufferDesc{};
    indexBufferDesc.sizeBytes = static_cast<u32_t>(sizeof(u32_t) * indices.size());
    indexBufferDesc.usage = eRHIBufferUsage::Index;
    indexBufferDesc.memoryUsage = eRHIMemoryUsage::Default;
    indexBufferDesc.debugName = "EldenRing.RHI.TestCube.IndexBuffer";
    m_hIndexBuffer = pDevice->CreateBuffer(indexBufferDesc, indices.data());

    RHIBufferDesc cameraBufferDesc{};
    cameraBufferDesc.sizeBytes = kCameraConstantBufferSize;
    cameraBufferDesc.usage = eRHIBufferUsage::Constant;
    cameraBufferDesc.memoryUsage = eRHIMemoryUsage::Dynamic;
    cameraBufferDesc.dynamic = true;
    cameraBufferDesc.debugName = "EldenRing.RHI.TestCube.CameraConstants";
    m_hCameraConstantBuffer = pDevice->CreateBuffer(cameraBufferDesc, nullptr);

    constexpr u32_t kCheckerTextureSize = 64;
    const std::vector<u32_t> checkerTexels = MakeCheckerTexels(kCheckerTextureSize);

    RHITextureDesc checkerTextureDesc{};
    checkerTextureDesc.width = kCheckerTextureSize;
    checkerTextureDesc.height = kCheckerTextureSize;
    checkerTextureDesc.mipLevels = 1;
    checkerTextureDesc.format = eRHIFormat::R8G8B8A8_UNorm;
    checkerTextureDesc.debugName = "EldenRing.RHI.TestCube.CheckerTexture";
    m_hCheckerTexture = pDevice->CreateTexture(
        checkerTextureDesc,
        checkerTexels.data(),
        kCheckerTextureSize * sizeof(u32_t));

    RHISamplerDesc checkerSamplerDesc{};
    checkerSamplerDesc.filter = eRHIFilter::Linear;
    checkerSamplerDesc.addressU = eRHIAddressMode::Wrap;
    checkerSamplerDesc.addressV = eRHIAddressMode::Wrap;
    checkerSamplerDesc.addressW = eRHIAddressMode::Wrap;
    checkerSamplerDesc.debugName = "EldenRing.RHI.TestCube.CheckerSampler";
    m_hCheckerSampler = pDevice->CreateSampler(checkerSamplerDesc);

    RHIBindingSlot bindingSlots[3]{};
    bindingSlots[0].slot = 0;
    bindingSlots[0].type = eRHIBindingType::ConstantBuffer;
    bindingSlots[0].visibility = eRHIShaderVisibility::Vertex;
    bindingSlots[1].slot = 0;
    bindingSlots[1].type = eRHIBindingType::ShaderResource;
    bindingSlots[1].visibility = eRHIShaderVisibility::Pixel;
    bindingSlots[2].slot = 0;
    bindingSlots[2].type = eRHIBindingType::Sampler;
    bindingSlots[2].visibility = eRHIShaderVisibility::Pixel;

    RHIBindGroupLayoutDesc cameraLayoutDesc{};
    cameraLayoutDesc.slots = bindingSlots;
    cameraLayoutDesc.slotCount = 3;
    cameraLayoutDesc.debugName = "EldenRing.RHI.TestCube.CameraBindGroupLayout";
    m_hCameraBindGroupLayout = pDevice->CreateBindGroupLayout(cameraLayoutDesc);

    RHIBindGroupResource bindGroupResources[3]{};
    bindGroupResources[0].slot = 0;
    bindGroupResources[0].type = eRHIBindingType::ConstantBuffer;
    bindGroupResources[0].bufferHandle = m_hCameraConstantBuffer;
    bindGroupResources[1].slot = 0;
    bindGroupResources[1].type = eRHIBindingType::ShaderResource;
    bindGroupResources[1].textureHandle = m_hCheckerTexture;
    bindGroupResources[2].slot = 0;
    bindGroupResources[2].type = eRHIBindingType::Sampler;
    bindGroupResources[2].samplerHandle = m_hCheckerSampler;

    RHIBindGroupDesc cameraBindGroupDesc{};
    cameraBindGroupDesc.layoutHandle = m_hCameraBindGroupLayout;
    cameraBindGroupDesc.resources = bindGroupResources;
    cameraBindGroupDesc.resourceCount = 3;
    cameraBindGroupDesc.debugName = "EldenRing.RHI.TestCube.CameraBindGroup";
    m_hCameraBindGroup = pDevice->CreateBindGroup(cameraBindGroupDesc);

    RHIInputElementDesc inputElements[] =
    {
        { "POSITION", 0, eRHIFormat::R32G32B32_Float, 0, 0 },
        { "COLOR", 0, eRHIFormat::R32G32B32A32_Float, 12, 0 },
        { "TEXCOORD", 0, eRHIFormat::R32G32_Float, 28, 0 },
    };

    RHIPipelineDesc pipelineDesc{};
    pipelineDesc.vsHandle = m_hVertexShader;
    pipelineDesc.psHandle = m_hPixelShader;
    pipelineDesc.inputElements = inputElements;
    pipelineDesc.inputElementCount = 3;
    pipelineDesc.topology = eRHIPrimitiveTopology::TriangleList;
    pipelineDesc.blendMode = eRHIBlendMode::Opaque;
    pipelineDesc.cullMode = eRHICullMode::Back;
    pipelineDesc.depthWrite = true;
    pipelineDesc.depthOp = eRHIDepthOp::LessEqual;
    pipelineDesc.dsvFormat = eRHIFormat::D24_UNorm_S8_UInt;
    pipelineDesc.bindGroupLayouts[0] = m_hCameraBindGroupLayout;
    pipelineDesc.bindGroupLayoutCount = 1;
    pipelineDesc.rtvFormats[0] = eRHIFormat::R8G8B8A8_UNorm;
    pipelineDesc.rtvCount = 1;
    pipelineDesc.debugName = "EldenRing.RHI.TestCube.Pipeline";
    m_hPipeline = pDevice->CreatePipeline(pipelineDesc);

    if (!IsReady())
    {
        Shutdown();
        return false;
    }

#if defined(_DEBUG)
    OutputDebugStringA("[CRHITestCubeRenderer] RHI test cube ready\n");
#endif

    return true;
}

void CRHITestCubeRenderer::Shutdown()
{
    if (!m_pDevice)
        return;

    if (m_hPipeline.IsValid())
    {
        m_pDevice->DestroyPipeline(m_hPipeline);
        m_hPipeline = {};
    }

    if (m_hCameraBindGroup.IsValid())
    {
        m_pDevice->DestroyBindGroup(m_hCameraBindGroup);
        m_hCameraBindGroup = {};
    }

    if (m_hCameraBindGroupLayout.IsValid())
    {
        m_pDevice->DestroyBindGroupLayout(m_hCameraBindGroupLayout);
        m_hCameraBindGroupLayout = {};
    }

    if (m_hCheckerSampler.IsValid())
    {
        m_pDevice->DestroySampler(m_hCheckerSampler);
        m_hCheckerSampler = {};
    }

    if (m_hCheckerTexture.IsValid())
    {
        m_pDevice->DestroyTexture(m_hCheckerTexture);
        m_hCheckerTexture = {};
    }

    if (m_hCameraConstantBuffer.IsValid())
    {
        m_pDevice->DestroyBuffer(m_hCameraConstantBuffer);
        m_hCameraConstantBuffer = {};
    }

    if (m_hIndexBuffer.IsValid())
    {
        m_pDevice->DestroyBuffer(m_hIndexBuffer);
        m_hIndexBuffer = {};
    }

    if (m_hVertexBuffer.IsValid())
    {
        m_pDevice->DestroyBuffer(m_hVertexBuffer);
        m_hVertexBuffer = {};
    }

    if (m_hPixelShader.IsValid())
    {
        m_pDevice->DestroyShader(m_hPixelShader);
        m_hPixelShader = {};
    }

    if (m_hVertexShader.IsValid())
    {
        m_pDevice->DestroyShader(m_hVertexShader);
        m_hVertexShader = {};
    }

    m_pDevice = nullptr;
}

void CRHITestCubeRenderer::Update(f32_t deltaTime)
{
    m_fRotationSeconds += deltaTime;
}

void CRHITestCubeRenderer::Render(IRHIDevice* pDevice)
{
    if (!IsReady() || !pDevice)
        return;

    IRHICommandList* pCommandList = pDevice->GetFrameCommandList();
    if (!pCommandList)
        return;

    using namespace DirectX;

    const XMMATRIX world =
        XMMatrixRotationX(m_fRotationSeconds * 0.65f) *
        XMMatrixRotationY(m_fRotationSeconds);

    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(0.0f, 1.15f, -3.0f, 1.0f),
        XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(60.0f),
        16.0f / 9.0f,
        0.1f,
        100.0f);

    CameraConstants constants{};
    XMStoreFloat4x4(&constants.worldViewProjection, world * view * projection);

    pCommandList->UpdateBuffer(
        m_hCameraConstantBuffer,
        &constants,
        static_cast<u32_t>(sizeof(constants)));

    pCommandList->SetPipeline(m_hPipeline);
    pCommandList->SetBindGroup(0, m_hCameraBindGroup);
    pCommandList->SetVertexBuffer(0, m_hVertexBuffer, sizeof(TestVertex), 0);
    pCommandList->SetIndexBuffer(m_hIndexBuffer, 0, eRHIFormat::R32_UInt);
    pCommandList->DrawIndexed(CubeGeometry::INDEX_COUNT, 1, 0, 0, 0);
}
