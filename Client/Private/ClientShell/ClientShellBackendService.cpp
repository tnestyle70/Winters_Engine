#include "ClientShell/ClientShellBackendService.h"

#include "ClientShell/ClientShellDataStore.h"

#include <cstdlib>
#include <utility>

namespace
{
	constexpr const char* PROFILE_SERVICE_URL = "http://127.0.0.1:8084";
	constexpr const char* SHOP_SERVICE_URL = "http://127.0.0.1:8086";
	constexpr const char* MATCH_SERVICE_URL = "http://127.0.0.1:8083";
	constexpr const char* REPLAY_SERVICE_URL = "http://127.0.0.1:8087";

	std::string ReadServiceURL(const char* variable, const char* fallback)
	{
		char* value = nullptr;
		size_t length = 0u;
		if (_dupenv_s(&value, &length, variable) != 0 ||
			!value || length <= 1u)
		{
			std::free(value);
			return fallback;
		}
		std::string result(value, length - 1u);
		std::free(value);
		return result;
	}
}

CClientShellBackendService& CClientShellBackendService::Instance()
{
	static CClientShellBackendService s_Instance;
	return s_Instance;
}

void CClientShellBackendService::ConfigureFromSession(const CClientShellSession& session)
{
	Reset();
	if (HasInFlightRequests())
		return;

	if (session.IsOfflineAccount() || session.GetAccessToken().empty())
	{
		m_strStatus = "Offline shell data";
		return;
	}

	m_pProfileClient = Client::CProfileClient::Create(PROFILE_SERVICE_URL);
	m_pShopClient = Client::CShopClient::Create(SHOP_SERVICE_URL);
	m_pMatchClient = Client::CMatchClient::Create(MATCH_SERVICE_URL);
	m_pReplayClient = Client::CReplayClient::Create(
		ReadServiceURL("WINTERS_REPLAY_SERVICE_URL", REPLAY_SERVICE_URL));
	m_pProfileClient->SetAuthToken(session.GetAccessToken());
	m_pShopClient->SetAuthToken(session.GetAccessToken());
	m_pMatchClient->SetAuthToken(session.GetAccessToken());
	m_pReplayClient->SetAuthToken(session.GetAccessToken());

	m_strUserID = session.GetUserID();
	m_bConfigured = true;
	m_bInitialSyncRequested = false;
	m_vCloudReplayItems.clear();
	++m_uReplayLibraryRevision;
	m_strStatus = "Backend shell ready";
}

void CClientShellBackendService::Reset()
{
	++m_uGeneration;
	m_bConfigured = false;
	m_bInitialSyncRequested = false;
	m_strUserID.clear();

	if (HasInFlightRequests())
	{
		m_bResetAfterCallbacks = true;
		m_strStatus = "Waiting for pending shell requests";
		return;
	}

	DestroyClients();
	m_strStatus.clear();
}

void CClientShellBackendService::ProcessCallbacks()
{
	if (m_pProfileClient)
		m_pProfileClient->ProcessCallbacks();
	if (m_pShopClient)
		m_pShopClient->ProcessCallbacks();
	if (m_pMatchClient)
		m_pMatchClient->ProcessCallbacks();
	if (m_pReplayClient)
		m_pReplayClient->ProcessCallbacks();

	TryFinishDeferredReset();
	TryStartPostMatchRefresh();
	if (m_bMatchHistoryRefreshPending && !m_bProfileRequestInFlight)
		RequestMatchHistory();
}

void CClientShellBackendService::RequestInitialSync()
{
	if (!m_bConfigured || m_bInitialSyncRequested)
		return;

	m_bInitialSyncRequested = true;
	RequestProfileSync();
	RequestStorefrontSync();
}

void CClientShellBackendService::RequestProfileSync()
{
	if (!m_bConfigured || !m_pProfileClient || m_bProfileRequestInFlight)
		return;

	m_bProfileRequestInFlight = true;
	const u32_t uGeneration = m_uGeneration;
	m_pProfileClient->GetMyProfile(
		[this, uGeneration](const Client::ProfileData& profile)
			{
				m_bProfileRequestInFlight = false;
				if (uGeneration == m_uGeneration)
				{
					CClientShellDataStore::Instance().ApplyProfileData(profile);
					m_strStatus = profile.error.empty()
						? "Profile synced"
						: "Profile sync failed: " + profile.error;
				}
				TryFinishDeferredReset();
			});
}

