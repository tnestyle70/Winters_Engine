#pragma once

#include "GameContext.h"
#include "WintersTypes.h"

struct FormOverrideComponent
{
	eChampion baseChampion = eChampion::END;
	eChampion visualChampion = eChampion::END;
	eChampion skillChampion = eChampion::END;
	u8_t skillSlotMask = 0x0Fu;
	f32_t fRemainingSec = 0.f;
	bool_t bActive = false;
};