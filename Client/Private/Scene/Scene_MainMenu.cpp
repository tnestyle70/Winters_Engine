#include "Scene/Scene_MainMenu.h"

#include "ClientShell/ClientShellBackendService.h"
#include "ClientShell/ClientShellDataStore.h"
#include "ClientShell/ClientShellSession.h"
#include "GameInstance.h"
#include "GameMode/GameModeCatalog.h"
#include "GameModule/GameModuleRegistry.h"
#include "Scene/Scene_Login.h"

#include <utility>

namespace
{
	constexpr ImageSourceRect kGameStartRect{ 47.f, 24.f, 236.f, 70.f };
}

std::unique_ptr<CScene_MainMenu> CScene_MainMenu::Create()
{
	return std::unique_ptr<CScene_MainMenu>(new CScene_MainMenu());
}

bool CScene_MainMenu::OnEnter()
{
	m_strStatus = "Client shell ready";
	m_bPlayRequested = false;
	m_bLogoutRequested = false;
	m_ImageUI.Initialize(
		L"Texture/UI/MainMenu1.png",
		1545,
		859);

	CClientShellDataStore::Instance().SeedOfflineDefaults(CClientShellSession::Instance());
	CGameModeCatalog::Instance().LoadFromJson();
	if (const GameModeDef* pDefaultMode = CGameModeCatalog::Instance().GetDefaultMode())
	{
		CClientShellDataStore::Instance().SetLobbyGameMode(
			pDefaultMode->strModeID,
			pDefaultMode->strQueueName.empty() ? pDefaultMode->strDisplayName : pDefaultMode->strQueueName);
	}

	CClientShellBackendService::Instance().ConfigureFromSession(CClientShellSession::Instance());
	CClientShellBackendService::Instance().RequestInitialSync();
	if (!CClientShellBackendService::Instance().GetStatus().empty())
		m_strStatus = CClientShellBackendService::Instance().GetStatus();

	return true;
}

void CScene_MainMenu::OnExit()
{
	m_ImageUI.Shutdown();
	m_strStatus.clear();
}

void CScene_MainMenu::OnUpdate(f32_t /*dt*/)
{
	CClientShellBackendService::Instance().ProcessCallbacks();
	if (!CClientShellBackendService::Instance().GetStatus().empty())
		m_strStatus = CClientShellBackendService::Instance().GetStatus();

	if (m_ImageUI.WasSourceRectClicked(kGameStartRect))
		RequestPlay();

	if (m_bLogoutRequested) { m_bLogoutRequested = false; ChangeToLogin(); return; }
	if (m_bPlayRequested) { m_bPlayRequested = false; LaunchSelectedProduct(); return; }
}

void CScene_MainMenu::OnLateUpdate(f32_t /*dt*/)
{}

void CScene_MainMenu::OnRender()
{
	m_ImageUI.Render();
}

void CScene_MainMenu::OnImGui()
{}

void CScene_MainMenu::RequestPlay()
{
	m_strStatus = "Launching selected product...";
	m_bPlayRequested = true;
}

void CScene_MainMenu::RequestLogout()
{
	m_bLogoutRequested = true;
}

bool_t CScene_MainMenu::LaunchSelectedProduct()
{
	CClientShellSession& session = CClientShellSession::Instance();
	CGameModuleRegistry& registry = CGameModuleRegistry::Instance();
	GameLaunchConfig launchConfig = session.GetLaunchConfig();
	launchConfig.strGameModeID = CClientShellDataStore::Instance().GetLobbyState().strSelectedGameModeID;

	if (!registry.Activate(session.GetSelectedProduct(), launchConfig))
	{
		m_strStatus = "Failed to initialize game module";
		return false;
	}

	IGameModule* pModule = registry.GetActiveModule();
	if (!pModule)
		return false;

	auto pScene = pModule->CreateInitialScene();
	if (!pScene)
		return false;

	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(pModule->GetInitialSceneID()),
		std::move(pScene));

	return true;
}

void CScene_MainMenu::ChangeToLogin()
{
	CClientShellBackendService::Instance().Reset();
	CClientShellSession::Instance().Logout();
	CClientShellDataStore::Instance().Reset();

	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::Login),
		CScene_Login::Create()
	);
}
