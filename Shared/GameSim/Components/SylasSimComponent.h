#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct SylasSimComponent
{
	u8_t passiveStacks = 0;
	f32_t passiveRemainingSec = 0.f;
};

struct SylasDashComponent
{
	Vec3 vStart{};
	Vec3 vEnd{};
	f32_t fElapsedSec = 0.f;
	f32_t fDurationSec = 0.18f;
};
