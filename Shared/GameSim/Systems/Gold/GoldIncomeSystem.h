#pragma once

#include "WintersTypes.h"

class CWorld;
struct TickContext;

// 패시브 골드는 컴포넌트 없이 tickIndex에서만 파생한다.
// 상태를 추가하지 않아야 WKF1 키프레임 레이아웃이 보존된다.
inline constexpr u64_t kPassiveGoldStartTick = 3300ull;         // 110s * 30Hz (LoL 1:50)
inline constexpr u64_t kPassiveGoldGrantIntervalTicks = 30ull;  // 1s
inline constexpr u32_t kPassiveGoldPerGrant = 2u;               // ~2.0g/s (LoL 2.04g/s)

class CGoldIncomeSystem
{
public:
	static void Execute(CWorld& world, const TickContext& tc);

private:
	CGoldIncomeSystem() = delete;
};
