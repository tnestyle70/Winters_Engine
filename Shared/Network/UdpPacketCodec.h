#pragma once

#include "Shared/Network/UdpPacketHeader.h"

#include <cstddef>
#include <cstring>
#include <span>
#include <vector>

namespace UdpWire
{
    inline void WriteU16(u8_t* destination, u16_t value)
    {
        destination[0] = static_cast<u8_t>(value >> 8u);
        destination[1] = static_cast<u8_t>(value);
    }

    inline void WriteU32(u8_t* destination, u32_t value)
    {
        destination[0] = static_cast<u8_t>(value >> 24u);
        destination[1] = static_cast<u8_t>(value >> 16u);
        destination[2] = static_cast<u8_t>(value >> 8u);
        destination[3] = static_cast<u8_t>(value);
    }

    inline void WriteU64(u8_t* destination, u64_t value)
    {
        WriteU32(destination, static_cast<u32_t>(value >> 32u));
        WriteU32(destination + 4u, static_cast<u32_t>(value));
    }

    inline u16_t ReadU16(const u8_t* source)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(source[0]) << 8u) |
            static_cast<u16_t>(source[1]));
    }

    inline u32_t ReadU32(const u8_t* source)
    {
        return
            (static_cast<u32_t>(source[0]) << 24u) |
            (static_cast<u32_t>(source[1]) << 16u) |
            (static_cast<u32_t>(source[2]) << 8u) |
            static_cast<u32_t>(source[3]);
    }

    inline u64_t ReadU64(const u8_t* source)
    {
        return
            (static_cast<u64_t>(ReadU32(source)) << 32u) |
            static_cast<u64_t>(ReadU32(source + 4u));
    }
}

enum class eUdpDecodeError : u8_t
{
    None = 0,
    NullBuffer,
    DatagramTooSmall,
    DatagramTooLarge,
    InvalidMagic,
    UnsupportedVersion,
    InvalidHeaderSize,
    InvalidFlags,
    InvalidReserved,
    InvalidLength,
    InvalidType,
    InvalidLane,
    InvalidSequence,
    InvalidAssociation,
};

struct UdpPacketView
{
    UdpPacketHeader header{};
    const u8_t* payload = nullptr;
    u16_t payloadSize = 0;
};

inline bool ValidateUdpPacketHeader(
    const UdpPacketHeader& header,
    eUdpDecodeError& outError)
{
    if ((header.flags & ~kUdpKnownPacketFlags) != 0u)
    {
        outError = eUdpDecodeError::InvalidFlags;
        return false;
    }
    if (header.reserved != 0u)
    {
        outError = eUdpDecodeError::InvalidReserved;
        return false;
    }
    if (!IsValidPacketLane(header.lane))
    {
        outError = eUdpDecodeError::InvalidLane;
        return false;
    }
    if (header.packetSeq == 0u)
    {
        outError = eUdpDecodeError::InvalidSequence;
        return false;
    }

    const bool bAckOnly = (header.flags & UdpPacketFlag_AckOnly) != 0u;
    if (bAckOnly)
    {
        if (header.type != ePacketType::None ||
            header.messageSeq != 0u ||
            header.payloadSize != 0u ||
            (header.flags & (UdpPacketFlag_Fragment | UdpPacketFlag_Handshake)) != 0u)
        {
            outError = eUdpDecodeError::InvalidFlags;
            return false;
        }
    }
    else
    {
        if (!IsKnownPacketType(header.type) || header.messageSeq == 0u)
        {
            outError = eUdpDecodeError::InvalidType;
            return false;
        }

        const PacketSemantics semantics = ResolvePacketSemantics(header.type);
        if (semantics.lane != header.lane)
        {
            outError = eUdpDecodeError::InvalidLane;
            return false;
        }

        const bool bHandshake =
            (header.flags & UdpPacketFlag_Handshake) != 0u;
        if (bHandshake != IsTransportHandshakePacketType(header.type) ||
            (bHandshake &&
                (header.flags & UdpPacketFlag_Fragment) != 0u))
        {
            outError = eUdpDecodeError::InvalidFlags;
            return false;
        }
    }

    switch (header.type)
    {
    case ePacketType::TransportClientHello:
    case ePacketType::TransportServerRetry:
    case ePacketType::TransportClientConnect:
        if (header.connectionId != 0u || header.generation != 0u)
        {
            outError = eUdpDecodeError::InvalidAssociation;
            return false;
        }
        break;
    case ePacketType::TransportServerAccept:
    case ePacketType::TransportClientConfirm:
        if (header.connectionId == 0u || header.generation == 0u)
        {
            outError = eUdpDecodeError::InvalidAssociation;
            return false;
        }
        break;
    case ePacketType::None:
        if (!bAckOnly || header.connectionId == 0u || header.generation == 0u)
        {
            outError = eUdpDecodeError::InvalidAssociation;
            return false;
        }
        break;
    default:
        if (header.connectionId == 0u || header.generation == 0u)
        {
            outError = eUdpDecodeError::InvalidAssociation;
            return false;
        }
        break;
    }

    outError = eUdpDecodeError::None;
    return true;
}

