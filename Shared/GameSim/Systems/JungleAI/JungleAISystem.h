#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <vector>

class CWorld;

class CJungleAISystem final
{
public:
    static void Execute(CWorld& world, const TickContext& tc,
        std::vector<GameCommand>& outCommands);

private:
    CJungleAISystem() = delete;
};
