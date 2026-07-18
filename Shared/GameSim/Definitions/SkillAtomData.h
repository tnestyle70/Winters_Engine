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

struct SkillInputSpec
{
    eSkillInputActivation activation = eSkillInputActivation::Press;
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
    // Animation/presentation duration. Kept under the legacy field name until
    // SkillDef visual timing is fully retired.
    f32_t lockDurationSec[kSkillAtomStageMax] = {};
    // Server command/action lock. This is intentionally independent from the
    // animation duration above.
    f32_t commandLockSec[kSkillAtomStageMax] = {};
    eSkillActionMovePolicy movePolicy[kSkillAtomStageMax] =
    {
        eSkillActionMovePolicy::Allow,
        eSkillActionMovePolicy::Allow,
    };
    bool_t bCreatesActionState[kSkillAtomStageMax] = { true, true };
    bool_t bPresentationLoopWhileActive[kSkillAtomStageMax] = {};
};

struct SkillChargeSpec
{
    bool_t bEnabled = false;
    bool_t bAutoRelease = false;
    u8_t reserved[2]{};
    f32_t maxHoldSec = 0.f;
    f32_t minRangeScale = 1.f;
    f32_t maxRangeScale = 1.f;
    f32_t minDamageScale = 1.f;
    f32_t maxDamageScale = 1.f;
    f32_t minStunSec = 0.f;
    f32_t maxStunSec = 0.f;
};

inline f32_t ResolveSkillChargeRatio(
    u64_t startTick,
    u64_t maxReleaseTick,
    u64_t currentTick)
{
    if (maxReleaseTick <= startTick || currentTick <= startTick)
        return 0.f;
    if (currentTick >= maxReleaseTick)
        return 1.f;

    return static_cast<f32_t>(currentTick - startTick) /
        static_cast<f32_t>(maxReleaseTick - startTick);
}

inline f32_t ResolveSkillChargeValue(
    f32_t minValue,
    f32_t maxValue,
    f32_t chargeRatio)
{
    const f32_t clampedRatio = chargeRatio < 0.f
        ? 0.f
        : (chargeRatio > 1.f ? 1.f : chargeRatio);
    return minValue + (maxValue - minValue) * clampedRatio;
}

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
    TargetHealthThresholdRatio,
    AcquireRange,
    LifetimeSec,
    RespawnSec,
    SideDotThreshold,
    TargetMaxHpRatio,
    ChallengeDurationSec,
    HealDurationSec,
    HealRadius,
    HealIntervalSec,
    HealAmount,
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
    SkillInputSpec input{};
    SkillTargetSpec target{};
    SkillCostSpec cost{};
    SkillCooldownSpec cooldown{};
    SkillRangeSpec range{};
    SkillStageSpec stage{};
    SkillFacingSpec facing{};
    SkillChargeSpec charge{};
    SkillEffectSpec effect{};
    SummonPolicySpec summonPolicy{};
};
