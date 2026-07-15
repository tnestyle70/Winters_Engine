#include "Shared/Network/UdpReliabilityChannel.h"

#include "Shared/Network/SeqMath.h"

#include <algorithm>

namespace
{
    constexpr size_t kMaxPendingPackets = 32u;
    constexpr size_t kMaxPendingBytes = 256u * 1024u;
    constexpr u32_t kMaxSendAttempts = 8u;

    bool IsAcked(u32_t sequence, u32_t ackSequence, u32_t ackBitfield)
    {
        if (sequence == 0u || ackSequence == 0u)
            return false;
        if (sequence == ackSequence)
            return true;
        if (!SeqGreater(ackSequence, sequence))
            return false;

        const u32_t distance = SeqDistance(ackSequence, sequence);
        if (distance == 0u || distance > 32u)
            return false;
        return (ackBitfield & (1u << (distance - 1u))) != 0u;
    }
}

u32_t CUdpReliabilityChannel::NextSendSeq()
{
    const u32_t sequence = m_nextSendSeq++;
    if (m_nextSendSeq == 0u)
        m_nextSendSeq = 1u;
    return sequence == 0u ? NextSendSeq() : sequence;
}

eUdpReceiveStatus CUdpReliabilityChannel::MarkReceived(u32_t sequence)
{
    if (sequence == 0u)
        return eUdpReceiveStatus::TooOld;

    if (m_latestReceivedSeq == 0u)
    {
        m_latestReceivedSeq = sequence;
        m_receivedMask = 0u;
        return eUdpReceiveStatus::NewPacket;
    }

    if (sequence == m_latestReceivedSeq)
        return eUdpReceiveStatus::Duplicate;

    if (SeqGreater(sequence, m_latestReceivedSeq))
    {
        const u32_t distance = SeqDistance(sequence, m_latestReceivedSeq);
        if (distance > 32u)
        {
            m_receivedMask = 0u;
        }
        else if (distance == 32u)
        {
            m_receivedMask = 1u << 31u;
        }
        else
        {
            m_receivedMask =
                (m_receivedMask << distance) |
                (1u << (distance - 1u));
        }
        m_latestReceivedSeq = sequence;
        return eUdpReceiveStatus::NewPacket;
    }

    const u32_t distance = SeqDistance(m_latestReceivedSeq, sequence);
    if (distance == 0u || distance > 32u)
        return eUdpReceiveStatus::TooOld;

    const u32_t bit = 1u << (distance - 1u);
    if ((m_receivedMask & bit) != 0u)
        return eUdpReceiveStatus::Duplicate;
    m_receivedMask |= bit;
    return eUdpReceiveStatus::NewPacket;
}

void CUdpReliabilityChannel::BuildAck(
    u32_t& outAckSeq,
    u32_t& outAckBitfield) const
{
    outAckSeq = m_latestReceivedSeq;
    outAckBitfield = m_receivedMask;
}

bool CUdpReliabilityChannel::QueueReliable(
    u32_t sequence,
    std::vector<u8_t> packet,
    u64_t nowMs)
{
    if (sequence == 0u || packet.empty() ||
        m_pendingReliable.size() >= kMaxPendingPackets ||
        packet.size() > kMaxPendingBytes - m_pendingBytes)
    {
        return false;
    }

    m_pendingBytes += packet.size();
    m_pendingReliable.push_back({
        sequence,
        1u,
        nowMs,
        std::move(packet),
    });
    return true;
}

bool CUdpReliabilityChannel::CanQueueReliable(
    size_t packetCount,
    size_t byteCount) const
{
    if (packetCount == 0u ||
        packetCount > kMaxPendingPackets - m_pendingReliable.size() ||
        byteCount > kMaxPendingBytes - m_pendingBytes)
    {
        return false;
    }
    if (m_pendingReliable.empty())
        return packetCount <= kMaxPendingPackets;

    const u32_t prospectiveLast =
        m_nextSendSeq + static_cast<u32_t>(packetCount - 1u);
    return SeqDistance(
        prospectiveLast,
        m_pendingReliable.front().seq) <= 32u;
}

