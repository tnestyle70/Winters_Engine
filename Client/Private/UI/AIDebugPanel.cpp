#include "Network/Client/ClientNetwork.h"
#include "UI/AIDebugPanel.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Network/Client/CommandSerializer.h"
#include "Network/Client/SnapshotApplier.h"
#include "Scene/Scene_InGame.h"
#include "Manager/Navigation/NavGrid.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <cstdio>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

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
		case eChampionAIState::GroupMidDefense: return "GroupMidDefense";
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
		case eChampionAIIntent::DefendMid: return "DefendMid";
		case eChampionAIIntent::Retreat: return "Retreat";
		case eChampionAIIntent::Recall: return "Recall";
		default: return "Unknown";
		}
	}

	const char* ChampionAIBrainName(eChampionAIBrainType brainType)
	{
		switch (brainType)
		{
		case eChampionAIBrainType::RuleBased: return "RuleBased";
		case eChampionAIBrainType::PlayerLike: return "PlayerLike";
		case eChampionAIBrainType::Decision: return "Decision";
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
		case eChampionAIDecisionBlockReason::PolicyCastInterval: return "PolicyInterval";
		case eChampionAIDecisionBlockReason::RuntimeSkillCooldown: return "RuntimeCooldown";
		default: return "Unknown";
		}
	}

	const char* ChampionAIShadowStatusName(u8_t status)
	{
		switch (static_cast<eChampionAIShadowStatusV1>(status))
		{
		case eChampionAIShadowStatusV1::Disabled: return "Disabled";
		case eChampionAIShadowStatusV1::Evaluated: return "Evaluated";
		case eChampionAIShadowStatusV1::InvalidArtifact: return "InvalidArtifact";
		case eChampionAIShadowStatusV1::InvalidTrace: return "InvalidTrace";
		case eChampionAIShadowStatusV1::InsufficientLegalCandidates:
			return "InsufficientLegalCandidates";
		default: return "InvalidStatus";
		}
	}

	const char* ChampionAIShadowCandidateName(u8_t candidateKind)
	{
		switch (candidateKind)
		{
		case 1u: return "Retreat";
		case 2u: return "Fight";
		case 3u: return "Farm";
		case 4u: return "Siege";
		default: return "None";
		}
	}

	u8_t ResolveChampionAIShadowRunnerUp(
		const ChampionAIDecisionTraceEntry& row)
	{
		u8_t runnerUpKind = 0u;
		f32_t runnerUpLogit = 0.f;
		for (u8_t kind = 1u; kind <= kChampionAIShadowCandidateCountV1; ++kind)
		{
			const u32_t bit = 1u << (kind - 1u);
			if ((row.shadowLegalCandidateMask & bit) == 0u ||
				kind == row.shadowSelectedCandidateKind)
			{
				continue;
			}
			const f32_t logit = row.shadowLogits[kind - 1u];
			if (runnerUpKind == 0u || logit > runnerUpLogit)
			{
				runnerUpKind = kind;
				runnerUpLogit = logit;
			}
		}
		return runnerUpKind;
	}

	const char* ChampionAIShadowFeatureName(u16_t featureIndex)
	{
		static_assert(kChampionAIShadowFeatureCountV1 == 67u);
		static_assert(kChampionAIShadowCandidateCountV1 == 4u);
		static constexpr const char* kCandidateFeatureNames[] =
		{
			"candidate_kind_1",
			"candidate_kind_2",
			"candidate_kind_3",
			"candidate_kind_4",
		};
		static constexpr const char* kTargetRelationFeatureNames[] =
		{
			"target_relation_none",
			"target_relation_self",
			"target_relation_enemy_champion",
			"target_relation_enemy_minion",
			"target_relation_enemy_structure",
			"target_relation_allied_wave",
			"target_relation_other_observed",
		};
		static constexpr const char* kContextFeatureNames[] =
		{
			"capability_flags",
			"self_level",
			"enemy_level",
			"self_hp_ratio",
			"enemy_hp_ratio",
			"self_gold",
			"enemy_gold",
			"enemy_distance",
			"attack_range",
			"turret_danger",
			"legal_candidate_mask",
			"illegal_candidate_mask",
			"available_action_mask",
			"available_skill_mask",
		};

		if (featureIndex < 4u)
			return kCandidateFeatureNames[featureIndex];
		if (featureIndex < 11u)
			return kTargetRelationFeatureNames[featureIndex - 4u];
		if (featureIndex >= kChampionAIShadowFeatureCountV1)
			return "n/a";

		const u16_t contextOffset = static_cast<u16_t>(featureIndex - 11u);
		const u16_t candidateIndex = static_cast<u16_t>(contextOffset / 14u);
		const u16_t contextIndex = static_cast<u16_t>(contextOffset % 14u);
		static thread_local char featureName[64]{};
		sprintf_s(
			featureName,
			"kind_%u_x_%s",
			static_cast<u32_t>(candidateIndex + 1u),
			kContextFeatureNames[contextIndex]);
		return featureName;
	}

	const char* AIExecutorStateName(u8_t state)
	{
		switch (static_cast<AiExecutorStateV1>(state))
		{
		case AiExecutorStateV1::Unknown: return "Unknown";
		case AiExecutorStateV1::Submitted: return "Submitted";
		case AiExecutorStateV1::Accepted: return "Accepted";
		case AiExecutorStateV1::Rejected: return "Rejected";
		default: return "Invalid";
		}
	}

	const char* AIExecutorReasonName(u16_t reason)
	{
		switch (static_cast<eCommandExecutionReason>(reason))
		{
		case eCommandExecutionReason::None: return "None";
		case eCommandExecutionReason::UnsupportedCommand: return "UnsupportedCommand";
		case eCommandExecutionReason::InvalidIssuer: return "InvalidIssuer";
		case eCommandExecutionReason::IssuerNotAlive: return "IssuerNotAlive";
		case eCommandExecutionReason::StateBlocked: return "StateBlocked";
		case eCommandExecutionReason::ActionBlocked: return "ActionBlocked";
		case eCommandExecutionReason::PossessionPending: return "PossessionPending";
		case eCommandExecutionReason::InvalidPayload: return "InvalidPayload";
		case eCommandExecutionReason::MissingComponent: return "MissingComponent";
		case eCommandExecutionReason::InvalidTarget: return "InvalidTarget";
		case eCommandExecutionReason::DeadTarget: return "DeadTarget";
		case eCommandExecutionReason::UntargetableTarget: return "UntargetableTarget";
		case eCommandExecutionReason::FriendlyTarget: return "FriendlyTarget";
		case eCommandExecutionReason::Cooldown: return "Cooldown";
		case eCommandExecutionReason::UnlearnedSkill: return "UnlearnedSkill";
		case eCommandExecutionReason::InvalidSkillStage: return "InvalidSkillStage";
		case eCommandExecutionReason::InsufficientResource: return "InsufficientResource";
		case eCommandExecutionReason::OutOfRange: return "OutOfRange";
		case eCommandExecutionReason::NavigationBlocked: return "NavigationBlocked";
		case eCommandExecutionReason::NoActiveRecall: return "NoActiveRecall";
		case eCommandExecutionReason::MissingSummonerSpell: return "MissingSummonerSpell";
		case eCommandExecutionReason::ChampionRuleBlocked: return "ChampionRuleBlocked";
		case eCommandExecutionReason::CarriedStateBlocked: return "CarriedStateBlocked";
		default: return "UnknownReason";
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

	struct AITuningDraftState
	{
		u32_t targetNetId = NULL_NET_ENTITY;
		eChampionAITuningId tuningId = eChampionAITuningId::Count;
		f32_t fDraft = 0.f;
		f32_t fLastSent = 0.f;
		bool_t bInitialized = false;
		bool_t bDirty = false;
		bool_t bPending = false;
		bool_t bHasLastSent = false;
		bool_t bEchoTimedOut = false;
		double sentAtSec = 0.0;
		double statusUntilSec = 0.0;
	};

	std::vector<AITuningDraftState> s_AITuningDrafts;

	AITuningDraftState& ResolveAITuningDraft(
		u32_t targetNetId,
		eChampionAITuningId tuningId)
	{
		for (AITuningDraftState& state : s_AITuningDrafts)
		{
			if (state.targetNetId == targetNetId && state.tuningId == tuningId)
				return state;
		}

		s_AITuningDrafts.push_back(AITuningDraftState{});
		AITuningDraftState& state = s_AITuningDrafts.back();
		state.targetNetId = targetNetId;
		state.tuningId = tuningId;
		return state;
	}

	const char* ChampionAICandidateName(u8_t candidateKind)
	{
		switch (static_cast<AiCandidateKindV1>(candidateKind))
		{
		case AiCandidateKindV1::Retreat: return "Retreat";
		case AiCandidateKindV1::Fight: return "Fight";
		case AiCandidateKindV1::Farm: return "Farm";
		case AiCandidateKindV1::Siege: return "Structure";
		default: return "None";
		}
	}

	const char* ChampionAIFeatureName(u16_t featureId)
	{
		switch (static_cast<AiFeatureIdV1>(featureId))
		{
		case AiFeatureIdV1::UtilityScore: return "UtilityScore";
		case AiFeatureIdV1::PositiveOpportunity: return "PositiveOpportunity";
		case AiFeatureIdV1::TurretRisk: return "TurretRisk";
		case AiFeatureIdV1::ObservedComboRisk: return "ObservedComboRisk";
		case AiFeatureIdV1::ClampOrThresholdAdjustment: return "Clamp/Threshold";
		case AiFeatureIdV1::HealthPressure: return "HealthPressure";
		case AiFeatureIdV1::FarmOpportunity: return "FarmOpportunity";
		case AiFeatureIdV1::StructureExposure: return "StructureExposure";
		default: return "Unknown";
		}
	}

	AiCandidateKindV1 ChampionAIIntentCandidate(eChampionAIIntent intent)
	{
		switch (intent)
		{
		case eChampionAIIntent::Retreat: return AiCandidateKindV1::Retreat;
		case eChampionAIIntent::AttackChampion: return AiCandidateKindV1::Fight;
		case eChampionAIIntent::FarmMinion: return AiCandidateKindV1::Farm;
		case eChampionAIIntent::SiegeStructure: return AiCandidateKindV1::Siege;
		default: return AiCandidateKindV1::None;
		}
	}

	void RenderChampionAIDecisionRanking(const ChampionAIDebugComponent& debug)
	{
		const AiDecisionTraceV1& decision = debug.utilityDecision;
		if (debug.utilityCandidateTick == 0u ||
			decision.candidateCount != kAiDecisionCandidateCapacityV1)
		{
			ImGui::TextDisabled("No current candidate evidence.");
			return;
		}

		std::array<const AiCandidateEvidenceV1*, kAiDecisionCandidateCapacityV1>
			ranked{};
		for (u8_t index = 0u; index < kAiDecisionCandidateCapacityV1; ++index)
			ranked[index] = &decision.candidates[index];
		std::sort(
			ranked.begin(),
			ranked.end(),
			[](const AiCandidateEvidenceV1* lhs, const AiCandidateEvidenceV1* rhs)
			{
				const bool_t lhsLegal =
					(lhs->flags & kAiCandidateLegalFlagV1) != 0u;
				const bool_t rhsLegal =
					(rhs->flags & kAiCandidateLegalFlagV1) != 0u;
				if (lhsLegal != rhsLegal)
					return lhsLegal;
				if (lhs->score != rhs->score)
					return lhs->score > rhs->score;
				return lhs->candidateKind < rhs->candidateKind;
			});

		ImGui::TextDisabled(
			"Server valuation evidence tick %llu. Legal candidates rank before blocked ones.",
			static_cast<unsigned long long>(debug.utilityCandidateTick));
		if (debug.utilitySelectionTick == 0u)
			ImGui::TextDisabled("No decision committed this tick; current intent is ACTIVE / HELD.");

		const AiCandidateKindV1 activeKind = ChampionAIIntentCandidate(debug.intent);
		const AiCandidateEvidenceV1* pTopLegal = nullptr;
		if ((ranked.front()->flags & kAiCandidateLegalFlagV1) != 0u)
			pTopLegal = ranked.front();
		if (ImGui::BeginTable(
			"DecisionRanking", 6,
			ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24.f);
			ImGui::TableSetupColumn("Behavior");
			ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 60.f);
			ImGui::TableSetupColumn("Legal", ImGuiTableColumnFlags_WidthFixed, 45.f);
			ImGui::TableSetupColumn("Decision");
			ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthFixed, 70.f);
			ImGui::TableHeadersRow();
			for (u8_t rank = 0u; rank < ranked.size(); ++rank)
			{
				const AiCandidateEvidenceV1& candidate = *ranked[rank];
				const bool_t bLegal =
					(candidate.flags & kAiCandidateLegalFlagV1) != 0u;
				const bool_t bSelected =
					(candidate.flags & kAiCandidateSelectedFlagV1) != 0u;
				const bool_t bActiveHeld = !bSelected &&
					candidate.candidateKind == static_cast<u8_t>(activeKind);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%u", static_cast<u32_t>(rank + 1u));
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(ChampionAICandidateName(candidate.candidateKind));
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.3f", candidate.score);
				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(bLegal ? "yes" : "no");
				ImGui::TableSetColumnIndex(4);
				if (bSelected)
					ImGui::TextColored(ImVec4(0.35f, 0.9f, 0.45f, 1.f), "SELECTED THIS TICK");
				else if (bActiveHeld)
					ImGui::TextColored(ImVec4(0.95f, 0.78f, 0.30f, 1.f), "ACTIVE / HELD");
				else
					ImGui::TextDisabled(bLegal ? "available" : "blocked");
				ImGui::TableSetColumnIndex(5);
				ImGui::Text("%u", candidate.targetNetEntityId);
			}
			ImGui::EndTable();
		}

		if (pTopLegal && activeKind != AiCandidateKindV1::None &&
			pTopLegal->candidateKind != static_cast<u8_t>(activeKind))
		{
			ImGui::TextColored(
				ImVec4(0.95f, 0.78f, 0.30f, 1.f),
				"Raw #1 differs from active intent: a hard gate, margin, or intent hold is in effect.");
		}
		ImGui::TextDisabled(
			"Brain order: Retreat >= 0.65 -> post-combo -> held intent -> Fight margin -> Structure -> Farm.");

		if (ImGui::CollapsingHeader("Score Calculations"))
		{
			for (u8_t rank = 0u; rank < ranked.size(); ++rank)
			{
				const AiCandidateEvidenceV1& candidate = *ranked[rank];
				ImGui::PushID(candidate.candidateKind);
				const bool_t bOpen = ImGui::TreeNode(
					"score_calc",
					"#%u %s = %.3f",
					static_cast<u32_t>(rank + 1u),
					ChampionAICandidateName(candidate.candidateKind),
					candidate.score);
				if (bOpen)
				{
					f32_t sum = 0.f;
					for (u8_t termIndex = 0u;
						termIndex < candidate.contributionCount;
						++termIndex)
					{
						const AiFeatureContributionV1& term =
							candidate.contributions[termIndex];
						sum += term.contribution;
						ImGui::Text(
							"%s: %.3f x %.3f = %+.3f",
							ChampionAIFeatureName(term.featureId),
							term.rawValue,
							term.weight,
							term.contribution);
					}
					ImGui::TextDisabled("Contribution sum %.3f -> score %.3f", sum, candidate.score);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}
	}

	void ClearAITuningDrafts(u32_t targetNetId)
	{
		s_AITuningDrafts.erase(
			std::remove_if(
				s_AITuningDrafts.begin(),
				s_AITuningDrafts.end(),
				[targetNetId](const AITuningDraftState& state)
				{
					return state.targetNetId == targetNetId;
				}),
			s_AITuningDrafts.end());
	}

	void RenderTuningDrag(
		CScene_InGame* pScene,
		u32_t targetNetId,
		const char* pLabel,
		const char* pHelp,
		eChampionAITuningId tuningId,
		f32_t serverValue,
		f32_t dragSpeed,
		f32_t minValue,
		f32_t maxValue,
		const char* pFormat)
	{
		const bool_t bCanSend = targetNetId != NULL_NET_ENTITY &&
			CanSendAIDebugCommand(pScene);
		AITuningDraftState& state = ResolveAITuningDraft(targetNetId, tuningId);
		const f32_t epsilon = (std::max)(0.0001f, dragSpeed * 0.05f);
		if (!state.bInitialized)
		{
			state.fDraft = serverValue;
			state.bInitialized = true;
		}

		if (state.bPending &&
			std::fabs(serverValue - state.fLastSent) <= epsilon)
		{
			state.bPending = false;
			state.bEchoTimedOut = false;
			state.fDraft = serverValue;
		}
		else if (state.bPending && ImGui::GetTime() - state.sentAtSec > 2.0)
		{
			state.bPending = false;
			state.bEchoTimedOut = true;
			state.fDraft = serverValue;
			state.statusUntilSec = ImGui::GetTime() + 2.0;
		}
		if (state.bEchoTimedOut && ImGui::GetTime() > state.statusUntilSec)
			state.bEchoTimedOut = false;

		if (!state.bPending && !state.bDirty && !state.bEchoTimedOut)
			state.fDraft = serverValue;

		ImGui::PushID(static_cast<int>(targetNetId));
		ImGui::PushID(static_cast<int>(tuningId));
		ImGui::BeginDisabled(!bCanSend);
		bool_t bCommit = false;
		if (ImGui::BeginTable(
			"##TuningRow", 2,
			ImGuiTableFlags_SizingStretchProp |
				ImGuiTableFlags_NoSavedSettings))
		{
			ImGui::TableSetupColumn(
				"Label", ImGuiTableColumnFlags_WidthStretch, 0.42f);
			ImGui::TableSetupColumn(
				"Value", ImGuiTableColumnFlags_WidthStretch, 0.58f);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(pLabel);
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.f);
			if (ImGui::DragFloat(
				"##Value",
				&state.fDraft,
				dragSpeed,
				minValue,
				maxValue,
				pFormat,
				ImGuiSliderFlags_AlwaysClamp) && std::isfinite(state.fDraft))
			{
				state.bDirty = true;
				state.bEchoTimedOut = false;
			}
			bCommit = ImGui::IsItemDeactivatedAfterEdit();
			if (ImGui::IsItemHovered() && pHelp)
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(430.f);
				ImGui::TextUnformatted(pHelp);
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
			ImGui::EndTable();
		}
		ImGui::EndDisabled();
		if (bCommit && state.bDirty &&
			(!state.bHasLastSent ||
				std::fabs(state.fDraft - state.fLastSent) > epsilon))
		{
			pScene->GetCommandSerializer()->SendAIDebugTune(
				*pScene->GetNetworkView(), targetNetId,
				static_cast<u8_t>(tuningId), state.fDraft);
			state.fLastSent = state.fDraft;
			state.bHasLastSent = true;
			state.bPending = true;
			state.bDirty = false;
			state.sentAtSec = ImGui::GetTime();
		}
		else if (bCommit)
		{
			state.bDirty = false;
		}
		ImGui::PopID();
		ImGui::PopID();

		if (state.bPending)
			ImGui::TextDisabled("Waiting for authoritative server echo...");
		else if (state.bEchoTimedOut)
			ImGui::TextColored(
				ImVec4(1.f, 0.45f, 0.35f, 1.f),
				"No server echo; restored snapshot value.");
	}

	struct AIChronoDecisionSample
	{
		u64_t serverTick = 0u;
		u64_t timelineEpoch = 0u;
		u64_t branchId = 0u;
		u64_t toolRevision = 0u;
		NetEntityId netId = NULL_NET_ENTITY;
		eChampion champion = eChampion::END;
		eTeam team = eTeam::Neutral;
		ChampionAIDecisionTraceEntry decision{};
	};

	NetEntityId s_SelectedAINetId = NULL_NET_ENTITY;
	std::vector<AIChronoDecisionSample> s_ChronoDecisionHistory;
	u32_t s_ChronoSampleIntervalTicks = 30u;
	u32_t s_ChronoMaxSamples = 180u;
	u32_t s_ChronoRewindTicks = 150u;
	u64_t s_LastChronoSampleTick = ~0ull;
	u64_t s_LastChronoEpoch = ~0ull;
	u64_t s_LastChronoBranch = ~0ull;
	NetEntityId s_LastChronoNetId = NULL_NET_ENTITY;
	int s_SelectedChronoSample = -1;
	u64_t s_LastRequestedTargetTick = 0u;
	u64_t s_LastRestoredTick = 0u;
	CScene_InGame* s_pChronoScene = nullptr;
}

