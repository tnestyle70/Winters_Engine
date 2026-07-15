#include "Scene/Scene_Login.h"

#include "ClientShell/ClientShellSession.h"
#include "Dev/SmokeLog.h"
#include "GameInstance.h"
#include "Scene/Scene_MainMenu.h"

#include <cstring>
#include <cwchar>

#include <Windows.h>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
	constexpr const char* AUTH_SERVICE_URL = "http://127.0.0.1:8081";
	constexpr ImageSourceRect kLoginArrowRect{ 252.f, 976.f, 351.f, 1073.f };
}

std::unique_ptr<CScene_Login> CScene_Login::Create()
{
	return std::unique_ptr<CScene_Login>(new CScene_Login());
}

bool CScene_Login::OnEnter()
{
	m_pAuthClient = Client::CAuthClient::Create(AUTH_SERVICE_URL);
	strcpy_s(m_szEmail, sizeof(m_szEmail), "dev@winters.local");
	m_szPassword[0] = '\0';
	m_szLoginID[0] = '\0';
	m_strStatus = "Login";
	m_bOfflineLoginRequested = false;
	m_bLoginInFlight = false;
	m_bShowRegisterPrompt = false;
	m_ImageUI.Initialize(
		L"Texture/UI/Login1.png",
		2301,
		1289);

	const wchar_t* pCommandLine = GetCommandLineW();
	if (pCommandLine && std::wcsstr(pCommandLine, L"--banpick-smoke"))
	{
		Winters::DevSmoke::Log("[LoginSmoke] auto offline login\n");
		RequestOfflineLogin();
	}

	return true;
}

void CScene_Login::OnExit()
{
	m_ImageUI.Shutdown();
	m_pAuthClient.reset();
	m_strStatus.clear();
	m_bOfflineLoginRequested = false;
	m_bLoginInFlight = false;
}

void CScene_Login::OnUpdate(f32_t /*dt*/)
{
	if (m_pAuthClient)
		m_pAuthClient->ProcessCallbacks();

	if (!m_bLoginInFlight && m_ImageUI.WasSourceRectClicked(kLoginArrowRect))
	{
		if (m_szLoginID[0] == '\0')
			m_strStatus = "아이디를 입력하세요";
		else
			RequestIdLogin();
	}

	if (m_bOfflineLoginRequested)
	{
		m_bOfflineLoginRequested = false;
		ChangeToMainMenu();
		return;
	}
}

void CScene_Login::OnLateUpdate(f32_t /*dt*/)
{}

void CScene_Login::OnRender()
{
	m_ImageUI.Render();
}

void CScene_Login::OnImGui()
{
	const ImGuiViewport* pViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		ImVec2(pViewport->WorkPos.x + pViewport->WorkSize.x * 0.5f,
			pViewport->WorkPos.y + pViewport->WorkSize.y * 0.62f),
		ImGuiCond_Once,
		ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(380.f, 0.f), ImGuiCond_Once);
	if (!ImGui::Begin("계정 로그인", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::End();
		return;
	}

	ImGui::TextUnformatted("아이디");
	ImGui::SetNextItemWidth(-1.f);
	const bool_t bEnterPressed = ImGui::InputText(
		"##login_id", m_szLoginID, sizeof(m_szLoginID),
		ImGuiInputTextFlags_EnterReturnsTrue);

	ImGui::BeginDisabled(m_bLoginInFlight || m_szLoginID[0] == '\0');
	if (ImGui::Button("로그인", ImVec2(120.f, 0.f)) || (bEnterPressed && !m_bLoginInFlight && m_szLoginID[0] != '\0'))
		RequestIdLogin();

	// 회원가입은 404 유도와 무관하게 항상 노출한다.
	ImGui::SameLine();
	if (ImGui::Button("회원 가입", ImVec2(120.f, 0.f)))
		RequestIdRegister();
	ImGui::EndDisabled();

	// 비회원 입장은 백엔드 없이 오프라인 계정으로 바로 시작한다.
	ImGui::BeginDisabled(m_bLoginInFlight);
	if (ImGui::Button("비회원으로 시작", ImVec2(-1.f, 0.f)))
		RequestOfflineLogin();
	ImGui::EndDisabled();

	if (!m_strStatus.empty())
	{
		ImGui::Spacing();
		ImGui::TextWrapped("%s", m_strStatus.c_str());
	}

	ImGui::End();
}

