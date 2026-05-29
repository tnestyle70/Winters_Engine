#include "ClientShell/ClientShellBackendService.h"

#include "ClientShell/ClientShellDataStore.h"

namespace
{
	constexpr const char* PROFILE_SERVICE_URL = "http://127.0.0.1:8084";
	constexpr const char* SHOP_SERVICE_URL = "http://127.0.0.1:8086";
	constexpr const char* MATCH_SERVICE_URL = "http://127.0.0.1:8083";
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
	m_pProfileClient->SetAuthToken(session.GetAccessToken());
	m_pShopClient->SetAuthToken(session.GetAccessToken());
	m_pMatchClient->SetAuthToken(session.GetAccessToken());

	m_strUserID = session.GetUserID();
	m_bConfigured = true;
	m_bInitialSyncRequested = false;
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

	TryFinishDeferredReset();
}

void CClientShellBackendService::RequestInitialSync()
{
	if (!m_bConfigured || m_bInitialSyncRequested)
		return;

	m_bInitialSyncRequested = true;
	const u32_t uGeneration = m_uGeneration;

	if (m_pProfileClient && !m_strUserID.empty())
	{
		m_bProfileRequestInFlight = true;
		m_pProfileClient->GetProfile(
			m_strUserID,
			[this, uGeneration](const Client::ProfileData& profile)
			{
				m_bProfileRequestInFlight = false;
				if (uGeneration == m_uGeneration)
				{
					CClientShellDataStore::Instance().ApplyProfileData(profile);
					m_strStatus = profile.error.empty() ? "Profile synced" : "Profile sync failed: " + profile.error;
				}
				TryFinishDeferredReset();
			});
	}

	if (m_pShopClient)
	{
		m_bStoreRequestInFlight = true;
		m_pShopClient->ListItems(
			[this, uGeneration](const std::vector<Client::ShopItemData>& items)
			{
				m_bStoreRequestInFlight = false;
				if (uGeneration == m_uGeneration)
				{
					CClientShellDataStore::Instance().ApplyStoreItems(items);
					m_strStatus = items.empty() ? "Store sync returned no items" : "Store synced";
				}
				TryFinishDeferredReset();
			});
	}

	if (m_pShopClient && !m_strUserID.empty())
	{
		m_bInventoryRequestInFlight = true;
		m_pShopClient->GetInventory(
			m_strUserID,
			[this, uGeneration](const std::vector<Client::InventoryItemData>& items)
			{
				m_bInventoryRequestInFlight = false;
				if (uGeneration == m_uGeneration)
				{
					CClientShellDataStore::Instance().ApplyInventoryItems(items);
					m_strStatus = "Inventory synced";
				}
				TryFinishDeferredReset();
			});
	}
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
		m_strStatus = "Matchmaking request already in flight";
		return;
	}

	CClientShellDataStore::Instance().SetLobbySearching(strQueueName);
	m_bMatchRequestInFlight = true;
	m_strStatus = "Joining queue...";

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
	CClientShellDataStore::Instance().SetLobbyIdle();

	if (!m_bConfigured || !m_pMatchClient)
	{
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
				CClientShellDataStore::Instance().SetLobbyIdle();
				m_strStatus = status.error.empty() ? "Left queue" : "Leave queue failed: " + status.error;
			}
			TryFinishDeferredReset();
		});
}

bool_t CClientShellBackendService::HasInFlightRequests() const
{
	return m_bProfileRequestInFlight
		|| m_bStoreRequestInFlight
		|| m_bInventoryRequestInFlight
		|| m_bPurchaseRequestInFlight
		|| m_bMatchRequestInFlight;
}

void CClientShellBackendService::ApplyMatchStatus(const Client::MatchStatus& status)
{
	if (!status.error.empty())
	{
		CClientShellDataStore::Instance().SetLobbyIdle();
		m_strStatus = "Matchmaking failed: " + status.error;
		return;
	}

	if (status.status == "matched" || !status.matchId.empty())
	{
		CClientShellDataStore::Instance().SetLobbyMatched(status.matchId);
		m_strStatus = "Match found";
		return;
	}

	const std::string strQueueName = CClientShellDataStore::Instance().GetLobbyState().strQueueName.empty()
		? "Online Queue"
		: CClientShellDataStore::Instance().GetLobbyState().strQueueName;
	CClientShellDataStore::Instance().SetLobbySearching(strQueueName);
	m_strStatus = status.status.empty() ? "Searching for match" : "Queue status: " + status.status;
}

void CClientShellBackendService::DestroyClients()
{
	m_pProfileClient.reset();
	m_pShopClient.reset();
	m_pMatchClient.reset();
	m_bProfileRequestInFlight = false;
	m_bStoreRequestInFlight = false;
	m_bInventoryRequestInFlight = false;
	m_bPurchaseRequestInFlight = false;
	m_bMatchRequestInFlight = false;
	m_bResetAfterCallbacks = false;
}

void CClientShellBackendService::TryFinishDeferredReset()
{
	if (!m_bResetAfterCallbacks || HasInFlightRequests())
		return;

	DestroyClients();
	m_strStatus.clear();
}
