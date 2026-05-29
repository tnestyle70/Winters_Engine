#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

class CWorld;

class CCombatActionSystem final
{
public:
    static void Execute(CWorld& world, const TickContext& tc);

private:
    CCombatActionSystem() = delete;
};
