#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <vector>
#include <memory>

NS_BEGIN(Engine)

// CNavGrid is the game's simple 2D walkability map.
// - 512x512 cells, 0.5 world units per cell.
// - One bit per cell: 1 = walkable, 0 = blocked.
// - Origin is the world-space lower-left corner of cell (0,0).

class WINTERS_ENGINE CNavGrid final
{
public:
    static constexpr u32_t kCellCountX = 512;
    static constexpr u32_t kCellCountY = 512;
    static constexpr f32_t    kCellSize = 0.5f;
    static constexpr u32_t kTotalCells = kCellCountX * kCellCountY;
    static constexpr u32_t kByteSize = (kTotalCells + 7) / 8;

    struct Cell { int32_t x, y; };

    ~CNavGrid() = default;

    static std::unique_ptr<CNavGrid> Create(f32_t fOriginX, f32_t fOriginZ);

    bool_t IsWalkable(int32_t iCellX, int32_t iCellY) const;
    void   SetWalkable(int32_t iCellX, int32_t iCellY, bool_t bWalkable);
    void   SetAllWalkable(bool_t bWalkable);

    Cell   WorldToCell(const Vec3& vWorld) const;
    Vec3   CellToWorld(int32_t iCellX, int32_t iCellY) const;

    bool_t IsInBounds(int32_t iCellX, int32_t iCellY) const;
    bool_t TryFindNearestWalkableCell(Cell from, int32_t maxRadius, Cell& outCell) const;
    bool_t IsAreaWalkable(const Vec3& center, f32_t radiusWorld = 0.f) const;
    bool_t SegmentWalkable(const Vec3& from, const Vec3& to, f32_t radiusWorld = 0.f) const;
    bool_t IsCellWalkableForRadius(Cell cell, f32_t radiusWorld) const;
    bool_t LineCellsWalkableForRadius(Cell from, Cell to, f32_t radiusWorld) const;
    uint32_t CountWalkableCells() const;
    uint32_t ComputeContentHash() const;
    std::unique_ptr<CNavGrid> BuildInflated(f32_t radiusWorld) const;

    f32_t        Get_OriginX() const { return m_fOriginX; }
    f32_t        Get_OriginZ() const { return m_fOriginZ; }
    const uint8_t* Get_Bits() const { return m_vecBits.data(); }
    void         Load_Bits(const uint8_t* pData, size_t iSize);

    static bool_t SaveToFile(const wchar_t* pFilePath, const CNavGrid& navGrid);
    static std::unique_ptr<CNavGrid> LoadFromFile(const wchar_t* pFilePath);

private:
    CNavGrid() = default;

    f32_t                m_fOriginX = 0.f;
    f32_t                m_fOriginZ = 0.f;
    std::vector<uint8_t> m_vecBits;   // kByteSize
};

NS_END
