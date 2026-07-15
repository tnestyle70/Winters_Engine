#include "ClientShell/ClientShellDataStore.h"

#include <utility>

CClientShellDataStore& CClientShellDataStore::Instance()
{
	static CClientShellDataStore s_Instance;
	return s_Instance;
}

void CClientShellDataStore::Reset()
{
	m_bSeeded = false;
	m_bInitialSyncReady = false;
	m_uStorefrontRevision = 0;
	m_Profile = {};
	m_LobbyState = {};
	m_vecStoreItems.clear();
	m_vecFriends.clear();
	m_vecMatchHistory.clear();
}

void CClientShellDataStore::SeedOfflineDefaults(const CClientShellSession& session)
{
	if (m_bSeeded)
		return;

	m_Profile.strUserID = session.GetUserID();
	m_Profile.strDisplayName = session.GetDisplayName();
	m_Profile.iLevel = 30;
	m_Profile.iWins = 12;
	m_Profile.iLosses = 5;
	m_Profile.iMMR = 1000;
	m_Profile.iRP = 1350;
	m_LobbyState.strSelectedGameModeID = "summoners_rift";
	m_LobbyState.strQueueName = "Normal";
	m_LobbyState.strMatchID.clear();
	m_LobbyState.eQueueState = eLobbyQueueState::Idle;
	m_LobbyState.bMatchReady = false;

	m_vecStoreItems = {
		{ "skin_irelia_frost", "Frost Irelia", "Offline test skin", "Skin", "", "", 520, false },
		{ "skin_yasuo_night", "Night Yasuo", "Offline test skin", "Skin", "", "", 750, false },
		{ "boost_xp_1d", "XP Boost 1 Day", "Offline account boost", "Boost", "", "", 290, false },
	};

	m_vecFriends = {
		{ "friend_001", "WinterDev", eFriendPresence::Online, "In lobby" },
		{ "friend_002", "MinionLab", eFriendPresence::InGame, "Summoner's Rift" },
		{ "friend_003", "ShaderBox", eFriendPresence::Away, "Tuning FX" },
	};

	++m_uStorefrontRevision;
	m_bSeeded = true;
}

void CClientShellDataStore::ApplyProfileData(const Client::ProfileData& profile)
{
	if (!profile.error.empty())
		return;

	if (!profile.userId.empty())
		m_Profile.strUserID = profile.userId;
	if (!profile.username.empty())
		m_Profile.strDisplayName = profile.username;

	m_Profile.iWins = profile.wins;
	m_Profile.iLosses = profile.losses;
	m_Profile.iMMR = profile.mmr;
}

void CClientShellDataStore::ApplyStorefront(const Client::StorefrontData& storefront)
{
	if (!storefront.success)
		return;

	// RP·상품·소유권을 하나의 스냅샷으로 통째 교체한다 (부분 갱신 race 금지).
	std::vector<ShellStoreItem> vecNextItems;
	vecNextItems.reserve(storefront.items.size());
	for (const Client::ShopItemData& source : storefront.items)
	{
		ShellStoreItem item{};
		item.strItemID = source.id;
		item.strName = source.name;
		item.strItemType = source.itemType;
		item.strProductKey = source.productKey;
		item.strContentKey = source.contentKey;
		item.iPriceRP = static_cast<i32_t>(source.price);
		item.bOwned = source.owned;
		vecNextItems.push_back(std::move(item));
	}

	m_vecStoreItems = std::move(vecNextItems);
	m_Profile.iRP = static_cast<i32_t>(storefront.balanceRP);
	m_bInitialSyncReady = true;
	++m_uStorefrontRevision;
}

const ShellStoreItem* CClientShellDataStore::FindStoreItemByContentKey(const std::string& strContentKey) const
{
	for (const ShellStoreItem& item : m_vecStoreItems)
	{
		if (item.strContentKey == strContentKey)
			return &item;
	}
	return nullptr;
}

void CClientShellDataStore::ApplyStoreItems(const std::vector<Client::ShopItemData>& items)
{
	if (items.empty())
		return;

	std::vector<ShellStoreItem> vecNextItems;
	vecNextItems.reserve(items.size());

	for (const Client::ShopItemData& source : items)
	{
		ShellStoreItem item{};
		item.strItemID = source.id;
		item.strName = source.name;
		item.strDescription = source.description;
		item.strItemType = source.itemType;
		item.iPriceRP = static_cast<i32_t>(source.price);
		item.bOwned = IsStoreItemOwned(source.id);
		vecNextItems.push_back(std::move(item));
	}

	m_vecStoreItems = std::move(vecNextItems);
	++m_uStorefrontRevision;
}