void CUdpReliabilityChannel::OnAck(u32_t ackSeq, u32_t ackBitfield, u64_t nowMs)
{
    auto iterator = m_pendingReliable.begin();
    while (iterator != m_pendingReliable.end())
    {
        if (!IsAcked(iterator->seq, ackSeq, ackBitfield))
        {
            ++iterator;
            continue;
        }

        // Karn's algorithm: only never-retransmitted packets yield RTT
        // samples, so lastSendMs is guaranteed to be the first send time.
        if (iterator->sendAttempts == 1u && nowMs >= iterator->lastSendMs)
            ApplyRttSample(nowMs - iterator->lastSendMs);

        m_pendingBytes -= iterator->packet.size();
        iterator = m_pendingReliable.erase(iterator);
    }
}

void CUdpReliabilityChannel::ApplyRttSample(u64_t sampleMs)
{
    if (m_rttSampleCount == 0u)
    {
        m_srttMs = sampleMs;
        m_rttVarMs = sampleMs / 2u;
    }
    else
    {
        const u64_t deviation = m_srttMs > sampleMs
            ? m_srttMs - sampleMs
            : sampleMs - m_srttMs;
        m_rttVarMs = (3u * m_rttVarMs + deviation) / 4u;
        m_srttMs = (7u * m_srttMs + sampleMs) / 8u;
    }
    ++m_rttSampleCount;

    const u64_t variance = std::max(kUdpRtoGranularityMs, 4u * m_rttVarMs);
    m_currentRtoMs = std::clamp(
        m_srttMs + variance,
        kUdpMinRtoMs,
        kUdpMaxRtoMs);
}

void CUdpReliabilityChannel::CollectRetransmit(
    u64_t nowMs,
    std::vector<std::vector<u8_t>>& outPackets)
{
    bool retransmitted = false;
    auto iterator = m_pendingReliable.begin();
    while (iterator != m_pendingReliable.end())
    {
        const u32_t backoffShift = std::min(iterator->sendAttempts - 1u, 3u);
        const u64_t timeoutMs = std::min(
            m_currentRtoMs << backoffShift,
            kUdpMaxRetransmitTimeoutMs);
        // The caller may capture its clock before another thread queues a
        // packet with a fresher lastSendMs; an unguarded u64 subtraction
        // would underflow and force a spurious immediate retransmit.
        const u64_t elapsedMs = nowMs > iterator->lastSendMs
            ? nowMs - iterator->lastSendMs
            : 0u;
        if (elapsedMs < timeoutMs)
        {
            ++iterator;
            continue;
        }

        if (iterator->sendAttempts >= kMaxSendAttempts)
        {
            m_pendingBytes -= iterator->packet.size();
            ++m_retryExhaustedCount;
            iterator = m_pendingReliable.erase(iterator);
            continue;
        }

        outPackets.push_back(iterator->packet);
        iterator->lastSendMs = nowMs;
        ++iterator->sendAttempts;
        ++m_retransmitCount;
        retransmitted = true;
        ++iterator;
    }

    // Karn's algorithm, second half: keep a backed-off RTO after a timeout
    // until the next never-retransmitted packet yields a valid sample (which
    // recomputes the RTO from SRTT/RTTVAR). Without this, a path RTT above
    // the current RTO retransmits every packet, so no sample is ever taken
    // and adaptation freezes.
    if (retransmitted)
        m_currentRtoMs = std::min(m_currentRtoMs * 2u, kUdpMaxRtoMs);
}

void CUdpReliabilityChannel::Reset()
{
    m_nextSendSeq = 1u;
    m_latestReceivedSeq = 0u;
    m_receivedMask = 0u;
    m_pendingReliable.clear();
    m_pendingBytes = 0u;
    m_retransmitCount = 0u;
    m_retryExhaustedCount = 0u;
    m_srttMs = 0u;
    m_rttVarMs = 0u;
    m_currentRtoMs = kUdpInitialRtoMs;
    m_rttSampleCount = 0u;
}
