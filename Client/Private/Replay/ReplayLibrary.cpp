#include "Replay/ReplayLibrary.h"

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
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
			if (!bLocalDebug)
			{
				CReplayLibrary::ReadReplayPerspectiveMetadata(
					item.path,
					item.perspectiveNetId);
			}
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

eReplayPerspectiveMetadataStatus CReplayLibrary::ReadReplayPerspectiveMetadata(
	const wstring_t& replayPath,
	u32_t& outPerspectiveNetId)
{
	outPerspectiveNetId = 0u;
	if (replayPath.empty())
		return eReplayPerspectiveMetadataStatus::Invalid;

	const std::filesystem::path sidecar = replayPath + L".perspective";
	std::error_code ec;
	const bool_t exists = std::filesystem::exists(sidecar, ec);
	if (ec)
		return eReplayPerspectiveMetadataStatus::Invalid;
	if (!exists)
		return eReplayPerspectiveMetadataStatus::Missing;

	std::ifstream input(sidecar, std::ios::binary);
	u64_t value = 0u;
	if (!(input >> value) || value == 0u ||
		value > (std::numeric_limits<u32_t>::max)())
	{
		return eReplayPerspectiveMetadataStatus::Invalid;
	}
	input >> std::ws;
	if (!input.eof())
		return eReplayPerspectiveMetadataStatus::Invalid;

	outPerspectiveNetId = static_cast<u32_t>(value);
	return eReplayPerspectiveMetadataStatus::Valid;
}

bool_t CReplayLibrary::PublishAccountReplayCache(
	const wstring_t& temporaryReplayPath,
	const wstring_t& finalReplayPath,
	u32_t perspectiveNetId,
	std::string& outError)
{
	outError.clear();
	const std::filesystem::path temporaryReplay(temporaryReplayPath);
	const std::filesystem::path finalReplay(finalReplayPath);
	if (perspectiveNetId == 0u || temporaryReplay.empty() ||
		finalReplay.empty() || temporaryReplay == finalReplay)
	{
		outError = "invalid replay cache publish metadata";
		return false;
	}

	std::error_code ec;
	if (!std::filesystem::is_regular_file(temporaryReplay, ec) || ec)
	{
		outError = "temporary replay cache file is unavailable";
		return false;
	}
	const bool_t finalReplayExisted = std::filesystem::exists(finalReplay, ec);
	if (ec)
	{
		outError = "replay cache destination is unavailable";
		return false;
	}

	const std::filesystem::path sidecar =
		finalReplay.wstring() + L".perspective";
	const std::filesystem::path temporarySidecar =
		sidecar.wstring() + L".part";
	std::filesystem::remove(temporarySidecar, ec);
	ec.clear();
	std::ofstream output(
		temporarySidecar,
		std::ios::binary | std::ios::trunc);
	output << perspectiveNetId << '\n';
	output.flush();
	const bool_t outputGood = output.good();
	output.close();
	if (!outputGood || !MoveFileExW(
		temporarySidecar.c_str(),
		sidecar.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		std::filesystem::remove(temporarySidecar, ec);
		outError = "replay perspective metadata write failed";
		return false;
	}

	if (!MoveFileExW(
		temporaryReplay.c_str(),
		finalReplay.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
	{
		if (!finalReplayExisted)
			std::filesystem::remove(sidecar, ec);
		outError = "downloaded replay cache publish failed";
		return false;
	}
	return true;
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
