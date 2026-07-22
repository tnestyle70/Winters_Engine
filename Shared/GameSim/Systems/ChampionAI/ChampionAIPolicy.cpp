#include "Shared/GameSim/Systems/ChampionAI/ChampionAIPolicy.h"
#include "Shared/GameSim/Generated/ChampionAIPolicyData.generated.inl"
#include "Shared/GameSim/Systems/SkillRank/SkillRankSystem.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstring>
#include <limits>

namespace
{
    std::atomic<const ChampionAIRuntimeDefinitionPack*> g_runtimeChampionAIPack{ nullptr };
}

const ChampionAIProfile& GetChampionAIProfile(eChampion champion)
{
    if (const ChampionAIRuntimeDefinitionPack* runtime =
        g_runtimeChampionAIPack.load(std::memory_order_acquire))
    {
        for (std::size_t index = 0u; index < runtime->profileCount; ++index)
        {
            if (runtime->profiles[index].champion == champion)
                return runtime->profiles[index];
        }
        if (runtime->defaultProfile)
            return *runtime->defaultProfile;
    }
    for (const ChampionAIProfile& profile : ChampionAIDataGenerated::kProfiles)
    {
        if (profile.champion == champion)
            return profile;
    }
    return ChampionAIDataGenerated::kDefaultProfile;
}

const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion)
{
    if (const ChampionAIRuntimeDefinitionPack* runtime =
        g_runtimeChampionAIPack.load(std::memory_order_acquire))
    {
        for (std::size_t index = 0u; index < runtime->comboOverrideCount; ++index)
        {
            if (runtime->comboOverrides[index].champion == champion)
                return runtime->comboOverrides[index].plan;
        }
        if (runtime->defaultComboPlan)
            return *runtime->defaultComboPlan;
    }
    for (const ChampionAIDataGenerated::ComboEntry& entry : ChampionAIDataGenerated::kComboPlans)
    {
        if (entry.champion == champion)
            return entry.plan;
    }
    return ChampionAIDataGenerated::kDefaultComboPlan;
}

u8_t ResolveChampionAISkillLevelSlot(
    const ChampionAIProfile& profile,
    const SkillRankComponent& ranks,
    u8_t championLevel)
{
    if (ranks.pointsAvailable == 0u)
        return static_cast<u8_t>(eSkillSlot::SLOT_END);

    const auto canLevel = [&](u8_t slot)
    {
        SkillRankComponent candidate = ranks;
        return CSkillRankSystem::TryLevelSkill(candidate, championLevel, slot);
    };

    const u8_t ultimateSlot = static_cast<u8_t>(eSkillSlot::R);
    if (canLevel(ultimateSlot))
        return ultimateSlot;

    bool_t evaluated[4]{};
    const u8_t ruleCount = (std::min)(profile.skillRuleCount, static_cast<u8_t>(4u));
    for (u8_t candidateIndex = 0u; candidateIndex < ruleCount; ++candidateIndex)
    {
        u8_t bestIndex = ruleCount;
        f32_t bestScore = -1.f;
        for (u8_t index = 0u; index < ruleCount; ++index)
        {
            const ChampionAISkillRule& rule = profile.skillRules[index];
            if (evaluated[index] || rule.slot == ultimateSlot)
                continue;
            if (bestIndex == ruleCount || rule.score > bestScore)
            {
                bestIndex = index;
                bestScore = rule.score;
            }
        }

        if (bestIndex == ruleCount)
            break;
        evaluated[bestIndex] = true;
        const u8_t slot = profile.skillRules[bestIndex].slot;
        if (canLevel(slot))
            return slot;
    }

    constexpr u8_t fallbackSlots[] =
    {
        static_cast<u8_t>(eSkillSlot::Q),
        static_cast<u8_t>(eSkillSlot::W),
        static_cast<u8_t>(eSkillSlot::E),
    };
    for (u8_t slot : fallbackSlots)
    {
        if (canLevel(slot))
            return slot;
    }

    return static_cast<u8_t>(eSkillSlot::SLOT_END);
}

void PublishChampionAIRuntimeDefinitions(const ChampionAIRuntimeDefinitionPack* pack)
{
    g_runtimeChampionAIPack.store(pack, std::memory_order_release);
}

