#include "Scene/Scene_CustomMode.h"

#include "Core/CInput.h"
#include "GameInstance.h"
#include "GamePlay/ChampionCatalog.h"
#include "Network/Client/GameSessionClient.h"
#include "Scene/LobbyRosterHelpers.h"
#include "Scene/Scene_BanPick.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_MatchLoading.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"
#include "Shared/Schemas/Generated/cpp/LobbyTypes_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

#pragma push_macro("new")
#undef new
#include <imgui.h>
#pragma pop_macro("new")

#include <cstdio>
#include <utility>
#include <vector>

namespace
{
	constexpr ImageSourceRect kGamePlayRect{ 465.76f, 662.30f, 635.43f, 702.44f };
	constexpr ImageSourceRect kBlueRosterRect{ 41.59f, 213.24f, 528.14f, 455.74f };
	constexpr ImageSourceRect kRedRosterRect{ 544.77f, 213.24f, 1031.32f, 455.74f };
	constexpr f32_t kBlueSlotLeft = 41.59f;
	constexpr f32_t kRedSlotLeft = 544.77f;
	constexpr f32_t kSlotRightOffset = 486.55f;
	constexpr f32_t kSlotTop = 213.24f;
	constexpr f32_t kSlotHeight = 48.50f;

	bool_t HasLocalServerMatchSlot(const GameContext& context)
	{
		return context.bUseNetworkRoster &&
			context.MySessionId != 0 &&
			context.MyNetId != 0 &&
			context.MySlotId < kGameRosterSlotCount;
	}
}

std::unique_ptr<CScene_CustomMode> CScene_CustomMode::Create()
{
	return std::unique_ptr<CScene_CustomMode>(new CScene_CustomMode());
}

bool CScene_CustomMode::OnEnter()
{
	EnsureLobbyChampionCatalogReady();

	m_bSceneTransitionStarted = false;
	m_SelectedSlotId = 0;
	m_ImageUI.Initialize(
		L"Client/Bin/Resource/Texture/UI/CustomMode1.png",
		1280,
		720);
	m_vReplayItems = CReplayLibrary::ListLocalReplays();

	m_bServerLobbyActive = CGameSessionClient::Instance().Connect();
	if (!m_bServerLobbyActive)
	{
		GameContext& context = CGameInstance::Get()->Get_GameContext();
		InitializeLocalCustomRoom(context);
		m_SelectedSlotId = context.MySlotId;
	}
	return true;
}

void CScene_CustomMode::OnExit()
{
	m_ImageUI.Shutdown();
}

void CScene_CustomMode::OnUpdate(f32_t /*dt*/)
{
	if (m_bSceneTransitionStarted)
		return;

	if (m_bServerLobbyActive)
	{
		CGameSessionClient& session = CGameSessionClient::Instance();
		session.Pump();
		GameContext& context = CGameInstance::Get()->Get_GameContext();

		if (session.HasLobbyState())
			session.CopyLobbyToGameContext(context);

		if (session.GetLobbyPhase() == static_cast<u8_t>(Shared::Schema::LobbyPhase::ChampionSelect))
		{
			ChangeToChampionSelectScene();
			return;
		}

		if (session.IsServerLoading() && HasLocalServerMatchSlot(context))
		{
			session.ClearServerLoading();
			StartMatchLoadingScene();
			return;
		}

		if (session.IsGameStarting() && HasLocalServerMatchSlot(context))
		{
			session.ClearGameStarting();
			StartMatchLoadingScene();
			return;
		}
	}

	HandleInput();
}

void CScene_CustomMode::OnLateUpdate(f32_t /*dt*/)
{
}

void CScene_CustomMode::OnRender()
{
	m_ImageUI.Render();
}

void CScene_CustomMode::OnImGui()
{
	RenderRosterOverlay();
	RenderReplayPanel();
}

void CScene_CustomMode::HandleInput()
{
	if (m_bServerLobbyActive)
		HandleServerInput();
	else
		HandleLocalInput();
}

void CScene_CustomMode::HandleServerInput()
{
	CGameSessionClient& session = CGameSessionClient::Instance();
	if (!session.HasLobbyState())
		return;

	if (IsGamePlayClicked())
	{
		session.SendLobbyCommand(Shared::Schema::LobbyCommandKind::StartGame, 0);
		return;
	}

	if (ImGui::GetIO().WantCaptureMouse)
		return;
}

