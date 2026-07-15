#include "Manager/Navigation/MapSurfaceSampler.h"

#include "AssetFormat/Mesh/WMeshLoader.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

NS_BEGIN(Engine)

namespace
{
    constexpr f32_t kWallBlockerNormalY = 0.45f;
    constexpr f32_t kWallBlockerHalfWidth = 0.45f;
}

bool_t CMapSurfaceSampler::LoadFromWMesh(
    const wchar_t* pPath,
    const Mat4& matWorld,
    const std::atomic_bool* pCancel)
{
    Reset();

    if (pCancel && pCancel->load(std::memory_order_acquire))
        return false;

    Winters::Asset::WMeshLoaded mesh{};
    if (!Winters::Asset::CWMeshLoader::Load(pPath, mesh))
        return false;
    if (pCancel && pCancel->load(std::memory_order_acquire))
        return false;
    if (!mesh.pVertexBlob || !mesh.pIndexBlob || mesh.header.total_vertex_count == 0)
        return false;

    std::vector<Vec3> vertices{};
    if (!BuildWorldVertices(mesh, matWorld, vertices, pCancel))
    {
        Reset();
        return false;
    }

    if (!BuildSurfaceCells(mesh, vertices, pCancel))
    {
        Reset();
        return false;
    }

    m_bReady = true;
    LogLoadedSurface();
    return true;
}

bool_t CMapSurfaceSampler::BuildWorldVertices(
    const Winters::Asset::WMeshLoaded& mesh,
    const Mat4& matWorld,
    std::vector<Vec3>& outVertices,
    const std::atomic_bool* pCancel)
{
    const u32_t vertexCount = mesh.header.total_vertex_count;
    const u32_t stride = mesh.header.vertex_stride;
    outVertices.resize(vertexCount);

    m_fMinX = (std::numeric_limits<f32_t>::max)();
    m_fMinZ = (std::numeric_limits<f32_t>::max)();
    m_fMaxX = -(std::numeric_limits<f32_t>::max)();
    m_fMaxZ = -(std::numeric_limits<f32_t>::max)();

    const DirectX::XMMATRIX mat = matWorld.ToXMMATRIX();
    for (u32_t i = 0; i < vertexCount; ++i)
    {
        if ((i & 0x0FFFu) == 0u &&
            pCancel && pCancel->load(std::memory_order_acquire))
        {
            return false;
        }

        const u8_t* pVertex = mesh.pVertexBlob + static_cast<size_t>(i) * stride;
        const f32_t* pPos = reinterpret_cast<const f32_t*>(pVertex);
        const DirectX::XMVECTOR local =
            DirectX::XMVectorSet(pPos[0], pPos[1], pPos[2], 1.f);
        const DirectX::XMVECTOR world =
            DirectX::XMVector3TransformCoord(local, mat);
        DirectX::XMFLOAT3 out{};
        DirectX::XMStoreFloat3(&out, world);

        const Vec3 v{ out.x, out.y, out.z };
        outVertices[i] = v;
        m_fMinX = (std::min)(m_fMinX, v.x);
        m_fMinZ = (std::min)(m_fMinZ, v.z);
        m_fMaxX = (std::max)(m_fMaxX, v.x);
        m_fMaxZ = (std::max)(m_fMaxZ, v.z);
    }

    return
        (!pCancel || !pCancel->load(std::memory_order_acquire)) &&
        HasValidBounds();
}

bool_t CMapSurfaceSampler::HasValidBounds() const
{
    return
        std::isfinite(m_fMinX) &&
        std::isfinite(m_fMinZ) &&
        std::isfinite(m_fMaxX) &&
        std::isfinite(m_fMaxZ) &&
        (m_fMaxX - m_fMinX) > 0.001f &&
        (m_fMaxZ - m_fMinZ) > 0.001f;
}

bool_t CMapSurfaceSampler::BuildSurfaceCells(
    const Winters::Asset::WMeshLoaded& mesh,
    const std::vector<Vec3>& vertices,
    const std::atomic_bool* pCancel)
{
    const u32_t vertexCount = static_cast<u32_t>(vertices.size());
    const u32_t stride = mesh.header.vertex_stride;
    m_fStepX = (m_fMaxX - m_fMinX) / static_cast<f32_t>(kGridDim - 1);
    m_fStepZ = (m_fMaxZ - m_fMinZ) / static_cast<f32_t>(kGridDim - 1);
    m_vecCells.assign(kGridDim * kGridDim, SurfaceCell{});

    u32_t indicesSinceCancelCheck = 0;
    for (const auto& submesh : mesh.subMeshes)
    {
        const u32_t baseVertex = submesh.vertex_offset / stride;
        for (u32_t i = 0; i + 2 < submesh.index_count; i += 3)
        {
            indicesSinceCancelCheck += 3;
            if (indicesSinceCancelCheck >= 4096u)
            {
                indicesSinceCancelCheck = 0;
                if (pCancel && pCancel->load(std::memory_order_acquire))
                    return false;
            }

            const u32_t i0 = baseVertex + ReadIndex(mesh, submesh.index_offset, i + 0);
            const u32_t i1 = baseVertex + ReadIndex(mesh, submesh.index_offset, i + 1);
            const u32_t i2 = baseVertex + ReadIndex(mesh, submesh.index_offset, i + 2);
            if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                continue;

            RasterizeTriangle(vertices[i0], vertices[i1], vertices[i2]);
        }
    }

    return !pCancel || !pCancel->load(std::memory_order_acquire);
}