inline bool EncodeUdpPacket(
    UdpPacketHeader header,
    std::span<const u8_t> payload,
    std::vector<u8_t>& outDatagram,
    eUdpDecodeError* outError = nullptr)
{
    eUdpDecodeError error = eUdpDecodeError::None;
    if (payload.size() > kUdpMaxPacketPayloadBytes)
    {
        error = eUdpDecodeError::DatagramTooLarge;
    }
    else
    {
        header.magic = kUdpPacketMagic;
        header.version = kUdpPacketVersion;
        header.headerBytes = kUdpPacketHeaderBytes;
        header.payloadSize = static_cast<u16_t>(payload.size());
        if (!ValidateUdpPacketHeader(header, error))
        {
            // error populated by ValidateUdpPacketHeader.
        }
    }

    if (error != eUdpDecodeError::None)
    {
        outDatagram.clear();
        if (outError)
            *outError = error;
        return false;
    }

    outDatagram.resize(kUdpPacketHeaderBytes + payload.size());
    u8_t* bytes = outDatagram.data();
    UdpWire::WriteU16(bytes + 0u, header.magic);
    bytes[2] = header.version;
    bytes[3] = header.headerBytes;
    UdpWire::WriteU64(bytes + 4u, header.connectionId);
    UdpWire::WriteU32(bytes + 12u, header.generation);
    UdpWire::WriteU16(bytes + 16u, static_cast<u16_t>(header.type));
    bytes[18] = static_cast<u8_t>(header.lane);
    bytes[19] = header.flags;
    UdpWire::WriteU32(bytes + 20u, header.packetSeq);
    UdpWire::WriteU32(bytes + 24u, header.messageSeq);
    UdpWire::WriteU32(bytes + 28u, header.ackSeq);
    UdpWire::WriteU32(bytes + 32u, header.ackBitfield);
    UdpWire::WriteU16(bytes + 36u, header.payloadSize);
    UdpWire::WriteU16(bytes + 38u, header.reserved);
    if (!payload.empty())
    {
        std::memcpy(
            bytes + kUdpPacketHeaderBytes,
            payload.data(),
            payload.size());
    }
    if (outError)
        *outError = eUdpDecodeError::None;
    return true;
}

inline bool DecodeUdpPacket(
    const u8_t* datagram,
    size_t datagramSize,
    UdpPacketView& outPacket,
    eUdpDecodeError* outError = nullptr)
{
    outPacket = {};
    eUdpDecodeError error = eUdpDecodeError::None;
    if (!datagram)
        error = eUdpDecodeError::NullBuffer;
    else if (datagramSize < kUdpPacketHeaderBytes)
        error = eUdpDecodeError::DatagramTooSmall;
    else if (datagramSize > kUdpMaxDatagramBytes)
        error = eUdpDecodeError::DatagramTooLarge;

    if (error != eUdpDecodeError::None)
    {
        if (outError)
            *outError = error;
        return false;
    }

    UdpPacketHeader header{};
    header.magic = UdpWire::ReadU16(datagram + 0u);
    header.version = datagram[2];
    header.headerBytes = datagram[3];
    header.connectionId = UdpWire::ReadU64(datagram + 4u);
    header.generation = UdpWire::ReadU32(datagram + 12u);
    header.type = static_cast<ePacketType>(UdpWire::ReadU16(datagram + 16u));
    header.lane = static_cast<PacketLane>(datagram[18]);
    header.flags = datagram[19];
    header.packetSeq = UdpWire::ReadU32(datagram + 20u);
    header.messageSeq = UdpWire::ReadU32(datagram + 24u);
    header.ackSeq = UdpWire::ReadU32(datagram + 28u);
    header.ackBitfield = UdpWire::ReadU32(datagram + 32u);
    header.payloadSize = UdpWire::ReadU16(datagram + 36u);
    header.reserved = UdpWire::ReadU16(datagram + 38u);

    if (header.magic != kUdpPacketMagic)
        error = eUdpDecodeError::InvalidMagic;
    else if (header.version != kUdpPacketVersion)
        error = eUdpDecodeError::UnsupportedVersion;
    else if (header.headerBytes != kUdpPacketHeaderBytes)
        error = eUdpDecodeError::InvalidHeaderSize;
    else if (datagramSize !=
        static_cast<size_t>(header.headerBytes) + header.payloadSize)
        error = eUdpDecodeError::InvalidLength;
    else
        ValidateUdpPacketHeader(header, error);

    if (error != eUdpDecodeError::None)
    {
        if (outError)
            *outError = error;
        return false;
    }

    outPacket.header = header;
    outPacket.payload = datagram + header.headerBytes;
    outPacket.payloadSize = header.payloadSize;
    if (outError)
        *outError = eUdpDecodeError::None;
    return true;
}
