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
	static constexpr f32_t kChasePathRebuildIntervalSec = 0.20f;
	static constexpr f32_t kPathTargetRefreshDistanceSq = 0.1225f;
	static constexpr f32_t kPathWaypointArriveRadius = 0.35f;
	static constexpr u32_t kPathBuildBudgetPerTick = 4u;
	static constexpr u8_t kBlockedFramesBeforeRepath = 6u;
	static constexpr u8_t kFlowFieldStallFramesBeforePathFallback = 4u;
	static constexpr f32_t kFlowFieldProgressSlackSq = 0.01f;
	static constexpr f32_t kStructureAcquireRangePadding = 0.75f;

	static constexpr f32_t kTargetScanIntervalSec = 0.15f;
	static constexpr u32_t kTargetScanStaggerBuckets = 10u;
	static constexpr u8_t kRangedRoleType = 1u;
	static constexpr f32_t kAttackExitRangePadding = 0.18f;
	static constexpr f32_t kLaneAttackSpeedScale = 0.6f;
	static constexpr f32_t kMeleeAttackWindupSec =
		0.22f / kLaneAttackSpeedScale;
	static constexpr f32_t kRangedAttackWindupSec =
		0.28f / kLaneAttackSpeedScale;
	static constexpr f32_t kAttackRecoverySec =
		0.22f / kLaneAttackSpeedScale;

	// 웨이브 간격/초기 지연/미니언당 지연/공성 주기는 SpawnObject 팩 minionWave 로 이동 (M4).
	// 팩 미장착 시 폴백은 MinionWaveDef 기본값(= 이전 상수와 동일).
	static constexpr f32_t kWaveStartX = 5.0f;
};
