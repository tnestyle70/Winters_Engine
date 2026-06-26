#include "Network/Client/ClientNetwork.h"
#include "UI/AIDebugPanel.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Network/Client/CommandSerializer.h"
#include "Scene/Scene_InGame.h"
#include "Manager/Navigation/NavGrid.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <cstdio>

namespace
{
	const char* TeamName(eTeam team)
	{
		switch (team)
		{
		case eTeam::Blue: return "Blue";
		case eTeam::Red: return "Red";
		case eTeam::Neutral: return "Neutral";
		default: return "Unknown";
		}
	}

	const char* ChampionName(eChampion champion)
	{
		switch (champion)
		{
		case eChampion::IRELIA: return "Irelia";
		case eChampion::YASUO: return "Yasuo";
		case eChampion::KALISTA: return "Kalista";
		case eChampion::SYLAS: return "Sylas";
		case eChampion::VIEGO: return "Viego";
		case eChampion::ANNIE: return "Annie";
		case eChampion::ASHE: return "Ashe";
		case eChampion::FIORA: return "Fiora";
		case eChampion::GAREN: return "Garen";
		case eChampion::RIVEN: return "Riven";
		case eChampion::ZED: return "Zed";
		case eChampion::EZREAL: return "Ezreal";
		case eChampion::YONE: return "Yone";
		case eChampion::JAX: return "Jax";
		case eChampion::MASTERYI: return "MasterYi";
		case eChampion::KINDRED: return "Kindred";
		case eChampion::LEESIN: return "LeeSin";
		default: return "Unknown";
		}
	}

	const char* ChampionAIStateName(eChampionAIState state)
	{
		switch (state)
		{
		case eChampionAIState::MoveToOuterTurret: return "MoveToOuterTurret";
		case eChampionAIState::WaitForWave: return "WaitForWave";
		case eChampionAIState::LaneCombat: return "LaneCombat";
		case eChampionAIState::Diving: return "Diving";
		case eChampionAIState::Retreat: return "Retreat";
		case eChampionAIState::Recalling: return "Recalling";
		case eChampionAIState::Dead: return "Dead";
		default: return "Unknown";
		}
	}

	const char* ChampionAIActionName(eChampionAIAction action)
	{
		switch (action)
		{
		case eChampionAIAction::MoveToSafeAnchor: return "MoveToSafeAnchor";
		case eChampionAIAction::FollowWave: return "FollowWave";
		case eChampionAIAction::AttackMinion: return "AttackMinion";
		case eChampionAIAction::AttackChampion: return "AttackChampion";
		case eChampionAIAction::AttackStructure: return "AttackStructure";
		case eChampionAIAction::UseFlashEscape: return "UseFlashEscape";
		case eChampionAIAction::Retreat: return "Retreat";
		case eChampionAIAction::Recall: return "Recall";
		default: return "Unknown";
		}
	}

	const char* ChampionAIIntentName(eChampionAIIntent intent)
	{
		switch (intent)
		{
		case eChampionAIIntent::FarmMinion: return "FarmMinion";
		case eChampionAIIntent::AttackChampion: return "AttackChampion";
		case eChampionAIIntent::ExecuteDive: return "ExecuteDive";
		case eChampionAIIntent::SiegeStructure: return "SiegeStructure";
		case eChampionAIIntent::Retreat: return "Retreat";
		case eChampionAIIntent::Recall: return "Recall";
		default: return "Unknown";
		}
	}

	const char* ChampionAIDivePhaseName(eChampionAIDivePhase phase)
	{
		switch (phase)
		{
		case eChampionAIDivePhase::None: return "None";
		case eChampionAIDivePhase::EngageQ: return "EngageQ";
		case eChampionAIDivePhase::ArmW: return "ArmW";
		case eChampionAIDivePhase::BasicAttack: return "BasicAttack";
		case eChampionAIDivePhase::ExtraBasicAttack: return "ExtraBasicAttack";
		case eChampionAIDivePhase::FlashExit: return "FlashExit";
		case eChampionAIDivePhase::ExitMove: return "ExitMove";
		default: return "Unknown";
		}
	}

