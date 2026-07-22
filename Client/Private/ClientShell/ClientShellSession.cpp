#include "ClientShell/ClientShellSession.h"

CClientShellSession& CClientShellSession::Instance()
{
	static CClientShellSession s_Instance;
	return s_Instance;
}

void CClientShellSession::SetSelectedProduct(eGameProduct product, const GameLaunchConfig& config)
{
	m_eSelectedProduct = product;
	m_LaunchConfig = config;
}

void CClientShellSession::SetOfflineAccount(const std::string& displayName)
{
	ClearMatchAssignment();
	m_bAuthenticated = true;
	m_bOfflineAccount = true;
	m_strUserID = "offline";
	m_strDisplayName = displayName.empty() ? "Offline Player" : displayName;
	m_strAccessToken.clear();
}

void CClientShellSession::SetAuthenticatedAccount(
	const std::string& userId,
	const std::string& displayName,
	const std::string& accessToken)
{
	ClearMatchAssignment();
	m_bAuthenticated = true;
	m_bOfflineAccount = false;
	m_strUserID = userId;
	m_strDisplayName = displayName.empty() ? userId : displayName;
	m_strAccessToken = accessToken;
}

void CClientShellSession::Logout()
{
	ClearMatchAssignment();
	m_bAuthenticated = false;
	m_bOfflineAccount = false;
	m_strUserID.clear();
	m_strDisplayName.clear();
	m_strAccessToken.clear();
}

void CClientShellSession::SetMatchAssignment(const MatchAssignment& assignment)
{
	m_MatchAssignment = assignment;
}

void CClientShellSession::ClearMatchAssignment()
{
	m_MatchAssignment = {};
}

bool_t CClientShellSession::HasMatchAssignment() const
{
	return !m_MatchAssignment.strMatchID.empty() &&
		!m_MatchAssignment.strGameSessionID.empty() &&
		!m_MatchAssignment.strHost.empty() &&
		m_MatchAssignment.iPort > 0 &&
		!m_MatchAssignment.strTransport.empty() &&
		!m_MatchAssignment.strPlayerTicket.empty();
}
