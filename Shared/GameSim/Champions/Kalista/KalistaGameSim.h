#pragma once

#include "WintersTypes.h"

class CWorld;
struct TickContext;

namespace KalistaGameSim
{
    void RegisterHooks();
    void Tick(CWorld& world, const TickContext& tc);
}
