#include "Scene/Scene_MyInfo.h"

#include "ClientShell/ClientShellBackendService.h"
#include "ClientShell/ClientShellDataStore.h"
#include "ClientShell/ClientShellSession.h"
#include "GameInstance.h"
#include "GamePlay/LoLMatchContextRuntime.h"
#include "Replay/LocalMatchRecord.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_MainMenu.h"
#include "Scene/Scene_MatchLoading.h"

#include <algorithm>

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

namespace
{
	constexpr ImageSourceRect kBackButtonRect{ 30.f, 24.f, 170.f, 64.f };
	constexpr f32_t kReplayReadyPollIntervalSec = 1.f;
	constexpr f32_t kReplayReadyPollTimeoutSec = 60.f;
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

	const CClientShellSession& session = CClientShellSession::Instance();
	m_strExpectedReplayMatchID = session.HasMatchAssignment()
		? session.GetMatchAssignment().strMatchID
		: std::string{};
	m_fReplayReadyPollRemainingSec = m_strExpectedReplayMatchID.empty()
		? 0.f
		: kReplayReadyPollTimeoutSec;
	m_fReplayReadyPollCooldownSec = 0.f;

	CClientShellBackendService& backend = CClientShellBackendService::Instance();
	m_uObservedReplayLibraryRevision = backend.GetReplayLibraryRevision();
	backend.RequestMatchHistory();
	backend.RequestReplayLibrary();
	return true;
}

void CScene_MyInfo::OnExit()
{
	CClientShellBackendService::Instance().CancelReplayPlaybackIntent();
	m_ImageUI.Shutdown();
	m_vAccountReplayItems.clear();
	m_vDebugReplayItems.clear();
	m_vLocalMatchRecords.clear();
}

