#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

#include <cstddef>

struct BuffInstance
{
    u32_t buffDefId = 0;
    EntityID source = NULL_ENTITY;
    f32_t fDurationRemaining = 0.f;
    u8_t stackCount = 0;
    u8_t reservedStackAlignment[3]{};
    u32_t paramHash = 0;

    f32_t flatAdPerStack = 0.f;
    f32_t flatApPerStack = 0.f;
    f32_t flatArmorPerStack = 0.f;
    f32_t flatMrPerStack = 0.f;
    f32_t bonusAttackSpeedPerStack = 0.f;
    f32_t moveSpeedMul = 1.f;
    u32_t reservedExpireTickAlignment = 0u;
    u64_t uExpireTick = 0u;
};

struct BuffComponent
{
    static constexpr u8_t kMaxBuffs = 16;

    BuffInstance buffs[kMaxBuffs] = {};
    u8_t count = 0;
    u8_t reservedTail[7]{};
};

static_assert(sizeof(BuffInstance) == 56u);
static_assert(offsetof(BuffInstance, paramHash) == 16u);
static_assert(offsetof(BuffInstance, uExpireTick) == 48u);
static_assert(sizeof(BuffComponent) == 904u);
static_assert(offsetof(BuffComponent, count) == 896u);
