#include "Manager/Navigation/MapWalkableBaker.h"

#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/NavGrid.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <queue>
#include <vector>

NS_BEGIN(Engine)

namespace
{
    inline u32_t CellIndex(int32_t x, int32_t y)
    {
        return static_cast<u32_t>(y) * CNavGrid::kCellCountX +
            static_cast<u32_t>(x);
    }

    bool_t FindNearestCandidateCell(
        const CNavGrid& grid,
        const std::vector<uint8_t>& candidates,
        CNavGrid::Cell origin,
        int32_t maxRadius,
        CNavGrid::Cell& outCell)
    {
        for (int32_t r = 0; r <= maxRadius; ++r)
        {
            for (int32_t dy = -r; dy <= r; ++dy)
            {
                for (int32_t dx = -r; dx <= r; ++dx)
                {
                    if (std::abs(dx) != r && std::abs(dy) != r)
                        continue;

                    const int32_t x = origin.x + dx;
                    const int32_t y = origin.y + dy;
                    if (!grid.IsInBounds(x, y))
                        continue;

                    if (candidates[CellIndex(x, y)] == 0)
                        continue;

                    outCell = { x, y };
                    return true;
                }
            }
        }

        return false;
    }

    bool_t HasStableNeighbors(
        const CMapSurfaceSampler& surface,
        const CNavGrid& grid,
        int32_t x,
        int32_t y,
        f32_t height,
        const MapWalkableBakeDesc& desc)
    {
        static constexpr int32_t kDx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
        static constexpr int32_t kDy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

        int32_t validNeighbors = 0;
        int32_t validCardinalNeighbors = 0;
        for (int32_t i = 0; i < 8; ++i)
        {
            const int32_t nx = x + kDx[i];
            const int32_t ny = y + kDy[i];
            if (!grid.IsInBounds(nx, ny))
                return false;

            const bool_t bCardinal = (kDx[i] == 0 || kDy[i] == 0);

            const Vec3 wp = grid.CellToWorld(nx, ny);
            MapSurfaceSample neighbor{};
            if (!surface.SampleSurface(wp.x, wp.z, neighbor))
            {
                if (bCardinal)
                    return false;
                continue;
            }

            if (std::fabs(neighbor.height - height) > desc.maxStepHeight)
                return false;

            ++validNeighbors;
            if (bCardinal)
                ++validCardinalNeighbors;
        }

        return validCardinalNeighbors == 4 && validNeighbors >= 6;
    }

    bool_t HasAgentClearance(
        const std::vector<uint8_t>& connected,
        int32_t x,
        int32_t y,
        int32_t radiusCells)
    {
        if (radiusCells <= 0)
            return connected[CellIndex(x, y)] != 0;

        for (int32_t dy = -radiusCells; dy <= radiusCells; ++dy)
        {
            for (int32_t dx = -radiusCells; dx <= radiusCells; ++dx)
            {
                if ((dx * dx) + (dy * dy) > radiusCells * radiusCells)
                    continue;

                const int32_t nx = x + dx;
                const int32_t ny = y + dy;
                if (nx < 0 || ny < 0 ||
                    nx >= static_cast<int32_t>(CNavGrid::kCellCountX) ||
                    ny >= static_cast<int32_t>(CNavGrid::kCellCountY))
                {
                    return false;
                }

                if (connected[CellIndex(nx, ny)] == 0)
                    return false;
            }
        }

        return true;
    }
}

bool_t CMapWalkableBaker::BakeIntoNavGrid(
    const CMapSurfaceSampler& surface,
    CNavGrid& navGrid,
    const std::vector<Vec3>& playableSeeds,
    const MapWalkableBakeDesc& desc)
{
    if (!surface.IsReady())
        return false;

    constexpr u32_t kTotal = CNavGrid::kTotalCells;
    std::vector<uint8_t> candidates(kTotal, 0);
    std::vector<uint8_t> connected(kTotal, 0);

    u32_t candidateCount = 0;
    for (int32_t y = 0; y < static_cast<int32_t>(CNavGrid::kCellCountY); ++y)
    {
        for (int32_t x = 0; x < static_cast<int32_t>(CNavGrid::kCellCountX); ++x)
        {
            const Vec3 wp = navGrid.CellToWorld(x, y);
            MapSurfaceSample sample{};
            if (!surface.SampleSurface(wp.x, wp.z, sample))
                continue;
            if (std::fabs(sample.height - desc.playableBaseY) > desc.playableHeightBand)
                continue;
            if (sample.normalY < desc.minNormalY)
                continue;
            if (sample.height > desc.maxWorldY)
                continue;
            if (!HasStableNeighbors(surface, navGrid, x, y, sample.height, desc))
                continue;

            candidates[CellIndex(x, y)] = 1;
            ++candidateCount;
        }
    }

    std::queue<CNavGrid::Cell> open;
    u32_t seedCount = 0;
    for (const Vec3& seed : playableSeeds)
    {
        CNavGrid::Cell cell = navGrid.WorldToCell(seed);
        if (!navGrid.IsInBounds(cell.x, cell.y) ||
            candidates[CellIndex(cell.x, cell.y)] == 0)
        {
            CNavGrid::Cell nearest{};
            if (!FindNearestCandidateCell(navGrid, candidates, cell, 24, nearest))
                continue;
            cell = nearest;
        }

        const u32_t idx = CellIndex(cell.x, cell.y);
        if (connected[idx] != 0)
            continue;

        connected[idx] = 1;
        open.push(cell);
        ++seedCount;
    }

    static constexpr int32_t kDx[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static constexpr int32_t kDy[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };
    while (!open.empty())
    {
        const CNavGrid::Cell cur = open.front();
        open.pop();

        for (int32_t i = 0; i < 8; ++i)
        {
            const int32_t nx = cur.x + kDx[i];
            const int32_t ny = cur.y + kDy[i];
            if (!navGrid.IsInBounds(nx, ny))
                continue;

            const u32_t idx = CellIndex(nx, ny);
            if (candidates[idx] == 0 || connected[idx] != 0)
                continue;

            connected[idx] = 1;
            open.push({ nx, ny });
        }
    }

    u32_t connectedCount = 0;
    for (uint8_t value : connected)
        connectedCount += value ? 1u : 0u;

    navGrid.SetAllWalkable(false);
    u32_t finalCount = 0;
    const bool_t bUseClearance = connectedCount >= 1000u;
    for (int32_t y = 0; y < static_cast<int32_t>(CNavGrid::kCellCountY); ++y)
    {
        for (int32_t x = 0; x < static_cast<int32_t>(CNavGrid::kCellCountX); ++x)
        {
            const bool_t bWalkable = bUseClearance
                ? HasAgentClearance(connected, x, y, desc.agentRadiusCells)
                : (connected[CellIndex(x, y)] != 0);
            if (!bWalkable)
                continue;

            navGrid.SetWalkable(x, y, true);
            ++finalCount;
        }
    }

    char msg[256]{};
    sprintf_s(
        msg,
        "[MapWalkable] candidates=%u connected=%u final=%u seeds=%u clearance=%d\n",
        candidateCount,
        connectedCount,
        finalCount,
        seedCount,
        bUseClearance ? desc.agentRadiusCells : 0);
    OutputDebugStringA(msg);

    return finalCount > 0u;
}

NS_END
