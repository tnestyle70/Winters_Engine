#pragma once

#include "WintersTypes.h"

#include <type_traits>

enum class ePoseStateId : u16_t
{
    None = 0,
    Idle = 1,
    Run = 2,
    Dead = 3,
};

struct PoseStateComponent
{
    u16_t poseId = static_cast<u16_t>(ePoseStateId::Idle);
    u64_t startTick = 0;
};

static_assert(std::is_trivially_copyable_v<PoseStateComponent>,
    "PoseStateComponent must be trivially_copyable for sim determinism.");