	const char* ChampionAICommandKindName(u8_t kind)
	{
		switch (kind)
		{
		case 1u: return "Move";
		case 2u: return "CastSkill";
		case 3u: return "BasicAttack";
		case 7u: return "Recall";
		case 10u: return "Flash";
		default: return "None";
		}
	}

	const char* ChampionAIBlockReasonName(eChampionAIDecisionBlockReason reason)
	{
		switch (reason)
		{
		case eChampionAIDecisionBlockReason::None: return "None";
		case eChampionAIDecisionBlockReason::NoTarget: return "NoTarget";
		case eChampionAIDecisionBlockReason::TargetDead: return "TargetDead";
		case eChampionAIDecisionBlockReason::TargetUntargetable: return "Untargetable";
		case eChampionAIDecisionBlockReason::TargetOutOfRange: return "OutOfRange";
		case eChampionAIDecisionBlockReason::SelfLowHp: return "SelfLowHp";
		case eChampionAIDecisionBlockReason::TurretDanger: return "TurretDanger";
		case eChampionAIDecisionBlockReason::SkillCooldown: return "Cooldown";
		case eChampionAIDecisionBlockReason::FlashNotReady: return "FlashNotReady";
		case eChampionAIDecisionBlockReason::ActionLocked: return "ActionLocked";
		case eChampionAIDecisionBlockReason::StateBlocked: return "StateBlocked";
		case eChampionAIDecisionBlockReason::InvalidPath: return "InvalidPath";
		case eChampionAIDecisionBlockReason::CommandRejected: return "Rejected";
		default: return "Unknown";
		}
	}

	const char* MinionStateName(MinionStateComponent::State state)
	{
		switch (state)
		{
		case MinionStateComponent::Idle: return "Idle";
		case MinionStateComponent::LaneMove: return "LaneMove";
		case MinionStateComponent::Chase: return "Chase";
		case MinionStateComponent::Attack: return "Attack";
		case MinionStateComponent::Dead: return "Dead";
		default: return "Unknown";
		}
	}

	const char* LaneName(u8_t lane)
	{
		switch (lane)
		{
		case 0u: return "Top";
		case 1u: return "MID";
		case 2u: return "Bot";
		default: return "Unknown";
		}
	}

	const char* MinionRoleName(u8_t role)
	{
		switch (role)
		{
		case 0u: return "Melee";
		case 1u: return "Ranged";
		case 2u: return "Siege";
		case 3u: return "Super";
			default: return "Unknown";
		}
	}

	const char* SkillSlotName(u8_t slot)
	{
		switch (slot)
		{
		case 1u: return "Q";
		case 2u: return "W";
		case 3u: return "E";
		case 4u: return "R";
		default: return "-";
		}
	}

	u32_t ChampionAIActionBit(eChampionAIAction action)
	{
		switch (action)
		{
		case eChampionAIAction::MoveToSafeAnchor:
			return kChampionAIActionBitMoveToSafeAnchor;
		case eChampionAIAction::FollowWave:
			return kChampionAIActionBitFollowWave;
		case eChampionAIAction::AttackMinion:
			return kChampionAIActionBitAttackUnit;
		case eChampionAIAction::AttackChampion:
			return kChampionAIActionBitAttackChampion;
		case eChampionAIAction::AttackStructure:
			return kChampionAIActionBitAttackStructure;
		case eChampionAIAction::UseFlashEscape:
			return kChampionAIActionBitUseFlashEscape;
		case eChampionAIAction::Retreat:
			return kChampionAIActionBitRetreat;
		case eChampionAIAction::Recall:
			return 0u;
		default:
			return 0u;
		}
	}

	bool_t HasChampionAIAction(const ChampionAIDebugComponent& debug, eChampionAIAction action)
	{
		const u32_t bit = ChampionAIActionBit(action);
		return bit != 0u && (debug.availableActionMask & bit) != 0u;
	}

	bool_t HasChampionAISkill(const ChampionAIDebugComponent& debug, u8_t slot)
	{
		if (slot == 0u || slot > 4u)
			return false;
		return (debug.availableSkillMask & (1u << (slot - 1u))) != 0u;
	}

