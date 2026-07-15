#pragma once

#include "Shared/Network/PacketSemantics.h"
#include "WintersTypes.h"

constexpr u16_t kUdpPacketMagic = 0x5742;
constexpr u8_t kUdpPacketVersion = 3;
constexpr u8_t kUdpPacketHeaderBytes = 40;
constexpr u16_t kUdpMaxDatagramBytes = 1200;
constexpr u16_t kUdpMaxPacketPayloadBytes =
    kUdpMaxDatagramBytes - kUdpPacketHeaderBytes;
constexpr u32_t kUdpMaxMessageBytes = 64u * 1024u;

enum eUdpPacketFlags : u8_t
{
	UdpPacketFlag_None = 0u,
	UdpPacketFlag_Fragment = 1u << 0u,
	UdpPacketFlag_AckOnly = 1u << 1u,
	UdpPacketFlag_Handshake = 1u << 2u,
};

constexpr u8_t kUdpKnownPacketFlags =
    UdpPacketFlag_Fragment |
    UdpPacketFlag_AckOnly |
    UdpPacketFlag_Handshake;

// Logical host-order header. UdpPacketCodec serializes every field explicitly;
// sizeof(UdpPacketHeader) is deliberately not a wire contract.
struct UdpPacketHeader
{
	u16_t magic = kUdpPacketMagic;
	u8_t version = kUdpPacketVersion;
	u8_t headerBytes = kUdpPacketHeaderBytes;
	u64_t connectionId = 0;
	u32_t generation = 0;
	ePacketType type = ePacketType::None;
	PacketLane lane = PacketLane::Invalid;
	u8_t flags = UdpPacketFlag_None;
	u32_t packetSeq = 0;
	u32_t messageSeq = 0;
	u32_t ackSeq = 0;
	u32_t ackBitfield = 0;
	u16_t payloadSize = 0;
	u16_t reserved = 0;
};
