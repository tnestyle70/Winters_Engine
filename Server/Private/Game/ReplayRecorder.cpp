#include "Game/ReplayRecorder.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace
{
	std::atomic<u64_t> g_iReplaySpoolSequence{ 0u };

	u64_t NowUnixMs()
	{
		using namespace std::chrono;
		return static_cast<u64_t>(duration_cast<milliseconds>(
			system_clock::now().time_since_epoch()).count());
	}

	std::filesystem::path MakeSpoolPath(u32_t roomID)
	{
		using namespace std::chrono;
		const u64_t timestamp = static_cast<u64_t>(duration_cast<nanoseconds>(
			high_resolution_clock::now().time_since_epoch()).count());
		const u64_t sequence =
			g_iReplaySpoolSequence.fetch_add(1u, std::memory_order_relaxed);

		std::ostringstream name;
		name << "WintersReplay_" << roomID << '_' << timestamp
			<< '_' << sequence << ".tmp";
		return std::filesystem::temp_directory_path() / name.str();
	}

	std::filesystem::path MakePublishPath(
		const std::filesystem::path& destination)
	{
		const u64_t sequence =
			g_iReplaySpoolSequence.fetch_add(1u, std::memory_order_relaxed);
		std::wostringstream name;
		name << destination.filename().wstring()
			<< L".publishing." << sequence << L".tmp";
		return destination.parent_path() / name.str();
	}
}

CReplayRecorder::CReplayRecorder(u32_t roomID, u32_t tickRate)
	: m_iRoomID(roomID), m_iTickRate(tickRate)
{}

CReplayRecorder::~CReplayRecorder()
{
	if (m_spoolStream.is_open())
		m_spoolStream.close();
	if (!m_spoolPath.empty())
	{
		std::error_code error;
		std::filesystem::remove(m_spoolPath, error);
	}
}

std::unique_ptr<CReplayRecorder> CReplayRecorder::Create(u32_t roomID, u32_t tickRate)
{
	return std::unique_ptr<CReplayRecorder>(new CReplayRecorder(roomID, tickRate));
}

void CReplayRecorder::RecordSnapshot(u64_t tick, const u8_t* bytes, u32_t len)
{
	(void)Record(Winters::Replay::eReplayRecordType::Snapshot, tick, bytes, len);
}

void CReplayRecorder::RecordEvent(u64_t tick, const u8_t* bytes, u32_t len)
{
	(void)Record(Winters::Replay::eReplayRecordType::Event, tick, bytes, len);
}

bool_t CReplayRecorder::RecordCommand(
	u64_t tick, const Winters::Replay::ReplayCommandPayload& payload)
{
	if (!Winters::Replay::IsReplayCommandDomainSupported(payload.reserved0))
		return false;

	return Record(Winters::Replay::eReplayRecordType::Command, tick,
		reinterpret_cast<const u8_t*>(&payload),
		static_cast<u32_t>(sizeof(payload)));
}

bool_t CReplayRecorder::ShouldRecordSnapshot(
	u64_t tick,
	u64_t toolRevision,
	u64_t lastTick,
	u64_t lastToolRevision)
{
	return tick != lastTick || toolRevision != lastToolRevision;
}

bool_t CReplayRecorder::EnsureSpoolOpen()
{
	if (m_spoolStream.is_open())
		return true;
	if (m_bWriteFailed || m_bSealed || m_bFinalized)
		return false;

	try
	{
		m_spoolPath = MakeSpoolPath(m_iRoomID);
		m_spoolStream.open(
			m_spoolPath,
			std::ios::binary | std::ios::trunc);
		if (!m_spoolStream)
		{
			SetWriteFailure("failed to open replay spool file");
			return false;
		}

		const Winters::Replay::ReplayFileHeader placeholder{};
		m_spoolStream.write(
			reinterpret_cast<const char*>(&placeholder),
			sizeof(placeholder));
		if (!m_spoolStream.good())
		{
			SetWriteFailure("failed to initialize replay spool file");
			return false;
		}
		m_iSpoolBytes = sizeof(placeholder);
		return true;
	}
	catch (const std::exception& e)
	{
		SetWriteFailure(e.what());
		return false;
	}
}

void CReplayRecorder::SetWriteFailure(const std::string& error)
{
	if (m_bWriteFailed)
		return;
	m_bWriteFailed = true;
	m_strWriteError = error.empty() ? "unknown replay spool failure" : error;
}

