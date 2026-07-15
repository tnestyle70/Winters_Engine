#pragma once

#include "WintersTypes.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

namespace Winters::Replay
{
	inline constexpr char kReplayMagic[4] = { 'W', 'R', 'P', 'L' };
	inline constexpr u16_t kReplayMinSupportedVersion = 1;
	inline constexpr u16_t kReplayVersion = 2;
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
		Command = 3,
	};

	// Append-only command-domain values stored in ReplayCommandPayload::reserved0.
	// Existing v2 files wrote zero, so zero remains a supported legacy PlayerInput
	// value. New writers always emit one of the explicit non-zero values.
	enum class eReplayCommandDomain : u16_t
	{
		LegacyV2PlayerInput = 0u,
		PlayerInput = 1u,
		AuthoringMutation = 2u,
		ControlPlane = 3u,
		ObservationOnly = 4u,
	};

	// Journal disposition is intentionally separate from domain. Normal gameplay
	// input is journaled before deterministic gameplay validation so the same
	// accept/reject result can be reproduced. Rejected tool commands and gameplay
	// input consumed as void while paused never enter faithful re-simulation.
	enum class eReplayJournalOutcome : u8_t
	{
		SubmittedPlayerInput = 0u,
		AcceptedToolCommand = 1u,
		RejectedToolCommand = 2u,
		PausedGameplayVoid = 3u,
	};

	// v2 Command record payload: one externally-issued session command,
	// keyed by the record header's serverTick (= execution tick).
	// Bot AI commands are NOT journaled - they regenerate deterministically.
	struct ReplayCommandPayload
	{
		u32_t sourceSessionId = 0;
		u32_t sequenceNum = 0;
		u8_t kind = 0;
		u8_t slot = 0;
		u16_t itemId = 0;
		u32_t targetNetId = 0;
		f32_t groundPos[3]{};
		f32_t direction[3]{};
		u16_t practiceOperation = 0;
		u16_t reserved0 = 0;
		f32_t practiceValue = 0.f;
		u32_t practiceFlags = 0;
		u64_t clientTick = 0;
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
	static_assert(sizeof(ReplayCommandPayload) == 60, "ReplayCommandPayload size fixed");
	static_assert(std::is_standard_layout_v<ReplayCommandPayload>);
	static_assert(std::is_trivially_copyable_v<ReplayCommandPayload>);
	static_assert(offsetof(ReplayCommandPayload, reserved0) == 42u,
		"ReplayCommandPayload reserved0/domain ABI offset changed");
	static_assert(offsetof(ReplayCommandPayload, clientTick) == 52u,
		"ReplayCommandPayload tail ABI offset changed");

	inline bool_t IsReplayCommandDomainSupported(u16_t rawDomain)
	{
		return rawDomain <= static_cast<u16_t>(
			eReplayCommandDomain::ObservationOnly);
	}

	inline bool_t IsReplayCommandPayloadSupported(
		u16_t replayVersion,
		const void* pPayload,
		u32_t payloadSize)
	{
		if (replayVersion < 2u ||
			!pPayload ||
			payloadSize != sizeof(ReplayCommandPayload))
		{
			return false;
		}

		ReplayCommandPayload payload{};
		std::memcpy(&payload, pPayload, sizeof(payload));
		return IsReplayCommandDomainSupported(payload.reserved0);
	}

	inline eReplayCommandDomain GetReplayCommandDomain(
		const ReplayCommandPayload& payload)
	{
		return static_cast<eReplayCommandDomain>(payload.reserved0);
	}

	inline void SetReplayCommandDomain(
		ReplayCommandPayload& payload,
		eReplayCommandDomain domain)
	{
		payload.reserved0 = static_cast<u16_t>(domain);
	}

	inline bool_t IsFaithfulResimCommandDomain(eReplayCommandDomain domain)
	{
		return domain == eReplayCommandDomain::LegacyV2PlayerInput ||
			domain == eReplayCommandDomain::PlayerInput ||
			domain == eReplayCommandDomain::AuthoringMutation;
	}

	inline bool_t ShouldJournalReplayCommand(eReplayJournalOutcome outcome)
	{
		return outcome == eReplayJournalOutcome::SubmittedPlayerInput ||
			outcome == eReplayJournalOutcome::AcceptedToolCommand;
	}

	inline bool_t ShouldAdvanceToolRevision(
		eReplayCommandDomain domain,
		eReplayJournalOutcome outcome)
	{
		// Practice/AIDebug authoring and control commands both change authoritative
		// room/tool state. Observation-only requests never advance canonical state.
		return outcome == eReplayJournalOutcome::AcceptedToolCommand &&
			(domain == eReplayCommandDomain::AuthoringMutation ||
				domain == eReplayCommandDomain::ControlPlane);
	}

	inline bool_t IsReplayMagic(const ReplayFileHeader& header)
	{
		return std::memcmp(header.magic, kReplayMagic, sizeof(kReplayMagic))== 0;
	}

	inline bool_t IsSupportedReplayVersion(u16_t version)
	{
		return version >= kReplayMinSupportedVersion &&
			version <= kReplayVersion;
	}

	inline bool_t IsReplayRecordTypeSupported(u16_t version, u8_t type)
	{
		if (type == static_cast<u8_t>(eReplayRecordType::Snapshot) ||
			type == static_cast<u8_t>(eReplayRecordType::Event))
		{
			return true;
		}

		return version >= 2u &&
			type == static_cast<u8_t>(eReplayRecordType::Command);
	}
}
