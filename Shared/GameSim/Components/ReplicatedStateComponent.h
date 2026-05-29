#pragma once

#include "WintersTypes.h"

#include <type_traits>

struct ReplicatedStateComponent
{
    u32_t stateFlags = 0;
    u64_t serverTick = 0;
};

static_assert(std::is_trivially_copyable_v<ReplicatedStateComponent>,
    "ReplicatedStateComponent must be trivially_copyable for sim determinism.");
