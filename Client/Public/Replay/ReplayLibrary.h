#pragma once

#include "Defines.h"

#include <string>
#include <vector>

struct ReplayListItem
{
	wstring_t path;
	std::string displayName;
	uint64_t fileSizeBytes = 0;
};

class CReplayLibrary final
{
public:
	static wstring_t GetReplayDirectory();
	static std::vector<ReplayListItem> ListLocalReplays();
};