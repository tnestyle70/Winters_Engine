#include "Manager/Navigation/NavGrid.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

NS_BEGIN(Engine)

namespace
{
    constexpr u32_t NAVGRID_MAGIC = 0x47564E57u; //WNVG
    constexpr u32_t NAVGRID_VERSION = 1;

    uint64_t AllocateNavGridCacheId()
    {
        static std::atomic<uint64_t> sNextId{ 1ull };
        return sNextId.fetch_add(1ull, std::memory_order_relaxed);
    }

    struct NavGridFileHeader
    {
        u32_t magic = NAVGRID_MAGIC;
        u32_t version = NAVGRID_VERSION;
        f32_t originX = 0.f;
        f32_t originZ = 0.f;
        u32_t byteSize = Engine::CNavGrid::kByteSize;
    };

    bool_t CircleOverlapsAabbXZ(
        const Vec3& center,
        f32_t radius,
        f32_t minX,
        f32_t minZ,
        f32_t maxX,
        f32_t maxZ)
    {
        const f32_t closestX = (std::max)(minX, (std::min)(center.x, maxX));
        const f32_t closestZ = (std::max)(minZ, (std::min)(center.z, maxZ));
        const f32_t dx = center.x - closestX;
        const f32_t dz = center.z - closestZ;
        return (dx * dx + dz * dz) <= radius * radius;
    }

    bool_t SegmentIntersectsAabbXZ(
        const Vec3& from,
        const Vec3& to,
        f32_t minX,
        f32_t minZ,
        f32_t maxX,
        f32_t maxZ)
    {
        f32_t tMin = 0.f;
        f32_t tMax = 1.f;

        const auto ClipAxis = [&](f32_t start, f32_t delta, f32_t minValue, f32_t maxValue) -> bool_t
            {
                if (std::fabs(delta) < 0.000001f)
                    return start >= minValue && start <= maxValue;

                f32_t t1 = (minValue - start) / delta;
                f32_t t2 = (maxValue - start) / delta;
                if (t1 > t2)
                    std::swap(t1, t2);

                tMin = (std::max)(tMin, t1);
                tMax = (std::min)(tMax, t2);
                return tMin <= tMax;
            };

        return ClipAxis(from.x, to.x - from.x, minX, maxX) &&
            ClipAxis(from.z, to.z - from.z, minZ, maxZ);
    }
}

std::unique_ptr<CNavGrid> CNavGrid::Create(f32_t fOriginX, f32_t fOriginZ)
{
    auto pInstance = std::unique_ptr<CNavGrid>(new CNavGrid());
    pInstance->m_uCacheId = AllocateNavGridCacheId();
    pInstance->m_fOriginX = fOriginX;
    pInstance->m_fOriginZ = fOriginZ;
    pInstance->m_vecBits.assign(kByteSize, 0xFF);   // Default walkable; blockers carve cells out.
    return pInstance;
}

void CNavGrid::BumpRevision()
{
    ++m_uRevision;
    if (m_uRevision == 0u)
        m_uRevision = 1u;
}

bool_t CNavGrid::IsWalkable(int32_t iCellX, int32_t iCellY) const
{
    if (!IsInBounds(iCellX, iCellY)) return false;
    const uint32_t iIdx = static_cast<uint32_t>(iCellY) * kCellCountX
        + static_cast<uint32_t>(iCellX);
    return (m_vecBits[iIdx >> 3] >> (iIdx & 7)) & 1;
}

void CNavGrid::SetWalkable(int32_t iCellX, int32_t iCellY, bool_t bWalkable)
{
    if (!IsInBounds(iCellX, iCellY)) return;
    const uint32_t iIdx = static_cast<uint32_t>(iCellY) * kCellCountX
        + static_cast<uint32_t>(iCellX);
    const uint8_t  iMask = static_cast<uint8_t>(1 << (iIdx & 7));

    const bool_t bWasWalkable = (m_vecBits[iIdx >> 3] & iMask) != 0;
    if (bWasWalkable == bWalkable)
        return;

    if (bWalkable) m_vecBits[iIdx >> 3] |= iMask;
    else           m_vecBits[iIdx >> 3] &= ~iMask;

    BumpRevision();
}

void CNavGrid::SetAllWalkable(bool_t bWalkable)
{
    m_vecBits.assign(kByteSize, bWalkable ? 0xFF : 0x00);
    if (bWalkable && (kTotalCells & 7u) != 0u)
    {
        const uint32_t validBits = kTotalCells & 7u;
        const uint8_t mask = static_cast<uint8_t>((1u << validBits) - 1u);
        m_vecBits.back() &= mask;
    }

    BumpRevision();
}

