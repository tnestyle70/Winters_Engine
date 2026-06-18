#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "Manager/Navigation/NavGrid.h"
#include <vector>

NS_BEGIN(Engine)

class WINTERS_ENGINE CPathfinder final
{
public:
    static std::vector<CNavGrid::Cell> Find_Path(
        const CNavGrid* pGrid,
        CNavGrid::Cell  start,
        CNavGrid::Cell  goal);

    static std::vector<CNavGrid::Cell> FindPathForRadius(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell goal,
        f32_t radiusWorld);

    static bool_t TryFindNearestReachableGoal(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell rawGoal,
        int32_t maxRadius,
        CNavGrid::Cell& outGoal,
        std::vector<CNavGrid::Cell>* pOutPath = nullptr);

    static bool_t TryFindNearestReachableGoalForRadius(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell rawGoal,
        int32_t maxRadius,
        f32_t radiusWorld,
        CNavGrid::Cell& outGoal,
        std::vector<CNavGrid::Cell>* pOutPath = nullptr);

    static void PrewarmReachabilityCache(const CNavGrid* pGrid);
    static void PrewarmReachabilityCacheForRadius(const CNavGrid* pGrid, f32_t radiusWorld);

private:
    CPathfinder() = delete;
    static f32_t Octile(int32_t dx, int32_t dy);
};

NS_END
