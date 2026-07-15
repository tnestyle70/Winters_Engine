#include "UI/StructureTunerPanel.h"

// winsock2(ClientNetwork.h) 는 Windows.h 를 transit include 하는 Scene_InGame.h 보다 먼저.
#include "Network/Client/ClientNetwork.h"
#include "Network/Client/CommandSerializer.h"
#include "Scene/Scene_InGame.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/StructureGameDef.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <algorithm>
#include <string>

namespace UI
{
	namespace
	{
		struct StructureTunerState
		{
			// 기본값 = Data/LoL/ServerPrivate/Gameplay/SpawnObjectGameplayDefs.json 의 정의 팩 값
			f32_t fTurretHp = 3000.f;
			f32_t fInhibitorHp = 4000.f;
			f32_t fNexusHp = 5500.f;
			f32_t fTurretDamage = 150.f;
			std::string strStatus;
		};
		StructureTunerState g_State{};

		constexpr f32_t kStructureHpMin = 1.f;
		constexpr f32_t kStructureHpMax = 1000000.f;
		constexpr f32_t kTurretDamageMin = 1.f;
		constexpr f32_t kTurretDamageMax = 100000.f;

		bool_t CanSendToServer(CScene_InGame* pScene)
		{
			if (!pScene || !pScene->IsNetworkAuthoritativeGameplay())
				return false;
			CClientNetwork* pNetwork = pScene->GetNetworkView();
			return pScene->GetCommandSerializer() && pNetwork && pNetwork->IsConnected();
		}

		u32_t SendPractice(
			CScene_InGame* pScene,
			ePracticeOperation operation,
			f32_t value,
			u8_t slot)
		{
			return pScene->GetCommandSerializer()->SendPracticeControl(
				*pScene->GetNetworkView(),
				operation,
				value,
				0u,
				slot);
		}

		void SendStructureStat(
			CScene_InGame* pScene,
			eStructureStatOverrideId statId,
			f32_t value)
		{
			SendPractice(
				pScene,
				ePracticeOperation::ApplyStructureStatOverride,
				value,
				static_cast<u8_t>(statId));
		}

		void ApplyAllToServer(CScene_InGame* pScene)
		{
			g_State.fTurretHp = std::clamp(g_State.fTurretHp, kStructureHpMin, kStructureHpMax);
			g_State.fInhibitorHp = std::clamp(g_State.fInhibitorHp, kStructureHpMin, kStructureHpMax);
			g_State.fNexusHp = std::clamp(g_State.fNexusHp, kStructureHpMin, kStructureHpMax);
			g_State.fTurretDamage =
				std::clamp(g_State.fTurretDamage, kTurretDamageMin, kTurretDamageMax);

			SendPractice(pScene, ePracticeOperation::SetEnabled, 1.f, 0u);
			SendStructureStat(pScene, eStructureStatOverrideId::TurretMaxHp, g_State.fTurretHp);
			SendStructureStat(pScene, eStructureStatOverrideId::InhibitorMaxHp, g_State.fInhibitorHp);
			SendStructureStat(pScene, eStructureStatOverrideId::NexusMaxHp, g_State.fNexusHp);
			SendStructureStat(
				pScene, eStructureStatOverrideId::TurretAttackDamage, g_State.fTurretDamage);
			g_State.strStatus = "Applied - snapshot HP bars will confirm";
		}

		struct KindSummary
		{
			u32_t uAlive = 0u;
			u32_t uDead = 0u;
			f32_t fMinHp = 0.f;
			f32_t fMaxHp = 0.f;
		};

		void DrawLiveSummary(CWorld& world)
		{
			KindSummary summaries[3]{};
			world.ForEach<StructureComponent>(
				[&](EntityID, StructureComponent& structure)
				{
					if (structure.kind > static_cast<u32_t>(eStructureKind::Nexus))
						return;
					KindSummary& summary = summaries[structure.kind];
					if (structure.hp <= 0.f)
					{
						++summary.uDead;
						return;
					}
					if (summary.uAlive == 0u || structure.hp < summary.fMinHp)
						summary.fMinHp = structure.hp;
					summary.fMaxHp = (std::max)(summary.fMaxHp, structure.maxHp);
					++summary.uAlive;
				});

			static constexpr const char* kKindLabels[3] = { "Turret", "Inhibitor", "Nexus" };
			if (ImGui::BeginTable("StructureLive", 4, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Kind");
				ImGui::TableSetupColumn("Alive");
				ImGui::TableSetupColumn("Dead");
				ImGui::TableSetupColumn("HP (min / max)");
				ImGui::TableHeadersRow();
				for (u32_t kind = 0u; kind < 3u; ++kind)
				{
					const KindSummary& summary = summaries[kind];
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(kKindLabels[kind]);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%u", summary.uAlive);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%u", summary.uDead);
					ImGui::TableSetColumnIndex(3);
					if (summary.uAlive > 0u)
						ImGui::Text("%.0f / %.0f", summary.fMinHp, summary.fMaxHp);
					else
						ImGui::TextUnformatted("-");
				}
				ImGui::EndTable();
			}
		}
	}

	void CStructureTunerPanel::Render(CWorld& world, CScene_InGame* pScene)
	{
		if (!pScene)
			return;

		ImGui::SetNextWindowSize(ImVec2(360.f, 340.f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Structure Tuner (F4)"))
		{
			ImGui::End();
			return;
		}

		ImGui::TextUnformatted("Live structures (replicated)");
		DrawLiveSummary(world);
		ImGui::Separator();

		ImGui::TextUnformatted("Override values");
		ImGui::InputFloat("Turret Max HP", &g_State.fTurretHp, 100.f, 500.f, "%.0f");
		ImGui::InputFloat("Inhibitor Max HP", &g_State.fInhibitorHp, 100.f, 500.f, "%.0f");
		ImGui::InputFloat("Nexus Max HP", &g_State.fNexusHp, 100.f, 500.f, "%.0f");
		ImGui::InputFloat("Turret Attack Damage", &g_State.fTurretDamage, 10.f, 50.f, "%.0f");
		ImGui::Spacing();

		const bool_t bCanSend = CanSendToServer(pScene);
		if (!bCanSend)
			ImGui::BeginDisabled();

		if (ImGui::Button("Apply To Server"))
			ApplyAllToServer(pScene);
		ImGui::SameLine();
		if (ImGui::Button("Low HP Preset"))
		{
			// 파괴 파이프라인 검증용 저체력 프리셋 (기본값의 1/10)
			g_State.fTurretHp = 300.f;
			g_State.fInhibitorHp = 400.f;
			g_State.fNexusHp = 550.f;
			ApplyAllToServer(pScene);
		}
		ImGui::SameLine();
		if (ImGui::Button("Restore Defaults"))
		{
			g_State.fTurretHp = 3000.f;
			g_State.fInhibitorHp = 4000.f;
			g_State.fNexusHp = 5500.f;
			g_State.fTurretDamage = 150.f;
			SendPractice(pScene, ePracticeOperation::SetEnabled, 1.f, 0u);
			SendPractice(pScene, ePracticeOperation::ClearStructureStatOverrides, 0.f, 0u);
			g_State.strStatus = "Restored to definition pack defaults";
		}

		if (!bCanSend)
		{
			ImGui::EndDisabled();
			ImGui::TextDisabled("Requires authoritative server session (Debug server, host only)");
		}

		if (!g_State.strStatus.empty())
			ImGui::TextUnformatted(g_State.strStatus.c_str());

		ImGui::End();
	}
}
