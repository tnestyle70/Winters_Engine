#include "GameApp.h"

#include "ClientShell/ClientShellSession.h"
#include "GameInstance.h"
#include "GameModule/GameModuleRegistry.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_Login.h"
#include "Scene/Scene_MatchLoading.h"

#include <Windows.h>
#include <cwchar>
#include <string>
#include <utility>

namespace
{
	// --replay=<path>: 로그인/로비를 건너뛰고 리플레이 재생으로 직행 (프로파일링 재현 시나리오).
	std::wstring ParseReplayBootPath()
	{
		const wchar_t* pCommandLine = ::GetCommandLineW();
		if (!pCommandLine)
			return {};

		const wchar_t* pPrefix = L"--replay=";
		const wchar_t* pFound = wcsstr(pCommandLine, pPrefix);
		if (!pFound)
			return {};

		const wchar_t* pValue = pFound + wcslen(pPrefix);
		const bool_t bQuoted = *pValue == L'"';
		if (bQuoted)
			++pValue;
		const wchar_t* pEnd = pValue;
		while (*pEnd != L'\0' &&
			(bQuoted ? *pEnd != L'"' : (*pEnd != L' ' && *pEnd != L'\t')))
		{
			++pEnd;
		}
		return std::wstring(pValue, pEnd);
	}

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

	const std::wstring replayBootPath = ParseReplayBootPath();
	if (!replayBootPath.empty())
	{
		auto pLoadingMatch = CScene_MatchLoading::Create(
			[replayBootPath]() -> std::unique_ptr<IScene>
			{
				return std::unique_ptr<IScene>(new CScene_InGame(replayBootPath));
			}, 1.f);

		CGameInstance::Get()->Change_Scene(
			static_cast<uint32_t>(eSceneID::MatchLoading),
			std::move(pLoadingMatch));

		return true;
	}

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
