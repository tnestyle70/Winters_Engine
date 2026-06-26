#pragma once
#include "ECS/Entity.h"
#include "../Definitions/LoLMatchContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct LeeSinSimComponent
{
};

struct LeeSinQMarkComponent
{
	EntityID sourceEntity = NULL_ENTITY;
	f32_t fRemainingSec = 3.f;
};

struct LeeSinWardOwnerComponent
{
	EntityID owner = NULL_ENTITY;
};

struct LeeSinDashComponent
{
	Vec3 vStart{};
	Vec3 vEnd{};
	f32_t fElapsedSec = 0.f;
	f32_t fDurationSec = 0.18f;
};
