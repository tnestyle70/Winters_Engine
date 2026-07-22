#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <algorithm>
#include <cmath>

class CWorld;
struct DamageRequest;
struct DamageResult;
struct TickContext;

namespace IreliaGameSim
{
	inline constexpr f32_t kQBaseDashSpeedFallback = 14.f;
	inline constexpr f32_t kQStopGapFallback = 1.35f;

	inline Vec3 ResolveQDashEndPos(const Vec3& casterPos, const Vec3& targetPos, f32_t dashStopGap)
	{
		const f32_t distance = std::sqrtf(WintersMath::DistanceSqXZ(casterPos, targetPos));
		const f32_t clampedGap = std::min(std::max(0.f, dashStopGap), distance);
		const Vec3 dir = WintersMath::DirectionXZ(casterPos, targetPos, Vec3{});
		return Vec3{
			targetPos.x - dir.x * clampedGap,
			casterPos.y,
			targetPos.z - dir.z * clampedGap
		};
	}

	void RegisterHooks();
	bool_t CanCastBladesurge(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target);
	void Tick(CWorld& world, const TickContext& tc);
	void OnDamageResolved(
		CWorld& world,
		const TickContext& tc,
		const DamageRequest& request,
		const DamageResult& result);
}
