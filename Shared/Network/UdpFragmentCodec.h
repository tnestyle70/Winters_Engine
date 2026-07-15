#pragma once

#include "Shared/Network/UdpFragmentHeader.h"
#include "Shared/Network/UdpPacketCodec.h"

#include <span>

constexpr u16_t kUdpMaxFragmentDataBytes =
    kUdpMaxPacketPayloadBytes - kUdpFragmentHeaderBytes;
constexpr u16_t kUdpMaxFragmentsPerMessage = 64u;

inline void EncodeUdpFragmentHeader(
    const UdpFragmentHeader& header,
    u8_t* destination)
{
    UdpWire::WriteU32(destination + 0u, header.messageId);
    UdpWire::WriteU32(destination + 4u, header.messageBytes);
    UdpWire::WriteU16(destination + 8u, header.fragmentIndex);
    UdpWire::WriteU16(destination + 10u, header.fragmentCount);
    UdpWire::WriteU16(destination + 12u, header.fragmentPayloadSize);
    UdpWire::WriteU16(destination + 14u, header.reserved);
}

inline bool DecodeUdpFragmentHeader(
    std::span<const u8_t> bytes,
    UdpFragmentHeader& outHeader)
{
    outHeader = {};
    if (bytes.size() < kUdpFragmentHeaderBytes)
        return false;
    outHeader.messageId = UdpWire::ReadU32(bytes.data() + 0u);
    outHeader.messageBytes = UdpWire::ReadU32(bytes.data() + 4u);
    outHeader.fragmentIndex = UdpWire::ReadU16(bytes.data() + 8u);
    outHeader.fragmentCount = UdpWire::ReadU16(bytes.data() + 10u);
    outHeader.fragmentPayloadSize = UdpWire::ReadU16(bytes.data() + 12u);
    outHeader.reserved = UdpWire::ReadU16(bytes.data() + 14u);
    return true;
}
