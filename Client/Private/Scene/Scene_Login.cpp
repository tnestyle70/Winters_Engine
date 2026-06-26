#include "Scene/Scene_Login.h"

#include "ClientShell/ClientShellSession.h"
#include "Dev/SmokeLog.h"
#include "GameInstance.h"
#include "Scene/Scene_MainMenu.h"

#include <cstring>
#include <cwchar>

#include <Windows.h>

namespace
{
	constexpr const char* AUTH_SERVICE_URL = "http://127.0.0.1:8081";
	constexpr ImageSourceRect kLoginArrowRect{ 252.f, 976.f, 351.f, 1073.f };
}

std::unique_ptr<CScene_Login> CScene_Login::Create()
{
	return std::unique_ptr<CScene_Login>(new CScene_Login());
}

bool CScene_Login::OnEnter()
{
	m_pAuthClient = Client::CAuthClient::Create(AUTH_SERVICE_URL);
	strcpy_s(m_szEmail, sizeof(m_szEmail), "dev@winters.local");
	m_szPassword[0] = '\0';
	m_strStatus = "Login";
	m_bOfflineLoginRequested = false;
	m_bLoginInFlight = false;
	m_ImageUI.Initialize(
		L"Texture/UI/Login1.png",
		2301,
		1289);

	const wchar_t* pCommandLine = GetCommandLineW();
	if (pCommandLine && std::wcsstr(pCommandLine, L"--banpick-smoke"))
	{
		Winters::DevSmoke::Log("[LoginSmoke] auto offline login\n");
		RequestOfflineLogin();
	}

	return true;
}

void CScene_Login::OnExit()
{
	m_ImageUI.Shutdown();
	m_pAuthClient.reset();
	m_strStatus.clear();
	m_bOfflineLoginRequested = false;
	m_bLoginInFlight = false;
}

void CScene_Login::OnUpdate(f32_t /*dt*/)
{
	if (m_pAuthClient)
		m_pAuthClient->ProcessCallbacks();

	if (!m_bLoginInFlight && m_ImageUI.WasSourceRectClicked(kLoginArrowRect))
		RequestOfflineLogin();

	if (m_bOfflineLoginRequested)
	{
		m_bOfflineLoginRequested = false;
		ChangeToMainMenu();
		return;
	}
}

void CScene_Login::OnLateUpdate(f32_t /*dt*/)
{}

void CScene_Login::OnRender()
{
	m_ImageUI.Render();
}

void CScene_Login::OnImGui()
{}

void CScene_Login::RequestOfflineLogin()
{
	CClientShellSession::Instance().SetOfflineAccount("Offline Player");
	m_strStatus = "Launching selected module...";
	m_bOfflineLoginRequested = true;
}

void CScene_Login::RequestOnlineLogin()
{
	if (!m_pAuthClient)
	{
		m_strStatus = "Auth client is not initialized";
		return;
	}

	if (m_szEmail[0] == '\0' || m_szPassword[0] == '\0')
	{
		m_strStatus = "Email and password are required for online login";
		return;
	}

	const std::string strEmail = m_szEmail;
	const std::string strPassword = m_szPassword;

	m_bLoginInFlight = true;
	m_strStatus = "Signing in...";

	m_pAuthClient->Login(
		strEmail,
		strPassword,
		[this, strEmail](const Client::AuthResult& result)
		{
			HandleOnlineLoginResult(strEmail, result);
		});
}

void CScene_Login::HandleOnlineLoginResult(const std::string& email, const Client::AuthResult& result)
{
	m_bLoginInFlight = false;

	if (!result.success)
	{
		m_strStatus = result.error.empty() ? "Online login failed" : "Online login failed: " + result.error;
		return;
	}

	CClientShellSession::Instance().SetAuthenticatedAccount(
		email,
		email,
		result.accessToken);

	m_strStatus = "Login successful";
	m_bOfflineLoginRequested = true;
}

void CScene_Login::ChangeToMainMenu()
{
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::MainMenu),
		CScene_MainMenu::Create()
	);
}
