#include "Scene/Scene_GameSelect.h"

#include "GameInstance.h"
#include "GameModule/GameModuleRegistry.h"
#include "ClientShell/ClientShellSession.h"
#include "Dev/SmokeLog.h"
#include "Scene/Scene_Login.h"

#include <cstdio>
#include <cwchar>

#include <Windows.h>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

std::unique_ptr<CScene_GameSelect> CScene_GameSelect::Create()
{
	return std::unique_ptr<CScene_GameSelect>(new CScene_GameSelect());
}

bool CScene_GameSelect::OnEnter()
{
	CGameModuleRegistry::Instance().RegisterDefaults();
	m_strStatus = "Select game module";
	m_ePendingProduct = eGameProduct::None;
	m_bLaunchRequested = false;

	const wchar_t* pCommandLine = GetCommandLineW();
	if (pCommandLine && std::wcsstr(pCommandLine, L"--banpick-smoke"))
	{
		Winters::DevSmoke::Log("[GameSelectSmoke] auto launch LOL\n");
		RequestLaunch(eGameProduct::LOL);
	}

	return true;
}

void CScene_GameSelect::OnExit()
{
	m_strStatus.clear();
	m_ePendingProduct = eGameProduct::None;
	m_bLaunchRequested = false;
}

void CScene_GameSelect::OnUpdate(f32_t /*dt*/)
{
	if (!m_bLaunchRequested)
		return;

	const eGameProduct eProduct = m_ePendingProduct;
	m_bLaunchRequested = false;
	m_ePendingProduct = eGameProduct::None;

	TryLaunch(eProduct);
}

void CScene_GameSelect::OnLateUpdate(f32_t /*dt*/)
{
}

void CScene_GameSelect::OnRender()
{
}

void CScene_GameSelect::OnImGui()
{
	ImGuiIO& io = ImGui::GetIO();
	const ImVec2 vWindowSize(520.f, 360.f);
	ImGui::SetNextWindowPos(
		ImVec2((io.DisplaySize.x - vWindowSize.x) * 0.5f, (io.DisplaySize.y - vWindowSize.y) * 0.5f),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(vWindowSize, ImGuiCond_Always);

	constexpr ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings;

	if (!ImGui::Begin("Winters Game Select", nullptr, flags))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("Winters Client Shell");
	ImGui::Separator();

	DrawProductButton(eGameProduct::LOL);
	DrawProductButton(eGameProduct::EldenRing);
	DrawProductButton(eGameProduct::ClassServant);

	ImGui::Separator();
	ImGui::TextWrapped("%s", m_strStatus.c_str());

	ImGui::End();
}

GameLaunchConfig CScene_GameSelect::BuildLaunchConfig(eGameProduct product) const
{
	GameLaunchConfig config{};
	config.eProduct = product;
	config.bUseEditorTools = true;

	switch (product)
	{
	case eGameProduct::LOL:
		config.strGameModeID = "summoners_rift";
		config.strContentRoot = L"Client/Bin/Resource";
		config.strServiceNamespace = L"winters.lol";
		config.strServerEndpoint = L"http://127.0.0.1:8080";
		config.bUseOnlineServices = false;
		break;
	case eGameProduct::EldenRing:
		config.strContentRoot = L"Client/Bin/Resource";
		config.strServiceNamespace = L"winters.elden";
		config.strServerEndpoint = L"";
		config.bUseOnlineServices = false;
		break;
	case eGameProduct::ClassServant:
		config.strContentRoot = L"Client/Bin/Resource";
		config.strServiceNamespace = L"winters.classservant";
		config.strServerEndpoint = L"";
		config.bUseOnlineServices = false;
		break;
	default:
		break;
	}

	return config;
}

void CScene_GameSelect::RequestLaunch(eGameProduct product)
{
	m_ePendingProduct = product;
	m_bLaunchRequested = true;

	char szStatus[128] = {};
	std::snprintf(szStatus, sizeof(szStatus), "Launching %s...", GetGameProductName(product));
	m_strStatus = szStatus;
}

bool_t CScene_GameSelect::TryLaunch(eGameProduct product)
{
	if (product == eGameProduct::None)
	{
		m_strStatus = "Invalid game module";
		return false;
	}

	CGameModuleRegistry& registry = CGameModuleRegistry::Instance();
	IGameModule* pModule = registry.Find(product);
	if (!pModule)
	{
		m_strStatus = "This game module is not implemented yet";
		return false;
	}

	if (!pModule->IsAvailable())
	{
		m_strStatus = "This game module is not available yet";
		return false;
	}

	CClientShellSession::Instance().SetSelectedProduct(product, BuildLaunchConfig(product));
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::Login),
		CScene_Login::Create());
	return true;
}

void CScene_GameSelect::DrawProductButton(eGameProduct product)
{
	CGameModuleRegistry& registry = CGameModuleRegistry::Instance();
	IGameModule* pModule = registry.Find(product);
	const bool_t bAvailable = (pModule && pModule->IsAvailable());

	char szButton[96] = {};
	const char* pDisplayName = pModule ? pModule->GetDisplayName() : GetGameProductName(product);
	std::snprintf(szButton, sizeof(szButton), "%s##GameProduct%u",
		pDisplayName,
		static_cast<u32_t>(product));

	if (!bAvailable)
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);

	const bool_t bClicked = ImGui::Button(szButton, ImVec2(-1.f, 52.f));

	if (!bAvailable)
		ImGui::PopStyleVar();

	if (bClicked && bAvailable)
		RequestLaunch(product);

	if (!bAvailable)
	{
		ImGui::SameLine();
		ImGui::TextUnformatted("Not implemented");
	}
}
