#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct ZedSimComponent
{
	bool_t bShadowActive = false;
	Vec3 vShadowPosition{};
	Vec3 vShadowDirection{ 0.f, 0.f, 1.f };
	f32_t fShadowRemainingSec = 0.f;
};

struct ZedVanishComponent
{
	f32_t fRemainingSec = 0.f;
};

struct ZedDeathMarkComponent
{
    EntityID entitySource = NULL_ENTITY;
    u8_t rank = 1;
    f32_t fRemainingSec = 0.f;
    f32_t fMissingHealthDamageRatio = 0.f;
};
