#pragma once

#include "ECS/Entity.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Definitions/SkillTypes.h"
#include "WintersTypes.h"

class CScene_InGame;
class CWorld;

namespace UI
{
	struct AttackSpeedPlaybackScales
	{
		f32_t fAttackSpeedScale = 1.f;
		f32_t fAnimCorrectionScale = 1.f;
	};

	constexpr bool_t IsAttackSpeedPlaybackAction(
		u16_t actionId,
		u8_t sourceSlot)
	{
		const eActionStateId id = static_cast<eActionStateId>(actionId);
		if (id == eActionStateId::BasicAttack)
			return true;
		if (sourceSlot != static_cast<u8_t>(eSkillSlot::BasicAttack))
			return false;
		return id == eActionStateId::SkillQ ||
			id == eActionStateId::SkillW ||
			id == eActionStateId::SkillE ||
			id == eActionStateId::SkillR;
	}

	static_assert(IsAttackSpeedPlaybackAction(
		static_cast<u16_t>(eActionStateId::SkillW),
		static_cast<u8_t>(eSkillSlot::BasicAttack)));
	static_assert(!IsAttackSpeedPlaybackAction(
		static_cast<u16_t>(eActionStateId::SkillW),
		static_cast<u8_t>(eSkillSlot::W)));

	AttackSpeedPlaybackScales ResolveAttackSpeedPlaybackScales(
		const CWorld& world,
		EntityID entity);

	class CAttackSpeedLab
	{
	public:
		static void Open();
		static void Render(CScene_InGame* pScene);
		static void ResetRuntime();
		static void OnEntityRemoved(
			const CWorld& world,
			EntityID entity);
	};
}