namespace
{
    constexpr size_t kChampionAIShadowHeaderBytesV1 = 56u;
    constexpr size_t kChampionAIShadowFileBytesV1 =
        kChampionAIShadowHeaderBytesV1 +
        static_cast<size_t>(kChampionAIShadowFeatureCountV1) * sizeof(f32_t) * 3u;
    constexpr u16_t kChampionAIShadowScalarFloat32V1 = 1u;
    constexpr u8_t kChampionAIShadowMagicV1[8] =
    {
        'W', 'B', 'C', 'P', 'O', 'L', '1', 0u,
    };

    class ChampionAIShadowBinaryReader final
    {
    public:
        ChampionAIShadowBinaryReader(const u8_t* bytes, size_t byteCount)
            : m_bytes(bytes), m_byteCount(byteCount)
        {
        }

        bool_t ReadU8(u8_t& outValue)
        {
            if (!CanRead(1u))
                return false;
            outValue = m_bytes[m_offset++];
            return true;
        }

        bool_t ReadU16(u16_t& outValue)
        {
            if (!CanRead(2u))
                return false;
            outValue = static_cast<u16_t>(m_bytes[m_offset]) |
                static_cast<u16_t>(static_cast<u16_t>(m_bytes[m_offset + 1u]) << 8u);
            m_offset += 2u;
            return true;
        }

        bool_t ReadU32(u32_t& outValue)
        {
            if (!CanRead(4u))
                return false;
            outValue = 0u;
            for (u32_t shift = 0u; shift < 32u; shift += 8u)
                outValue |= static_cast<u32_t>(m_bytes[m_offset++]) << shift;
            return true;
        }

        bool_t ReadU64(u64_t& outValue)
        {
            if (!CanRead(8u))
                return false;
            outValue = 0u;
            for (u32_t shift = 0u; shift < 64u; shift += 8u)
                outValue |= static_cast<u64_t>(m_bytes[m_offset++]) << shift;
            return true;
        }

        bool_t ReadF32(f32_t& outValue)
        {
            u32_t bits = 0u;
            if (!ReadU32(bits))
                return false;
            static_assert(sizeof(bits) == sizeof(outValue));
            std::memcpy(&outValue, &bits, sizeof(outValue));
            return true;
        }

        bool_t ReadMagic()
        {
            for (u8_t expected : kChampionAIShadowMagicV1)
            {
                u8_t actual = 0u;
                if (!ReadU8(actual) || actual != expected)
                    return false;
            }
            return true;
        }

        size_t Offset() const
        {
            return m_offset;
        }

    private:
        bool_t CanRead(size_t count) const
        {
            return m_bytes != nullptr &&
                m_offset <= m_byteCount &&
                count <= m_byteCount - m_offset;
        }

        const u8_t* m_bytes = nullptr;
        size_t m_byteCount = 0u;
        size_t m_offset = 0u;
    };

    bool_t IsFiniteArtifact(
        const ChampionAIShadowPolicyArtifactV1& artifact)
    {
        if (artifact.policyRevision == 0u ||
            artifact.policyRevision <= artifact.sourcePolicyRevision ||
            artifact.featureOrderSha256Prefix !=
                kChampionAIShadowFeatureOrderSha256PrefixV1)
        {
            return false;
        }

        const auto isCanonicalFloat = [](f32_t value)
        {
            return std::isfinite(value) &&
                std::fpclassify(value) != FP_SUBNORMAL;
        };
        for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
        {
            if (!isCanonicalFloat(artifact.normalizationMean[i]) ||
                !isCanonicalFloat(artifact.normalizationInverseScale[i]) ||
                artifact.normalizationInverseScale[i] <= 0.f ||
                !isCanonicalFloat(artifact.weights[i]))
            {
                return false;
            }
        }
        return true;
    }

