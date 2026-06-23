#include "EldenRingRHITestCubeRenderer.h"

#include "Renderer/RHIMaterialResource.h"
#include "Renderer/RHIMeshResource.h"
#include "Renderer/RHISceneRenderer.h"
#include "Renderer/RenderWorldSnapshot.h"
#include "RHI/Geometry/CubeGeometry.h"
#include "RHI/IRHIDevice.h"

#include <Windows.h>

#include <DirectXMath.h>

#include <array>
#include <memory>

namespace
{
    struct TestVertex
    {
        float position[3];
        float color[4];
        float uv[2];
    };

    using CubeVertexArray = std::array<TestVertex, CubeGeometry::VERTEX_COUNT>;
    using CubeIndexArray = std::array<u32_t, CubeGeometry::INDEX_COUNT>;

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

    constexpr u32_t PackRgba(u32_t r, u32_t g, u32_t b, u32_t a = 255u)
    {
        return (r & 0xFFu) |
            ((g & 0xFFu) << 8u) |
            ((b & 0xFFu) << 16u) |
            ((a & 0xFFu) << 24u);
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
    if (!m_pDevice ||
        !m_pMeshResource ||
        !m_pMeshResource->IsReady() ||
        !m_pSceneRenderer ||
        !m_pSceneRenderer->IsReady())
    {
        return false;
    }

    for (u32_t i = 0; i < kMaterialCount; ++i)
    {
        if (!m_pMaterials[i] || !m_pMaterials[i]->IsReady())
            return false;
    }

    return true;
}

bool CRHITestCubeRenderer::Initialize(IRHIDevice* pDevice)
{
    if (!pDevice)
        return false;

    m_pDevice = pDevice;

    m_pSceneRenderer = CRHISceneRenderer::Create(pDevice);
    if (!m_pSceneRenderer)
    {
        Shutdown();
        return false;
    }

    const CubeVertexArray vertices = MakeCubeVertices();
    const CubeIndexArray indices = MakeCubeIndices();

    m_pMeshResource = CRHIMeshResource::CreateIndexed(
        pDevice,
        vertices.data(),
        static_cast<u32_t>(sizeof(TestVertex) * vertices.size()),
        static_cast<u32_t>(sizeof(TestVertex)),
        indices.data(),
        static_cast<u32_t>(indices.size()),
        eRenderVertexLayout::PositionColorUv,
        "EldenRing.RHI.TestCube.Mesh");

    constexpr u32_t kCheckerTextureSize = 64;
    constexpr u32_t kCheckerColors[kMaterialCount][2] =
    {
        { PackRgba(255u, 255u, 255u), PackRgba(48u, 48u, 48u) },
        { PackRgba(255u, 190u, 48u), PackRgba(24u, 80u, 145u) },
    };
    const char* const pMaterialNames[kMaterialCount] =
    {
        "EldenRing.RHI.TestCube.Material.A",
        "EldenRing.RHI.TestCube.Material.B",
    };

    for (u32_t i = 0; i < kMaterialCount; ++i)
    {
        m_pMaterials[i] = CRHIMaterialResource::CreateCheckerboard(
            pDevice,
            kCheckerTextureSize,
            kCheckerColors[i][0],
            kCheckerColors[i][1],
            pMaterialNames[i]);
    }

    if (!IsReady())
    {
        Shutdown();
        return false;
    }

#if defined(_DEBUG)
    OutputDebugStringA("[CRHITestCubeRenderer] RHI scene snapshot probe ready\n");
#endif

    return true;
}

void CRHITestCubeRenderer::Shutdown()
{
    m_pSceneRenderer.reset();

    for (u32_t i = 0; i < kMaterialCount; ++i)
        m_pMaterials[i].reset();

    m_pMeshResource.reset();
    m_pDevice = nullptr;
}

void CRHITestCubeRenderer::Update(f32_t deltaTime)
{
    m_fRotationSeconds += deltaTime;
}

void CRHITestCubeRenderer::Render(IRHIDevice* pDevice)
{
    if (!IsReady() || !pDevice || pDevice != m_pDevice)
        return;

    const RHIMeshSlice* pMeshSlices = m_pMeshResource->GetSlices();
    if (!pMeshSlices || m_pMeshResource->GetSliceCount() == 0)
        return;

    using namespace DirectX;

    const XMMATRIX rotation =
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

    RenderWorldSnapshot snapshot{};
    snapshot.view.matView = Mat4(view);
    snapshot.view.matProjection = Mat4(projection);
    snapshot.view.matViewProjection = Mat4(view * projection);
    snapshot.view.vCameraWorld = Vec3{ 0.0f, 1.15f, -3.0f };
    snapshot.view.iWidth = 1280;
    snapshot.view.iHeight = 720;
    snapshot.meshes.reserve(kMaterialCount);

    constexpr f32_t kOffsets[kMaterialCount] = { -0.75f, 0.75f };
    for (u32_t i = 0; i < kMaterialCount; ++i)
    {
        const XMMATRIX world =
            XMMatrixScaling(0.65f, 0.65f, 0.65f) *
            rotation *
            XMMatrixTranslation(kOffsets[i], 0.0f, 0.0f);

        RenderMeshItem item{};
        item.matWorld = Mat4(world);
        item.mesh = pMeshSlices[0];
        item.hAlbedoTexture = m_pMaterials[i]->GetAlbedoTexture();
        item.hSampler = m_pMaterials[i]->GetSampler();
        item.vTint = Vec4{ 1.f, 1.f, 1.f, 1.f };
        snapshot.meshes.push_back(item);
    }

    m_pSceneRenderer->Render(pDevice, snapshot);
}