void CScene_CustomMode::HandleLocalInput()
{
	if (IsGamePlayClicked())
	{
		ChangeToChampionSelectScene();
		return;
	}

	if (ImGui::GetIO().WantCaptureMouse)
		return;
}

void CScene_CustomMode::ChangeToChampionSelectScene()
{
	if (m_bSceneTransitionStarted)
		return;

	m_bSceneTransitionStarted = true;
	CGameInstance::Get()->Change_Scene(
		static_cast<u32_t>(eSceneID::BanPick),
		CScene_BanPick::Create());
}

void CScene_CustomMode::StartMatchLoadingScene()
{
	if (m_bSceneTransitionStarted)
		return;

	m_bSceneTransitionStarted = true;
	auto pLoadingMatch = CScene_MatchLoading::Create(
		[]() -> std::unique_ptr<IScene>
		{
			return std::unique_ptr<IScene>(new CScene_InGame());
		}, 3.f);

	CGameInstance::Get()->Change_Scene(
		static_cast<u32_t>(eSceneID::MatchLoading),
		std::move(pLoadingMatch));
}

void CScene_CustomMode::RenderReplayPanel()
{
	ImGui::SetNextWindowSize(ImVec2(360.f, 220.f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Replay Files"))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Refresh"))
		m_vReplayItems = CReplayLibrary::ListLocalReplays();

	if (m_vReplayItems.empty())
	{
		ImGui::TextUnformatted("No .wrpl files");
		ImGui::End();
		return;
	}

	for (const ReplayListItem& item : m_vReplayItems)
	{
		ImGui::PushID(item.path.c_str());
		ImGui::TextUnformatted(item.displayName.c_str());
		ImGui::SameLine();
		if (ImGui::Button("Play"))
			OpenReplay(item.path);
		ImGui::PopID();
	}

	ImGui::End();
}

void CScene_CustomMode::OpenReplay(const wstring_t& path)
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

u8_t CScene_CustomMode::ResolveClickedSlot(f32_t fSourceX, f32_t fSourceY) const
{
	if (fSourceY < kSlotTop || fSourceY >= kSlotTop + kSlotHeight * 5.f)
		return kInvalidGameRosterSlot;

	u8_t side = 0;
	if (fSourceX >= kBlueSlotLeft && fSourceX <= kBlueSlotLeft + kSlotRightOffset)
		side = 0;
	else if (fSourceX >= kRedSlotLeft && fSourceX <= kRedSlotLeft + kSlotRightOffset)
		side = 1;
	else
		return kInvalidGameRosterSlot;

	const u8_t row = static_cast<u8_t>((fSourceY - kSlotTop) / kSlotHeight);
	return static_cast<u8_t>(side * 5u + row);
}

bool_t CScene_CustomMode::IsGamePlayClicked() const
{
	return m_ImageUI.WasSourceRectClicked(kGamePlayRect);
}

bool_t CScene_CustomMode::AddBotToFirstEmptySlot(u32_t beginSlot, u32_t endSlot)
{
	if (m_bServerLobbyActive)
	{
		CGameSessionClient& session = CGameSessionClient::Instance();
		const GameContext& context = session.GetLobbyContext();
		for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
		{
			if (!IsSlotEmpty(context.Roster[i]))
				continue;

			m_SelectedSlotId = static_cast<u8_t>(i);
			return session.SendLobbyCommand(
				Shared::Schema::LobbyCommandKind::SetBotChampion,
				static_cast<u8_t>(i),
				GetDefaultBotChampion(i),
				2);
		}
		return false;
	}

	GameContext& context = CGameInstance::Get()->Get_GameContext();
	for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
	{
		if (!IsSlotEmpty(context.Roster[i]))
			continue;

		AddBotToSlot(context, i, GetDefaultBotChampion(i));
		m_SelectedSlotId = static_cast<u8_t>(i);
		return true;
	}
	return false;
}

bool_t CScene_CustomMode::JoinSlot(u8_t slotId)
{
	if (slotId >= kGameRosterSlotCount)
		return false;

	m_SelectedSlotId = slotId;
	if (m_bServerLobbyActive)
	{
		return CGameSessionClient::Instance().SendLobbyCommand(
			Shared::Schema::LobbyCommandKind::JoinSlot,
			slotId);
	}

	JoinLocalPlayerSlot(CGameInstance::Get()->Get_GameContext(), slotId);
	return true;
}

