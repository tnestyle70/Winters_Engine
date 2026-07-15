#pragma once

#include "WintersTypes.h"

#include <cstddef>
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
    u16_t reserved0 = 0u;
    u32_t reserved1 = 0u;
    u64_t startTick = 0;
};

static_assert(sizeof(PoseStateComponent) == 16u,
    "PoseStateComponent replay/checkpoint ABI changed.");
static_assert(offsetof(PoseStateComponent, startTick) == 8u,
    "PoseStateComponent startTick ABI offset changed.");
static_assert(std::is_trivially_copyable_v<PoseStateComponent>,
    "PoseStateComponent must be trivially_copyable for sim determinism.");
