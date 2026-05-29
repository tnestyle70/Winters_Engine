#pragma once

#include "IScene.h"
#include "Network/Backend/AuthClient.h"
#include "UI/ImageScenePresenter.h"

#include <memory>
#include <string>

class CScene_Login final : public IScene
{
public:
	static std::unique_ptr<CScene_Login> Create();
	~CScene_Login() override = default;

	bool OnEnter() override;
	void OnExit() override;
	void OnUpdate(f32_t dt) override;
	void OnLateUpdate(f32_t dt) override;
	void OnRender() override;
	void OnImGui() override;

private:
	CScene_Login() = default;
	CScene_Login(const CScene_Login&) = delete;
	CScene_Login& operator=(const CScene_Login&) = delete;
	
	void RequestOfflineLogin();
	void RequestOnlineLogin();
	void HandleOnlineLoginResult(const std::string& email, const Client::AuthResult& result);
	void ChangeToMainMenu();

	std::unique_ptr<Client::CAuthClient> m_pAuthClient{};
	CImageScenePresenter m_ImageUI{};
	char m_szEmail[128]{};
	char m_szPassword[128]{};
	std::string m_strStatus{};
	bool_t m_bOfflineLoginRequested = false;
	bool_t m_bLoginInFlight = false;
};