    bool_t IsValidTraceHeader(const AiDecisionTraceV1& trace)
    {
        if (trace.schemaVersion != kAiDecisionTraceSchemaVersionV1 ||
            trace.byteSize != sizeof(AiDecisionTraceV1) ||
            trace.observation.schemaVersion != kAiObservationSchemaVersionV1 ||
            trace.observation.byteSize != sizeof(AiObservationV1) ||
            trace.actionMask.schemaVersion != kAiActionMaskSchemaVersionV1 ||
            trace.actionMask.byteSize != sizeof(AiActionMaskV1) ||
            trace.candidateCount != kChampionAIShadowCandidateCountV1)
        {
            return false;
        }

        const u32_t legalMask = trace.actionMask.legalCandidateMask;
        const u32_t illegalMask = trace.actionMask.illegalCandidateMask;
        if ((legalMask & ~kAiAllCandidateBitsV1) != 0u ||
            (illegalMask & ~kAiAllCandidateBitsV1) != 0u ||
            (legalMask & illegalMask) != 0u ||
            (legalMask | illegalMask) != kAiAllCandidateBitsV1)
        {
            return false;
        }

        const AiObservationV1& observation = trace.observation;
        const f32_t observedValues[] =
        {
            observation.selfHpRatio,
            observation.enemyHpRatio,
            observation.selfGold,
            observation.enemyGold,
            observation.enemyDistance,
            observation.attackRange,
            observation.turretDanger,
        };
        for (f32_t value : observedValues)
        {
            if (!std::isfinite(value))
                return false;
        }

        u32_t seenKinds = 0u;
        for (u8_t i = 0u; i < trace.candidateCount; ++i)
        {
            const AiCandidateEvidenceV1& candidate = trace.candidates[i];
            if (candidate.schemaVersion != kAiCandidateEvidenceSchemaVersionV1 ||
                candidate.byteSize != sizeof(AiCandidateEvidenceV1) ||
                candidate.candidateKind < static_cast<u8_t>(AiCandidateKindV1::Retreat) ||
                candidate.candidateKind > static_cast<u8_t>(AiCandidateKindV1::Siege))
            {
                return false;
            }

            const u32_t bit = 1u << (candidate.candidateKind - 1u);
            if ((seenKinds & bit) != 0u)
                return false;
            seenKinds |= bit;

            const bool_t bMaskLegal = (legalMask & bit) != 0u;
            const bool_t bFlagLegal =
                (candidate.flags & kAiCandidateLegalFlagV1) != 0u;
            if (bMaskLegal != bFlagLegal)
                return false;
        }

        return seenKinds == kAiAllCandidateBitsV1;
    }

    const AiCandidateEvidenceV1* FindCandidate(
        const AiDecisionTraceV1& trace,
        u8_t candidateKind)
    {
        for (u8_t i = 0u; i < trace.candidateCount; ++i)
        {
            if (trace.candidates[i].candidateKind == candidateKind)
                return &trace.candidates[i];
        }
        return nullptr;
    }

    u8_t ResolveTargetRelation(
        const AiObservationV1& observation,
        u32_t targetNetEntityId)
    {
        if (targetNetEntityId == 0u)
            return 0u;
        if (targetNetEntityId == observation.selfNetEntityId)
            return 1u;
        if (targetNetEntityId == observation.enemyChampionNetEntityId)
            return 2u;
        if (targetNetEntityId == observation.enemyMinionNetEntityId)
            return 3u;
        if (targetNetEntityId == observation.enemyStructureNetEntityId)
            return 4u;
        if (targetNetEntityId == observation.alliedWaveNetEntityId)
            return 5u;
        return 6u;
    }

    void BuildChampionAIShadowFeatures(
        const AiDecisionTraceV1& trace,
        const AiCandidateEvidenceV1& candidate,
        f64_t (&outFeatures)[kChampionAIShadowFeatureCountV1])
    {
        for (f64_t& value : outFeatures)
            value = 0.0;

        const u8_t candidateKind = candidate.candidateKind;
        outFeatures[candidateKind - 1u] = 1.0;
        outFeatures[4u + ResolveTargetRelation(
            trace.observation,
            candidate.targetNetEntityId)] = 1.0;

        const f64_t context[14] =
        {
            static_cast<f64_t>(trace.observation.capabilityFlags),
            static_cast<f64_t>(trace.observation.selfLevel),
            static_cast<f64_t>(trace.observation.enemyLevel),
            static_cast<f64_t>(trace.observation.selfHpRatio),
            static_cast<f64_t>(trace.observation.enemyHpRatio),
            static_cast<f64_t>(trace.observation.selfGold),
            static_cast<f64_t>(trace.observation.enemyGold),
            static_cast<f64_t>(trace.observation.enemyDistance),
            static_cast<f64_t>(trace.observation.attackRange),
            static_cast<f64_t>(trace.observation.turretDanger),
            static_cast<f64_t>(trace.actionMask.legalCandidateMask),
            static_cast<f64_t>(trace.actionMask.illegalCandidateMask),
            static_cast<f64_t>(trace.actionMask.availableActionMask),
            static_cast<f64_t>(trace.actionMask.availableSkillMask),
        };
        const u16_t base = static_cast<u16_t>(
            11u + (candidateKind - 1u) * 14u);
        for (u16_t i = 0u; i < 14u; ++i)
            outFeatures[base + i] = context[i];
    }
}

