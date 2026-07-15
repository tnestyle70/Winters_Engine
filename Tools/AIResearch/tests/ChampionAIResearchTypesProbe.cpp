#include "Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAISystem.h"
#include "Tools/AIResearch/Native/AiDecisionTraceCaptureWriter.h"

#include <cstdio>
#include <cstring>
#include <type_traits>

namespace
{
    struct ReadOnlyWorldView
    {
        std::uint64_t factTick;
        std::uint32_t selfNetEntityId;
        std::uint32_t enemyNetEntityId;
        float selfHpRatio;
        float enemyHpRatio;
        std::uint32_t legalCandidateMask;
    };

    AiDecisionTraceV1 BuildRecord(const ReadOnlyWorldView& world)
    {
        AiDecisionTraceV1 trace = ChampionAIResearch::MakeDecisionTraceV1();
        trace.tick = world.factTick;
        trace.candidateCount = kAiDecisionCandidateCapacityV1;
        trace.observation.factTick = world.factTick;
        trace.observation.provenanceFlags =
            kAiObservationPrivilegedSourceFlagV1;
        trace.observation.selfNetEntityId = world.selfNetEntityId;
        trace.observation.enemyChampionNetEntityId = world.enemyNetEntityId;
        trace.observation.selfLevel = 6u;
        trace.observation.enemyLevel = 5u;
        trace.observation.selfHpRatio = world.selfHpRatio;
        trace.observation.enemyHpRatio = world.enemyHpRatio;
        trace.actionMask.legalCandidateMask = world.legalCandidateMask;
        trace.actionMask.illegalCandidateMask =
            kAiAllCandidateBitsV1 & ~world.legalCandidateMask;

        constexpr AiCandidateKindV1 kinds[kAiDecisionCandidateCapacityV1] = {
            AiCandidateKindV1::Retreat,
            AiCandidateKindV1::Fight,
            AiCandidateKindV1::Farm,
            AiCandidateKindV1::Siege,
        };
        constexpr float scores[kAiDecisionCandidateCapacityV1] = {
            0.10f,
            0.75f,
            0.20f,
            0.00f,
        };
        for (std::uint8_t i = 0u; i < kAiDecisionCandidateCapacityV1; ++i)
        {
            AiCandidateEvidenceV1& candidate = trace.candidates[i];
            candidate.candidateKind = static_cast<std::uint8_t>(kinds[i]);
            candidate.score = scores[i];
            candidate.contributionCount = 1u;
            candidate.contributions[0].featureId = static_cast<std::uint16_t>(
                AiFeatureIdV1::UtilityScore);
            candidate.contributions[0].rawValue = candidate.score;
            candidate.contributions[0].weight = 1.f;
            candidate.contributions[0].contribution = candidate.score;
        }

        AiCandidateEvidenceV1& fight = trace.candidates[1];
        fight.flags = kAiCandidateLegalFlagV1 | kAiCandidateHasTargetFlagV1;
        fight.targetNetEntityId = world.enemyNetEntityId;
        return trace;
    }

    bool ValidateNestedVersionsAndBounds(const AiDecisionTraceV1& trace)
    {
        if (trace.schemaVersion != kAiDecisionTraceSchemaVersionV1 ||
            trace.byteSize != sizeof(AiDecisionTraceV1) ||
            trace.observation.schemaVersion != kAiObservationSchemaVersionV1 ||
            trace.observation.byteSize != sizeof(AiObservationV1) ||
            trace.actionMask.schemaVersion != kAiActionMaskSchemaVersionV1 ||
            trace.actionMask.byteSize != sizeof(AiActionMaskV1) ||
            trace.candidateCount > kAiDecisionCandidateCapacityV1)
        {
            return false;
        }

        for (std::uint8_t i = 0u; i < trace.candidateCount; ++i)
        {
            const AiCandidateEvidenceV1& candidate = trace.candidates[i];
            if (candidate.schemaVersion != kAiCandidateEvidenceSchemaVersionV1 ||
                candidate.byteSize != sizeof(AiCandidateEvidenceV1) ||
                candidate.contributionCount > kAiFeatureContributionCapacityV1)
            {
                return false;
            }

            for (std::uint8_t j = 0u; j < kAiFeatureContributionCapacityV1; ++j)
            {
                const AiFeatureContributionV1& contribution =
                    candidate.contributions[j];
                if (contribution.schemaVersion !=
                        kAiFeatureContributionSchemaVersionV1 ||
                    contribution.byteSize != sizeof(AiFeatureContributionV1))
                {
                    return false;
                }
            }
        }
        return true;
    }
}

