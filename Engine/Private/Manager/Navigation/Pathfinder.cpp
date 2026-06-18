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

    f32_t OctileDistance(int32_t dx, int32_t dy)
    {
        const int32_t ax = std::abs(dx);
        const int32_t ay = std::abs(dy);
        return static_cast<f32_t>((std::max)(ax, ay))
            + kSqrt2m1 * static_cast<f32_t>((std::min)(ax, ay));
    }

    using CellPredicate = bool_t(*)(const CNavGrid*, CNavGrid::Cell, f32_t);
    using StepPredicate = bool_t(*)(const CNavGrid*, CNavGrid::Cell, int32_t, int32_t, f32_t);

    bool_t IsWalkableCell(const CNavGrid* pGrid, CNavGrid::Cell cell, f32_t)
    {
        return pGrid->IsWalkable(cell.x, cell.y);
    }

    bool_t IsWalkableCellForRadius(const CNavGrid* pGrid, CNavGrid::Cell cell, f32_t radiusWorld)
    {
        return pGrid->IsCellWalkableForRadius(cell, radiusWorld);
    }

    bool_t CanStepToNeighborNoRadius(
        const CNavGrid* pGrid,
        CNavGrid::Cell from,
        int32_t dx,
        int32_t dy,
        f32_t)
    {
        return CanStepToNeighbor(pGrid, from, dx, dy);
    }

    bool_t CanStepToNeighborWithRadius(
        const CNavGrid* pGrid,
        CNavGrid::Cell from,
        int32_t dx,
        int32_t dy,
        f32_t radiusWorld)
    {
        return CanStepToNeighborForRadius(pGrid, from, dx, dy, radiusWorld);
    }

    uint32_t BeginSearchGeneration(std::vector<uint32_t>& generations, uint32_t& currentGeneration)
    {
        if (generations.size() != CNavGrid::kTotalCells)
            generations.assign(CNavGrid::kTotalCells, 0u);

        ++currentGeneration;
        if (currentGeneration == 0u)
        {
            std::fill(generations.begin(), generations.end(), 0u);
            currentGeneration = 1u;
        }

        return currentGeneration;
    }

    struct ReachabilityCache
    {
        const CNavGrid* pGrid = nullptr;
        uint64_t gridCacheId = 0ull;
        uint32_t revision = 0u;
        f32_t radiusWorld = 0.f;
        bool_t bUseRadius = false;
        std::vector<int32_t> components{};
        std::vector<CNavGrid::Cell> nearestCell{};
        std::vector<int32_t> nearestComponent{};
        std::vector<uint16_t> nearestDistance{};
    };

    void BuildReachabilityCache(
        ReachabilityCache& cache,
        const CNavGrid* pGrid,
        f32_t radiusWorld,
        bool_t bUseRadius,
        CellPredicate pCanUseCell,
        StepPredicate pCanStep)
    {
        cache.pGrid = pGrid;
        cache.gridCacheId = pGrid->GetCacheId();
        cache.revision = pGrid->GetRevision();
        cache.radiusWorld = radiusWorld;
        cache.bUseRadius = bUseRadius;
        cache.components.assign(CNavGrid::kTotalCells, -1);
        cache.nearestCell.assign(CNavGrid::kTotalCells, CNavGrid::Cell{ -1, -1 });
        cache.nearestComponent.assign(CNavGrid::kTotalCells, -1);
        cache.nearestDistance.assign(
            CNavGrid::kTotalCells,
            (std::numeric_limits<uint16_t>::max)());

        std::queue<CNavGrid::Cell> open{};

        int32_t nextComponent = 0;
        for (int32_t y = 0; y < static_cast<int32_t>(CNavGrid::kCellCountY); ++y)
        {
            for (int32_t x = 0; x < static_cast<int32_t>(CNavGrid::kCellCountX); ++x)
            {
                const CNavGrid::Cell start{ x, y };
                const uint32_t startIdx = CellIdx(x, y);

                if (cache.components[startIdx] >= 0)
                    continue;
                if (!pCanUseCell(pGrid, start, radiusWorld))
                    continue;

                cache.components[startIdx] = nextComponent;
                open.push(start);

                while (!open.empty())
                {
                    const CNavGrid::Cell cur = open.front();
                    open.pop();

                    for (int32_t k = 0; k < 8; ++k)
                    {
                        const CNavGrid::Cell next{ cur.x + kDX[k], cur.y + kDY[k] };
                        if (!pGrid->IsInBounds(next.x, next.y))
                            continue;

                        const uint32_t nextIdx = CellIdx(next.x, next.y);
                        if (cache.components[nextIdx] >= 0)
                            continue;

                        if (!pCanStep(pGrid, cur, kDX[k], kDY[k], radiusWorld))
                            continue;

                        cache.components[nextIdx] = nextComponent;
                        open.push(next);
                    }
                }

                ++nextComponent;
            }
        }

        std::queue<BfsNode> nearestOpen{};
        for (int32_t y = 0; y < static_cast<int32_t>(CNavGrid::kCellCountY); ++y)
        {
            for (int32_t x = 0; x < static_cast<int32_t>(CNavGrid::kCellCountX); ++x)
            {
                const uint32_t idx = CellIdx(x, y);
                if (cache.components[idx] < 0)
                    continue;

                const CNavGrid::Cell cell{ x, y };
                cache.nearestCell[idx] = cell;
                cache.nearestComponent[idx] = cache.components[idx];
                cache.nearestDistance[idx] = 0u;
                nearestOpen.push({ cell, 0 });
            }
        }

        while (!nearestOpen.empty())
        {
            const BfsNode node = nearestOpen.front();
            nearestOpen.pop();

            const uint32_t curIdx = CellIdx(node.cell.x, node.cell.y);
            for (int32_t k = 0; k < 8; ++k)
            {
                const CNavGrid::Cell next{ node.cell.x + kDX[k], node.cell.y + kDY[k] };
                if (!pGrid->IsInBounds(next.x, next.y))
                    continue;

                const uint32_t nextIdx = CellIdx(next.x, next.y);
                if (cache.nearestComponent[nextIdx] >= 0)
                    continue;

                cache.nearestCell[nextIdx] = cache.nearestCell[curIdx];
                cache.nearestComponent[nextIdx] = cache.nearestComponent[curIdx];
                cache.nearestDistance[nextIdx] = static_cast<uint16_t>(node.depth + 1);
                nearestOpen.push({ next, node.depth + 1 });
            }
        }
    }

    ReachabilityCache& EnsureReachabilityCache(
        const CNavGrid* pGrid,
        f32_t radiusWorld,
        bool_t bUseRadius,
        CellPredicate pCanUseCell,
        StepPredicate pCanStep)
    {
        thread_local std::vector<ReachabilityCache> tls_caches{};

        const uint64_t gridCacheId = pGrid->GetCacheId();
        const uint32_t revision = pGrid->GetRevision();

        for (ReachabilityCache& cache : tls_caches)
        {
            if (cache.pGrid != pGrid ||
                cache.gridCacheId != gridCacheId ||
                cache.bUseRadius != bUseRadius ||
                cache.radiusWorld != radiusWorld)
            {
                continue;
            }

            if (cache.revision != revision ||
                cache.components.size() != CNavGrid::kTotalCells ||
                cache.nearestCell.size() != CNavGrid::kTotalCells)
            {
                BuildReachabilityCache(cache, pGrid, radiusWorld, bUseRadius, pCanUseCell, pCanStep);
            }

            return cache;
        }

        tls_caches.emplace_back();
        ReachabilityCache& cache = tls_caches.back();
        BuildReachabilityCache(cache, pGrid, radiusWorld, bUseRadius, pCanUseCell, pCanStep);
        return cache;
    }

    bool_t IsCellInComponent(
        const CNavGrid* pGrid,
        const ReachabilityCache& cache,
        CNavGrid::Cell cell,
        int32_t component)
    {
        if (component < 0 || !pGrid->IsInBounds(cell.x, cell.y))
            return false;

        const uint32_t idx = CellIdx(cell.x, cell.y);
        return idx < cache.components.size() && cache.components[idx] == component;
    }

    bool_t TryResolveFromNearestMap(
        const CNavGrid* pGrid,
        const ReachabilityCache& cache,
        CNavGrid::Cell rawGoal,
        int32_t maxRadius,
        int32_t targetComponent,
        CNavGrid::Cell& outCell)
    {
        if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
            return false;

        const uint32_t idx = CellIdx(rawGoal.x, rawGoal.y);
        if (idx >= cache.nearestComponent.size() ||
            idx >= cache.nearestDistance.size() ||
            idx >= cache.nearestCell.size())
            return false;

        if (cache.nearestComponent[idx] != targetComponent)
            return false;
        if (cache.nearestDistance[idx] > static_cast<uint16_t>((std::max)(0, maxRadius)))
            return false;

        outCell = cache.nearestCell[idx];
        return pGrid->IsInBounds(outCell.x, outCell.y);
    }

    bool_t FindNearestCellInComponent(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell rawGoal,
        int32_t maxRadius,
        const ReachabilityCache& cache,
        int32_t targetComponent,
        CNavGrid::Cell& outCell)
    {
        if (maxRadius < 0)
            return false;

        thread_local std::vector<uint32_t> tls_visitedGeneration(CNavGrid::kTotalCells);
        thread_local uint32_t tls_currentGeneration = 0u;

        const uint32_t currentGeneration =
            BeginSearchGeneration(tls_visitedGeneration, tls_currentGeneration);

        std::queue<BfsNode> open{};

        auto Push = [&](CNavGrid::Cell cell, int32_t depth)
            {
                if (!pGrid->IsInBounds(cell.x, cell.y))
                    return;

                const uint32_t idx = CellIdx(cell.x, cell.y);
                if (tls_visitedGeneration[idx] == currentGeneration)
                    return;

                tls_visitedGeneration[idx] = currentGeneration;
                open.push({ cell, depth });
            };

        Push(rawGoal, 0);

        int32_t currentDepth = 0;
        bool_t bFoundAtDepth = false;
        f32_t bestStartHeuristic = std::numeric_limits<f32_t>::infinity();
        CNavGrid::Cell bestCell{};

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

            if (IsCellInComponent(pGrid, cache, node.cell, targetComponent))
            {
                const f32_t startHeuristic =
                    OctileDistance(node.cell.x - start.x, node.cell.y - start.y);

                if (!bFoundAtDepth || startHeuristic < bestStartHeuristic)
                {
                    bFoundAtDepth = true;
                    bestStartHeuristic = startHeuristic;
                    bestCell = node.cell;
                }

                continue;
            }

            for (int32_t k = 0; k < 8; ++k)
                Push({ node.cell.x + kDX[k], node.cell.y + kDY[k] }, node.depth + 1);
        }

        if (!bFoundAtDepth)
            return false;

        outCell = bestCell;
        return true;
    }

    std::vector<CNavGrid::Cell> FindPathInternal(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell goal,
        f32_t radiusWorld,
        CellPredicate pCanUseCell,
        StepPredicate pCanStep,
        const char* pNodesCounterName)
    {
        std::vector<CNavGrid::Cell> emptyPath;
        if (!pGrid ||
            !pCanUseCell(pGrid, start, radiusWorld) ||
            !pCanUseCell(pGrid, goal, radiusWorld))
            return emptyPath;

        u32_t nodesVisited = 0;
        struct CounterFlush
        {
            const char* pName;
            const u32_t& count;

            ~CounterFlush()
            {
                WINTERS_PROFILE_COUNT(pName, static_cast<i32_t>(count));
            }
        } guard{ pNodesCounterName, nodesVisited };

        constexpr uint32_t kN = CNavGrid::kTotalCells;

        thread_local std::vector<f32_t> tls_gScore(kN);
        thread_local std::vector<int32_t> tls_parent(kN);
        thread_local std::vector<uint8_t> tls_closed(kN);
        thread_local std::vector<uint32_t> tls_generation(kN);
        thread_local uint32_t tls_currentGeneration = 0u;

        const uint32_t currentGeneration =
            BeginSearchGeneration(tls_generation, tls_currentGeneration);

        auto Touch = [&](uint32_t idx)
            {
                if (tls_generation[idx] == currentGeneration)
                    return;

                tls_generation[idx] = currentGeneration;
                tls_gScore[idx] = std::numeric_limits<f32_t>::infinity();
                tls_parent[idx] = -1;
                tls_closed[idx] = 0;
            };

        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

        const uint32_t startIdx = CellIdx(start.x, start.y);
        Touch(startIdx);
        tls_gScore[startIdx] = 0.f;
        open.push({ start.x, start.y, 0.f, OctileDistance(goal.x - start.x, goal.y - start.y) });

        while (!open.empty())
        {
            const Node cur = open.top();
            open.pop();

            const uint32_t curIdx = CellIdx(cur.cx, cur.cy);
            Touch(curIdx);

            if (tls_closed[curIdx])
                continue;

            tls_closed[curIdx] = 1;
            ++nodesVisited;

            if (cur.cx == goal.x && cur.cy == goal.y)
                break;

            for (int32_t k = 0; k < 8; ++k)
            {
                const int32_t dx = kDX[k];
                const int32_t dy = kDY[k];
                const int32_t nx = cur.cx + dx;
                const int32_t ny = cur.cy + dy;

                if (!pCanStep(pGrid, { cur.cx, cur.cy }, dx, dy, radiusWorld))
                    continue;

                const uint32_t nextIdx = CellIdx(nx, ny);
                Touch(nextIdx);

                const f32_t step = (dx != 0 && dy != 0) ? kSqrt2 : 1.f;
                const f32_t tentativeG = cur.fG + step;

                if (tentativeG < tls_gScore[nextIdx])
                {
                    tls_gScore[nextIdx] = tentativeG;
                    tls_parent[nextIdx] = static_cast<int32_t>(curIdx);
                    const f32_t h = OctileDistance(goal.x - nx, goal.y - ny);
                    open.push({ nx, ny, tentativeG, tentativeG + h });
                }
            }
        }

        const uint32_t goalIdx = CellIdx(goal.x, goal.y);
        Touch(goalIdx);

        if (tls_parent[goalIdx] == -1 && !(goal.x == start.x && goal.y == start.y))
            return emptyPath;

        std::vector<CNavGrid::Cell> path;
        uint32_t curIdx = goalIdx;

        while (true)
        {
            const int32_t cx = static_cast<int32_t>(curIdx % CNavGrid::kCellCountX);
            const int32_t cy = static_cast<int32_t>(curIdx / CNavGrid::kCellCountX);
            path.push_back({ cx, cy });

            if (cx == start.x && cy == start.y)
                break;

            const int32_t parentIdx = tls_parent[curIdx];
            if (parentIdx < 0)
            {
                path.clear();
                return emptyPath;
            }

            curIdx = static_cast<uint32_t>(parentIdx);
        }

        std::reverse(path.begin(), path.end());
        return path;
    }

    bool_t TryBuildDirectCellPath(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell goal,
        f32_t radiusWorld,
        std::vector<CNavGrid::Cell>& outPath)
    {
        const Vec3 from = pGrid->CellToWorld(start.x, start.y);
        const Vec3 to = pGrid->CellToWorld(goal.x, goal.y);

        if (!pGrid->SegmentWalkable(from, to, radiusWorld))
            return false;

        outPath.clear();
        outPath.push_back(start);
        if (goal.x != start.x || goal.y != start.y)
            outPath.push_back(goal);

        WINTERS_PROFILE_COUNT("AStar::DirectBypass", 1);
        return true;
    }

    bool_t TryFindNearestReachableGoalInternal(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell rawGoal,
        int32_t maxRadius,
        f32_t radiusWorld,
        bool_t bUseRadius,
        CellPredicate pCanUseCell,
        StepPredicate pCanStep,
        const char* pNodesCounterName,
        CNavGrid::Cell& outGoal,
        std::vector<CNavGrid::Cell>* pOutPath)
    {
        if (!pGrid || !pCanUseCell(pGrid, start, radiusWorld))
            return false;

        if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
            return false;

        ReachabilityCache& cache =
            EnsureReachabilityCache(pGrid, radiusWorld, bUseRadius, pCanUseCell, pCanStep);

        const uint32_t startIdx = CellIdx(start.x, start.y);
        if (startIdx >= cache.components.size())
            return false;

        const int32_t startComponent = cache.components[startIdx];
        if (startComponent < 0)
            return false;

        CNavGrid::Cell resolvedGoal{};
        if (IsCellInComponent(pGrid, cache, rawGoal, startComponent))
        {
            resolvedGoal = rawGoal;
        }
        else if (TryResolveFromNearestMap(
            pGrid,
            cache,
            rawGoal,
            maxRadius,
            startComponent,
            resolvedGoal))
        {
            WINTERS_PROFILE_COUNT("AStar::NearestMapHit", 1);
        }
        else if (!FindNearestCellInComponent(
            pGrid,
            start,
            rawGoal,
            maxRadius,
            cache,
            startComponent,
            resolvedGoal))
        {
            return false;
        }

        if (pOutPath)
        {
            if (!TryBuildDirectCellPath(pGrid, start, resolvedGoal, radiusWorld, *pOutPath))
            {
                *pOutPath = FindPathInternal(
                    pGrid,
                    start,
                    resolvedGoal,
                    radiusWorld,
                    pCanUseCell,
                    pCanStep,
                    pNodesCounterName);

                if (pOutPath->empty())
                    return false;
            }
        }

        outGoal = resolvedGoal;
        return true;
    }
}

