#include "Scene/Scene_MyInfo.h"

#include "ClientShell/ClientShellBackendService.h"
#include "ClientShell/ClientShellDataStore.h"
#include "ClientShell/ClientShellSession.h"
#include "GameInstance.h"
#include "Replay/LocalMatchRecord.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_MainMenu.h"
#include "Scene/Scene_MatchLoading.h"

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
	constexpr ImageSourceRect kBackButtonRect{ 30.f, 24.f, 170.f, 64.f };
}

std::unique_ptr<CScene_MyInfo> CScene_MyInfo::Create()
{
	return std::unique_ptr<CScene_MyInfo>(new CScene_MyInfo());
}

bool CScene_MyInfo::OnEnter()
{
	m_ImageUI.Initialize(
		L"Texture/UI/MatchLoadingBackground.png",
		g_iWinSizeX,
		g_iWinSizeY);

	m_bBackRequested = false;
	m_bSceneTransitionStarted = false;
	ReloadReplayItems();
	ReloadLocalMatchRecords();

	CClientShellBackendService& backend = CClientShellBackendService::Instance();
	m_uObservedReplayLibraryRevision = backend.GetReplayLibraryRevision();
	backend.RequestMatchHistory();
	backend.RequestReplayLibrary();
	return true;
}

void CScene_MyInfo::OnExit()
{
	m_ImageUI.Shutdown();
	m_vAccountReplayItems.clear();
	m_vDebugReplayItems.clear();
	m_vLocalMatchRecords.clear();
}

void CScene_MyInfo::OnUpdate(f32_t /*dt*/)
{
	CClientShellBackendService::Instance().ProcessCallbacks();
	const u32_t replayRevision =
		CClientShellBackendService::Instance().GetReplayLibraryRevision();
	if (m_uObservedReplayLibraryRevision != replayRevision)
	{
		m_uObservedReplayLibraryRevision = replayRevision;
		ReloadReplayItems();
	}

	if (m_ImageUI.WasSourceRectClicked(kBackButtonRect))
		m_bBackRequested = true;

	if (m_bBackRequested && !m_bSceneTransitionStarted)
	{
		m_bBackRequested = false;
		ChangeToMainMenu();
		return;
	}
}

void CScene_MyInfo::OnLateUpdate(f32_t /*dt*/)
{}

void CScene_MyInfo::OnRender()
{
	if (!m_ImageUI.Begin())
		return;

	m_ImageUI.DrawBackground();
	m_ImageUI.DrawSourceRect(kBackButtonRect, Vec4(0.02f, 0.06f, 0.10f, 0.85f));
	m_ImageUI.DrawSourceRectOutline(kBackButtonRect, Vec4(0.68f, 0.52f, 0.18f, 0.95f), 1.5f);
	m_ImageUI.End();
}

