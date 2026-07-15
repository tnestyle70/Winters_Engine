#include "Network/UdpIocpCore.h"
#include "UdpClient.h"

#include "Shared/Network/UdpFragmentCodec.h"
#include "Shared/Network/UdpOrderedReceiveQueue.h"
#include "Shared/Network/UdpPacketCodec.h"
#include "Shared/Network/UdpReassemblyBuffer.h"
#include "Shared/Network/UdpReliabilityChannel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    constexpr u32_t kMeasuredSnapshotBytes = 22'104u;

    std::vector<u8_t> MakePattern(size_t size, u8_t salt)
    {
        std::vector<u8_t> bytes(size);
        for (size_t index = 0u; index < bytes.size(); ++index)
        {
            bytes[index] = static_cast<u8_t>(
                (index * 131u + salt * 17u) & 0xFFu);
        }
        return bytes;
    }

    bool TestCodec()
    {
        UdpPacketHeader header{};
        header.connectionId = 0x0102030405060708ull;
        header.generation = 0x11223344u;
        header.type = ePacketType::LobbyCommand;
        header.lane = PacketLane::Lobby;
        header.packetSeq = 0x55667788u;
        header.messageSeq = 0x10203040u;
        header.ackSeq = 0x90A0B0C0u;
        header.ackBitfield = 0xDEADBEEFu;
        const std::vector<u8_t> payload{ 1u, 2u, 3u, 4u };
        std::vector<u8_t> datagram;
        if (!EncodeUdpPacket(header, payload, datagram) ||
            datagram.size() != kUdpPacketHeaderBytes + payload.size() ||
            datagram[4] != 0x01u || datagram[11] != 0x08u ||
            datagram[12] != 0x11u || datagram[15] != 0x44u)
        {
            return false;
        }

        UdpPacketView decoded{};
        if (!DecodeUdpPacket(datagram.data(), datagram.size(), decoded) ||
            decoded.header.connectionId != header.connectionId ||
            decoded.header.generation != header.generation ||
            decoded.header.packetSeq != header.packetSeq ||
            decoded.header.messageSeq != header.messageSeq ||
            decoded.payloadSize != payload.size() ||
            !std::equal(
                payload.begin(),
                payload.end(),
                decoded.payload))
        {
            return false;
        }

        datagram[36] = 0u;
        datagram[37] = 5u;
        return !DecodeUdpPacket(datagram.data(), datagram.size(), decoded);
    }

    bool TestOrderedGapBuffer()
    {
        CUdpOrderedReceiveQueue queue(4u, 64u);
        std::vector<UdpOrderedMessage> delivered;
        if (queue.Push(
            ePacketType::Event,
            2u,
            { 2u },
            delivered) != eUdpOrderedPushResult::Buffered ||
            !delivered.empty())
        {
            return false;
        }
        if (queue.Push(
            ePacketType::Event,
            1u,
            { 1u },
            delivered) != eUdpOrderedPushResult::Delivered ||
            delivered.size() != 2u ||
            delivered[0].sequence != 1u ||
            delivered[1].sequence != 2u)
        {
            return false;
        }
        return queue.GetNextExpectedSequence() == 3u &&
            queue.GetPendingCount() == 0u;
    }

    bool TestLaneAckIsolation()
    {
        CUdpReliabilityChannel control;
        CUdpReliabilityChannel snapshot;
        const u32_t controlSequence = control.NextSendSeq();
        if (!control.QueueReliable(
            controlSequence,
            std::vector<u8_t>{ 0xC0u },
            0u))
        {
            return false;
        }
        for (u32_t sequence = 1u; sequence <= 100u; ++sequence)
            snapshot.MarkReceived(sequence);

        std::vector<std::vector<u8_t>> retransmits;
        control.CollectRetransmit(121u, retransmits);
        if (retransmits.size() != 1u ||
            retransmits[0] != std::vector<u8_t>{ 0xC0u })
        {
            return false;
        }

        const size_t before = control.GetPendingPacketCount();
        if (!control.CanQueueReliable(31u, 31u) ||
            control.CanQueueReliable(32u, 32u) ||
            control.GetPendingPacketCount() != before)
        {
            return false;
        }
        return true;
    }

    bool TestAdaptiveRto()
    {
        CUdpReliabilityChannel channel;
        if (channel.GetCurrentRtoMs() != kUdpInitialRtoMs ||
            channel.GetRttSampleCount() != 0u)
        {
            return false;
        }

        // First 40 ms sample: SRTT=40, RTTVAR=20, RTO=clamp(40+80)=120.
        const u32_t first = channel.NextSendSeq();
        if (!channel.QueueReliable(first, { 0x01u }, 1'000u))
            return false;
        channel.OnAck(first, 0u, 1'040u);
        if (channel.GetRttSampleCount() != 1u ||
            channel.GetSmoothedRttMs() != 40u ||
            channel.GetCurrentRtoMs() != 120u)
        {
            return false;
        }

        // Steady 40 ms samples decay RTTVAR to zero; RTO settles on the
        // 60 ms floor instead of the 120 ms initial value.
        for (u32_t index = 0u; index < 32u; ++index)
        {
            const u64_t sendMs = 2'000u + index * 100u;
            const u32_t sequence = channel.NextSendSeq();
            if (!channel.QueueReliable(sequence, { 0x02u }, sendMs))
                return false;
            channel.OnAck(sequence, 0u, sendMs + 40u);
        }
        if (channel.GetSmoothedRttMs() != 40u ||
            channel.GetCurrentRtoMs() != kUdpMinRtoMs)
        {
            return false;
        }

        // Retransmit timing must follow the adapted RTO.
        const u32_t probe = channel.NextSendSeq();
        if (!channel.QueueReliable(probe, { 0x03u }, 10'000u))
            return false;
        std::vector<std::vector<u8_t>> retransmits;
        channel.CollectRetransmit(
            10'000u + channel.GetCurrentRtoMs() - 1u,
            retransmits);
        if (!retransmits.empty())
            return false;
        channel.CollectRetransmit(
            10'000u + channel.GetCurrentRtoMs(),
            retransmits);
        if (retransmits.size() != 1u)
            return false;

        // Karn part two: a timeout keeps a doubled RTO, the ambiguous ack
        // takes no sample, and the next clean sample restores the
        // formula-driven RTO.
        if (channel.GetCurrentRtoMs() != 2u * kUdpMinRtoMs)
            return false;
        channel.OnAck(probe, 0u, 11'000u);
        if (channel.GetRttSampleCount() != 33u)
            return false;
        const u32_t recovery = channel.NextSendSeq();
        if (!channel.QueueReliable(recovery, { 0x05u }, 20'000u))
            return false;
        channel.OnAck(recovery, 0u, 20'040u);
        return channel.GetRttSampleCount() == 34u &&
            channel.GetCurrentRtoMs() == kUdpMinRtoMs;
    }

    bool TestKarnRetransmitExclusion()
    {
        CUdpReliabilityChannel channel;
        const u32_t sequence = channel.NextSendSeq();
        if (!channel.QueueReliable(sequence, { 0x04u }, 0u))
            return false;

        // Ack after a retransmit: ambiguous sample must be discarded while
        // the timeout leaves a doubled (backed-off) RTO in place.
        std::vector<std::vector<u8_t>> retransmits;
        channel.CollectRetransmit(kUdpInitialRtoMs, retransmits);
        if (retransmits.size() != 1u)
            return false;
        channel.OnAck(sequence, 0u, 5'000u);
        return channel.GetRttSampleCount() == 0u &&
            channel.GetCurrentRtoMs() == 2u * kUdpInitialRtoMs &&
            channel.GetPendingPacketCount() == 0u;
    }

    // Deterministic loss/reorder/duplication pipe (seeded LCG, no wall clock).
    class CChaosPipe
    {
    public:
        explicit CChaosPipe(u64_t seed) : m_state(seed) {}

        void Send(u64_t nowMs, const std::vector<u8_t>& bytes)
        {
            if (NextPercent() < 25u)
                return;
            const u32_t copies = NextPercent() < 10u ? 2u : 1u;
            for (u32_t copy = 0u; copy < copies; ++copy)
            {
                m_inFlight.push_back({
                    nowMs + 10u + NextPercent() % 40u,
                    bytes,
                });
            }
        }

        void Deliver(u64_t nowMs, std::vector<std::vector<u8_t>>& outBytes)
        {
            auto iterator = m_inFlight.begin();
            while (iterator != m_inFlight.end())
            {
                if (iterator->deliverAtMs > nowMs)
                {
                    ++iterator;
                    continue;
                }
                outBytes.push_back(std::move(iterator->bytes));
                iterator = m_inFlight.erase(iterator);
            }
        }

    private:
        struct InFlightPacket
        {
            u64_t deliverAtMs = 0;
            std::vector<u8_t> bytes;
        };

        u32_t NextPercent()
        {
            m_state = m_state * 6364136223846793005ull +
                1442695040888963407ull;
            return static_cast<u32_t>((m_state >> 33u) % 100u);
        }

        u64_t m_state;
        std::vector<InFlightPacket> m_inFlight;
    };

    bool TestReliableChaosPipe()
    {
        constexpr u32_t kMessageCount = 200u;
        CUdpReliabilityChannel sender;
        CUdpReliabilityChannel receiver;
        CUdpOrderedReceiveQueue ordered(64u, 64u * 1024u);
        CChaosPipe dataPipe(0x57494E5445525331ull);
        CChaosPipe ackPipe(0x4348414F53000001ull);

        u32_t nextMessage = 1u;
        u32_t expectedDelivery = 1u;
        bool orderViolation = false;

        for (u64_t nowMs = 0u; nowMs <= 120'000u; nowMs += 10u)
        {
            while (nextMessage <= kMessageCount &&
                sender.CanQueueReliable(1u, 9u))
            {
                const u32_t packetSeq = sender.NextSendSeq();
                std::vector<u8_t> bytes(9u);
                UdpWire::WriteU32(bytes.data() + 0u, packetSeq);
                UdpWire::WriteU32(bytes.data() + 4u, nextMessage);
                bytes[8] = static_cast<u8_t>(nextMessage & 0xFFu);
                if (!sender.QueueReliable(packetSeq, bytes, nowMs))
                    return false;
                dataPipe.Send(nowMs, bytes);
                ++nextMessage;
            }

            std::vector<std::vector<u8_t>> retransmits;
            sender.CollectRetransmit(nowMs, retransmits);
            for (const auto& packet : retransmits)
                dataPipe.Send(nowMs, packet);

            std::vector<std::vector<u8_t>> arrived;
            dataPipe.Deliver(nowMs, arrived);
            bool receivedAny = false;
            for (const auto& bytes : arrived)
            {
                if (bytes.size() != 9u)
                    return false;
                receivedAny = true;
                const u32_t packetSeq = UdpWire::ReadU32(bytes.data() + 0u);
                const u32_t messageSeq = UdpWire::ReadU32(bytes.data() + 4u);
                if (receiver.MarkReceived(packetSeq) !=
                    eUdpReceiveStatus::NewPacket)
                {
                    continue;
                }
                std::vector<UdpOrderedMessage> delivered;
                if (ordered.Push(
                    ePacketType::Event,
                    messageSeq,
                    { bytes[8] },
                    delivered) == eUdpOrderedPushResult::Overflow)
                {
                    return false;
                }
                for (const auto& message : delivered)
                {
                    if (message.sequence != expectedDelivery ||
                        message.payload.size() != 1u ||
                        message.payload[0] != static_cast<u8_t>(
                            expectedDelivery & 0xFFu))
                    {
                        orderViolation = true;
                    }
                    ++expectedDelivery;
                }
            }

            if (receivedAny)
            {
                u32_t ackSeq = 0u;
                u32_t ackBitfield = 0u;
                receiver.BuildAck(ackSeq, ackBitfield);
                std::vector<u8_t> ack(8u);
                UdpWire::WriteU32(ack.data() + 0u, ackSeq);
                UdpWire::WriteU32(ack.data() + 4u, ackBitfield);
                ackPipe.Send(nowMs, ack);
            }

            std::vector<std::vector<u8_t>> acks;
            ackPipe.Deliver(nowMs, acks);
            for (const auto& bytes : acks)
            {
                if (bytes.size() != 8u)
                    return false;
                sender.OnAck(
                    UdpWire::ReadU32(bytes.data() + 0u),
                    UdpWire::ReadU32(bytes.data() + 4u),
                    nowMs);
            }

            if (expectedDelivery > kMessageCount &&
                nextMessage > kMessageCount &&
                sender.GetPendingPacketCount() == 0u)
            {
                break;
            }
        }

        return !orderViolation &&
            expectedDelivery == kMessageCount + 1u &&
            sender.GetPendingPacketCount() == 0u &&
            sender.GetRetryExhaustedCount() == 0u &&
            sender.GetRetransmitCount() > 0u &&
            sender.GetRttSampleCount() > 0u;
    }

    std::vector<std::vector<u8_t>> BuildFragmentDatagrams(
        const std::vector<u8_t>& payload,
        u32_t messageSequence)
    {
        const u16_t fragmentCount = static_cast<u16_t>(
            (payload.size() + kUdpMaxFragmentDataBytes - 1u) /
            kUdpMaxFragmentDataBytes);
        std::vector<std::vector<u8_t>> datagrams;
        datagrams.reserve(fragmentCount);
        for (u16_t index = 0u; index < fragmentCount; ++index)
        {
            const u32_t offset = static_cast<u32_t>(index) *
                kUdpMaxFragmentDataBytes;
            const u16_t bytes = static_cast<u16_t>(std::min<size_t>(
                kUdpMaxFragmentDataBytes,
                payload.size() - offset));
            UdpFragmentHeader fragment{};
            fragment.messageId = messageSequence;
            fragment.messageBytes = static_cast<u32_t>(payload.size());
            fragment.fragmentIndex = index;
            fragment.fragmentCount = fragmentCount;
            fragment.fragmentPayloadSize = bytes;
            std::vector<u8_t> fragmentPayload(
                kUdpFragmentHeaderBytes + bytes);
            EncodeUdpFragmentHeader(fragment, fragmentPayload.data());
            std::copy_n(
                payload.data() + offset,
                bytes,
                fragmentPayload.data() + kUdpFragmentHeaderBytes);

            UdpPacketHeader header{};
            header.connectionId = 7u;
            header.generation = 1u;
            header.type = ePacketType::Snapshot;
            header.lane = PacketLane::Snapshot;
            header.flags = UdpPacketFlag_Fragment;
            header.packetSeq = static_cast<u32_t>(index) + 1u;
            header.messageSeq = messageSequence;
            std::vector<u8_t> datagram;
            if (!EncodeUdpPacket(header, fragmentPayload, datagram))
                return {};
            datagrams.push_back(std::move(datagram));
        }
        return datagrams;
    }

    bool TestMeasuredPayloadReassembly()
    {
        const std::vector<u8_t> payload =
            MakePattern(kMeasuredSnapshotBytes, 0x31u);
        auto datagrams = BuildFragmentDatagrams(payload, 77u);
        if (datagrams.size() != 20u ||
            std::any_of(
                datagrams.begin(),
                datagrams.end(),
                [](const auto& datagram)
                {
                    return datagram.size() > kUdpMaxDatagramBytes;
                }))
        {
            return false;
        }

        CUdpReassemblyBuffer reassembly;
        UdpReassembledMessage complete{};
        UdpPacketView duplicateView{};
        if (!DecodeUdpPacket(
            datagrams.back().data(),
            datagrams.back().size(),
            duplicateView) ||
            reassembly.Push(
                duplicateView.header,
                std::span<const u8_t>(
                    duplicateView.payload,
                    duplicateView.payloadSize),
                0u,
                complete) != eUdpReassemblyResult::Accepted ||
            reassembly.Push(
                duplicateView.header,
                std::span<const u8_t>(
                    duplicateView.payload,
                    duplicateView.payloadSize),
                1u,
                complete) != eUdpReassemblyResult::Duplicate)
        {
            return false;
        }

        for (size_t reverse = datagrams.size() - 1u; reverse-- > 0u;)
        {
            UdpPacketView packet{};
            if (!DecodeUdpPacket(
                datagrams[reverse].data(),
                datagrams[reverse].size(),
                packet))
            {
                return false;
            }
            const eUdpReassemblyResult result = reassembly.Push(
                packet.header,
                std::span<const u8_t>(packet.payload, packet.payloadSize),
                static_cast<u64_t>(datagrams.size() - reverse),
                complete);
            if (reverse != 0u && result != eUdpReassemblyResult::Accepted)
                return false;
            if (reverse == 0u && result != eUdpReassemblyResult::Complete)
                return false;
        }
        if (complete.payload != payload || reassembly.GetPendingCount() != 0u)
            return false;

        CUdpReassemblyBuffer missing(2u, 64u * 1024u, 10u);
        UdpPacketView first{};
        if (!DecodeUdpPacket(
            datagrams.front().data(),
            datagrams.front().size(),
            first) ||
            missing.Push(
                first.header,
                std::span<const u8_t>(first.payload, first.payloadSize),
                0u,
                complete) != eUdpReassemblyResult::Accepted ||
            missing.Expire(11u) != 1u ||
            missing.GetPendingBytes() != 0u)
        {
            return false;
        }

        auto newerDatagrams = BuildFragmentDatagrams(payload, 78u);
        UdpPacketView newerFirst{};
        CUdpReassemblyBuffer latestWins;
        if (newerDatagrams.size() != datagrams.size() ||
            !DecodeUdpPacket(
                datagrams.front().data(),
                datagrams.front().size(),
                first) ||
            !DecodeUdpPacket(
                newerDatagrams.front().data(),
                newerDatagrams.front().size(),
                newerFirst) ||
            latestWins.Push(
                first.header,
                std::span<const u8_t>(first.payload, first.payloadSize),
                0u,
                complete) != eUdpReassemblyResult::Accepted ||
            latestWins.Push(
                newerFirst.header,
                std::span<const u8_t>(
                    newerFirst.payload,
                    newerFirst.payloadSize),
                1u,
                complete) != eUdpReassemblyResult::Accepted ||
            latestWins.GetPendingCount() != 1u ||
            latestWins.Push(
                first.header,
                std::span<const u8_t>(first.payload, first.payloadSize),
                2u,
                complete) != eUdpReassemblyResult::Duplicate)
        {
            return false;
        }
        return true;
    }

    template <typename Predicate>
    bool PumpUntil(
        CUdpClient& client,
        Predicate&& predicate,
        std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            client.PumpReceivedFrames();
            if (predicate())
                return true;
            std::this_thread::sleep_for(5ms);
        }
        client.PumpReceivedFrames();
        return predicate();
    }

    bool TestLiveLoopback()
    {
        auto client = CUdpClient::Create();
        if (!client)
            return false;

        CUdpIocpCore server;
        const std::vector<u8_t> ticket{
            'u', 'd', 'p', '-', 'l', 'o', 'o', 'p', 'b', 'a', 'c', 'k'
        };
        server.SetTicketValidator(
            [&ticket](std::span<const u8_t> candidate)
            {
                return candidate.size() == ticket.size() &&
                    std::equal(
                        candidate.begin(),
                        candidate.end(),
                        ticket.begin());
            });

        std::atomic<u64_t> connectedId{ 0u };
        std::atomic<u32_t> connectedCallbacks{ 0u };
        std::atomic<u32_t> serverLargeFrames{ 0u };
        const std::vector<u8_t> largeClientPayload =
            MakePattern(kMeasuredSnapshotBytes, 0x52u);
        const std::vector<u8_t> largeServerPayload =
            MakePattern(kMeasuredSnapshotBytes, 0x73u);

        server.SetConnectionCallback(
            [&server, &connectedId, &connectedCallbacks](
                u64_t connectionId,
                u32_t,
                bool connected)
            {
                if (!connected)
                    return;
                connectedId.store(connectionId, std::memory_order_release);
                connectedCallbacks.fetch_add(1u, std::memory_order_relaxed);
                for (u8_t ordinal = 0u; ordinal < 4u; ++ordinal)
                {
                    const u8_t payload[] = { ordinal };
                    server.SendToConnection(
                        connectionId,
                        ePacketType::Hello,
                        payload,
                        static_cast<u32_t>(sizeof(payload)));
                }
            });

        server.SetFrameCallback(
            [&server, &largeClientPayload, &serverLargeFrames](
                UdpServerInboundFrame frame)
            {
                if (frame.type == ePacketType::LobbyCommand)
                {
                    server.SendToConnection(
                        frame.connectionId,
                        ePacketType::LobbyState,
                        frame.payload.data(),
                        static_cast<u32_t>(frame.payload.size()));
                }
                else if (frame.type == ePacketType::Event &&
                    frame.payload == largeClientPayload)
                {
                    serverLargeFrames.fetch_add(1u, std::memory_order_relaxed);
                }
            });

        if (!server.Start(0u, 2u, 8u))
        {
            std::printf("[UdpLoopbackHarness] live fail: server start\n");
            return false;
        }

        std::mutex clientFrameMutex;
        std::vector<u8_t> helloOrdinals;
        std::vector<u8_t> lobbyEcho;
        std::vector<u8_t> snapshotPayload;
        client->SetFrameCallback(
            [&clientFrameMutex,
                &helloOrdinals,
                &lobbyEcho,
                &snapshotPayload](
                    ePacketType type,
                    u32_t,
                    const u8_t* payload,
                    u32_t payloadBytes)
            {
                std::lock_guard frameLock(clientFrameMutex);
                if (type == ePacketType::Hello && payloadBytes == 1u)
                    helloOrdinals.push_back(payload[0]);
                else if (type == ePacketType::LobbyState)
                    lobbyEcho.assign(payload, payload + payloadBytes);
                else if (type == ePacketType::Snapshot)
                    snapshotPayload.assign(payload, payload + payloadBytes);
            });

        if (!client->Connect(
            "127.0.0.1",
            server.GetBoundPort(),
            ticket,
            3'000u))
        {
            std::printf("[UdpLoopbackHarness] live fail: connect\n");
            server.Shutdown();
            return false;
        }

        if (!PumpUntil(
            *client,
            [&helloOrdinals, &clientFrameMutex]()
            {
                std::lock_guard frameLock(clientFrameMutex);
                return helloOrdinals.size() == 4u;
            },
            2s))
        {
            std::printf("[UdpLoopbackHarness] live fail: hello burst\n");
            client->Disconnect();
            server.Shutdown();
            return false;
        }
        {
            std::lock_guard frameLock(clientFrameMutex);
            if (helloOrdinals != std::vector<u8_t>{ 0u, 1u, 2u, 3u })
            {
                std::printf("[UdpLoopbackHarness] live fail: hello order count=%zu\n", helloOrdinals.size());
                return false;
            }
        }

        const std::vector<u8_t> lobbyCommand{ 9u, 8u, 7u, 6u };
        if (!client->Send(
            ePacketType::LobbyCommand,
            lobbyCommand.data(),
            static_cast<u32_t>(lobbyCommand.size())) ||
            !PumpUntil(
                *client,
                [&lobbyEcho, &lobbyCommand, &clientFrameMutex]()
                {
                    std::lock_guard frameLock(clientFrameMutex);
                    return lobbyEcho == lobbyCommand;
                },
                2s))
        {
            std::printf("[UdpLoopbackHarness] live fail: lobby echo\n");
            client->Disconnect();
            server.Shutdown();
            return false;
        }

        const u64_t connectionId = connectedId.load(std::memory_order_acquire);
        if (connectionId == 0u ||
            server.SendToConnection(
                connectionId,
                ePacketType::Snapshot,
                largeServerPayload.data(),
                static_cast<u32_t>(largeServerPayload.size())) !=
                eUdpSendResult::Queued ||
            !PumpUntil(
                *client,
                [&snapshotPayload, &largeServerPayload, &clientFrameMutex]()
                {
                    std::lock_guard frameLock(clientFrameMutex);
                    return snapshotPayload == largeServerPayload;
                },
                3s))
        {
            std::printf("[UdpLoopbackHarness] live fail: snapshot 22104\n");
            client->Disconnect();
            server.Shutdown();
            return false;
        }

        if (!client->Send(
            ePacketType::Event,
            largeClientPayload.data(),
            static_cast<u32_t>(largeClientPayload.size())))
        {
            std::printf("[UdpLoopbackHarness] live fail: client event enqueue\n");
            client->Disconnect();
            server.Shutdown();
            return false;
        }
        const auto serverFrameDeadline =
            std::chrono::steady_clock::now() + 3s;
        while (std::chrono::steady_clock::now() < serverFrameDeadline &&
            serverLargeFrames.load(std::memory_order_acquire) == 0u)
        {
            std::this_thread::sleep_for(5ms);
        }

        const UdpClientMetrics clientMetrics = client->GetMetrics();
        const UdpServerMetrics liveServerMetrics = server.GetMetrics();
        const bool livePass =
            connectedCallbacks.load(std::memory_order_relaxed) == 1u &&
            serverLargeFrames.load(std::memory_order_relaxed) == 1u &&
            clientMetrics.reassemblyComplete >= 1u &&
            liveServerMetrics.reassemblyComplete >= 1u &&
            liveServerMetrics.connectedPeers == 1u;
        if (!livePass)
        {
            std::printf(
                "[UdpLoopbackHarness] live final fail callbacks=%u serverLarge=%u clientReasm=%llu serverReasm=%llu peers=%llu invalidS=%llu invalidC=%llu\n",
                connectedCallbacks.load(std::memory_order_relaxed),
                serverLargeFrames.load(std::memory_order_relaxed),
                static_cast<unsigned long long>(clientMetrics.reassemblyComplete),
                static_cast<unsigned long long>(liveServerMetrics.reassemblyComplete),
                static_cast<unsigned long long>(liveServerMetrics.connectedPeers),
                static_cast<unsigned long long>(liveServerMetrics.invalidDatagrams),
                static_cast<unsigned long long>(clientMetrics.invalidDatagrams));
        }

        client->Disconnect();
        server.Shutdown();
        const UdpServerMetrics shutdownMetrics = server.GetMetrics();
        return livePass && shutdownMetrics.outstandingIo == 0u;
    }
}

int main()
{
    const bool codec = TestCodec();
    const bool ordered = TestOrderedGapBuffer();
    const bool laneIsolation = TestLaneAckIsolation();
    const bool adaptiveRto = TestAdaptiveRto();
    const bool karn = TestKarnRetransmitExclusion();
    const bool chaos = TestReliableChaosPipe();
    const bool reassembly = TestMeasuredPayloadReassembly();
    const bool loopback = TestLiveLoopback();
    const bool pass = codec && ordered && laneIsolation &&
        adaptiveRto && karn && chaos && reassembly && loopback;
    std::printf(
        "[UdpLoopbackHarness] %s codec=%d ordered=%d laneAck=%d rto=%d karn=%d chaos=%d reassembly22104=%d live=%d\n",
        pass ? "PASS" : "FAIL",
        codec ? 1 : 0,
        ordered ? 1 : 0,
        laneIsolation ? 1 : 0,
        adaptiveRto ? 1 : 0,
        karn ? 1 : 0,
        chaos ? 1 : 0,
        reassembly ? 1 : 0,
        loopback ? 1 : 0);
    return pass ? 0 : 1;
}
