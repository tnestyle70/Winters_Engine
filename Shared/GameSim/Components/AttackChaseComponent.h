#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

struct AttackChaseComponent
{
    EntityID target = NULL_ENTITY;
    u32_t sequenceNum = 0;
    u8_t commandKind = 3;
    u8_t slot = 0;
    u16_t itemId = 0;
    f32_t effectiveRange = 0.f;
    Vec3 groundPos{};
    Vec3 direction{};
    f32_t repathTimer = 0.f;
    bool_t bActive = false;
};

static_assert(std::is_trivially_copyable_v<AttackChaseComponent>,
    "AttackChaseComponent must be trivially_copyable for sim determinism.");
