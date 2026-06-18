#pragma once

#include "Defines.h"
#include "Shared/Replay/ReplayFormat.h"

#include <memory>
#include <string>
#include <vector>

class CEventApplier;
class CSnapshotApplier;
class CWorld;
class EntityIdMap;

class CReplayPlayer final
{
public:
	static std::unique_ptr<CReplayPlayer> LoadFromFile(
		const wstring_t& path,
		std::string& outError);

	bool_t Update(
		f32_t dt, CWorld& world,
		EntityIdMap& entityMap,
		CSnapshotApplier& snapshotApplier,
		CEventApplier& eventApplier);

	void SetPaused(bool_t bPaused) { m_bPaused = bPaused; }
	bool_t IsPaused() const { return m_bPaused; }

	void SetPlaybackRate(f32_t rate);
	f32_t GetPlaybackRate() const { return m_fPlaybackRate; }

	bool_t IsFinished() const { return m_bFinished; }
	u64_t GetCurrentTick() const { return m_iCurrentTick; }
	u64_t GetFirstTick() const { return m_Header.firstTick; }
	u64_t GetLastTick() const { return m_Header.lastTick; }
	u32_t GetRecordCount() const { return static_cast<u32_t>(m_Records.size()); }
	const std::string& GetDisplayName() const { return m_strDisplayName; }

private:
	struct ReplayRecord
	{
		Winters::Replay::ReplayRecordHeader header{};
		std::vector<u8_t> payload{};
	};

	CReplayPlayer() = default;

	bool_t ApplyTickGroup(
		size_t begin,
		size_t end,
		CWorld& world,
		EntityIdMap& entityMap,
		CSnapshotApplier& snapshotApplier,
		CEventApplier& eventApplier);

	Winters::Replay::ReplayFileHeader m_Header{};
	std::vector<ReplayRecord> m_Records{};
	size_t m_iNextRecord = 0;
	double m_fPlayheadTick = 0.0;
	f32_t m_fTickRate = 30.f;
	f32_t m_fPlaybackRate = 1.f;
	u64_t m_iCurrentTick = 0;
	bool_t m_bPaused = false;
	bool_t m_bFinished = false;
	std::string m_strDisplayName;
};
