#pragma once

#include "WintersTypes.h"

struct JungleCampGameDef
{
    f32_t maxHp = 0.f;
    f32_t radius = 0.f;
    f32_t attackRange = 0.f;
    f32_t attackDamage = 0.f;
    f32_t attackCooldown = 0.f;
    f32_t moveSpeed = 0.f;
    f32_t baseArmor = 0.f;
    f32_t baseMr = 0.f;
    f32_t aggroRange = 0.f;
    f32_t leashRange = 0.f;
    f32_t respawnDelaySec = 30.f;
};
