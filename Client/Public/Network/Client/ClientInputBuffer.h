#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <array>
#include <functional>

class CClientInputBuffer final
{
public:
	static constexpr u32_t kCapacity = 120;

	void Push(const GameCommandWire& wire);
	void DropAcked(u32_t ackedSeq);
	void ForEachAfter(u32_t ackedSeq, const std::function<void(const GameCommandWire&)>& fn) const;

private:
	std::array<GameCommandWire, kCapacity> m_commands{};
	u32_t m_head = 0;
	u32_t m_count = 0;
};