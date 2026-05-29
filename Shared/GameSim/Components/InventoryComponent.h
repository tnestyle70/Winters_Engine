#pragma once

#include "WintersTypes.h"

struct InventoryComponent
{
	static constexpr u8_t kMaxSlots = 6;
	u16_t itemIds[kMaxSlots] = {};
	u8_t count = 0;
};