void CScene_MyInfo::OnUpdate(f32_t dt)
{
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	backend.ProcessCallbacks();

	wstring_t replayPath;
	u32_t perspectiveNetId = 0u;
	if (!m_bSceneTransitionStarted &&
		backend.ConsumeReplayPlaybackPath(replayPath, perspectiveNetId))
	{
		OpenReplay(replayPath, perspectiveNetId);
		return;
	}

	const u32_t replayRevision = backend.GetReplayLibraryRevision();
	if (m_uObservedReplayLibraryRevision != replayRevision)
	{
		m_uObservedReplayLibraryRevision = replayRevision;
		ReloadReplayItems();
	}
	UpdateExpectedReplayPolling(dt);

	if (m_ImageUI.WasSourceRectClicked(kBackButtonRect))
		m_bBackRequested = true;

	if (m_bBackRequested && !m_bSceneTransitionStarted)
	{
		m_bBackRequested = false;
		ChangeToMainMenu();
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
	const ImGuiIO& io = ImGui::GetIO();
	constexpr f32_t kOuterMargin = 24.f;
	constexpr f32_t kColumnGap = 16.f;
	constexpr f32_t kContentTop = 96.f;
	constexpr f32_t kProfileHeight = 210.f;
	const f32_t contentHeight = (std::max)(
		420.f,
		io.DisplaySize.y - kContentTop - kOuterMargin);
	const f32_t leftWidth = std::clamp(
		io.DisplaySize.x * 0.30f,
		280.f,
		320.f);
	const f32_t replayX = kOuterMargin + leftWidth + kColumnGap;
	const f32_t replayWidth = (std::max)(
		320.f,
		io.DisplaySize.x - replayX - kOuterMargin);
	const f32_t historyY = kContentTop + kProfileHeight + kColumnGap;
	const f32_t historyHeight = (std::max)(
		180.f,
		contentHeight - kProfileHeight - kColumnGap);

	ImGui::SetNextWindowPos(
		ImVec2(kOuterMargin, kContentTop),
		ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(
		ImVec2(leftWidth, kProfileHeight),
		ImGuiCond_Appearing);
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

	ImGui::SetNextWindowPos(
		ImVec2(kOuterMargin, historyY),
		ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(
		ImVec2(leftWidth, historyHeight),
		ImGuiCond_Appearing);
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

	ImGui::SetNextWindowPos(
		ImVec2(replayX, kContentTop),
		ImGuiCond_Appearing);
	ImGui::SetNextWindowSize(
		ImVec2(replayWidth, contentHeight),
		ImGuiCond_Appearing);
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

bool_t CScene_MyInfo::HasExpectedCloudReplay() const
{
	if (m_strExpectedReplayMatchID.empty())
		return true;
	const auto& items = CClientShellBackendService::Instance()
		.GetCloudReplayItems();
	return std::any_of(items.begin(), items.end(),
		[this](const Client::CloudReplayItem& item)
		{
			return item.matchId == m_strExpectedReplayMatchID;
		});
}

void CScene_MyInfo::UpdateExpectedReplayPolling(f32_t dt)
{
	if (m_strExpectedReplayMatchID.empty() || HasExpectedCloudReplay())
	{
		m_fReplayReadyPollRemainingSec = 0.f;
		return;
	}
	if (m_fReplayReadyPollRemainingSec <= 0.f)
		return;

	m_fReplayReadyPollRemainingSec = (std::max)(
		0.f, m_fReplayReadyPollRemainingSec - dt);
	m_fReplayReadyPollCooldownSec -= dt;
	CClientShellBackendService& backend =
		CClientShellBackendService::Instance();
	if (m_fReplayReadyPollCooldownSec <= 0.f &&
		!backend.IsReplayRequestInFlight())
	{
		m_fReplayReadyPollCooldownSec = kReplayReadyPollIntervalSec;
		backend.RequestReplayLibrary();
	}
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
	if (!m_strExpectedReplayMatchID.empty() && !HasExpectedCloudReplay())
	{
		if (m_fReplayReadyPollRemainingSec > 0.f)
			ImGui::TextUnformatted("이번 경기 리플레이 업로드 처리 중...");
		else
			ImGui::TextUnformatted(
				"리플레이 준비가 지연되고 있습니다. 서버 upload 로그를 확인하세요.");
	}
	if (items.empty())
	{
		ImGui::TextUnformatted("No cloud replay is available for this account.");
		return;
	}

	const f32_t actionWidth =
		ImGui::CalcTextSize("다시보기").x +
		ImGui::GetStyle().FramePadding.x * 2.f;
	if (ImGui::BeginTable(
		"CloudReplayRows",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Replay", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(
			"Action",
			ImGuiTableColumnFlags_WidthFixed,
			actionWidth);
		for (const Client::CloudReplayItem& item : items)
		{
			ImGui::PushID(item.replayId.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const std::string shortMatchId = item.matchId.size() > 8u
				? item.matchId.substr(0u, 8u)
				: item.matchId;
			const char* pCreatedAt = item.createdAt.empty()
				? "unknown time"
				: item.createdAt.c_str();
			ImGui::Text("%s | match %s | %.2f MiB",
				pCreatedAt,
				shortMatchId.c_str(),
				static_cast<double>(item.sizeBytes) / (1024.0 * 1024.0));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"match %s\nreplay %s\ncreated %s\nformat v%d",
					item.matchId.c_str(),
					item.replayId.c_str(),
					pCreatedAt,
					item.formatVersion);
			}
			ImGui::TableSetColumnIndex(1);
			const bool_t bCanPlay =
				!backend.IsReplayRequestInFlight() &&
				item.perspectiveNetId != 0u;
			if (!bCanPlay)
				ImGui::BeginDisabled();
			if (ImGui::Button("다시보기"))
				backend.RequestReplayPlayback(item);
			if (!bCanPlay)
			{
				ImGui::EndDisabled();
				if (item.perspectiveNetId == 0u &&
					ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("계정 관전자 정보 복구가 필요합니다.");
				}
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
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

	const f32_t actionWidth =
		ImGui::CalcTextSize("재생").x +
		ImGui::GetStyle().FramePadding.x * 2.f;
	if (ImGui::BeginTable(
		"CachedReplayRows",
		2,
		ImGuiTableFlags_SizingStretchProp |
		ImGuiTableFlags_NoSavedSettings))
	{
		ImGui::TableSetupColumn("Replay", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(
			"Action",
			ImGuiTableColumnFlags_WidthFixed,
			actionWidth);
		for (const ReplayListItem& item : items)
		{
			ImGui::PushID(item.path.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const std::string shortName = item.displayName.size() > 8u
				? item.displayName.substr(0u, 8u)
				: item.displayName;
			ImGui::Text("%s | %.2f MiB",
				shortName.c_str(),
				static_cast<double>(item.fileSizeBytes) / (1024.0 * 1024.0));
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", item.displayName.c_str());
			ImGui::TableSetColumnIndex(1);
			const bool_t bCanPlay =
				item.bLocalDebug || item.perspectiveNetId != 0u;
			if (!bCanPlay)
				ImGui::BeginDisabled();
			if (ImGui::Button("재생"))
				OpenReplay(item.path, item.perspectiveNetId);
			if (!bCanPlay)
			{
				ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip(
						"계정 관전자 정보가 없습니다. Cloud / account에서 다시 받으세요.");
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}

void CScene_MyInfo::OpenReplay(
	const wstring_t& path,
	u32_t perspectiveNetId)
{
	if (m_bSceneTransitionStarted)
		return;

	m_bSceneTransitionStarted = true;
	Client::CLoLMatchContextRuntime::Instance().Reset();
	auto pLoadingMatch = CScene_MatchLoading::Create(
		[path, perspectiveNetId]() -> std::unique_ptr<IScene>
		{
			return std::unique_ptr<IScene>(
				new CScene_InGame(path, perspectiveNetId));
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
