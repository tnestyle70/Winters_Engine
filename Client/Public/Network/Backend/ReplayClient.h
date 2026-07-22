#pragma once

#include "Defines.h"
#include "Network/Backend/CHttpClient.h"

#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

NS_BEGIN(Client)

struct CloudReplayItem
{
	string replayId;
	string matchId;
	string status;
	u64_t sizeBytes = 0u;
	string checksumSha256;
	i32_t formatVersion = 0;
	i32_t tickRate = 0;
	u64_t recordCount = 0u;
	u32_t perspectiveNetId = 0u;
	string createdAt;
	bool_t downloaded = false;
};

struct ReplayPageResult
{
	vector<CloudReplayItem> items;
	string nextCursor;
	string error;
};

struct ReplayDownloadResult
{
	bool_t success = false;
	string replayId;
	wstring_t localPath;
	u32_t perspectiveNetId = 0u;
	string error;
};

using ReplayPageCallback = function<void(const ReplayPageResult&)>;
using ReplayDownloadCallback = function<void(const ReplayDownloadResult&)>;

class CReplayClient final
{
public:
	~CReplayClient();

	static unique_ptr<CReplayClient> Create(const string& baseURL);
	void SetAuthToken(const string& token);
	void ListMine(ReplayPageCallback callback);
	void DownloadMine(
		const CloudReplayItem& item,
		const string& accountUserID,
		ReplayDownloadCallback callback);
	void ProcessCallbacks();

private:
	CReplayClient() = default;
	void LaunchDownload(
		CloudReplayItem item,
		string accountUserID,
		string presignedURL,
		ReplayDownloadCallback callback);
	void PruneCompletedDownloads();

	unique_ptr<CHttpClient> m_pHttp;
	mutex m_CallbackMutex;
	queue<function<void()>> m_PendingCallbacks;
	mutex m_RequestMutex;
	vector<future<void>> m_PendingRequests;
};

NS_END
