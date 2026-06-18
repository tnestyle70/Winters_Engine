#include "Game/ReplayRecorder.h"

#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

namespace
{
	u64_t NowUnixMs()
	{
		using namespace std::chrono;
		return static_cast<u64_t>(duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count());
	}
}

CReplayRecorder::CReplayRecorder(u32_t roomID, u32_t tickRate)
	: m_iRoomID(roomID), m_iTickRate(tickRate)
{}

std::unique_ptr<CReplayRecorder> CReplayRecorder::Create(u32_t roomID, u32_t tickRate)
{
	return std::unique_ptr<CReplayRecorder>(new CReplayRecorder(roomID, tickRate));
}

void CReplayRecorder::RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len)
{
	Record(Winters::Replay::eReplayRecordType::Snapshot, tick, bytes, len);
}

void CReplayRecorder::RecordEvent(u64_t tick, const u8_t * bytes, u32_t len)
{
	Record(Winters::Replay::eReplayRecordType::Event, tick, bytes, len);
}

void CReplayRecorder::Record(Winters::Replay::eReplayRecordType type,
	u64_t tick, const u8_t* bytes, u32_t len)
{
	if (!bytes || len == 0)
		return;

	if (m_Records.empty())
		m_iFirstTick = tick;
	m_iLastTick = tick;

	ReplayRecord record{};
	record.header.type = static_cast<u8_t>(type);
	record.header.payloadSize = len;
	record.header.serverTick = tick;
	record.header.sequence = static_cast<u32_t>(tick & 0xFFFFFFFFull);
	record.payload.assign(bytes, bytes + len);
	m_Records.emplace_back(std::move(record));

	if (type == Winters::Replay::eReplayRecordType::Snapshot)
		++m_iSnapshotCount;
	else if (type == Winters::Replay::eReplayRecordType::Event)
		++m_iEventCount;
}

bool_t CReplayRecorder::SaveToFile(const wstring_t& path, std::string& outError) const
{
	outError.clear();

	if (m_Records.empty())
	{
		outError = "no replay records";
		return false;
	}

	try
	{
		const std::filesystem::path fsPath(path);
		if (fsPath.has_parent_path())
			std::filesystem::create_directories(fsPath.parent_path());

		std::ofstream out(fsPath, std::ios::binary);
		if (!out)
		{
			outError = "failed to open replay file";
			return false;
		}

		Winters::Replay::ReplayFileHeader header{};
		header.recordCount = static_cast<u32_t>(m_Records.size());
		header.snapshotCount = m_iSnapshotCount;
		header.eventCount = m_iEventCount;
		header.firstTick = m_iFirstTick;
		header.lastTick = m_iLastTick;
		header.createdUnixMs = NowUnixMs();

		out.write(reinterpret_cast<const char*>(&header), sizeof(header));

		for (const ReplayRecord& record : m_Records)
		{
			out.write(reinterpret_cast<const char*>(&record.header), sizeof(record.header));
			out.write(reinterpret_cast<const char*>(record.payload.data()),
				static_cast<std::streamsize>(record.payload.size()));
		}
		if (!out.good())
		{
			outError = "failed to write replay file";
			return false;
		}

		return true;
	}
	catch (const std::exception& e)
	{
		outError = e.what();
		return false;
	}
}

wstring_t CReplayRecorder::MakeDefaultPath() const
{
	std::wstringstream ss;
	ss << L"Replay/room" << m_iRoomID
		<< L"_tick" << m_iFirstTick
		<< L"_" << m_iLastTick
		<< L".wrpl";
	return ss.str();
}
