#pragma once

#include <cstdint>

enum class ePacketType : std::uint16_t
{
    None = 0,
    CommandBatch = 1,
    Snapshot = 2,
    Event = 3,
    Hello = 10,
    Heartbeat = 11,
    Disconnect = 12,
    LobbyCommand = 20,
    LobbyState = 21,
    GameStart = 22,

    // Transport association packets. These never enter GameRoom/GameSim.
    TransportClientHello = 100,
    TransportServerRetry = 101,
    TransportClientConnect = 102,
    TransportServerAccept = 103,
    TransportClientConfirm = 104,
};

constexpr bool IsTransportHandshakePacketType(ePacketType type)
{
    switch (type)
    {
    case ePacketType::TransportClientHello:
    case ePacketType::TransportServerRetry:
    case ePacketType::TransportClientConnect:
    case ePacketType::TransportServerAccept:
    case ePacketType::TransportClientConfirm:
        return true;
    default:
        return false;
    }
}

constexpr bool IsKnownPacketType(ePacketType type)
{
    switch (type)
    {
    case ePacketType::CommandBatch:
    case ePacketType::Snapshot:
    case ePacketType::Event:
    case ePacketType::Hello:
    case ePacketType::Heartbeat:
    case ePacketType::Disconnect:
    case ePacketType::LobbyCommand:
    case ePacketType::LobbyState:
    case ePacketType::GameStart:
    case ePacketType::TransportClientHello:
    case ePacketType::TransportServerRetry:
    case ePacketType::TransportClientConnect:
    case ePacketType::TransportServerAccept:
    case ePacketType::TransportClientConfirm:
        return true;
    case ePacketType::None:
    default:
        return false;
    }
}
