#pragma once

#include "WintersTypes.h"

// Server-authoritative minion knobs.
// Client ImGui may display these through debug readout, but must not mutate gameplay truth directly.
struct ServerMinionTuning final
{
	ServerMinionTuning() = delete;

	static constexpr f32_t kPathAgentRadius = 0.5f;
	static constexpr f32_t kMinionLaneClearanceRadius = 0.5f;
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
	static constexpr f32_t kWaveStartX = 5.0f;
};
