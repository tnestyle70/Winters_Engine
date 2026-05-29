#pragma once
#include "Defines.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "GameContext.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameObject/SkillDef.h"

#include <functional>
#include <string>

class CWorld;
class ModelRenderer;

namespace Engine
{
	class CFxStaticMeshRenderer;
}

struct SkillHookContext
{
	CWorld* pWorld = nullptr;
	EntityID casterEntity = NULL_ENTITY;
	eTeam casterTeam = eTeam::Blue;
	const SkillDef* pDef = nullptr;
	const CastSkillCommand* pCommand = nullptr;
	u8_t skillStage = 1;
	f32_t fDeltaTime = 0.f;
	std::string* pKeyOut = nullptr;
	Engine::CFxStaticMeshRenderer* pFxMeshRenderer = nullptr;
	ModelRenderer* pCasterRenderer = nullptr;
	f32_t fGlobalAnimSpeed = 1.f;
	u32_t actionSeq = 0;
	bool_t bPlayPassiveDashAnimation = true;
	std::function<void(const Vec3&)> startLocalDash{};
	std::function<void(const Vec3&, const Vec3&, f32_t, EntityID)> startPointDash{};
	std::function<void(EntityID)> startTargetDash{};
	std::function<void(EntityID)> startUltimateDash{};
	std::function<EntityID(const Vec3&, f32_t)> findAirborneTarget{};
	std::function<void(EntityID, f32_t)> applyTargetDamage{};
	std::function<void(const char*, const char*, bool_t)> setLocalLoopAnimations{};
	std::function<void(f32_t)> setLocalDashDuration{};
	std::function<f32_t()> getLocalDashDuration{};
	std::function<void(bool_t)> setLocalActionAnimActive{};
};
