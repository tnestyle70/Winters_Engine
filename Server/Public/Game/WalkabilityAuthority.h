#pragma once

#include "Manager/Navigation/NavGrid.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine
{
    class CMapSurfaceSampler;
}

namespace Winters::Map
{
    struct StageData;
}

struct WalkabilityGridCell
{
    int32_t x = -1;
    int32_t y = -1;
};

struct WalkabilityStageBounds
{
    f32_t minX = 0.f;
    f32_t minZ = 0.f;
    f32_t maxX = 0.f;
    f32_t maxZ = 0.f;
    bool_t bAny = false;
};

struct WalkabilityNavGridCoverageResult
{
    WalkabilityStageBounds bounds{};
    bool_t bCovered = true;
};

struct WalkabilitySurfaceAdjustResult
{
    Vec3 position{};
    f32_t sampledY = 0.f;
    f32_t surfaceDeltaY = 0.f;
    bool_t bSampled = false;
    bool_t bRejectedBySurfaceDelta = false;
};

struct MoveTargetResolveResult
{
    Vec3 resolvedTarget{};
    WalkabilitySurfaceAdjustResult surface{};
    WalkabilityGridCell startCell{};
    WalkabilityGridCell nearestStartCell{};
    WalkabilityGridCell rawGoalCell{};
    WalkabilityGridCell resolvedGoalCell{};
    f32_t gridOriginX = 0.f;
    f32_t gridOriginZ = 0.f;
    u32_t pathSize = 0u;
    bool_t bSuccess = false;
    bool_t bNoGrid = false;
    bool_t bStartBlocked = false;
    bool_t bOutOfBounds = false;
    bool_t bCorrected = false;
};

enum class eMovePathBuildMode : u8_t
{
    None,
    NoGrid,
    Direct,
    PathSingle,
    Path,
};

struct MovePathBuildResult
{
    Vec3 resolvedTarget{};
    std::vector<Vec3> waypoints{};
    std::vector<WalkabilitySurfaceAdjustResult> rejectedSurfaces{};
    WalkabilityGridCell startCell{};
    WalkabilityGridCell rawGoalCell{};
    WalkabilityGridCell resolvedGoalCell{};
    u32_t rawPathSize = 0u;
    u32_t smoothedPathSize = 0u;
    eMovePathBuildMode mode = eMovePathBuildMode::None;
    bool_t bSuccess = false;
};

struct WalkabilityPathResolveResult
{
    std::wstring path{};
    bool_t bResolved = false;
};

struct WalkabilityAuthoredNavGridLoadResult
{
    std::unique_ptr<Engine::CNavGrid> pGrid{};
    std::wstring path{};
    bool_t bPathResolved = false;
    bool_t bLoadFailed = false;
};

class CWalkabilityAuthority final
{
public:
    static WalkabilityPathResolveResult ResolveWMeshPath();

    static WalkabilityAuthoredNavGridLoadResult TryLoadAuthoredNavGrid(
        const wchar_t* pStagePath);

    static WalkabilityNavGridCoverageResult CheckNavGridCoversStageBounds(
        const Engine::CNavGrid& navGrid,
        const Winters::Map::StageData& stage,
        f32_t padding);

    static bool_t IsWalkableXZ(
        const Engine::CNavGrid* pPathGrid,
        const Engine::CNavGrid* pBaseGrid,
        const Vec3& pos);

    static bool_t SegmentWalkableXZ(
        const Engine::CNavGrid* pBaseGrid,
        const Vec3& from,
        const Vec3& to,
        f32_t radiusWorld);

    static bool_t TryClampMoveSegmentXZ(
        const Engine::CNavGrid* pBaseGrid,
        const Engine::CMapSurfaceSampler* pSurfaceSampler,
        const Vec3& from,
        const Vec3& desired,
        f32_t radiusWorld,
        Vec3& outPosition);

    static bool_t TrySampleHeight(
        const Engine::CMapSurfaceSampler* pSurfaceSampler,
        f32_t x,
        f32_t z,
        f32_t& outY);

    static bool_t TryResolveWalkablePosition(
        const Engine::CNavGrid* pPathGrid,
        const Engine::CNavGrid* pBaseGrid,
        const Engine::CMapSurfaceSampler* pSurfaceSampler,
        const Vec3& rawPos,
        int32_t maxRadius,
        Vec3& outPos);

    static MoveTargetResolveResult ResolveMoveTarget(
        const Engine::CNavGrid* pPathGrid,
        const Engine::CNavGrid* pBaseGrid,
        const Engine::CMapSurfaceSampler* pSurfaceSampler,
        const Vec3& from,
        const Vec3& rawTarget,
        f32_t maxSurfaceDeltaY);

    static MovePathBuildResult BuildMovePath(
        const Engine::CNavGrid* pPathGrid,
        const Engine::CNavGrid* pBaseGrid,
        const Engine::CMapSurfaceSampler* pSurfaceSampler,
        const Vec3& from,
        const Vec3& rawTarget,
        u16_t maxWaypoints,
        f32_t maxSurfaceDeltaY);

private:
    CWalkabilityAuthority() = delete;
};
