#pragma once

#include "WintersTypes.h"

struct TeamScoreState
{
	u16_t iTotalKills = 0;
	u16_t iDestroyedTurrets = 0;
	u16_t iDragons = 0;
	u16_t iBarons = 0;
};

struct MatchScoreComponent
{
	TeamScoreState Blue{};
	TeamScoreState Red{};
};