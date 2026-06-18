#pragma once

#include "GameContext.h"
#include "WintersTypes.h"

struct SpellbookOverrideComponent
{
	eChampion sourceChampion = eChampion::END;
	u8_t sourceSlot = 4;
	u8_t localSlot = 4;
	u8_t sourceRank = 0;
	f32_t fRemainingSec = 0.f;
	bool_t bActive = false;
};