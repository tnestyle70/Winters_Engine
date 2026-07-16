#include "Network/Backend/ReplayClient.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

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
}

int main()
{
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
			client->DownloadMine(
				*selected,
				userID,
				[&](const Client::ReplayDownloadResult& result)
				{
					downloadCompleted = true;
					success = result.success;
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

	if (!listCompleted || !downloadCompleted || !success)
	{
		std::cerr << "replay_client_smoke=fail reason="
			<< (error.empty() ? "timeout" : error) << "\n";
		return 1;
	}
	std::wcout << L"replay_client_smoke=pass local_path=" << localPath << L"\n";
	return 0;
}