void CMapSurfaceSampler::LogLoadedSurface() const
{
    char msg[224]{};
    sprintf_s(
        msg,
        "[MapSurface] loaded grid=%u bounds=(%.1f,%.1f)-(%.1f,%.1f)\n",
        static_cast<u32_t>(kGridDim),
        m_fMinX,
        m_fMinZ,
        m_fMaxX,
        m_fMaxZ);
    OutputDebugStringA(msg);
}

bool_t CMapSurfaceSampler::SampleHeight(f32_t x, f32_t z, f32_t& outHeight) const
{
    MapSurfaceSample sample{};
    if (!SampleSurface(x, z, sample))
        return false;

    outHeight = sample.height;
    return true;
}

bool_t CMapSurfaceSampler::SampleSurface(f32_t x, f32_t z, MapSurfaceSample& outSample) const
{
    if (!m_bReady || m_vecCells.empty())
        return false;
    if (x < m_fMinX || x > m_fMaxX || z < m_fMinZ || z > m_fMaxZ)
        return false;

    const f32_t gx = (x - m_fMinX) / m_fStepX;
    const f32_t gz = (z - m_fMinZ) / m_fStepZ;
    const i32_t ix = static_cast<i32_t>(std::floor(gx));
    const i32_t iz = static_cast<i32_t>(std::floor(gz));
    const f32_t tx = gx - static_cast<f32_t>(ix);
    const f32_t tz = gz - static_cast<f32_t>(iz);

    SurfaceCell c00{}, c10{}, c01{}, c11{};
    if (ReadCell(ix, iz, c00) &&
        ReadCell(ix + 1, iz, c10) &&
        ReadCell(ix, iz + 1, c01) &&
        ReadCell(ix + 1, iz + 1, c11))
    {
        const f32_t h0 = c00.height + (c10.height - c00.height) * tx;
        const f32_t h1 = c01.height + (c11.height - c01.height) * tx;
        outSample.height = h0 + (h1 - h0) * tz;
        outSample.normalY = (std::min)((std::min)(c00.normalY, c10.normalY),
            (std::min)(c01.normalY, c11.normalY));
        return true;
    }

    SurfaceCell nearest{};
    if (!ReadNearestCell(
        static_cast<i32_t>(std::round(gx)),
        static_cast<i32_t>(std::round(gz)),
        nearest))
    {
        return false;
    }

    outSample.height = nearest.height;
    outSample.normalY = nearest.normalY;
    return true;
}

void CMapSurfaceSampler::Reset()
{
    m_bReady = false;
    m_vecCells.clear();
    m_fMinX = m_fMinZ = 0.f;
    m_fMaxX = m_fMaxZ = 0.f;
    m_fStepX = m_fStepZ = 1.f;
}

u32_t CMapSurfaceSampler::ReadIndex(
    const Winters::Asset::WMeshLoaded& mesh,
    u32_t byteOffset,
    u32_t ordinal)
{
    const u8_t* pIndex =
        mesh.pIndexBlob + byteOffset +
        static_cast<size_t>(ordinal) * mesh.header.index_stride;
    if (mesh.header.index_stride == 4)
        return *reinterpret_cast<const u32_t*>(pIndex);

    return static_cast<u32_t>(*reinterpret_cast<const u16_t*>(pIndex));
}

i32_t CMapSurfaceSampler::ToGridX(f32_t x) const
{
    return static_cast<i32_t>(std::floor((x - m_fMinX) / m_fStepX));
}

i32_t CMapSurfaceSampler::ToGridZ(f32_t z) const
{
    return static_cast<i32_t>(std::floor((z - m_fMinZ) / m_fStepZ));
}

i32_t CMapSurfaceSampler::ClampGrid(i32_t v) const
{
    return (std::clamp)(v, 0, kGridDim - 1);
}

f32_t CMapSurfaceSampler::GridToX(i32_t ix) const
{
    return m_fMinX + static_cast<f32_t>(ix) * m_fStepX;
}

