#pragma once

#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

class CWorld;

class CBuffSystem
{
public:
    static bool AddOrRefresh(BuffComponent& component, const BuffInstance& instance);
    static void Execute(CWorld& world, const TickContext& tc);
};
