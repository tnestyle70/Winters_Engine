#pragma once

#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <atomic>
#include <vector>

namespace Winters::Asset
{
    struct WMeshLoaded;
}

NS_BEGIN(Engine)

struct MapSurfaceSample
{
    f32_t height = 0.f;
    f32_t normalY = 1.f;
    bool_t bWallBlocker = false;
};

class WINTERS_ENGINE CMapSurfaceSampler final
{
public:
    bool_t LoadFromWMesh(
        const wchar_t* pPath,
        const Mat4& matWorld,
        const std::atomic_bool* pCancel = nullptr);

    bool_t IsReady() const { return m_bReady; }
    bool_t SampleHeight(f32_t x, f32_t z, f32_t& outHeight) const;
    bool_t SampleSurface(f32_t x, f32_t z, MapSurfaceSample& outSample) const;
    bool_t HasWallBlockerNear(f32_t x, f32_t z, f32_t radius) const;

    f32_t GetMinX() const { return m_fMinX; }
    f32_t GetMinZ() const { return m_fMinZ; }
    f32_t GetMaxX() const { return m_fMaxX; }
    f32_t GetMaxZ() const { return m_fMaxZ; }

private:
    static constexpr i32_t kGridDim = 512;
    static constexpr f32_t kInvalidHeight = -3.402823466e+38f;

    struct SurfaceCell
    {
        f32_t height = kInvalidHeight;
        f32_t normalY = 0.f;
        bool_t bWallBlocker = false;
    };

    void Reset();
    bool_t BuildWorldVertices(
        const Winters::Asset::WMeshLoaded& mesh,
        const Mat4& matWorld,
        std::vector<Vec3>& outVertices,
        const std::atomic_bool* pCancel);
    bool_t HasValidBounds() const;
    bool_t BuildSurfaceCells(
        const Winters::Asset::WMeshLoaded& mesh,
        const std::vector<Vec3>& vertices,
        const std::atomic_bool* pCancel);
    void LogLoadedSurface() const;

    static u32_t ReadIndex(
        const Winters::Asset::WMeshLoaded& mesh,
        u32_t byteOffset,
        u32_t ordinal);

    i32_t ToGridX(f32_t x) const;
    i32_t ToGridZ(f32_t z) const;
    i32_t ClampGrid(i32_t v) const;
    f32_t GridToX(i32_t ix) const;
    f32_t GridToZ(i32_t iz) const;
    bool_t IsValidHeight(f32_t h) const;
    bool_t IsValidCell(const SurfaceCell& cell) const;
    bool_t SelectBetterSurface(const SurfaceCell& cell, f32_t height, f32_t normalY) const;

    void RasterizeTriangle(const Vec3& a, const Vec3& b, const Vec3& c);
    bool_t ReadCell(i32_t ix, i32_t iz, SurfaceCell& outCell) const;

    void RasterizeWallBlocker(const Vec3& a, const Vec3& b, const Vec3& c);
    void MarkWallBlockerCell(i32_t ix, i32_t iz);

    bool_t ReadNearestCell(i32_t ix, i32_t iz, SurfaceCell& outCell) const;

    bool_t m_bReady = false;
    std::vector<SurfaceCell> m_vecCells{};
    f32_t m_fMinX = 0.f;
    f32_t m_fMinZ = 0.f;
    f32_t m_fMaxX = 0.f;
    f32_t m_fMaxZ = 0.f;
    f32_t m_fStepX = 1.f;
    f32_t m_fStepZ = 1.f;
};

NS_END
