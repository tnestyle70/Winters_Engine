#pragma once

#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"

namespace Client
{
	inline bool_t DecodeAIDebugUtilityEvidence(
		const Shared::Schema::EntitySnapshot& snapshot,
		u64_t serverTick,
		ChampionAIDebugComponent& outDebug)
	{
		outDebug.utilityCandidateTick = 0u;
		outDebug.utilitySelectionTick = 0u;
		outDebug.utilityDecision = ChampionAIResearch::MakeDecisionTraceV1();

		const auto* pKinds = snapshot.aiDebugCandidateKinds();
		const auto* pFlags = snapshot.aiDebugCandidateFlags();
		const auto* pTargets = snapshot.aiDebugCandidateTargets();
		const auto* pScores = snapshot.aiDebugCandidateScores();
		const auto* pTermCounts = snapshot.aiDebugCandidateTermCounts();
		const auto* pFeatureIds = snapshot.aiDebugCandidateTermFeatureIds();
		const auto* pRawValues = snapshot.aiDebugCandidateTermRawValues();
		const auto* pWeights = snapshot.aiDebugCandidateTermWeights();
		const auto* pContributions = snapshot.aiDebugCandidateTermContributions();
		constexpr u32_t kCandidateCount = kAiDecisionCandidateCapacityV1;
		constexpr u32_t kTermCount =
			kCandidateCount * kAiFeatureContributionCapacityV1;
		if (snapshot.aiDebugCandidateTick() != serverTick ||
			pKinds == nullptr || pKinds->size() != kCandidateCount ||
			pFlags == nullptr || pFlags->size() != kCandidateCount ||
			pTargets == nullptr || pTargets->size() != kCandidateCount ||
			pScores == nullptr || pScores->size() != kCandidateCount ||
			pTermCounts == nullptr || pTermCounts->size() != kCandidateCount ||
			pFeatureIds == nullptr || pFeatureIds->size() != kTermCount ||
			pRawValues == nullptr || pRawValues->size() != kTermCount ||
			pWeights == nullptr || pWeights->size() != kTermCount ||
			pContributions == nullptr || pContributions->size() != kTermCount)
		{
			return false;
		}

		AiDecisionTraceV1& decision = outDebug.utilityDecision;
		decision.tick = serverTick;
		decision.candidateCount = static_cast<u8_t>(kCandidateCount);
		const bool_t bSelectedThisTick =
			snapshot.aiDebugSelectionTick() == serverTick;
		outDebug.utilityCandidateTick = serverTick;
		outDebug.utilitySelectionTick = bSelectedThisTick ? serverTick : 0u;
		for (u32_t candidateIndex = 0u;
			candidateIndex < kCandidateCount;
			++candidateIndex)
		{
			AiCandidateEvidenceV1& candidate = decision.candidates[candidateIndex];
			candidate.candidateKind = pKinds->Get(candidateIndex);
			candidate.flags = pFlags->Get(candidateIndex);
			if (!bSelectedThisTick)
				candidate.flags &= static_cast<u8_t>(~kAiCandidateSelectedFlagV1);
			candidate.targetNetEntityId = pTargets->Get(candidateIndex);
			candidate.score = pScores->Get(candidateIndex);
			candidate.contributionCount = pTermCounts->Get(candidateIndex);
			if (candidate.contributionCount > kAiFeatureContributionCapacityV1)
			{
				outDebug.utilityCandidateTick = 0u;
				outDebug.utilitySelectionTick = 0u;
				outDebug.utilityDecision = ChampionAIResearch::MakeDecisionTraceV1();
				return false;
			}
			for (u32_t termIndex = 0u;
				termIndex < candidate.contributionCount;
				++termIndex)
			{
				const u32_t flatIndex = candidateIndex *
					kAiFeatureContributionCapacityV1 + termIndex;
				AiFeatureContributionV1& term = candidate.contributions[termIndex];
				term.featureId = pFeatureIds->Get(flatIndex);
				term.rawValue = pRawValues->Get(flatIndex);
				term.weight = pWeights->Get(flatIndex);
				term.contribution = pContributions->Get(flatIndex);
			}
			if ((candidate.flags & kAiCandidateSelectedFlagV1) != 0u)
				decision.selectedCandidateKind = candidate.candidateKind;
		}
		return true;
	}
}
