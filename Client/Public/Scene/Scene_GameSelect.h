#pragma once
#include "IScene.h"
#include "GameModule/GameLaunchConfig.h"

#include <memory>
#include <string>

class CScene_GameSelect final : public IScene
{
public:
	static std::unique_ptr<CScene_GameSelect> Create();
	~CScene_GameSelect() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnLateUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CScene_GameSelect() = default;
	CScene_GameSelect(const CScene_GameSelect&) = delete;
	CScene_GameSelect& operator=(const CScene_GameSelect&) = delete;

	GameLaunchConfig BuildLaunchConfig(eGameProduct product) const;
	void RequestLaunch(eGameProduct product);
	bool_t TryLaunch(eGameProduct product);
	void DrawProductButton(eGameProduct product);

	std::string m_strStatus{};
	eGameProduct m_ePendingProduct = eGameProduct::None;
	bool_t m_bLaunchRequested = false;
};
