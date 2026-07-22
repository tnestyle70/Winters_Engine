#include "Network/Backend/ReplayClient.h"
#include "Replay/ReplayLibrary.h"

#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
	bool ReadEnvironment(const char* name, std::string& outValue)
	{
		char* value = nullptr;
		size_t length = 0u;
		if (_dupenv_s(&value, &length, name) != 0 ||
			!value || length <= 1u)
		{
			std::free(value);
			outValue.clear();
			return false;
		}
		outValue.assign(value, length - 1u);
		std::free(value);
		return true;
	}

	const ReplayListItem* FindReplay(
		const std::vector<ReplayListItem>& items,
		const wstring_t& path)
	{
		for (const ReplayListItem& item : items)
		{
			if (item.path == path)
				return &item;
		}
		return nullptr;
	}

	bool RunPerspectiveFileContractProbe(std::string& outError)
	{
		const std::string accountUserID =
			"replay-smoke-" + std::to_string(GetCurrentProcessId());
		const std::filesystem::path accountDirectory(
			CReplayLibrary::GetAccountDataDirectory(accountUserID));
		const std::filesystem::path cacheDirectory(
			CReplayLibrary::GetAccountReplayCacheDirectory(accountUserID));
		if (accountDirectory.empty() || cacheDirectory.empty())
		{
			outError = "probe account directory is unavailable";
			return false;
		}

		std::error_code ec;
		std::filesystem::remove_all(accountDirectory, ec);
		std::filesystem::create_directories(cacheDirectory, ec);
		if (ec)
		{
			outError = "probe cache directory create failed";
			return false;
		}
		auto cleanup = [&]()
		{
			std::error_code ignored;
			std::filesystem::remove_all(accountDirectory, ignored);
		};
		auto fail = [&](const char* message)
		{
			outError = message;
			cleanup();
			return false;
		};
		auto writeText = [&](const std::filesystem::path& path,
			const char* text)
		{
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output << text;
			output.flush();
			return output.good();
		};

		const std::filesystem::path validFinal = cacheDirectory / L"valid.wrpl";
		const std::filesystem::path validPart = validFinal.wstring() + L".part";
		if (!writeText(validPart, "WRPL-probe"))
			return fail("probe replay write failed");
		std::string publishError;
		if (!CReplayLibrary::PublishAccountReplayCache(
			validPart.wstring(), validFinal.wstring(), 7u, publishError))
		{
			outError = publishError;
			cleanup();
			return false;
		}
		u32_t perspectiveNetId = 0u;
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			validFinal.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Valid || perspectiveNetId != 7u)
		{
			return fail("valid perspective metadata was not preserved");
		}

		const std::filesystem::path missing = cacheDirectory / L"missing.wrpl";
		const std::filesystem::path malformed = cacheDirectory / L"malformed.wrpl";
		const std::filesystem::path overflow = cacheDirectory / L"overflow.wrpl";
		if (!writeText(missing, "WRPL-probe") ||
			!writeText(malformed, "WRPL-probe") ||
			!writeText(overflow, "WRPL-probe") ||
			!writeText(malformed.wstring() + L".perspective", "broken\n") ||
			!writeText(overflow.wstring() + L".perspective", "4294967296\n"))
		{
			return fail("invalid metadata fixture write failed");
		}
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			missing.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Missing || perspectiveNetId != 0u)
		{
			return fail("missing metadata did not fail closed");
		}
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			malformed.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Invalid || perspectiveNetId != 0u)
		{
			return fail("malformed metadata did not fail closed");
		}
		if (CReplayLibrary::ReadReplayPerspectiveMetadata(
			overflow.wstring(), perspectiveNetId) !=
			eReplayPerspectiveMetadataStatus::Invalid || perspectiveNetId != 0u)
		{
			return fail("overflow metadata did not fail closed");
		}

		const std::vector<ReplayListItem> items =
			CReplayLibrary::ListAccountReplayCache(accountUserID);
		const ReplayListItem* validItem = FindReplay(items, validFinal.wstring());
		const ReplayListItem* missingItem = FindReplay(items, missing.wstring());
		const ReplayListItem* malformedItem = FindReplay(items, malformed.wstring());
		const ReplayListItem* overflowItem = FindReplay(items, overflow.wstring());
		if (!validItem || validItem->perspectiveNetId != 7u ||
			!missingItem || missingItem->perspectiveNetId != 0u ||
			!malformedItem || malformedItem->perspectiveNetId != 0u ||
			!overflowItem || overflowItem->perspectiveNetId != 0u)
		{
			return fail("account replay listing violated perspective contract");
		}

		const std::filesystem::path sidecarFailurePart =
			cacheDirectory / L"sidecar-failure.part";
		const std::filesystem::path unpublished =
			cacheDirectory / L"absent-parent" / L"unpublished.wrpl";
		if (!writeText(sidecarFailurePart, "WRPL-probe") ||
			CReplayLibrary::PublishAccountReplayCache(
				sidecarFailurePart.wstring(),
				unpublished.wstring(), 9u, publishError) ||
			std::filesystem::exists(unpublished, ec))
		{
			return fail("sidecar failure exposed a replay cache file");
		}

		cleanup();
		outError.clear();
		return true;
	}
}

