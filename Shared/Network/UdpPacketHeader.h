#pragma once

#include "WintersTypes.h"

constexpr u16_t kUdpPacketMagic = 0x5742;
constexpr u8_t kUdpPacketVersion = 2;
constexpr u16_t kUdpMtuPayloadBytes = 1200;

enum class eUdpChannel : u8_t
{
	ReliableOrdered = 0,
	ReliableUnordered = 1,
	UnreliableSequenced = 2,
};

enum eUdpPacketFlags : u16_t
{
	UdpPacketFlag_None = 0,
	UdpPacketFlag_Fragment = 1u << 0,
	UdpPacketFlag_AckOnly = 1u << 1,
};

#pragma pack(push, 1)
struct UdpPacketHeader
{
	u16_t magic = kUdpPacketMagic;
	u8_t version = kUdpPacketVersion;
	u8_t channel = static_cast<u8_t>(eUdpChannel::UnreliableSequenced);
	u16_t type = 0;
	u16_t flags = UdpPacketFlag_None;
	u32_t channelSeq = 0;
	u32_t ackSeq = 0;
	u32_t ackBitfield = 0;
	u16_t payloadSize = 0;
	u16_t reserved = 0;
};

#pragma pack(pop)

static_assert(sizeof(UdpPacketHeader) == 24);