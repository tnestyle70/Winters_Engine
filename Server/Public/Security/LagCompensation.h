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
	bool_t TryGetHistoricalStateAtTick(
		EntityHandle hEntity,
		u64_t uExpectedTick,
		LagCompensatedEntityState& outState) const override;

	// Chrono Break: 되감기 후 미래 틱 히스토리는 무효 — 전체 클리어(6틱 내 자가 회복).
	void Reset()
	{
		m_history.clear();
		m_latestTick = 0;
	}

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
