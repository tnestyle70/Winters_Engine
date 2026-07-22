#pragma once

#include "Defines.h"
#include "GameModule/GameLaunchConfig.h"

#include <string>

struct MatchAssignment
{
	std::string strMatchID{};
	std::string strGameSessionID{};
	std::string strHost{};
	i32_t iPort = 0;
	std::string strTransport{};
	std::string strPlayerTicket{};
	i64_t iExpiresAtUnix = 0;
};

class CClientShellSession final
{
public:
	static CClientShellSession& Instance();

	void SetSelectedProduct(eGameProduct product, const GameLaunchConfig& config);
	eGameProduct GetSelectedProduct() const { return m_eSelectedProduct; }
	const GameLaunchConfig& GetLaunchConfig() const { return m_LaunchConfig; }
	bool_t HasSelectedProduct() const { return m_eSelectedProduct != eGameProduct::None; }

	void SetOfflineAccount(const std::string& displayName);
	void SetAuthenticatedAccount(
		const std::string& userId,
		const std::string& displayName,
		const std::string& accessToken);
	void Logout();
	void SetMatchAssignment(const MatchAssignment& assignment);
	void ClearMatchAssignment();

	bool_t IsAuthenticated() const { return m_bAuthenticated; }
	bool_t IsOfflineAccount() const { return m_bOfflineAccount; }
	const std::string& GetUserID() const { return m_strUserID; }
	const std::string& GetDisplayName() const { return m_strDisplayName; }
	const std::string& GetAccessToken() const { return m_strAccessToken; }
	bool_t HasMatchAssignment() const;
	const MatchAssignment& GetMatchAssignment() const { return m_MatchAssignment; }

private:
	CClientShellSession() = default;
	~CClientShellSession() = default;
	CClientShellSession(const CClientShellSession&) = delete;
	CClientShellSession& operator=(const CClientShellSession&) = delete;

	eGameProduct m_eSelectedProduct = eGameProduct::None;
	GameLaunchConfig m_LaunchConfig{};

	bool_t m_bAuthenticated = false;
	bool_t m_bOfflineAccount = false;
	std::string m_strUserID{};
	std::string m_strDisplayName{};
	std::string m_strAccessToken{};
	MatchAssignment m_MatchAssignment{};
};
