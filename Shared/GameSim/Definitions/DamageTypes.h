#pragma once

#include "WintersTypes.h"

#include <cstdint>

enum class eDamageType : uint8_t
{
    Physical = 0,
    Magic = 1,
    True = 2,
};

enum eDamageFlags : uint32_t
{
    DamageFlag_None = 0,
    DamageFlag_CanCrit = 1u << 0,
    DamageFlag_CanLifesteal = 1u << 1,
    DamageFlag_OnHit = 1u << 2,
};

enum class eDamageSourceKind : uint8_t
{
    Unknown = 0,
    BasicAttack = 1,
    Skill = 2,
    Item = 3,
    Rune = 4,
};

struct DamageFormulaDef
{
    static constexpr u8_t kMaxRank = 5u;

    bool_t bValid = false;
    u8_t rankCount = 1u;
    eDamageType type = eDamageType::Physical;
    u32_t flags = DamageFlag_None;
    f32_t flatByRank[kMaxRank]{};
    f32_t totalAdRatioByRank[kMaxRank]{};
    f32_t bonusAdRatioByRank[kMaxRank]{};
    f32_t apRatioByRank[kMaxRank]{};
    f32_t targetMaxHpRatioByRank[kMaxRank]{};
    f32_t targetMissingHpRatioByRank[kMaxRank]{};
};

inline u8_t ClampDamageFormulaRank(const DamageFormulaDef& formula, u8_t rank)
{
    const u8_t count = formula.rankCount > 0u
        ? (formula.rankCount < DamageFormulaDef::kMaxRank
            ? formula.rankCount
            : DamageFormulaDef::kMaxRank)
        : 1u;
    const u8_t resolved = rank > 0u ? rank : 1u;
    return resolved < count ? resolved : count;
}

inline f32_t ResolveDamageFormulaRankedValue(
    const DamageFormulaDef& formula,
    const f32_t values[DamageFormulaDef::kMaxRank],
    u8_t rank)
{
    return values[ClampDamageFormulaRank(formula, rank) - 1u];
}
