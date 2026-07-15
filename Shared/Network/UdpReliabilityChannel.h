#pragma once

#include "Shared/Network/UdpPacketHeader.h"

#include <deque>
#include <cstddef>
#include <vector>

enum class eUdpReceiveStatus : u8_t
{
	NewPacket = 0,
	Duplicate,
	TooOld,
};

// RFC 6298-style retransmission timer bounds. Min/granularity are tightened
// for a 30 Hz game loop; retransmits never wait longer than the final cap
// even after exponential backoff.
constexpr u64_t kUdpInitialRtoMs = 120u;
constexpr u64_t kUdpMinRtoMs = 60u;
constexpr u64_t kUdpMaxRtoMs = 1000u;
constexpr u64_t kUdpRtoGranularityMs = 10u;
constexpr u64_t kUdpMaxRetransmitTimeoutMs = 2000u;

class CUdpReliabilityChannel final
{
public:
	u32_t NextSendSeq();
	eUdpReceiveStatus MarkReceived(u32_t seq);
	void BuildAck(u32_t& outAckSeq, u32_t& outAckBitfield) const;
	bool QueueReliable(u32_t seq, std::vector<u8_t> packet, u64_t nowMs);
	bool CanQueueReliable(size_t packetCount, size_t byteCount) const;
	void OnAck(u32_t ackSeq, u32_t ackBitfield, u64_t nowMs);
	void CollectRetransmit(u64_t nowMs, std::vector<std::vector<u8_t>>& outPackets);
	void Reset();

	size_t GetPendingPacketCount() const { return m_pendingReliable.size(); }
	size_t GetPendingBytes() const { return m_pendingBytes; }
	u64_t GetRetransmitCount() const { return m_retransmitCount; }
	u64_t GetRetryExhaustedCount() const { return m_retryExhaustedCount; }
	u64_t GetSmoothedRttMs() const { return m_srttMs; }
	u64_t GetCurrentRtoMs() const { return m_currentRtoMs; }
	u64_t GetRttSampleCount() const { return m_rttSampleCount; }

private:
	struct PendingPacket
	{
		u32_t seq = 0;
		u32_t sendAttempts = 0;
		u64_t lastSendMs = 0;
		std::vector<u8_t> packet;
	};

	void ApplyRttSample(u64_t sampleMs);

	u32_t m_nextSendSeq = 1;
	u32_t m_latestReceivedSeq = 0;
	u32_t m_receivedMask = 0;
	std::deque<PendingPacket> m_pendingReliable;
	size_t m_pendingBytes = 0;
	u64_t m_retransmitCount = 0;
	u64_t m_retryExhaustedCount = 0;
	u64_t m_srttMs = 0;
	u64_t m_rttVarMs = 0;
	u64_t m_currentRtoMs = kUdpInitialRtoMs;
	u64_t m_rttSampleCount = 0;
};
