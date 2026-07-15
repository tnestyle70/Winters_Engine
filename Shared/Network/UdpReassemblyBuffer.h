#pragma once

#include "Shared/Network/UdpFragmentCodec.h"
#include "Shared/Network/UdpPacketHeader.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <span>
#include <vector>

enum class eUdpReassemblyResult : u8_t
{
    Accepted = 0,
    Complete,
    Duplicate,
    Invalid,
    Overflow,
};

struct UdpReassembledMessage
{
    ePacketType type = ePacketType::None;
    PacketLane lane = PacketLane::Invalid;
    u32_t messageSequence = 0;
    std::vector<u8_t> payload;
};

class CUdpReassemblyBuffer final
{
public:
    explicit CUdpReassemblyBuffer(
        size_t maxMessages = 8u,
        size_t maxBytes = 256u * 1024u,
        u64_t timeoutMs = 7'000u)
        : m_maxMessages(maxMessages),
          m_maxBytes(maxBytes),
          m_timeoutMs(timeoutMs)
    {
    }

    eUdpReassemblyResult Push(
        const UdpPacketHeader& packetHeader,
        std::span<const u8_t> fragmentPayload,
        u64_t nowMs,
        UdpReassembledMessage& outMessage)
    {
        outMessage = {};
        if ((packetHeader.flags & UdpPacketFlag_Fragment) == 0u)
            return eUdpReassemblyResult::Invalid;

        UdpFragmentHeader fragment{};
        if (!DecodeUdpFragmentHeader(fragmentPayload, fragment) ||
            fragment.reserved != 0u ||
            fragment.messageId == 0u ||
            fragment.messageId != packetHeader.messageSeq ||
            fragment.messageBytes == 0u ||
            fragment.messageBytes > kUdpMaxMessageBytes ||
            fragment.fragmentCount < 2u ||
            fragment.fragmentCount > kUdpMaxFragmentsPerMessage ||
            fragment.fragmentIndex >= fragment.fragmentCount ||
            fragment.fragmentPayloadSize == 0u ||
            fragment.fragmentPayloadSize > kUdpMaxFragmentDataBytes ||
            fragmentPayload.size() !=
                kUdpFragmentHeaderBytes + fragment.fragmentPayloadSize)
        {
            return eUdpReassemblyResult::Invalid;
        }

        const u32_t expectedFragmentCount =
            (fragment.messageBytes + kUdpMaxFragmentDataBytes - 1u) /
            kUdpMaxFragmentDataBytes;
        const u32_t fragmentOffset =
            static_cast<u32_t>(fragment.fragmentIndex) *
            kUdpMaxFragmentDataBytes;
        if (fragmentOffset >= fragment.messageBytes)
            return eUdpReassemblyResult::Invalid;
        const u32_t expectedFragmentBytes = std::min<u32_t>(
            kUdpMaxFragmentDataBytes,
            fragment.messageBytes - fragmentOffset);
        if (fragment.fragmentCount != expectedFragmentCount ||
            fragment.fragmentPayloadSize != expectedFragmentBytes)
        {
            return eUdpReassemblyResult::Invalid;
        }

        const u64_t key =
            (static_cast<u64_t>(static_cast<u8_t>(packetHeader.lane)) << 32u) |
            fragment.messageId;
        auto iterator = m_pending.find(key);
        if (iterator == m_pending.end())
        {
            const PacketSemantics semantics =
                ResolvePacketSemantics(packetHeader.type);
            if (!IsReliableDelivery(semantics.delivery))
            {
                auto pendingIterator = m_pending.begin();
                while (pendingIterator != m_pending.end())
                {
                    const PendingMessage& pending = pendingIterator->second;
                    if (pending.type != packetHeader.type ||
                        pending.lane != packetHeader.lane)
                    {
                        ++pendingIterator;
                        continue;
                    }
                    if (SeqGreater(
                        pending.messageSequence,
                        packetHeader.messageSeq))
                    {
                        return eUdpReassemblyResult::Duplicate;
                    }
                    if (!SeqGreater(
                        packetHeader.messageSeq,
                        pending.messageSequence))
                    {
                        ++pendingIterator;
                        continue;
                    }
                    m_pendingBytes -= pending.messageBytes;
                    pendingIterator = m_pending.erase(pendingIterator);
                }
            }

            if (m_pending.size() >= m_maxMessages ||
                fragment.messageBytes > m_maxBytes - m_pendingBytes)
            {
                return eUdpReassemblyResult::Overflow;
            }

            PendingMessage pending{};
            pending.type = packetHeader.type;
            pending.lane = packetHeader.lane;
            pending.messageSequence = packetHeader.messageSeq;
            pending.messageBytes = fragment.messageBytes;
            pending.fragmentCount = fragment.fragmentCount;
            pending.lastUpdateMs = nowMs;
            pending.bytes.resize(fragment.messageBytes);
            pending.received.resize(fragment.fragmentCount, false);
            m_pendingBytes += fragment.messageBytes;
            iterator = m_pending.emplace(key, std::move(pending)).first;
        }

        PendingMessage& pending = iterator->second;
        if (pending.type != packetHeader.type ||
            pending.lane != packetHeader.lane ||
            pending.messageSequence != packetHeader.messageSeq ||
            pending.messageBytes != fragment.messageBytes ||
            pending.fragmentCount != fragment.fragmentCount)
        {
            return eUdpReassemblyResult::Invalid;
        }
        if (pending.received[fragment.fragmentIndex])
            return eUdpReassemblyResult::Duplicate;

        const u8_t* fragmentData =
            fragmentPayload.data() + kUdpFragmentHeaderBytes;
        std::copy_n(
            fragmentData,
            fragment.fragmentPayloadSize,
            pending.bytes.data() + fragmentOffset);
        pending.received[fragment.fragmentIndex] = true;
        ++pending.receivedCount;
        pending.lastUpdateMs = nowMs;

        if (pending.receivedCount != pending.fragmentCount)
            return eUdpReassemblyResult::Accepted;

        outMessage.type = pending.type;
        outMessage.lane = pending.lane;
        outMessage.messageSequence = pending.messageSequence;
        outMessage.payload = std::move(pending.bytes);
        m_pendingBytes -= pending.messageBytes;
        m_pending.erase(iterator);
        return eUdpReassemblyResult::Complete;
    }

    size_t Expire(u64_t nowMs)
    {
        size_t expired = 0u;
        auto iterator = m_pending.begin();
        while (iterator != m_pending.end())
        {
            if (nowMs - iterator->second.lastUpdateMs < m_timeoutMs)
            {
                ++iterator;
                continue;
            }
            m_pendingBytes -= iterator->second.messageBytes;
            iterator = m_pending.erase(iterator);
            ++expired;
        }
        return expired;
    }

    void Reset()
    {
        m_pending.clear();
        m_pendingBytes = 0u;
    }

    size_t GetPendingCount() const { return m_pending.size(); }
    size_t GetPendingBytes() const { return m_pendingBytes; }

private:
    struct PendingMessage
    {
        ePacketType type = ePacketType::None;
        PacketLane lane = PacketLane::Invalid;
        u32_t messageSequence = 0;
        u32_t messageBytes = 0;
        u16_t fragmentCount = 0;
        u16_t receivedCount = 0;
        u64_t lastUpdateMs = 0;
        std::vector<u8_t> bytes;
        std::vector<bool> received;
    };

    size_t m_maxMessages = 0u;
    size_t m_maxBytes = 0u;
    u64_t m_timeoutMs = 0u;
    size_t m_pendingBytes = 0u;
    std::map<u64_t, PendingMessage> m_pending;
};
