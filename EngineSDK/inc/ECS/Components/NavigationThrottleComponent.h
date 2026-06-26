#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

struct NavRepathThrottleComponent
{
	Vec3 vLastAcceptedTarget{};
	f32_t fCooldownRemaining = 0.f;
	f32_t fMinRepathInterval = 0.5f;
	f32_t fTargetMoveThreshold = 1.5f;
	bool_t bHasAcceptedTarget = false;
};