void CClientShellBackendService::RequestStorefrontSync()
{
	if (!m_bConfigured || !m_pShopClient || m_bStoreRequestInFlight)
		return;

	m_bStoreRequestInFlight = true;
	const u32_t uGeneration = m_uGeneration;
	m_pShopClient->GetStorefront(
		[this, uGeneration](const Client::StorefrontData& storefront)
		{
			m_bStoreRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (storefront.success)
				{
					CClientShellDataStore::Instance().ApplyStorefront(storefront);
					m_strStatus = "Storefront synced";
				}
				else
				{
					m_strStatus = storefront.error.empty()
						? "Storefront sync failed"
						: "Storefront sync failed: " + storefront.error;
				}
			}
			TryFinishDeferredReset();
		});
}

void CClientShellBackendService::RequestMatchHistory()
{
	if (!m_bConfigured || !m_pProfileClient)
		return;
	if (m_bProfileRequestInFlight)
	{
		m_bMatchHistoryRefreshPending = true;
		return;
	}

	m_bMatchHistoryRefreshPending = false;
	const u32_t uGeneration = m_uGeneration;
	m_bProfileRequestInFlight = true;
	m_pProfileClient->GetMyHistory(
		[this, uGeneration](const std::vector<Client::MatchRecord>& records)
		{
			m_bProfileRequestInFlight = false;
			if (uGeneration == m_uGeneration)
				CClientShellDataStore::Instance().ApplyMatchHistory(records);
			TryFinishDeferredReset();
		});
}

void CClientShellBackendService::RequestReplayLibrary()
{
	if (!m_bConfigured || !m_pReplayClient || m_bReplayRequestInFlight)
		return;

	m_bReplayRequestInFlight = true;
	const u32_t uGeneration = m_uGeneration;
	m_pReplayClient->ListMine(
		[this, uGeneration](const Client::ReplayPageResult& result)
		{
			m_bReplayRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (result.error.empty())
				{
					m_vCloudReplayItems = result.items;
					++m_uReplayLibraryRevision;
					m_strStatus = "Cloud replay library synced";
				}
				else
				{
					m_strStatus = "Replay sync failed: " + result.error;
				}
			}
			TryFinishDeferredReset();
		});
}

void CClientShellBackendService::RequestPostMatchRefresh()
{
	if (!m_bConfigured)
		return;

	m_bPostMatchRefreshPending = true;
	TryStartPostMatchRefresh();
}

void CClientShellBackendService::TryStartPostMatchRefresh()
{
	if (!m_bConfigured || HasInFlightRequests())
		return;

	if (m_bPostMatchRefreshPending)
	{
		m_bPostMatchRefreshPending = false;
		RequestProfileSync();
		RequestStorefrontSync();
		RequestReplayLibrary();
		RequestMatchHistory();
		return;
	}
}

void CClientShellBackendService::RequestReplayDownload(
	const Client::CloudReplayItem& item)
{
	BeginReplayDownload(item, false);
}

void CClientShellBackendService::RequestReplayPlayback(
	const Client::CloudReplayItem& item)
{
	BeginReplayDownload(item, true);
}

