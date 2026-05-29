#include "Manager/Navigation/Pathfinder.h"
#include "ProfilerAPI.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <queue>

namespace
{
    struct Node
    {
        int32_t cx, cy;
        f32_t   fG, fF;
        bool    operator>(const Node& o) const { return fF > o.fF; }
    };

    constexpr f32_t kSqrt2 = 1.41421356237f;
    constexpr f32_t kSqrt2m1 = kSqrt2 - 1.f;

    // 8-neighbor movement offsets.
    constexpr int32_t kDX[8] = { -1,  0,  1, -1,  1, -1,  0,  1 };
    constexpr int32_t kDY[8] = { -1, -1, -1,  0,  0,  1,  1,  1 };

    inline uint32_t CellIdx(int32_t cx, int32_t cy)
    {
        return static_cast<uint32_t>(cy) * CNavGrid::kCellCountX
            + static_cast<uint32_t>(cx);
    }

    struct BfsNode
    {
        CNavGrid::Cell cell{};
        int32_t depth = 0;
    };

    bool_t CanStepToNeighbor(
        const CNavGrid* pGrid,
        CNavGrid::Cell from,
        int32_t dx, int32_t dy)
    {
        const int32_t nx = from.x + dx;
        const int32_t ny = from.y + dy;

        if (!pGrid->IsWalkable(nx, ny))
            return false;

        if (dx != 0 && dy != 0)
        {
            if (!pGrid->IsWalkable(from.x + dx, from.y) ||
                !pGrid->IsWalkable(from.x, from.y + dy))
            {
                return false;
            }
        }

        return true;
    }

    bool_t CanStepToNeighborForRadius(
        const CNavGrid* pGrid,
        CNavGrid::Cell from,
        int32_t dx, int32_t dy,
        f32_t radiusWorld)
    {
        const CNavGrid::Cell next{ from.x + dx, from.y + dy };

        if (!pGrid->IsCellWalkableForRadius(next, radiusWorld))
            return false;

        if (dx != 0 && dy != 0)
        {
            if (!pGrid->IsCellWalkableForRadius({ from.x + dx, from.y }, radiusWorld) ||
                !pGrid->IsCellWalkableForRadius({ from.x, from.y + dy }, radiusWorld))
            {
                return false;
            }
        }

        return true;
    }
}

f32_t CPathfinder::Octile(int32_t dx, int32_t dy)
{
    const int32_t ax = std::abs(dx);
    const int32_t ay = std::abs(dy);
    return static_cast<f32_t>(std::max(ax, ay))
        + kSqrt2m1 * static_cast<f32_t>(std::min(ax, ay));
}

