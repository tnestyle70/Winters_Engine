#include "Replay/ReplayLibrary.h"

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

namespace
{
	std::filesystem::path ResolveLocalAppDataRoot()
	{
		PWSTR pKnownFolder = nullptr;
		const HRESULT result = SHGetKnownFolderPath(
			FOLDERID_LocalAppData,
			KF_FLAG_DEFAULT,
			nullptr,
			&pKnownFolder);
		if (FAILED(result) || !pKnownFolder)
			return {};

		const std::filesystem::path root(pKnownFolder);
		CoTaskMemFree(pKnownFolder);
		return root / L"Winters";
	}

	std::vector<ReplayListItem> ListReplayFiles(
		const std::filesystem::path& replayDir,
		bool_t bLocalDebug)
	{
		std::vector<ReplayListItem> items;

		std::error_code ec;
		if (replayDir.empty() || !std::filesystem::exists(replayDir, ec))
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
			item.bLocalDebug = bLocalDebug;

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
}

bool_t CReplayLibrary::IsSafeAccountKey(const std::string& strUserID)
{
	if (strUserID.empty() || strUserID.size() > 64u)
		return false;

	return std::all_of(strUserID.begin(), strUserID.end(),
		[](unsigned char value)
		{
			return std::isalnum(value) != 0 || value == '-' || value == '_';
		});
}

wstring_t CReplayLibrary::GetAccountDataDirectory(const std::string& strUserID)
{
	if (!IsSafeAccountKey(strUserID))
		return {};

	const std::filesystem::path root = ResolveLocalAppDataRoot();
	if (root.empty())
		return {};

	return (root / L"Accounts" / std::filesystem::path(strUserID)).wstring();
}

wstring_t CReplayLibrary::GetAccountReplayCacheDirectory(
	const std::string& strUserID)
{
	const std::filesystem::path accountDir(GetAccountDataDirectory(strUserID));
	if (accountDir.empty())
		return {};
	return (accountDir / L"ReplayCache").wstring();
}

wstring_t CReplayLibrary::GetLocalDebugReplayDirectory()
{
	return L"Replay";
}

std::vector<ReplayListItem> CReplayLibrary::ListAccountReplayCache(
	const std::string& strUserID)
{
	return ListReplayFiles(GetAccountReplayCacheDirectory(strUserID), false);
}

std::vector<ReplayListItem> CReplayLibrary::ListLocalDebugReplays()
{
	return ListReplayFiles(GetLocalDebugReplayDirectory(), true);
}

std::vector<ReplayListItem> CReplayLibrary::ListLocalReplays()
{
	return ListLocalDebugReplays();
}
