#pragma once

#include "Shared/Network/PacketType.h"
#include "Shared/Network/SeqMath.h"
#include "WintersTypes.h"

#include <cstddef>
#include <map>
#include <utility>
#include <vector>

struct UdpOrderedMessage
{
    ePacketType type = ePacketType::None;
    u32_t sequence = 0;
    std::vector<u8_t> payload;
};

enum class eUdpOrderedPushResult : u8_t
{
    Delivered = 0,
    Buffered,
    DuplicateOrOld,
    Overflow,
};

// Per-lane ReliableOrdered gap buffer. It owns payload bytes and releases only
// a contiguous sequence beginning at nextExpectedSequence.
class CUdpOrderedReceiveQueue final
{
public:
    explicit CUdpOrderedReceiveQueue(
        size_t maxMessages = 64u,
        size_t maxBytes = 128u * 1024u)
        : m_maxMessages(maxMessages),
          m_maxBytes(maxBytes)
    {
    }

    eUdpOrderedPushResult Push(
        ePacketType type,
        u32_t sequence,
        std::vector<u8_t> payload,
        std::vector<UdpOrderedMessage>& outDelivered)
    {
        if (sequence == 0u)
            return eUdpOrderedPushResult::DuplicateOrOld;

        if (sequence == m_nextExpectedSequence)
        {
            outDelivered.push_back({ type, sequence, std::move(payload) });
            AdvanceExpected();
            DrainContiguous(outDelivered);
            return eUdpOrderedPushResult::Delivered;
        }

        if (!SeqGreater(sequence, m_nextExpectedSequence))
            return eUdpOrderedPushResult::DuplicateOrOld;
        if (m_pending.find(sequence) != m_pending.end())
            return eUdpOrderedPushResult::DuplicateOrOld;
        if (m_pending.size() >= m_maxMessages ||
            payload.size() > m_maxBytes - m_pendingBytes)
        {
            return eUdpOrderedPushResult::Overflow;
        }

        m_pendingBytes += payload.size();
        m_pending.emplace(sequence, UdpOrderedMessage{
            type,
            sequence,
            std::move(payload),
        });
        return eUdpOrderedPushResult::Buffered;
    }

    void Reset(u32_t nextExpectedSequence = 1u)
    {
        m_nextExpectedSequence = nextExpectedSequence == 0u
            ? 1u
            : nextExpectedSequence;
        m_pending.clear();
        m_pendingBytes = 0u;
    }

    u32_t GetNextExpectedSequence() const { return m_nextExpectedSequence; }
    size_t GetPendingCount() const { return m_pending.size(); }
    size_t GetPendingBytes() const { return m_pendingBytes; }

private:
    void AdvanceExpected()
    {
        ++m_nextExpectedSequence;
        if (m_nextExpectedSequence == 0u)
            m_nextExpectedSequence = 1u;
    }

    void DrainContiguous(std::vector<UdpOrderedMessage>& outDelivered)
    {
        for (;;)
        {
            auto iterator = m_pending.find(m_nextExpectedSequence);
            if (iterator == m_pending.end())
                break;
            m_pendingBytes -= iterator->second.payload.size();
            outDelivered.push_back(std::move(iterator->second));
            m_pending.erase(iterator);
            AdvanceExpected();
        }
    }

    size_t m_maxMessages = 0u;
    size_t m_maxBytes = 0u;
    u32_t m_nextExpectedSequence = 1u;
    size_t m_pendingBytes = 0u;
    std::map<u32_t, UdpOrderedMessage> m_pending;
};