bool_t DecodeChampionAIShadowPolicyArtifactV1(
    const u8_t* bytes,
    size_t byteCount,
    ChampionAIShadowPolicyArtifactV1& outArtifact)
{
    if (bytes == nullptr || byteCount != kChampionAIShadowFileBytesV1)
        return false;

    ChampionAIShadowBinaryReader reader(bytes, byteCount);
    u16_t artifactSchemaVersion = 0u;
    u16_t headerBytes = 0u;
    u32_t fileBytes = 0u;
    u16_t traceSchemaVersion = 0u;
    u16_t observationSchemaVersion = 0u;
    u16_t actionSchemaVersion = 0u;
    u16_t featureCount = 0u;
    u16_t candidateCount = 0u;
    u16_t scalarType = 0u;
    u32_t reserved = 0u;
    ChampionAIShadowPolicyArtifactV1 decoded{};

    if (!reader.ReadMagic() ||
        !reader.ReadU16(artifactSchemaVersion) ||
        !reader.ReadU16(headerBytes) ||
        !reader.ReadU32(fileBytes) ||
        !reader.ReadU16(traceSchemaVersion) ||
        !reader.ReadU16(observationSchemaVersion) ||
        !reader.ReadU16(actionSchemaVersion) ||
        !reader.ReadU16(featureCount) ||
        !reader.ReadU16(candidateCount) ||
        !reader.ReadU16(scalarType) ||
        !reader.ReadU64(decoded.policyRevision) ||
        !reader.ReadU64(decoded.sourcePolicyRevision) ||
        !reader.ReadU64(decoded.featureOrderSha256Prefix) ||
        !reader.ReadU32(reserved))
    {
        return false;
    }

    if (artifactSchemaVersion != kChampionAIShadowPolicySchemaVersionV1 ||
        headerBytes != kChampionAIShadowHeaderBytesV1 ||
        fileBytes != byteCount ||
        traceSchemaVersion != kAiDecisionTraceSchemaVersionV1 ||
        observationSchemaVersion != kAiObservationSchemaVersionV1 ||
        actionSchemaVersion != kAiActionMaskSchemaVersionV1 ||
        featureCount != kChampionAIShadowFeatureCountV1 ||
        candidateCount != kChampionAIShadowCandidateCountV1 ||
        scalarType != kChampionAIShadowScalarFloat32V1 ||
        reserved != 0u ||
        reader.Offset() != kChampionAIShadowHeaderBytesV1)
    {
        return false;
    }

    for (u16_t i = 0u; i < featureCount; ++i)
    {
        if (!reader.ReadF32(decoded.normalizationMean[i]))
            return false;
    }
    for (u16_t i = 0u; i < featureCount; ++i)
    {
        if (!reader.ReadF32(decoded.normalizationInverseScale[i]))
            return false;
    }
    for (u16_t i = 0u; i < featureCount; ++i)
    {
        if (!reader.ReadF32(decoded.weights[i]))
            return false;
    }

    if (reader.Offset() != byteCount || !IsFiniteArtifact(decoded))
        return false;

    outArtifact = decoded;
    return true;
}

