#pragma once

#include "Shared/Network/PacketType.h"

#include <cstdint>

enum class PacketDelivery : std::uint8_t
{
    ReliableOrdered = 0,
    ReliableUnordered,
    UnreliableSequenced,
};

enum class PacketLane : std::uint8_t
{
    Invalid = 0,
    Control = 1,
    Heartbeat = 2,
    Lobby = 3,
    Command = 4,
    Event = 5,
    Snapshot = 6,
    Telemetry = 7,
};

constexpr std::uint8_t kPacketLaneCount = 8;

struct PacketSemantics
{
    PacketDelivery delivery = PacketDelivery::ReliableOrdered;
    PacketLane lane = PacketLane::Invalid;
};

constexpr PacketSemantics ResolvePacketSemantics(ePacketType type)
{
    switch (type)
    {
    case ePacketType::CommandBatch:
        return { PacketDelivery::ReliableOrdered, PacketLane::Command };
    case ePacketType::Event:
        return { PacketDelivery::ReliableOrdered, PacketLane::Event };
    case ePacketType::Snapshot:
        return { PacketDelivery::UnreliableSequenced, PacketLane::Snapshot };
    case ePacketType::Heartbeat:
        return { PacketDelivery::UnreliableSequenced, PacketLane::Heartbeat };
    case ePacketType::LobbyCommand:
    case ePacketType::LobbyState:
        return { PacketDelivery::ReliableOrdered, PacketLane::Lobby };
    case ePacketType::Hello:
    case ePacketType::Disconnect:
    case ePacketType::GameStart:
    case ePacketType::TransportClientHello:
    case ePacketType::TransportServerRetry:
    case ePacketType::TransportClientConnect:
    case ePacketType::TransportServerAccept:
    case ePacketType::TransportClientConfirm:
        return { PacketDelivery::ReliableOrdered, PacketLane::Control };
    case ePacketType::None:
    default:
        return {};
    }
}

constexpr bool IsReliableDelivery(PacketDelivery delivery)
{
    return delivery != PacketDelivery::UnreliableSequenced;
}

constexpr bool IsValidPacketLane(PacketLane lane)
{
    const auto value = static_cast<std::uint8_t>(lane);
    return value > static_cast<std::uint8_t>(PacketLane::Invalid) &&
        value < kPacketLaneCount;
}
