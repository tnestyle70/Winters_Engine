#include "Game/ServerMinionWaveRuntime.h"

#include "Game/ServerMinionTuning.h"
#include "Manager/Navigation/NavGrid.h"
#include "Manager/Navigation/Pathfinder.h"
#include "Shared/GameSim/Definitions/StageData.h"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace
{
	constexpr u8_t kLaneTop = static_cast<u8_t>(Winters::Map::eLane::Top);
	constexpr u8_t kLaneMid = static_cast<u8_t>(Winters::Map::eLane::Mid);
	constexpr u8_t kLaneBot = static_cast<u8_t>(Winters::Map::eLane::Bot);
	Vec3 BuildLaneFormationPosition(
		const Vec3& start,
		const Vec3& next,
		f32_t forwardOffset,
		f32_t sideOffset)
	{
		const Vec3 dir = WintersMath::DirectionXZ(
			start,
			next,
			Vec3{ 1.f, 0.f, 0.f },
			std::numeric_limits<f32_t>::epsilon());
		const f32_t dirX = dir.x;
		const f32_t dirZ = dir.z;
		const f32_t rightX = -dirZ;
		const f32_t rightZ = dirX;

		return Vec3{
			start.x + dirX * forwardOffset + rightX * sideOffset,
			start.y,
			start.z + dirZ * forwardOffset + rightZ * sideOffset
		};
	}

	void Trace(const CServerMinionWaveRuntime::TraceCallback& trace, const char* pText)
	{
		if (trace && pText)
			trace(pText);
	}

	constexpr f32_t kSameLaneWaypointDistSq = 0.0001f;
	constexpr int32_t kLaneStartSearchRadius = 16;
	constexpr int32_t kLaneGoalSearchRadius = 96;

	void AppendUniqueLaneWaypoint(std::vector<Vec3>& outWaypoints, const Vec3& waypoint)
	{
		if (!outWaypoints.empty() &&
			WintersMath::DistanceSqXZ(outWaypoints.back(), waypoint) <= kSameLaneWaypointDistSq)
		{
			return;
		}

		outWaypoints.push_back(waypoint);
	}

	void AppendResolvedLaneStart(
		const Engine::CNavGrid& navGrid,
		const Vec3& rawWaypoint,
		std::vector<Vec3>& outWaypoints)
	{
		Engine::CNavGrid::Cell cell = navGrid.WorldToCell(rawWaypoint);
		if (navGrid.IsWalkable(cell.x, cell.y))
		{
			AppendUniqueLaneWaypoint(outWaypoints, rawWaypoint);
			return;
		}

		Engine::CNavGrid::Cell nearest{};
		if (navGrid.TryFindNearestWalkableCell(cell, kLaneStartSearchRadius, nearest))
		{
			Vec3 resolved = navGrid.CellToWorld(nearest.x, nearest.y);
			resolved.y = rawWaypoint.y;
			AppendUniqueLaneWaypoint(outWaypoints, resolved);
			return;
		}

		AppendUniqueLaneWaypoint(outWaypoints, rawWaypoint);
	}

	std::vector<Engine::CNavGrid::Cell> SmoothLanePathCells(
		const Engine::CNavGrid& navGrid,
		const std::vector<Engine::CNavGrid::Cell>& path)
	{
		if (path.size() <= 2u)
			return path;

		std::vector<Engine::CNavGrid::Cell> smoothed{};
		smoothed.reserve(path.size());
		smoothed.push_back(path.front());

		size_t anchor = 0u;
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

	bool_t TryAppendResolvedLaneSegment(
		const Engine::CNavGrid& navGrid,
		const Vec3& from,
		const Vec3& to,
		std::vector<Vec3>& outWaypoints)
	{
		Engine::CNavGrid::Cell start = navGrid.WorldToCell(from);
		if (!navGrid.IsWalkable(start.x, start.y))
		{
			Engine::CNavGrid::Cell nearestStart{};
			if (!navGrid.TryFindNearestWalkableCell(start, kLaneStartSearchRadius, nearestStart))
				return false;
			start = nearestStart;
		}

		const Engine::CNavGrid::Cell rawGoal = navGrid.WorldToCell(to);
		if (!navGrid.IsInBounds(rawGoal.x, rawGoal.y))
			return false;

		if (navGrid.IsWalkable(rawGoal.x, rawGoal.y) &&
			navGrid.SegmentWalkable(from, to, 0.f))
		{
			AppendUniqueLaneWaypoint(outWaypoints, to);
			return true;
		}

		Engine::CNavGrid::Cell resolvedGoal{};
		std::vector<Engine::CNavGrid::Cell> path{};
		if (!Engine::CPathfinder::TryFindNearestReachableGoal(
			&navGrid,
			start,
			rawGoal,
			kLaneGoalSearchRadius,
			resolvedGoal,
			&path))
		{
			return false;
		}

		const std::vector<Engine::CNavGrid::Cell> smoothed = SmoothLanePathCells(navGrid, path);
		for (size_t i = 1u; i < smoothed.size(); ++i)
		{
			Vec3 waypoint = navGrid.CellToWorld(smoothed[i].x, smoothed[i].y);
			waypoint.y = to.y;
			AppendUniqueLaneWaypoint(outWaypoints, waypoint);
		}

		if (smoothed.size() <= 1u)
		{
			Vec3 waypoint = navGrid.CellToWorld(resolvedGoal.x, resolvedGoal.y);
			waypoint.y = to.y;
			AppendUniqueLaneWaypoint(outWaypoints, waypoint);
		}

		return !outWaypoints.empty();
	}

	void BuildResolvedLaneWaypoints(
		const Engine::CNavGrid& navGrid,
		const std::vector<Vec3>& rawWaypoints,
		std::vector<Vec3>& outWaypoints)
	{
		outWaypoints.clear();
		if (rawWaypoints.empty())
			return;

		AppendResolvedLaneStart(navGrid, rawWaypoints.front(), outWaypoints);

		for (size_t i = 1u; i < rawWaypoints.size(); ++i)
		{
			const Vec3 from = outWaypoints.empty() ? rawWaypoints[i - 1u] : outWaypoints.back();
			if (!TryAppendResolvedLaneSegment(navGrid, from, rawWaypoints[i], outWaypoints))
				AppendUniqueLaneWaypoint(outWaypoints, rawWaypoints[i]);
		}
	}
}

void CServerMinionWaveRuntime::Clear()
{
	for (auto& teamRows : m_wayPoints)
	{
		for (auto& laneWaypoints : teamRows)
			laneWaypoints.clear();
	}

	m_flowField.Clear();
	m_nextWaveTick = 0u;
	m_waveIndex = 0u;
	ClearResolvedWaypoints();
}

void CServerMinionWaveRuntime::ClearResolvedWaypoints()
{
	for (auto& teamRows : m_navWayPoints)
	{
		for (auto& laneWaypoints : teamRows)
			laneWaypoints.clear();
	}
}

void CServerMinionWaveRuntime::CacheWaypoints(const Winters::Map::StageData& stage)
{
	for (auto& teamRows : m_wayPoints)
	{
		for (auto& laneWaypoints : teamRows)
			laneWaypoints.clear();
	}
	ClearResolvedWaypoints();

	std::vector<Winters::Map::MinionWaypointEntry> sorted = stage.minionWaypoints;
	std::sort(sorted.begin(), sorted.end(),
		[](const Winters::Map::MinionWaypointEntry& lhs,
			const Winters::Map::MinionWaypointEntry& rhs)
		{
			if (lhs.team != rhs.team)
				return lhs.team < rhs.team;
			if (lhs.lane != rhs.lane)
				return lhs.lane < rhs.lane;
			return lhs.order < rhs.order;
		});

	for (const auto& waypoint : sorted)
	{
		if (waypoint.team >= 2u || waypoint.lane >= 3u)
			continue;

		m_wayPoints[waypoint.team][waypoint.lane].push_back(
			Vec3{ waypoint.px, waypoint.py, waypoint.pz });
	}
}

void CServerMinionWaveRuntime::SanitizeWaypoints(
	const ResolveWalkableCallback& resolveWalkable,
	const TraceCallback& trace)
{
	u32_t corrected = 0u;
	for (u32_t teamIndex = 0u; teamIndex < 2u; ++teamIndex)
	{
		for (u32_t lane = 0u; lane < 3u; ++lane)
		{
			for (Vec3& waypoint : m_wayPoints[teamIndex][lane])
			{
				Vec3 resolved{};
				if (!resolveWalkable || !resolveWalkable(waypoint, 16, resolved))
					continue;

				if (WintersMath::DistanceSqXZ(waypoint, resolved) <= 0.0001f)
					continue;

				waypoint = resolved;
				++corrected;
			}
		}
	}

	char msg[160]{};
	sprintf_s(msg, "[ServerMinionNav] sanitized waypoints corrected=%u\n", corrected);
	Trace(trace, msg);
	ClearResolvedWaypoints();
}

void CServerMinionWaveRuntime::RebuildFlowFields(const Engine::CNavGrid* pGrid)
{
	ClearResolvedWaypoints();

	if (!pGrid)
	{
		m_flowField.Clear();
		return;
	}

	for (u32_t teamIndex = 0u; teamIndex < 2u; ++teamIndex)
	{
		for (u8_t lane = 0u; lane < 3u; ++lane)
			BuildResolvedLaneWaypoints(*pGrid, m_wayPoints[teamIndex][lane], m_navWayPoints[teamIndex][lane]);
	}

	m_flowField.Build(*pGrid, m_navWayPoints);
}

void CServerMinionWaveRuntime::ResetWaveSchedule()
{
	m_nextWaveTick = 0u;
}

void CServerMinionWaveRuntime::ScheduleFirstWave(u64_t currentTick, const TraceCallback& trace)
{
	m_nextWaveTick = currentTick + ServerMinionTuning::kInitialWaveDelayTicks;

	char msg[160]{};
	sprintf_s(msg,
		"[GameRoom] First minion wave scheduled tick=%llu delayTicks=%llu\n",
		static_cast<unsigned long long>(m_nextWaveTick),
		static_cast<unsigned long long>(ServerMinionTuning::kInitialWaveDelayTicks));
	Trace(trace, msg);
}

void CServerMinionWaveRuntime::TickWave(
	u64_t tickIndex,
	const SpawnCallback& spawn)
{
	if (m_nextWaveTick == 0u || tickIndex < m_nextWaveTick)
		return;

	SpawnWave(spawn);
	m_nextWaveTick = tickIndex + ServerMinionTuning::kWaveIntervalTicks;
}

bool_t CServerMinionWaveRuntime::TryResolveFlowDirection(
	eTeam team,
	u8_t lane,
	const Vec3& pos,
	Vec3& outDirection) const
{
	return m_flowField.TryResolveDirection(team, ResolveWaypointLane(team, lane), pos, outDirection);
}

u32_t CServerMinionWaveRuntime::GetWaypointCount(eTeam team, u8_t lane) const
{
	const std::vector<Vec3>& waypoints = GetActiveWaypoints(team, lane);
	return static_cast<u32_t>(waypoints.size());
}

const std::vector<Vec3>& CServerMinionWaveRuntime::GetActiveWaypoints(eTeam team, u8_t lane) const
{
	static const std::vector<Vec3> kEmpty{};

	const u32_t teamIndex = static_cast<u32_t>(team);
	if (teamIndex >= 2u || lane >= 3u)
		return kEmpty;

	const std::vector<Vec3>& resolvedWaypoints = m_navWayPoints[teamIndex][lane];
	if (!resolvedWaypoints.empty())
		return resolvedWaypoints;

	return m_wayPoints[teamIndex][lane];
}

Vec3 CServerMinionWaveRuntime::GetWaypoint(eTeam team, u8_t lane, u32_t index) const
{
	const std::vector<Vec3>& waypoints = GetActiveWaypoints(team, lane);
	if (waypoints.empty())
		return Vec3{};
	if (index >= waypoints.size())
		return waypoints.back();

	return waypoints[index];
}

u8_t CServerMinionWaveRuntime::ResolveWaypointLane(eTeam team, u8_t lane)
{
	if (team != eTeam::Red)
		return lane;

	if (lane == kLaneTop)
		return kLaneBot;
	if (lane == kLaneBot)
		return kLaneTop;

	return lane;
}

void CServerMinionWaveRuntime::SpawnWave(
	const SpawnCallback& spawn)
{
	if (!spawn)
		return;

	struct MinionSpawnSlot
	{
		u8_t role = 0u;
		f32_t forwardOffset = 0.f;
		f32_t sideOffset = 0.f;
	};

	static constexpr u8_t kRoleMelee = 0u;
	static constexpr u8_t kRoleRanged = 1u;
	static constexpr MinionSpawnSlot kSpawnSlots[] =
	{
		{ kRoleMelee, 3.6f, -0.9f },
		{ kRoleMelee, 4.8f, 0.0f },
		{ kRoleMelee, 6.0f, 0.9f },
		{ kRoleRanged, 0.0f, -0.9f },
		{ kRoleRanged, 1.2f, 0.0f },
		{ kRoleRanged, 2.4f, 0.9f },
	};
	static constexpr u8_t kLanes[] = { kLaneTop, kLaneMid, kLaneBot };

	bool_t bHasStageWaypoints = false;
	for (u8_t lane : kLanes)
	{
		bHasStageWaypoints =
			bHasStageWaypoints ||
			GetWaypointCount(eTeam::Blue, ResolveWaypointLane(eTeam::Blue, lane)) > 0u ||
			GetWaypointCount(eTeam::Red, ResolveWaypointLane(eTeam::Red, lane)) > 0u;
	}

	const u32_t laneCount = bHasStageWaypoints
		? static_cast<u32_t>(sizeof(kLanes) / sizeof(kLanes[0]))
		: 1u;

	for (u32_t laneIndex = 0u; laneIndex < laneCount; ++laneIndex)
	{
		const u8_t lane = bHasStageWaypoints ? kLanes[laneIndex] : kLaneMid;
		for (const MinionSpawnSlot& slot : kSpawnSlots)
		{
			Vec3 bluePos{
				ServerMinionTuning::kWaveStartX + slot.forwardOffset,
				1.f,
				slot.sideOffset
			};
			const u8_t blueWaypointLane = ResolveWaypointLane(eTeam::Blue, lane);
			const u32_t blueWaypointCount = GetWaypointCount(eTeam::Blue, blueWaypointLane);
			if (bHasStageWaypoints && blueWaypointCount > 0u)
			{
				const Vec3 start = GetWaypoint(eTeam::Blue, blueWaypointLane, 0u);
				const Vec3 next = GetWaypoint(
					eTeam::Blue,
					blueWaypointLane,
					blueWaypointCount >= 2u ? 1u : 0u);
				bluePos = BuildLaneFormationPosition(
					start,
					next,
					slot.forwardOffset,
					slot.sideOffset);
			}

			Vec3 redPos{
				-ServerMinionTuning::kWaveStartX - slot.forwardOffset,
				1.f,
				slot.sideOffset
			};
			const u8_t redWaypointLane = ResolveWaypointLane(eTeam::Red, lane);
			const u32_t redWaypointCount = GetWaypointCount(eTeam::Red, redWaypointLane);
			if (bHasStageWaypoints && redWaypointCount > 0u)
			{
				const Vec3 start = GetWaypoint(eTeam::Red, redWaypointLane, 0u);
				const Vec3 next = GetWaypoint(
					eTeam::Red,
					redWaypointLane,
					redWaypointCount >= 2u ? 1u : 0u);
				redPos = BuildLaneFormationPosition(
					start,
					next,
					slot.forwardOffset,
					slot.sideOffset);
			}

			spawn(SpawnRequest{ eTeam::Blue, slot.role, lane, bluePos });
			spawn(SpawnRequest{ eTeam::Red, slot.role, lane, redPos });
		}
	}

	++m_waveIndex;
}