bool_t CScene_CustomMode::RemoveBotAndCompactTeam(u32_t beginSlot, u32_t endSlot, u8_t slotId)
{
	if (m_bServerLobbyActive)
		return SendBotRemoval(slotId);

	return CompactLocalBotRemoval(
		CGameInstance::Get()->Get_GameContext(),
		beginSlot,
		endSlot,
		slotId);
}

bool_t CScene_CustomMode::SendBotRemoval(u8_t slotId)
{
	return CGameSessionClient::Instance().SendLobbyCommand(
		Shared::Schema::LobbyCommandKind::SetBotChampion,
		slotId,
		eChampion::END);
}

bool_t CScene_CustomMode::CompactLocalBotRemoval(GameContext& context, u32_t beginSlot, u32_t endSlot, u8_t slotId)
{
	if (slotId < beginSlot || slotId >= endSlot || slotId >= kGameRosterSlotCount)
		return false;
	if (!context.Roster[slotId].bBot)
		return false;

	std::vector<GameRosterSlot> occupied;
	for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
	{
		if (i == slotId)
			continue;
		if (IsSlotOccupied(context.Roster[i]))
			occupied.push_back(context.Roster[i]);
	}

	u32_t occupiedIndex = 0;
	for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
	{
		if (occupiedIndex < occupied.size())
		{
			context.Roster[i] = occupied[occupiedIndex++];
			context.Roster[i].slotId = static_cast<u8_t>(i);
			context.Roster[i].team = GetTeamFromSlotId(i);

			if (context.Roster[i].bHuman &&
				context.Roster[i].sessionId == context.MySessionId)
			{
				context.MySlotId = static_cast<u8_t>(i);
				context.MyTeam = context.Roster[i].team;
			}
		}
		else
		{
			context.Roster[i] = GameRosterSlot{};
		}
	}

	return true;
}

void CScene_CustomMode::SetBotLane(u8_t slotId, u8_t lane)
{
	if (m_bServerLobbyActive)
	{
		CGameSessionClient::Instance().SendLobbyCommand(
			Shared::Schema::LobbyCommandKind::SetBotLane,
			slotId,
			eChampion::END,
			0,
			lane);
		return;
	}

	SetBotSlotLane(CGameInstance::Get()->Get_GameContext(), slotId, lane);
}

bool_t CScene_CustomMode::SetBotChampion(u8_t slotId, eChampion champion)
{
	if (slotId >= kGameRosterSlotCount || !IsRosterChampionSupported(champion))
		return false;

	if (m_bServerLobbyActive)
	{
		CGameSessionClient& session = CGameSessionClient::Instance();
		const GameContext& context = session.GetLobbyContext();
		if (!context.Roster[slotId].bBot)
			return false;

		m_SelectedSlotId = slotId;
		return session.SendLobbyCommand(
			Shared::Schema::LobbyCommandKind::SetBotChampion,
			slotId,
			champion,
			context.Roster[slotId].botDifficulty ? context.Roster[slotId].botDifficulty : 2);
	}

	GameContext& context = CGameInstance::Get()->Get_GameContext();
	if (!context.Roster[slotId].bBot)
		return false;

	m_SelectedSlotId = slotId;
	AssignChampionToSlot(context, slotId, champion);
	return true;
}

void CScene_CustomMode::RenderRosterOverlay()
{
	const GameContext& context = m_bServerLobbyActive
		? CGameSessionClient::Instance().GetLobbyContext()
		: CGameInstance::Get()->Get_GameContext();

	RenderTeamRoster(context, 0, 5, kBlueRosterRect);
	RenderTeamRoster(context, 5, 10, kRedRosterRect);
}

