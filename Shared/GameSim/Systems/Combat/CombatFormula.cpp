#include "Shared/GameSim/Systems/Combat/CombatFormula.h"

#include <algorithm>
#include <cmath>

f32_t CCombatFormula::GrowthMultiplier(u8_t level)
{
    if (level <= 1)
        return 0.f;

    const f32_t n = static_cast<f32_t>(level - 1);
    return n * (0.7025f + 0.0175f * n);
}

f32_t CCombatFormula::ResolveStatAtLevel(f32_t baseValue, f32_t growthValue, u8_t level)
{
    return baseValue + growthValue * GrowthMultiplier(level);
}

f32_t CCombatFormula::EffectiveResistance(
    f32_t baseResist,
    f32_t bonusResist,
    const ResistBreakdown& pen)
{
    f32_t basePart = baseResist;
    f32_t bonusPart = bonusResist;

    const f32_t totalBeforeFlat = basePart + bonusPart;
    if (pen.flatReduction != 0.f)
    {
        if (std::abs(totalBeforeFlat) > 0.000001f)
        {
            const f32_t baseShare = basePart / totalBeforeFlat;
            const f32_t bonusShare = bonusPart / totalBeforeFlat;
            basePart -= pen.flatReduction * baseShare;
            bonusPart -= pen.flatReduction * bonusShare;
        }
        else
        {
            basePart -= pen.flatReduction;
        }
    }

    if (basePart + bonusPart <= 0.f)
        return basePart + bonusPart;

    const f32_t reductionMul = 1.f - std::clamp(pen.percentReduction, 0.f, 1.f);
    basePart *= reductionMul;
    bonusPart *= reductionMul;

    if (basePart + bonusPart <= 0.f)
        return basePart + bonusPart;

    bonusPart *= 1.f - std::clamp(pen.percentBonusPen, 0.f, 1.f);

    f32_t total = basePart + bonusPart;
    if (total <= 0.f)
        return total;

    total *= 1.f - std::clamp(pen.percentPen, 0.f, 1.f);
    if (total <= 0.f)
        return total;

    total = std::max(0.f, total - pen.flatPen);
    return total;
}

f32_t CCombatFormula::ResistanceDamageMultiplier(f32_t effectiveResistance)
{
    if (effectiveResistance >= 0.f)
        return 100.f / (100.f + effectiveResistance);

    return 2.f - (100.f / (100.f - effectiveResistance));
}

f32_t CCombatFormula::ApplyResistance(f32_t amount, f32_t effectiveResistance)
{
    return amount * ResistanceDamageMultiplier(effectiveResistance);
}

f32_t CCombatFormula::ResolveAttackSpeed(
    f32_t baseAttackSpeed,
    f32_t attackSpeedRatio,
    f32_t attackSpeedGrowth,
    f32_t bonusAttackSpeed,
    u8_t level)
{
    const f32_t growthBonus = attackSpeedGrowth * GrowthMultiplier(level);
    const f32_t ratio = (attackSpeedRatio > 0.f) ? attackSpeedRatio : baseAttackSpeed;
    const f32_t result = baseAttackSpeed + ratio * (growthBonus + bonusAttackSpeed);
    return std::clamp(result, 0.2f, 3.003f);
}

f32_t CCombatFormula::ResolveAbilityCooldown(f32_t baseCooldown, f32_t abilityHaste)
{
    const f32_t haste = std::max(0.f, abilityHaste);
    return baseCooldown * (100.f / (100.f + haste));
}