void CClientShellDataStore::ApplyInventoryItems(const std::vector<Client::InventoryItemData>& items)
{
	bool_t bChanged = false;
	for (const Client::InventoryItemData& source : items)
	{
		if (source.itemId.empty())
			continue;

		bool_t bFound = false;
		for (ShellStoreItem& item : m_vecStoreItems)
		{
			if (item.strItemID != source.itemId)
				continue;

			if (!item.bOwned)
			{
				item.bOwned = true;
				bChanged = true;
			}
			bFound = true;
			break;
		}

		if (!bFound)
		{
			ShellStoreItem item{};
			item.strItemID = source.itemId;
			item.strName = source.name.empty() ? source.itemId : source.name;
			item.strItemType = source.itemType;
			item.bOwned = true;
			m_vecStoreItems.push_back(std::move(item));
			bChanged = true;
		}
	}

	if (bChanged)
		++m_uStorefrontRevision;
}

bool_t CClientShellDataStore::ApplyPurchaseResult(
	const std::string& itemId,
	const Client::PurchaseResult& result,
	std::string& outStatus)
{
	if (!result.success)
	{
		outStatus = result.error.empty() ? "Purchase failed" : "Purchase failed: " + result.error;
		return false;
	}

	MarkStoreItemOwned(itemId);
	if (result.remainingCoins >= 0)
		m_Profile.iRP = static_cast<i32_t>(result.remainingCoins);
	++m_uStorefrontRevision;

	outStatus = result.status == "already_owned"
		? "이미 구매한 상품입니다."
		: "구매 완료";
	return true;
}

void CClientShellDataStore::ApplyMatchHistory(const std::vector<Client::MatchRecord>& records)
{
	m_vecMatchHistory = records;
}

bool_t CClientShellDataStore::PurchaseOfflineItem(const std::string& itemId, std::string& outStatus)
{
	for (ShellStoreItem& item : m_vecStoreItems)
	{
		if (item.strItemID != itemId)
			continue;

		if (item.bOwned)
		{
			outStatus = "이미 구매한 상품입니다.";
			return false;
		}

		if (m_Profile.iRP < item.iPriceRP)
		{
			outStatus = "Not enough RP";
			return false;
		}

		m_Profile.iRP -= item.iPriceRP;
		item.bOwned = true;
		++m_uStorefrontRevision;
		outStatus = "Purchased " + item.strName;
		return true;
	}

	outStatus = "Item not found";
	return false;
}

void CClientShellDataStore::SetLobbyGameMode(const std::string& modeID, const std::string& queueName)
{
	m_LobbyState.strSelectedGameModeID = modeID;
	m_LobbyState.strQueueName = queueName;
	SetLobbyIdle();
}

void CClientShellDataStore::SetLobbyIdle()
{
	m_LobbyState.eQueueState = eLobbyQueueState::Idle;
	m_LobbyState.strMatchID.clear();
	m_LobbyState.bMatchReady = false;
}

void CClientShellDataStore::SetLobbySearching(const std::string& queueName)
{
	m_LobbyState.strQueueName = queueName;
	m_LobbyState.eQueueState = eLobbyQueueState::Searching;
	m_LobbyState.strMatchID.clear();
	m_LobbyState.bMatchReady = false;
}

void CClientShellDataStore::SetLobbyMatched(const std::string& matchId)
{
	m_LobbyState.eQueueState = eLobbyQueueState::MatchFound;
	m_LobbyState.strMatchID = matchId.empty() ? "offline_match" : matchId;
	m_LobbyState.bMatchReady = true;
}

bool_t CClientShellDataStore::IsStoreItemOwned(const std::string& itemId) const
{
	for (const ShellStoreItem& item : m_vecStoreItems)
	{
		if (item.strItemID == itemId)
			return item.bOwned;
	}

	return false;
}

void CClientShellDataStore::MarkStoreItemOwned(const std::string& itemId)
{
	for (ShellStoreItem& item : m_vecStoreItems)
	{
		if (item.strItemID != itemId)
			continue;

		item.bOwned = true;
		return;
	}
}
