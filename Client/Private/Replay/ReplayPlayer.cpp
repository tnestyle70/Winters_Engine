#include "Replay/ReplayPlayer.h"

#include "Network/Client/EventApplier.h"
#include "Network/Client/SnapshotApplier.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/Event_generated.h"
#include "Shared/Schemas/Generated/cpp/Snapshot_generated.h"
#include <flatbuffers/flatbuffers.h>
#pragma pop_macro("max")
#pragma pop_macro("min")

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>

namespace
{
	constexpr u32_t kMaxReplayPayloadBytes = 16u * 1024u * 1024u;
	constexpr u32_t kMaxReplayRecordCount = 4u * 1024u * 1024u;

	bool_t IsReplayFlatBufferPayloadValid(
		Winters::Replay::eReplayRecordType recordType,
		const std::vector<u8_t>& payload)
	{
		flatbuffers::Verifier verifier(payload.data(), payload.size());
		if (recordType == Winters::Replay::eReplayRecordType::Snapshot)
			return Shared::Schema::VerifySnapshotBuffer(verifier);
		if (recordType == Winters::Replay::eReplayRecordType::Event)
			return Shared::Schema::VerifyEventPacketBuffer(verifier);
		return true;
	}
}

std::unique_ptr<CReplayPlayer> CReplayPlayer::LoadFromFile(
	const wstring_t& path,
	std::string& outError)
{
	outError.clear();

	const std::filesystem::path replayPath(path);
	std::error_code fileSizeError;
	const std::uintmax_t fileSize =
		std::filesystem::file_size(replayPath, fileSizeError);
	if (fileSizeError || fileSize < Winters::Replay::kReplayHeaderSize)
	{
		outError = "invalid replay file size";
		return nullptr;
	}

	std::ifstream in(replayPath, std::ios::binary);
	if (!in)
	{
		outError = "failed to open replay file";
		return nullptr;
	}

	auto player = std::unique_ptr<CReplayPlayer>(new CReplayPlayer());
	in.read(reinterpret_cast<char*>(&player->m_Header), sizeof(player->m_Header));
	if (!in.good())
	{
		outError = "failed to read replay header";
		return nullptr;
	}

	if (!Winters::Replay::IsReplayMagic(player->m_Header) ||
		!Winters::Replay::IsSupportedReplayVersion(player->m_Header.version) ||
		player->m_Header.headerSize != Winters::Replay::kReplayHeaderSize)
	{
		outError = "invalid replay header";
		return nullptr;
	}
	const std::uintmax_t minimumRecordBytes =
		static_cast<std::uintmax_t>(Winters::Replay::kReplayRecordHeaderSize) + 1u;
	const std::uintmax_t maximumRecordsFromFile =
		(fileSize - Winters::Replay::kReplayHeaderSize) / minimumRecordBytes;
	if (player->m_Header.recordCount == 0u ||
		player->m_Header.recordCount > kMaxReplayRecordCount ||
		player->m_Header.recordCount > maximumRecordsFromFile ||
		player->m_Header.snapshotCount > player->m_Header.recordCount ||
		player->m_Header.eventCount > player->m_Header.recordCount ||
		player->m_Header.snapshotCount + player->m_Header.eventCount >
			player->m_Header.recordCount)
	{
		outError = "invalid replay record counts";
		return nullptr;
	}

	player->m_RecordIndex.reserve(player->m_Header.recordCount);
	player->m_FullSnapshotRecordIndices.reserve(player->m_Header.snapshotCount);
	u32_t snapshotCount = 0u;
	u32_t eventCount = 0u;
	u64_t previousTick = 0u;
	for (u32_t i = 0; i < player->m_Header.recordCount; ++i)
	{
		ReplayRecordIndex record{};
		in.read(reinterpret_cast<char*>(&record.header), sizeof(record.header));
		if (!in.good())
		{
			outError = "failed to read replay record header";
			return nullptr;
		}

		if (record.header.headerSize != Winters::Replay::kReplayRecordHeaderSize ||
			record.header.payloadSize == 0 ||
			record.header.payloadSize > kMaxReplayPayloadBytes ||
			!Winters::Replay::IsReplayRecordTypeSupported(
				player->m_Header.version,
				record.header.type))
		{
			outError = "invalid replay record";
			return nullptr;
		}

		const std::streampos payloadPosition = in.tellg();
		if (payloadPosition == std::streampos(-1))
		{
			outError = "failed to locate replay payload";
			return nullptr;
		}
		record.payloadOffset = static_cast<u64_t>(
			static_cast<std::streamoff>(payloadPosition));
		player->m_PayloadScratch.resize(record.header.payloadSize);
		in.read(
			reinterpret_cast<char*>(player->m_PayloadScratch.data()),
			static_cast<std::streamsize>(player->m_PayloadScratch.size()));
		if (!in.good())
		{
			outError = "failed to read replay payload";
			return nullptr;
		}
		if (i > 0u && record.header.serverTick < previousTick)
		{
			outError = "non-monotonic replay requires branch-aware playback";
			return nullptr;
		}
		previousTick = record.header.serverTick;

		const auto recordType =
			static_cast<Winters::Replay::eReplayRecordType>(record.header.type);
		if (recordType == Winters::Replay::eReplayRecordType::Command &&
			!Winters::Replay::IsReplayCommandPayloadSupported(
				player->m_Header.version,
				player->m_PayloadScratch.data(),
				record.header.payloadSize))
		{
			outError = "invalid replay command payload";
			return nullptr;
		}
		if (!IsReplayFlatBufferPayloadValid(
			recordType, player->m_PayloadScratch))
		{
			outError = recordType == Winters::Replay::eReplayRecordType::Snapshot
				? "invalid replay snapshot payload"
				: "invalid replay event payload";
			return nullptr;
		}

		if (recordType == Winters::Replay::eReplayRecordType::Snapshot)
		{
			++snapshotCount;
			const auto* snapshot = Shared::Schema::GetSnapshot(
				player->m_PayloadScratch.data());
			if (snapshot && snapshot->deltaBaseTick() == 0u)
			{
				player->m_FullSnapshotRecordIndices.push_back(
					player->m_RecordIndex.size());
			}
		}
		else if (recordType == Winters::Replay::eReplayRecordType::Event)
			++eventCount;

		player->m_RecordIndex.emplace_back(record);
	}

	if (player->m_RecordIndex.empty())
	{
		outError = "replay has no records";
		return nullptr;
	}
	if (in.peek() != std::char_traits<char>::eof())
	{
		outError = "replay has trailing bytes";
		return nullptr;
	}
	if (snapshotCount != player->m_Header.snapshotCount ||
		eventCount != player->m_Header.eventCount ||
		player->m_RecordIndex.front().header.serverTick != player->m_Header.firstTick ||
		player->m_RecordIndex.back().header.serverTick != player->m_Header.lastTick)
	{
		outError = "replay header summary mismatch";
		return nullptr;
	}
	if (player->m_FullSnapshotRecordIndices.empty())
	{
		outError = "replay has no full snapshots";
		return nullptr;
	}

	const size_t firstSnapshotIndex =
		player->m_FullSnapshotRecordIndices.front();
	const u64_t firstSnapshotTick =
		player->m_RecordIndex[firstSnapshotIndex].header.serverTick;
	player->m_iNextRecord = firstSnapshotIndex;
	while (player->m_iNextRecord > 0u &&
		player->m_RecordIndex[player->m_iNextRecord - 1u].header.serverTick ==
			firstSnapshotTick)
	{
		--player->m_iNextRecord;
	}
	player->m_iCurrentTick = firstSnapshotTick;
	player->m_fPlayheadTick = static_cast<double>(firstSnapshotTick);
	player->m_strDisplayName = std::filesystem::path(path).filename().string();
	player->m_PayloadScratch.clear();
	player->m_Stream = std::move(in);
	player->m_Stream.clear();
	return player;
}

