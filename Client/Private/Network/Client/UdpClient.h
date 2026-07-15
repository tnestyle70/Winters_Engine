#pragma once

#include "Shared/Network/PacketType.h"
#include "WintersTypes.h"

#include <functional>
#include <memory>
#include <span>

struct UdpClientMetrics
{
    u64_t recvDatagrams = 0;
    u64_t recvBytes = 0;
    u64_t sendDatagrams = 0;
    u64_t sendBytes = 0;
    u64_t invalidDatagrams = 0;
    u64_t duplicateDatagrams = 0;
    u64_t retransmits = 0;
    u64_t retryExhausted = 0;
    u64_t reassemblyComplete = 0;
    u64_t reassemblyOverflow = 0;
    u64_t reassemblyTimeout = 0;
    u64_t outboundQueueBytes = 0;
    u64_t outboundQueueDrops = 0;
};

class CUdpClient final
{
public:
    using FrameCallback =
        std::function<void(ePacketType, u32_t, const u8_t*, u32_t)>;

    static std::unique_ptr<CUdpClient> Create();
    ~CUdpClient();

    CUdpClient(const CUdpClient&) = delete;
    CUdpClient& operator=(const CUdpClient&) = delete;

    // Lifecycle calls are internally serialized. Connect is synchronous; a
    // concurrent Disconnect waits until the attempt completes. Reconnecting
    // after a worker-side failure joins and clears the previous worker first.
    bool Connect(
        const char* host,
        u16_t port,
        std::span<const u8_t> ticket,
        u32_t timeoutMs = 3'000u);
    bool Connect(const char* host, u16_t port);
    void Disconnect();

    bool Send(
        ePacketType type,
        const u8_t* payload,
        u32_t payloadSize);
    void SetFrameCallback(FrameCallback callback);
    void PumpReceivedFrames();

    bool IsConnected() const;
    u64_t GetConnectionId() const;
    u32_t GetGeneration() const;
    UdpClientMetrics GetMetrics() const;

private:
    CUdpClient();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
