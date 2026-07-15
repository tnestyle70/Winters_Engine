#pragma once

#include "WintersTypes.h"

constexpr u16_t kUdpFragmentHeaderBytes = 16u;

// Logical host-order metadata. UdpFragmentCodec owns the wire byte order.
struct UdpFragmentHeader
{
	u32_t messageId = 0;
	u32_t messageBytes = 0;
	u16_t fragmentIndex = 0;
	u16_t fragmentCount = 0;
	u16_t fragmentPayloadSize = 0;
	u16_t reserved = 0;
};
