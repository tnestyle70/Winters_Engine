#pragma once

#include "ECS/Entity.h"
#include "WintersTypes.h"

#include <type_traits>

enum class eRuneId : u8_t
{
    None = 0,
    LethalTempo = 1,
    Electrocute = 2,
    AdaptiveForce = 3,
};

namespace RuneTuning
{
    constexpr u8_t kLethalTempoMaxStacks = 5u;
    constexpr f32_t kLethalTempoAttackSpeedPerStack = 0.10f;
}

struct RuneLoadoutComponent
{
    static constexpr u8_t kMaxRunes = 6u;

    eRuneId eRunes[kMaxRunes] = {};
    u8_t iCount = 0;
};

struct RuneRuntimeComponent
{
    u8_t iLethalTempoStacks = 0;
};

static_assert(std::is_trivially_copyable_v<RuneLoadoutComponent>);
static_assert(std::is_trivially_copyable_v<RuneRuntimeComponent>);
