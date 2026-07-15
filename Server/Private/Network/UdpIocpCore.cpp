#include "Network/UdpIocpCore.h"

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
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
    constexpr u64_t kCookieBucketSeconds = 30u;
    constexpr u64_t kPeerIdleTimeoutMs = 15'000u;
    constexpr u32_t kMaxPeers = 64u;
    constexpr u32_t kMaxReceiveDepth = 64u;
    constexpr u32_t kMaxWorkerCount = 8u;
    constexpr size_t kMaxOutboundDatagrams = 4096u;
    constexpr size_t kMaxOutboundBytes = 4u * 1024u * 1024u;
    constexpr size_t kMaxDispatchFrames = 256u;
    constexpr size_t kMaxDispatchBytes = 2u * 1024u * 1024u;

    u64_t NowMs()
    {
        return static_cast<u64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    u64_t CurrentCookieBucket()
    {
        return static_cast<u64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) /
            kCookieBucketSeconds;
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

    bool FillRandom(void* destination, size_t size)
    {
        if (!destination || size == 0u || size > ULONG_MAX)
            return false;
        return BCryptGenRandom(
            nullptr,
            static_cast<PUCHAR>(destination),
            static_cast<ULONG>(size),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
    }

    bool ComputeHmacSha256(
        std::span<const u8_t> key,
        std::span<const u8_t> message,
        std::array<u8_t, 32u>& outDigest)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectBytes = 0u;
        DWORD hashBytes = 0u;
        DWORD written = 0u;
        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            BCRYPT_ALG_HANDLE_HMAC_FLAG);
        if (status < 0)
            return false;

        status = BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectBytes),
            sizeof(objectBytes),
            &written,
            0u);
        if (status >= 0)
        {
            status = BCryptGetProperty(
                algorithm,
                BCRYPT_HASH_LENGTH,
                reinterpret_cast<PUCHAR>(&hashBytes),
                sizeof(hashBytes),
                &written,
                0u);
        }

        std::vector<u8_t> object;
        if (status >= 0 && hashBytes == outDigest.size())
        {
            object.resize(objectBytes);
            status = BCryptCreateHash(
                algorithm,
                &hash,
                object.data(),
                static_cast<ULONG>(object.size()),
                const_cast<PUCHAR>(key.data()),
                static_cast<ULONG>(key.size()),
                0u);
        }
        else if (status >= 0)
        {
            status = static_cast<NTSTATUS>(-1);
        }

        if (status >= 0)
        {
            status = BCryptHashData(
                hash,
                const_cast<PUCHAR>(message.data()),
                static_cast<ULONG>(message.size()),
                0u);
        }
        if (status >= 0)
        {
            status = BCryptFinishHash(
                hash,
                outDigest.data(),
                static_cast<ULONG>(outDigest.size()),
                0u);
        }

        if (hash)
            BCryptDestroyHash(hash);
        BCryptCloseAlgorithmProvider(algorithm, 0u);
        return status >= 0;
    }

    bool ConstantTimeCookieEquals(const UdpCookie& lhs, const UdpCookie& rhs)
    {
        u8_t difference = 0u;
        for (size_t index = 0u; index < lhs.size(); ++index)
            difference |= static_cast<u8_t>(lhs[index] ^ rhs[index]);
        return difference == 0u;
    }

    u32_t NextNonZero(u32_t& value)
    {
        const u32_t result = value++;
        if (value == 0u)
            value = 1u;
        return result == 0u ? NextNonZero(value) : result;
    }
}

struct CUdpIocpCore::Impl
{
    enum class IoOperation : u8_t
    {
        Receive = 0,
        Send,
    };

    struct IoContext
    {
        OVERLAPPED overlapped{};
        IoOperation operation = IoOperation::Receive;
        WSABUF wsaBuffer{};
        std::array<u8_t, kUdpMaxDatagramBytes> bytes{};
        sockaddr_storage endpoint{};
        int endpointLength = sizeof(sockaddr_storage);
        DWORD flags = 0u;
    };

    struct LaneState
    {
        u32_t nextSendMessageSequence = 1u;
        u32_t lastUnreliableMessageSequence = 0u;
        CUdpReliabilityChannel packetReliability;
        CUdpOrderedReceiveQueue orderedReceive{};
        CUdpReassemblyBuffer reassembly{};
    };

    struct Peer
    {
        std::mutex mutex;
        u64_t connectionId = 0;
        u32_t generation = 1u;
        u64_t clientNonce = 0;
        UdpCookie cookie{};
        sockaddr_storage endpoint{};
        int endpointLength = 0;
        bool confirmed = false;
        bool activationPublished = false;
        u32_t acceptPacketSequence = 0u;
        u64_t lastReceiveMs = 0;
        std::array<LaneState, kPacketLaneCount> lanes{};
        std::deque<UdpServerInboundFrame> dispatchQueue;
        size_t dispatchBytes = 0u;
        bool dispatching = false;
    };

    struct OutboundDatagram
    {
        sockaddr_storage endpoint{};
        int endpointLength = 0;
        std::vector<u8_t> bytes;
    };

    struct AtomicMetrics
    {
        std::atomic<u64_t> recvDatagrams{ 0u };
        std::atomic<u64_t> recvBytes{ 0u };
        std::atomic<u64_t> sendDatagrams{ 0u };
        std::atomic<u64_t> sendBytes{ 0u };
        std::atomic<u64_t> invalidDatagrams{ 0u };
        std::atomic<u64_t> invalidCookies{ 0u };
        std::atomic<u64_t> rejectedTickets{ 0u };
        std::atomic<u64_t> unknownConnections{ 0u };
        std::atomic<u64_t> duplicateDatagrams{ 0u };
        std::atomic<u64_t> orderedBuffered{ 0u };
        std::atomic<u64_t> orderedOverflow{ 0u };
        std::atomic<u64_t> reassemblyComplete{ 0u };
        std::atomic<u64_t> reassemblyOverflow{ 0u };
        std::atomic<u64_t> reassemblyTimeout{ 0u };
        std::atomic<u64_t> retransmits{ 0u };
        std::atomic<u64_t> retryExhausted{ 0u };
        std::atomic<u64_t> outboundQueueDrops{ 0u };
    } metrics;

