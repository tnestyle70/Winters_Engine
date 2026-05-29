#pragma once

#include "WintersAPI.h"
#include "ECS/World.h"

namespace SharedSim
{
    // Temporary adapter boundary for Phase 7F.
    // GameSim code includes this file instead of reaching into Engine ECS
    // directly; replacing the backing world becomes a one-file change later.
    using World = ::CWorld;
}
