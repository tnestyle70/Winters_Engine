#pragma once

#include "WintersTypes.h"

#include <array>
#include <cmath>

struct PracticeMinionAttackDamagePolicy
{
    static constexpr u8_t kRoleCount = 4u;
    static constexpr f32_t kMaximumValue = 1000000.f;

    std::array<f32_t, kRoleCount> values{};

    bool_t Apply(u8_t role, f32_t value)
    {
        if (role >= kRoleCount || !std::isfinite(value) || value < 0.f ||
            value > kMaximumValue)
        {
            return false;
        }

        values[role] = value;
        return true;
    }

    void Clear()
    {
        values.fill(0.f);
    }

    f32_t Resolve(
        u8_t role,
        f32_t packAttackDamage,
        f32_t timeGrowth,
        bool_t bPracticeEnabled) const
    {
        if (bPracticeEnabled && role < kRoleCount && values[role] > 0.f)
            return values[role];
        return packAttackDamage * timeGrowth;
    }
};