    SOCKET socket = INVALID_SOCKET;
    HANDLE completionPort = nullptr;
    std::atomic<bool> running{ false };
    std::atomic<u64_t> outstandingIo{ 0u };
    std::atomic<u64_t> lastMaintenanceMs{ 0u };
    std::atomic<u32_t> handshakePacketSequence{ 1u };
    u16_t boundPort = 0u;
    u32_t workerCount = 0u;
    std::array<u8_t, 32u> cookieSecret{};
    std::vector<std::unique_ptr<IoContext>> receiveContexts;
    std::vector<std::thread> workers;

    mutable std::mutex outboundMutex;
    std::deque<OutboundDatagram> outboundQueue;
    size_t outboundQueueBytes = 0u;

    mutable std::mutex callbackMutex;
    TicketValidator ticketValidator;
    ConnectionCallback connectionCallback;
    FrameCallback frameCallback;

    mutable std::mutex peerMutex;
    std::unordered_map<u64_t, std::shared_ptr<Peer>> peers;

    ~Impl()
    {
        Shutdown();
    }

    u32_t NextHandshakePacketSequence()
    {
        u32_t sequence = handshakePacketSequence.fetch_add(
            1u,
            std::memory_order_relaxed);
        if (sequence == 0u)
        {
            sequence = handshakePacketSequence.fetch_add(
                1u,
                std::memory_order_relaxed);
        }
        return sequence;
    }

    bool BuildCookie(
        const sockaddr_storage& endpoint,
        int endpointLength,
        u64_t clientNonce,
        u64_t bucket,
        UdpCookie& outCookie) const
    {
        if (endpoint.ss_family != AF_INET ||
            endpointLength < static_cast<int>(sizeof(sockaddr_in)))
        {
            return false;
        }

        const auto& address = reinterpret_cast<const sockaddr_in&>(endpoint);
        std::array<u8_t, 23u> input{};
        input[0] = kUdpPacketVersion;
        std::memcpy(input.data() + 1u, &address.sin_addr.s_addr, 4u);
        std::memcpy(input.data() + 5u, &address.sin_port, 2u);
        UdpWire::WriteU64(input.data() + 7u, clientNonce);
        UdpWire::WriteU64(input.data() + 15u, bucket);

        std::array<u8_t, 32u> digest{};
        if (!ComputeHmacSha256(cookieSecret, input, digest))
            return false;
        std::copy_n(digest.begin(), outCookie.size(), outCookie.begin());
        return true;
    }

    bool ValidateCookie(
        const sockaddr_storage& endpoint,
        int endpointLength,
        const UdpClientConnectPayload& connectPayload) const
    {
        const u64_t currentBucket = CurrentCookieBucket();
        if (connectPayload.cookieBucket > currentBucket ||
            currentBucket - connectPayload.cookieBucket > 1u)
        {
            return false;
        }

        UdpCookie expected{};
        return BuildCookie(
            endpoint,
            endpointLength,
            connectPayload.clientNonce,
            connectPayload.cookieBucket,
            expected) &&
            ConstantTimeCookieEquals(expected, connectPayload.cookie);
    }

    bool PostSendRaw(
        const sockaddr_storage& endpoint,
        int endpointLength,
        std::span<const u8_t> datagram)
    {
        if (!running.load(std::memory_order_acquire) ||
            socket == INVALID_SOCKET ||
            datagram.empty() ||
            datagram.size() > kUdpMaxDatagramBytes ||
            endpointLength <= 0)
        {
            return false;
        }

        auto context = std::make_unique<IoContext>();
        context->operation = IoOperation::Send;
        std::copy(datagram.begin(), datagram.end(), context->bytes.begin());
        context->wsaBuffer.buf = reinterpret_cast<char*>(context->bytes.data());
        context->wsaBuffer.len = static_cast<ULONG>(datagram.size());
        context->endpoint = endpoint;
        context->endpointLength = endpointLength;

        DWORD bytesSent = 0u;
        outstandingIo.fetch_add(1u, std::memory_order_acq_rel);
        const int result = WSASendTo(
            socket,
            &context->wsaBuffer,
            1u,
            &bytesSent,
            0u,
            reinterpret_cast<const sockaddr*>(&context->endpoint),
            context->endpointLength,
            &context->overlapped,
            nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
        {
            outstandingIo.fetch_sub(1u, std::memory_order_acq_rel);
            return false;
        }

        context.release();
        metrics.sendDatagrams.fetch_add(1u, std::memory_order_relaxed);
        metrics.sendBytes.fetch_add(datagram.size(), std::memory_order_relaxed);
        return true;
    }

    bool EnqueueOutboundBatch(std::vector<OutboundDatagram> datagrams)
    {
        if (datagrams.empty() || !running.load(std::memory_order_acquire))
            return false;

        size_t byteCount = 0u;
        for (const auto& datagram : datagrams)
        {
            if (datagram.bytes.empty() ||
                datagram.bytes.size() > kUdpMaxDatagramBytes)
            {
                return false;
            }
            byteCount += datagram.bytes.size();
        }

        {
            std::lock_guard outboundLock(outboundMutex);
            if (datagrams.size() > kMaxOutboundDatagrams - outboundQueue.size() ||
                byteCount > kMaxOutboundBytes - outboundQueueBytes)
            {
                metrics.outboundQueueDrops.fetch_add(
                    datagrams.size(),
                    std::memory_order_relaxed);
                return false;
            }
            outboundQueueBytes += byteCount;
            for (auto& datagram : datagrams)
                outboundQueue.push_back(std::move(datagram));
        }
        PostQueuedCompletionStatus(completionPort, 0u, 1u, nullptr);
        return true;
    }

    bool EnqueueOutbound(
        const sockaddr_storage& endpoint,
        int endpointLength,
        std::vector<u8_t> datagram)
    {
        std::vector<OutboundDatagram> batch;
        batch.push_back({ endpoint, endpointLength, std::move(datagram) });
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
            if (!PostSendRaw(
                datagram.endpoint,
                datagram.endpointLength,
                datagram.bytes))
            {
                metrics.outboundQueueDrops.fetch_add(1u, std::memory_order_relaxed);
            }
        }
    }

