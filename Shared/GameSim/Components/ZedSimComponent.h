#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

struct ZedShadowState
{
	bool_t bActive = false;
	u8_t reservedAlignment[3]{};
	Vec3 vPosition{};
	Vec3 vDirection{ 0.f, 0.f, 1.f };
	f32_t fRemainingSec = 0.f;
};
static_assert(sizeof(ZedShadowState) == 32u);

struct ZedSimComponent
{
	ZedShadowState wShadow{};
	ZedShadowState rShadow{};
};
static_assert(sizeof(ZedSimComponent) == 64u);

struct ZedVanishComponent
{
	f32_t fRemainingSec = 0.f;
};

struct ZedDeathMarkComponent
{
    EntityID entitySource = NULL_ENTITY;
    u8_t rank = 1;
    bool_t bLethalMarkerVisible = false;
    u8_t reservedMarkerAlignment[2]{};
    f32_t fRemainingSec = 0.f;
    f32_t fMissingHealthDamageRatio = 0.f;
};
static_assert(sizeof(ZedDeathMarkComponent) == 16u);
