#pragma once

#include <cstdint>

// Research records cross process and replay boundaries. Their uint32 identity
// fields carry EntityIdMap-issued NetEntityId values, never process-local IDs.

inline constexpr std::uint16_t kAiObservationSchemaVersionV1 = 1u;
inline constexpr std::uint16_t kAiActionMaskSchemaVersionV1 = 1u;
inline constexpr std::uint16_t kAiFeatureContributionSchemaVersionV1 = 1u;
inline constexpr std::uint16_t kAiCandidateEvidenceSchemaVersionV1 = 1u;
inline constexpr std::uint16_t kAiDecisionTraceSchemaVersionV1 = 1u;

inline constexpr std::uint8_t kAiFeatureContributionCapacityV1 = 4u;
inline constexpr std::uint8_t kAiDecisionCandidateCapacityV1 = 4u;

enum class AiCandidateKindV1 : std::uint8_t
{
    None = 0u,
    Retreat = 1u,
    Fight = 2u,
    Farm = 3u,
    Siege = 4u,
};

enum class AiExecutorStateV1 : std::uint8_t
{
    Unknown = 0u,
    Submitted = 1u,
    Accepted = 2u,
    Rejected = 3u,
};

enum class AiFeatureIdV1 : std::uint16_t
{
    None = 0u,
    UtilityScore = 1u,
    PositiveOpportunity = 2u,
    TurretRisk = 3u,
    ObservedComboRisk = 4u,
    ClampOrThresholdAdjustment = 5u,
    HealthPressure = 6u,
    FarmOpportunity = 7u,
    StructureExposure = 8u,
};

inline constexpr std::uint32_t kAiObservationCanMoveFlagV1 = 1u << 0;
inline constexpr std::uint32_t kAiObservationCanAttackFlagV1 = 1u << 1;
inline constexpr std::uint32_t kAiObservationCanCastFlagV1 = 1u << 2;
inline constexpr std::uint32_t kAiObservationEnemyTargetableFlagV1 = 1u << 3;
inline constexpr std::uint32_t kAiObservationAlliedWaveNearbyFlagV1 = 1u << 4;
inline constexpr std::uint32_t kAiObservationStructureWaveTankingFlagV1 = 1u << 5;
inline constexpr std::uint32_t kAiObservationInsideEnemyTurretFlagV1 = 1u << 6;
inline constexpr std::uint32_t kAiObservationPrivilegedSourceFlagV1 = 1u << 0;
inline constexpr std::uint32_t kAiObservationTeamFilteredFlagV1 = 1u << 1;

inline constexpr std::uint32_t kAiCandidateRetreatBitV1 = 1u << 0;
inline constexpr std::uint32_t kAiCandidateFightBitV1 = 1u << 1;
inline constexpr std::uint32_t kAiCandidateFarmBitV1 = 1u << 2;
inline constexpr std::uint32_t kAiCandidateSiegeBitV1 = 1u << 3;
inline constexpr std::uint32_t kAiAllCandidateBitsV1 =
    kAiCandidateRetreatBitV1 |
    kAiCandidateFightBitV1 |
    kAiCandidateFarmBitV1 |
    kAiCandidateSiegeBitV1;

inline constexpr std::uint8_t kAiCandidateLegalFlagV1 = 1u << 0;
inline constexpr std::uint8_t kAiCandidateSelectedFlagV1 = 1u << 1;
inline constexpr std::uint8_t kAiCandidateHasTargetFlagV1 = 1u << 2;

struct AiObservationV1
{
    std::uint16_t schemaVersion;
    std::uint16_t byteSize;
    std::uint32_t capabilityFlags;
    std::uint32_t provenanceFlags;
    std::uint32_t reservedHeader;
    std::uint64_t factTick;

    std::uint32_t selfNetEntityId;
    std::uint32_t enemyChampionNetEntityId;
    std::uint32_t enemyMinionNetEntityId;
    std::uint32_t enemyStructureNetEntityId;
    std::uint32_t alliedWaveNetEntityId;

    std::uint8_t selfLevel;
    std::uint8_t enemyLevel;
    std::uint16_t reserved0;