static_assert(std::is_standard_layout_v<AiObservationV1>);
static_assert(std::is_trivial_v<AiObservationV1>);
static_assert(std::is_trivially_copyable_v<AiObservationV1>);
static_assert(std::is_standard_layout_v<AiActionMaskV1>);
static_assert(std::is_trivial_v<AiActionMaskV1>);
static_assert(std::is_trivially_copyable_v<AiActionMaskV1>);
static_assert(std::is_standard_layout_v<AiFeatureContributionV1>);
static_assert(std::is_trivial_v<AiFeatureContributionV1>);
static_assert(std::is_trivially_copyable_v<AiFeatureContributionV1>);
static_assert(std::is_standard_layout_v<AiCandidateEvidenceV1>);
static_assert(std::is_trivial_v<AiCandidateEvidenceV1>);
static_assert(std::is_trivially_copyable_v<AiCandidateEvidenceV1>);
static_assert(std::is_standard_layout_v<AiDecisionTraceV1>);
static_assert(std::is_trivial_v<AiDecisionTraceV1>);
static_assert(std::is_trivially_copyable_v<AiDecisionTraceV1>);
static_assert(sizeof(AiObservationV1) == 80u);
static_assert(sizeof(AiActionMaskV1) == 20u);
static_assert(sizeof(AiFeatureContributionV1) == 20u);
static_assert(sizeof(AiCandidateEvidenceV1) == 96u);
static_assert(sizeof(AiDecisionTraceV1) == 528u);

using ReadOnlyBuilderSignature = AiDecisionTraceV1 (*)(
    const TickContext&,
    EntityID,
    const ChampionAIComponent&,
    const ChampionAIPerception&);
static_assert(std::is_same_v<
    decltype(&CChampionAISystem::BuildResearchDecisionTrace),
    ReadOnlyBuilderSignature>);

int main(int argc, char** argv)
{
    constexpr std::uint32_t kProcessLocalEntityFixture = 7u;
    constexpr std::uint32_t kStableEnemyNetEntityId = 7007u;
    const ReadOnlyWorldView world{
        1234u,
        6001u,
        kStableEnemyNetEntityId,
        0.80f,
        0.40f,
        kAiCandidateFightBitV1,
    };
    const ReadOnlyWorldView before = world;

    const AiDecisionTraceV1 first = BuildRecord(world);
    const AiDecisionTraceV1 second = BuildRecord(world);
    const bool sameInputIsByteIdentical =
        std::memcmp(&first, &second, sizeof(first)) == 0;
    const bool sourceWasReadOnly =
        std::memcmp(&world, &before, sizeof(world)) == 0;
    const bool stableIdentityPreserved =
        first.observation.enemyChampionNetEntityId == kStableEnemyNetEntityId &&
        first.candidates[1].targetNetEntityId == kStableEnemyNetEntityId &&
        first.candidates[1].targetNetEntityId != kProcessLocalEntityFixture;
    const bool privilegedProvenanceIsExplicit =
        (first.observation.provenanceFlags &
            kAiObservationPrivilegedSourceFlagV1) != 0u &&
        (first.observation.provenanceFlags &
            kAiObservationTeamFilteredFlagV1) == 0u;
    const bool illegalMaskRepresented =
        first.actionMask.legalCandidateMask == kAiCandidateFightBitV1 &&
        (first.actionMask.illegalCandidateMask & kAiCandidateFightBitV1) == 0u &&
        (first.actionMask.illegalCandidateMask & kAiCandidateRetreatBitV1) != 0u &&
        (first.actionMask.illegalCandidateMask & kAiCandidateFarmBitV1) != 0u &&
        (first.actionMask.illegalCandidateMask & kAiCandidateSiegeBitV1) != 0u;
    const bool executorStartsUnknown =
        first.executorState == static_cast<std::uint8_t>(
            AiExecutorStateV1::Unknown) &&
        first.executorReason == 0u &&
        static_cast<std::uint8_t>(AiExecutorStateV1::Accepted) !=
            static_cast<std::uint8_t>(AiExecutorStateV1::Submitted) &&
        static_cast<std::uint8_t>(AiExecutorStateV1::Rejected) !=
            static_cast<std::uint8_t>(AiExecutorStateV1::Submitted);

    const bool pass =
        ValidateNestedVersionsAndBounds(first) &&
        sameInputIsByteIdentical &&
        sourceWasReadOnly &&
        stableIdentityPreserved &&
        privilegedProvenanceIsExplicit &&
        illegalMaskRepresented &&
        executorStartsUnknown;

    if (pass && argc == 3 && std::strcmp(argv[1], "--write-fixture") == 0)
    {
        AiDecisionTraceV1 capture = first;
        capture.selectedCandidateKind = static_cast<std::uint8_t>(
            AiCandidateKindV1::Fight);
        capture.candidates[1].flags |= kAiCandidateSelectedFlagV1;
        capture.executorState = static_cast<std::uint8_t>(
            AiExecutorStateV1::Submitted);
        capture.executorReason = 0u;
        capture.commandKind = 3u;
        capture.commandSlot = 1u;
        capture.commandTargetNetEntityId = kStableEnemyNetEntityId;
        capture.commandSequence = 17u;

        if (!Winters::AIResearchTools::WriteAiDecisionTraceCaptureV1(
                argv[2],
                &capture,
                1u))
        {
            std::printf("[AIResearchTypes] FAIL: fixture write failed\n");
            return 1;
        }
    }
    else if (argc != 1)
    {
        std::printf(
            "Usage: ChampionAIResearchTypesProbe.exe "
            "[--write-fixture output.bin]\n");
        return 1;
    }

    std::printf(
        "[AIResearchTypes] %s: POD/schema/bounds, deterministic bytes, "
        "read-only source, NetEntityId, privileged provenance, illegal mask, "
        "executor Unknown, bytes=%zu\n",
        pass ? "PASS" : "FAIL",
        sizeof(AiDecisionTraceV1));
    return pass ? 0 : 1;
}
