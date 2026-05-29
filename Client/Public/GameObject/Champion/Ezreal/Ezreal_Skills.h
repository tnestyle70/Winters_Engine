#pragma once

#include "GamePlay/SkillHookContext.h"
#include "GamePlay/VisualHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"

namespace Ezreal
{
	void OnCastFrame_BA(SkillHookContext& ctx);
	void OnCastFrame_Q(SkillHookContext& ctx);
	void OnCastFrame_W(SkillHookContext& ctx);
	void OnCastFrame_R(SkillHookContext& ctx);

	void OnCastAccepted_E(SkillHookContext& ctx);
	void OnKeySwap_E(SkillHookContext& ctx);

	namespace Gameplay
	{
		void OnCastFrame_BA(GameplayHookContext& ctx);
		void OnCastFrame_Q(GameplayHookContext& ctx);
		void OnCastFrame_W(GameplayHookContext& ctx);
		void OnCastFrame_R(GameplayHookContext& ctx);
		void OnCastAccepted_E(GameplayHookContext& ctx);
	}

	namespace Visual
	{
		void OnKeySwap_E(VisualHookContext& ctx);
		void OnCastAccepted_E_Visual(VisualHookContext& ctx);
		void OnCastFrame_BA_Visual(VisualHookContext& ctx);
		void OnCastFrame_Q_Visual(VisualHookContext& ctx);
		void OnCastFrame_W_Visual(VisualHookContext& ctx);
		void OnCastFrame_R_Visual(VisualHookContext& ctx);
	}
}

void Ezreal_KeepAlive();
