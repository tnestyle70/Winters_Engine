#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
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

struct SylasQExplosionComponent
{
	EntityHandle hCaster = NULL_ENTITY_HANDLE;
	Vec3 vCenter{};
	u64_t uDetonateTick = 0u;
	u8_t uRank = 1u;
};
