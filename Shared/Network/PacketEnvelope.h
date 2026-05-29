#pragma once

#include "WintersTypes.h"

#include <cstdint>
#include <cstring>
#include <vector>

constexpr uint16_t kPacketMagic = 0x5742;       // 'WB' Winters Binary
constexpr uint16_t kPacketVersion = 1;
constexpr uint32_t kMaxPacketPayloadSize = 64u * 1024u;

enum class ePacketType : uint16_t
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
};

enum ePacketFlags : uint16_t
{
    PacketFlag_None = 0,
    PacketFlag_Compressed = 1u << 0,
    PacketFlag_Encrypted = 1u << 1,
};

#pragma pack(push, 1)
struct PacketHeader
{
    uint16_t magic = kPacketMagic;
    uint16_t version = kPacketVersion;
    uint16_t type = static_cast<uint16_t>(ePacketType::None);
    uint16_t flags = PacketFlag_None;
    uint32_t payloadSize = 0;
    uint32_t sequence = 0;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 16,
    "PacketHeader must be 16 bytes for wire stability.");

struct ParsedFrame
{
    ePacketType type = ePacketType::None;
    uint32_t sequence = 0;
    const uint8_t* payload = nullptr;
    uint32_t payloadSize = 0;
};

inline std::vector<uint8_t> WrapEnvelope(ePacketType type, uint32_t sequence,
    const uint8_t* payload, uint32_t payloadSize)
{
    std::vector<uint8_t> packet(sizeof(PacketHeader) + payloadSize);

    PacketHeader header{};
    header.type = static_cast<uint16_t>(type);
    header.payloadSize = payloadSize;
    header.sequence = sequence;

    std::memcpy(packet.data(), &header, sizeof(PacketHeader));
    if (payloadSize != 0 && payload != nullptr)
    {
        std::memcpy(packet.data() + sizeof(PacketHeader), payload, payloadSize);
    }

    return packet;
}

inline bool TryExtractFrame(const uint8_t* buffer, uint32_t available,
    ParsedFrame& outFrame, uint32_t& consumed)
{
    consumed = 0;
    outFrame = {};

    if (buffer == nullptr || available < sizeof(PacketHeader))
        return false;

    PacketHeader header{};
    std::memcpy(&header, buffer, sizeof(PacketHeader));

    if (header.magic != kPacketMagic ||
        header.version != kPacketVersion ||
        header.payloadSize > kMaxPacketPayloadSize)
    {
        consumed = available;
        return false;
    }

    const uint32_t frameSize = static_cast<uint32_t>(sizeof(PacketHeader)) + header.payloadSize;
    if (available < frameSize)
        return false;

    outFrame.type = static_cast<ePacketType>(header.type);
    outFrame.sequence = header.sequence;
    outFrame.payload = buffer + sizeof(PacketHeader);
    outFrame.payloadSize = header.payloadSize;
    consumed = frameSize;
    return true;
}
