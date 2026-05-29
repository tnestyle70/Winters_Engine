#pragma once

#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <deque>
#include <unordered_map>

class CLagCompensation final : public ILagCompensationQuery
{
public:
	static constexpr u64_t kMaxRewindMs = 200;
	static constexpr u64_t kTickRate = 30;
	static constexpr u64_t kMaxRewindTicks = (kMaxRewindMs * kTickRate + 999) / 1000;

	void RecordHistory(CWorld& world, u64_t tickIndex);
	bool_t TryGetHistoricalState(
		EntityID entity,
		u64_t rewindTicks,
		LagCompensatedEntityState& outState) const override;

private:
	struct HistoryFrame
	{
		u64_t tickIndex = 0;
		EntityGeneration generation = NULL_ENTITY_GENERATION;
		LagCompensatedEntityState state{};
	};

	std::unordered_map<EntityID, std::deque<HistoryFrame>> m_history;
	u64_t m_latestTick = 0;
};
