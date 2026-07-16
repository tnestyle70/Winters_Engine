#pragma once

#include "LoLMatchContext.h"
#include "Shared/GameSim/Definitions/DamageTypes.h"
#include "SkillTypes.h"
#include "WintersTypes.h"

inline constexpr u8_t kSkillAtomStageMax = 2;
inline constexpr u8_t kSkillEffectParamMax = 16;
inline constexpr u8_t kSkillRankValueMax = 5;

struct SkillSlotBinding
{
    bool_t bValid = false;
    eChampion champion = eChampion::END;
    u8_t slot = 0;
    u16_t skillId = 0;
};

struct SkillTargetSpec
{
    bool_t bValid = false;
    eTargetShape shape[kSkillAtomStageMax] = { eTargetShape::Self, eTargetShape::Self };
    eTargetResolvePolicy resolvePolicy = eTargetResolvePolicy::Direct;
};

struct SkillCostSpec
{
    u8_t rankCount = 1;
    f32_t manaCostByRank[kSkillRankValueMax]{};
    // Legacy client SkillDef adapter only. Canonical gameplay pack uses the array above.
    f32_t manaCost = 0.f;
};

struct SkillCooldownSpec
{
    u8_t rankCount = 1;
    f32_t cooldownSecByRank[kSkillRankValueMax]{};
    // Legacy client SkillDef adapter only. Canonical gameplay pack uses the array above.
    f32_t cooldownSec = 0.f;
};

struct SkillRangeSpec
{
    f32_t rangeMax = 0.f;
};

struct SkillStageSpec
{
    u8_t stageCount = 1;
    f32_t stageWindowSec = 0.f;
    f32_t lockDurationSec[kSkillAtomStageMax] = {};
};

struct SkillFacingSpec
{
    eSkillFacingMode mode[kSkillAtomStageMax] =
    {
        eSkillFacingMode::None,
        eSkillFacingMode::None,
    };
};

enum class eSkillEffectParamId : u8_t
{
    None = 0,
    BaseDamage,
    DamagePerRank,
    Range,
    Speed,
    MoveSpeedMul,
    StunDurationSec,
    SlowDurationSec,
    AirborneDurationSec,
    MarkDurationSec,
    StackWindowSec,
    Gap,
    DashDistance,
    DashDurationSec,
    TargetDashDurationSec,
    HalfAngleCos,
    Radius,
    ShieldDurationSec,
    ShieldBaseAmount,
    ShieldAmountPerRank,
    ShieldArmorPerRank,
    DashDelaySec,
    EffectDurationSec,
    TickIntervalSec,
    RefreshDurationSec,
    VanishDurationSec,
    MissingHealthDamageRatio,
    MinHealthAmount,
    HealBaseAmount,
    HealAmountPerRank,
    RectLength,
    RectWidth,
    HalfWidth,
    DisarmDurationSec,
    TornadoSpeed,
    TornadoDurationSec,
    TornadoRadius,
    TornadoDamage,
    DashAreaRadius,
    DashAreaDamage,
    BonusAd,
    BonusAttackSpeed,
    TotalAdRatio,
    BonusAdRatio,
    ApRatio,
    NonEpicBaseDamage,
    NonEpicDamagePerRank,
    CooldownRefundSec,
    ManaRestoreFlat,
    CastTimeSec,
    ManaCostPerRank,
    CooldownReductionPerRank,
    MaxStacks,
    RectLengthPerRank,
    FormationDelaySec,
    DamagePerSpear,
};

enum class eSummonPolicyParamId : u8_t
{
    None = 0,
    DurationSec,
    MoveSpeed,
    AttackRange,
    SightRange,
    AttackCooldownSec,
    BaseAttackDamage,
    AttackDamagePerRank,
    BaseHp,
    HpPerRank,
    Radius,
    RoleType,
    Lane,
};

struct SkillEffectParam
{
    eSkillEffectParamId id = eSkillEffectParamId::None;
    f32_t value = 0.f;
};

struct SummonPolicyParam
{
    eSummonPolicyParamId id = eSummonPolicyParamId::None;
    f32_t value = 0.f;
};

struct SkillEffectSpec
{
    DamageFormulaDef damage{};
    u16_t scalingTableId = 0;
    u32_t gameplayPolicyId = 0;
    u32_t replicatedCueId = 0;
    u8_t paramCount = 0;
    SkillEffectParam params[kSkillEffectParamMax] = {};
};

inline constexpr u8_t kSummonPolicyParamMax = 16;

struct SummonPolicySpec
{
    bool_t bValid = false;
    u8_t paramCount = 0;
    SummonPolicyParam params[kSummonPolicyParamMax] = {};
};

inline const SkillEffectParam* FindSkillEffectParam(
    const SkillEffectSpec& effect,
    eSkillEffectParamId id)
{
    for (u8_t index = 0; index < effect.paramCount && index < kSkillEffectParamMax; ++index)
    {
        if (effect.params[index].id == id)
        {
            return &effect.params[index];
        }
    }

    return nullptr;
}

inline f32_t ResolveSkillEffectParam(
    const SkillEffectSpec& effect,
    eSkillEffectParamId id,
    f32_t fallbackValue = 0.f)
{
    if (const SkillEffectParam* param = FindSkillEffectParam(effect, id))
    {
        return param->value;
    }

    return fallbackValue;
}

inline const SummonPolicyParam* FindSummonPolicyParam(
    const SummonPolicySpec& policy,
    eSummonPolicyParamId id)
{
    for (u8_t index = 0; index < policy.paramCount && index < kSummonPolicyParamMax; ++index)
    {
        if (policy.params[index].id == id)
        {
            return &policy.params[index];
        }
    }

    return nullptr;
}

inline f32_t ResolveSummonPolicyParam(
    const SummonPolicySpec& policy,
    eSummonPolicyParamId id,
    f32_t fallbackValue = 0.f)
{
    if (const SummonPolicyParam* param = FindSummonPolicyParam(policy, id))
    {
        return param->value;
    }

    return fallbackValue;
}

struct SkillGameAtomBundle
{
    bool_t bValid = false;
    SkillSlotBinding slot{};
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillEffectSpec effect{};
    SummonPolicySpec summonPolicy{};
};
