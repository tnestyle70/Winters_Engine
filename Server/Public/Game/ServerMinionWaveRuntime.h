#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Game/ServerMinionFlowField.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <functional>
#include <vector>

namespace Engine
{
	class CNavGrid;
}

namespace Winters::Map
{
	struct StageData;
}

class CServerMinionWaveRuntime final
{
public:
	struct SpawnRequest
	{
		eTeam team = eTeam::Blue;
		u8_t roleType = 0u;
		u8_t lane = 0u;
		Vec3 pos{};
	};

	using SpawnCallback = std::function<void(const SpawnRequest&)>;
	using TraceCallback = std::function<void(const char*)>;
	using ResolveWalkableCallback = std::function<bool_t(const Vec3&, int32_t, Vec3&)>;

	void Clear();
	void CacheWaypoints(const Winters::Map::StageData& stage);
	void SanitizeWaypoints(const ResolveWalkableCallback& resolveWalkable,
		const TraceCallback& trace);
	void RebuildFlowFields(const Engine::CNavGrid* pGrid);
	void ResetWaveSchedule();
	void ScheduleFirstWave(u64_t currentTick, const TraceCallback& trace);
	void TickWave(u64_t tickIndex, const SpawnCallback& spawn);
	
	bool_t TryResolveFlowDirection(eTeam team, u8_t lane, const Vec3& pos, Vec3& outDirection) const;
	u32_t GetWaypointCount(eTeam team, u8_t lane) const;
	Vec3 GetWaypoint(eTeam team, u8_t lane, u32_t index) const;
	u32_t GetWaveIndex() const { return m_waveIndex; }

	static u8_t ResolveWaypointLane(eTeam team, u8_t lane);

	struct PendingSpawn
	{
		u64_t dueTick = 0u;
		SpawnRequest request{};
	};

	// Chrono Break: 틱 스케줄 상태만 왕복. waypoints/flowfield는 부트타임 파생 데이터라 제외.
	struct WaveState
	{
		u64_t nextWaveTick = 0u;
		u32_t waveIndex = 0u;
		std::vector<PendingSpawn> pendingSpawns{};
	};
	WaveState CaptureWaveState() const
	{
		return WaveState{ m_nextWaveTick, m_waveIndex, m_pendingSpawns };
	}
	void RestoreWaveState(const WaveState& state)
	{
		m_nextWaveTick = state.nextWaveTick;
		m_waveIndex = state.waveIndex;
		m_pendingSpawns = state.pendingSpawns;
	}

private:

	void EnqueueWave(u64_t tickIndex);
	const std::vector<Vec3>& GetActiveWaypoints(eTeam team, u8_t lane) const;
	void ClearResolvedWaypoints();

	u64_t m_nextWaveTick = 0u;
	u32_t m_waveIndex = 0u;
	std::vector<Vec3> m_wayPoints[2][3];
	std::vector<Vec3> m_navWayPoints[2][3];
	std::vector<PendingSpawn> m_pendingSpawns;
	CServerMinionFlowField m_flowField{};
};
