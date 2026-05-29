#pragma once
#include "IScene.h"
#include "UI/ImageScenePresenter.h"

#include <memory>
#include <string>

class CScene_MainMenu final : public IScene
{
public:
	static std::unique_ptr<CScene_MainMenu> Create();
	~CScene_MainMenu() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnLateUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CScene_MainMenu() = default;
	CScene_MainMenu(const CScene_MainMenu&) = delete;
	CScene_MainMenu& operator=(const CScene_MainMenu&) = delete;

	void RequestPlay();
	void RequestLogout();

	bool_t LaunchSelectedProduct();
	void ChangeToLogin();

	std::string m_strStatus{};
	CImageScenePresenter m_ImageUI{};
	bool_t m_bPlayRequested = false;
	bool_t m_bLogoutRequested = false;
};
