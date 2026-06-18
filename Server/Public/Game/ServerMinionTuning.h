#pragma once

#include "WintersTypes.h"

// Server-authoritative minion knobs.
// Client ImGui may display these through debug readout, but must not mutate gameplay truth directly.
struct ServerMinionTuning final
{
	ServerMinionTuning() = delete;

	static constexpr f32_t kPathAgentRadius = 0.5f;
	static constexpr f32_t kMinionLaneClearanceRadius = 0.5f;
	static constexpr f32_t kMinionSoftSeparationRadiusScale = 0.65f;
	static constexpr f32_t kMinionSoftSeparationWeight = 0.35f;
	static constexpr f32_t kMinionSoftSeparationMaxStep = 0.18f;
	static constexpr f32_t kLanePathRebuildIntervalSec = 1.00f;
	static constexpr f32_t kChasePathRebuildIntervalSec = 0.60f;
	static constexpr f32_t kPathTargetRefreshDistanceSq = 1.0f;
	static constexpr f32_t kPathWaypointArriveRadius = 0.35f;
	static constexpr u32_t kPathBuildBudgetPerTick = 4u;
	static constexpr u8_t kBlockedFramesBeforeRepath = 6u;
	static constexpr u8_t kFlowFieldStallFramesBeforePathFallback = 4u;
	static constexpr f32_t kFlowFieldProgressSlackSq = 0.01f;
	static constexpr f32_t kStructureAcquireRangePadding = 0.75f;

	static constexpr f32_t kTargetScanIntervalSec = 0.50f;
	static constexpr u32_t kTargetScanStaggerBuckets = 10u;

	static constexpr u64_t kWaveIntervalTicks = 900u;
	static constexpr u64_t kInitialWaveDelayTicks = 300u;
	// 웨이브 내 미니언을 한 마리씩 시간차로 내보내는 간격 (30Hz 기준 10틱 ≈ 0.33초)
	static constexpr u64_t kPerMinionSpawnDelayTicks = 10u;
	static constexpr f32_t kWaveStartX = 5.0f;
};