void CClientShellBackendService::BeginReplayDownload(
	const Client::CloudReplayItem& item,
	bool_t bOpenAfterDownload)
{
	if (!m_bConfigured || !m_pReplayClient || m_bReplayRequestInFlight)
		return;
	if (item.perspectiveNetId == 0u)
	{
		m_strStatus = "Replay account perspective is unavailable";
		return;
	}

	m_bReplayRequestInFlight = true;
	m_strStatus = bOpenAfterDownload
		? "Preparing replay playback..."
		: "Downloading replay...";
	const u32_t uGeneration = m_uGeneration;
	const u32_t uPlaybackIntent = bOpenAfterDownload
		? ++m_uReplayPlaybackIntent
		: m_uReplayPlaybackIntent;
	m_pReplayClient->DownloadMine(
		item,
		m_strUserID,
		[this, uGeneration, uPlaybackIntent, bOpenAfterDownload](
			const Client::ReplayDownloadResult& result)
		{
			m_bReplayRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (result.success)
				{
					++m_uReplayLibraryRevision;
					m_strStatus = "Replay downloaded to account cache";
					if (bOpenAfterDownload &&
						uPlaybackIntent == m_uReplayPlaybackIntent)
					{
						m_strReadyReplayPlaybackPath = result.localPath;
						m_uReadyReplayPerspectiveNetId =
							result.perspectiveNetId;
					}
				}
				else
				{
					m_strStatus = result.error.empty()
						? "Replay download failed"
						: "Replay download failed: " + result.error;
				}
			}
			TryFinishDeferredReset();
		});
}

bool_t CClientShellBackendService::ConsumeReplayPlaybackPath(
	wstring_t& outPath,
	u32_t& outPerspectiveNetId)
{
	if (m_strReadyReplayPlaybackPath.empty())
		return false;
	outPath = std::move(m_strReadyReplayPlaybackPath);
	outPerspectiveNetId = m_uReadyReplayPerspectiveNetId;
	m_strReadyReplayPlaybackPath.clear();
	m_uReadyReplayPerspectiveNetId = 0u;
	return !outPath.empty();
}

void CClientShellBackendService::CancelReplayPlaybackIntent()
{
	++m_uReplayPlaybackIntent;
	m_strReadyReplayPlaybackPath.clear();
	m_uReadyReplayPerspectiveNetId = 0u;
}

void CClientShellBackendService::RequestPurchase(const std::string& itemId)
{
	if (itemId.empty())
		return;

	if (!m_bConfigured || !m_pShopClient)
	{
		CClientShellDataStore::Instance().PurchaseOfflineItem(itemId, m_strStatus);
		return;
	}

	if (m_bPurchaseRequestInFlight)
	{
		m_strStatus = "Purchase already in flight";
		return;
	}

	m_bPurchaseRequestInFlight = true;
	m_strStatus = "Purchasing...";

	const u32_t uGeneration = m_uGeneration;
	m_pShopClient->Purchase(
		itemId,
		[this, uGeneration, itemId](const Client::PurchaseResult& result)
		{
			m_bPurchaseRequestInFlight = false;
			if (uGeneration == m_uGeneration)
				CClientShellDataStore::Instance().ApplyPurchaseResult(itemId, result, m_strStatus);
			TryFinishDeferredReset();
		});
}

void CClientShellBackendService::RequestJoinQueue()
{
	CClientShellSession::Instance().ClearMatchAssignment();
	const std::string strQueueName = CClientShellDataStore::Instance().GetLobbyState().strQueueName.empty()
		? "Queue"
		: CClientShellDataStore::Instance().GetLobbyState().strQueueName;

	if (!m_bConfigured || !m_pMatchClient)
	{
		CClientShellDataStore::Instance().SetLobbySearching(strQueueName);
		CClientShellDataStore::Instance().SetLobbyMatched("offline_match");
		m_strStatus = "Offline match ready";
		return;
	}

	if (m_bMatchRequestInFlight)
	{
		m_strStatus = "Custom lobby request already in flight";
		return;
	}

	CClientShellDataStore::Instance().SetLobbySearching(strQueueName);
	m_bMatchRequestInFlight = true;
	m_strStatus = "Joining custom lobby...";

	const u32_t uGeneration = m_uGeneration;
	m_pMatchClient->JoinQueue(
		[this, uGeneration](const Client::MatchStatus& status)
		{
			m_bMatchRequestInFlight = false;
			if (uGeneration == m_uGeneration)
				ApplyMatchStatus(status);
			TryFinishDeferredReset();
		});
}

