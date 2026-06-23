#pragma once

#include "WintersTypes.h"

inline constexpr u8_t kGameSimMinionRoleMelee = 0;
inline constexpr u8_t kGameSimMinionRoleRanged = 1;
inline constexpr u8_t kGameSimMinionRoleSiege = 2;
inline constexpr u8_t kGameSimMinionRoleSuper = 3;
inline constexpr u8_t kGameSimMinionRoleTibbers = 4;

struct MinionCombatDef
{
    f32_t moveSpeed = 4.f;
    f32_t attackRange = 1.5f;
    f32_t sightRange = 12.f;
    f32_t attackDamage = 40.f;
    f32_t attackCooldownMax = 1.f;
    f32_t maxHp = 450.f;
};

inline MinionCombatDef ResolveMinionCombatDef(u8_t roleType)
{
    switch (roleType)
    {
    case kGameSimMinionRoleRanged:
        return { 4.f, 8.f, 14.f, 60.f, 1.2f, 450.f };
    case kGameSimMinionRoleSiege:
        return { 3.5f, 10.f, 16.f, 40.f, 1.f, 450.f };
    case kGameSimMinionRoleSuper:
        return { 5.f, 2.f, 14.f, 100.f, 1.f, 1000.f };
    case kGameSimMinionRoleTibbers:
        return { 5.2f, 2.2f, 14.f, 80.f, 1.f, 1500.f };
    case kGameSimMinionRoleMelee:
    default:
        return { 4.f, 1.5f, 12.f, 40.f, 1.f, 450.f };
    }
}
