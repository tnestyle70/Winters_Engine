#pragma once

#include "ClientShell/ClientShellSession.h"
#include "ClientShell/ClientShellTypes.h"
#include "Network/Backend/CShopClient.h"
#include "Network/Backend/ProfileClient.h"

class CClientShellDataStore final
{
public:
	static CClientShellDataStore& Instance();

	void Reset();
	void SeedOfflineDefaults(const CClientShellSession& session);

	const ShellProfileSummary& GetProfile() const { return m_Profile; }
	const std::vector<ShellStoreItem>& GetStoreItems() const { return m_vecStoreItems; }
	const std::vector<ShellFriendEntry>& GetFriends() const { return m_vecFriends; }
	const ShellLobbyState& GetLobbyState() const { return m_LobbyState; }
	const std::vector<Client::MatchRecord>& GetMatchHistory() const { return m_vecMatchHistory; }
	void ApplyMatchHistory(const std::vector<Client::MatchRecord>& records);

	void ApplyProfileData(const Client::ProfileData& profile);
	void ApplyStorefront(const Client::StorefrontData& storefront);
	void ApplyStoreItems(const std::vector<Client::ShopItemData>& items);
	void ApplyInventoryItems(const std::vector<Client::InventoryItemData>& items);
	bool_t ApplyPurchaseResult(
		const std::string& itemId,
		const Client::PurchaseResult& result,
		std::string& outStatus);
	bool_t PurchaseOfflineItem(const std::string& itemId, std::string& outStatus);

	// storefront 스냅샷 적용 완료 여부 — false면 SyncPending(미보유로 간주 금지).
	bool_t IsInitialSyncReady() const { return m_bInitialSyncReady; }
	u32_t GetStorefrontRevision() const { return m_uStorefrontRevision; }
	const ShellStoreItem* FindStoreItemByContentKey(const std::string& strContentKey) const;

	void SetLobbyGameMode(const std::string& modeID, const std::string& queueName);
	void SetLobbyIdle();
	void SetLobbySearching(const std::string& queueName);
	void SetLobbyMatched(const std::string& matchId);

private:
	CClientShellDataStore() = default;
	~CClientShellDataStore() = default;
	CClientShellDataStore(const CClientShellDataStore&) = delete;
	CClientShellDataStore& operator=(const CClientShellDataStore&) = delete;

	bool_t IsStoreItemOwned(const std::string& itemId) const;
	void MarkStoreItemOwned(const std::string& itemId);

	bool_t m_bSeeded = false;
	bool_t m_bInitialSyncReady = false;
	u32_t m_uStorefrontRevision = 0;
	ShellProfileSummary m_Profile{};
	ShellLobbyState m_LobbyState{};
	std::vector<ShellStoreItem> m_vecStoreItems{};
	std::vector<ShellFriendEntry> m_vecFriends{};
	std::vector<Client::MatchRecord> m_vecMatchHistory{};
};
