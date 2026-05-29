#pragma once
#include "GameModule/IGameModule.h"
#include "GameMode/IGameMode.h"

class CLOLGameModule final : public IGameModule
{
public:
	static std::unique_ptr<CLOLGameModule> Create();
	~CLOLGameModule() override = default;

	eGameProduct GetProductID() const override { return eGameProduct::LOL; }
	const char* GetDisplayName() const override { return "LOL"; }
	bool_t IsAvailable() const override { return true; }

	bool_t InitializeClient(const GameLaunchConfig& config) override;
	void ShutdownClient() override;

	eSceneID GetInitialSceneID() const override { return eSceneID::SceneLoading; }
	std::unique_ptr<IScene> CreateInitialScene() override;

private:
	CLOLGameModule() = default;
	CLOLGameModule(const CLOLGameModule&) = delete;
	CLOLGameModule& operator=(const CLOLGameModule&) = delete;

	bool_t m_bInitialized = false;
	GameLaunchConfig m_Config{};
	std::unique_ptr<IGameMode> m_pGameMode{};
};
