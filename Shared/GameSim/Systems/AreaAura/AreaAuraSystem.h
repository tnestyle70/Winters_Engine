#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

class CWorld;

class CAreaAuraSystem final
{
public:
	static void Execute(CWorld& world, const TickContext& tc);
};