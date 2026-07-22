#include "Scene/Scene_MainMenu.h"

#include "ClientShell/ClientShellBackendService.h"
#include "ClientShell/ClientShellDataStore.h"
#include "ClientShell/ClientShellSession.h"
#include "GameInstance.h"
#include "GameMode/GameModeCatalog.h"
#include "GameModule/GameModuleRegistry.h"
#include "Scene/Scene_Login.h"
#include "Scene/Scene_MyInfo.h"
#include "Scene/Scene_Shop.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <utility>

namespace
{
	constexpr ImageSourceRect kGameStartRect{ 47.f, 24.f, 236.f, 70.f };
	// MainMenu1.png 소스 공간(1545x859) 기준: Game Start 아래 상점 버튼, 우상단 나의 정보 초상화.
	constexpr ImageSourceRect kShopButtonRect{ 95.f, 92.f, 284.f, 138.f };
	constexpr ImageSourceRect kMyInfoPortraitRect{ 1405.f, 24.f, 1505.f, 124.f };
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
	m_bShopRequested = false;
	m_bMyInfoRequested = false;
	m_bWaitingForMatch = false;
	m_ImageUI.Initialize(
		L"Texture/UI/MainMenu1.png",
		1545,
		859);

	if (IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice())
	{
		m_pMyInfoPortrait = Engine::CTexture::Create(
			pDevice,
			L"Texture/UI/Champion/Portraits/yasuo_square.png",
			Engine::eTexSamplerMode::Clamp,
			Engine::eTexColorSpace::IgnoreSRGB);
	}

	// 온라인 계정에 offline 더미(RP 1350/level 30)가 섞이지 않도록 offline 세션에서만 seed한다.
	if (CClientShellSession::Instance().IsOfflineAccount())
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

void CScene_MainMenu::OnUpdate(f32_t dt)
{
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	backend.ProcessCallbacks();
	if (!backend.GetStatus().empty())
		m_strStatus = backend.GetStatus();

	const bool_t bGameStartClicked =
		m_ImageUI.WasSourceRectClicked(kGameStartRect);
	if (m_bWaitingForMatch)
	{
		UpdateOnlineMatchmaking(dt);
	}
	else
	{
		if (bGameStartClicked)
			RequestPlay();
		if (m_ImageUI.WasSourceRectClicked(kShopButtonRect))
			m_bShopRequested = true;
		if (m_ImageUI.WasSourceRectClicked(kMyInfoPortraitRect))
			m_bMyInfoRequested = true;
	}

	if (m_bLogoutRequested) { m_bLogoutRequested = false; ChangeToLogin(); return; }
	if (m_bShopRequested) { m_bShopRequested = false; ChangeToShop(); return; }
	if (m_bMyInfoRequested) { m_bMyInfoRequested = false; ChangeToMyInfo(); return; }
	if (m_bPlayRequested) { m_bPlayRequested = false; LaunchSelectedProduct(); return; }
}

void CScene_MainMenu::OnLateUpdate(f32_t /*dt*/)
{}

void CScene_MainMenu::OnRender()
{
	if (!m_ImageUI.Begin())
		return;

	m_ImageUI.DrawBackground();

	// 상점 버튼 (Game Start 아래)
	m_ImageUI.DrawSourceRect(kShopButtonRect, Vec4(0.02f, 0.06f, 0.10f, 0.85f));
	m_ImageUI.DrawSourceRectOutline(kShopButtonRect, Vec4(0.68f, 0.52f, 0.18f, 0.95f), 1.5f);

	// 우상단 나의 정보 챔피언 초상화 버튼
	if (m_pMyInfoPortrait)
		m_ImageUI.DrawSourceImage(m_pMyInfoPortrait.get(), kMyInfoPortraitRect, Vec4(1.f, 1.f, 1.f, 1.f));
	else
		m_ImageUI.DrawSourceRect(kMyInfoPortraitRect, Vec4(0.05f, 0.05f, 0.08f, 0.9f));
	m_ImageUI.DrawSourceRectOutline(kMyInfoPortraitRect, Vec4(0.68f, 0.52f, 0.18f, 0.95f), 2.f);

	m_ImageUI.End();
}

void CScene_MainMenu::OnImGui()
{
	ImDrawList* pDraw = ImGui::GetForegroundDrawList();
	if (!pDraw)
		return;

	auto drawSourceText = [this, pDraw](f32_t fX, f32_t fY, ImU32 color, const char* pText)
	{
		ImageScreenRect screenRect{};
		if (m_ImageUI.SourceRectToScreen(ImageSourceRect{ fX, fY, fX + 1.f, fY + 1.f }, screenRect))
			pDraw->AddText(ImVec2(screenRect.fX, screenRect.fY), color, pText);
	};

	drawSourceText(148.f, 104.f, IM_COL32(235, 220, 180, 255), "상점");

	const CClientShellSession& session = CClientShellSession::Instance();
	const CClientShellDataStore& store = CClientShellDataStore::Instance();

	drawSourceText(1405.f, 132.f, IM_COL32(240, 230, 200, 255), session.GetDisplayName().c_str());
	if (session.IsAuthenticated() && store.IsInitialSyncReady())
	{
		char szBuffer[64]{};
		sprintf_s(szBuffer, sizeof(szBuffer), "%d RP", store.GetProfile().iRP);
		drawSourceText(1405.f, 156.f, IM_COL32(120, 210, 255, 255), szBuffer);
	}
	if (m_bWaitingForMatch)
	{
		drawSourceText(
			47.f,
			78.f,
			IM_COL32(120, 210, 255, 255),
			m_strStatus.c_str());
	}
}

void CScene_MainMenu::RequestPlay()
{
	const CClientShellSession& session = CClientShellSession::Instance();
	if (!session.IsAuthenticated() || session.IsOfflineAccount())
	{
		m_strStatus = "Launching selected product...";
		m_bPlayRequested = true;
		return;
	}

	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	if (!backend.IsConfigured())
	{
		m_strStatus = "Online lobby backend is unavailable";
		return;
	}

	m_bWaitingForMatch = true;
	m_strStatus = "Joining authenticated custom lobby...";
	backend.RequestJoinQueue();
}

void CScene_MainMenu::UpdateOnlineMatchmaking(f32_t /*dt*/)
{
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	const ShellLobbyState& lobby =
		CClientShellDataStore::Instance().GetLobbyState();
	if (CClientShellSession::Instance().HasMatchAssignment() &&
		lobby.bMatchReady)
	{
		m_bWaitingForMatch = false;
		m_strStatus = "Authenticated custom lobby ready";
		m_bPlayRequested = true;
		return;
	}

	if (lobby.eQueueState == eLobbyQueueState::Idle &&
		!backend.IsMatchRequestInFlight())
	{
		m_bWaitingForMatch = false;
		return;
	}
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

void CScene_MainMenu::ChangeToShop()
{
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::Shop),
		CScene_Shop::Create());
}

void CScene_MainMenu::ChangeToMyInfo()
{
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::MyInfo),
		CScene_MyInfo::Create());
}
