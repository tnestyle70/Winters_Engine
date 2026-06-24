#pragma once

#include "ECS/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

enum class eCombatActionKind : u8_t
{
    None = 0,
    BasicAttack = 1,
    Skill = 2,
};

enum class eCombatActionMovePolicy : u8_t
{
    None = 0,
    CancelBeforeImpactMoveAfterImpact = 1,
    QueueMoveUntilImpact = 2,
    LockUntilEnd = 3,
};

namespace CombatActionFlags
{
    constexpr u16_t JaxEmpower = 0x0001u;
    constexpr u16_t SylasPassive = 0x0002u;
}

struct CombatActionComponent
{
    eCombatActionKind eKind = eCombatActionKind::None;
    eCombatActionMovePolicy eMovePolicy = eCombatActionMovePolicy::None;
    u8_t uSlot = 0;
    u8_t uStage = 1;
    u16_t uFlags = 0;
    EntityID entityTarget = NULL_ENTITY;
    u32_t uSequenceNum = 0;
    u64_t uStartTick = 0;
    u64_t uImpactTick = 0;
    u64_t uEndTick = 0;
    bool_t bImpactIssued = false;
    bool_t bQueuedMove = false;
    Vec3 vQueuedMoveTarget{};
    Vec3 vQueuedMoveDirection{};
};

static_assert(std::is_trivially_copyable_v<CombatActionComponent>,
    "CombatActionComponent must be trivially_copyable for sim determinism.");