    bool SendHandshake(
        const sockaddr_storage& endpoint,
        int endpointLength,
        UdpPacketHeader header,
        std::span<const u8_t> payload)
    {
        header.lane = PacketLane::Control;
        header.flags = UdpPacketFlag_Handshake;
        header.packetSeq = NextHandshakePacketSequence();
        header.messageSeq = header.packetSeq;
        std::vector<u8_t> datagram;
        return EncodeUdpPacket(header, payload, datagram) &&
            EnqueueOutbound(endpoint, endpointLength, std::move(datagram));
    }

    void SendRetry(
        const sockaddr_storage& endpoint,
        int endpointLength,
        u64_t clientNonce)
    {
        UdpServerRetryPayload retry{};
        retry.clientNonce = clientNonce;
        retry.cookieBucket = CurrentCookieBucket();
        if (!BuildCookie(
            endpoint,
            endpointLength,
            clientNonce,
            retry.cookieBucket,
            retry.cookie))
        {
            return;
        }

        const std::vector<u8_t> payload = EncodeUdpServerRetry(retry);
        UdpPacketHeader header{};
        header.type = ePacketType::TransportServerRetry;
        SendHandshake(endpoint, endpointLength, header, payload);
    }

    void SendAccept(const std::shared_ptr<Peer>& peer)
    {
        UdpServerAcceptPayload accept{};
        sockaddr_storage endpoint{};
        int endpointLength = 0;
        UdpPacketHeader header{};
        {
            std::lock_guard peerLock(peer->mutex);
            accept.connectionId = peer->connectionId;
            accept.generation = peer->generation;
            accept.clientNonce = peer->clientNonce;
            header.connectionId = peer->connectionId;
            header.generation = peer->generation;
            header.type = ePacketType::TransportServerAccept;
            header.lane = PacketLane::Control;
            header.flags = UdpPacketFlag_Handshake;
            LaneState& controlLane =
                peer->lanes[static_cast<u8_t>(PacketLane::Control)];
            if (peer->acceptPacketSequence == 0u)
            {
                peer->acceptPacketSequence =
                    controlLane.packetReliability.NextSendSeq();
            }
            header.packetSeq = peer->acceptPacketSequence;
            header.messageSeq = 1u;
            controlLane.packetReliability.BuildAck(
                header.ackSeq,
                header.ackBitfield);
            endpoint = peer->endpoint;
            endpointLength = peer->endpointLength;
        }
        const std::vector<u8_t> payload = EncodeUdpServerAccept(accept);
        std::vector<u8_t> datagram;
        if (EncodeUdpPacket(header, payload, datagram))
            EnqueueOutbound(endpoint, endpointLength, std::move(datagram));
    }

    std::shared_ptr<Peer> FindPeer(u64_t connectionId) const
    {
        std::lock_guard registryLock(peerMutex);
        const auto iterator = peers.find(connectionId);
        return iterator == peers.end() ? nullptr : iterator->second;
    }

