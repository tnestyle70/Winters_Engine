#pragma once

#include "WintersTypes.h"

#include <type_traits>

struct ReplicatedStateComponent
{
    u32_t stateFlags = 0;
    u32_t gameplayStateFlags = 0;
    f32_t gameplayMoveSpeedMul = 1.f;
    u8_t forcedMotionKind = 0;
    f32_t fForcedMotionRemainingSec = 0.f;
    u64_t serverTick = 0;
};

static_assert(std::is_trivially_copyable_v<ReplicatedStateComponent>,
    "ReplicatedStateComponent must be trivially_copyable for sim determinism.");