	bool_t CanSendAIDebugCommand(CScene_InGame* pScene)
	{
		if (!pScene || !pScene->IsNetworkAuthoritativeGameplay())
			return false;

		CClientNetwork* pNetwork = pScene->GetNetworkView();
		return pScene->GetCommandSerializer() &&
			pNetwork &&
			pNetwork->IsConnected();
	}

	void RenderActionButton(
		CScene_InGame* pScene,
		u32_t targetNetId,
		const char* pLabel,
		eChampionAIAction action,
		bool_t bAvailable,
		u8_t skillSlot = 0u)
	{
		const bool_t bCanSend =
			bAvailable &&
			targetNetId != NULL_NET_ENTITY &&
			CanSendAIDebugCommand(pScene);
		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button(pLabel))
		{
			pScene->GetCommandSerializer()->SendAIDebugControl(
				*pScene->GetNetworkView(),
				targetNetId,
				action,
				skillSlot);
		}
		ImGui::EndDisabled();
	}

	void RenderForceActionButton(
		CScene_InGame* pScene,
		u32_t targetNetId,
		const char* pLabel,
		eChampionAIAction action)
	{
		const bool_t bCanSend = targetNetId != NULL_NET_ENTITY &&
			CanSendAIDebugCommand(pScene);
		ImGui::BeginDisabled(!bCanSend);
		if (ImGui::Button(pLabel))
		{
			pScene->GetCommandSerializer()->SendAIDebugControl(
				*pScene->GetNetworkView(),
				targetNetId,
				action,
				kChampionAIDebugForceActionSkillSlot);
		}
		ImGui::EndDisabled();
	}

	void RenderTuningSlider(
		CScene_InGame* pScene,
		u32_t targetNetId,
		const char* pLabel,
		eChampionAITuningId tuningId,
		f32_t value,
		f32_t minValue,
		f32_t maxValue)
	{
		const bool_t bCanSend = targetNetId != NULL_NET_ENTITY &&
			CanSendAIDebugCommand(pScene);
		f32_t current = value;
		ImGui::BeginDisabled(!bCanSend);
		ImGui::PushID(static_cast<int>(tuningId));
		if (ImGui::SliderFloat(pLabel, &current, minValue, maxValue, "%.2f"))
		{
			pScene->GetCommandSerializer()->SendAIDebugTune(
				*pScene->GetNetworkView(),
				targetNetId,
				static_cast<u8_t>(tuningId),
				current);
		}
		ImGui::PopID();
		ImGui::EndDisabled();
	}

	NetEntityId s_SelectedAINetId = NULL_NET_ENTITY;
}

