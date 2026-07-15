#pragma once

#include "WintersTypes.h"
#include "Shared/Replay/ReplayFormat.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

class CReplayRecorder final
{
public:
	static std::unique_ptr<CReplayRecorder> Create(u32_t roomID, u32_t tickRate);
	~CReplayRecorder();

	void RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len);
	void RecordEvent(u64_t tick, const u8_t* bytes, u32_t len);
	bool_t RecordCommand(u64_t tick,
		const Winters::Replay::ReplayCommandPayload& payload);

	static bool_t ShouldRecordSnapshot(
		u64_t tick,
		u64_t toolRevision,
		u64_t lastTick,
		u64_t lastToolRevision);

	bool_t SaveToFile(const wstring_t& path, std::string& outError);
	wstring_t MakeDefaultPath() const;

	bool_t IsEmpty() const { return m_iRecordCount == 0u; }
	bool_t HasWriteFailure() const { return m_bWriteFailed; }
	const std::string& GetWriteError() const { return m_strWriteError; }
	u32_t GetRoomID() const { return m_iRoomID; }
	u32_t GetTickRate() const { return m_iTickRate; }
	u32_t GetRecordCount() const { return m_iRecordCount; }
	u32_t GetSnapshotCount() const { return m_iSnapshotCount; }
	u32_t GetEventCount() const { return m_iEventCount; }
	u32_t GetCommandCount() const { return m_iCommandCount; }
	u64_t GetFirstTick() const { return m_iFirstTick; }
	u64_t GetLastTick() const { return m_iLastTick; }
	u64_t GetPayloadBytes() const { return m_iPayloadBytes; }
	u64_t GetSpoolBytes() const { return m_iSpoolBytes; }

private:
	CReplayRecorder(u32_t roomID, u32_t tickRate);

	bool_t EnsureSpoolOpen();
	void SetWriteFailure(const std::string& error);
	bool_t Record(Winters::Replay::eReplayRecordType type, u64_t tick,
		const u8_t* bytes, u32_t len);

	u32_t m_iRoomID = 0;
	u32_t m_iTickRate = 0;
	std::filesystem::path m_spoolPath{};
	std::ofstream m_spoolStream{};
	std::string m_strWriteError{};
	u32_t m_iRecordCount = 0;
	u32_t m_iSnapshotCount = 0;
	u32_t m_iEventCount = 0;
	u32_t m_iCommandCount = 0;
	u64_t m_iFirstTick = 0;
	u64_t m_iLastTick = 0;
	u64_t m_iPayloadBytes = 0;
	u64_t m_iSpoolBytes = 0;
	bool_t m_bWriteFailed = false;
	bool_t m_bSealed = false;
	bool_t m_bFinalized = false;
};
