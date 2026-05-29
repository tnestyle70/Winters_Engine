#include "Game/ServerMinionFlowField.h"

#include "Manager/Navigation/NavGrid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    constexpr f32_t kLaneCenterPullStartDistanceSq = 0.35f * 0.35f;
    constexpr f32_t kLaneCenterPullWeight = 0.75f;

    Vec3 ResolveSegmentForward(
        const std::vector<Vec3>& waypoints, size_t segmentIndex, f32_t t)
    {
        if (segmentIndex + 1u >= waypoints.size())
            return Vec3{};

        if (t > 0.85f && segmentIndex + 2u < waypoints.size())
        {
            return WintersMath::NormalizeXZOrZero(Vec3{
                waypoints[segmentIndex + 2u].x - waypoints[segmentIndex + 1u].x,
                0.f,
                waypoints[segmentIndex + 2u].z - waypoints[segmentIndex + 1u].z },
                std::numeric_limits<f32_t>::epsilon());
        }

        return WintersMath::NormalizeXZOrZero(Vec3{
            waypoints[segmentIndex + 1u].x - waypoints[segmentIndex].x,
            0.f,
            waypoints[segmentIndex + 1u].z - waypoints[segmentIndex].z },
            std::numeric_limits<f32_t>::epsilon());
    }

    Vec3 ResolveSegmentClosestPoint(
        const std::vector<Vec3>& waypoints, size_t segmentIndex, f32_t t)
    {
        if (segmentIndex + 1u >= waypoints.size())
            return Vec3{};

        const Vec3& start = waypoints[segmentIndex];
        const Vec3& end = waypoints[segmentIndex + 1u];
        return Vec3{
            start.x + (end.x - start.x) * t,
            start.y,
            start.z + (end.z - start.z) * t
        };
    }

    Vec3 ResolveFlowDirection(
        const std::vector<Vec3>& waypoints, size_t segmentIndex, f32_t t, const Vec3& pos)
    {
        const Vec3 forward = ResolveSegmentForward(waypoints, segmentIndex, t);
        if ((forward.x * forward.x + forward.z * forward.z) <= 0.0001f)
            return Vec3{};

        const Vec3 closest = ResolveSegmentClosestPoint(waypoints, segmentIndex, t);
        const Vec3 toCenter{ closest.x - pos.x, 0.f, closest.z - pos.z };
        const f32_t centerDistSq = toCenter.x * toCenter.x + toCenter.z * toCenter.z;
        if (centerDistSq <= kLaneCenterPullStartDistanceSq)
            return forward;

        const Vec3 centerDir = WintersMath::NormalizeXZOrZero(
            toCenter,
            std::numeric_limits<f32_t>::epsilon());

        return WintersMath::NormalizeXZOrZero(Vec3{
            forward.x + centerDir.x * kLaneCenterPullWeight,
            0.f,
            forward.z + centerDir.z * kLaneCenterPullWeight },
            std::numeric_limits<f32_t>::epsilon());
    }
}

void CServerMinionFlowField::Clear()
{
    for (FlowField& field : m_fields)
    {
        field.bReady = false;
        field.originX = 0.f;
        field.originZ = 0.f;
        field.directions.clear();
    }
}

bool_t CServerMinionFlowField::Build(
    const Engine::CNavGrid& navGrid,
    const std::vector<Vec3>(&waypoints)[2][3])
{
    Clear();

    bool_t bAnyBuilt = false;
    for (u32_t teamIndex = 0u; teamIndex < 2u; ++teamIndex)
    {
        for (u8_t lane = 0u; lane < 3u; ++lane)
        {
            const std::vector<Vec3>& laneWaypoints = waypoints[teamIndex][lane];
            if (laneWaypoints.size() < 2u)
                continue;

            FlowField& field = m_fields[teamIndex * 3u + lane];
            field.originX = navGrid.Get_OriginX();
            field.originZ = navGrid.Get_OriginZ();
            field.directions.assign(Engine::CNavGrid::kTotalCells, Vec3{});

            for (u32_t y = 0u; y < Engine::CNavGrid::kCellCountY; ++y)
            {
                for (u32_t x = 0u; x < Engine::CNavGrid::kCellCountX; ++x)
                {
                    if (!navGrid.IsWalkable(static_cast<int32_t>(x), static_cast<int32_t>(y)))
                        continue;

                    const Vec3 pos = navGrid.CellToWorld(static_cast<int32_t>(x), static_cast<int32_t>(y));
                    f32_t bestScore = (std::numeric_limits<f32_t>::max)();
                    size_t bestSegment = 0u;
                    f32_t bestT = 0.f;

                    for (size_t i = 1u; i < laneWaypoints.size(); ++i)
                    {
                        f32_t t = 0.f;
                        const f32_t score = WintersMath::DistanceSqPointToSegmentXZ(
                            pos,
                            laneWaypoints[i - 1u],
                            laneWaypoints[i],
                            &t,
                            std::numeric_limits<f32_t>::epsilon());
                        if (score < bestScore)
                        {
                            bestScore = score;
                            bestSegment = i - 1u;
                            bestT = t;
                        }
                    }

                    field.directions[y * Engine::CNavGrid::kCellCountX + x] =
                        ResolveFlowDirection(laneWaypoints, bestSegment, bestT, pos);
                }
            }

            field.bReady = true;
            bAnyBuilt = true;
        }
    }

    return bAnyBuilt;
}

bool_t CServerMinionFlowField::TryResolveDirection(
    eTeam team,
    u8_t lane,
    const Vec3& pos,
    Vec3& outDirection) const
{
    outDirection = Vec3{};

    const u32_t index = ResolveFieldIndex(team, lane);
    if (index >= m_fields.size())
        return false;

    const FlowField& field = m_fields[index];
    if (!field.bReady || field.directions.size() != Engine::CNavGrid::kTotalCells)
        return false;

    const int32_t cellX = static_cast<int32_t>(
        std::floor((pos.x - field.originX) / Engine::CNavGrid::kCellSize));
    const int32_t cellY = static_cast<int32_t>(
        std::floor((pos.z - field.originZ) / Engine::CNavGrid::kCellSize));

    if (cellX < 0 ||
        cellY < 0 ||
        cellX >= static_cast<int32_t>(Engine::CNavGrid::kCellCountX) ||
        cellY >= static_cast<int32_t>(Engine::CNavGrid::kCellCountY))
    {
        return false;
    }

    outDirection = field.directions[
        static_cast<u32_t>(cellY) * Engine::CNavGrid::kCellCountX +
        static_cast<u32_t>(cellX)];
    return (outDirection.x * outDirection.x + outDirection.z * outDirection.z) > 0.0001f;
}

bool_t CServerMinionFlowField::HasField(eTeam team, u8_t lane) const
{
    const u32_t index = ResolveFieldIndex(team, lane);
    return index < m_fields.size() && m_fields[index].bReady;
}

u32_t CServerMinionFlowField::ResolveFieldIndex(eTeam team, u8_t lane)
{
    const u32_t teamIndex = static_cast<u32_t>(team);
    if (teamIndex >= 2u || lane >= 3u)
        return kFieldCount;

    return teamIndex * 3u + lane;
}
