#pragma once

#include "WintersTypes.h"

#pragma pack(push, 1)
struct UdpFragmentHeader
{
	u32_t messageId = 0;
	u16_t fragmentIndex = 0;
	u16_t fragmentCount = 0;
	u16_t fragmentPayloadSize = 0;
};

#pragma pack(pop)

static_assert(sizeof(UdpFragmentHeader) == 10);