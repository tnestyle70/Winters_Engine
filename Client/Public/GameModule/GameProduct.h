#pragma once
#include "Defines.h"

enum class eGameProduct : u32_t
{
	None = 0,
	LOL,
	EldenRing,
	ClassServant,
};

inline const char* GetGameProductName(eGameProduct product)
{
	switch (product)
	{
	case eGameProduct::LOL:
		return "Winters League";
	case eGameProduct::EldenRing:
		return "Winters Elden";
	case eGameProduct::ClassServant:
		return "Class & Servant";
	default:
		return "Unknown";
	}
}
