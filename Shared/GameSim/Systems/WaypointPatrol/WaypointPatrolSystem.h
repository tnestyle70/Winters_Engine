#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

class CWorld;

class CWaypointPatrolSystem final
{
public:
    static void Execute(CWorld& world, const TickContext& tc);
};
