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

	// 자기 프로필은 JWT claims 기반 /profile/me — client가 user id를 보내지 않는다.
	if (m_pProfileClient)
	{
		m_bProfileRequestInFlight = true;
		m_pProfileClient->GetMyProfile(
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

	// RP·상품·소유권은 storefront 하나의 원자 스냅샷 — items/inventory 조합 race 제거.
	RequestStorefrontSync();
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

void CClientShellBackendService::RequestReportMatchResult(bool_t bVictory)
{
	// 비회원/백엔드 미실행이면 보고하지 않는다 — 게임 흐름은 로컬 전적만으로 완결 (S035).
	if (!m_bConfigured || !m_pProfileClient)
		return;

	const u32_t uGeneration = m_uGeneration;
	m_bProfileRequestInFlight = true;
	m_pProfileClient->ReportMyMatch(
		bVictory,
		[this, uGeneration](const Client::MatchReportResult& result)
		{
			m_bProfileRequestInFlight = false;
			if (uGeneration == m_uGeneration)
			{
				if (result.success)
				{
					m_strStatus = "Match reported";
					// MMR/RP가 바뀌었으니 다음 메인메뉴 진입 sync가 최신 값을 받도록
					// 프로필/상점 재요청 래치를 푼다.
					m_bInitialSyncRequested = false;
				}
				else
				{
					m_strStatus = result.error.empty()
						? "Match report failed"
						: "Match report failed: " + result.error;
				}
			}
			TryFinishDeferredReset();
		});
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
