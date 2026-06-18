#include "Replay/ReplayPlayer.h"

#include "Network/Client/EventApplier.h"
#include "Network/Client/SnapshotApplier.h"

#include <filesystem>
#include <fstream>
#include <utility>

namespace
{
	constexpr u32_t kMaxReplayPayloadBytes = 16u * 1024u * 1024u;

	bool_t IsValidReplayRecordType(u8_t type)
	{
		return type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Snapshot) ||
			type == static_cast<u8_t>(Winters::Replay::eReplayRecordType::Event);
	}
}

std::unique_ptr<CReplayPlayer> CReplayPlayer::LoadFromFile(
	const wstring_t& path,
	std::string& outError)
{
	outError.clear();

	std::ifstream in(std::filesystem::path(path), std::ios::binary);
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
		player->m_Header.version != Winters::Replay::kReplayVersion ||
		player->m_Header.headerSize != Winters::Replay::kReplayHeaderSize)
	{
		outError = "invalid replay header";
		return nullptr;
	}

	player->m_Records.reserve(player->m_Header.recordCount);
	for (u32_t i = 0; i < player->m_Header.recordCount; ++i)
	{
		ReplayRecord record{};
		in.read(reinterpret_cast<char*>(&record.header), sizeof(record.header));
		if (!in.good())
		{
			outError = "failed to read replay record header";
			return nullptr;
		}

		if (record.header.headerSize != Winters::Replay::kReplayRecordHeaderSize ||
			record.header.payloadSize == 0 ||
			record.header.payloadSize > kMaxReplayPayloadBytes ||
			!IsValidReplayRecordType(record.header.type))
		{
			outError = "invalid replay record";
			return nullptr;
		}

		record.payload.resize(record.header.payloadSize);
		in.read(
			reinterpret_cast<char*>(record.payload.data()),
			static_cast<std::streamsize>(record.payload.size()));
		if (!in.good())
		{
			outError = "failed to read replay payload";
			return nullptr;
		}

		player->m_Records.emplace_back(std::move(record));
	}

	if (player->m_Records.empty())
	{
		outError = "replay has no records";
		return nullptr;
	}

	player->m_iCurrentTick = player->m_Header.firstTick;
	player->m_fPlayheadTick = static_cast<double>(player->m_Header.firstTick);
	player->m_strDisplayName = std::filesystem::path(path).filename().string();
	return player;
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
	if (m_bPaused || m_bFinished || m_Records.empty())
		return false;

	if (dt > 0.f)
		m_fPlayheadTick += static_cast<double>(dt * m_fTickRate * m_fPlaybackRate);

	u64_t targetTick = static_cast<u64_t>(m_fPlayheadTick);
	if (targetTick < m_Header.firstTick)
		targetTick = m_Header.firstTick;

	bool_t bApplied = false;
	while (m_iNextRecord < m_Records.size() &&
		m_Records[m_iNextRecord].header.serverTick <= targetTick)
	{
		const u64_t tick = m_Records[m_iNextRecord].header.serverTick;
		const size_t begin = m_iNextRecord;
		size_t end = begin;

		while (end < m_Records.size() &&
			m_Records[end].header.serverTick == tick)
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

		m_iCurrentTick = tick;
		m_iNextRecord = end;
	}

	if (m_iNextRecord >= m_Records.size())
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
		const ReplayRecord& record = m_Records[i];
		if (static_cast<Winters::Replay::eReplayRecordType>(record.header.type) !=
			Winters::Replay::eReplayRecordType::Snapshot)
		{
			continue;
		}

		snapshotApplier.OnSnapshot(
			world,
			entityMap,
			record.payload.data(),
			static_cast<u32_t>(record.payload.size()));
		bApplied = true;
	}

	for (size_t i = begin; i < end; ++i)
	{
		const ReplayRecord& record = m_Records[i];
		if (static_cast<Winters::Replay::eReplayRecordType>(record.header.type) !=
			Winters::Replay::eReplayRecordType::Event)
		{
			continue;
		}

		eventApplier.OnEvent(
			world,
			entityMap,
			record.payload.data(),
			static_cast<u32_t>(record.payload.size()));
		bApplied = true;
	}

	return bApplied;
}