void UI::CAIDebugPanel::Render(CWorld& world, CScene_InGame* pScene)
{
	struct AIRow
	{
		EntityID entity = NULL_ENTITY;
		ChampionComponent champion{};
		ChampionAIDebugComponent debug{};
		f32_t hp = 0.f;
		f32_t maxHp = 0.f;
	};

	std::vector<AIRow> bots{};
	bots.reserve(16u);
	world.ForEach<ChampionComponent>(
		[&](EntityID entity, ChampionComponent& champion)
		{
			if (!world.HasComponent<ChampionAIDebugComponent>(entity))
				return;
			const auto& debug = world.GetComponent<ChampionAIDebugComponent>(entity);
			if (!debug.bPresent)
				return;

			AIRow row{};
			row.entity = entity;
			row.champion = champion;
			row.debug = debug;
			if (world.HasComponent<HealthComponent>(entity))
			{
				const auto& health = world.GetComponent<HealthComponent>(entity);
				row.hp = health.fCurrent;
				row.maxHp = health.fMaximum;
			}
			else
			{
				row.hp = champion.hp;
				row.maxHp = champion.maxHp;
			}
			bots.push_back(row);
		});

	if (!bots.empty() && s_SelectedAINetId == NULL_NET_ENTITY)
		s_SelectedAINetId = bots.front().debug.netId;
	const AIRow* pSelected = nullptr;
	for (const AIRow& row : bots)
	{
		if (row.debug.netId == s_SelectedAINetId)
		{
			pSelected = &row;
			break;
		}
	}
	if (!pSelected && !bots.empty())
	{
		s_SelectedAINetId = bots.front().debug.netId;
		pSelected = &bots.front();
	}

	ImGui::SetNextWindowSize(ImVec2(700.f, 620.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(560.f, 380.f), ImVec2(1100.f, 1000.f));
	if (!ImGui::Begin("AI Debug"))
	{
		ImGui::End();
		return;
	}

	if (bots.empty())
	{
		ImGui::TextDisabled("No AI snapshot yet.");
		ImGui::End();
		return;
	}

	char preview[96]{};
	sprintf_s(
		preview,
		"%s (%s)",
		ChampionName(pSelected->champion.id),
		TeamName(pSelected->champion.team));
	ImGui::SetNextItemWidth(-1.f);
	if (ImGui::BeginCombo("##Bot", preview))
	{
		for (const AIRow& row : bots)
		{
			char label[112]{};
			sprintf_s(
				label,
				"%s (%s)##%u",
				ChampionName(row.champion.id),
				TeamName(row.champion.team),
				row.debug.netId);
			const bool_t selected = row.debug.netId == s_SelectedAINetId;
			if (ImGui::Selectable(label, selected))
				s_SelectedAINetId = row.debug.netId;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	const ChampionAIDebugComponent& debug = pSelected->debug;
	const bool_t bCanSend = CanSendAIDebugCommand(pScene);
	if (!bCanSend)
		ImGui::TextDisabled("Server-authoritative practice session required.");

	if (ImGui::BeginTabBar("AIDebugModes"))
	{
		if (ImGui::BeginTabItem("Live Tuning"))
		{
			ImGui::TextDisabled(
				"Server snapshot values. Drag or double-click for an exact value.");
			ImGui::TextDisabled(
				"Session-only override for this bot; Reset restores its JSON profile.");
			ImGui::SeparatorText("Decision Ranking");
			RenderChampionAIDecisionRanking(debug);

			ImGui::SeparatorText("Lane Choice - immediate");
			RenderTuningDrag(
				pScene, debug.netId, "Follow Wave Search (m)",
				"Maximum distance used to find an allied lane minion. "
				"Higher values join a farther wave; lower values wait near the safe anchor.",
				eChampionAITuningId::FollowWaveSearchRange,
				debug.fFollowWaveSearchRange, 0.5f, 10.f, 120.f, "%.1f");
			RenderTuningDrag(
				pScene, debug.netId, "Farm Priority (x)",
				"Multiplier for farm utility before intent selection. "
				"Higher values prefer minion farming; lower values let fight or siege win sooner.",
				eChampionAITuningId::FarmPriority,
				debug.fFarmPriority, 0.01f, 0.f, 3.f, "%.2f");
			RenderTuningDrag(
				pScene, debug.netId, "Turret Danger Limit",
				"The bot retreats when measured turret danger exceeds this value "
				"without a wave tanking. Higher values tolerate more turret risk.",
				eChampionAITuningId::TurretDangerThreshold,
				debug.fTurretDangerThreshold, 0.01f, 0.f, 1.f, "%.2f");

			ImGui::BeginDisabled(!bCanSend || debug.netId == NULL_NET_ENTITY);
			if (ImGui::Button("Reset Selected Bot", ImVec2(180.f, 0.f)))
			{
				pScene->GetCommandSerializer()->SendAIDebugResetTuning(
					*pScene->GetNetworkView(), debug.netId);
				ClearAITuningDrafts(debug.netId);
			}
			ImGui::EndDisabled();

			if (ImGui::CollapsingHeader("Lane Perception"))
			{
				RenderTuningDrag(
					pScene, debug.netId, "Champion Scan (m)",
					"Enemy champion perception radius. Higher values notice and score fights earlier.",
					eChampionAITuningId::ChampionScanRange,
					debug.fChampionScanRange, 0.1f, 1.f, 40.f, "%.1f");
				RenderTuningDrag(
					pScene, debug.netId, "Minion Scan (m)",
					"Enemy minion perception radius used by FarmMinion. "
					"It must be large enough to see the next last-hit or chase target.",
					eChampionAITuningId::MinionScanRange,
					debug.fMinionScanRange, 0.1f, 1.f, 40.f, "%.1f");
				RenderTuningDrag(
					pScene, debug.netId, "Structure Scan (m)",
					"Enemy structure perception radius used for siege scoring.",
					eChampionAITuningId::StructureScanRange,
					debug.fStructureScanRange, 0.1f, 1.f, 60.f, "%.1f");
				RenderTuningDrag(
					pScene, debug.netId, "Leash Range (m)",
					"Maximum pursuit distance from the bot's lane goal before it disengages.",
					eChampionAITuningId::LeashRange,
					debug.fLeashRange, 0.1f, 1.f, 60.f, "%.1f");
			}

			if (ImGui::CollapsingHeader("Combat, Survival & Cadence"))
			{
				RenderTuningDrag(
					pScene, debug.netId, "Champion Score Margin",
					"Extra fight score required above farm score. "
					"Higher values make champion engagement more conservative.",
					eChampionAITuningId::ChampionScoreMargin,
					debug.fChampionScoreMargin, 0.01f, 0.f, 1.f, "%.2f");
				RenderTuningDrag(
					pScene, debug.netId, "Retreat HP",
					"Retreat starts at or below this self HP ratio.",
					eChampionAITuningId::RetreatHpRatio,
					debug.fRetreatHpRatio, 0.01f, 0.01f, 0.90f, "%.2f");
				RenderTuningDrag(
					pScene, debug.netId, "Reengage HP",
					"After recovery, normal lane decisions resume at or above this HP ratio.",
					eChampionAITuningId::ReengageHpRatio,
					debug.fReengageHpRatio, 0.01f, 0.01f, 1.f, "%.2f");
				RenderTuningDrag(
					pScene, debug.netId, "Skill Cast Interval (s)",
					"Minimum interval between fresh AI skill sequences. "
					"Lower values cast more often; active combos remain uninterrupted.",
					eChampionAITuningId::SkillCastMinInterval,
					debug.fSkillCastMinInterval, 0.05f, 0.f, 15.f, "%.2f");
			}

			if (ImGui::CollapsingHeader("Test Actions"))
			{
				RenderForceActionButton(
					pScene, debug.netId, "Safe", eChampionAIAction::MoveToSafeAnchor);
				ImGui::SameLine();
				RenderForceActionButton(
					pScene, debug.netId, "Farm", eChampionAIAction::AttackMinion);
				ImGui::SameLine();
				RenderForceActionButton(
					pScene, debug.netId, "Fight", eChampionAIAction::AttackChampion);
				ImGui::SameLine();
				RenderForceActionButton(
					pScene, debug.netId, "Structure", eChampionAIAction::AttackStructure);
				ImGui::SameLine();
				RenderForceActionButton(
					pScene, debug.netId, "Retreat", eChampionAIAction::Retreat);
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Observe"))
		{
			ImGui::SeparatorText("Now");
			ImGui::Text(
				"%s  |  %s  |  %s",
				ChampionAIStateName(debug.state),
				ChampionAIIntentName(debug.intent),
				ChampionAIActionName(debug.action));
			ImGui::Text(
				"HP %.0f / %.0f  |  Brain %s",
				pSelected->hp, pSelected->maxHp,
				ChampionAIBrainName(debug.brainType));
			ImGui::Text(
				"Fight %.2f   Farm %.2f   Structure %.2f   Retreat %.2f",
				debug.fChampionDecisionScore,
				debug.fFarmDecisionScore,
				debug.fStructureDecisionScore,
				debug.fRetreatDecisionScore);

			ImGui::SeparatorText("Targets & Move");
			ImGui::Text(
				"Chosen %u  |  Enemy minion %u  |  Allied wave %u",
				debug.targetNetId,
				debug.enemyMinionNetId,
				debug.alliedWaveNetId);
			ImGui::Text(
				"Wave distance %.1f  |  support %.1f  |  search %.1f",
				debug.fWaveDistance,
				debug.fWaveSupportRange,
				debug.fFollowWaveSearchRange);
			ImGui::Text(
				"Last %s %s -> target %u  goal (%.1f, %.1f)",
				ChampionAICommandKindName(debug.lastCommandKind),
				SkillSlotName(debug.lastCommandSlot),
				debug.lastCommandTargetNetId,
				debug.lastCommandPos.x,
				debug.lastCommandPos.z);
			ImGui::Text(
				"Block %s  |  Executor %s / %s",
				ChampionAIBlockReasonName(debug.lastBlockReason),
				AIExecutorStateName(debug.lastExecutorState),
				AIExecutorReasonName(debug.lastExecutorReason));

			if (ImGui::CollapsingHeader("Latest Decision"))
			{
				if (debug.debugDecisionTraceCount == 0u)
				{
					ImGui::TextDisabled("No decision yet.");
				}
				else
				{
					const ChampionAIDecisionTraceEntry& latest =
						debug.debugDecisionTrace[debug.debugDecisionTraceCount - 1u];
					ImGui::Text(
						"Tick %llu  |  %s  |  %s",
						static_cast<unsigned long long>(latest.tick),
						ChampionAIIntentName(latest.intent),
						ChampionAIActionName(latest.action));
					ImGui::Text(
						"Block %s  |  Target %llu  |  Command %s %s",
						ChampionAIBlockReasonName(latest.blockReason),
						static_cast<unsigned long long>(latest.target),
						ChampionAICommandKindName(latest.commandKind),
						SkillSlotName(latest.commandSlot));
				}
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

#if 0 // Legacy all-in-one AI inspection surface retained as backend reference only.
void UI::CAIDebugPanel::Render(CWorld& world, CScene_InGame* pScene)
{
	ImGui::SetNextWindowSize(ImVec2(820.f, 720.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSizeConstraints(ImVec2(560.f, 320.f), ImVec2(1400.f, 1200.f));
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
	std::vector<SelectedAI> botRows;
	botRows.reserve(16);

	world.ForEach<ChampionComponent, TransformComponent>(
		[&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
		{
			if (!world.HasComponent<ChampionAIDebugComponent>(entity))
				return;

			auto& debug = world.GetComponent<ChampionAIDebugComponent>(entity);
			if (!debug.bPresent)
				return;

			SelectedAI row{};
			row.bValid = true;
			row.entity = entity;
			row.champion = champion;
			row.debug = debug;
			row.pos = transform.GetLocalPosition();
			row.hp = world.HasComponent<HealthComponent>(entity)
				? world.GetComponent<HealthComponent>(entity).fCurrent
				: champion.hp;
			row.maxHp = world.HasComponent<HealthComponent>(entity)
				? world.GetComponent<HealthComponent>(entity).fMaximum
				: champion.maxHp;
			row.actionId = world.HasComponent<ActionStateComponent>(entity)
				? world.GetComponent<ActionStateComponent>(entity).actionId
				: 0u;
			botRows.push_back(row);
		});

	const u32_t botCount = static_cast<u32_t>(botRows.size());
	if (botCount > 0 && s_SelectedAINetId == NULL_NET_ENTITY)
		s_SelectedAINetId = botRows.front().debug.netId;
	for (const SelectedAI& row : botRows)
	{
		if (row.debug.netId == s_SelectedAINetId)
		{
			selectedAI = row;
			break;
		}
	}

	if (botCount > 0)
	{
		char preview[96]{};
		if (selectedAI.bValid)
		{
			sprintf_s(preview, "%s (%s) net=%u",
				ChampionName(selectedAI.champion.id),
				TeamName(selectedAI.champion.team),
				selectedAI.debug.netId);
		}
		ImGui::SetNextItemWidth(320.f);
		if (ImGui::BeginCombo("Bot", selectedAI.bValid ? preview : "select bot"))
		{
			for (const SelectedAI& row : botRows)
			{
				char label[96]{};
				sprintf_s(label, "%s (%s) net=%u##%llu",
					ChampionName(row.champion.id),
					TeamName(row.champion.team),
					row.debug.netId,
					static_cast<unsigned long long>(row.entity));
				const bool_t bRowSelected = row.debug.netId == s_SelectedAINetId;
				if (ImGui::Selectable(label, bRowSelected))
					s_SelectedAINetId = row.debug.netId;
				if (bRowSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	if (s_pChronoScene != pScene)
	{
		s_pChronoScene = pScene;
		s_ChronoDecisionHistory.clear();
		s_SelectedChronoSample = -1;
		s_LastChronoSampleTick = ~0ull;
		s_LastChronoEpoch = ~0ull;
		s_LastChronoBranch = ~0ull;
		s_LastChronoNetId = NULL_NET_ENTITY;
		s_LastRequestedTargetTick = 0u;
		s_LastRestoredTick = 0u;
	}

	if (selectedAI.bValid && pScene && pScene->GetSnapshotApplier() &&
		selectedAI.debug.debugDecisionTraceCount > 0u)
	{
		const auto* pSnapshotApplier = pScene->GetSnapshotApplier();
		const u64_t serverTick = pSnapshotApplier->GetLastAppliedServerTick();
		const SnapshotTimelineState& timeline = pSnapshotApplier->GetTimelineState();

		if (s_LastChronoNetId != selectedAI.debug.netId)
		{
			s_ChronoDecisionHistory.clear();
			s_SelectedChronoSample = -1;
			s_LastChronoSampleTick = ~0ull;
			s_LastChronoEpoch = ~0ull;
			s_LastChronoBranch = ~0ull;
			s_LastChronoNetId = selectedAI.debug.netId;
		}

		const bool_t bTimelineChanged =
			s_LastChronoEpoch != ~0ull &&
			(s_LastChronoEpoch != timeline.timelineEpoch ||
				s_LastChronoBranch != timeline.branchId);
		if (bTimelineChanged)
			s_LastRestoredTick = serverTick;

		const bool_t bSampleDue =
			s_LastChronoSampleTick == ~0ull ||
			bTimelineChanged ||
			serverTick < s_LastChronoSampleTick ||
			serverTick - s_LastChronoSampleTick >= s_ChronoSampleIntervalTicks;
		const ChampionAIDecisionTraceEntry& latest =
			selectedAI.debug.debugDecisionTrace[
				selectedAI.debug.debugDecisionTraceCount - 1u];
		const bool_t bDuplicateDecision =
			!s_ChronoDecisionHistory.empty() &&
			!bTimelineChanged &&
			s_ChronoDecisionHistory.back().netId == selectedAI.debug.netId &&
			s_ChronoDecisionHistory.back().decision.tick == latest.tick;
		if (bSampleDue && !bDuplicateDecision)
		{
			AIChronoDecisionSample sample{};
			sample.serverTick = serverTick;
			sample.timelineEpoch = timeline.timelineEpoch;
			sample.branchId = timeline.branchId;
			sample.toolRevision = timeline.toolRevision;
			sample.netId = selectedAI.debug.netId;
			sample.champion = selectedAI.champion.id;
			sample.team = selectedAI.champion.team;
			sample.decision = latest;
			s_ChronoDecisionHistory.push_back(sample);
			while (s_ChronoDecisionHistory.size() > s_ChronoMaxSamples)
			{
				s_ChronoDecisionHistory.erase(s_ChronoDecisionHistory.begin());
				if (s_SelectedChronoSample >= 0)
					--s_SelectedChronoSample;
			}
			s_LastChronoSampleTick = serverTick;
			s_LastChronoEpoch = timeline.timelineEpoch;
			s_LastChronoBranch = timeline.branchId;
		}
	}

	if (ImGui::CollapsingHeader("All Bots"))
	{
		if (ImGui::BeginTable(
			"ChampionAISnapshotTable",
			11,
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
			ImGui::TableSetupColumn("Brain");
			ImGui::TableSetupColumn("Macro");
			ImGui::TableSetupColumn("Action");
			ImGui::TableSetupColumn("Target");
			ImGui::TableSetupColumn("Pos");
			ImGui::TableSetupColumn("HP");
			ImGui::TableSetupColumn("ActionId");
			ImGui::TableHeadersRow();

			for (const SelectedAI& row : botRows)
			{
				const bool_t bSelected = row.debug.netId == s_SelectedAINetId;

				ImGui::PushID(static_cast<int>(row.entity));
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				char label[96]{};
				sprintf_s(label, "%s##%llu",
					ChampionName(row.champion.id),
					static_cast<unsigned long long>(row.entity));
				if (ImGui::Selectable(label, bSelected, ImGuiSelectableFlags_SpanAllColumns))
					s_SelectedAINetId = row.debug.netId;
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(TeamName(row.champion.team));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIStateName(row.debug.state));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIIntentName(row.debug.intent));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIBrainName(row.debug.brainType));
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(
					row.debug.bMidDefenseActive ? "DefendMid" : "HomeLane");
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(ChampionAIActionName(row.debug.action));
				ImGui::TableNextColumn();
				ImGui::Text("%u", row.debug.targetNetId);
				ImGui::TableNextColumn();
				ImGui::Text("%.1f %.1f %.1f", row.pos.x, row.pos.y, row.pos.z);
				ImGui::TableNextColumn();
				ImGui::Text("%.0f / %.0f", row.hp, row.maxHp);
				ImGui::TableNextColumn();
				ImGui::Text("%u", static_cast<u32_t>(row.actionId));
				ImGui::PopID();
			}

			ImGui::EndTable();
		}
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
		const ChampionAIDecisionTraceEntry* pLatestDecision =
			debug.debugDecisionTraceCount > 0u
			? &debug.debugDecisionTrace[debug.debugDecisionTraceCount - 1u]
			: nullptr;

		if (ImGui::CollapsingHeader("Selected AI", ImGuiTreeNodeFlags_DefaultOpen))
		{
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
		ImGui::Text("Brain: %s   Macro: %s",
			ChampionAIBrainName(debug.brainType),
			debug.bMidDefenseActive ? "DefendMid" : "HomeLane");
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
		ImGui::Text("Scores: retreat %.2f   champion %.2f   farm %.2f   structure %.2f",
			debug.fRetreatDecisionScore,
			debug.fChampionDecisionScore,
			debug.fFarmDecisionScore,
			debug.fStructureDecisionScore);
		ImGui::Text("Decision gates: canFight=%s   postBA=%s   legal=0x%08X   illegal=0x%08X",
			debug.bCanAttackChampion ? "yes" : "no",
			debug.bPostComboBAAllowed ? "yes" : "no",
			pLatestDecision ? pLatestDecision->legalCandidateMask : 0u,
			pLatestDecision ? pLatestDecision->illegalCandidateMask : 0u);
		if (pLatestDecision)
		{
			ImGui::Text("Shadow BC: %s   artifact r%llu sha:%016llx",
				ChampionAIShadowStatusName(pLatestDecision->shadowStatus),
				static_cast<unsigned long long>(pLatestDecision->shadowPolicyRevision),
				static_cast<unsigned long long>(pLatestDecision->shadowPolicySha256Prefix));
			ImGui::Text("active-final %s -> shadow %s   %s   legal=0x%08X",
				ChampionAIShadowCandidateName(pLatestDecision->shadowActiveCandidateKind),
				ChampionAIShadowCandidateName(pLatestDecision->shadowSelectedCandidateKind),
				pLatestDecision->shadowStatus ==
					static_cast<u8_t>(eChampionAIShadowStatusV1::Evaluated)
					? (pLatestDecision->bShadowDisagreed ? "DISAGREE" : "AGREE")
					: "N/A",
				pLatestDecision->shadowLegalCandidateMask);
			ImGui::Text("logits: R %.5f / F %.5f / Farm %.5f / Siege %.5f   selected margin %.5f",
				pLatestDecision->shadowLogits[0],
				pLatestDecision->shadowLogits[1],
				pLatestDecision->shadowLogits[2],
				pLatestDecision->shadowLogits[3],
				pLatestDecision->shadowSelectedMargin);
			ImGui::TextDisabled("Raw masked logits; these values are not probabilities.");
			const u8_t runnerUpKind =
				ResolveChampionAIShadowRunnerUp(*pLatestDecision);
			ImGui::Text("top margin contribution vs %s: %s (#%u)  %+.5f",
				ChampionAIShadowCandidateName(runnerUpKind),
				ChampionAIShadowFeatureName(pLatestDecision->shadowTopFeatureIndex),
				static_cast<u32_t>(pLatestDecision->shadowTopFeatureIndex),
				pLatestDecision->shadowTopFeatureContribution);
		}
		else
		{
			ImGui::TextDisabled("Shadow BC: no decision-tick evidence yet.");
		}
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
		ImGui::Text("Risk: HP %.0f%%   retreat<=%.0f%%   reengage>=%.0f%%   turret %.2f/%.2f",
			debug.fSelfHpRatio * 100.f,
			debug.fRetreatHpRatio * 100.f,
			debug.fReengageHpRatio * 100.f,
			debug.fTurretDanger,
			debug.fTurretDangerThreshold);
		ImGui::Text("Profile: aggression %.2f   kite %.2f   policy cast %.2fs remaining %.2fs",
			profile.aggression,
			profile.kiteBias,
			debug.fSkillCastMinInterval,
			debug.fSkillCastCooldownTimer);
		ImGui::Text("Executor: seq=%u   state=%s   reason=%s(%u)",
			debug.lastCommandSequence,
			AIExecutorStateName(debug.lastExecutorState),
			AIExecutorReasonName(debug.lastExecutorReason),
			static_cast<u32_t>(debug.lastExecutorReason));

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

		}

		if (ImGui::CollapsingHeader("Runtime Tuning"))
		{
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
		RenderTuningSlider(pScene, debug.netId, "Skill Cast Min Interval", eChampionAITuningId::SkillCastMinInterval,
			debug.fSkillCastMinInterval, 0.f, 15.f);
		ImGui::BeginDisabled(!bCanSend || debug.netId == NULL_NET_ENTITY);
		if (ImGui::Button("Reset Runtime Tuning"))
			pScene->GetCommandSerializer()->SendAIDebugResetTuning(*pScene->GetNetworkView(), debug.netId);
		ImGui::EndDisabled();

		}

		if (ImGui::CollapsingHeader("Decision Trace"))
		{
		if (debug.debugDecisionTraceCount == 0u)
		{
			ImGui::TextDisabled("No decision trace rows yet.");
		}
		else if (ImGui::BeginTable(
			"AIDecisionTraceTable",
			12,
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
			ImGui::TableSetupColumn("Masks");
			ImGui::TableSetupColumn("Executor");
			ImGui::TableSetupColumn("Shadow");
			ImGui::TableSetupColumn("Artifact");
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
				ImGui::Text("%.2f/%.2f/%.2f/%.2f",
					row.retreatScore,
					row.championScore,
					row.farmScore,
					row.structureScore);
				ImGui::TableNextColumn();
				ImGui::Text("%X/%X", row.legalCandidateMask, row.illegalCandidateMask);
				ImGui::TableNextColumn();
				ImGui::Text("%u %s/%s",
					row.commandSequence,
					AIExecutorStateName(row.executorState),
					AIExecutorReasonName(row.executorReason));
				ImGui::TableNextColumn();
				ImGui::Text("%s->%s %s",
					ChampionAIShadowCandidateName(row.shadowActiveCandidateKind),
					ChampionAIShadowCandidateName(row.shadowSelectedCandidateKind),
					ChampionAIShadowStatusName(row.shadowStatus));
				ImGui::TableNextColumn();
				ImGui::Text("r%llu/%016llx",
					static_cast<unsigned long long>(row.shadowPolicyRevision),
					static_cast<unsigned long long>(row.shadowPolicySha256Prefix));
			}

			ImGui::EndTable();
		}
		}

		if (ImGui::CollapsingHeader("Chrono Decision Timeline", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const CSnapshotApplier* pSnapshotApplier = pScene
				? pScene->GetSnapshotApplier()
				: nullptr;
			const u64_t currentTick = pSnapshotApplier
				? pSnapshotApplier->GetLastAppliedServerTick()
				: 0u;
			const SnapshotTimelineState timeline = pSnapshotApplier
				? pSnapshotApplier->GetTimelineState()
				: SnapshotTimelineState{};

			ImGui::Text("Tick %llu   epoch %llu   branch %llu   toolRev %llu   %s x%.2f",
				static_cast<unsigned long long>(currentTick),
				static_cast<unsigned long long>(timeline.timelineEpoch),
				static_cast<unsigned long long>(timeline.branchId),
				static_cast<unsigned long long>(timeline.toolRevision),
				timeline.simPaused ? "paused" : "running",
				timeline.simSpeedMul);
			ImGui::TextDisabled(
				"Local selected-bot cache only while F9 is open. Restore snaps to nearest <= target 30-tick keyframe.");

			int sampleInterval = static_cast<int>(s_ChronoSampleIntervalTicks);
			if (ImGui::SliderInt("Sample interval (ticks)", &sampleInterval, 1, 300))
				s_ChronoSampleIntervalTicks = static_cast<u32_t>(sampleInterval);
			int maxSamples = static_cast<int>(s_ChronoMaxSamples);
			if (ImGui::SliderInt("Max local samples", &maxSamples, 16, 512))
			{
				s_ChronoMaxSamples = static_cast<u32_t>(maxSamples);
				while (s_ChronoDecisionHistory.size() > s_ChronoMaxSamples)
				{
					s_ChronoDecisionHistory.erase(s_ChronoDecisionHistory.begin());
					if (s_SelectedChronoSample >= 0)
						--s_SelectedChronoSample;
				}
			}
			int rewindTicks = static_cast<int>(s_ChronoRewindTicks);
			if (ImGui::SliderInt("Rewind delta (ticks)", &rewindTicks, 1, 1800))
				s_ChronoRewindTicks = static_cast<u32_t>(rewindTicks);

			const u64_t requestedTarget = currentTick > s_ChronoRewindTicks
				? currentTick - s_ChronoRewindTicks
				: 1u;
			ImGui::Text("Request: -%u ticks (%.3fs) -> target %llu",
				s_ChronoRewindTicks,
				static_cast<f32_t>(s_ChronoRewindTicks) /
					static_cast<f32_t>(DeterministicTime::kTicksPerSecond),
				static_cast<unsigned long long>(requestedTarget));

			const bool_t bCanRewind = bCanSend &&
				pSnapshotApplier &&
				currentTick > s_ChronoRewindTicks;
			ImGui::BeginDisabled(!bCanRewind);
			if (ImGui::Button("Rewind authoritative simulation"))
			{
				s_LastRequestedTargetTick = requestedTarget;
				s_LastRestoredTick = 0u;
				pScene->GetCommandSerializer()->SendPracticeControl(
					*pScene->GetNetworkView(),
					ePracticeOperation::RewindSimulationSeconds,
					static_cast<f32_t>(s_ChronoRewindTicks) /
						static_cast<f32_t>(DeterministicTime::kTicksPerSecond));
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Clear local timeline"))
			{
				s_ChronoDecisionHistory.clear();
				s_SelectedChronoSample = -1;
				s_LastChronoSampleTick = ~0ull;
			}

			if (s_LastRequestedTargetTick != 0u)
			{
				if (s_LastRestoredTick != 0u)
				{
					ImGui::Text("Last request target %llu   observed restored %llu",
						static_cast<unsigned long long>(s_LastRequestedTargetTick),
						static_cast<unsigned long long>(s_LastRestoredTick));
				}
				else
				{
					ImGui::Text("Last request target %llu   observed restored pending",
						static_cast<unsigned long long>(s_LastRequestedTargetTick));
				}
			}

			if (s_ChronoDecisionHistory.empty())
			{
				ImGui::TextDisabled("No branch-tagged decision sample yet.");
			}
			else if (ImGui::BeginTable(
				"AIChronoDecisionTable",
				10,
				ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_RowBg |
				ImGuiTableFlags_Resizable |
				ImGuiTableFlags_ScrollY,
				ImVec2(0.f, 180.f)))
			{
				ImGui::TableSetupColumn("ServerTick");
				ImGui::TableSetupColumn("Epoch/Branch");
				ImGui::TableSetupColumn("DecisionTick");
				ImGui::TableSetupColumn("State");
				ImGui::TableSetupColumn("Intent");
				ImGui::TableSetupColumn("Action");
				ImGui::TableSetupColumn("Why");
				ImGui::TableSetupColumn("Executor");
				ImGui::TableSetupColumn("Shadow");
				ImGui::TableSetupColumn("Artifact");
				ImGui::TableHeadersRow();

				for (int i = static_cast<int>(s_ChronoDecisionHistory.size()) - 1;
					i >= 0;
					--i)
				{
					const AIChronoDecisionSample& sample =
						s_ChronoDecisionHistory[static_cast<size_t>(i)];
					const ChampionAIDecisionTraceEntry& row = sample.decision;
					ImGui::PushID(i);
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					char tickLabel[32]{};
					sprintf_s(tickLabel, "%llu",
						static_cast<unsigned long long>(sample.serverTick));
					if (ImGui::Selectable(
						tickLabel,
						s_SelectedChronoSample == i,
						ImGuiSelectableFlags_SpanAllColumns))
					{
						s_SelectedChronoSample = i;
					}
					ImGui::TableNextColumn();
					ImGui::Text("%llu/%llu",
						static_cast<unsigned long long>(sample.timelineEpoch),
						static_cast<unsigned long long>(sample.branchId));
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
					ImGui::Text("%s/%s",
						AIExecutorStateName(row.executorState),
						AIExecutorReasonName(row.executorReason));
					ImGui::TableNextColumn();
					ImGui::Text("%s->%s %s",
						ChampionAIShadowCandidateName(row.shadowActiveCandidateKind),
						ChampionAIShadowCandidateName(row.shadowSelectedCandidateKind),
						ChampionAIShadowStatusName(row.shadowStatus));
					ImGui::TableNextColumn();
					ImGui::Text("r%llu/%016llx",
						static_cast<unsigned long long>(row.shadowPolicyRevision),
						static_cast<unsigned long long>(row.shadowPolicySha256Prefix));
					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			if (s_SelectedChronoSample >= 0 &&
				static_cast<size_t>(s_SelectedChronoSample) <
					s_ChronoDecisionHistory.size())
			{
				const AIChronoDecisionSample& sample =
					s_ChronoDecisionHistory[
						static_cast<size_t>(s_SelectedChronoSample)];
				const ChampionAIDecisionTraceEntry& row = sample.decision;
				ImGui::SeparatorText("Selected decision evidence");
				ImGui::Text("%s %s net=%u   scores R/F/Farm/Siege %.3f / %.3f / %.3f / %.3f",
					TeamName(sample.team),
					ChampionName(sample.champion),
					sample.netId,
					row.retreatScore,
					row.championScore,
					row.farmScore,
					row.structureScore);
				ImGui::Text("Shadow BC %s   artifact r%llu sha:%016llx",
					ChampionAIShadowStatusName(row.shadowStatus),
					static_cast<unsigned long long>(row.shadowPolicyRevision),
					static_cast<unsigned long long>(row.shadowPolicySha256Prefix));
				ImGui::Text("active-final %s -> shadow %s   %s   logits %.5f / %.5f / %.5f / %.5f   margin %.5f",
					ChampionAIShadowCandidateName(row.shadowActiveCandidateKind),
					ChampionAIShadowCandidateName(row.shadowSelectedCandidateKind),
					row.shadowStatus ==
						static_cast<u8_t>(eChampionAIShadowStatusV1::Evaluated)
						? (row.bShadowDisagreed ? "DISAGREE" : "AGREE")
						: "N/A",
					row.shadowLogits[0],
					row.shadowLogits[1],
					row.shadowLogits[2],
					row.shadowLogits[3],
					row.shadowSelectedMargin);
				const u8_t runnerUpKind =
					ResolveChampionAIShadowRunnerUp(row);
				ImGui::Text("top margin contribution vs %s: %s (#%u)  %+.5f   shadow legal=0x%08X",
					ChampionAIShadowCandidateName(runnerUpKind),
					ChampionAIShadowFeatureName(row.shadowTopFeatureIndex),
					static_cast<u32_t>(row.shadowTopFeatureIndex),
					row.shadowTopFeatureContribution,
					row.shadowLegalCandidateMask);
				ImGui::Text("HP self/enemy %.3f/%.3f   distance %.2f   turret %.3f   masks %08X/%08X",
					row.selfHpRatio,
					row.enemyHpRatio,
					row.enemyDistance,
					row.turretDanger,
					row.legalCandidateMask,
					row.illegalCandidateMask);
				ImGui::Text("Command seq=%u %s/%u   block=%s   executor=%s/%s   policy %.2f/%.2f",
					row.commandSequence,
					ChampionAICommandKindName(row.commandKind),
					static_cast<u32_t>(row.commandSlot),
					ChampionAIBlockReasonName(row.blockReason),
					AIExecutorStateName(row.executorState),
					AIExecutorReasonName(row.executorReason),
					row.skillCastIntervalRemainingSec,
					row.skillCastIntervalSec);
			}
		}
	}

	if (ImGui::CollapsingHeader("Server Minions"))
	{
	ImGui::TextDisabled("Snapshot readout only. Server gameplay tuning comes from the active definition pack.");

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
	}

	ImGui::End();
}
#endif