    void HandleClientConnect(
        const sockaddr_storage& endpoint,
        int endpointLength,
        const UdpPacketView& packet)
    {
        UdpClientConnectPayload connectPayload{};
        if (!DecodeUdpClientConnect(
            std::span<const u8_t>(packet.payload, packet.payloadSize),
            connectPayload) ||
            !ValidateCookie(endpoint, endpointLength, connectPayload))
        {
            metrics.invalidCookies.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        TicketValidator validator;
        {
            std::lock_guard callbackLock(callbackMutex);
            validator = ticketValidator;
        }
        bool ticketAccepted = false;
        try
        {
            ticketAccepted = validator && validator(connectPayload.ticket);
        }
        catch (...)
        {
            ticketAccepted = false;
        }
        if (!ticketAccepted)
        {
            metrics.rejectedTickets.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        std::shared_ptr<Peer> peer;
        {
            std::lock_guard registryLock(peerMutex);
            for (const auto& [registeredId, registeredPeer] : peers)
            {
                (void)registeredId;
                std::lock_guard peerLock(registeredPeer->mutex);
                if (registeredPeer->clientNonce == connectPayload.clientNonce &&
                    ConstantTimeCookieEquals(
                        registeredPeer->cookie,
                        connectPayload.cookie) &&
                    EndpointEquals(
                        registeredPeer->endpoint,
                        registeredPeer->endpointLength,
                        endpoint,
                        endpointLength))
                {
                    peer = registeredPeer;
                    break;
                }
            }

            if (!peer)
            {
                if (peers.size() >= kMaxPeers)
                    return;

                u64_t connectionId = 0u;
                for (u32_t attempt = 0u; attempt < 16u; ++attempt)
                {
                    u64_t candidate = 0u;
                    if (FillRandom(&candidate, sizeof(candidate)) &&
                        candidate != 0u &&
                        peers.find(candidate) == peers.end())
                    {
                        connectionId = candidate;
                        break;
                    }
                }
                if (connectionId == 0u)
                    return;

                peer = std::make_shared<Peer>();
                peer->connectionId = connectionId;
                peer->clientNonce = connectPayload.clientNonce;
                peer->cookie = connectPayload.cookie;
                peer->endpoint = endpoint;
                peer->endpointLength = endpointLength;
                peer->lastReceiveMs = NowMs();
                peers.emplace(connectionId, peer);
            }
        }
        SendAccept(peer);
    }

    void HandleHandshake(
        const sockaddr_storage& endpoint,
        int endpointLength,
        const UdpPacketView& packet)
    {
        if (packet.header.type == ePacketType::TransportClientHello)
        {
            UdpClientHelloPayload hello{};
            if (!DecodeUdpClientHello(
                std::span<const u8_t>(packet.payload, packet.payloadSize),
                hello))
            {
                metrics.invalidDatagrams.fetch_add(1u, std::memory_order_relaxed);
                return;
            }
            SendRetry(endpoint, endpointLength, hello.clientNonce);
            return;
        }
        if (packet.header.type == ePacketType::TransportClientConnect)
        {
            HandleClientConnect(endpoint, endpointLength, packet);
            return;
        }
        if (packet.header.type != ePacketType::TransportClientConfirm)
            return;

        auto peer = FindPeer(packet.header.connectionId);
        if (!peer)
        {
            metrics.unknownConnections.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        bool publishActivation = false;
        bool acknowledgeConfirm = false;
        {
            std::lock_guard peerLock(peer->mutex);
            if (peer->generation != packet.header.generation ||
                !EndpointEquals(
                    peer->endpoint,
                    peer->endpointLength,
                    endpoint,
                    endpointLength))
            {
                return;
            }
            const u64_t nowMs = NowMs();
            LaneState& controlLane =
                peer->lanes[static_cast<u8_t>(PacketLane::Control)];
            controlLane.packetReliability.OnAck(
                packet.header.ackSeq,
                packet.header.ackBitfield,
                nowMs);
            const eUdpReceiveStatus receiveStatus =
                controlLane.packetReliability.MarkReceived(
                    packet.header.packetSeq);
            if (receiveStatus != eUdpReceiveStatus::NewPacket)
            {
                metrics.duplicateDatagrams.fetch_add(1u, std::memory_order_relaxed);
            }
            peer->lastReceiveMs = nowMs;
            if (receiveStatus == eUdpReceiveStatus::NewPacket &&
                !peer->confirmed)
            {
                peer->confirmed = true;
                publishActivation = true;
            }
            else if (peer->activationPublished)
                acknowledgeConfirm = true;
        }

        if (publishActivation)
        {
            ConnectionCallback callback;
            {
                std::lock_guard callbackLock(callbackMutex);
                callback = connectionCallback;
            }
            try
            {
                if (callback)
                    callback(peer->connectionId, peer->generation, true);
            }
            catch (...)
            {
                Disconnect(peer->connectionId);
                return;
            }
            {
                std::lock_guard registryLock(peerMutex);
                const auto iterator = peers.find(peer->connectionId);
                if (iterator == peers.end() || iterator->second != peer)
                    return;
            }
            {
                std::lock_guard peerLock(peer->mutex);
                // The application callback may reject capacity and synchronously
                // disconnect this peer. Never publish activation or ACK Confirm
                // after that rejection.
                if (!peer->confirmed)
                    return;
                peer->activationPublished = true;
            }
            acknowledgeConfirm = true;
        }
        if (acknowledgeConfirm)
            SendAckOnly(peer, PacketLane::Control);
    }

    void SendAckOnly(const std::shared_ptr<Peer>& peer, PacketLane lane)
    {
        UdpPacketHeader header{};
        sockaddr_storage endpoint{};
        int endpointLength = 0;
        {
            std::lock_guard peerLock(peer->mutex);
            LaneState& laneState =
                peer->lanes[static_cast<u8_t>(lane)];
            header.connectionId = peer->connectionId;
            header.generation = peer->generation;
            header.type = ePacketType::None;
            header.lane = lane;
            header.flags = UdpPacketFlag_AckOnly;
            // ACK-only datagrams do not participate in the receive sequence
            // window. A fixed non-zero value satisfies the wire contract
            // without pushing reliable packets outside the 32-bit ACK mask.
            header.packetSeq = 1u;
            laneState.packetReliability.BuildAck(
                header.ackSeq,
                header.ackBitfield);
            endpoint = peer->endpoint;
            endpointLength = peer->endpointLength;
        }
        std::vector<u8_t> datagram;
        if (EncodeUdpPacket(header, {}, datagram))
            EnqueueOutbound(endpoint, endpointLength, std::move(datagram));
    }

    void DrainPeerDispatch(const std::shared_ptr<Peer>& peer)
    {
        for (;;)
        {
            UdpServerInboundFrame frame{};
            {
                std::lock_guard peerLock(peer->mutex);
                if (peer->dispatchQueue.empty())
                {
                    peer->dispatching = false;
                    return;
                }
                peer->dispatchBytes -=
                    peer->dispatchQueue.front().payload.size();
                frame = std::move(peer->dispatchQueue.front());
                peer->dispatchQueue.pop_front();
            }

            FrameCallback callback;
            {
                std::lock_guard callbackLock(callbackMutex);
                callback = frameCallback;
            }
            if (callback)
            {
                try
                {
                    callback(std::move(frame));
                }
                catch (...)
                {
                    Disconnect(peer->connectionId);
                    return;
                }
            }
        }
    }

    void HandleAssociatedPacket(
        const sockaddr_storage& endpoint,
        int endpointLength,
        const UdpPacketView& packet)
    {
        auto peer = FindPeer(packet.header.connectionId);
        if (!peer)
        {
            metrics.unknownConnections.fetch_add(1u, std::memory_order_relaxed);
            return;
        }

        std::vector<UdpOrderedMessage> delivered;
        bool shouldAck = false;
        bool duplicate = false;
        bool fatalOverflow = false;
        bool startDispatch = false;
        {
            std::lock_guard peerLock(peer->mutex);
            if (peer->generation != packet.header.generation ||
                !EndpointEquals(
                    peer->endpoint,
                    peer->endpointLength,
                    endpoint,
                    endpointLength))
            {
                return;
            }
            if (!peer->activationPublished)
                return;

            const u64_t nowMs = NowMs();
            peer->lastReceiveMs = nowMs;
            LaneState& lane =
                peer->lanes[static_cast<u8_t>(packet.header.lane)];
            lane.packetReliability.OnAck(
                packet.header.ackSeq,
                packet.header.ackBitfield,
                nowMs);
            const bool ackOnly =
                (packet.header.flags & UdpPacketFlag_AckOnly) != 0u;
            const PacketSemantics semantics = ackOnly
                ? PacketSemantics{}
                : ResolvePacketSemantics(packet.header.type);
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
            shouldAck = !ackOnly;
            if (duplicate || ackOnly)
            {
                // ACK state was still consumed above.
            }
            else
            {
                UdpReassembledMessage complete{};
                bool hasCompleteMessage = false;
                if ((packet.header.flags & UdpPacketFlag_Fragment) != 0u)
                {
                    const bool staleUnreliableFragment =
                        !IsReliableDelivery(semantics.delivery) &&
                        lane.lastUnreliableMessageSequence != 0u &&
                        !SeqGreater(
                            packet.header.messageSeq,
                            lane.lastUnreliableMessageSequence);
                    const eUdpReassemblyResult reassemblyResult =
                        staleUnreliableFragment
                            ? eUdpReassemblyResult::Duplicate
                            : lane.reassembly.Push(
                                packet.header,
                                std::span<const u8_t>(
                                    packet.payload,
                                    packet.payloadSize),
                                NowMs(),
                                complete);
                    if (reassemblyResult == eUdpReassemblyResult::Complete)
                    {
                        hasCompleteMessage = true;
                        metrics.reassemblyComplete.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                    }
                    else if (reassemblyResult == eUdpReassemblyResult::Overflow)
                    {
                        metrics.reassemblyOverflow.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                        fatalOverflow = IsReliableDelivery(semantics.delivery);
                    }
                    else if (reassemblyResult == eUdpReassemblyResult::Invalid)
                    {
                        metrics.invalidDatagrams.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                        fatalOverflow =
                            IsReliableDelivery(semantics.delivery);
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
                    hasCompleteMessage = true;
                }

                if (hasCompleteMessage &&
                    semantics.delivery == PacketDelivery::ReliableOrdered)
                {
                    const eUdpOrderedPushResult pushResult =
                        lane.orderedReceive.Push(
                            complete.type,
                            complete.messageSequence,
                            std::move(complete.payload),
                            delivered);
                    if (pushResult == eUdpOrderedPushResult::Buffered)
                    {
                        metrics.orderedBuffered.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                    }
                    else if (pushResult == eUdpOrderedPushResult::Overflow)
                    {
                        metrics.orderedOverflow.fetch_add(
                            1u,
                            std::memory_order_relaxed);
                        fatalOverflow = true;
                    }
                }
                else if (hasCompleteMessage &&
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

            if (!fatalOverflow)
            {
                for (auto& message : delivered)
                {
                    if (peer->dispatchQueue.size() >= kMaxDispatchFrames ||
                        message.payload.size() >
                            kMaxDispatchBytes - peer->dispatchBytes)
                    {
                        fatalOverflow = true;
                        break;
                    }
                    peer->dispatchBytes += message.payload.size();
                    peer->dispatchQueue.push_back(UdpServerInboundFrame{
                        peer->connectionId,
                        peer->generation,
                        message.type,
                        message.sequence,
                        std::move(message.payload),
                    });
                }
                if (!fatalOverflow &&
                    !peer->dispatchQueue.empty() &&
                    !peer->dispatching)
                {
                    peer->dispatching = true;
                    startDispatch = true;
                }
            }
        }

        if (shouldAck)
            SendAckOnly(peer, packet.header.lane);
        if (fatalOverflow)
        {
            Disconnect(peer->connectionId);
            return;
        }
        if (duplicate)
            return;
        if (startDispatch)
            DrainPeerDispatch(peer);
    }

    void HandleDatagram(
        const sockaddr_storage& endpoint,
        int endpointLength,
        const u8_t* bytes,
        size_t byteCount)
    {
        metrics.recvDatagrams.fetch_add(1u, std::memory_order_relaxed);
        metrics.recvBytes.fetch_add(byteCount, std::memory_order_relaxed);

        UdpPacketView packet{};
        if (!DecodeUdpPacket(bytes, byteCount, packet))
        {
            metrics.invalidDatagrams.fetch_add(1u, std::memory_order_relaxed);
            return;
        }
        if (IsTransportHandshakePacketType(packet.header.type))
        {
            HandleHandshake(endpoint, endpointLength, packet);
            return;
        }
        HandleAssociatedPacket(endpoint, endpointLength, packet);
    }

    bool PostReceive(IoContext& context)
    {
        if (!running.load(std::memory_order_acquire) || socket == INVALID_SOCKET)
            return false;

        std::memset(&context.overlapped, 0, sizeof(context.overlapped));
        context.operation = IoOperation::Receive;
        context.wsaBuffer.buf = reinterpret_cast<char*>(context.bytes.data());
        context.wsaBuffer.len = static_cast<ULONG>(context.bytes.size());
        context.endpoint = {};
        context.endpointLength = sizeof(context.endpoint);
        context.flags = 0u;

        DWORD received = 0u;
        outstandingIo.fetch_add(1u, std::memory_order_acq_rel);
        const int result = WSARecvFrom(
            socket,
            &context.wsaBuffer,
            1u,
            &received,
            &context.flags,
            reinterpret_cast<sockaddr*>(&context.endpoint),
            &context.endpointLength,
            &context.overlapped,
            nullptr);
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
        {
            outstandingIo.fetch_sub(1u, std::memory_order_acq_rel);
            return false;
        }
        return true;
    }

    void RunMaintenance()
    {
        const u64_t nowMs = NowMs();
        u64_t previous = lastMaintenanceMs.load(std::memory_order_relaxed);
        if (nowMs - previous < 25u ||
            !lastMaintenanceMs.compare_exchange_strong(
                previous,
                nowMs,
                std::memory_order_acq_rel))
        {
            return;
        }

        std::vector<std::shared_ptr<Peer>> peerSnapshot;
        {
            std::lock_guard registryLock(peerMutex);
            peerSnapshot.reserve(peers.size());
            for (const auto& [connectionId, peer] : peers)
            {
                (void)connectionId;
                peerSnapshot.push_back(peer);
            }
        }

        std::vector<u64_t> expired;
        for (const auto& peer : peerSnapshot)
        {
            std::vector<std::vector<u8_t>> retransmits;
            sockaddr_storage endpoint{};
            int endpointLength = 0;
            u64_t retryBefore = 0u;
            u64_t retryAfter = 0u;
            u64_t retransmitBefore = 0u;
            u64_t retransmitAfter = 0u;
            bool fatalReliability = false;
            {
                std::lock_guard peerLock(peer->mutex);
                // nowMs was captured before the registry lock; another IOCP
                // worker may have stamped a fresher lastReceiveMs since. The
                // unguarded u64 subtraction would underflow and expire a
                // peer that received a datagram moments ago.
                if (nowMs > peer->lastReceiveMs &&
                    nowMs - peer->lastReceiveMs >= kPeerIdleTimeoutMs)
                {
                    expired.push_back(peer->connectionId);
                    continue;
                }
                for (u8_t laneIndex = 1u;
                    laneIndex < kPacketLaneCount;
                    ++laneIndex)
                {
                    LaneState& lane = peer->lanes[laneIndex];
                    retryBefore +=
                        lane.packetReliability.GetRetryExhaustedCount();
                    retransmitBefore +=
                        lane.packetReliability.GetRetransmitCount();
                    lane.packetReliability.CollectRetransmit(
                        nowMs,
                        retransmits);
                    retryAfter +=
                        lane.packetReliability.GetRetryExhaustedCount();
                    retransmitAfter +=
                        lane.packetReliability.GetRetransmitCount();
                    const size_t reassemblyExpired =
                        lane.reassembly.Expire(nowMs);
                    metrics.reassemblyTimeout.fetch_add(
                        reassemblyExpired,
                        std::memory_order_relaxed);
                    if (reassemblyExpired != 0u &&
                        IsReliableLane(static_cast<PacketLane>(laneIndex)))
                    {
                        fatalReliability = true;
                    }
                }
                endpoint = peer->endpoint;
                endpointLength = peer->endpointLength;
            }
            metrics.retryExhausted.fetch_add(
                retryAfter - retryBefore,
                std::memory_order_relaxed);
            metrics.retransmits.fetch_add(
                retransmitAfter - retransmitBefore,
                std::memory_order_relaxed);
            if (retryAfter != retryBefore)
                fatalReliability = true;
            std::vector<OutboundDatagram> retransmitBatch;
            retransmitBatch.reserve(retransmits.size());
            for (auto& datagram : retransmits)
            {
                retransmitBatch.push_back({
                    endpoint,
                    endpointLength,
                    std::move(datagram),
                });
            }
            if (!retransmitBatch.empty())
                EnqueueOutboundBatch(std::move(retransmitBatch));
            if (fatalReliability)
                expired.push_back(peer->connectionId);
        }

        for (u64_t connectionId : expired)
            Disconnect(connectionId);
    }

    void WorkerLoop()
    {
        for (;;)
        {
            DWORD bytesTransferred = 0u;
            ULONG_PTR completionKey = 0u;
            OVERLAPPED* overlapped = nullptr;
            const BOOL succeeded = GetQueuedCompletionStatus(
                completionPort,
                &bytesTransferred,
                &completionKey,
                &overlapped,
                25u);
            (void)completionKey;

            if (overlapped)
            {
                auto* context = reinterpret_cast<IoContext*>(overlapped);
                outstandingIo.fetch_sub(1u, std::memory_order_acq_rel);
                if (context->operation == IoOperation::Receive)
                {
                    if (succeeded && bytesTransferred > 0u)
                    {
                        HandleDatagram(
                            context->endpoint,
                            context->endpointLength,
                            context->bytes.data(),
                            bytesTransferred);
                    }
                    if (running.load(std::memory_order_acquire))
                        PostReceive(*context);
                }
                else
                {
                    delete context;
                }
            }

            if (running.load(std::memory_order_acquire))
            {
                DrainOutbound(64u);
                RunMaintenance();
            }
            else if (outstandingIo.load(std::memory_order_acquire) == 0u)
                break;
        }
    }

    bool Start(u16_t port, u32_t requestedWorkers, u32_t receiveDepth)
    {
        if (running.load(std::memory_order_acquire) ||
            requestedWorkers == 0u ||
            requestedWorkers > kMaxWorkerCount ||
            receiveDepth == 0u ||
            receiveDepth > kMaxReceiveDepth ||
            !FillRandom(cookieSecret.data(), cookieSecret.size()))
        {
            return false;
        }

        socket = WSASocketW(
            AF_INET,
            SOCK_DGRAM,
            IPPROTO_UDP,
            nullptr,
            0u,
            WSA_FLAG_OVERLAPPED);
        if (socket == INVALID_SOCKET)
            return false;

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

        sockaddr_in bindAddress{};
        bindAddress.sin_family = AF_INET;
        bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
        bindAddress.sin_port = htons(port);
        if (bind(
            socket,
            reinterpret_cast<const sockaddr*>(&bindAddress),
            sizeof(bindAddress)) == SOCKET_ERROR)
        {
            closesocket(socket);
            socket = INVALID_SOCKET;
            return false;
        }

        int addressLength = sizeof(bindAddress);
        if (getsockname(
            socket,
            reinterpret_cast<sockaddr*>(&bindAddress),
            &addressLength) == SOCKET_ERROR)
        {
            closesocket(socket);
            socket = INVALID_SOCKET;
            return false;
        }
        boundPort = ntohs(bindAddress.sin_port);

        completionPort = CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(socket),
            nullptr,
            0u,
            requestedWorkers);
        if (!completionPort)
        {
            closesocket(socket);
            socket = INVALID_SOCKET;
            return false;
        }

        workerCount = requestedWorkers;
        running.store(true, std::memory_order_release);
        workers.reserve(workerCount);
        for (u32_t index = 0u; index < workerCount; ++index)
            workers.emplace_back(&Impl::WorkerLoop, this);

        receiveContexts.reserve(receiveDepth);
        for (u32_t index = 0u; index < receiveDepth; ++index)
        {
            auto context = std::make_unique<IoContext>();
            if (!PostReceive(*context))
            {
                Shutdown();
                return false;
            }
            receiveContexts.push_back(std::move(context));
        }
        return true;
    }

    void Shutdown()
    {
        if (!running.exchange(false, std::memory_order_acq_rel))
            return;

        const SOCKET closingSocket = socket;
        if (closingSocket != INVALID_SOCKET)
        {
            closesocket(closingSocket);
        }
        for (u32_t index = 0u; index < workerCount; ++index)
            PostQueuedCompletionStatus(completionPort, 0u, 0u, nullptr);
        for (auto& worker : workers)
        {
            if (worker.joinable())
                worker.join();
        }
        workers.clear();
        socket = INVALID_SOCKET;
        receiveContexts.clear();
        {
            std::lock_guard outboundLock(outboundMutex);
            metrics.outboundQueueDrops.fetch_add(
                outboundQueue.size(),
                std::memory_order_relaxed);
            outboundQueue.clear();
            outboundQueueBytes = 0u;
        }

        if (completionPort)
        {
            CloseHandle(completionPort);
            completionPort = nullptr;
        }

        std::vector<std::pair<u64_t, u32_t>> disconnected;
        {
            std::lock_guard registryLock(peerMutex);
            disconnected.reserve(peers.size());
            for (const auto& [connectionId, peer] : peers)
            {
                std::lock_guard peerLock(peer->mutex);
                if (peer->confirmed)
                    disconnected.emplace_back(connectionId, peer->generation);
            }
            peers.clear();
        }
        ConnectionCallback callback;
        {
            std::lock_guard callbackLock(callbackMutex);
            callback = connectionCallback;
        }
        if (callback)
        {
            for (const auto& [connectionId, generation] : disconnected)
            {
                try
                {
                    callback(connectionId, generation, false);
                }
                catch (...)
                {
                    // Shutdown is already committed. One application callback
                    // must not prevent the remaining peers from being released.
                }
            }
        }
        boundPort = 0u;
        workerCount = 0u;
        outstandingIo.store(0u, std::memory_order_release);
    }

    eUdpSendResult Send(
        u64_t connectionId,
        ePacketType type,
        const u8_t* payload,
        u32_t payloadSize)
    {
        if (!running.load(std::memory_order_acquire))
            return eUdpSendResult::NotRunning;
        if (!IsKnownPacketType(type) ||
            IsTransportHandshakePacketType(type) ||
            (payloadSize != 0u && !payload) ||
            payloadSize > kUdpMaxMessageBytes)
        {
            return eUdpSendResult::InvalidPacket;
        }

        auto peer = FindPeer(connectionId);
        if (!peer)
            return eUdpSendResult::NoAssociation;

        std::vector<std::pair<u32_t, std::vector<u8_t>>> encoded;
        const PacketSemantics semantics = ResolvePacketSemantics(type);
        {
            std::lock_guard peerLock(peer->mutex);
            if (!peer->confirmed)
                return eUdpSendResult::NotConfirmed;
            LaneState& lane =
                peer->lanes[static_cast<u8_t>(semantics.lane)];
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
                return eUdpSendResult::ReliableQueueFull;
            }
            const u32_t messageSequence =
                lane.nextSendMessageSequence == 0u
                    ? 1u
                    : lane.nextSendMessageSequence;
            UdpPacketHeader baseHeader{};
            baseHeader.connectionId = peer->connectionId;
            baseHeader.generation = peer->generation;
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
                    return eUdpSendResult::InvalidPacket;
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
                    EncodeUdpFragmentHeader(
                        fragment,
                        fragmentPayload.data());
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
                        return eUdpSendResult::InvalidPacket;
                    }
                    encoded.emplace_back(
                        fragmentHeader.packetSeq,
                        std::move(datagram));
                }
            }

            size_t outboundBytes = 0u;
            for (const auto& packet : encoded)
                outboundBytes += packet.second.size();
            {
                std::lock_guard outboundLock(outboundMutex);
                if (encoded.size() >
                        kMaxOutboundDatagrams - outboundQueue.size() ||
                    outboundBytes >
                        kMaxOutboundBytes - outboundQueueBytes)
                {
                    metrics.outboundQueueDrops.fetch_add(
                        encoded.size(),
                        std::memory_order_relaxed);
                    return eUdpSendResult::OutboundQueueFull;
                }

                if (IsReliableDelivery(semantics.delivery))
                {
                    const u64_t nowMs = NowMs();
                    for (const auto& packet : encoded)
                    {
                        const bool queued =
                            lane.packetReliability.QueueReliable(
                                packet.first,
                                packet.second,
                                nowMs);
                        if (!queued)
                        {
                            // Preflight above makes this unreachable while
                            // peerMutex is held. Treat it as association-fatal
                            // rather than publishing a partial reliable message.
                            return eUdpSendResult::ReliableQueueFull;
                        }
                    }
                }

                outboundQueueBytes += outboundBytes;
                for (auto& packet : encoded)
                {
                    outboundQueue.push_back({
                        peer->endpoint,
                        peer->endpointLength,
                        std::move(packet.second),
                    });
                }
                const u32_t committedSequence = NextNonZero(
                    lane.nextSendMessageSequence);
                if (committedSequence != messageSequence)
                    return eUdpSendResult::SocketError;
            }
        }
        PostQueuedCompletionStatus(completionPort, 0u, 1u, nullptr);
        return eUdpSendResult::Queued;
    }

    bool Disconnect(u64_t connectionId)
    {
        std::shared_ptr<Peer> peer;
        {
            std::lock_guard registryLock(peerMutex);
            const auto iterator = peers.find(connectionId);
            if (iterator == peers.end())
                return false;
            peer = iterator->second;
            peers.erase(iterator);
        }

        bool wasConfirmed = false;
        {
            std::lock_guard peerLock(peer->mutex);
            wasConfirmed = peer->confirmed;
            peer->confirmed = false;
            peer->activationPublished = false;
        }

        ConnectionCallback callback;
        {
            std::lock_guard callbackLock(callbackMutex);
            callback = connectionCallback;
        }
        if (callback && wasConfirmed)
        {
            try
            {
                callback(peer->connectionId, peer->generation, false);
            }
            catch (...)
            {
                // Disconnect is already committed; callback failures do not
                // resurrect the association or escape an IOCP worker.
            }
        }
        return true;
    }

    UdpServerMetrics SnapshotMetrics() const
    {
        UdpServerMetrics result{};
        result.recvDatagrams = metrics.recvDatagrams.load(std::memory_order_relaxed);
        result.recvBytes = metrics.recvBytes.load(std::memory_order_relaxed);
        result.sendDatagrams = metrics.sendDatagrams.load(std::memory_order_relaxed);
        result.sendBytes = metrics.sendBytes.load(std::memory_order_relaxed);
        result.invalidDatagrams = metrics.invalidDatagrams.load(std::memory_order_relaxed);
        result.invalidCookies = metrics.invalidCookies.load(std::memory_order_relaxed);
        result.rejectedTickets = metrics.rejectedTickets.load(std::memory_order_relaxed);
        result.unknownConnections = metrics.unknownConnections.load(std::memory_order_relaxed);
        result.duplicateDatagrams = metrics.duplicateDatagrams.load(std::memory_order_relaxed);
        result.orderedBuffered = metrics.orderedBuffered.load(std::memory_order_relaxed);
        result.orderedOverflow = metrics.orderedOverflow.load(std::memory_order_relaxed);
        result.reassemblyComplete = metrics.reassemblyComplete.load(std::memory_order_relaxed);
        result.reassemblyOverflow = metrics.reassemblyOverflow.load(std::memory_order_relaxed);
        result.reassemblyTimeout = metrics.reassemblyTimeout.load(std::memory_order_relaxed);
        result.retransmits = metrics.retransmits.load(std::memory_order_relaxed);
        result.retryExhausted = metrics.retryExhausted.load(std::memory_order_relaxed);
        result.outboundQueueDrops = metrics.outboundQueueDrops.load(std::memory_order_relaxed);
        result.outstandingIo = outstandingIo.load(std::memory_order_relaxed);
        {
            std::lock_guard outboundLock(outboundMutex);
            result.outboundQueueBytes = outboundQueueBytes;
        }
        std::lock_guard registryLock(peerMutex);
        result.connectedPeers = static_cast<u64_t>(std::count_if(
            peers.begin(),
            peers.end(),
            [](const auto& entry)
            {
                std::lock_guard peerLock(entry.second->mutex);
                return entry.second->confirmed;
            }));
        return result;
    }
};

CUdpIocpCore::CUdpIocpCore()
    : m_impl(std::make_unique<Impl>())
{
}

CUdpIocpCore::~CUdpIocpCore() = default;

void CUdpIocpCore::SetTicketValidator(TicketValidator validator)
{
    std::lock_guard callbackLock(m_impl->callbackMutex);
    m_impl->ticketValidator = std::move(validator);
}

void CUdpIocpCore::SetConnectionCallback(ConnectionCallback callback)
{
    std::lock_guard callbackLock(m_impl->callbackMutex);
    m_impl->connectionCallback = std::move(callback);
}

void CUdpIocpCore::SetFrameCallback(FrameCallback callback)
{
    std::lock_guard callbackLock(m_impl->callbackMutex);
    m_impl->frameCallback = std::move(callback);
}

bool CUdpIocpCore::Start(u16_t port, u32_t workerCount, u32_t receiveDepth)
{
    return m_impl->Start(port, workerCount, receiveDepth);
}

void CUdpIocpCore::Shutdown()
{
    m_impl->Shutdown();
}

eUdpSendResult CUdpIocpCore::SendToConnection(
    u64_t connectionId,
    ePacketType type,
    const u8_t* payload,
    u32_t payloadSize)
{
    return m_impl->Send(connectionId, type, payload, payloadSize);
}

bool CUdpIocpCore::DisconnectConnection(u64_t connectionId)
{
    return m_impl->Disconnect(connectionId);
}

bool CUdpIocpCore::IsRunning() const
{
    return m_impl->running.load(std::memory_order_acquire);
}

u16_t CUdpIocpCore::GetBoundPort() const
{
    return m_impl->boundPort;
}

UdpServerMetrics CUdpIocpCore::GetMetrics() const
{
    return m_impl->SnapshotMetrics();
}