void CScene_CustomMode::RenderBotChampionButton(u8_t slotId, f32_t width, f32_t height)
{
	const GameContext& context = m_bServerLobbyActive
		? CGameSessionClient::Instance().GetLobbyContext()
		: CGameInstance::Get()->Get_GameContext();

	if (slotId >= kGameRosterSlotCount || !context.Roster[slotId].bBot)
		return;

	const GameRosterSlot& slot = context.Roster[slotId];
	const eChampion champion = slot.champion;
	char label[64]{};
	sprintf_s(label, "%s##BotChampion", GetRosterChampionLabel(champion));

	if (ImGui::Button(label, ImVec2(width, height)))
		ImGui::OpenPopup("BotChampionPopup");

	if (ImGui::BeginPopup("BotChampionPopup"))
	{
		const std::vector<ChampionCatalogEntry>& champions =
			CChampionCatalog::Instance().GetBotChampions();

		for (const ChampionCatalogEntry& entry : champions)
		{
			if (!IsRosterChampionSupported(entry.id))
				continue;

			const bool_t bSelected = entry.id == champion;
			if (ImGui::Selectable(GetRosterChampionLabel(entry.id), bSelected))
				SetBotChampion(slotId, entry.id);

			if (bSelected)
				ImGui::SetItemDefaultFocus();
		}

		ImGui::Separator();
		const char* laneLabels[] = { "Top", "Mid", "Bot" };
		const u8_t laneValues[] = { kGameSimLaneTop, kGameSimLaneMid, kGameSimLaneBot };
		for (u32_t i = 0; i < 3u; ++i)
		{
			char laneLabel[24]{};
			sprintf_s(laneLabel, "Lane: %s", laneLabels[i]);
			if (ImGui::Selectable(laneLabel, slot.botLane == laneValues[i]))
				SetBotLane(slotId, laneValues[i]);
		}

		ImGui::Separator();
		if (ImGui::Selectable("Remove Bot"))
		{
			const u32_t beginSlot = slotId < 5 ? 0u : 5u;
			RemoveBotAndCompactTeam(beginSlot, beginSlot + 5u, slotId);
		}

		ImGui::EndPopup();
	}
}

void CScene_CustomMode::RenderTeamRoster(
	const GameContext& context,
	u32_t beginSlot,
	u32_t endSlot,
	const ImageSourceRect& rect)
{
	ImageScreenRect screen{};
	if (!m_ImageUI.SourceRectToScreen(rect, screen))
		return;

	ImGui::SetNextWindowPos(ImVec2(screen.fX, screen.fY), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(screen.fW, screen.fH), ImGuiCond_Always);

	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse;

	char windowName[32]{};
	sprintf_s(windowName, "CustomModeRoster_%u", beginSlot);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));

	if (ImGui::Begin(windowName, nullptr, flags))
	{
		const u8_t team = static_cast<u8_t>(beginSlot < 5 ? 0 : 1);
		const bool_t bLocalInTeam =
			context.MySlotId < kGameRosterSlotCount &&
			context.MyTeam == team;
		const u32_t slotCount = endSlot > beginSlot ? endSlot - beginSlot : 1u;
		const f32_t rowHeight = screen.fH / static_cast<f32_t>(slotCount);
		const f32_t rowPadX = 6.f;
		const f32_t rowPadY = 4.f;
		const f32_t rowWidth = screen.fW > rowPadX * 2.f ? screen.fW - rowPadX * 2.f : screen.fW;
		const f32_t buttonHeight = rowHeight > rowPadY * 2.f ? rowHeight - rowPadY * 2.f : rowHeight;
		bool_t bEmptyControlDrawn = false;

		for (u32_t i = beginSlot; i < endSlot && i < kGameRosterSlotCount; ++i)
		{
			const GameRosterSlot& slot = context.Roster[i];
			const f32_t rowY = static_cast<f32_t>(i - beginSlot) * rowHeight + rowPadY;
			ImGui::PushID(static_cast<int>(i));
			ImGui::SetCursorPos(ImVec2(rowPadX, rowY));

			if (slot.bBot)
			{
				RenderBotChampionButton(static_cast<u8_t>(i), rowWidth, buttonHeight);
			}
			else if (slot.bHuman)
			{
				char label[48]{};
				sprintf_s(label, "Player %u", slot.sessionId);
				ImGui::TextUnformatted(label);
			}
			else if (!bEmptyControlDrawn)
			{
				f32_t addW = rowWidth;
				if (!bLocalInTeam)
				{
					const f32_t joinW = rowWidth > 120.f ? 96.f : rowWidth * 0.45f;
					char joinLabel[24]{};
					sprintf_s(joinLabel, "Join %s", team == 0 ? "Blue" : "Red");
					if (ImGui::Button(joinLabel, ImVec2(joinW, buttonHeight)))
						JoinSlot(static_cast<u8_t>(i));

					ImGui::SameLine(0.f, 6.f);
					addW = rowWidth - joinW - 6.f;
					if (addW < 32.f)
						addW = 32.f;
				}

				if (ImGui::Button("+ Add Bot", ImVec2(addW, buttonHeight)))
					AddBotToFirstEmptySlot(beginSlot, endSlot);
				bEmptyControlDrawn = true;
			}
			else
			{
				ImGui::Dummy(ImVec2(rowWidth, buttonHeight));
			}

			ImGui::PopID();
		}
	}

	ImGui::End();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}