void CClientShellBackendService::RequestPollMatchStatus()
{
	if (!m_bConfigured || !m_pMatchClient)
	{
		CClientShellDataStore::Instance().SetLobbyMatched("offline_match");
		m_strStatus = "Offline match ready";
		return;
	}

	if (m_bMatchRequestInFlight)
	{
		m_strStatus = "Matchmaking request already in flight";
		return;
	}

	m_bMatchRequestInFlight = true;
	m_strStatus = "Checking match status...";

	const u32_t uGeneration = m_uGeneration;
	m_pMatchClient->PollStatus(
		[this, uGeneration](const Client::MatchStatus& status)
		{
			m_bMatchRequestInFlight = false;
			if (uGeneration == m_uGeneration)
				ApplyMatchStatus(status);
			TryFinishDeferredReset();
		});
}

void CClientShellBackendService::RequestLeaveQueue()
{
	if (!m_bConfigured || !m_pMatchClient)
	{
		CClientShellSession::Instance().ClearMatchAssignment();
		CClientShellDataStore::Instance().SetLobbyIdle();
		m_strStatus = "Left offline queue";
		return;
	}

	if (m_bMatchRequestInFlight)
	{
		m_strStatus = "Matchmaking request already in flight";
		return;
	}

	m_bMatchRequestInFlight = true;
	m_strStatus = "Leaving queue...";

	const u32_t uGeneration = m_uGeneration;
	m_pMatchClient->LeaveQueue(
		[this, uGeneration](const Client::MatchStatus& status)
		{
			m_bMatchRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (status.error.empty())
				{
					CClientShellSession::Instance().ClearMatchAssignment();
					CClientShellDataStore::Instance().SetLobbyIdle();
					m_strStatus = "Left queue";
				}
				else
				{
					m_strStatus = "Leave queue failed: " + status.error;
				}
			}
			TryFinishDeferredReset();
		});
}

bool_t CClientShellBackendService::HasInFlightRequests() const
{
	return m_bProfileRequestInFlight
		|| m_bStoreRequestInFlight
		|| m_bPurchaseRequestInFlight
		|| m_bMatchRequestInFlight
		|| m_bReplayRequestInFlight;
}

void CClientShellBackendService::ApplyMatchStatus(const Client::MatchStatus& status)
{
	if (!status.error.empty())
	{
		CClientShellDataStore::Instance().SetLobbyIdle();
		m_strStatus = "Custom lobby join failed: " + status.error;
		return;
	}

	if (status.status == "matched" || !status.matchId.empty())
	{
		MatchAssignment assignment{};
		assignment.strMatchID = status.matchId;
		assignment.strGameSessionID = status.gameSessionId;
		assignment.strHost = status.host;
		assignment.iPort = status.port;
		assignment.strTransport = status.transport;
		assignment.strPlayerTicket = status.playerTicket;
		assignment.iExpiresAtUnix = status.expiresAt;
		CClientShellSession::Instance().SetMatchAssignment(assignment);
		CClientShellDataStore::Instance().SetLobbyMatched(status.matchId);
		m_strStatus = "Custom lobby ready";
		return;
	}

	CClientShellDataStore::Instance().SetLobbyIdle();
	m_strStatus = status.status.empty() ? "Custom lobby unavailable" : "Lobby status: " + status.status;
}

void CClientShellBackendService::DestroyClients()
{
	m_pProfileClient.reset();
	m_pShopClient.reset();
	m_pMatchClient.reset();
	m_pReplayClient.reset();
	m_vCloudReplayItems.clear();
	m_strReadyReplayPlaybackPath.clear();
	m_uReadyReplayPerspectiveNetId = 0u;
	++m_uReplayPlaybackIntent;
	m_bProfileRequestInFlight = false;
	m_bStoreRequestInFlight = false;
	m_bPurchaseRequestInFlight = false;
	m_bMatchRequestInFlight = false;
	m_bReplayRequestInFlight = false;
	m_bPostMatchRefreshPending = false;
	m_bMatchHistoryRefreshPending = false;
	m_bResetAfterCallbacks = false;
}

void CClientShellBackendService::TryFinishDeferredReset()
{
	if (!m_bResetAfterCallbacks || HasInFlightRequests())
		return;

	DestroyClients();
	m_strStatus.clear();
}
