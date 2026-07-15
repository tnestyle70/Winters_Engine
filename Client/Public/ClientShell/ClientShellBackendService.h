#pragma once

#include "ClientShell/ClientShellSession.h"
#include "Network/Backend/CShopClient.h"
#include "Network/Backend/MatchClient.h"
#include "Network/Backend/ProfileClient.h"

#include <memory>
#include <string>
#include <vector>

class CClientShellBackendService final
{
public:
	static CClientShellBackendService& Instance();

	void ConfigureFromSession(const CClientShellSession& session);
	void Reset();
	void ProcessCallbacks();
	void RequestInitialSync();
	void RequestStorefrontSync();
	void RequestMatchHistory();
	// 게임종료 매치결과 보고 — 오프라인/미구성이면 조용히 스킵 (S035).
	void RequestReportMatchResult(bool_t bVictory);
	void RequestPurchase(const std::string& itemId);
	void RequestJoinQueue();
	void RequestPollMatchStatus();
	void RequestLeaveQueue();

	bool_t IsConfigured() const { return m_bConfigured; }
	bool_t IsPurchaseInFlight() const { return m_bPurchaseRequestInFlight; }
	bool_t IsStorefrontSyncInFlight() const { return m_bStoreRequestInFlight; }
	const std::string& GetStatus() const { return m_strStatus; }

private:
	CClientShellBackendService() = default;
	~CClientShellBackendService() = default;
	CClientShellBackendService(const CClientShellBackendService&) = delete;
	CClientShellBackendService& operator=(const CClientShellBackendService&) = delete;

	bool_t HasInFlightRequests() const;
	void ApplyMatchStatus(const Client::MatchStatus& status);
	void DestroyClients();
	void TryFinishDeferredReset();

	std::unique_ptr<Client::CProfileClient> m_pProfileClient{};
	std::unique_ptr<Client::CShopClient> m_pShopClient{};
	std::unique_ptr<Client::CMatchClient> m_pMatchClient{};
	std::string m_strUserID{};
	std::string m_strStatus{};

	u32_t m_uGeneration = 0;
	bool_t m_bConfigured = false;
	bool_t m_bInitialSyncRequested = false;
	bool_t m_bResetAfterCallbacks = false;
	bool_t m_bProfileRequestInFlight = false;
	bool_t m_bStoreRequestInFlight = false;
	bool_t m_bPurchaseRequestInFlight = false;
	bool_t m_bMatchRequestInFlight = false;
};
