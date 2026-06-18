#include "UI/ChampionTuner.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace UI
{
	void UI::CChampionTuner::Render(CScene_InGame* pScene)
	{
		if (!pScene)
			return;

		ImGui::SetNextWindowPos(ImVec2(620.f, 260.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(380.f, 180.f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Champion Data"))
		{
			ImGui::End();
			return;
		}

		ImGui::TextDisabled("Gameplay tuning is server-authoritative.");
		ImGui::Separator();
		ImGui::TextWrapped(
			"Scene_InGame champion Get/Set wrappers were removed. "
			"Use server debug override data after the data-center pass.");

		ImGui::End();
	}
}