ChampionAIShadowDecisionV1 EvaluateChampionAIShadowPolicyV1(
    const ChampionAIShadowPolicyArtifactV1* artifact,
    const AiDecisionTraceV1& trace)
{
    ChampionAIShadowDecisionV1 result{};
    if (artifact == nullptr)
        return result;
    if (!IsFiniteArtifact(*artifact))
    {
        result.status = eChampionAIShadowStatusV1::InvalidArtifact;
        return result;
    }
    if (!IsValidTraceHeader(trace))
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }

    result.legalCandidateMask = trace.actionMask.legalCandidateMask;
    result.activeCandidateKind = trace.selectedCandidateKind;
    if (result.activeCandidateKind <
            static_cast<u8_t>(AiCandidateKindV1::Retreat) ||
        result.activeCandidateKind >
            static_cast<u8_t>(AiCandidateKindV1::Siege) ||
        (result.legalCandidateMask &
            (1u << (result.activeCandidateKind - 1u))) == 0u)
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }
    u8_t legalCount = 0u;
    for (u8_t kind = 1u; kind <= kChampionAIShadowCandidateCountV1; ++kind)
    {
        if ((result.legalCandidateMask & (1u << (kind - 1u))) != 0u)
            ++legalCount;
    }
    if (legalCount < 2u)
    {
        result.status = eChampionAIShadowStatusV1::InsufficientLegalCandidates;
        return result;
    }

    f64_t bestLogit = -std::numeric_limits<f64_t>::infinity();
    f64_t secondLogit = -std::numeric_limits<f64_t>::infinity();
    u8_t bestKind = 0u;
    u8_t secondKind = 0u;
    for (u8_t kind = 1u; kind <= kChampionAIShadowCandidateCountV1; ++kind)
    {
        const u32_t bit = 1u << (kind - 1u);
        if ((result.legalCandidateMask & bit) == 0u)
            continue;

        const AiCandidateEvidenceV1* candidate = FindCandidate(trace, kind);
        if (candidate == nullptr)
        {
            result.status = eChampionAIShadowStatusV1::InvalidTrace;
            return result;
        }

        f64_t features[kChampionAIShadowFeatureCountV1]{};
        BuildChampionAIShadowFeatures(trace, *candidate, features);
        f64_t logit = 0.0;
        for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
        {
            logit +=
                (features[i] - static_cast<f64_t>(artifact->normalizationMean[i])) *
                static_cast<f64_t>(artifact->normalizationInverseScale[i]) *
                static_cast<f64_t>(artifact->weights[i]);
        }
        if (!std::isfinite(logit) ||
            logit > static_cast<f64_t>(std::numeric_limits<f32_t>::max()) ||
            logit < -static_cast<f64_t>(std::numeric_limits<f32_t>::max()))
        {
            result.status = eChampionAIShadowStatusV1::InvalidArtifact;
            return result;
        }
        result.logits[kind - 1u] = static_cast<f32_t>(logit);

        if (logit > bestLogit)
        {
            secondLogit = bestLogit;
            secondKind = bestKind;
            bestLogit = logit;
            bestKind = kind;
        }
        else if (logit > secondLogit)
        {
            secondLogit = logit;
            secondKind = kind;
        }
    }

    if (bestKind == 0u || secondKind == 0u ||
        !std::isfinite(bestLogit) || !std::isfinite(secondLogit))
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }

    result.status = eChampionAIShadowStatusV1::Evaluated;
    result.shadowCandidateKind = bestKind;
    result.bDisagreed = result.activeCandidateKind != bestKind;
    result.selectedMargin = static_cast<f32_t>(bestLogit - secondLogit);

    const AiCandidateEvidenceV1* selectedCandidate = FindCandidate(trace, bestKind);
    const AiCandidateEvidenceV1* runnerUpCandidate =
        FindCandidate(trace, secondKind);
    if (selectedCandidate == nullptr || runnerUpCandidate == nullptr)
    {
        result.status = eChampionAIShadowStatusV1::InvalidTrace;
        return result;
    }
    f64_t selectedFeatures[kChampionAIShadowFeatureCountV1]{};
    f64_t runnerUpFeatures[kChampionAIShadowFeatureCountV1]{};
    BuildChampionAIShadowFeatures(trace, *selectedCandidate, selectedFeatures);
    BuildChampionAIShadowFeatures(trace, *runnerUpCandidate, runnerUpFeatures);
    f64_t greatestAbsoluteContribution = -1.0;
    for (u16_t i = 0u; i < kChampionAIShadowFeatureCountV1; ++i)
    {
        // The normalization mean cancels in the selected-vs-runner-up
        // logit margin. Reporting this delta answers why the winner ranked
        // above its nearest alternative instead of showing a common offset.
        const f64_t contribution =
            (selectedFeatures[i] - runnerUpFeatures[i]) *
            static_cast<f64_t>(artifact->normalizationInverseScale[i]) *
            static_cast<f64_t>(artifact->weights[i]);
        const f64_t absoluteContribution = std::abs(contribution);
        if (absoluteContribution > greatestAbsoluteContribution)
        {
            greatestAbsoluteContribution = absoluteContribution;
            result.topFeatureIndex = i;
            result.topFeatureContribution = static_cast<f32_t>(contribution);
        }
    }
    return result;
}
