#pragma once
#include "Engine_Defines.h"
#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "Manager/Navigation/NavGrid.h"
#include <vector>

NS_BEGIN(Engine)

// 빈 경로의 원인 구분 (P3 특수상황 명시 — 과거 minion-stuck 사고의 silent empty path 대책).
// 빈 vector 하나로 뭉개지던 4가지 실패 원인을 호출자가 구분할 수 있게 한다.
enum class ePathFindResult : u8_t
{
    Success = 0,
    NullGrid,
    StartBlocked,
    GoalBlocked,
    NoRoute,
    BrokenPath,
};

class WINTERS_ENGINE CPathfinder final
{
public:
    static std::vector<CNavGrid::Cell> Find_Path(
        const CNavGrid* pGrid,
        CNavGrid::Cell  start,
        CNavGrid::Cell  goal,
        ePathFindResult* pOutResult = nullptr);

    static std::vector<CNavGrid::Cell> FindPathForRadius(
        const CNavGrid* pGrid,
        CNavGrid::Cell start,
        CNavGrid::Cell goal,
        f32_t radiusWorld,
        ePathFindResult* pOutResult = nullptr);

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
