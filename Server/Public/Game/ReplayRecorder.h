#pragma once

#include "WintersTypes.h"
#include "Shared/Replay/ReplayFormat.h"

#include <memory>
#include <string>
#include <vector>

class CReplayRecorder final
{
public:
	static std::unique_ptr<CReplayRecorder> Create(u32_t roomID, u32_t tickRate);

	void RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len);
	void RecordEvent(u64_t tick, const u8_t* bytes, u32_t len);

	bool_t SaveToFile(const wstring_t& path, std::string& outError) const;
	wstring_t MakeDefaultPath() const;

	bool_t IsEmpty() const { return m_Records.empty(); }
	u32_t GetRoomID() const { return m_iRoomID; }
	u32_t GetTickRate() const { return m_iTickRate; }
	u32_t GetRecordCount() const { return static_cast<u32_t>(m_Records.size()); }
	u32_t GetSnapshotCount() const { return m_iSnapshotCount; }
	u32_t GetEventCount() const { return m_iEventCount; }
	u64_t GetFirstTick() const { return m_iFirstTick; }
	u64_t GetLastTick() const { return m_iLastTick; }

private:
	struct ReplayRecord
	{
		Winters::Replay::ReplayRecordHeader header{};
		std::vector<u8_t> payload{};
	};

	CReplayRecorder(u32_t roomID, u32_t tickRate);

	void Record(Winters::Replay::eReplayRecordType type, u64_t tick,
		const u8_t* bytes, u32_t len);

	u32_t m_iRoomID = 0;
	u32_t m_iTickRate = 0;
	//이거 이름 겹치는 거 의도함? 대답 부탁
	std::vector<ReplayRecord> m_Records{};
	u32_t m_iSnapshotCount = 0;
	u32_t m_iEventCount = 0;
	u64_t m_iFirstTick = 0;
	u64_t m_iLastTick = 0;
};
