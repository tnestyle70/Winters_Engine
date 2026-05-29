#pragma once

#include "GameMode/IGameMode.h"

#include <memory>

class CLOLGameModeRuntime final : public IGameMode
{
public:
	static std::unique_ptr<CLOLGameModeRuntime> Create(const GameModeDef& def);
	~CLOLGameModeRuntime() override = default;

	const GameModeDef& GetDef() const override { return m_Def; }
	bool_t OnLoadContent() override;
	bool_t OnCreateWorld(CWorld& world) override;
	void OnMatchStart() override;
	void Tick(f32_t dt) override;
	void OnEntityKilled(EntityID victim, EntityID killer) override;
	bool_t IsGameOver() const override { return m_bGameOver; }

private:
	explicit CLOLGameModeRuntime(const GameModeDef& def);
	CLOLGameModeRuntime(const CLOLGameModeRuntime&) = delete;
	CLOLGameModeRuntime& operator=(const CLOLGameModeRuntime&) = delete;

	GameModeDef m_Def{};
	bool_t m_bGameOver = false;
};