bool_t CReplayPlayer::SeekToTick(
	u64_t targetTick,
	CWorld& world,
	EntityIdMap& entityMap,
	CSnapshotApplier& snapshotApplier,
	CEventApplier& eventApplier)
{
	if (m_RecordIndex.empty() || m_FullSnapshotRecordIndices.empty())
	{
		SetPlaybackError("replay seek requires full snapshots");
		return false;
	}

	targetTick = (std::max)(m_Header.firstTick, targetTick);
	targetTick = (std::min)(m_Header.lastTick, targetTick);

	const auto upper = std::upper_bound(
		m_FullSnapshotRecordIndices.begin(),
		m_FullSnapshotRecordIndices.end(),
		targetTick,
		[this](u64_t tick, size_t recordIndex)
		{
			return tick < m_RecordIndex[recordIndex].header.serverTick;
		});

	size_t snapshotIndex = m_FullSnapshotRecordIndices.front();
	if (upper != m_FullSnapshotRecordIndices.begin())
		snapshotIndex = *(upper - 1);
	else
		targetTick = m_RecordIndex[snapshotIndex].header.serverTick;

	size_t begin = snapshotIndex;
	const u64_t snapshotTick = m_RecordIndex[snapshotIndex].header.serverTick;
	while (begin > 0u && m_RecordIndex[begin - 1u].header.serverTick == snapshotTick)
		--begin;

	m_strPlaybackError.clear();
	m_bFinished = false;
	m_iNextRecord = begin;
	m_iCurrentTick = snapshotTick;
	m_fPlayheadTick = static_cast<double>(targetTick);
	snapshotApplier.ResetForReplaySeek(world, entityMap, targetTick);

	bool_t bApplied = false;
	while (m_iNextRecord < m_RecordIndex.size() &&
		m_RecordIndex[m_iNextRecord].header.serverTick <= targetTick)
	{
		const u64_t tick = m_RecordIndex[m_iNextRecord].header.serverTick;
		const size_t groupBegin = m_iNextRecord;
		size_t groupEnd = groupBegin;
		while (groupEnd < m_RecordIndex.size() &&
			m_RecordIndex[groupEnd].header.serverTick == tick)
		{
			++groupEnd;
		}

		bApplied = ApplyTickGroup(
			groupBegin,
			groupEnd,
			world,
			entityMap,
			snapshotApplier,
			eventApplier) || bApplied;
		if (!m_strPlaybackError.empty())
			return false;

		m_iCurrentTick = tick;
		m_iNextRecord = groupEnd;
	}

	m_bFinished = m_iNextRecord >= m_RecordIndex.size();
	return bApplied;
}

