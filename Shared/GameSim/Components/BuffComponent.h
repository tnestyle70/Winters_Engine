#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

struct BuffInstance
{
    u32_t buffDefId = 0;
    EntityID source = NULL_ENTITY;
    f32_t fDurationRemaining = 0.f;
    u8_t stackCount = 0;
    u32_t paramHash = 0;

    f32_t flatAdPerStack = 0.f;
    f32_t flatApPerStack = 0.f;
    f32_t flatArmorPerStack = 0.f;
    f32_t flatMrPerStack = 0.f;
    f32_t moveSpeedMul = 1.f;
};

struct BuffComponent
{
    static constexpr u8_t kMaxBuffs = 16;

    BuffInstance buffs[kMaxBuffs] = {};
    u8_t count = 0;
};