void CScene_MyInfo::OnImGui()
{
	if (ImDrawList* pDraw = ImGui::GetForegroundDrawList())
	{
		ImageScreenRect screenRect{};
		if (m_ImageUI.SourceRectToScreen(ImageSourceRect{ 46.f, 34.f, 47.f, 35.f }, screenRect))
			pDraw->AddText(ImVec2(screenRect.fX, screenRect.fY), IM_COL32(235, 220, 180, 255), "← 뒤로가기");
		if (m_ImageUI.SourceRectToScreen(ImageSourceRect{ 560.f, 34.f, 561.f, 35.f }, screenRect))
			pDraw->AddText(ImVec2(screenRect.fX, screenRect.fY), IM_COL32(240, 230, 200, 255), "나의 정보");
	}

	const CClientShellDataStore& store = CClientShellDataStore::Instance();
	const ShellProfileSummary& profile = store.GetProfile();

	ImGui::SetNextWindowPos(ImVec2(60.f, 110.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.f, 210.f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("내 프로필", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		ImGui::Text("소환사명: %s", profile.strDisplayName.c_str());
		ImGui::Text("보유 RP: %d", profile.iRP);
		ImGui::Text("전적: %d승 %d패", profile.iWins, profile.iLosses);
		ImGui::Text("MMR: %d", profile.iMMR);
		if (!store.IsInitialSyncReady())
			ImGui::TextDisabled("(백엔드 동기화 대기 중)");
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(60.f, 340.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(330.f, 300.f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("전적 기록", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		if (ImGui::Button("새로고침"))
		{
			CClientShellBackendService::Instance().RequestMatchHistory();
			ReloadLocalMatchRecords();
		}

		const std::vector<Client::MatchRecord>& records = store.GetMatchHistory();
		if (records.empty() && m_vLocalMatchRecords.empty())
		{
			ImGui::TextUnformatted("저장된 전적이 없습니다");
		}
		else
		{
			for (const Client::MatchRecord& record : records)
			{
				ImGui::Text("[%s] %d/%d/%d",
					record.result.c_str(), record.kills, record.deaths, record.assists);
			}
			for (const std::string& localRecord : m_vLocalMatchRecords)
				ImGui::TextUnformatted(localRecord.c_str());
		}
	}
	ImGui::End();

	ImGui::SetNextWindowPos(ImVec2(430.f, 110.f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(480.f, 530.f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("리플레이", nullptr, ImGuiWindowFlags_NoCollapse))
	{
		if (ImGui::Button("새로고침"))
		{
			ReloadReplayItems();
			CClientShellBackendService::Instance().RequestReplayLibrary();
		}

		if (ImGui::BeginTabBar("ReplayLibraryTabs"))
		{
			if (ImGui::BeginTabItem("Cloud / account"))
			{
				DrawCloudReplayItems();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("내 리플레이"))
			{
				DrawReplayItems(
					m_vAccountReplayItems,
					"현재 계정에 다운로드된 리플레이가 없습니다");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("로컬/디버그"))
			{
				ImGui::TextDisabled("개발 경로 Replay/*.wrpl — 계정 라이브러리와 분리됨");
				DrawReplayItems(
					m_vDebugReplayItems,
					"로컬 디버그 리플레이가 없습니다");
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}

void CScene_MyInfo::DrawCloudReplayItems()
{
	CClientShellBackendService& backend = CClientShellBackendService::Instance();
	if (!backend.IsConfigured())
	{
		ImGui::TextDisabled("Cloud replays require an online account.");
		return;
	}
	if (backend.IsReplayRequestInFlight())
		ImGui::TextDisabled("Syncing or downloading...");

	const std::vector<Client::CloudReplayItem>& items =
		backend.GetCloudReplayItems();
	if (items.empty())
	{
		ImGui::TextUnformatted("No cloud replay is available for this account.");
		return;
	}

	for (const Client::CloudReplayItem& item : items)
	{
		ImGui::PushID(item.replayId.c_str());
		ImGui::Text("match %s | %.2f MiB | v%d",
			item.matchId.c_str(),
			static_cast<double>(item.sizeBytes) / (1024.0 * 1024.0),
			item.formatVersion);
		ImGui::SameLine();
		if (!backend.IsReplayRequestInFlight() && ImGui::Button("Download"))
			backend.RequestReplayDownload(item);
		ImGui::PopID();
	}
}

void CScene_MyInfo::ReloadLocalMatchRecords()
{
	m_vLocalMatchRecords = Winters::LoadLocalMatchRecordSummaries(
		CClientShellSession::Instance().GetUserID());
}

void CScene_MyInfo::ReloadReplayItems()
{
	const std::string& strUserID = CClientShellSession::Instance().GetUserID();
	m_vAccountReplayItems = CReplayLibrary::ListAccountReplayCache(strUserID);
	m_vDebugReplayItems = CReplayLibrary::ListLocalDebugReplays();
}

void CScene_MyInfo::DrawReplayItems(
	const std::vector<ReplayListItem>& items,
	const char* pEmptyText)
{
	if (items.empty())
	{
		ImGui::TextUnformatted(pEmptyText);
		return;
	}

	for (const ReplayListItem& item : items)
	{
		ImGui::PushID(item.path.c_str());
		ImGui::TextUnformatted(item.displayName.c_str());
		ImGui::SameLine();
		if (ImGui::Button("재생"))
			OpenReplay(item.path);
		ImGui::PopID();
	}
}

void CScene_MyInfo::OpenReplay(const wstring_t& path)
{
	if (m_bSceneTransitionStarted)
		return;

	m_bSceneTransitionStarted = true;
	auto pLoadingMatch = CScene_MatchLoading::Create(
		[path]() -> std::unique_ptr<IScene>
		{
			return std::unique_ptr<IScene>(new CScene_InGame(path));
		}, 1.f);

	CGameInstance::Get()->Change_Scene(
		static_cast<u32_t>(eSceneID::MatchLoading),
		std::move(pLoadingMatch));
}

void CScene_MyInfo::ChangeToMainMenu()
{
	if (m_bSceneTransitionStarted)
		return;

	m_bSceneTransitionStarted = true;
	CGameInstance::Get()->Change_Scene(
		static_cast<uint32_t>(eSceneID::MainMenu),
		CScene_MainMenu::Create());
}