f32_t CMapSurfaceSampler::GridToZ(i32_t iz) const
{
    return m_fMinZ + static_cast<f32_t>(iz) * m_fStepZ;
}

bool_t CMapSurfaceSampler::IsValidHeight(f32_t h) const
{
    return h > (kInvalidHeight * 0.5f);
}

bool_t CMapSurfaceSampler::IsValidCell(const SurfaceCell& cell) const
{
    return IsValidHeight(cell.height);
}

bool_t CMapSurfaceSampler::SelectBetterSurface(const SurfaceCell& cell, f32_t height, f32_t normalY) const
{
    constexpr f32_t kWalkableLikeNormalY = 0.60f;
    constexpr f32_t kSameSurfaceHeightTolerance = 0.75f;
    constexpr f32_t kNormalTieBreak = 0.04f;

    if (!IsValidCell(cell))
        return true;

    const bool_t bCurrentWalkableLike = cell.normalY >= kWalkableLikeNormalY;
    const bool_t bNextWalkableLike = normalY >= kWalkableLikeNormalY;
    if (bCurrentWalkableLike != bNextWalkableLike)
        return bNextWalkableLike;

    if (bNextWalkableLike)
    {
        const f32_t heightDelta = height - cell.height;
        if (std::fabs(heightDelta) > kSameSurfaceHeightTolerance)
            return height > cell.height;

        if (normalY > cell.normalY + kNormalTieBreak)
            return true;
        if (cell.normalY > normalY + kNormalTieBreak)
            return false;

        return height > cell.height;
    }

    return height > cell.height;
}

void CMapSurfaceSampler::RasterizeTriangle(const Vec3& a, const Vec3& b, const Vec3& c)
{
    const f32_t denom =
        (b.z - c.z) * (a.x - c.x) +
        (c.x - b.x) * (a.z - c.z);
    if (std::fabs(denom) <= 0.000001f)
        return;

    const Vec3 ab{ b.x - a.x, b.y - a.y, b.z - a.z };
    const Vec3 ac{ c.x - a.x, c.y - a.y, c.z - a.z };
    const f32_t normalY = std::fabs(Vec3::Cross(ab, ac).Normalized().y);

    const f32_t minX = (std::min)((std::min)(a.x, b.x), c.x);
    const f32_t maxX = (std::max)((std::max)(a.x, b.x), c.x);
    const f32_t minZ = (std::min)((std::min)(a.z, b.z), c.z);
    const f32_t maxZ = (std::max)((std::max)(a.z, b.z), c.z);

    const i32_t x0 = ClampGrid(ToGridX(minX));
    const i32_t x1 = ClampGrid(ToGridX(maxX) + 1);
    const i32_t z0 = ClampGrid(ToGridZ(minZ));
    const i32_t z1 = ClampGrid(ToGridZ(maxZ) + 1);

    for (i32_t iz = z0; iz <= z1; ++iz)
    {
        const f32_t z = GridToZ(iz);
        for (i32_t ix = x0; ix <= x1; ++ix)
        {
            const f32_t x = GridToX(ix);
            const f32_t w0 =
                ((b.z - c.z) * (x - c.x) +
                    (c.x - b.x) * (z - c.z)) / denom;
            const f32_t w1 =
                ((c.z - a.z) * (x - c.x) +
                    (a.x - c.x) * (z - c.z)) / denom;
            const f32_t w2 = 1.f - w0 - w1;

            if (w0 < -0.001f || w1 < -0.001f || w2 < -0.001f)
                continue;

            const f32_t h = a.y * w0 + b.y * w1 + c.y * w2;
            SurfaceCell& cell = m_vecCells[static_cast<size_t>(iz) * kGridDim + ix];
            if (SelectBetterSurface(cell, h, normalY))
            {
                cell.height = h;
                cell.normalY = normalY;
            }
        }
    }
}

bool_t CMapSurfaceSampler::ReadCell(i32_t ix, i32_t iz, SurfaceCell& outCell) const
{
    if (ix < 0 || iz < 0 || ix >= kGridDim || iz >= kGridDim)
        return false;

    const SurfaceCell& cell = m_vecCells[static_cast<size_t>(iz) * kGridDim + ix];
    if (!IsValidCell(cell))
        return false;

    outCell = cell;
    return true;
}

bool_t CMapSurfaceSampler::ReadNearestCell(i32_t ix, i32_t iz, SurfaceCell& outCell) const
{
    for (i32_t r = 0; r <= 8; ++r)
    {
        for (i32_t dz = -r; dz <= r; ++dz)
        {
            for (i32_t dx = -r; dx <= r; ++dx)
            {
                if (std::abs(dx) != r && std::abs(dz) != r)
                    continue;
                if (ReadCell(ix + dx, iz + dz, outCell))
                    return true;
            }
        }
    }

    return false;
}

NS_END
