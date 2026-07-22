#pragma once

#include "Defines.h"

#include <string>
#include <vector>

struct ReplayListItem
{
	wstring_t path;
	std::string displayName;
	uint64_t fileSizeBytes = 0;
	bool_t bLocalDebug = false;
	u32_t perspectiveNetId = 0u;
};

enum class eReplayPerspectiveMetadataStatus : u8_t
{
	Missing,
	Valid,
	Invalid,
};

class CReplayLibrary final
{
public:
	static bool_t IsSafeAccountKey(const std::string& strUserID);
	static wstring_t GetAccountDataDirectory(const std::string& strUserID);
	static wstring_t GetAccountReplayCacheDirectory(const std::string& strUserID);
	static wstring_t GetLocalDebugReplayDirectory();

	static std::vector<ReplayListItem> ListAccountReplayCache(
		const std::string& strUserID);
	static std::vector<ReplayListItem> ListLocalDebugReplays();
	static eReplayPerspectiveMetadataStatus ReadReplayPerspectiveMetadata(
		const wstring_t& replayPath,
		u32_t& outPerspectiveNetId);
	static bool_t PublishAccountReplayCache(
		const wstring_t& temporaryReplayPath,
		const wstring_t& finalReplayPath,
		u32_t perspectiveNetId,
		std::string& outError);

	// CustomMode의 명시적 로컬 리플레이 선택 경로를 위한 호환 별칭.
	static std::vector<ReplayListItem> ListLocalReplays();
};