void CReplayPlayer::SetPlaybackRate(f32_t rate)
{
	if (rate < 0.1f)
		rate = 0.1f;
	if (rate > 8.f)
		rate = 8.f;
	m_fPlaybackRate = rate;
}

bool_t CReplayPlayer::Update(
	f32_t dt,
	CWorld& world,
	EntityIdMap& entityMap,
	CSnapshotApplier& snapshotApplier,
	CEventApplier& eventApplier)
{
	if (m_bPaused || m_bFinished || m_RecordIndex.empty())
		return false;

	if (dt > 0.f)
		m_fPlayheadTick += static_cast<double>(dt * m_fTickRate * m_fPlaybackRate);

	u64_t targetTick = static_cast<u64_t>(m_fPlayheadTick);
	if (targetTick < m_Header.firstTick)
		targetTick = m_Header.firstTick;

	bool_t bApplied = false;
	while (m_iNextRecord < m_RecordIndex.size() &&
		m_RecordIndex[m_iNextRecord].header.serverTick <= targetTick)
	{
		const u64_t tick = m_RecordIndex[m_iNextRecord].header.serverTick;
		const size_t begin = m_iNextRecord;
		size_t end = begin;

		while (end < m_RecordIndex.size() &&
			m_RecordIndex[end].header.serverTick == tick)
		{
			++end;
		}

		bApplied = ApplyTickGroup(
			begin,
			end,
			world,
			entityMap,
			snapshotApplier,
			eventApplier) || bApplied;
		if (!m_strPlaybackError.empty())
			return bApplied;

		m_iCurrentTick = tick;
		m_iNextRecord = end;
	}

	if (m_iNextRecord >= m_RecordIndex.size())
		m_bFinished = true;

	return bApplied;
}

bool_t CReplayPlayer::ApplyTickGroup(
	size_t begin,
	size_t end,
	CWorld& world,
	EntityIdMap& entityMap,
	CSnapshotApplier& snapshotApplier,
	CEventApplier& eventApplier)
{
	bool_t bApplied = false;

	for (size_t i = begin; i < end; ++i)
	{
		const ReplayRecordIndex& record = m_RecordIndex[i];
		if (static_cast<Winters::Replay::eReplayRecordType>(record.header.type) !=
			Winters::Replay::eReplayRecordType::Snapshot)
		{
			continue;
		}
		if (!ReadPayload(record))
			return bApplied;

		snapshotApplier.OnSnapshot(
			world,
			entityMap,
			m_PayloadScratch.data(),
			static_cast<u32_t>(m_PayloadScratch.size()));
		bApplied = true;
	}

	for (size_t i = begin; i < end; ++i)
	{
		const ReplayRecordIndex& record = m_RecordIndex[i];
		if (static_cast<Winters::Replay::eReplayRecordType>(record.header.type) !=
			Winters::Replay::eReplayRecordType::Event)
		{
			continue;
		}
		if (!ReadPayload(record))
			return bApplied;

		eventApplier.OnEvent(
			world,
			entityMap,
			m_PayloadScratch.data(),
			static_cast<u32_t>(m_PayloadScratch.size()));
		bApplied = true;
	}

	return bApplied;
}

bool_t CReplayPlayer::ReadPayload(const ReplayRecordIndex& record)
{
	m_Stream.clear();
	m_Stream.seekg(static_cast<std::streamoff>(record.payloadOffset), std::ios::beg);
	if (!m_Stream.good())
	{
		SetPlaybackError("failed to seek replay payload");
		return false;
	}

	m_PayloadScratch.resize(record.header.payloadSize);
	m_Stream.read(
		reinterpret_cast<char*>(m_PayloadScratch.data()),
		static_cast<std::streamsize>(m_PayloadScratch.size()));
	if (!m_Stream.good())
	{
		SetPlaybackError("failed to read replay payload");
		return false;
	}

	const auto recordType =
		static_cast<Winters::Replay::eReplayRecordType>(record.header.type);
	if (!IsReplayFlatBufferPayloadValid(recordType, m_PayloadScratch))
	{
		SetPlaybackError(recordType == Winters::Replay::eReplayRecordType::Snapshot
			? "replay snapshot changed after validation"
			: "replay event changed after validation");
		return false;
	}
	return true;
}

void CReplayPlayer::SetPlaybackError(const char* error)
{
	m_strPlaybackError = error ? error : "replay playback failed";
	m_bFinished = true;
}