    float selfHpRatio;
    float enemyHpRatio;
    float selfGold;
    float enemyGold;
    float enemyDistance;
    float attackRange;
    float turretDanger;
    std::uint32_t reserved1;
};

struct AiActionMaskV1
{
    std::uint16_t schemaVersion;
    std::uint16_t byteSize;
    std::uint32_t legalCandidateMask;
    std::uint32_t illegalCandidateMask;
    std::uint32_t availableActionMask;
    std::uint32_t availableSkillMask;
};

struct AiFeatureContributionV1
{
    std::uint16_t schemaVersion;
    std::uint16_t byteSize;
    std::uint16_t featureId;
    std::uint16_t reserved0;
    float rawValue;
    float weight;
    float contribution;
};

struct AiCandidateEvidenceV1
{
    std::uint16_t schemaVersion;
    std::uint16_t byteSize;
    std::uint8_t candidateKind;
    std::uint8_t flags;
    std::uint8_t contributionCount;
    std::uint8_t reserved0;
    std::uint32_t targetNetEntityId;
    float score;
    AiFeatureContributionV1 contributions[kAiFeatureContributionCapacityV1];
};

struct AiDecisionTraceV1
{
    std::uint16_t schemaVersion;
    std::uint16_t byteSize;
    std::uint8_t candidateCount;
    std::uint8_t selectedCandidateKind;
    std::uint8_t executorState;
    std::uint8_t commandKind;
    std::uint8_t commandSlot;
    std::uint8_t blockReason;
    std::uint16_t executorReason;
    std::uint8_t state;
    std::uint8_t intent;
    std::uint8_t action;
    std::uint8_t reserved0[1];
    std::uint64_t tick;

    AiObservationV1 observation;
    AiActionMaskV1 actionMask;
    AiCandidateEvidenceV1 candidates[kAiDecisionCandidateCapacityV1];

    std::uint32_t commandTargetNetEntityId;
    std::uint32_t commandSequence;
    float commandPositionX;
    float commandPositionY;
    float commandPositionZ;
};

namespace ChampionAIResearch
{
    inline AiObservationV1 MakeObservationV1() noexcept
    {
        AiObservationV1 value{};
        value.schemaVersion = kAiObservationSchemaVersionV1;
        value.byteSize = static_cast<std::uint16_t>(sizeof(AiObservationV1));
        return value;
    }

    inline AiActionMaskV1 MakeActionMaskV1() noexcept
    {
        AiActionMaskV1 value{};
        value.schemaVersion = kAiActionMaskSchemaVersionV1;
        value.byteSize = static_cast<std::uint16_t>(sizeof(AiActionMaskV1));
        return value;
    }

    inline AiFeatureContributionV1 MakeFeatureContributionV1() noexcept
    {
        AiFeatureContributionV1 value{};
        value.schemaVersion = kAiFeatureContributionSchemaVersionV1;
        value.byteSize =
            static_cast<std::uint16_t>(sizeof(AiFeatureContributionV1));
        return value;
    }

    inline AiCandidateEvidenceV1 MakeCandidateEvidenceV1() noexcept
    {
        AiCandidateEvidenceV1 value{};
        value.schemaVersion = kAiCandidateEvidenceSchemaVersionV1;
        value.byteSize = static_cast<std::uint16_t>(sizeof(AiCandidateEvidenceV1));
        for (std::uint8_t i = 0u; i < kAiFeatureContributionCapacityV1; ++i)
            value.contributions[i] = MakeFeatureContributionV1();
        return value;
    }

    inline AiDecisionTraceV1 MakeDecisionTraceV1() noexcept
    {
        AiDecisionTraceV1 value{};
        value.schemaVersion = kAiDecisionTraceSchemaVersionV1;
        value.byteSize = static_cast<std::uint16_t>(sizeof(AiDecisionTraceV1));
        value.executorState = static_cast<std::uint8_t>(
            AiExecutorStateV1::Unknown);
        value.observation = MakeObservationV1();
        value.actionMask = MakeActionMaskV1();
        for (std::uint8_t i = 0u; i < kAiDecisionCandidateCapacityV1; ++i)
            value.candidates[i] = MakeCandidateEvidenceV1();
        return value;
    }
}
