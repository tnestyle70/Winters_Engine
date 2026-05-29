#pragma once

#include "Defines.h"

#include <string>

struct GameModeDef
{
	std::string strModeID{};
	std::string strDisplayName{};
	std::string strMapID{};
	std::string strRulesetID{};
	std::string strQueueName{};
	u32_t uTeamSize = 1;
	bool_t bAvailable = false;
	bool_t bMatchmakingEnabled = false;
	bool_t bPracticeMode = false;
};
