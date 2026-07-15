#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct ZedSimComponent
{
	bool_t bShadowActive = false;
	u8_t reservedShadowAlignment[3]{};
	Vec3 vShadowPosition{};
	Vec3 vShadowDirection{ 0.f, 0.f, 1.f };
	f32_t fShadowRemainingSec = 0.f;
};
static_assert(sizeof(ZedSimComponent) == 32u);

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
