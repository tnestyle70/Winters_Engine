#pragma once

#include "Shared/Network/PacketType.h"
#include "WintersTypes.h"

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

enum class eUdpSendResult : u8_t
{
    Queued = 0,
    NotRunning,
    NoAssociation,
    NotConfirmed,
    InvalidPacket,
    ReliableQueueFull,
    OutboundQueueFull,
    SocketError,
};

struct UdpServerInboundFrame
{
    u64_t connectionId = 0;
    u32_t generation = 0;
    ePacketType type = ePacketType::None;
    u32_t messageSequence = 0;
    std::vector<u8_t> payload;
};

struct UdpAuthenticatedIdentity
{
    std::string matchID;
    std::string userID;
    std::string gameSessionID;
    u64_t expiresAtUnix = 0;
    bool_t bAuthenticated = false;
};

struct UdpServerMetrics
{
    u64_t recvDatagrams = 0;
    u64_t recvBytes = 0;
    u64_t sendDatagrams = 0;
    u64_t sendBytes = 0;
    u64_t invalidDatagrams = 0;
    u64_t invalidCookies = 0;
    u64_t rejectedTickets = 0;
    u64_t unknownConnections = 0;
    u64_t duplicateDatagrams = 0;
    u64_t orderedBuffered = 0;
    u64_t orderedOverflow = 0;
    u64_t reassemblyComplete = 0;
    u64_t reassemblyOverflow = 0;
    u64_t reassemblyTimeout = 0;
    u64_t retransmits = 0;
    u64_t retryExhausted = 0;
    u64_t outboundQueueBytes = 0;
    u64_t outboundQueueDrops = 0;
    u64_t connectedPeers = 0;
    u64_t outstandingIo = 0;
};

class CUdpIocpCore final
{
public:
    using TicketValidator = std::function<bool(
        std::span<const u8_t>,
        UdpAuthenticatedIdentity&)>;
    using ConnectionCallback = std::function<void(
        u64_t,
        u32_t,
        bool,
        const UdpAuthenticatedIdentity&)>;
    using FrameCallback = std::function<void(UdpServerInboundFrame)>;

    CUdpIocpCore();
    ~CUdpIocpCore();

    CUdpIocpCore(const CUdpIocpCore&) = delete;
    CUdpIocpCore& operator=(const CUdpIocpCore&) = delete;

    void SetTicketValidator(TicketValidator validator);
    void SetConnectionCallback(ConnectionCallback callback);
    void SetFrameCallback(FrameCallback callback);

    bool Start(u16_t port, u32_t workerCount = 1u, u32_t receiveDepth = 8u);
    void Shutdown();

    eUdpSendResult SendToConnection(
        u64_t connectionId,
        ePacketType type,
        const u8_t* payload,
        u32_t payloadSize);

    bool DisconnectConnection(u64_t connectionId);
    bool IsRunning() const;
    u16_t GetBoundPort() const;
    UdpServerMetrics GetMetrics() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
