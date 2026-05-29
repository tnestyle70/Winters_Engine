#include "GameApp.h"

#include "ClientShell/ClientShellSession.h"
#include "GameInstance.h"
#include "GameModule/GameModuleRegistry.h"
#include "Scene/Scene_Login.h"

#include <utility>

namespace
{
	GameLaunchConfig BuildDefaultLOLLaunchConfig()
	{
		GameLaunchConfig config{};
		config.eProduct = eGameProduct::LOL;
		config.strGameModeID = "summoners_rift";
		config.strContentRoot = L"Client/Bin/Resource";
		config.strServiceNamespace = L"winters.lol";
		config.strServerEndpoint = L"http://127.0.0.1:8080";
		config.bUseOnlineServices = false;
		config.bUseEditorTools = true;
		return config;
	}
}

bool CGameApp::OnInit()
{
	CGameModuleRegistry::Instance().RegisterDefaults();
	CClientShellSession::Instance().SetSelectedProduct(
		eGameProduct::LOL,
		BuildDefaultLOLLaunchConfig());

	auto pLogin = CScene_Login::Create();

	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::Login),
		std::move(pLogin));

	return true;
}

void CGameApp::OnShutdown()
{
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::End),
		nullptr);

	CGameModuleRegistry::Instance().ShutdownActiveModule();
}
