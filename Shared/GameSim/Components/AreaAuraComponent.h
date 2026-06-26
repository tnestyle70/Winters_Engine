#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

enum class eAreaAuraShape : u8_t
{
	Circle, 
	Cone, 
	Box,
};

enum class eAreaAuraApplyMode : u8_t
{
	OwnerOnly, 
	Allies,
	Enemies, 
	All,
};

struct AreaAuraComponent
{
	EntityID  owner = NULL_ENTITY;
	eTeam ownerTeam = eTeam::Neutral;
	eAreaAuraShape shape = eAreaAuraShape::Circle;
	eAreaAuraApplyMode applyMode = eAreaAuraApplyMode::OwnerOnly;

	Vec3 vCenter{};
	Vec3 vDir{ 0.f, 0.f, 1.f };
	f32_t fRadius = 1.f;
	f32_t fHalfWidth = 1.f;
	f32_t fLength = 1.f;

	f32_t fRemainingSec = 0.f;
	f32_t fTickIntervalSec = 0.1f;
	f32_t fTickAccumulatorSec = 0.f;

	StatusEffectApplyDesc status{};
	bool_t bApplyStatus = false;
	bool_t bDestroyOnExpire = true;
};
