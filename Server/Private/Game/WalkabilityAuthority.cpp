#include "Game/WalkabilityAuthority.h"

#include "Manager/Navigation/MapSurfaceSampler.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Navigation/Pathfinder.h"
#include "Shared/GameSim/Definitions/StageData.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <vector>

namespace
{
    bool_t FileExistsForServer(const std::wstring& path)
    {
        const DWORD attr = GetFileAttributesW(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool_t TryResolveExistingServerPath(
        const std::wstring& candidate,
        std::wstring& outPath)
    {
        wchar_t full[MAX_PATH]{};
        const DWORD got = GetFullPathNameW(candidate.c_str(), MAX_PATH, full, nullptr);
        if (got == 0 || got >= MAX_PATH)
            return false;

        if (!FileExistsForServer(full))
            return false;

        outPath = full;
        return true;
    }

    void PushUniqueServerPath(std::vector<std::wstring>& paths, const std::wstring& path)
    {
        if (path.empty())
            return;

        for (const std::wstring& existing : paths)
        {
            if (_wcsicmp(existing.c_str(), path.c_str()) == 0)
                return;
        }

        paths.push_back(path);
    }

    void EnsureServerTrailingSlash(std::wstring& path)
    {
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
            path.push_back(L'\\');
    }

    void PushWorkspaceDataPathCandidate(
        std::vector<std::wstring>& paths,
        const std::wstring& startDir,
        const wchar_t* pFileName)
    {
        if (!pFileName || pFileName[0] == L'\0')
            return;

        std::wstring base = startDir;
        EnsureServerTrailingSlash(base);

        for (u32_t depth = 0; depth < 8 && !base.empty(); ++depth)
        {
            if (FileExistsForServer(base + L"Winters.sln"))
            {
                PushUniqueServerPath(paths, base + L"Data\\" + pFileName);
                return;
            }

            std::wstring trimmed = base;
            while (!trimmed.empty() && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
                trimmed.pop_back();

            const size_t slash = trimmed.find_last_of(L"\\/");
            if (slash == std::wstring::npos)
                break;

            base = trimmed.substr(0, slash + 1);
        }
    }

    bool_t TryResolveAuthoredNavGridPath(
        const wchar_t* pStagePath,
        std::wstring& outPath)
    {
        outPath.clear();

        std::vector<std::wstring> candidates{};
        if (pStagePath && pStagePath[0] != L'\0')
        {
            std::wstring fromStage = pStagePath;
            const size_t dot = fromStage.find_last_of(L'.');
            if (dot != std::wstring::npos)
                fromStage.resize(dot);
            fromStage += L".navgrid";
            PushUniqueServerPath(candidates, fromStage);
        }

        wchar_t exePath[MAX_PATH]{};
        const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (len > 0 && len < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash + 1);
                PushWorkspaceDataPathCandidate(candidates, exeDir, L"Stage1.navgrid");
            }
        }

        wchar_t cwd[MAX_PATH]{};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
        {
            std::wstring cwdDir = cwd;
            if (!cwdDir.empty() && cwdDir.back() != L'\\' && cwdDir.back() != L'/')
                cwdDir.push_back(L'\\');
            PushWorkspaceDataPathCandidate(candidates, cwdDir, L"Stage1.navgrid");
        }

        for (const std::wstring& candidate : candidates)
        {
            if (TryResolveExistingServerPath(candidate, outPath))
                return true;
        }

        return false;
    }

    const Engine::CNavGrid* SelectWalkabilityGrid(
        const Engine::CNavGrid* pPathGrid,
        const Engine::CNavGrid* pBaseGrid)
    {
        return pPathGrid ? pPathGrid : pBaseGrid;
    }

    WalkabilityGridCell ToWalkabilityCell(const Engine::CNavGrid::Cell& cell)
    {
        return WalkabilityGridCell{ cell.x, cell.y };
    }

    WalkabilitySurfaceAdjustResult ApplySafeMoveHeight(
        const Engine::CMapSurfaceSampler* pSurfaceSampler,
        const Vec3& pos,
        f32_t fallbackY,
        f32_t surfaceDeltaBaseY,
        f32_t maxSurfaceDeltaY)
    {
        WalkabilitySurfaceAdjustResult result{};
        result.position = pos;

        f32_t sampledY = fallbackY;
        if (!CWalkabilityAuthority::TrySampleHeight(
            pSurfaceSampler,
            result.position.x,
            result.position.z,
            sampledY))
        {
            result.position.y = fallbackY;
            return result;
        }

        result.bSampled = true;
        result.sampledY = sampledY;
        result.surfaceDeltaY = sampledY - surfaceDeltaBaseY;
        if (std::fabs(result.surfaceDeltaY) <= maxSurfaceDeltaY)
        {
            result.position.y = sampledY;
            return result;
        }

        result.bRejectedBySurfaceDelta = true;
        result.position.y = fallbackY;
        return result;
    }

    void PushRejectedSurface(
        std::vector<WalkabilitySurfaceAdjustResult>& rejectedSurfaces,
        const WalkabilitySurfaceAdjustResult& surface)
    {
        if (surface.bRejectedBySurfaceDelta)
            rejectedSurfaces.push_back(surface);
    }

    std::vector<Engine::CNavGrid::Cell> SmoothPathCells(
        const Engine::CNavGrid& navGrid,
        const std::vector<Engine::CNavGrid::Cell>& path)
    {
        if (path.size() <= 2)
            return path;

        std::vector<Engine::CNavGrid::Cell> smoothed{};
        smoothed.reserve(path.size());
        smoothed.push_back(path.front());

        size_t anchor = 0;
        while (anchor + 1u < path.size())
        {
            size_t best = anchor + 1u;
            for (size_t probe = path.size() - 1u; probe > anchor + 1u; --probe)
            {
                if (navGrid.LineCellsWalkableForRadius(path[anchor], path[probe], 0.f))
                {
                    best = probe;
                    break;
                }
            }

            smoothed.push_back(path[best]);
            anchor = best;
        }

        return smoothed;
    }

    void IncludeStageGameplayBoundsPoint(WalkabilityStageBounds& bounds, const Vec3& p)
    {
        if (!bounds.bAny)
        {
            bounds.minX = p.x;
            bounds.minZ = p.z;
            bounds.maxX = p.x;
            bounds.maxZ = p.z;
            bounds.bAny = true;
            return;
        }

        bounds.minX = (std::min)(bounds.minX, p.x);
        bounds.minZ = (std::min)(bounds.minZ, p.z);
        bounds.maxX = (std::max)(bounds.maxX, p.x);
        bounds.maxZ = (std::max)(bounds.maxZ, p.z);
    }

    WalkabilityStageBounds BuildStageGameplayBounds(
        const Winters::Map::StageData& stage)
    {
        WalkabilityStageBounds bounds{};

        for (const auto& waypoint : stage.minionWaypoints)
        {
            IncludeStageGameplayBoundsPoint(
                bounds,
                Vec3{ waypoint.px, waypoint.py, waypoint.pz });
        }

        for (const auto& structure : stage.structures)
        {
            if (structure.bVisible == 0u)
                continue;

            IncludeStageGameplayBoundsPoint(
                bounds,
                Vec3{ structure.px, structure.py, structure.pz });
        }

        return bounds;
    }
}

WalkabilityPathResolveResult CWalkabilityAuthority::ResolveWMeshPath()
{
    WalkabilityPathResolveResult result{};

    std::vector<std::wstring> candidates;
    wchar_t exePath[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len > 0 && len < MAX_PATH)
    {
        std::wstring exeDir = exePath;
        const size_t slash = exeDir.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
        {
            exeDir.resize(slash + 1);
            candidates.push_back(exeDir + L"Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
            candidates.push_back(exeDir + L"..\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
            candidates.push_back(exeDir + L"..\\..\\..\\Client\\Bin\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
            candidates.push_back(exeDir + L"..\\..\\..\\Client\\Bin\\Debug\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
        }
    }

    wchar_t cwd[MAX_PATH]{};
    const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
    if (cwdLen > 0 && cwdLen < MAX_PATH)
    {
        std::wstring cwdDir = cwd;
        if (!cwdDir.empty() && cwdDir.back() != L'\\' && cwdDir.back() != L'/')
            cwdDir.push_back(L'\\');
        candidates.push_back(cwdDir + L"Client\\Bin\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
    }

    candidates.push_back(L"Client\\Bin\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");
    candidates.push_back(L"Client\\Bin\\Debug\\Resource\\Texture\\MAP\\output\\sr_base_flip.wmesh");

    for (const std::wstring& candidate : candidates)
    {
        if (TryResolveExistingServerPath(candidate, result.path))
        {
            result.bResolved = true;
            return result;
        }
    }

    return result;
}

WalkabilityAuthoredNavGridLoadResult CWalkabilityAuthority::TryLoadAuthoredNavGrid(
    const wchar_t* pStagePath)
{
    WalkabilityAuthoredNavGridLoadResult result{};

    if (!TryResolveAuthoredNavGridPath(pStagePath, result.path))
        return result;

    result.bPathResolved = true;
    result.pGrid = Engine::CNavGrid::LoadFromFile(result.path.c_str());
    result.bLoadFailed = !result.pGrid;
    return result;
}

WalkabilityNavGridCoverageResult CWalkabilityAuthority::CheckNavGridCoversStageBounds(
    const Engine::CNavGrid& navGrid,
    const Winters::Map::StageData& stage,
    f32_t padding)
{
    WalkabilityNavGridCoverageResult result{};
    result.bounds = BuildStageGameplayBounds(stage);
    if (!result.bounds.bAny)
        return result;

    const f32_t navMinX = navGrid.Get_OriginX();
    const f32_t navMinZ = navGrid.Get_OriginZ();
    const f32_t navMaxX =
        navMinX + Engine::CNavGrid::kCellCountX * Engine::CNavGrid::kCellSize;
    const f32_t navMaxZ =
        navMinZ + Engine::CNavGrid::kCellCountY * Engine::CNavGrid::kCellSize;

    result.bCovered =
        result.bounds.minX >= navMinX + padding &&
        result.bounds.minZ >= navMinZ + padding &&
        result.bounds.maxX <= navMaxX - padding &&
        result.bounds.maxZ <= navMaxZ - padding;
    return result;
}

bool_t CWalkabilityAuthority::IsWalkableXZ(
    const Engine::CNavGrid* pPathGrid,
    const Engine::CNavGrid* pBaseGrid,
    const Vec3& pos)
{
    const Engine::CNavGrid* pGrid = SelectWalkabilityGrid(pPathGrid, pBaseGrid);
    if (!pGrid)
        return true;

    const Engine::CNavGrid::Cell cell = pGrid->WorldToCell(pos);
    return pGrid->IsWalkable(cell.x, cell.y);
}

bool_t CWalkabilityAuthority::SegmentWalkableXZ(
    const Engine::CNavGrid* pBaseGrid,
    const Vec3& from,
    const Vec3& to,
    f32_t radiusWorld)
{
    if (!pBaseGrid)
        return true;

    return pBaseGrid->SegmentWalkable(from, to, (std::max)(0.f, radiusWorld));
}

bool_t CWalkabilityAuthority::TryClampMoveSegmentXZ(
    const Engine::CNavGrid* pBaseGrid,
    const Engine::CMapSurfaceSampler* pSurfaceSampler,
    const Vec3& from,
    const Vec3& desired,
    f32_t radiusWorld,
    Vec3& outPosition)
{
    outPosition = desired;
    if (!pBaseGrid)
        return true;

    const f32_t radius = (std::max)(0.f, radiusWorld);
    if (pBaseGrid->SegmentWalkable(from, desired, radius))
        return true;

    const Engine::CNavGrid::Cell fromCell = pBaseGrid->WorldToCell(from);
    if (!pBaseGrid->IsWalkable(fromCell.x, fromCell.y))
    {
        Engine::CNavGrid::Cell nearest{};
        if (!pBaseGrid->TryFindNearestWalkableCell(fromCell, 16, nearest))
        {
            outPosition = from;
            return false;
        }

        outPosition = pBaseGrid->CellToWorld(nearest.x, nearest.y);
        if (!TrySampleHeight(pSurfaceSampler, outPosition.x, outPosition.z, outPosition.y))
            outPosition.y = from.y;
        return true;
    }

    // 풋프린트 오버랩 탈출: 중심 셀은 walkable 이지만 반경 풋프린트가 carve 에
    // 겹친 시작점은 SegmentWalkable(from, *, radius) 가 전 방향 실패해 영구
    // 웨지가 된다(대시 착지/플래시/강제이동 종료 진입). 반경 0 클램프로
    // 셀 단위 보행을 허용해 겹침에서 걸어 나올 수 있게 한다.
    // SegmentWalkable(from, from, radius) 는 길이 0 세그먼트로
    // IsAreaWalkable(from, radius) 와 동치인 풋프린트 검사다.
    if (radius > 0.f && !pBaseGrid->SegmentWalkable(from, from, radius))
    {
        Vec3 escaped = desired;
        if (TryClampMoveSegmentXZ(pBaseGrid, pSurfaceSampler, from, desired, 0.f, escaped))
        {
            outPosition = escaped;
            return true;
        }

        outPosition = from;
        return false;
    }

    f32_t low = 0.f;
    f32_t high = 1.f;
    for (u32_t i = 0; i < 12u; ++i)
    {
        const f32_t mid = (low + high) * 0.5f;
        const Vec3 probe{
            from.x + (desired.x - from.x) * mid,
            from.y + (desired.y - from.y) * mid,
            from.z + (desired.z - from.z) * mid
        };

        if (pBaseGrid->SegmentWalkable(from, probe, radius))
            low = mid;
        else
            high = mid;
    }

    if (low <= 0.001f)
    {
        outPosition = from;
        return false;
    }

    outPosition = Vec3{
        from.x + (desired.x - from.x) * low,
        from.y + (desired.y - from.y) * low,
        from.z + (desired.z - from.z) * low
    };
    return true;
}

bool_t CWalkabilityAuthority::TrySampleHeight(
    const Engine::CMapSurfaceSampler* pSurfaceSampler,
    f32_t x,
    f32_t z,
    f32_t& outY)
{
    if (!pSurfaceSampler)
        return false;

    f32_t height = 0.f;
    if (!pSurfaceSampler->SampleHeight(x, z, height))
        return false;

    outY = height + 0.05f;
    return true;
}

bool_t CWalkabilityAuthority::TryResolveWalkablePosition(
    const Engine::CNavGrid* pPathGrid,
    const Engine::CNavGrid* pBaseGrid,
    const Engine::CMapSurfaceSampler* pSurfaceSampler,
    const Vec3& rawPos,
    int32_t maxRadius,
    Vec3& outPos)
{
    const Engine::CNavGrid* pGrid = SelectWalkabilityGrid(pPathGrid, pBaseGrid);

    if (!pGrid)
    {
        outPos = rawPos;
        return true;
    }

    const Engine::CNavGrid::Cell cell = pGrid->WorldToCell(rawPos);

    if (pGrid->IsWalkable(cell.x, cell.y))
    {
        outPos = rawPos;
        return true;
    }

    Engine::CNavGrid::Cell nearest{};
    if (!pGrid->TryFindNearestWalkableCell(cell, maxRadius, nearest))
        return false;

    outPos = pGrid->CellToWorld(nearest.x, nearest.y);
    if (!TrySampleHeight(pSurfaceSampler, outPos.x, outPos.z, outPos.y))
        outPos.y = rawPos.y;

    return true;
}

MoveTargetResolveResult CWalkabilityAuthority::ResolveMoveTarget(
    const Engine::CNavGrid* pPathGrid,
    const Engine::CNavGrid* pBaseGrid,
    const Engine::CMapSurfaceSampler* pSurfaceSampler,
    const Vec3& from,
    const Vec3& rawTarget,
    f32_t maxSurfaceDeltaY)
{
    MoveTargetResolveResult result{};
    result.resolvedTarget = rawTarget;

    const Engine::CNavGrid* pGrid = SelectWalkabilityGrid(pPathGrid, pBaseGrid);
    if (!pGrid)
    {
        result.bSuccess = true;
        result.bNoGrid = true;
        result.surface = ApplySafeMoveHeight(
            pSurfaceSampler,
            result.resolvedTarget,
            from.y,
            from.y,
            maxSurfaceDeltaY);
        result.resolvedTarget = result.surface.position;
        return result;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(from);
    result.startCell = ToWalkabilityCell(start);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return result;

        result.bStartBlocked = true;
        result.nearestStartCell = ToWalkabilityCell(nearestStart);
        start = nearestStart;
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);
    result.rawGoalCell = ToWalkabilityCell(rawGoal);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
    {
        result.bOutOfBounds = true;
        result.gridOriginX = pGrid->Get_OriginX();
        result.gridOriginZ = pGrid->Get_OriginZ();
        return result;
    }

    Engine::CNavGrid::Cell resolved{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        return result;
    }

    result.resolvedGoalCell = ToWalkabilityCell(resolved);
    result.pathSize = static_cast<u32_t>(path.size());
    result.bCorrected = resolved.x != rawGoal.x || resolved.y != rawGoal.y;
    result.resolvedTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    result.surface = ApplySafeMoveHeight(
        pSurfaceSampler,
        result.resolvedTarget,
        from.y,
        from.y,
        maxSurfaceDeltaY);
    result.resolvedTarget = result.surface.position;
    result.bSuccess = true;
    return result;
}

MovePathBuildResult CWalkabilityAuthority::BuildMovePath(
    const Engine::CNavGrid* pPathGrid,
    const Engine::CNavGrid* pBaseGrid,
    const Engine::CMapSurfaceSampler* pSurfaceSampler,
    const Vec3& from,
    const Vec3& rawTarget,
    u16_t maxWaypoints,
    f32_t maxSurfaceDeltaY)
{
    MovePathBuildResult result{};
    result.resolvedTarget = rawTarget;
    if (maxWaypoints == 0u)
        return result;

    const Engine::CNavGrid* pGrid = SelectWalkabilityGrid(pPathGrid, pBaseGrid);
    if (!pGrid)
    {
        result.mode = eMovePathBuildMode::NoGrid;
        const WalkabilitySurfaceAdjustResult surface = ApplySafeMoveHeight(
            pSurfaceSampler,
            result.resolvedTarget,
            from.y,
            from.y,
            maxSurfaceDeltaY);
        result.resolvedTarget = surface.position;
        PushRejectedSurface(result.rejectedSurfaces, surface);
        result.waypoints.push_back(result.resolvedTarget);
        result.bSuccess = true;
        return result;
    }

    Engine::CNavGrid::Cell start = pGrid->WorldToCell(from);
    result.startCell = ToWalkabilityCell(start);
    if (!pGrid->IsWalkable(start.x, start.y))
    {
        Engine::CNavGrid::Cell nearestStart{};
        if (!pGrid->TryFindNearestWalkableCell(start, 16, nearestStart))
            return result;
        start = nearestStart;
        result.startCell = ToWalkabilityCell(start);
    }

    const Engine::CNavGrid::Cell rawGoal = pGrid->WorldToCell(rawTarget);
    result.rawGoalCell = ToWalkabilityCell(rawGoal);
    if (!pGrid->IsInBounds(rawGoal.x, rawGoal.y))
        return result;

    if (pGrid->IsWalkable(rawGoal.x, rawGoal.y) &&
        pGrid->SegmentWalkable(from, rawTarget, 0.f))
    {
        result.mode = eMovePathBuildMode::Direct;
        const WalkabilitySurfaceAdjustResult surface = ApplySafeMoveHeight(
            pSurfaceSampler,
            rawTarget,
            from.y,
            from.y,
            maxSurfaceDeltaY);
        result.resolvedTarget = surface.position;
        PushRejectedSurface(result.rejectedSurfaces, surface);
        result.waypoints.push_back(result.resolvedTarget);
        result.bSuccess = true;
        return result;
    }

    Engine::CNavGrid::Cell resolved{};
    std::vector<Engine::CNavGrid::Cell> path{};
    if (!Engine::CPathfinder::TryFindNearestReachableGoal(
        pGrid,
        start,
        rawGoal,
        96,
        resolved,
        &path))
    {
        return result;
    }

    result.resolvedGoalCell = ToWalkabilityCell(resolved);
    result.rawPathSize = static_cast<u32_t>(path.size());
    result.resolvedTarget = pGrid->CellToWorld(resolved.x, resolved.y);
    const WalkabilitySurfaceAdjustResult targetSurface = ApplySafeMoveHeight(
        pSurfaceSampler,
        result.resolvedTarget,
        from.y,
        from.y,
        maxSurfaceDeltaY);
    result.resolvedTarget = targetSurface.position;
    PushRejectedSurface(result.rejectedSurfaces, targetSurface);

    Engine::CNavGrid::Cell lastAppended{ -1, -1 };
    auto AppendCell = [&](Engine::CNavGrid::Cell cell) -> bool_t
        {
            if (cell.x == lastAppended.x && cell.y == lastAppended.y)
                return true;
            if (result.waypoints.size() >= maxWaypoints)
                return false;

            Vec3 waypoint = pGrid->CellToWorld(cell.x, cell.y);
            const WalkabilitySurfaceAdjustResult waypointSurface = ApplySafeMoveHeight(
                pSurfaceSampler,
                waypoint,
                result.resolvedTarget.y,
                from.y,
                maxSurfaceDeltaY);
            waypoint = waypointSurface.position;
            PushRejectedSurface(result.rejectedSurfaces, waypointSurface);

            result.waypoints.push_back(waypoint);
            lastAppended = cell;
            return true;
        };

    const std::vector<Engine::CNavGrid::Cell> smoothedPath = SmoothPathCells(*pGrid, path);
    result.smoothedPathSize = static_cast<u32_t>(smoothedPath.size());
    if (smoothedPath.size() <= 1)
    {
        result.mode = eMovePathBuildMode::PathSingle;
        result.bSuccess = AppendCell(resolved);
        return result;
    }

    result.mode = eMovePathBuildMode::Path;
    for (size_t i = 1; i < smoothedPath.size(); ++i)
    {
        if (!AppendCell(smoothedPath[i]))
            return result;
    }

    result.bSuccess = !result.waypoints.empty();
    return result;
}