bool_t CReplayRecorder::Record(Winters::Replay::eReplayRecordType type,
	u64_t tick, const u8_t* bytes, u32_t len)
{
	if (!bytes || len == 0u || !EnsureSpoolOpen())
		return false;

	Winters::Replay::ReplayRecordHeader header{};
	header.type = static_cast<u8_t>(type);
	header.payloadSize = len;
	header.serverTick = tick;
	header.sequence = static_cast<u32_t>(tick & 0xFFFFFFFFull);
	m_spoolStream.write(reinterpret_cast<const char*>(&header), sizeof(header));
	m_spoolStream.write(
		reinterpret_cast<const char*>(bytes),
		static_cast<std::streamsize>(len));
	if (!m_spoolStream.good())
	{
		SetWriteFailure("failed to append replay spool record");
		return false;
	}

	if (m_iRecordCount == 0u)
		m_iFirstTick = tick;
	m_iLastTick = tick;
	++m_iRecordCount;
	m_iPayloadBytes += len;
	m_iSpoolBytes += sizeof(header) + static_cast<u64_t>(len);

	if (type == Winters::Replay::eReplayRecordType::Snapshot)
		++m_iSnapshotCount;
	else if (type == Winters::Replay::eReplayRecordType::Event)
		++m_iEventCount;
	else if (type == Winters::Replay::eReplayRecordType::Command)
		++m_iCommandCount;
	return true;
}

bool_t CReplayRecorder::SaveToFile(const wstring_t& path, std::string& outError)
{
	outError.clear();

	if (m_iRecordCount == 0u)
	{
		outError = "no replay records";
		return false;
	}
	if (m_bWriteFailed)
	{
		outError = m_strWriteError;
		return false;
	}
	if (m_bFinalized)
	{
		outError = "replay already finalized";
		return false;
	}

	try
	{
		if (!m_bSealed)
		{
			if (!m_spoolStream.is_open())
			{
				outError = "replay spool is not open";
				return false;
			}

			m_spoolStream.flush();
			if (!m_spoolStream.good())
			{
				SetWriteFailure("failed to flush replay spool file");
				outError = m_strWriteError;
				return false;
			}
			m_spoolStream.close();
			m_bSealed = true;
		}

		Winters::Replay::ReplayFileHeader header{};
		header.recordCount = m_iRecordCount;
		header.snapshotCount = m_iSnapshotCount;
		header.eventCount = m_iEventCount;
		header.firstTick = m_iFirstTick;
		header.lastTick = m_iLastTick;
		header.createdUnixMs = NowUnixMs();

		std::fstream spool(
			m_spoolPath,
			std::ios::binary | std::ios::in | std::ios::out);
		if (!spool)
		{
			outError = "failed to reopen replay spool file";
			return false;
		}
		spool.seekp(0, std::ios::beg);
		spool.write(reinterpret_cast<const char*>(&header), sizeof(header));
		spool.flush();
		if (!spool.good())
		{
			outError = "failed to finalize replay header";
			return false;
		}
		spool.close();

		const std::filesystem::path fsPath(path);
		if (fsPath.has_parent_path())
			std::filesystem::create_directories(fsPath.parent_path());

		const std::filesystem::path publishPath = MakePublishPath(fsPath);
		std::error_code fileError;
		std::filesystem::copy_file(
			m_spoolPath,
			publishPath,
			std::filesystem::copy_options::overwrite_existing,
			fileError);
		if (fileError)
		{
			outError = "failed to stage replay publish: " + fileError.message();
			return false;
		}

		fileError.clear();
		const u64_t publishBytes = static_cast<u64_t>(
			std::filesystem::file_size(publishPath, fileError));
		if (fileError || publishBytes != m_iSpoolBytes)
		{
			std::filesystem::remove(publishPath, fileError);
			outError = "staged replay size mismatch";
			return false;
		}

		if (!MoveFileExW(
			publishPath.c_str(),
			fsPath.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			const DWORD lastError = GetLastError();
			std::filesystem::remove(publishPath, fileError);
			outError = "failed to atomically publish replay: " +
				std::error_code(
					static_cast<int>(lastError),
					std::system_category()).message();
			return false;
		}

		m_bFinalized = true;
		fileError.clear();
		if (std::filesystem::remove(m_spoolPath, fileError))
			m_spoolPath.clear();
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
