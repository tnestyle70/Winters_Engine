#include "UdpClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <bcrypt.h>
#include <MSWSock.h>
#include <mstcpip.h>

#include "Shared/Network/PacketSemantics.h"
#include "Shared/Network/UdpFragmentCodec.h"
#include "Shared/Network/UdpHandshake.h"
#include "Shared/Network/UdpOrderedReceiveQueue.h"
#include "Shared/Network/UdpPacketCodec.h"
#include "Shared/Network/UdpReassemblyBuffer.h"
#include "Shared/Network/UdpReliabilityChannel.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
    constexpr size_t kMaxOutboundDatagrams = 4096u;
    constexpr size_t kMaxOutboundBytes = 4u * 1024u * 1024u;
    constexpr size_t kMaxPendingFrames = 256u;
    constexpr size_t kMaxPendingFrameBytes = 2u * 1024u * 1024u;
    constexpr u64_t kHandshakeRetryMs = 120u;
    constexpr u32_t kMaxHandshakeAttempts = 16u;
    constexpr u64_t kHeartbeatIntervalMs = 1'000u;
    constexpr u64_t kServerIdleTimeoutMs = 15'000u;

    std::atomic<int> g_udpWsaRefCount{ 0 };
    std::mutex g_udpWsaMutex;

    u64_t NowMs()
    {
        return static_cast<u64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    bool EnsureWsa()
    {
        std::lock_guard wsaLock(g_udpWsaMutex);
        const int referenceCount =
            g_udpWsaRefCount.load(std::memory_order_relaxed);
        if (referenceCount != 0)
        {
            g_udpWsaRefCount.store(
                referenceCount + 1,
                std::memory_order_relaxed);
            return true;
        }
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) == 0)
        {
            g_udpWsaRefCount.store(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    void ReleaseWsa()
    {
        std::lock_guard wsaLock(g_udpWsaMutex);
        const int referenceCount =
            g_udpWsaRefCount.load(std::memory_order_relaxed);
        if (referenceCount <= 0)
            return;
        if (referenceCount == 1)
        {
            g_udpWsaRefCount.store(0, std::memory_order_relaxed);
            WSACleanup();
            return;
        }
        g_udpWsaRefCount.store(
            referenceCount - 1,
            std::memory_order_relaxed);
    }

    bool FillRandom(void* destination, size_t size)
    {
        return destination && size != 0u && size <= ULONG_MAX &&
            BCryptGenRandom(
                nullptr,
                static_cast<PUCHAR>(destination),
                static_cast<ULONG>(size),
                BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
    }

    bool EndpointEquals(
        const sockaddr_storage& lhs,
        int lhsLength,
        const sockaddr_storage& rhs,
        int rhsLength)
    {
        if (lhsLength != rhsLength || lhs.ss_family != rhs.ss_family)
            return false;
        if (lhs.ss_family == AF_INET)
        {
            const auto& left = reinterpret_cast<const sockaddr_in&>(lhs);
            const auto& right = reinterpret_cast<const sockaddr_in&>(rhs);
            return left.sin_port == right.sin_port &&
                left.sin_addr.s_addr == right.sin_addr.s_addr;
        }
        return false;
    }

    bool ResolveEndpoint(
        const char* host,
        u16_t port,
        sockaddr_storage& outEndpoint,
        int& outEndpointLength)
    {
        if (!host || host[0] == '\0')
            return false;

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
        char service[16]{};
        sprintf_s(service, "%u", static_cast<unsigned>(port));

        addrinfo* result = nullptr;
        const int resolveResult = getaddrinfo(host, service, &hints, &result);
        if (resolveResult != 0 || !result ||
            result->ai_addrlen > sizeof(outEndpoint))
        {
            if (result)
                freeaddrinfo(result);
            return false;
        }
        std::memcpy(&outEndpoint, result->ai_addr, result->ai_addrlen);
        outEndpointLength = static_cast<int>(result->ai_addrlen);
        freeaddrinfo(result);
        return true;
    }

    u32_t NextNonZero(u32_t& value)
    {
        const u32_t result = value++;
        if (value == 0u)
            value = 1u;
        return result == 0u ? NextNonZero(value) : result;
    }

    bool IsReliableLane(PacketLane lane)
    {
        switch (lane)
        {
        case PacketLane::Control:
        case PacketLane::Lobby:
        case PacketLane::Command:
        case PacketLane::Event:
            return true;
        case PacketLane::Heartbeat:
        case PacketLane::Snapshot:
        case PacketLane::Telemetry:
        case PacketLane::Invalid:
        default:
            return false;
        }
    }
}

struct CUdpClient::Impl
{
    enum class HandshakeState : u8_t
    {
        Disconnected = 0,
        HelloSent,
        ConnectSent,
        ConfirmSent,
        Connected,
        Failed,
    };

    struct LaneState
    {
        u32_t nextSendMessageSequence = 1u;
        u32_t lastUnreliableMessageSequence = 0u;
        CUdpReliabilityChannel packetReliability;
        CUdpOrderedReceiveQueue orderedReceive{};
        CUdpReassemblyBuffer reassembly{};
    };

    struct OutboundDatagram
    {
        std::vector<u8_t> bytes;
    };

    struct AtomicMetrics
    {
        std::atomic<u64_t> recvDatagrams{ 0u };
        std::atomic<u64_t> recvBytes{ 0u };
        std::atomic<u64_t> sendDatagrams{ 0u };
        std::atomic<u64_t> sendBytes{ 0u };
        std::atomic<u64_t> invalidDatagrams{ 0u };
        std::atomic<u64_t> duplicateDatagrams{ 0u };
        std::atomic<u64_t> retransmits{ 0u };
        std::atomic<u64_t> retryExhausted{ 0u };
        std::atomic<u64_t> reassemblyComplete{ 0u };
        std::atomic<u64_t> reassemblyOverflow{ 0u };
        std::atomic<u64_t> reassemblyTimeout{ 0u };
        std::atomic<u64_t> outboundQueueDrops{ 0u };
    } metrics;

    SOCKET socket = INVALID_SOCKET;
    sockaddr_storage serverEndpoint{};
    int serverEndpointLength = 0;
    std::thread networkThread;
    std::mutex lifecycleMutex;
    std::atomic<bool> running{ false };
    std::atomic<bool> connected{ false };

    mutable std::mutex stateMutex;
    std::condition_variable stateCondition;
    HandshakeState handshakeState = HandshakeState::Disconnected;
    u64_t clientNonce = 0u;
    u64_t connectionId = 0u;
    u32_t generation = 0u;
    u32_t nextHandshakePacketSequence = 1u;
    UdpCookie cookie{};
    u64_t cookieBucket = 0u;
    std::vector<u8_t> ticket;
    u64_t lastHandshakeSendMs = 0u;
    u32_t handshakeAttempts = 0u;
    u64_t lastServerReceiveMs = 0u;
    u64_t lastHeartbeatSendMs = 0u;
    std::array<LaneState, kPacketLaneCount> lanes{};

    mutable std::mutex outboundMutex;
    std::deque<OutboundDatagram> outboundQueue;
    size_t outboundQueueBytes = 0u;

    mutable std::mutex pendingMutex;
    std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>> pendingFrames;
    size_t pendingFrameBytes = 0u;
    FrameCallback frameCallback;

    ~Impl()
    {
        Disconnect();
    }

    bool EnqueueOutboundBatch(std::vector<OutboundDatagram> datagrams)
    {
        if (datagrams.empty() || !running.load(std::memory_order_acquire))
            return false;
        size_t bytes = 0u;
        for (const auto& datagram : datagrams)
        {
            if (datagram.bytes.empty() ||
                datagram.bytes.size() > kUdpMaxDatagramBytes)
            {
                return false;
            }
            bytes += datagram.bytes.size();
        }

        std::lock_guard outboundLock(outboundMutex);
        if (datagrams.size() > kMaxOutboundDatagrams - outboundQueue.size() ||
            bytes > kMaxOutboundBytes - outboundQueueBytes)
        {
            metrics.outboundQueueDrops.fetch_add(
                datagrams.size(),
                std::memory_order_relaxed);
            return false;
        }
        outboundQueueBytes += bytes;
        for (auto& datagram : datagrams)
            outboundQueue.push_back(std::move(datagram));
        return true;
    }

    bool EnqueueOutbound(std::vector<u8_t> datagram)
    {
        std::vector<OutboundDatagram> batch;
        batch.push_back({ std::move(datagram) });
        return EnqueueOutboundBatch(std::move(batch));
    }

    void DrainOutbound(size_t maxDatagrams)
    {
        std::vector<OutboundDatagram> drained;
        {
            std::lock_guard outboundLock(outboundMutex);
            const size_t count = std::min(maxDatagrams, outboundQueue.size());
            drained.reserve(count);
            for (size_t index = 0u; index < count; ++index)
            {
                outboundQueueBytes -= outboundQueue.front().bytes.size();
                drained.push_back(std::move(outboundQueue.front()));
                outboundQueue.pop_front();
            }
        }

        for (const auto& datagram : drained)
        {
            const int sent = sendto(
                socket,
                reinterpret_cast<const char*>(datagram.bytes.data()),
                static_cast<int>(datagram.bytes.size()),
                0,
                reinterpret_cast<const sockaddr*>(&serverEndpoint),
                serverEndpointLength);
            if (sent == static_cast<int>(datagram.bytes.size()))
            {
                metrics.sendDatagrams.fetch_add(1u, std::memory_order_relaxed);
                metrics.sendBytes.fetch_add(
                    datagram.bytes.size(),
                    std::memory_order_relaxed);
            }
            else
            {
                metrics.outboundQueueDrops.fetch_add(1u, std::memory_order_relaxed);
            }
        }
    }

    bool BuildHandshakePacket(
        ePacketType type,
        std::span<const u8_t> payload,
        std::vector<u8_t>& outDatagram)
    {
        UdpPacketHeader header{};
        header.type = type;
        header.lane = PacketLane::Control;
        header.flags = UdpPacketFlag_Handshake;
        header.packetSeq = NextNonZero(nextHandshakePacketSequence);
        header.messageSeq = header.packetSeq;
        return EncodeUdpPacket(header, payload, outDatagram);
    }

    bool SendClientHello()
    {
        std::vector<u8_t> datagram;
        {
            std::lock_guard stateLock(stateMutex);
            UdpClientHelloPayload hello{};
            hello.clientNonce = clientNonce;
            const std::vector<u8_t> payload = EncodeUdpClientHello(hello);
            if (!BuildHandshakePacket(
                ePacketType::TransportClientHello,
                payload,
                datagram))
            {
                return false;
            }
            handshakeState = HandshakeState::HelloSent;
            lastHandshakeSendMs = NowMs();
            ++handshakeAttempts;
        }
        return EnqueueOutbound(std::move(datagram));
    }

    bool SendClientConnect()
    {
        std::vector<u8_t> datagram;
        {
            std::lock_guard stateLock(stateMutex);
            UdpClientConnectPayload connectPayload{};
            connectPayload.clientNonce = clientNonce;
            connectPayload.cookieBucket = cookieBucket;
            connectPayload.cookie = cookie;
            connectPayload.ticket = ticket;
            const std::vector<u8_t> payload =
                EncodeUdpClientConnect(connectPayload);
            if (payload.empty() || !BuildHandshakePacket(
                ePacketType::TransportClientConnect,
                payload,
                datagram))
            {
                return false;
            }
            handshakeState = HandshakeState::ConnectSent;
            lastHandshakeSendMs = NowMs();
            ++handshakeAttempts;
        }
        return EnqueueOutbound(std::move(datagram));
    }

    bool SendClientConfirm()
    {
        std::vector<u8_t> datagram;
        {
            std::lock_guard stateLock(stateMutex);
            if (connectionId == 0u || generation == 0u)
                return false;
            LaneState& controlLane =
                lanes[static_cast<u8_t>(PacketLane::Control)];
            UdpPacketHeader header{};
            header.connectionId = connectionId;
            header.generation = generation;
            header.type = ePacketType::TransportClientConfirm;
            header.lane = PacketLane::Control;
            header.flags = UdpPacketFlag_Handshake;
            header.packetSeq =
                controlLane.packetReliability.NextSendSeq();
            header.messageSeq = 1u;
            controlLane.packetReliability.BuildAck(
                header.ackSeq,
                header.ackBitfield);
            if (!EncodeUdpPacket(header, {}, datagram))
                return false;
            handshakeState = HandshakeState::ConfirmSent;
            lastHandshakeSendMs = NowMs();
            ++handshakeAttempts;
        }
        return EnqueueOutbound(std::move(datagram));
    }

    void SendAckOnly(PacketLane laneValue)
    {
        std::vector<u8_t> datagram;
        {
            std::lock_guard stateLock(stateMutex);
            if (handshakeState != HandshakeState::ConfirmSent &&
                handshakeState != HandshakeState::Connected)
                return;
            LaneState& lane = lanes[static_cast<u8_t>(laneValue)];
            UdpPacketHeader header{};
            header.connectionId = connectionId;
            header.generation = generation;
            header.type = ePacketType::None;
            header.lane = laneValue;
            header.flags = UdpPacketFlag_AckOnly;
            // ACK-only datagrams do not participate in the receive sequence
            // window. A fixed non-zero value satisfies the wire contract
            // without pushing reliable packets outside the 32-bit ACK mask.
            header.packetSeq = 1u;
            lane.packetReliability.BuildAck(
                header.ackSeq,
                header.ackBitfield);
            if (!EncodeUdpPacket(header, {}, datagram))
                return;
        }
        EnqueueOutbound(std::move(datagram));
    }

    void MarkFailedFromWorker()
    {
        connected.store(false, std::memory_order_release);
        running.store(false, std::memory_order_release);
        {
            std::lock_guard stateLock(stateMutex);
            handshakeState = HandshakeState::Failed;
        }
        stateCondition.notify_all();
        if (socket != INVALID_SOCKET)
            shutdown(socket, SD_BOTH);
    }

    void HandleHandshake(const UdpPacketView& packet)
    {
        if (packet.header.type == ePacketType::TransportServerRetry)
        {
            UdpServerRetryPayload retry{};
            if (!DecodeUdpServerRetry(
                std::span<const u8_t>(packet.payload, packet.payloadSize),
                retry))
            {
                metrics.invalidDatagrams.fetch_add(1u, std::memory_order_relaxed);
                return;
            }
            {
                std::lock_guard stateLock(stateMutex);
                if (retry.clientNonce != clientNonce ||
                    (handshakeState != HandshakeState::HelloSent &&
                        handshakeState != HandshakeState::ConnectSent))
                {
                    return;
                }
                cookie = retry.cookie;
                cookieBucket = retry.cookieBucket;
                handshakeAttempts = 0u;
            }
            SendClientConnect();
            return;
        }

        if (packet.header.type != ePacketType::TransportServerAccept)
            return;
        UdpServerAcceptPayload accept{};
        if (!DecodeUdpServerAccept(
            std::span<const u8_t>(packet.payload, packet.payloadSize),
            accept) ||
            accept.connectionId != packet.header.connectionId ||
            accept.generation != packet.header.generation ||
            accept.clientNonce != clientNonce)
        {
            metrics.invalidDatagrams.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        {
            std::lock_guard stateLock(stateMutex);
            const bool firstAccept = connectionId == 0u;
            if (!firstAccept &&
                (connectionId != accept.connectionId ||
                    generation != accept.generation))
            {
                return;
            }
            if (firstAccept)
            {
                connectionId = accept.connectionId;
                generation = accept.generation;
                for (auto& lane : lanes)
                {
                    lane.nextSendMessageSequence = 1u;
                    lane.lastUnreliableMessageSequence = 0u;
                    lane.packetReliability.Reset();
                    lane.orderedReceive.Reset();
                    lane.reassembly.Reset();
                }
            }
            const u64_t nowMs = NowMs();
            LaneState& controlLane =
                lanes[static_cast<u8_t>(PacketLane::Control)];
            controlLane.packetReliability.OnAck(
                packet.header.ackSeq,
                packet.header.ackBitfield,
                nowMs);
            controlLane.packetReliability.MarkReceived(packet.header.packetSeq);
            handshakeAttempts = 0u;
            lastServerReceiveMs = nowMs;
            if (handshakeState == HandshakeState::Connected)
                return;
        }
        SendClientConfirm();
    }

    void QueueDelivered(std::vector<UdpOrderedMessage> delivered)
    {
        if (delivered.empty())
            return;
        bool fatalOverflow = false;
        {
            std::lock_guard pendingLock(pendingMutex);
            for (auto& message : delivered)
            {
                if (message.type == ePacketType::Snapshot)
                {
                    auto iterator = pendingFrames.begin();
                    while (iterator != pendingFrames.end())
                    {
                        if (std::get<0>(*iterator) == ePacketType::Snapshot)
                        {
                            pendingFrameBytes -= std::get<2>(*iterator).size();
                            iterator = pendingFrames.erase(iterator);
                        }
                        else
                            ++iterator;
                    }
                }
                if (pendingFrames.size() >= kMaxPendingFrames ||
                    message.payload.size() >
                        kMaxPendingFrameBytes - pendingFrameBytes)
                {
                    fatalOverflow = message.type != ePacketType::Snapshot;
                    continue;
                }
                pendingFrameBytes += message.payload.size();
                pendingFrames.emplace_back(
                    message.type,
                    message.sequence,
                    std::move(message.payload));
            }
        }
        if (fatalOverflow)
            MarkFailedFromWorker();
    }

    void HandleAssociated(const UdpPacketView& packet)
    {
        std::vector<UdpOrderedMessage> delivered;
        bool shouldAck = false;
        bool fatal = false;
        {
            std::lock_guard stateLock(stateMutex);
            if (packet.header.connectionId != connectionId ||
                packet.header.generation != generation ||
                (handshakeState != HandshakeState::ConfirmSent &&
                    handshakeState != HandshakeState::Connected))
            {
                return;
            }
            const u64_t nowMs = NowMs();
            lastServerReceiveMs = nowMs;
            LaneState& lane =
                lanes[static_cast<u8_t>(packet.header.lane)];
            lane.packetReliability.OnAck(
                packet.header.ackSeq,
                packet.header.ackBitfield,
                nowMs);
            const bool ackOnly =
                (packet.header.flags & UdpPacketFlag_AckOnly) != 0u;
            const PacketSemantics semantics = ackOnly
                ? PacketSemantics{}
                : ResolvePacketSemantics(packet.header.type);
            bool duplicate = false;
            if (!ackOnly)
            {
                const eUdpReceiveStatus receiveStatus =
                    lane.packetReliability.MarkReceived(
                        packet.header.packetSeq);
                const bool allowLateUnreliableFragment =
                    receiveStatus == eUdpReceiveStatus::TooOld &&
                    (packet.header.flags & UdpPacketFlag_Fragment) != 0u &&
                    !IsReliableDelivery(semantics.delivery);
                duplicate =
                    receiveStatus == eUdpReceiveStatus::Duplicate ||
                    (receiveStatus == eUdpReceiveStatus::TooOld &&
                        !allowLateUnreliableFragment);
                if (duplicate)
                {
                    metrics.duplicateDatagrams.fetch_add(
                        1u,
                        std::memory_order_relaxed);
                }
            }
            if (ackOnly &&
                packet.header.lane == PacketLane::Control &&
                handshakeState == HandshakeState::ConfirmSent)
            {
                handshakeState = HandshakeState::Connected;
                connected.store(true, std::memory_order_release);
                stateCondition.notify_all();
            }
            shouldAck = !ackOnly;
            if (duplicate || ackOnly)
            {
                // ACK state and activation evidence were consumed above.
            }
            else
            {
                UdpReassembledMessage complete{};
                bool completeReady = false;
                if ((packet.header.flags & UdpPacketFlag_Fragment) != 0u)
                {
                    const bool staleUnreliableFragment =
                        !IsReliableDelivery(semantics.delivery) &&
                        lane.lastUnreliableMessageSequence != 0u &&
                        !SeqGreater(
                            packet.header.messageSeq,
                            lane.lastUnreliableMessageSequence);
                    const eUdpReassemblyResult result =
                        staleUnreliableFragment
                            ? eUdpReassemblyResult::Duplicate
                            : lane.reassembly.Push(
                                packet.header,
                                std::span<const u8_t>(
                                    packet.payload,
                                    packet.payloadSize),
                                NowMs(),
                                complete);
                    if (result == eUdpReassemblyResult::Complete)
                    {
                        completeReady = true;
                        metrics.reassemblyComplete.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                    }
                    else if (result == eUdpReassemblyResult::Overflow)
                    {
                        metrics.reassemblyOverflow.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                        fatal = IsReliableDelivery(semantics.delivery);
                    }
                    else if (result == eUdpReassemblyResult::Invalid)
                    {
                        metrics.invalidDatagrams.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                        fatal = IsReliableDelivery(semantics.delivery);
                    }
                }
                else
                {
                    complete.type = packet.header.type;
                    complete.lane = packet.header.lane;
                    complete.messageSequence = packet.header.messageSeq;
                    complete.payload.assign(
                        packet.payload,
                        packet.payload + packet.payloadSize);
                    completeReady = true;
                }

                if (completeReady &&
                    semantics.delivery == PacketDelivery::ReliableOrdered)
                {
                    const eUdpOrderedPushResult pushResult =
                        lane.orderedReceive.Push(
                            complete.type,
                            complete.messageSequence,
                            std::move(complete.payload),
                            delivered);
                    fatal = pushResult == eUdpOrderedPushResult::Overflow;
                }
                else if (completeReady &&
                    (lane.lastUnreliableMessageSequence == 0u ||
                        SeqGreater(
                            complete.messageSequence,
                            lane.lastUnreliableMessageSequence)))
                {
                    lane.lastUnreliableMessageSequence =
                        complete.messageSequence;
                    delivered.push_back({
                        complete.type,
                        complete.messageSequence,
                        std::move(complete.payload),
                    });
                }
            }
        }

        if (shouldAck)
            SendAckOnly(packet.header.lane);
        if (fatal)
        {
            MarkFailedFromWorker();
            return;
        }
        QueueDelivered(std::move(delivered));
    }

    void HandleDatagram(
        const sockaddr_storage& source,
        int sourceLength,
        const u8_t* bytes,
        size_t byteCount)
    {
        metrics.recvDatagrams.fetch_add(1u, std::memory_order_relaxed);
        metrics.recvBytes.fetch_add(byteCount, std::memory_order_relaxed);
        if (!EndpointEquals(
            source,
            sourceLength,
            serverEndpoint,
            serverEndpointLength))
        {
            metrics.invalidDatagrams.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        UdpPacketView packet{};
        if (!DecodeUdpPacket(bytes, byteCount, packet))
        {
            metrics.invalidDatagrams.fetch_add(1u, std::memory_order_relaxed);
            return;
        }
        if (IsTransportHandshakePacketType(packet.header.type))
            HandleHandshake(packet);
        else
            HandleAssociated(packet);
    }

    void RunMaintenance()
    {
        const u64_t nowMs = NowMs();
        HandshakeState state = HandshakeState::Disconnected;
        u64_t handshakeElapsed = 0u;
        u32_t attempts = 0u;
        bool sendHeartbeat = false;
        bool serverTimedOut = false;
        std::vector<std::vector<u8_t>> retransmits;
        bool reliableFailure = false;
        {
            std::lock_guard stateLock(stateMutex);
            state = handshakeState;
            handshakeElapsed = nowMs - lastHandshakeSendMs;
            attempts = handshakeAttempts;
            if (state == HandshakeState::Connected)
            {
                sendHeartbeat = nowMs - lastHeartbeatSendMs >=
                    kHeartbeatIntervalMs;
                serverTimedOut = nowMs - lastServerReceiveMs >=
                    kServerIdleTimeoutMs;
            }

            for (u8_t laneIndex = 1u;
                laneIndex < kPacketLaneCount;
                ++laneIndex)
            {
                LaneState& lane = lanes[laneIndex];
                const u64_t retryBefore =
                    lane.packetReliability.GetRetryExhaustedCount();
                const u64_t retransmitBefore =
                    lane.packetReliability.GetRetransmitCount();
                lane.packetReliability.CollectRetransmit(nowMs, retransmits);
                const u64_t retryAfter =
                    lane.packetReliability.GetRetryExhaustedCount();
                const u64_t retransmitAfter =
                    lane.packetReliability.GetRetransmitCount();
                metrics.retryExhausted.fetch_add(
                    retryAfter - retryBefore,
                    std::memory_order_relaxed);
                metrics.retransmits.fetch_add(
                    retransmitAfter - retransmitBefore,
                    std::memory_order_relaxed);
                if (retryAfter != retryBefore)
                    reliableFailure = true;
                const size_t expired = lane.reassembly.Expire(nowMs);
                metrics.reassemblyTimeout.fetch_add(
                    expired,
                    std::memory_order_relaxed);
                if (expired != 0u &&
                    IsReliableLane(static_cast<PacketLane>(laneIndex)))
                {
                    reliableFailure = true;
                }
            }
        }

        std::vector<OutboundDatagram> retransmitBatch;
        retransmitBatch.reserve(retransmits.size());
        for (auto& datagram : retransmits)
            retransmitBatch.push_back({ std::move(datagram) });
        if (!retransmitBatch.empty())
            EnqueueOutboundBatch(std::move(retransmitBatch));

        if (serverTimedOut || reliableFailure)
        {
            MarkFailedFromWorker();
            return;
        }
        if (attempts >= kMaxHandshakeAttempts &&
            state != HandshakeState::Connected)
        {
            MarkFailedFromWorker();
            return;
        }
        if (handshakeElapsed >= kHandshakeRetryMs)
        {
            if (state == HandshakeState::HelloSent)
                SendClientHello();
            else if (state == HandshakeState::ConnectSent)
                SendClientConnect();
            else if (state == HandshakeState::ConfirmSent)
                SendClientConfirm();
        }
        if (sendHeartbeat)
        {
            {
                std::lock_guard stateLock(stateMutex);
                lastHeartbeatSendMs = nowMs;
            }
            Send(ePacketType::Heartbeat, nullptr, 0u);
        }
    }

    void NetworkLoop()
    {
        std::array<u8_t, kUdpMaxDatagramBytes> receiveBuffer{};
        while (running.load(std::memory_order_acquire))
        {
            DrainOutbound(64u);

            sockaddr_storage source{};
            int sourceLength = sizeof(source);
            const int received = recvfrom(
                socket,
                reinterpret_cast<char*>(receiveBuffer.data()),
                static_cast<int>(receiveBuffer.size()),
                0,
                reinterpret_cast<sockaddr*>(&source),
                &sourceLength);
            if (received > 0)
            {
                HandleDatagram(
                    source,
                    sourceLength,
                    receiveBuffer.data(),
                    static_cast<size_t>(received));
            }
            else if (received == SOCKET_ERROR)
            {
                const int error = WSAGetLastError();
                if (error == WSAEMSGSIZE)
                {
                    metrics.invalidDatagrams.fetch_add(
                        1u,
                        std::memory_order_relaxed);
                    RunMaintenance();
                    continue;
                }
                if (error != WSAETIMEDOUT &&
                    error != WSAEWOULDBLOCK &&
                    error != WSAEINTR &&
                    running.load(std::memory_order_acquire))
                {
                    MarkFailedFromWorker();
                    break;
                }
            }
            RunMaintenance();
        }
    }

    void DisconnectLocked()
    {
        connected.store(false, std::memory_order_release);
        running.store(false, std::memory_order_release);
        const SOCKET closingSocket = socket;
        if (closingSocket != INVALID_SOCKET)
        {
            shutdown(closingSocket, SD_BOTH);
            closesocket(closingSocket);
        }
        if (networkThread.joinable() &&
            networkThread.get_id() != std::this_thread::get_id())
        {
            networkThread.join();
        }
        socket = INVALID_SOCKET;
        {
            std::lock_guard stateLock(stateMutex);
            handshakeState = HandshakeState::Disconnected;
            connectionId = 0u;
            generation = 0u;
            ticket.clear();
        }
        {
            std::lock_guard outboundLock(outboundMutex);
            outboundQueue.clear();
            outboundQueueBytes = 0u;
        }
        {
            std::lock_guard pendingLock(pendingMutex);
            pendingFrames.clear();
            pendingFrameBytes = 0u;
        }
    }

    bool Connect(
        const char* host,
        u16_t port,
        std::span<const u8_t> connectTicket,
        u32_t timeoutMs)
    {
        std::lock_guard lifecycleLock(lifecycleMutex);
        sockaddr_storage resolvedEndpoint{};
        int resolvedEndpointLength = 0;
        if (running.load(std::memory_order_acquire) ||
            timeoutMs == 0u ||
            connectTicket.size() > kUdpMaxTicketBytes ||
            !ResolveEndpoint(
                host,
                port,
                resolvedEndpoint,
                resolvedEndpointLength))
        {
            return false;
        }

        if (networkThread.joinable() || socket != INVALID_SOCKET)
            DisconnectLocked();
        serverEndpoint = resolvedEndpoint;
        serverEndpointLength = resolvedEndpointLength;

        socket = WSASocketW(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP,
            nullptr,
            0u,
            WSA_FLAG_OVERLAPPED);
        if (socket == INVALID_SOCKET)
            return false;

        DWORD receiveTimeoutMs = 10u;
        setsockopt(
            socket,
            SOL_SOCKET,
            SO_RCVTIMEO,
            reinterpret_cast<const char*>(&receiveTimeoutMs),
            sizeof(receiveTimeoutMs));
        BOOL disableConnectionReset = FALSE;
        DWORD ignored = 0u;
        WSAIoctl(
            socket,
            SIO_UDP_CONNRESET,
            &disableConnectionReset,
            sizeof(disableConnectionReset),
            nullptr,
            0u,
            &ignored,
            nullptr,
            nullptr);

        sockaddr_in localEndpoint{};
        localEndpoint.sin_family = AF_INET;
        localEndpoint.sin_addr.s_addr = htonl(INADDR_ANY);
        localEndpoint.sin_port = htons(0u);
        if (bind(
            socket,
            reinterpret_cast<const sockaddr*>(&localEndpoint),
            sizeof(localEndpoint)) == SOCKET_ERROR)
        {
            closesocket(socket);
            socket = INVALID_SOCKET;
            return false;
        }

        {
            std::lock_guard stateLock(stateMutex);
            clientNonce = 0u;
            if (!FillRandom(&clientNonce, sizeof(clientNonce)) ||
                clientNonce == 0u)
            {
                closesocket(socket);
                socket = INVALID_SOCKET;
                return false;
            }
            ticket.assign(connectTicket.begin(), connectTicket.end());
            connectionId = 0u;
            generation = 0u;
            nextHandshakePacketSequence = 1u;
            handshakeState = HandshakeState::Disconnected;
            handshakeAttempts = 0u;
            lastServerReceiveMs = NowMs();
            lastHeartbeatSendMs = NowMs();
        }

        running.store(true, std::memory_order_release);
        networkThread = std::thread(&Impl::NetworkLoop, this);
        if (!SendClientHello())
        {
            DisconnectLocked();
            return false;
        }

        std::unique_lock stateLock(stateMutex);
        const bool completed = stateCondition.wait_for(
            stateLock,
            std::chrono::milliseconds(timeoutMs),
            [this]()
            {
                return handshakeState == HandshakeState::Connected ||
                    handshakeState == HandshakeState::Failed;
            });
        const bool succeeded = completed &&
            handshakeState == HandshakeState::Connected;
        stateLock.unlock();
        if (!succeeded)
            DisconnectLocked();
        return succeeded;
    }

    void Disconnect()
    {
        std::lock_guard lifecycleLock(lifecycleMutex);
        DisconnectLocked();
    }

    bool Send(ePacketType type, const u8_t* payload, u32_t payloadSize)
    {
        if (!connected.load(std::memory_order_acquire) ||
            !IsKnownPacketType(type) ||
            IsTransportHandshakePacketType(type) ||
            (payloadSize != 0u && !payload) ||
            payloadSize > kUdpMaxMessageBytes)
        {
            return false;
        }

        const PacketSemantics semantics = ResolvePacketSemantics(type);
        std::vector<std::pair<u32_t, std::vector<u8_t>>> encoded;
        {
            std::lock_guard stateLock(stateMutex);
            LaneState& lane = lanes[static_cast<u8_t>(semantics.lane)];
            const size_t packetCount =
                payloadSize <= kUdpMaxPacketPayloadBytes
                    ? 1u
                    : (payloadSize + kUdpMaxFragmentDataBytes - 1u) /
                        kUdpMaxFragmentDataBytes;
            const size_t encodedBytes =
                payloadSize <= kUdpMaxPacketPayloadBytes
                    ? kUdpPacketHeaderBytes + payloadSize
                    : packetCount *
                            (kUdpPacketHeaderBytes + kUdpFragmentHeaderBytes) +
                        payloadSize;
            if (IsReliableDelivery(semantics.delivery) &&
                !lane.packetReliability.CanQueueReliable(
                    packetCount,
                    encodedBytes))
            {
                return false;
            }
            const u32_t messageSequence =
                lane.nextSendMessageSequence == 0u
                    ? 1u
                    : lane.nextSendMessageSequence;
            UdpPacketHeader baseHeader{};
            baseHeader.connectionId = connectionId;
            baseHeader.generation = generation;
            baseHeader.type = type;
            baseHeader.lane = semantics.lane;
            baseHeader.messageSeq = messageSequence;
            lane.packetReliability.BuildAck(
                baseHeader.ackSeq,
                baseHeader.ackBitfield);

            if (payloadSize <= kUdpMaxPacketPayloadBytes)
            {
                baseHeader.packetSeq =
                    lane.packetReliability.NextSendSeq();
                std::vector<u8_t> datagram;
                if (!EncodeUdpPacket(
                    baseHeader,
                    std::span<const u8_t>(payload, payloadSize),
                    datagram))
                {
                    return false;
                }
                encoded.emplace_back(
                    baseHeader.packetSeq,
                    std::move(datagram));
            }
            else
            {
                const u16_t fragmentCount = static_cast<u16_t>(
                    (payloadSize + kUdpMaxFragmentDataBytes - 1u) /
                    kUdpMaxFragmentDataBytes);
                encoded.reserve(fragmentCount);
                for (u16_t fragmentIndex = 0u;
                    fragmentIndex < fragmentCount;
                    ++fragmentIndex)
                {
                    const u32_t offset =
                        static_cast<u32_t>(fragmentIndex) *
                        kUdpMaxFragmentDataBytes;
                    const u16_t fragmentBytes = static_cast<u16_t>(
                        std::min<u32_t>(
                            kUdpMaxFragmentDataBytes,
                            payloadSize - offset));
                    UdpFragmentHeader fragment{};
                    fragment.messageId = messageSequence;
                    fragment.messageBytes = payloadSize;
                    fragment.fragmentIndex = fragmentIndex;
                    fragment.fragmentCount = fragmentCount;
                    fragment.fragmentPayloadSize = fragmentBytes;
                    std::vector<u8_t> fragmentPayload(
                        kUdpFragmentHeaderBytes + fragmentBytes);
                    EncodeUdpFragmentHeader(fragment, fragmentPayload.data());
                    std::copy_n(
                        payload + offset,
                        fragmentBytes,
                        fragmentPayload.data() + kUdpFragmentHeaderBytes);

                    UdpPacketHeader fragmentHeader = baseHeader;
                    fragmentHeader.flags = UdpPacketFlag_Fragment;
                    fragmentHeader.packetSeq =
                        lane.packetReliability.NextSendSeq();
                    std::vector<u8_t> datagram;
                    if (!EncodeUdpPacket(
                        fragmentHeader,
                        fragmentPayload,
                        datagram))
                    {
                        return false;
                    }
                    encoded.emplace_back(
                        fragmentHeader.packetSeq,
                        std::move(datagram));
                }
            }

            size_t reliableBytes = 0u;
            for (const auto& packet : encoded)
                reliableBytes += packet.second.size();
            std::lock_guard outboundLock(outboundMutex);
            if (encoded.size() >
                    kMaxOutboundDatagrams - outboundQueue.size() ||
                reliableBytes > kMaxOutboundBytes - outboundQueueBytes)
            {
                metrics.outboundQueueDrops.fetch_add(
                    encoded.size(),
                    std::memory_order_relaxed);
                return false;
            }
            if (IsReliableDelivery(semantics.delivery))
            {
                const u64_t nowMs = NowMs();
                for (const auto& packet : encoded)
                {
                    if (!lane.packetReliability.QueueReliable(
                        packet.first,
                        packet.second,
                        nowMs))
                    {
                        return false;
                    }
                }
            }
            outboundQueueBytes += reliableBytes;
            for (auto& packet : encoded)
                outboundQueue.push_back({ std::move(packet.second) });
            const u32_t committedSequence = NextNonZero(
                lane.nextSendMessageSequence);
            return committedSequence == messageSequence;
        }
    }

    void PumpReceivedFrames()
    {
        FrameCallback callback;
        std::vector<std::tuple<ePacketType, u32_t, std::vector<u8_t>>> drained;
        {
            std::lock_guard pendingLock(pendingMutex);
            callback = frameCallback;
            pendingFrameBytes = 0u;
            drained.swap(pendingFrames);
        }
        if (!callback)
            return;
        for (auto& [type, sequence, payload] : drained)
        {
            callback(
                type,
                sequence,
                payload.data(),
                static_cast<u32_t>(payload.size()));
        }
    }

    UdpClientMetrics SnapshotMetrics() const
    {
        UdpClientMetrics result{};
        result.recvDatagrams = metrics.recvDatagrams.load(std::memory_order_relaxed);
        result.recvBytes = metrics.recvBytes.load(std::memory_order_relaxed);
        result.sendDatagrams = metrics.sendDatagrams.load(std::memory_order_relaxed);
        result.sendBytes = metrics.sendBytes.load(std::memory_order_relaxed);
        result.invalidDatagrams = metrics.invalidDatagrams.load(std::memory_order_relaxed);
        result.duplicateDatagrams = metrics.duplicateDatagrams.load(std::memory_order_relaxed);
        result.retransmits = metrics.retransmits.load(std::memory_order_relaxed);
        result.retryExhausted = metrics.retryExhausted.load(std::memory_order_relaxed);
        result.reassemblyComplete = metrics.reassemblyComplete.load(std::memory_order_relaxed);
        result.reassemblyOverflow = metrics.reassemblyOverflow.load(std::memory_order_relaxed);
        result.reassemblyTimeout = metrics.reassemblyTimeout.load(std::memory_order_relaxed);
        result.outboundQueueDrops = metrics.outboundQueueDrops.load(std::memory_order_relaxed);
        {
            std::lock_guard outboundLock(outboundMutex);
            result.outboundQueueBytes = outboundQueueBytes;
        }
        return result;
    }
};

CUdpClient::CUdpClient()
    : m_impl(std::make_unique<Impl>())
{
}

std::unique_ptr<CUdpClient> CUdpClient::Create()
{
    if (!EnsureWsa())
        return nullptr;
    return std::unique_ptr<CUdpClient>(new CUdpClient());
}

CUdpClient::~CUdpClient()
{
    m_impl->Disconnect();
    ReleaseWsa();
}

bool CUdpClient::Connect(
    const char* host,
    u16_t port,
    std::span<const u8_t> ticket,
    u32_t timeoutMs)
{
    return m_impl->Connect(host, port, ticket, timeoutMs);
}

bool CUdpClient::Connect(const char* host, u16_t port)
{
    return Connect(host, port, {}, 3'000u);
}

void CUdpClient::Disconnect()
{
    m_impl->Disconnect();
}

bool CUdpClient::Send(
    ePacketType type,
    const u8_t* payload,
    u32_t payloadSize)
{
    return m_impl->Send(type, payload, payloadSize);
}

void CUdpClient::SetFrameCallback(FrameCallback callback)
{
    std::lock_guard pendingLock(m_impl->pendingMutex);
    m_impl->frameCallback = std::move(callback);
}

void CUdpClient::PumpReceivedFrames()
{
    m_impl->PumpReceivedFrames();
}

bool CUdpClient::IsConnected() const
{
    return m_impl->connected.load(std::memory_order_acquire);
}

u64_t CUdpClient::GetConnectionId() const
{
    std::lock_guard stateLock(m_impl->stateMutex);
    return m_impl->connectionId;
}

u32_t CUdpClient::GetGeneration() const
{
    std::lock_guard stateLock(m_impl->stateMutex);
    return m_impl->generation;
}

UdpClientMetrics CUdpClient::GetMetrics() const
{
    return m_impl->SnapshotMetrics();
}
