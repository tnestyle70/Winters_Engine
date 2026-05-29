#pragma once

#include "Shared/Network/UdpPacketHeader.h"

#include <deque>
#include <vector>

class CUdpReliabilityChannel final
{
public:
	u32_t NextSendSeq();
	void MarkReceived(u32_t seq);
	void BuildAck(u32_t& outAckSeq, u32_t& outAckBitfield) const;
	void QueueReliable(u32_t seq, std::vector<u8_t> packet, u64_t nowMs);
	void OnAck(u32_t ackSeq, u32_t ackBitfield);
	void CollectRetransmit(u64_t nowMs, std::vector<std::vector<u8_t>>& outPackets);

private:
	struct PendingPacket
	{
		u32_t seq = 0;
		u64_t lastSendMs = 0;
		std::vector<u8_t> packet;
	};

	u32_t m_nextSendSeq = 1;
	u32_t m_latestReceivedSeq = 0;
	u32_t m_receivedMask = 0;
	std::deque<PendingPacket> m_pendingReliable;
};