CNavGrid::Cell CNavGrid::WorldToCell(const Vec3& vWorld) const
{
    const f32_t fLocalX = vWorld.x - m_fOriginX;
    const f32_t fLocalZ = vWorld.z - m_fOriginZ;
    return { static_cast<int32_t>(std::floor(fLocalX / kCellSize)),
             static_cast<int32_t>(std::floor(fLocalZ / kCellSize)) };
}

Vec3 CNavGrid::CellToWorld(int32_t iCellX, int32_t iCellY) const
{
    return { m_fOriginX + (iCellX + 0.5f) * kCellSize,
             0.f,
             m_fOriginZ + (iCellY + 0.5f) * kCellSize };
}

bool_t CNavGrid::IsInBounds(int32_t iCellX, int32_t iCellY) const
{
    return iCellX >= 0 && iCellX < static_cast<int32_t>(kCellCountX)
        && iCellY >= 0 && iCellY < static_cast<int32_t>(kCellCountY);
}

bool_t CNavGrid::TryFindNearestWalkableCell(Cell from, int32_t maxRadius, Cell& outCell) const
{
    for (int32_t r = 0; r <= maxRadius; ++r)
    {
        for (int32_t dy = -r; dy <= r; ++dy)
        {
            for (int32_t dx = -r; dx <= r; ++dx)
            {
                if (std::abs(dx) != r && std::abs(dy) != r)
                    continue;

                const int32_t x = from.x + dx;
                const int32_t y = from.y + dy;
                if (!IsWalkable(x, y))
                    continue;

                outCell = { x, y };
                return true;
            }
        }
    }

    return false;
}

bool_t CNavGrid::IsAreaWalkable(const Vec3& center, f32_t radiusWorld) const
{
    const f32_t radius = (std::max)(0.f, radiusWorld);
    const Cell minCell = WorldToCell({ center.x - radius, center.y, center.z - radius });
    const Cell maxCell = WorldToCell({ center.x + radius, center.y, center.z + radius });

    for (int32_t y = minCell.y; y <= maxCell.y; ++y)
    {
        for (int32_t x = minCell.x; x <= maxCell.x; ++x)
        {
            if (!IsInBounds(x, y))
                return false;

            if (IsWalkable(x, y))
                continue;

            const f32_t minX = m_fOriginX + static_cast<f32_t>(x) * kCellSize;
            const f32_t minZ = m_fOriginZ + static_cast<f32_t>(y) * kCellSize;
            const f32_t maxX = minX + kCellSize;
            const f32_t maxZ = minZ + kCellSize;

            if (radius <= 0.f || CircleOverlapsAabbXZ(center, radius, minX, minZ, maxX, maxZ))
                return false;
        }
    }

    return true;
}

bool_t CNavGrid::SegmentWalkable(const Vec3& from, const Vec3& to, f32_t radiusWorld) const
{
    const f32_t radius = (std::max)(0.f, radiusWorld);

    if (!IsAreaWalkable(from, radius) || !IsAreaWalkable(to, radius))
        return false;

    const f32_t minWorldX = (std::min)(from.x, to.x) - radius;
    const f32_t maxWorldX = (std::max)(from.x, to.x) + radius;
    const f32_t minWorldZ = (std::min)(from.z, to.z) - radius;
    const f32_t maxWorldZ = (std::max)(from.z, to.z) + radius;

    const Cell minCell = WorldToCell({ minWorldX, from.y, minWorldZ });
    const Cell maxCell = WorldToCell({ maxWorldX, from.y, maxWorldZ });

    for (int32_t y = minCell.y; y <= maxCell.y; ++y)
    {
        for (int32_t x = minCell.x; x <= maxCell.x; ++x)
        {
            if (!IsInBounds(x, y))
                return false;

            if (IsWalkable(x, y))
                continue;

            const f32_t cellMinX = m_fOriginX + static_cast<f32_t>(x) * kCellSize - radius;
            const f32_t cellMinZ = m_fOriginZ + static_cast<f32_t>(y) * kCellSize - radius;
            const f32_t cellMaxX = cellMinX + kCellSize + radius * 2.f;
            const f32_t cellMaxZ = cellMinZ + kCellSize + radius * 2.f;

            if (SegmentIntersectsAabbXZ(from, to, cellMinX, cellMinZ, cellMaxX, cellMaxZ))
            {
                static u32_t sSegmentBlockedLogCount = 0;
                if (sSegmentBlockedLogCount < 64u)
                {
                    char msg[256]{};
                    sprintf_s(msg,
                        "[NavGrid] segment blocked cell=(%d,%d) radius=%.2f from=(%.2f,%.2f) to=(%.2f,%.2f) origin=(%.2f,%.2f)\n",
                        x,
                        y,
                        radius,
                        from.x,
                        from.z,
                        to.x,
                        to.z,
                        m_fOriginX,
                        m_fOriginZ);
                    OutputDebugStringA(msg);
                    ++sSegmentBlockedLogCount;
                }
                return false;
            }
        }
    }

    return true;
}