int main(int argc, char** argv)
{
	std::string fileContractError;
	if (!RunPerspectiveFileContractProbe(fileContractError))
	{
		std::cerr << "replay_perspective_contract=fail reason="
			<< fileContractError << "\n";
		return 3;
	}
	if (argc == 2 && std::string(argv[1]) == "--perspective-contract-only")
	{
		std::cout << "replay_perspective_contract=pass\n";
		return 0;
	}

	std::string accessToken;
	std::string userID;
	std::string expectedMatchID;
	std::string serviceURL = "http://127.0.0.1:8087";
	ReadEnvironment("WINTERS_REPLAY_SERVICE_URL", serviceURL);
	if (!ReadEnvironment("WINTERS_REPLAY_SMOKE_ACCESS_TOKEN", accessToken) ||
		!ReadEnvironment("WINTERS_REPLAY_SMOKE_USER_ID", userID))
	{
		std::cerr << "replay_client_smoke=fail reason=missing_environment\n";
		return 2;
	}
	ReadEnvironment("WINTERS_REPLAY_SMOKE_MATCH_ID", expectedMatchID);

	auto client = Client::CReplayClient::Create(serviceURL);
	client->SetAuthToken(accessToken);

	bool listCompleted = false;
	bool downloadCompleted = false;
	bool success = false;
	std::string error;
	wstring_t localPath;
	u32_t expectedPerspectiveNetId = 0u;
	client->ListMine(
		[&](const Client::ReplayPageResult& page)
		{
			listCompleted = true;
			if (!page.error.empty())
			{
				error = page.error;
				downloadCompleted = true;
				return;
			}
			const Client::CloudReplayItem* selected = nullptr;
			for (const Client::CloudReplayItem& item : page.items)
			{
				if (expectedMatchID.empty() || item.matchId == expectedMatchID)
				{
					selected = &item;
					break;
				}
			}
			if (!selected)
			{
				error = "expected replay is not visible";
				downloadCompleted = true;
				return;
			}
			if (selected->perspectiveNetId == 0u)
			{
				error = "cloud replay perspective is unavailable";
				downloadCompleted = true;
				return;
			}
			expectedPerspectiveNetId = selected->perspectiveNetId;
			client->DownloadMine(
				*selected,
				userID,
				[&](const Client::ReplayDownloadResult& result)
				{
					downloadCompleted = true;
					success = result.success &&
						result.perspectiveNetId == expectedPerspectiveNetId;
					error = result.error;
					localPath = result.localPath;
				});
		});

	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::seconds(30);
	while (!downloadCompleted && std::chrono::steady_clock::now() < deadline)
	{
		client->ProcessCallbacks();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	client->ProcessCallbacks();
	if (success)
	{
		const std::vector<ReplayListItem> cached =
			CReplayLibrary::ListAccountReplayCache(userID);
		const ReplayListItem* cachedItem = FindReplay(cached, localPath);
		if (!cachedItem ||
			cachedItem->perspectiveNetId != expectedPerspectiveNetId)
		{
			success = false;
			error = "cloud and account-cache perspectives differ";
		}
	}

	if (!listCompleted || !downloadCompleted || !success)
	{
		std::cerr << "replay_client_smoke=fail reason="
			<< (error.empty() ? "timeout" : error) << "\n";
		return 1;
	}
	std::wcout << L"replay_client_smoke=pass local_path=" << localPath << L"\n";
	return 0;
}