void UI::CAIDebugPanel::Render(CWorld& world, CScene_InGame* pScene)
{
	ImGui::SetNextWindowSize(ImVec2(820.f, 560.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("AI Debug"))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Champion AI");
	ImGui::SameLine();
	ImGui::TextDisabled("(F9 AI panel, F10 legacy debug)");
	ImGui::Separator();

	struct SelectedAI
	{
		bool_t bValid = false;
		EntityID entity = NULL_ENTITY;
		ChampionComponent champion{};
		ChampionAIDebugComponent debug{};
		Vec3 pos{};
		f32_t hp = 0.f;
		f32_t maxHp = 0.f;
		u16_t actionId = 0u;
	};

	SelectedAI selectedAI{};
	u32_t botCount = 0;

	if (ImGui::BeginTable(
		"ChampionAISnapshotTable",
		9,
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY,
		ImVec2(0.f, 180.f)))
	{
		ImGui::TableSetupColumn("AI");
		ImGui::TableSetupColumn("Team");
		ImGui::TableSetupColumn("State");
		ImGui::TableSetupColumn("Intent");
		ImGui::TableSetupColumn("Action");
		ImGui::TableSetupColumn("Target");
		ImGui::TableSetupColumn("Pos");
		ImGui::TableSetupColumn("HP");
		ImGui::TableSetupColumn("ActionId");
		ImGui::TableHeadersRow();

		world.ForEach<ChampionComponent, TransformComponent>(
			[&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
			{
				if (!world.HasComponent<ChampionAIDebugComponent>(entity))
					return;

				auto& debug = world.GetComponent<ChampionAIDebugComponent>(entity);
				if (!debug.bPresent)
					return;

				++botCount;
				if (s_SelectedAINetId == NULL_NET_ENTITY)
					s_SelectedAINetId = debug.netId;

				const Vec3 pos = transform.GetLocalPosition();
				const f32_t hp = world.HasComponent<HealthComponent>(entity)
					? world.GetComponent<HealthComponent>(entity).fCurrent
					: champion.hp;
				const f32_t maxHp = world.HasComponent<HealthComponent>(entity)
					? world.GetComponent<HealthComponent>(entity).fMaximum
					: champion.maxHp;
				const u16_t actionId = world.HasComponent<ActionStateComponent>(entity)
					? world.GetComponent<ActionStateComponent>(entity).actionId
					: 0u;
				const bool_t bSelected = debug.netId == s_SelectedAINetId;

				ImGui::PushID(static_cast<int>(entity));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				char label[96]{};
				sprintf_s(label, "%s##%llu",
					ChampionName(champion.id),
					static_cast<unsigned long long>(entity));
				if (ImGui::Selectable(label, bSelected, ImGuiSelectableFlags_SpanAllColumns))
					s_SelectedAINetId = debug.netId;
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(TeamName(champion.team));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIStateName(debug.state));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIIntentName(debug.intent));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIActionName(debug.action));
				ImGui::TableNextColumn();
				ImGui::Text("%u", debug.targetNetId);
				ImGui::TableNextColumn();
				ImGui::Text("%.1f %.1f %.1f", pos.x, pos.y, pos.z);
				ImGui::TableNextColumn();
				ImGui::Text("%.0f / %.0f", hp, maxHp);
				ImGui::TableNextColumn();
				ImGui::Text("%u", static_cast<u32_t>(actionId));
				ImGui::PopID();

				if (bSelected)
				{
					selectedAI.bValid = true;
					selectedAI.entity = entity;
					selectedAI.champion = champion;
					selectedAI.debug = debug;
					selectedAI.pos = pos;
					selectedAI.hp = hp;
					selectedAI.maxHp = maxHp;
					selectedAI.actionId = actionId;
				}
			});

		ImGui::EndTable();
	}

	if (botCount == 0)
	{
		ImGui::TextDisabled(
			"No Champion AI snapshot yet. If bots are visible, check server SnapshotBuilder debug flags.");
	}
	else if (!selectedAI.bValid)
	{
		s_SelectedAINetId = NULL_NET_ENTITY;
	}

	if (selectedAI.bValid)
	{
		const ChampionAIProfile& profile = GetChampionAIProfile(selectedAI.champion.id);
		const auto& debug = selectedAI.debug;
		const bool_t bCanSend = CanSendAIDebugCommand(pScene);

		ImGui::SeparatorText("Selected Champion AI");
		ImGui::Text("%s / %s   entity=%llu   net=%u",
			TeamName(selectedAI.champion.team),
			ChampionName(selectedAI.champion.id),
			static_cast<unsigned long long>(selectedAI.entity),
			debug.netId);
		ImGui::Text("State: %s   Intent: %s   Action: %s   TargetNet: %u",
			ChampionAIStateName(debug.state),
			ChampionAIIntentName(debug.intent),
			ChampionAIActionName(debug.action),
			debug.targetNetId);
		ImGui::Text("HP: %.1f / %.1f   Move: %.2f   Action: %u   Override: %s",
			selectedAI.hp,
			selectedAI.maxHp,
			debug.moveSpeed,
			static_cast<u32_t>(selectedAI.actionId),
			debug.bOverridePending ? "pending" : "none");
		ImGui::Text("Profile range: preferred %.1f   champ %.1f   minion %.1f   structure %.1f   leash %.1f",
			profile.preferredRange,
			profile.championScanRange,
			profile.minionScanRange,
			profile.structureScanRange,
			profile.leashRange);
		ImGui::Text("Scores: champion %.2f   farm %.2f   structure %.2f   canFight=%s   postBA=%s",
			debug.fChampionDecisionScore,
			debug.fFarmDecisionScore,
			debug.fStructureDecisionScore,
			debug.bCanAttackChampion ? "yes" : "no",
			debug.bPostComboBAAllowed ? "yes" : "no");
		ImGui::Text("Perception: champScan %.1f   diveScan %.1f   flash %.1f   lowHP net=%u %.0f%% dist=%.1f",
			debug.fChampionScanRange,
			debug.fDiveScanRange,
			debug.fFlashRange,
			debug.lowHpEnemyNetId,
			debug.fLowHpEnemyRatio * 100.f,
			debug.fLowHpEnemyDistance);
		ImGui::Text("Dive: phase=%s target=%u   block=%s   lastCmd=%s slot=%u target=%u pos=(%.1f, %.1f, %.1f)",
			ChampionAIDivePhaseName(debug.divePhase),
			debug.diveTargetNetId,
			ChampionAIBlockReasonName(debug.lastBlockReason),
			ChampionAICommandKindName(debug.lastCommandKind),
			static_cast<u32_t>(debug.lastCommandSlot),
			debug.lastCommandTargetNetId,
			debug.lastCommandPos.x,
			debug.lastCommandPos.y,
			debug.lastCommandPos.z);
		ImGui::Text("Risk: retreat %.0f%%   reengage %.0f%%   aggression %.2f   kite %.2f",
			profile.retreatHpRatio * 100.f,
			profile.reengageHpRatio * 100.f,
			profile.aggression,
			profile.kiteBias);

		if (!bCanSend)
			ImGui::TextDisabled("Debug control disabled: server-authoritative network session required.");

		ImGui::SeparatorText("Actions");
		RenderActionButton(pScene, debug.netId, "Safe", eChampionAIAction::MoveToSafeAnchor,
			HasChampionAIAction(debug, eChampionAIAction::MoveToSafeAnchor));
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "Wave", eChampionAIAction::FollowWave,
			HasChampionAIAction(debug, eChampionAIAction::FollowWave));
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "Minion", eChampionAIAction::AttackMinion,
			HasChampionAIAction(debug, eChampionAIAction::AttackMinion));
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "Champion", eChampionAIAction::AttackChampion,
			HasChampionAIAction(debug, eChampionAIAction::AttackChampion));
		ImGui::SameLine();
		RenderForceActionButton(pScene, debug.netId, "Force Champion",
			eChampionAIAction::AttackChampion);
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "Structure", eChampionAIAction::AttackStructure,
			HasChampionAIAction(debug, eChampionAIAction::AttackStructure));
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "FlashExit", eChampionAIAction::UseFlashEscape,
			HasChampionAIAction(debug, eChampionAIAction::UseFlashEscape));
		ImGui::SameLine();
		RenderActionButton(pScene, debug.netId, "Retreat", eChampionAIAction::Retreat,
			HasChampionAIAction(debug, eChampionAIAction::Retreat));
		ImGui::SameLine();
		ImGui::BeginDisabled(!bCanSend || debug.netId == NULL_NET_ENTITY);
		if (ImGui::Button("Clear"))
			pScene->GetCommandSerializer()->SendAIDebugClear(*pScene->GetNetworkView(), debug.netId);
		ImGui::EndDisabled();

		ImGui::SeparatorText("Skills");
		for (u8_t slot = 1u; slot <= 4u; ++slot)
		{
			bool_t bRuleFound = false;
			f32_t minRange = 0.f;
			f32_t score = 0.f;
			for (u8_t i = 0; i < profile.skillRuleCount; ++i)
			{
				if (profile.skillRules[i].slot == slot)
				{
					bRuleFound = true;
					minRange = profile.skillRules[i].minRange;
					score = profile.skillRules[i].score;
					break;
				}
			}

			ImGui::PushID(static_cast<int>(slot));
			const bool_t bSkillAvailable =
				HasChampionAIAction(debug, eChampionAIAction::AttackChampion) &&
				HasChampionAISkill(debug, slot);
			RenderActionButton(pScene, debug.netId, SkillSlotName(slot),
				eChampionAIAction::AttackChampion, bSkillAvailable, slot);
			ImGui::SameLine();
			ImGui::TextDisabled("rule=%s  min=%.1f  score=%.2f",
				bRuleFound ? "yes" : "no",
				minRange,
				score);
			ImGui::PopID();
		}

		ImGui::SeparatorText("Runtime Tuning");
		RenderTuningSlider(pScene, debug.netId, "Champion Scan", eChampionAITuningId::ChampionScanRange,
			debug.fChampionScanRange, 1.f, 40.f);
		RenderTuningSlider(pScene, debug.netId, "Minion Scan", eChampionAITuningId::MinionScanRange,
			debug.fMinionScanRange, 1.f, 40.f);
		RenderTuningSlider(pScene, debug.netId, "Structure Scan", eChampionAITuningId::StructureScanRange,
			debug.fStructureScanRange, 1.f, 60.f);
		RenderTuningSlider(pScene, debug.netId, "Leash Range", eChampionAITuningId::LeashRange,
			debug.fLeashRange, 1.f, 60.f);
		RenderTuningSlider(pScene, debug.netId, "Retreat HP", eChampionAITuningId::RetreatHpRatio,
			debug.fRetreatHpRatio, 0.01f, 0.90f);
		RenderTuningSlider(pScene, debug.netId, "Reengage HP", eChampionAITuningId::ReengageHpRatio,
			debug.fReengageHpRatio, 0.01f, 1.f);
		RenderTuningSlider(pScene, debug.netId, "Champion Score Margin", eChampionAITuningId::ChampionScoreMargin,
			debug.fChampionScoreMargin, 0.f, 1.f);
		RenderTuningSlider(pScene, debug.netId, "Turret Danger", eChampionAITuningId::TurretDangerThreshold,
			debug.fTurretDangerThreshold, 0.f, 1.f);
		RenderTuningSlider(pScene, debug.netId, "Post BA Self HP", eChampionAITuningId::PostComboBASelfHpMinRatio,
			debug.fPostComboBASelfHpMinRatio, 0.f, 1.f);
		RenderTuningSlider(pScene, debug.netId, "Post BA Enemy Margin", eChampionAITuningId::PostComboBAEnemyHpMargin,
			debug.fPostComboBAEnemyHpMargin, -1.f, 1.f);
		RenderTuningSlider(pScene, debug.netId, "Post BA Window", eChampionAITuningId::PostComboBAWindow,
			debug.fPostComboBAWindow, 0.f, 5.f);
		RenderTuningSlider(pScene, debug.netId, "Low HP Execute", eChampionAITuningId::LowHpExecuteThreshold,
			debug.fLowHpExecuteThreshold, 0.01f, 0.50f);
		RenderTuningSlider(pScene, debug.netId, "Dive Scan", eChampionAITuningId::DiveScanRange,
			debug.fDiveScanRange, 1.f, 40.f);
		RenderTuningSlider(pScene, debug.netId, "Dive Extra BA Window", eChampionAITuningId::DiveExtraBAWindow,
			debug.fDiveExtraBAWindow, 0.f, 5.f);
		ImGui::BeginDisabled(!bCanSend || debug.netId == NULL_NET_ENTITY);
		if (ImGui::Button("Reset Runtime Tuning"))
			pScene->GetCommandSerializer()->SendAIDebugResetTuning(*pScene->GetNetworkView(), debug.netId);
		ImGui::EndDisabled();

		ImGui::SeparatorText("Decision Trace");
		if (debug.debugDecisionTraceCount == 0u)
		{
			ImGui::TextDisabled("No decision trace rows yet.");
		}
		else if (ImGui::BeginTable(
			"AIDecisionTraceTable",
			8,
			ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_ScrollY,
			ImVec2(0.f, 130.f)))
		{
			ImGui::TableSetupColumn("Tick");
			ImGui::TableSetupColumn("State");
			ImGui::TableSetupColumn("Intent");
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("Block");
			ImGui::TableSetupColumn("Cmd");
			ImGui::TableSetupColumn("Target");
			ImGui::TableSetupColumn("Score");
			ImGui::TableHeadersRow();

			for (int i = static_cast<int>(debug.debugDecisionTraceCount) - 1; i >= 0; --i)
			{
				const ChampionAIDecisionTraceEntry& row = debug.debugDecisionTrace[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%llu", static_cast<unsigned long long>(row.tick));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIStateName(row.state));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIIntentName(row.intent));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIActionName(row.action));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIBlockReasonName(row.blockReason));
				ImGui::TableNextColumn();
				ImGui::Text("%s/%u",
					ChampionAICommandKindName(row.commandKind),
					static_cast<u32_t>(row.commandSlot));
				ImGui::TableNextColumn();
				ImGui::Text("%llu", static_cast<unsigned long long>(row.target));
				ImGui::TableNextColumn();
				ImGui::Text("%.2f/%.2f/%.2f",
					row.championScore,
					row.farmScore,
					row.structureScore);
			}

			ImGui::EndTable();
		}
	}

	ImGui::SeparatorText("Server Minions");
	ImGui::TextDisabled("Snapshot readout only. Server gameplay tuning lives in ServerMinionTuning.");

	const CNavGrid* pMinionDebugGrid = pScene
		? (pScene->GetPathNavGrid() ? pScene->GetPathNavGrid() : pScene->GetNavGrid())
		: nullptr;

	u32_t minionCount = 0u;
	u32_t attackingCount = 0u;
	u32_t movingCount = 0u;

	if (ImGui::BeginTable(
		"ServerMinionSnapshotTable",
		11,
		ImGuiTableFlags_BordersInnerV |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollY,
		ImVec2(0.f, 150.f)))
	{
		ImGui::TableSetupColumn("Team");
		ImGui::TableSetupColumn("Lane");
		ImGui::TableSetupColumn("Role");
		ImGui::TableSetupColumn("State");
		ImGui::TableSetupColumn("Entity");
		ImGui::TableSetupColumn("Pos");
		ImGui::TableSetupColumn("Cell");
		ImGui::TableSetupColumn("Path");
		ImGui::TableSetupColumn("Blocked");
		ImGui::TableSetupColumn("HP");
		ImGui::TableSetupColumn("Move");
		ImGui::TableHeadersRow();

		world.ForEach<MinionStateComponent, TransformComponent>(
			[&](EntityID entity, MinionStateComponent& state, TransformComponent& transform)
			{
				++minionCount;
				if (state.current == MinionStateComponent::Attack)
					++attackingCount;
				if (state.current == MinionStateComponent::LaneMove ||
					state.current == MinionStateComponent::Chase)
				{
					++movingCount;
				}

				const MinionComponent* pMinion = world.HasComponent<MinionComponent>(entity)
					? &world.GetComponent<MinionComponent>(entity)
					: nullptr;
				const Vec3 pos = transform.GetLocalPosition();
				const CNavGrid::Cell cell = pMinionDebugGrid
					? pMinionDebugGrid->WorldToCell(pos)
					: CNavGrid::Cell{ -1, -1 };
				const f32_t hp = pMinion ? pMinion->hp : 0.f;
				const f32_t maxHp = pMinion ? pMinion->maxHp : 0.f;
				const u8_t role = pMinion ? pMinion->roleType : state.type;
				const u8_t lane = pMinion ? pMinion->laneType : state.lane;

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(TeamName(state.team));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(LaneName(lane));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(MinionRoleName(role));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(MinionStateName(state.current));
				ImGui::TableNextColumn();
				ImGui::Text("%llu", static_cast<unsigned long long>(entity));
				ImGui::TableNextColumn();
				ImGui::Text("%.1f %.1f %.1f", pos.x, pos.y, pos.z);
				ImGui::TableNextColumn();
				if (pMinionDebugGrid && pMinionDebugGrid->IsInBounds(cell.x, cell.y))
					ImGui::Text("%d,%d", cell.x, cell.y);
				else
					ImGui::TextUnformatted("-");
				ImGui::TableNextColumn();
				ImGui::Text("%u/%u",
					static_cast<u32_t>(state.PathIndex),
					static_cast<u32_t>(state.PathCount));
				ImGui::TableNextColumn();
				ImGui::Text("%u", static_cast<u32_t>(state.BlockedMoveFrames));
				ImGui::TableNextColumn();
				ImGui::Text("%.0f / %.0f", hp, maxHp);
				ImGui::TableNextColumn();
				ImGui::Text("%.2f", state.moveSpeed);
			});

		ImGui::EndTable();
	}

	ImGui::Text("Minions: %u   Moving: %u   Attacking: %u",
		minionCount,
		movingCount,
		attackingCount);

	ImGui::End();
}