bool_t CNavGrid::IsCellWalkableForRadius(Cell cell, f32_t radiusWorld) const
{
    if (!IsInBounds(cell.x, cell.y))
        return false;

    return IsAreaWalkable(CellToWorld(cell.x, cell.y), radiusWorld);
}

bool_t CNavGrid::LineCellsWalkableForRadius(Cell from, Cell to, f32_t radiusWorld) const
{
    if (!IsInBounds(from.x, from.y) || !IsInBounds(to.x, to.y))
        return false;

    return SegmentWalkable(
        CellToWorld(from.x, from.y),
        CellToWorld(to.x, to.y),
        radiusWorld);
}

uint32_t CNavGrid::CountWalkableCells() const
{
    uint32_t total = 0;
    for (uint32_t y = 0; y < kCellCountY; ++y)
    {
        for (uint32_t x = 0; x < kCellCountX; ++x)
        {
            if (IsWalkable(static_cast<int32_t>(x), static_cast<int32_t>(y)))
                ++total;
        }
    }
    return total;
}

uint32_t CNavGrid::ComputeContentHash() const
{
    uint32_t hash = 2166136261u;
    auto AddByte = [&](uint8_t value)
        {
            hash ^= value;
            hash *= 16777619u;
        };

    const uint8_t* pOriginX = reinterpret_cast<const uint8_t*>(&m_fOriginX);
    const uint8_t* pOriginZ = reinterpret_cast<const uint8_t*>(&m_fOriginZ);
    for (size_t i = 0; i < sizeof(m_fOriginX); ++i)
        AddByte(pOriginX[i]);
    for (size_t i = 0; i < sizeof(m_fOriginZ); ++i)
        AddByte(pOriginZ[i]);
    for (uint8_t value : m_vecBits)
        AddByte(value);

    return hash;
}

std::unique_ptr<CNavGrid> CNavGrid::BuildInflated(f32_t radiusWorld) const
{
    auto pInflated = CNavGrid::Create(m_fOriginX, m_fOriginZ);
    if (!pInflated)
        return nullptr;

    const f32_t radius = (std::max)(0.f, radiusWorld);
    for (uint32_t y = 0; y < kCellCountY; ++y)
    {
        for (uint32_t x = 0; x < kCellCountX; ++x)
        {
            const Cell cell{ static_cast<int32_t>(x), static_cast<int32_t>(y) };
            pInflated->SetWalkable(cell.x, cell.y, IsAreaWalkable(CellToWorld(cell.x, cell.y), radius));
        }
    }

    return pInflated;
}

void CNavGrid::Load_Bits(const uint8_t* pData, size_t iSize)
{
    if (iSize != kByteSize) return;
    std::memcpy(m_vecBits.data(), pData, kByteSize);
    BumpRevision();
}

bool_t CNavGrid::SaveToFile(const wchar_t* pFilePath, const CNavGrid& navGrid)
{
    if (!pFilePath)
        return false;

    FILE* pFile = nullptr;

    if (_wfopen_s(&pFile, pFilePath, L"wb") != 0 || !pFile)
        return false;

    NavGridFileHeader header{};
    header.originX = navGrid.Get_OriginX();
    header.originZ = navGrid.Get_OriginZ();

    const bool_t ok = fwrite(&header, sizeof(header), 1, pFile) == 1 &&
        fwrite(navGrid.Get_Bits(), 1, CNavGrid::kByteSize, pFile) ==
        CNavGrid::kByteSize;

    fclose(pFile);

    return ok;
}

std::unique_ptr<CNavGrid> CNavGrid::LoadFromFile(const wchar_t* pFilePath)
{
    if (!pFilePath)
        return nullptr;

    FILE* pFile = nullptr;
    if (_wfopen_s(&pFile, pFilePath, L"rb") != 0 || !pFile)
        return nullptr;

    NavGridFileHeader header{};
    if (fread(&header, sizeof(header), 1, pFile) != 1 ||
        header.magic != NAVGRID_MAGIC ||
        header.version != NAVGRID_VERSION ||
        header.byteSize != CNavGrid::kByteSize)
    {
        fclose(pFile);
        return nullptr;
    }

    std::vector<uint8_t> bits(CNavGrid::kByteSize);
    if (fread(bits.data(), 1, bits.size(), pFile) != bits.size())
    {
        fclose(pFile);
        return nullptr;
    }

    fclose(pFile);

    auto navGrid = CNavGrid::Create(header.originX, header.originZ);
    navGrid->Load_Bits(bits.data(), bits.size());
    return navGrid;
}

NS_END
