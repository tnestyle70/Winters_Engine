#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

namespace IreliaGameSim
{
	inline constexpr f32_t kQDashStopGap = 1.35f;

	inline Vec3 ResolveQDashEndPos(const Vec3& casterPos, const Vec3& targetPos)
	{
		const Vec3 dir = WintersMath::DirectionXZ(casterPos, targetPos, Vec3{});
		return Vec3{
			targetPos.x - dir.x * kQDashStopGap,
			casterPos.y,
			targetPos.z - dir.z * kQDashStopGap
		};
	}

	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
}
