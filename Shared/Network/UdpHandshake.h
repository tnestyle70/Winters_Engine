#pragma once

#include "Shared/Network/UdpPacketCodec.h"

#include <array>
#include <cstring>
#include <span>
#include <vector>

constexpr size_t kUdpCookieBytes = 16u;
constexpr size_t kUdpMaxTicketBytes = 384u;

using UdpCookie = std::array<u8_t, kUdpCookieBytes>;

struct UdpClientHelloPayload
{
    u64_t clientNonce = 0;
    u16_t maxDatagramBytes = kUdpMaxDatagramBytes;
    u16_t reserved = 0;
    u32_t capabilities = 0;
};

struct UdpServerRetryPayload
{
    u64_t clientNonce = 0;
    u64_t cookieBucket = 0;
    UdpCookie cookie{};
};

struct UdpClientConnectPayload
{
    u64_t clientNonce = 0;
    u64_t cookieBucket = 0;
    UdpCookie cookie{};
    std::vector<u8_t> ticket;
};

struct UdpServerAcceptPayload
{
    u64_t connectionId = 0;
    u32_t generation = 0;
    u16_t maxDatagramBytes = kUdpMaxDatagramBytes;
    u16_t reserved = 0;
    u64_t clientNonce = 0;
};

inline std::vector<u8_t> EncodeUdpClientHello(
    const UdpClientHelloPayload& payload)
{
    std::vector<u8_t> bytes(16u);
    UdpWire::WriteU64(bytes.data() + 0u, payload.clientNonce);
    UdpWire::WriteU16(bytes.data() + 8u, payload.maxDatagramBytes);
    UdpWire::WriteU16(bytes.data() + 10u, payload.reserved);
    UdpWire::WriteU32(bytes.data() + 12u, payload.capabilities);
    return bytes;
}

inline bool DecodeUdpClientHello(
    std::span<const u8_t> bytes,
    UdpClientHelloPayload& outPayload)
{
    outPayload = {};
    if (bytes.size() != 16u)
        return false;
    outPayload.clientNonce = UdpWire::ReadU64(bytes.data() + 0u);
    outPayload.maxDatagramBytes = UdpWire::ReadU16(bytes.data() + 8u);
    outPayload.reserved = UdpWire::ReadU16(bytes.data() + 10u);
    outPayload.capabilities = UdpWire::ReadU32(bytes.data() + 12u);
    return outPayload.clientNonce != 0u &&
        outPayload.reserved == 0u &&
        outPayload.maxDatagramBytes >= kUdpPacketHeaderBytes &&
        outPayload.maxDatagramBytes <= kUdpMaxDatagramBytes;
}

inline std::vector<u8_t> EncodeUdpServerRetry(
    const UdpServerRetryPayload& payload)
{
    std::vector<u8_t> bytes(32u);
    UdpWire::WriteU64(bytes.data() + 0u, payload.clientNonce);
    UdpWire::WriteU64(bytes.data() + 8u, payload.cookieBucket);
    std::memcpy(bytes.data() + 16u, payload.cookie.data(), payload.cookie.size());
    return bytes;
}

inline bool DecodeUdpServerRetry(
    std::span<const u8_t> bytes,
    UdpServerRetryPayload& outPayload)
{
    outPayload = {};
    if (bytes.size() != 32u)
        return false;
    outPayload.clientNonce = UdpWire::ReadU64(bytes.data() + 0u);
    outPayload.cookieBucket = UdpWire::ReadU64(bytes.data() + 8u);
    std::memcpy(outPayload.cookie.data(), bytes.data() + 16u, outPayload.cookie.size());
    return outPayload.clientNonce != 0u;
}

inline std::vector<u8_t> EncodeUdpClientConnect(
    const UdpClientConnectPayload& payload)
{
    if (payload.ticket.size() > kUdpMaxTicketBytes)
        return {};

    std::vector<u8_t> bytes(34u + payload.ticket.size());
    UdpWire::WriteU64(bytes.data() + 0u, payload.clientNonce);
    UdpWire::WriteU64(bytes.data() + 8u, payload.cookieBucket);
    std::memcpy(bytes.data() + 16u, payload.cookie.data(), payload.cookie.size());
    UdpWire::WriteU16(bytes.data() + 32u, static_cast<u16_t>(payload.ticket.size()));
    if (!payload.ticket.empty())
        std::memcpy(bytes.data() + 34u, payload.ticket.data(), payload.ticket.size());
    return bytes;
}

inline bool DecodeUdpClientConnect(
    std::span<const u8_t> bytes,
    UdpClientConnectPayload& outPayload)
{
    outPayload = {};
    if (bytes.size() < 34u)
        return false;
    const u16_t ticketBytes = UdpWire::ReadU16(bytes.data() + 32u);
    if (ticketBytes > kUdpMaxTicketBytes || bytes.size() != 34u + ticketBytes)
        return false;

    outPayload.clientNonce = UdpWire::ReadU64(bytes.data() + 0u);
    outPayload.cookieBucket = UdpWire::ReadU64(bytes.data() + 8u);
    std::memcpy(outPayload.cookie.data(), bytes.data() + 16u, outPayload.cookie.size());
    outPayload.ticket.assign(bytes.begin() + 34u, bytes.end());
    return outPayload.clientNonce != 0u;
}

inline std::vector<u8_t> EncodeUdpServerAccept(
    const UdpServerAcceptPayload& payload)
{
    std::vector<u8_t> bytes(24u);
    UdpWire::WriteU64(bytes.data() + 0u, payload.connectionId);
    UdpWire::WriteU32(bytes.data() + 8u, payload.generation);
    UdpWire::WriteU16(bytes.data() + 12u, payload.maxDatagramBytes);
    UdpWire::WriteU16(bytes.data() + 14u, payload.reserved);
    UdpWire::WriteU64(bytes.data() + 16u, payload.clientNonce);
    return bytes;
}

inline bool DecodeUdpServerAccept(
    std::span<const u8_t> bytes,
    UdpServerAcceptPayload& outPayload)
{
    outPayload = {};
    if (bytes.size() != 24u)
        return false;
    outPayload.connectionId = UdpWire::ReadU64(bytes.data() + 0u);
    outPayload.generation = UdpWire::ReadU32(bytes.data() + 8u);
    outPayload.maxDatagramBytes = UdpWire::ReadU16(bytes.data() + 12u);
    outPayload.reserved = UdpWire::ReadU16(bytes.data() + 14u);
    outPayload.clientNonce = UdpWire::ReadU64(bytes.data() + 16u);
    return outPayload.connectionId != 0u &&
        outPayload.generation != 0u &&
        outPayload.clientNonce != 0u &&
        outPayload.reserved == 0u &&
        outPayload.maxDatagramBytes >= kUdpPacketHeaderBytes &&
        outPayload.maxDatagramBytes <= kUdpMaxDatagramBytes;
}
