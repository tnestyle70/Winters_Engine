#pragma once

#include "WintersTypes.h"

struct ResistBreakdown
{
    f32_t flatReduction = 0.f;
    f32_t percentReduction = 0.f;
    f32_t percentBonusPen = 0.f;
    f32_t percentPen = 0.f;
    f32_t flatPen = 0.f;
};

class CCombatFormula
{
public:
    static f32_t GrowthMultiplier(u8_t level);
    static f32_t ResolveStatAtLevel(f32_t baseValue, f32_t growthValue, u8_t level);

    static f32_t EffectiveResistance(
        f32_t baseResist,
        f32_t bonusResist,
        const ResistBreakdown& pen);

    static f32_t ResistanceDamageMultiplier(f32_t effectiveResistance);
    static f32_t ApplyResistance(f32_t amount, f32_t effectiveResistance);

    static f32_t ResolveAttackSpeed(
        f32_t baseAttackSpeed,
        f32_t attackSpeedRatio,
        f32_t attackSpeedGrowth,
        f32_t bonusAttackSpeed,
        u8_t level);

    static f32_t ResolveAbilityCooldown(f32_t baseCooldown, f32_t abilityHaste);
};
