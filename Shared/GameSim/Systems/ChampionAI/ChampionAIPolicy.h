#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/SkillDef.h"
#include "Shared/GameSim/Systems/ChampionAI/ChampionAIResearchTypes.h"
#include "WintersTypes.h"

#include <cstddef>

struct ChampionAISkillRule
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    f32_t minRange = 0.f;
    f32_t score = 0.f;
};

enum class eChampionAIComboTargetMode : u8_t
{
    TargetEntity,
    AwayFromTarget,
    WardBehindTarget,
    LastOwnWard,
    SylasHijackTarget,
    SylasStolenUltimateTarget,
};

struct ChampionAIComboStep
{
    u8_t slot = static_cast<u8_t>(eSkillSlot::BasicAttack);
    u16_t itemId = 0;
    f32_t minRange = 0.f;
    f32_t maxRange = 0.f;
    f32_t selfHpMinRatio = 0.f;
    f32_t enemyHpMaxRatio = 1.f;
    u8_t targetMode = static_cast<u8_t>(eChampionAIComboTargetMode::TargetEntity);
};

struct ChampionAIComboPlan
{
    ChampionAIComboStep steps[10]{};
    u8_t stepCount = 0;
};

struct ChampionAIProfile
{
    eChampion champion = eChampion::END;
    f32_t preferredRange = 1.5f;
    f32_t championScanRange = 6.f;
    f32_t minionScanRange = 10.f;
    f32_t structureScanRange = 18.f;
    f32_t leashRange = 14.f;
    f32_t aggression = 1.f;
    f32_t kiteBias = 0.f;
    f32_t retreatHpRatio = 0.35f;
    f32_t reengageHpRatio = 0.55f;
    f32_t minionPressureWeight = 1.f;
    f32_t turretRiskWeight = 1.f;
    f32_t lastHitWeight = 1.f;
    f32_t siegeWeight = 1.f;
    ChampionAISkillRule skillRules[4]{};
    u8_t skillRuleCount = 0;
};

struct ChampionAIComboOverride
{
    eChampion champion = eChampion::END;
    ChampionAIComboPlan plan{};
};

struct ChampionAIRuntimeDefinitionPack
{
    const ChampionAIProfile* defaultProfile = nullptr;
    const ChampionAIProfile* profiles = nullptr;
    std::size_t profileCount = 0u;
    const ChampionAIComboPlan* defaultComboPlan = nullptr;
    const ChampionAIComboOverride* comboOverrides = nullptr;
    std::size_t comboOverrideCount = 0u;
};

const ChampionAIProfile& GetChampionAIProfile(eChampion champion);
const ChampionAIComboPlan& GetChampionAIComboPlan(eChampion champion);
void PublishChampionAIRuntimeDefinitions(const ChampionAIRuntimeDefinitionPack* pack);

inline constexpr u16_t kChampionAIShadowPolicySchemaVersionV1 = 1u;
inline constexpr u16_t kChampionAIShadowFeatureCountV1 = 67u;
inline constexpr u16_t kChampionAIShadowCandidateCountV1 = 4u;
inline constexpr u16_t kChampionAIShadowInvalidFeatureIndexV1 = 0xFFFFu;
inline constexpr u64_t kChampionAIShadowFeatureOrderSha256PrefixV1 =
    0x9208820578DF2314ull;

enum class eChampionAIShadowStatusV1 : u8_t
{
    Disabled = 0u,
    Evaluated,
    InvalidArtifact,
    InvalidTrace,
    InsufficientLegalCandidates,
};

struct ChampionAIShadowPolicyArtifactV1
{
    u64_t policyRevision = 0u;
    u64_t sourcePolicyRevision = 0u;
    u64_t featureOrderSha256Prefix = 0u;
    u64_t binarySha256Prefix = 0u;
    f32_t normalizationMean[kChampionAIShadowFeatureCountV1]{};
    f32_t normalizationInverseScale[kChampionAIShadowFeatureCountV1]{};
    f32_t weights[kChampionAIShadowFeatureCountV1]{};
};

struct ChampionAIShadowDecisionV1
{
    eChampionAIShadowStatusV1 status = eChampionAIShadowStatusV1::Disabled;
    u8_t activeCandidateKind = 0u;
    u8_t shadowCandidateKind = 0u;
    bool_t bDisagreed = false;
    u32_t legalCandidateMask = 0u;
    f32_t logits[kChampionAIShadowCandidateCountV1]{};
    f32_t selectedMargin = 0.f;
    u16_t topFeatureIndex = kChampionAIShadowInvalidFeatureIndexV1;
    f32_t topFeatureContribution = 0.f;
};

bool_t DecodeChampionAIShadowPolicyArtifactV1(
    const u8_t* bytes,
    size_t byteCount,
    ChampionAIShadowPolicyArtifactV1& outArtifact);

ChampionAIShadowDecisionV1 EvaluateChampionAIShadowPolicyV1(
    const ChampionAIShadowPolicyArtifactV1* artifact,
    const AiDecisionTraceV1& trace);
