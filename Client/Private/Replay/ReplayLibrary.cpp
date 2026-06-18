#include "Replay/ReplayLibrary.h"

#include <algorithm>
#include <filesystem>
#include <utility>

wstring_t CReplayLibrary::GetReplayDirectory()
{
	return L"Replay";
}

std::vector<ReplayListItem> CReplayLibrary::ListLocalReplays()
{
	std::vector<ReplayListItem> items;

	std::error_code ec;
	const std::filesystem::path replayDir(GetReplayDirectory());
	if (!std::filesystem::exists(replayDir, ec))
		return items;

	for (const auto& entry : std::filesystem::directory_iterator(replayDir, ec))
	{
		if (ec || !entry.is_regular_file())
			continue;

		const auto path = entry.path();
		if (path.extension() != L".wrpl")
			continue;

		ReplayListItem item{};
		item.path = path.wstring();
		item.displayName = path.filename().string();

		std::error_code sizeEc;
		item.fileSizeBytes = entry.file_size(sizeEc);
		items.push_back(std::move(item));
	}

	std::sort(items.begin(), items.end(),
		[](const ReplayListItem& lhs, const ReplayListItem& rhs)
		{
			return lhs.displayName < rhs.displayName;
		});

	return items;
}
