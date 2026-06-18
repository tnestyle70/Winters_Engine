#pragma once

#include "WintersTypes.h"

class CWorld;
struct TickContext;

namespace RivenGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