void CScene_Login::RequestOfflineLogin()
{
	CClientShellSession::Instance().SetOfflineAccount("Offline Player");
	m_strStatus = "Launching selected module...";
	m_bOfflineLoginRequested = true;
}

void CScene_Login::RequestOnlineLogin()
{
	if (!m_pAuthClient)
	{
		m_strStatus = "Auth client is not initialized";
		return;
	}

	if (m_szEmail[0] == '\0' || m_szPassword[0] == '\0')
	{
		m_strStatus = "Email and password are required for online login";
		return;
	}

	const std::string strEmail = m_szEmail;
	const std::string strPassword = m_szPassword;

	m_bLoginInFlight = true;
	m_strStatus = "Signing in...";

	m_pAuthClient->Login(
		strEmail,
		strPassword,
		[this, strEmail](const Client::AuthResult& result)
		{
			HandleOnlineLoginResult(strEmail, result);
		});
}

void CScene_Login::HandleOnlineLoginResult(const std::string& email, const Client::AuthResult& result)
{
	m_bLoginInFlight = false;

	if (!result.success)
	{
		m_strStatus = result.error.empty() ? "Online login failed" : "Online login failed: " + result.error;
		return;
	}

	CClientShellSession::Instance().SetAuthenticatedAccount(
		email,
		email,
		result.accessToken);

	m_strStatus = "Login successful";
	m_bOfflineLoginRequested = true;
}

void CScene_Login::RequestIdLogin()
{
	if (!m_pAuthClient || m_bLoginInFlight || m_szLoginID[0] == '\0')
		return;

	m_bLoginInFlight = true;
	m_bShowRegisterPrompt = false;
	m_strStatus = "로그인 중...";

	m_pAuthClient->LoginByID(
		m_szLoginID,
		[this](const Client::AuthResult& result)
		{
			HandleIdAuthResult(result);
		});
}

void CScene_Login::RequestIdRegister()
{
	if (!m_pAuthClient || m_bLoginInFlight || m_szLoginID[0] == '\0')
		return;

	m_bLoginInFlight = true;
	m_bRegisterInFlight = true;
	m_strStatus = "회원 가입 중...";

	m_pAuthClient->RegisterByID(
		m_szLoginID,
		[this](const Client::AuthResult& result)
		{
			HandleIdAuthResult(result);
		});
}

void CScene_Login::HandleIdAuthResult(const Client::AuthResult& result)
{
	m_bLoginInFlight = false;
	const bool_t bWasRegister = m_bRegisterInFlight;
	m_bRegisterInFlight = false;

	if (result.success)
	{
		// 회원가입 성공은 자동 입장하지 않는다 — 문구를 보여주고
		// 사용자가 로그인 버튼으로 명시적으로 입장한다.
		if (bWasRegister)
		{
			m_strStatus = "회원가입 성공했습니다. 로그인 버튼으로 입장하세요.";
			m_bShowRegisterPrompt = false;
			return;
		}

		CClientShellSession::Instance().SetAuthenticatedAccount(
			result.userID,
			result.displayName,
			result.accessToken);

		m_strStatus = "로그인 성공";
		m_bShowRegisterPrompt = false;
		m_bOfflineLoginRequested = true;
		return;
	}

	// DB에 없는 회원 → 회원 가입 유도 (Services /auth/id/login 404 계약).
	if (result.statusCode == 404)
	{
		m_bShowRegisterPrompt = true;
		m_strStatus = "존재하지 않는 회원입니다. 회원 가입을 하세요";
		return;
	}

	if (result.statusCode == 409)
	{
		m_bShowRegisterPrompt = false;
		m_strStatus = "이미 존재하는 아이디입니다. 로그인을 사용하세요";
		return;
	}

	m_strStatus = result.error.empty()
		? "로그인 실패 (백엔드 서비스 상태를 확인하세요)"
		: "로그인 실패: " + result.error;
}

void CScene_Login::ChangeToMainMenu()
{
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::MainMenu),
		CScene_MainMenu::Create()
	);
}
