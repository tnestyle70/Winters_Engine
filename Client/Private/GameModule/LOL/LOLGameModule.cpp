#include "GameModule/LOL/LOLGameModule.h"

#include "GameMode/GameModeCatalog.h"
#include "GameMode/LOL/LOLGameModeRuntime.h"
#include "GameObject/ChampionDef.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "Manager/Jungle_Manager.h"
#include "Manager/Minion_Manager.h"
#include "Manager/Structure_Manager.h"
#include "Scene/Scene_CustomMode.h"
#include "Scene/Scene_Loading.h"

std::unique_ptr<CLOLGameModule> CLOLGameModule::Create()
{
	return std::unique_ptr<CLOLGameModule>(new CLOLGameModule());
}

bool_t CLOLGameModule::InitializeClient(const GameLaunchConfig& config)
{
	if (m_bInitialized)
		return true;

	m_Config = config;

	CGameModeCatalog::Instance().LoadFromJson();
	const GameModeDef* pModeDef = CGameModeCatalog::Instance().Find(config.strGameModeID);
	if (!pModeDef)
		pModeDef = CGameModeCatalog::Instance().GetDefaultMode();

	if (pModeDef)
	{
		m_pGameMode = CLOLGameModeRuntime::Create(*pModeDef);
		m_pGameMode->OnLoadContent();
	}

	CStructure_Manager::Get()->Initialize(nullptr);
	CJungle_Manager::Get()->Initialize(nullptr);
	CMinion_Manager::Get()->Initialize(nullptr);

	BootstrapChampionModules();
	RegisterAllLegacy();
	CChampionCatalog::Instance().RebuildFromRegistry();

	m_bInitialized = true;
	return true;
}

void CLOLGameModule::ShutdownClient()
{
	if (!m_bInitialized)
		return;

	CJungle_Manager::Get()->Shutdown();
	CMinion_Manager::Get()->Shutdown();
	CStructure_Manager::Get()->Shutdown();

	m_pGameMode.reset();
	m_bInitialized = false;
}

std::unique_ptr<IScene> CLOLGameModule::CreateInitialScene()
{
	return CScene_Loading::Create(
		eSceneID::CustomMode,
		[]() -> std::unique_ptr<IScene>
		{
			return CScene_CustomMode::Create();
		});
}