std::vector<CNavGrid::Cell> CPathfinder::Find_Path(
    const CNavGrid* pGrid, CNavGrid::Cell start, CNavGrid::Cell goal)
{
    WINTERS_PROFILE_SCOPE("AStar::FindPath");

    std::vector<CNavGrid::Cell> emptyPath;
    if (!pGrid || !pGrid->IsWalkable(start.x, start.y) ||
        !pGrid->IsWalkable(goal.x, goal.y))
        return emptyPath;

    // Flush visited-node profiling on every return path.
    u32_t nodesVisited = 0;
    struct CounterFlush
    {
        const u32_t& count;
        ~CounterFlush()
        {
            WINTERS_PROFILE_COUNT("AStar::NodesVisited",
                static_cast<i32_t>(count));
        }
    }guard{nodesVisited};

    constexpr uint32_t kN = CNavGrid::kTotalCells;

    thread_local std::vector<f32_t>   tls_gScore(kN);
    thread_local std::vector<int32_t> tls_parent(kN);
    thread_local std::vector<uint8_t> tls_closed(kN);

    std::fill(tls_gScore.begin(), tls_gScore.end(), std::numeric_limits<f32_t>::infinity());
    std::fill(tls_parent.begin(), tls_parent.end(), -1);
    std::fill(tls_closed.begin(), tls_closed.end(), 0);

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    tls_gScore[CellIdx(start.x, start.y)] = 0.f;
    open.push({ start.x, start.y, 0.f, Octile(goal.x - start.x, goal.y - start.y) });

    while (!open.empty())
    {
        const Node cur = open.top(); open.pop();
        const uint32_t iCur = CellIdx(cur.cx, cur.cy);
        if (tls_closed[iCur]) continue;
        tls_closed[iCur] = 1;

        // Count cells touched by this A* search for profiling/debug overlays.
        ++nodesVisited;

        if (cur.cx == goal.x && cur.cy == goal.y) break;

        for (int32_t k = 0; k < 8; ++k)
        {
            const int32_t dx = kDX[k];
            const int32_t dy = kDY[k];
            const int32_t nx = cur.cx + dx;
            const int32_t ny = cur.cy + dy;

            if (!CanStepToNeighbor(pGrid, { cur.cx, cur.cy }, dx, dy))
                continue;

            const f32_t fStep = (dx != 0 && dy != 0) ? kSqrt2 : 1.f;

            const f32_t fTentG = cur.fG + fStep;
            const uint32_t iN = CellIdx(nx, ny);

            if (fTentG < tls_gScore[iN])
            {
                tls_gScore[iN] = fTentG;
                tls_parent[iN] = static_cast<int32_t>(iCur);
                const f32_t fH = Octile(goal.x - nx, goal.y - ny);
                open.push({ nx, ny, fTentG, fTentG + fH });
            }
        }
    }

    // Rebuild the path by walking parents from goal back to start.
    std::vector<CNavGrid::Cell> path;
    const uint32_t iGoal = CellIdx(goal.x, goal.y);
    if (tls_parent[iGoal] == -1 && !(goal.x == start.x && goal.y == start.y))
        return emptyPath;

    uint32_t iCur = iGoal;
    while (true)
    {
        const int32_t cx = static_cast<int32_t>(iCur % CNavGrid::kCellCountX);
        const int32_t cy = static_cast<int32_t>(iCur / CNavGrid::kCellCountX);
        path.push_back({ cx, cy });
        if (cx == start.x && cy == start.y) break;
        const int32_t iP = tls_parent[iCur];
        if (iP < 0) { path.clear(); return emptyPath; }
        iCur = static_cast<uint32_t>(iP);
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<CNavGrid::Cell> CPathfinder::FindPathForRadius(
    const CNavGrid* pGrid,
    CNavGrid::Cell start,
    CNavGrid::Cell goal,
    f32_t radiusWorld)
{
    WINTERS_PROFILE_SCOPE("AStar::FindPathForRadius");

    std::vector<CNavGrid::Cell> emptyPath;
    if (!pGrid)
        return emptyPath;

    if (!pGrid->IsCellWalkableForRadius(start, radiusWorld))
        return emptyPath;
    if (!pGrid->IsCellWalkableForRadius(goal, radiusWorld))
        return emptyPath;

    u32_t nodesVisited = 0;
    struct CounterFlush
    {
        const u32_t& count;
        ~CounterFlush()
        {
            WINTERS_PROFILE_COUNT("AStarRadius::NodesVisited",
                static_cast<i32_t>(count));
        }
    } guard{ nodesVisited };

    constexpr uint32_t kN = CNavGrid::kTotalCells;

    thread_local std::vector<f32_t> tls_gScore(kN);
    thread_local std::vector<int32_t> tls_parent(kN);
    thread_local std::vector<uint8_t> tls_closed(kN);

    std::fill(tls_gScore.begin(), tls_gScore.end(), std::numeric_limits<f32_t>::infinity());
    std::fill(tls_parent.begin(), tls_parent.end(), -1);
    std::fill(tls_closed.begin(), tls_closed.end(), 0);

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

    tls_gScore[CellIdx(start.x, start.y)] = 0.f;
    open.push({ start.x, start.y, 0.f, Octile(goal.x - start.x, goal.y - start.y) });

    while (!open.empty())
    {
        const Node cur = open.top();
        open.pop();

        const uint32_t iCur = CellIdx(cur.cx, cur.cy);
        if (tls_closed[iCur])
            continue;

        tls_closed[iCur] = 1;
        ++nodesVisited;

        if (cur.cx == goal.x && cur.cy == goal.y)
            break;

        for (int32_t k = 0; k < 8; ++k)
        {
            const int32_t dx = kDX[k];
            const int32_t dy = kDY[k];
            const int32_t nx = cur.cx + dx;
            const int32_t ny = cur.cy + dy;

            if (!CanStepToNeighborForRadius(pGrid, { cur.cx, cur.cy }, dx, dy, radiusWorld))
                continue;

            const f32_t fStep = (dx != 0 && dy != 0) ? kSqrt2 : 1.f;
            const f32_t fTentG = cur.fG + fStep;
            const uint32_t iN = CellIdx(nx, ny);

            if (fTentG < tls_gScore[iN])
            {
                tls_gScore[iN] = fTentG;
                tls_parent[iN] = static_cast<int32_t>(iCur);
                const f32_t fH = Octile(goal.x - nx, goal.y - ny);
                open.push({ nx, ny, fTentG, fTentG + fH });
            }
        }
    }

    std::vector<CNavGrid::Cell> path;
    const uint32_t iGoal = CellIdx(goal.x, goal.y);
    if (tls_parent[iGoal] == -1 && !(goal.x == start.x && goal.y == start.y))
        return emptyPath;

    uint32_t iCur = iGoal;
    while (true)
    {
        const int32_t cx = static_cast<int32_t>(iCur % CNavGrid::kCellCountX);
        const int32_t cy = static_cast<int32_t>(iCur / CNavGrid::kCellCountX);
        path.push_back({ cx, cy });
        if (cx == start.x && cy == start.y)
            break;

        const int32_t iP = tls_parent[iCur];
        if (iP < 0)
        {
            path.clear();
            return emptyPath;
        }
        iCur = static_cast<uint32_t>(iP);
    }

    std::reverse(path.begin(), path.end());
    return path;
}

bool_t CPathfinder::TryFindNearestReachableGoal(
    const CNavGrid* pGrid,
    CNavGrid::Cell start,
    CNavGrid::Cell rawGoal,
    int32_t maxRadius,
    CNavGrid::Cell& outGoal,
    std::vector<CNavGrid::Cell>* pOutPath)
{
    if (!pGrid || !pGrid->IsWalkable(start.x, start.y))
        return false;

    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    auto Accept = [&](CNavGrid::Cell cell, std::vector<CNavGrid::Cell>& outPath) -> bool_t
        {
            if (!pGrid->IsWalkable(cell.x, cell.y))
                return false;

            outPath = Find_Path(pGrid, start, cell);
            return !outPath.empty();
        };

    std::vector<uint8_t> visited(CNavGrid::kTotalCells, 0);
    std::queue<BfsNode> open{};

    visited[CellIdx(rawGoal.x, rawGoal.y)] = 1;
    open.push({ rawGoal, 0 });

    int32_t currentDepth = 0;
    bool_t bFoundAtDepth = false;
    CNavGrid::Cell bestCell{};
    std::vector<CNavGrid::Cell> bestPath{};
    size_t bestPathLen = (std::numeric_limits<size_t>::max)();

    while (!open.empty())
    {
        const BfsNode node = open.front();
        open.pop();

        if (node.depth > maxRadius)
            break;
        if (node.depth != currentDepth)
        {
            if (bFoundAtDepth)
                break;
            currentDepth = node.depth;
        }

        std::vector<CNavGrid::Cell> candidatePath{};
        if (Accept(node.cell, candidatePath))
        {
            if (!bFoundAtDepth || candidatePath.size() < bestPathLen)
            {
                bFoundAtDepth = true;
                bestCell = node.cell;
                bestPath = std::move(candidatePath);
                bestPathLen = bestPath.size();
            }
            continue;
        }
        for (int32_t k = 0; k < 8; ++k)
        {
            const CNavGrid::Cell next{
                node.cell.x + kDX[k],
                node.cell.y + kDY[k]
            };
            if (!pGrid->IsInBounds(next.x, next.y))
                continue;

            const uint32_t nextIdx = CellIdx(next.x, next.y);
            if (visited[nextIdx])
                continue;
            visited[nextIdx] = 1;
            open.push({ next, node.depth + 1 });
        }
    }

    if (!bFoundAtDepth)
        return false;

    outGoal = bestCell;
    if (pOutPath)
        *pOutPath = bestPath;

    return true;
}

bool_t CPathfinder::TryFindNearestReachableGoalForRadius(
    const CNavGrid* pGrid,
    CNavGrid::Cell start,
    CNavGrid::Cell rawGoal,
    int32_t maxRadius,
    f32_t radiusWorld,
    CNavGrid::Cell& outGoal,
    std::vector<CNavGrid::Cell>* pOutPath)
{
    if (!pGrid || !pGrid->IsCellWalkableForRadius(start, radiusWorld))
        return false;

    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return false;

    auto Accept = [&](CNavGrid::Cell cell, std::vector<CNavGrid::Cell>& outPath) -> bool_t
        {
            if (!pGrid->IsCellWalkableForRadius(cell, radiusWorld))
                return false;

            outPath = FindPathForRadius(pGrid, start, cell, radiusWorld);
            return !outPath.empty();
        };

    std::vector<uint8_t> visited(CNavGrid::kTotalCells, 0);
    std::queue<BfsNode> open{};

    visited[CellIdx(rawGoal.x, rawGoal.y)] = 1;
    open.push({ rawGoal, 0 });

    int32_t currentDepth = 0;
    bool_t bFoundAtDepth = false;
    CNavGrid::Cell bestCell{};
    std::vector<CNavGrid::Cell> bestPath{};
    size_t bestPathLen = (std::numeric_limits<size_t>::max)();

    while (!open.empty())
    {
        const BfsNode node = open.front();
        open.pop();

        if (node.depth > maxRadius)
            break;
        if (node.depth != currentDepth)
        {
            if (bFoundAtDepth)
                break;
            currentDepth = node.depth;
        }

        std::vector<CNavGrid::Cell> candidatePath{};
        if (Accept(node.cell, candidatePath))
        {
            if (!bFoundAtDepth || candidatePath.size() < bestPathLen)
            {
                bFoundAtDepth = true;
                bestCell = node.cell;
                bestPath = std::move(candidatePath);
                bestPathLen = bestPath.size();
            }
            continue;
        }

        for (int32_t k = 0; k < 8; ++k)
        {
            const CNavGrid::Cell next{
                node.cell.x + kDX[k],
                node.cell.y + kDY[k]
            };
            if (!pGrid->IsInBounds(next.x, next.y))
                continue;

            const uint32_t nextIdx = CellIdx(next.x, next.y);
            if (visited[nextIdx])
                continue;

            visited[nextIdx] = 1;
            open.push({ next, node.depth + 1 });
        }
    }

    if (!bFoundAtDepth)
        return false;

    outGoal = bestCell;
    if (pOutPath)
        *pOutPath = bestPath;

    return true;
}
