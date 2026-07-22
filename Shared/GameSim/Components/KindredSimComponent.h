#pragma once
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct KindredSimComponent
{
	EntityID markedTarget = NULL_ENTITY;
	u8_t mountingDreadHitCount = 0;
	u8_t uECastRank = 1u;
	f32_t fEMarkRemainingSec = 0.f;
	f32_t fWRemainingSec = 0.f;
	f32_t fWTickAccumulatorSec = 0.f;
	u8_t uWCastRank = 1u;
	Vec3 vWCenter{};
	f32_t fRRemainingSec = 0.f;
	f32_t fRHealAmount = 0.f;
	Vec3 vRCenter{};
};

struct KindredHealthFloorComponent
{
	EntityID sourceEntity = NULL_ENTITY;
	f32_t fRemainingSec = 0.f;
	f32_t fMinHealth = 1.f;
};
