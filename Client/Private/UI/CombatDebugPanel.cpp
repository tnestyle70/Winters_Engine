#include "UI/CombatDebugPanel.h"
#include "Scene/Scene_InGame.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "GameInstance.h"
#include "GameContext.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace UI
{
	void CCombatDebugPanel::Render(CWorld& world, CScene_InGame* pScene)
	{
		if (!pScene || !pScene->IsShowCombatDebug())
			return;

		ImGui::SetNextWindowPos(ImVec2(640.f, 50.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(320.f, 260.f), ImGuiCond_FirstUseEver);

		bool_t bOpen = pScene->IsShowCombatDebug();
		if (!ImGui::Begin("Combat Debug", &bOpen))
		{
			ImGui::End();
			return;
		}
		pScene->SetShowCombatDebug(bOpen);

		using namespace Engine;
		const eChampion champ = CGameInstance::Get()->Get_GameContext().SelectedChampion;
		const char* pChampName = (champ == eChampion::IRELIA) ? "Irelia" :
			(champ == eChampion::YASUO) ? "Yasuo" : "None";

		ImGui::Text("Player : %s", pChampName);
		ImGui::Text("Hovered: %u (team=%d)",
			pScene->GetHoveredEntity(), (int)pScene->GetHoveredTeam());
		ImGui::Text("Last action : %s (%.1fs)",
			pScene->GetLastActionLabel(),
			pScene->GetLastActionTimer() > 0.f ? pScene->GetLastActionTimer() : 0.f);

		const EntityID playerEntity = pScene->GetPlayerEntity();
		if (champ == eChampion::YASUO && playerEntity != NULL_ENTITY
			&& world.HasComponent<YasuoStateComponent>(playerEntity))
		{
			auto& ys = world.GetComponent<YasuoStateComponent>(playerEntity);
			ImGui::Text("Yasuo Q stack: %u  (%.1fs)", ys.qStackCount, ys.qStackTimer);
			ImGui::Text("Yasuo E active: %s (%.2fs)",
				ys.bEActive ? "YES" : "no", ys.eActiveTimer);
		}
		ImGui::Separator();
		ImGui::Text("-- Sylas Target --");
		Vec3 v = pScene->GetSylasTestPos();
		if (ImGui::DragFloat3("Pos", reinterpret_cast<float*>(&v), 0.1f, -100.f, 100.f))
		{
			pScene->SetSylasTestPos(v);
		}
		f32_t fRadius = pScene->GetChampionHitRadius();
		if (ImGui::SliderFloat("ChampionHitRadius", &fRadius, 0.2f, 5.0f))
			pScene->SetChampionHitRadius(fRadius);

		f32_t fHeight = pScene->GetChampionHitHeight();
		if (ImGui::SliderFloat("ChampionHitHeight", &fHeight, 0.5f, 8.0f))
			pScene->SetChampionHitHeight(fHeight);

		if (ImGui::Button("Near Base"))
			pScene->SetSylasTestPos({ 0.f, 3.f, 6.f });
		ImGui::SameLine();
		if (ImGui::Button("Near Blue Nexus"))
			pScene->SetSylasTestPos({ -55.f, 3.f, -55.f });
		ImGui::SameLine();
		if (ImGui::Button("Reset"))
			pScene->SetSylasTestPos({ -9.f, 3.f, 0.f });

		ImGui::End();
	}
}
