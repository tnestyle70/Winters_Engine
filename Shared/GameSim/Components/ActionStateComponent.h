#pragma once

#include "WintersTypes.h"

#include <type_traits>

enum class eActionStateId : u16_t
{
    None = 0,
    BasicAttack = 10,
    SkillQ = 20,
    SkillW = 21,
    SkillE = 22,
    SkillR = 23,
    Recall = 30,
    DeathStart = 50,
    ViegoConsumeSoul = 60,
};

struct ActionStateComponent
{
    u16_t actionId = static_cast<u16_t>(eActionStateId::None);
    u64_t startTick = 0;
    u32_t sequence = 0;
    u8_t stage = 1;
};

static_assert(std::is_trivially_copyable_v<ActionStateComponent>,
    "ActionStateComponent must be trivially_copyable for sim determinism.");
