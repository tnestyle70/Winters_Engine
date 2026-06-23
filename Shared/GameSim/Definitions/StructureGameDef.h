#pragma once

#include "WintersTypes.h"

enum class eStructureKind : u8_t
{
    Turret = 0,
    Inhibitor = 1,
    Nexus = 2,
};

struct TurretAIGameDef
{
    f32_t attackRange = 0.f;
    f32_t attackCooldownMax = 0.f;
    f32_t attackDamage = 0.f;
    f32_t nexusAttackDamage = 0.f;
    f32_t projectileSpeed = 0.f;
    f32_t turretSightRange = 0.f;
    f32_t structureSightRange = 0.f;
    f32_t bodyHeight = 0.f;
    f32_t bodyOffsetY = 0.f;
};

struct StructureGameDef
{
    f32_t turretMaxHp = 0.f;
    f32_t inhibitorMaxHp = 0.f;
    f32_t nexusMaxHp = 0.f;
    TurretAIGameDef turretAI{};
};
