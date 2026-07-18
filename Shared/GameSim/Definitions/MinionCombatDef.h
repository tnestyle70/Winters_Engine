#pragma once

#include "WintersTypes.h"

inline constexpr u8_t kGameSimMinionRoleMelee = 0;
inline constexpr u8_t kGameSimMinionRoleRanged = 1;
inline constexpr u8_t kGameSimMinionRoleSiege = 2;
inline constexpr u8_t kGameSimMinionRoleSuper = 3;
inline constexpr u8_t kGameSimMinionRoleTibbers = 4;

struct MinionCombatDef
{
    f32_t moveSpeed = 0.f;
    f32_t attackRange = 0.f;
    f32_t sightRange = 0.f;
    f32_t attackDamage = 0.f;
    f32_t attackCooldownMax = 0.f;
    f32_t maxHp = 0.f;
};