f32_t CPathfinder::Octile(int32_t dx, int32_t dy)
{
    return OctileDistance(dx, dy);
}

std::vector<CNavGrid::Cell> CPathfinder::Find_Path(
    const CNavGrid* pGrid, CNavGrid::Cell start, CNavGrid::Cell goal)
{
    WINTERS_PROFILE_SCOPE("AStar::FindPath");

    return FindPathInternal(
        pGrid,
        start,
        goal,
        0.f,
        IsWalkableCell,
        CanStepToNeighborNoRadius,
        "AStar::NodesVisited");
}

std::vector<CNavGrid::Cell> CPathfinder::FindPathForRadius(
    const CNavGrid* pGrid,
    CNavGrid::Cell start,
    CNavGrid::Cell goal,
    f32_t radiusWorld)
{
    WINTERS_PROFILE_SCOPE("AStar::FindPathForRadius");

    return FindPathInternal(
        pGrid,
        start,
        goal,
        radiusWorld,
        IsWalkableCellForRadius,
        CanStepToNeighborWithRadius,
        "AStarRadius::NodesVisited");
}

void CPathfinder::PrewarmReachabilityCache(const CNavGrid* pGrid)
{
    if (!pGrid)
        return;

    (void)EnsureReachabilityCache(
        pGrid,
        0.f,
        false,
        IsWalkableCell,
        CanStepToNeighborNoRadius);
}

void CPathfinder::PrewarmReachabilityCacheForRadius(const CNavGrid* pGrid, f32_t radiusWorld)
{
    if (!pGrid)
        return;

    (void)EnsureReachabilityCache(
        pGrid,
        radiusWorld,
        true,
        IsWalkableCellForRadius,
        CanStepToNeighborWithRadius);
}

bool_t CPathfinder::TryFindNearestReachableGoal(
    const CNavGrid* pGrid,
    CNavGrid::Cell start,
    CNavGrid::Cell rawGoal,
    int32_t maxRadius,
    CNavGrid::Cell& outGoal,
    std::vector<CNavGrid::Cell>* pOutPath)
{
    return TryFindNearestReachableGoalInternal(
        pGrid,
        start,
        rawGoal,
        maxRadius,
        0.f,
        false,
        IsWalkableCell,
        CanStepToNeighborNoRadius,
        "AStar::NodesVisited",
        outGoal,
        pOutPath);
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
    return TryFindNearestReachableGoalInternal(
        pGrid,
        start,
        rawGoal,
        maxRadius,
        radiusWorld,
        true,
        IsWalkableCellForRadius,
        CanStepToNeighborWithRadius,
        "AStarRadius::NodesVisited",
        outGoal,
        pOutPath);
}
