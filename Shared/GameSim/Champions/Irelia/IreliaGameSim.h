#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

class CWorld;
struct TickContext;

namespace IreliaGameSim
{
	inline Vec3 ResolveQDashEndPos(const Vec3& casterPos, const Vec3& targetPos, f32_t dashStopGap)
	{
		const Vec3 dir = WintersMath::DirectionXZ(casterPos, targetPos, Vec3{});
		return Vec3{
			targetPos.x - dir.x * dashStopGap,
			casterPos.y,
			targetPos.z - dir.z * dashStopGap
		};
	}

	void RegisterHooks();
	void Tick(CWorld& world, const TickContext& tc);
}
