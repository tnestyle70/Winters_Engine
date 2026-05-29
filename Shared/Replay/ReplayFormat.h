#pragma once

#include "WintersTypes.h"

#include <cstring>

namespace Winters::Replay
{
	inline constexpr char kReplayMagic[4] = { 'W', 'R', 'P', 'L' };
	inline constexpr u16_t kReplayVersion = 1;
	inline constexpr u16_t kReplayHeaderSize = 48;
	inline constexpr u16_t kReplayRecordHeaderSize = 24;

#pragma pack(push, 1)
	struct ReplayFileHeader
	{
		char magic[4] = { 'W', 'R', 'P', 'L' };
		u16_t version = kReplayVersion;
		u16_t headerSize = kReplayHeaderSize;
		u32_t flags = 0;
		u32_t recordCount = 0;
		u32_t snapshotCount = 0;
		u32_t eventCount = 0;
		u64_t firstTick = 0;
		u64_t lastTick = 0;
		u64_t createdUnixMs = 0;
	};

	enum class eReplayRecordType : u8_t
	{
		Snapshot = 1,
		Event = 2,
	};

	struct ReplayRecordHeader
	{
		u8_t type = 0;
		u8_t reserved0 = 0;
		u16_t headerSize = kReplayRecordHeaderSize;
		u32_t payloadSize = 0;
		u64_t serverTick = 0;
		u32_t sequence = 0;
		u32_t reserved1 = 0;
	};
#pragma pack(pop)
	//바로 컴파일 에러 띄우는 static_assert?
	static_assert(sizeof(ReplayFileHeader) == kReplayHeaderSize);
	static_assert(sizeof(ReplayRecordHeader) == kReplayRecordHeaderSize);

	inline bool_t IsReplayMagic(const ReplayFileHeader& header)
	{
		return std::memcmp(header.magic, kReplayMagic, sizeof(kReplayMagic))== 0;
	}
}