#include "Network/Client/ClientInputBuffer.h"

void CClientInputBuffer::Push(const GameCommandWire& wire)
{
	m_commands[m_head] = wire;
	m_head = (m_head + 1) % kCapacity;
	if (m_count < kCapacity)
		++m_count;
}

void CClientInputBuffer::DropAcked(u32_t ackedSeq)
{
	u32_t kept = 0;
	std::array<GameCommandWire, kCapacity> next{};

	ForEachAfter(ackedSeq, [&](const GameCommandWire& wire)
		{
			next[kept++] = wire;
		});

	m_commands = next;
	m_head = kept % kCapacity;
	m_count = kept;
}

void CClientInputBuffer::Clear()
{
	m_commands = {};
	m_head = 0;
	m_count = 0;
}

void CClientInputBuffer::ForEachAfter(u32_t ackedSeq,
	const std::function<void(const GameCommandWire&)>&fn) const
{
	const u32_t start = (m_head + kCapacity - m_count) % kCapacity;
	for (u32_t i = 0; i < m_count; ++i)
	{
		const GameCommandWire& wire = m_commands[(start + i) % kCapacity];
		if (wire.sequenceNum > ackedSeq)
			fn(wire);
	}
}
