#include "Scene/Scene_BanPick.h"

#include "Core/CInput.h"
#include "Dev/SmokeLog.h"
#include "GameInstance.h"
#include "GamePlay/ChampionCatalog.h"
#include "Network/Client/GameSessionClient.h"
#include "Scene/LobbyRosterHelpers.h"
#include "Scene/Scene_CustomMode.h"
#include "Scene/Scene_InGame.h"
#include "Scene/Scene_MatchLoading.h"

#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "Shared/Schemas/Generated/cpp/LobbyTypes_generated.h"
#pragma pop_macro("max")
#pragma pop_macro("min")

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <string>
#include <utility>

#include <Windows.h>

namespace
{
	constexpr eChampion kDefaultSmokeChampion = eChampion::EZREAL;
	constexpr ImageSourceRect kReadyButtonRect{ 696.f, 700.f, 904.f, 754.f };
	constexpr u32_t kChampionGridColumns = 6;
	constexpr f32_t kChampionGridLeft = 454.f;
	constexpr f32_t kChampionGridTop = 143.f;
	constexpr f32_t kChampionCellW = 82.f;
	constexpr f32_t kChampionCellH = 82.f;
	constexpr f32_t kChampionPitchX = 122.f;
	constexpr f32_t kChampionPitchY = 115.f;
	constexpr f32_t kSlotPanelW = 302.f;
	constexpr f32_t kSlotPanelH = 89.f;
	constexpr f32_t kSlotPanelLeftBlue = 32.f;
	constexpr f32_t kSlotPanelLeftRed = 1221.f;
	constexpr f32_t kSlotPanelTop = 305.f;
	constexpr f32_t kSlotPanelPitchY = 103.f;
	constexpr f32_t kSlotPortraitInsetX = 16.f;
	constexpr f32_t kSlotPortraitInsetY = 8.f;
	constexpr f32_t kSlotPortraitSize = 72.f;

	struct BanPickSmokeOptions
	{
		bool_t bEnabled = false;
		bool_t bStart = false;
		u8_t slotId = 0;
		u8_t startMinHumans = 1;
		eChampion champion = kDefaultSmokeChampion;
	};

	const wchar_t* FindCommandValue(const wchar_t* pCommandLine, const wchar_t* pKey)
	{
		if (!pCommandLine || !pKey)
			return nullptr;

		const wchar_t* pHit = wcsstr(pCommandLine, pKey);
		return pHit ? pHit + wcslen(pKey) : nullptr;
	}

	bool_t HasCommandFlag(const wchar_t* pCommandLine, const wchar_t* pFlag)
	{
		if (!pCommandLine || !pFlag)
			return false;

		const u32_t flagLen = static_cast<u32_t>(wcslen(pFlag));
		const wchar_t* pSearch = pCommandLine;
		while (const wchar_t* pHit = wcsstr(pSearch, pFlag))
		{
			const wchar_t next = pHit[flagLen];
			if (next == L'\0' || iswspace(next) || next == L'"')
				return true;
			pSearch = pHit + flagLen;
		}

		return false;
	}

	bool_t ParseCommandU32(const wchar_t* pCommandLine, const wchar_t* pKey, u32_t& outValue)
	{
		const wchar_t* pValue = FindCommandValue(pCommandLine, pKey);
		if (!pValue)
			return false;

		u32_t value = 0;
		bool_t bAny = false;
		while (*pValue >= L'0' && *pValue <= L'9')
		{
			bAny = true;
			value = value * 10u + static_cast<u32_t>(*pValue - L'0');
			++pValue;
		}

		if (!bAny)
			return false;

		outValue = value;
		return true;
	}

	bool_t ParseCommandToken(const wchar_t* pCommandLine, const wchar_t* pKey, wchar_t* pOut, u32_t capacity)
	{
		if (!pOut || capacity == 0)
			return false;

		pOut[0] = L'\0';
		const wchar_t* pValue = FindCommandValue(pCommandLine, pKey);
		if (!pValue)
			return false;

		u32_t i = 0;
		while (*pValue && !iswspace(*pValue) && *pValue != L'"' && i + 1u < capacity)
			pOut[i++] = *pValue++;

		pOut[i] = L'\0';
		return i > 0;
	}

	eChampion ParseSmokeChampionToken(const wchar_t* pToken)
	{
		if (!pToken || pToken[0] == L'\0')
			return kDefaultSmokeChampion;

		if (_wcsicmp(pToken, L"IRELIA") == 0) return eChampion::IRELIA;
		if (_wcsicmp(pToken, L"YASUO") == 0) return eChampion::YASUO;
		if (_wcsicmp(pToken, L"KALISTA") == 0) return eChampion::KALISTA;
		if (_wcsicmp(pToken, L"SYLAS") == 0) return eChampion::SYLAS;
		if (_wcsicmp(pToken, L"VIEGO") == 0) return eChampion::VIEGO;
		if (_wcsicmp(pToken, L"ANNIE") == 0) return eChampion::ANNIE;
		if (_wcsicmp(pToken, L"ASHE") == 0) return eChampion::ASHE;
		if (_wcsicmp(pToken, L"FIORA") == 0) return eChampion::FIORA;
		if (_wcsicmp(pToken, L"GAREN") == 0) return eChampion::GAREN;
		if (_wcsicmp(pToken, L"RIVEN") == 0) return eChampion::RIVEN;
		if (_wcsicmp(pToken, L"ZED") == 0) return eChampion::ZED;
		if (_wcsicmp(pToken, L"EZREAL") == 0) return eChampion::EZREAL;
		if (_wcsicmp(pToken, L"YONE") == 0) return eChampion::YONE;
		if (_wcsicmp(pToken, L"JAX") == 0) return eChampion::JAX;
		if (_wcsicmp(pToken, L"MASTERYI") == 0) return eChampion::MASTERYI;
		if (_wcsicmp(pToken, L"KINDRED") == 0) return eChampion::KINDRED;
		if (_wcsicmp(pToken, L"LEESIN") == 0) return eChampion::LEESIN;

		return kDefaultSmokeChampion;
	}

	BanPickSmokeOptions ParseBanPickSmokeOptions()
	{
		BanPickSmokeOptions options{};
		const wchar_t* pCommandLine = GetCommandLineW();
		if (!pCommandLine || !wcsstr(pCommandLine, L"--banpick-smoke"))
			return options;

		options.bEnabled = true;
		options.bStart = HasCommandFlag(pCommandLine, L"--smoke-start");

		u32_t slot = 0;
		bool_t bExplicitSlot = false;
		if (ParseCommandU32(pCommandLine, L"--smoke-slot=", slot) && slot < kGameRosterSlotCount)
		{
			options.slotId = static_cast<u8_t>(slot);
			bExplicitSlot = true;
		}

		if (!bExplicitSlot)
		{
			wchar_t teamToken[16] = {};
			const bool_t bHasTeamToken =
				ParseCommandToken(pCommandLine, L"--smoke-team=", teamToken, 16);
			if (HasCommandFlag(pCommandLine, L"--smoke-red") ||
				(bHasTeamToken && _wcsicmp(teamToken, L"red") == 0))
			{
				options.slotId = 5;
			}
			else if (HasCommandFlag(pCommandLine, L"--smoke-blue") ||
				(bHasTeamToken && _wcsicmp(teamToken, L"blue") == 0))
			{
				options.slotId = 0;
			}
		}

		u32_t minHumans = 0;
		if (ParseCommandU32(pCommandLine, L"--smoke-start-min-humans=", minHumans))
		{
			if (minHumans < 1u)
				minHumans = 1u;
			if (minHumans > kGameRosterSlotCount)
				minHumans = kGameRosterSlotCount;
			options.startMinHumans = static_cast<u8_t>(minHumans);
		}

		wchar_t championToken[32] = {};
		if (ParseCommandToken(pCommandLine, L"--smoke-champion=", championToken, 32))
			options.champion = ParseSmokeChampionToken(championToken);

		return options;
	}

	bool_t GetChampionSelectSlotRect(
		const GameContext& context,
		u32_t slotId,
		ImageSourceRect& outRect)
	{
		if (slotId >= kGameRosterSlotCount || !IsSlotOccupied(context.Roster[slotId]))
			return false;

		const bool_t bRed = slotId >= 5;
		u32_t row = 0;
		const u32_t begin = bRed ? 5u : 0u;
		for (u32_t i = begin; i < slotId; ++i)
		{
			if (IsSlotOccupied(context.Roster[i]))
				++row;
		}

		const f32_t left = bRed ? kSlotPanelLeftRed : kSlotPanelLeftBlue;
		const f32_t top = kSlotPanelTop + static_cast<f32_t>(row) * kSlotPanelPitchY;
		outRect = ImageSourceRect{ left, top, left + kSlotPanelW, top + kSlotPanelH };
		return true;
	}

	ImageSourceRect GetChampionSelectSlotPortraitRect(const ImageSourceRect& panelRect)
	{
		return ImageSourceRect{
			panelRect.fLeft + kSlotPortraitInsetX,
			panelRect.fTop + kSlotPortraitInsetY,
			panelRect.fLeft + kSlotPortraitInsetX + kSlotPortraitSize,
			panelRect.fTop + kSlotPortraitInsetY + kSlotPortraitSize
		};
	}

	u8_t ResolveVisibleSelectedSlotId(const GameContext& context, u8_t requestedSlotId)
	{
		if (requestedSlotId < kGameRosterSlotCount && IsSlotOccupied(context.Roster[requestedSlotId]))
			return requestedSlotId;
		if (context.MySlotId < kGameRosterSlotCount && IsSlotOccupied(context.Roster[context.MySlotId]))
			return context.MySlotId;
		return kInvalidGameRosterSlot;
	}

	bool_t IsLocalHumanSlot(const GameRosterSlot& slot, const GameContext& context)
	{
		return slot.bHuman &&
			context.MySessionId != 0 &&
			slot.sessionId == context.MySessionId;
	}

	bool_t IsLocalHumanSlot(const GameRosterSlot& slot, const CGameSessionClient& session)
	{
		return slot.bHuman &&
			session.GetMySessionId() != 0 &&
			slot.sessionId == session.GetMySessionId();
	}

	bool_t HasLocalServerMatchSlot(const GameContext& context)
	{
		return context.bUseNetworkRoster &&
			context.MySessionId != 0 &&
			context.MyNetId != 0 &&
			context.MySlotId < kGameRosterSlotCount;
	}
}

std::unique_ptr<CScene_BanPick> CScene_BanPick::Create()
{
	return std::unique_ptr<CScene_BanPick>(new CScene_BanPick());
}

bool CScene_BanPick::OnEnter()
{
	EnsureLobbyChampionCatalogReady();

	m_bSceneTransitionStarted = false;
	m_SelectedChampion = eChampion::END;
	m_ChampionCells.clear();
	m_ServerSmoke = {};
	m_ImageUI.Initialize(
		L"Client/Bin/Resource/Texture/UI/IreliaSelect1.png",
		1555,
		861);
	BuildChampionCells();

	const BanPickSmokeOptions smokeOptions = ParseBanPickSmokeOptions();
	m_ServerSmoke.bEnabled = smokeOptions.bEnabled;
	m_ServerSmoke.bStartGameWhenReady = smokeOptions.bStart;
	m_ServerSmoke.slotId = smokeOptions.slotId;
	m_ServerSmoke.minHumansToStart = smokeOptions.startMinHumans;
	m_ServerSmoke.champion = smokeOptions.champion;

	if (m_ServerSmoke.bEnabled)
	{
		m_SelectedSlotId = m_ServerSmoke.slotId;
		char msg[192]{};
		sprintf_s(msg,
			"[BanPickSmoke] enabled slot=%u champion=%u start=%u minHumans=%u\n",
			static_cast<u32_t>(m_ServerSmoke.slotId),
			static_cast<u32_t>(m_ServerSmoke.champion),
			m_ServerSmoke.bStartGameWhenReady ? 1u : 0u,
			static_cast<u32_t>(m_ServerSmoke.minHumansToStart));
		Winters::DevSmoke::Log("%s", msg);
	}

	m_bServerLobbyActive = CGameSessionClient::Instance().Connect();
	if (m_bServerLobbyActive)
	{
		if (!m_ServerSmoke.bEnabled)
		{
			const GameContext& context = CGameSessionClient::Instance().GetLobbyContext();
			m_SelectedSlotId = context.MySlotId < kGameRosterSlotCount ? context.MySlotId : 0;
		}
		Winters::DevSmoke::Log("[BanPick] server champion select mode\n");
		return true;
	}

	GameContext& context = CGameInstance::Get()->Get_GameContext();
	if (context.MySessionId == 0 || context.MySlotId == kInvalidGameRosterSlot)
		InitializeLocalCustomRoom(context);

	if (m_ServerSmoke.bEnabled)
	{
		JoinLocalPlayerSlot(context, m_ServerSmoke.slotId);
		AssignChampionToSlot(context, m_ServerSmoke.slotId, m_ServerSmoke.champion);
		FinalizeRosterForStart(context);
		m_SelectedSlotId = m_ServerSmoke.slotId;
	}
	else
	{
		m_SelectedSlotId = context.MySlotId < kGameRosterSlotCount ? context.MySlotId : 0;
	}

	Winters::DevSmoke::Log("[BanPick] local champion select mode\n");
	return true;
}

void CScene_BanPick::OnExit()
{
	m_ChampionCells.clear();
	m_ImageUI.Shutdown();
}

void CScene_BanPick::OnUpdate(f32_t dt)
{
	if (m_bSceneTransitionStarted)
		return;

	if (!m_bServerLobbyActive)
	{
		HandleChampionSelectInput();
		if (m_ServerSmoke.bEnabled &&
			m_ServerSmoke.bStartGameWhenReady &&
			!m_ServerSmoke.bStartCommandSent)
		{
			m_ServerSmoke.bStartCommandSent = true;
			Winters::DevSmoke::Log("[BanPickSmoke] local StartGame\n");
			StartMatchLoadingScene();
		}
		return;
	}

	CGameSessionClient& session = CGameSessionClient::Instance();
	session.Pump();

	if (session.HasLobbyState())
	{
		session.CopyLobbyToGameContext(CGameInstance::Get()->Get_GameContext());
		const GameContext& context = session.GetLobbyContext();
		if (ResolveVisibleSelectedSlotId(context, m_SelectedSlotId) == kInvalidGameRosterSlot &&
			context.MySlotId < kGameRosterSlotCount)
		{
			m_SelectedSlotId = context.MySlotId;
		}
	}

	UpdateServerSmokeAutomation(dt);

	if (session.GetLobbyPhase() == static_cast<u8_t>(Shared::Schema::LobbyPhase::SeatSelect))
	{
		m_bSceneTransitionStarted = true;
		CGameInstance::Get()->Change_Scene(
			static_cast<u32_t>(eSceneID::CustomMode),
			CScene_CustomMode::Create());
		return;
	}

	GameContext& gameContext = CGameInstance::Get()->Get_GameContext();
	if (session.IsServerLoading() && HasLocalServerMatchSlot(gameContext))
	{
		session.ClearServerLoading();
		StartMatchLoadingScene();
		return;
	}

	if (session.IsGameStarting() && HasLocalServerMatchSlot(gameContext))
	{
		session.ClearGameStarting();
		StartMatchLoadingScene();
		return;
	}

	HandleChampionSelectInput();
}

void CScene_BanPick::OnLateUpdate(f32_t /*dt*/)
{
}

void CScene_BanPick::OnRender()
{
	if (!m_ImageUI.Begin())
		return;

	m_ImageUI.DrawBackground();
	RenderChampionGridAndRosterOverlay();
	m_ImageUI.End();
}

void CScene_BanPick::OnImGui()
{
}

void CScene_BanPick::HandleChampionSelectInput()
{
	if (m_bServerLobbyActive)
		HandleServerChampionSelectInput();
	else
		HandleLocalChampionSelectInput();
}

void CScene_BanPick::HandleServerChampionSelectInput()
{
	CGameSessionClient& session = CGameSessionClient::Instance();
	if (!session.HasLobbyState())
		return;

	const GameContext& context = session.GetLobbyContext();
	f32_t fSourceX = 0.f;
	f32_t fSourceY = 0.f;
	const bool_t bClicked = CInput::Get().IsLButtonPressed() &&
		m_ImageUI.ScreenToSource(
			static_cast<f32_t>(CInput::Get().GetMouseX()),
			static_cast<f32_t>(CInput::Get().GetMouseY()),
			fSourceX,
			fSourceY);

	if (bClicked)
	{
		const u8_t slotId = ResolveClickedChampionSlot(fSourceX, fSourceY, context);
		if (slotId < kGameRosterSlotCount)
		{
			m_SelectedSlotId = slotId;
			return;
		}

		const eChampion champion = ResolveClickedChampion(fSourceX, fSourceY);
		if (champion != eChampion::END)
		{
			const u8_t selectedSlotId = ResolveVisibleSelectedSlotId(context, m_SelectedSlotId);
			if (selectedSlotId >= kGameRosterSlotCount)
				return;

			const GameRosterSlot& selected = context.Roster[selectedSlotId];
			m_SelectedChampion = champion;
			if (IsLocalHumanSlot(selected, session))
			{
				session.SendLobbyCommand(
					Shared::Schema::LobbyCommandKind::PickChampion,
					selectedSlotId,
					champion);
			}
			else if (selected.bBot)
			{
				session.SendLobbyCommand(
					Shared::Schema::LobbyCommandKind::SetBotChampion,
					selectedSlotId,
					champion,
					selected.botDifficulty ? selected.botDifficulty : 2);
			}
			return;
		}
	}

	if (IsReadyButtonClicked())
	{
		session.SendLobbyCommand(
			Shared::Schema::LobbyCommandKind::StartGame,
			0);
	}
}

void CScene_BanPick::HandleLocalChampionSelectInput()
{
	GameContext& context = CGameInstance::Get()->Get_GameContext();
	if (context.MySessionId == 0 || context.MySlotId == kInvalidGameRosterSlot)
	{
		InitializeLocalCustomRoom(context);
		m_SelectedSlotId = context.MySlotId;
	}

	f32_t fSourceX = 0.f;
	f32_t fSourceY = 0.f;
	const bool_t bClicked = CInput::Get().IsLButtonPressed() &&
		m_ImageUI.ScreenToSource(
			static_cast<f32_t>(CInput::Get().GetMouseX()),
			static_cast<f32_t>(CInput::Get().GetMouseY()),
			fSourceX,
			fSourceY);

	if (bClicked)
	{
		const u8_t slotId = ResolveClickedChampionSlot(fSourceX, fSourceY, context);
		if (slotId < kGameRosterSlotCount)
		{
			m_SelectedSlotId = slotId;
			return;
		}

		const eChampion champion = ResolveClickedChampion(fSourceX, fSourceY);
		if (champion != eChampion::END)
		{
			const u8_t selectedSlotId = ResolveVisibleSelectedSlotId(context, m_SelectedSlotId);
			if (selectedSlotId >= kGameRosterSlotCount)
				return;

			GameRosterSlot& selected = context.Roster[selectedSlotId];
			m_SelectedChampion = champion;
			if (IsLocalHumanSlot(selected, context) || selected.bBot)
				AssignChampionToSlot(context, selectedSlotId, champion);
			return;
		}
	}

	if (IsReadyButtonClicked())
	{
		char reason[160]{};
		if (!ValidateRosterForStart(context, reason, sizeof(reason)))
			return;

		FinalizeRosterForStart(context);
		StartMatchLoadingScene();
	}
}

void CScene_BanPick::RenderChampionGridAndRosterOverlay()
{
	const GameContext& context = m_bServerLobbyActive
		? CGameSessionClient::Instance().GetLobbyContext()
		: CGameInstance::Get()->Get_GameContext();

	const u8_t selectedSlotId = ResolveVisibleSelectedSlotId(context, m_SelectedSlotId);
	const eChampion selectedSlotChampion =
		selectedSlotId < kGameRosterSlotCount ? context.Roster[selectedSlotId].champion : eChampion::END;

	RenderChampionSelectSlots(context, selectedSlotId);

	for (const ChampionCell& cell : m_ChampionCells)
	{
		m_ImageUI.DrawSourceRect(cell.rect, Vec4(0.005f, 0.025f, 0.035f, 0.96f));
		if (cell.pTexture)
		{
			m_ImageUI.DrawSourceImage(
				cell.pTexture.get(),
				cell.rect,
				Vec4(1.f, 1.f, 1.f, 1.f));
		}
		m_ImageUI.DrawSourceRectOutline(cell.rect, Vec4(0.68f, 0.52f, 0.18f, 0.78f), 1.f);

		if (cell.champion == selectedSlotChampion || cell.champion == m_SelectedChampion)
			m_ImageUI.DrawSourceRect(cell.rect, Vec4(0.1f, 0.85f, 1.f, 0.32f));
	}
}

void CScene_BanPick::RenderChampionSelectSlots(const GameContext& context, u8_t selectedSlotId)
{
	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		const GameRosterSlot& slot = context.Roster[i];
		ImageSourceRect panelRect{};
		if (!GetChampionSelectSlotRect(context, i, panelRect))
			continue;

		Vec4 fillColor = i < 5 ? Vec4(0.01f, 0.05f, 0.09f, 0.78f) : Vec4(0.09f, 0.02f, 0.04f, 0.78f);
		Vec4 outlineColor = i < 5 ? Vec4(0.68f, 0.52f, 0.18f, 0.95f) : Vec4(0.72f, 0.43f, 0.20f, 0.95f);
		if (slot.bBot)
			fillColor = Vec4(0.08f, 0.06f, 0.02f, 0.78f);
		if (i == selectedSlotId)
			outlineColor = Vec4(0.10f, 0.85f, 1.f, 1.f);

		m_ImageUI.DrawSourceRect(panelRect, fillColor);
		m_ImageUI.DrawSourceRectOutline(panelRect, outlineColor, i == selectedSlotId ? 3.f : 1.5f);

		if (Engine::CTexture* pTexture = FindChampionTexture(slot.champion))
		{
			const ImageSourceRect portraitRect = GetChampionSelectSlotPortraitRect(panelRect);
			m_ImageUI.DrawSourceImage(
				pTexture,
				portraitRect,
				Vec4(1.f, 1.f, 1.f, 1.f));
			m_ImageUI.DrawSourceRectOutline(
				portraitRect,
				i == selectedSlotId ? Vec4(0.10f, 0.85f, 1.f, 0.95f) : Vec4(0.68f, 0.52f, 0.18f, 0.72f),
				1.f);
		}
	}
}

void CScene_BanPick::BuildChampionCells()
{
	m_ChampionCells.clear();
	const std::vector<ChampionCatalogEntry>& champions =
		CChampionCatalog::Instance().GetSelectableChampions();

	IRHIDevice* pDevice = CGameInstance::Get()->Get_RHIDevice();
	if (!pDevice)
		return;

	for (u32_t i = 0; i < static_cast<u32_t>(champions.size()); ++i)
	{
		const eChampion champion = champions[i].id;
		if (!IsRosterChampionSupported(champion))
			continue;

		const u32_t cellIndex = static_cast<u32_t>(m_ChampionCells.size());
		const u32_t col = cellIndex % kChampionGridColumns;
		const u32_t row = cellIndex / kChampionGridColumns;

		ChampionCell cell{};
		cell.champion = champion;
		cell.rect = ImageSourceRect{
			kChampionGridLeft + static_cast<f32_t>(col) * kChampionPitchX,
			kChampionGridTop + static_cast<f32_t>(row) * kChampionPitchY,
			kChampionGridLeft + static_cast<f32_t>(col) * kChampionPitchX + kChampionCellW,
			kChampionGridTop + static_cast<f32_t>(row) * kChampionPitchY + kChampionCellH
		};

		if (const wchar_t* pPath = GetRosterChampionPortraitPath(champion))
		{
			cell.pTexture = Engine::CTexture::Create(
				pDevice,
				std::wstring(pPath),
				Engine::eTexSamplerMode::Clamp,
				Engine::eTexColorSpace::IgnoreSRGB);
		}

		m_ChampionCells.emplace_back(std::move(cell));
	}
}

void CScene_BanPick::StartMatchLoadingScene()
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

void CScene_BanPick::UpdateServerSmokeAutomation(f32_t dt)
{
	if (!m_ServerSmoke.bEnabled || !m_bServerLobbyActive || m_bSceneTransitionStarted)
		return;

	CGameSessionClient& session = CGameSessionClient::Instance();
	if (!session.HasLobbyState() || session.GetMySessionId() == 0)
		return;

	m_ServerSmoke.commandRetryTimerSec += dt;
	if (m_ServerSmoke.commandRetryTimerSec < 0.25f)
		return;
	m_ServerSmoke.commandRetryTimerSec = 0.f;

	const GameContext& context = session.GetLobbyContext();
	const u32_t slotId = m_ServerSmoke.slotId < kGameRosterSlotCount ? m_ServerSmoke.slotId : 0u;
	const GameRosterSlot& slot = context.Roster[slotId];
	const bool_t bSlotIsMine = IsLocalHumanSlot(slot, session);
	const u8_t phase = session.GetLobbyPhase();

	if (phase == static_cast<u8_t>(Shared::Schema::LobbyPhase::SeatSelect))
	{
		if (!bSlotIsMine)
		{
			if (session.SendLobbyCommand(
				Shared::Schema::LobbyCommandKind::JoinSlot,
				static_cast<u8_t>(slotId)))
			{
				Winters::DevSmoke::Log(
					"[BanPickSmoke] JoinSlot sent slot=%u sid=%u\n",
					slotId,
					session.GetMySessionId());
			}
			return;
		}

		if (m_ServerSmoke.bStartGameWhenReady)
		{
			session.SendLobbyCommand(
				Shared::Schema::LobbyCommandKind::StartGame,
				0);
		}
		return;
	}

	if (phase != static_cast<u8_t>(Shared::Schema::LobbyPhase::ChampionSelect))
		return;

	if (slot.champion != m_ServerSmoke.champion)
	{
		if (session.SendLobbyCommand(
			Shared::Schema::LobbyCommandKind::PickChampion,
			static_cast<u8_t>(slotId),
			m_ServerSmoke.champion))
		{
			Winters::DevSmoke::Log(
				"[BanPickSmoke] PickChampion sent slot=%u champion=%u sid=%u\n",
				slotId,
				static_cast<u32_t>(m_ServerSmoke.champion),
				session.GetMySessionId());
		}
		return;
	}

	if (!m_ServerSmoke.bStartGameWhenReady || m_ServerSmoke.bStartCommandSent)
		return;

	u32_t humanCount = 0;
	u32_t botCount = 0;
	u32_t occupiedCount = 0;
	CountRoster(context, humanCount, botCount, occupiedCount);
	(void)botCount;
	(void)occupiedCount;

	if (humanCount < m_ServerSmoke.minHumansToStart)
		return;

	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		const GameRosterSlot& rosterSlot = context.Roster[i];
		if (rosterSlot.bHuman && !IsRosterChampionSupported(rosterSlot.champion))
			return;
	}

	if (session.SendLobbyCommand(
		Shared::Schema::LobbyCommandKind::StartGame,
		0))
	{
		m_ServerSmoke.bStartCommandSent = true;
		Winters::DevSmoke::Log(
			"[BanPickSmoke] StartGame sent humans=%u minHumans=%u\n",
			humanCount,
			static_cast<u32_t>(m_ServerSmoke.minHumansToStart));
	}
}

eChampion CScene_BanPick::ResolveClickedChampion(f32_t fSourceX, f32_t fSourceY) const
{
	for (const ChampionCell& cell : m_ChampionCells)
	{
		if (fSourceX >= cell.rect.fLeft &&
			fSourceX <= cell.rect.fRight &&
			fSourceY >= cell.rect.fTop &&
			fSourceY <= cell.rect.fBottom)
		{
			return cell.champion;
		}
	}
	return eChampion::END;
}

u8_t CScene_BanPick::ResolveClickedChampionSlot(
	f32_t fSourceX,
	f32_t fSourceY,
	const GameContext& context) const
{
	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		ImageSourceRect rect{};
		if (!GetChampionSelectSlotRect(context, i, rect))
			continue;

		if (fSourceX >= rect.fLeft &&
			fSourceX <= rect.fRight &&
			fSourceY >= rect.fTop &&
			fSourceY <= rect.fBottom)
		{
			return static_cast<u8_t>(i);
		}
	}
	return kInvalidGameRosterSlot;
}

Engine::CTexture* CScene_BanPick::FindChampionTexture(eChampion champion) const
{
	for (const ChampionCell& cell : m_ChampionCells)
	{
		if (cell.champion == champion)
			return cell.pTexture.get();
	}
	return nullptr;
}

bool_t CScene_BanPick::IsReadyButtonClicked() const
{
	return m_ImageUI.WasSourceRectClicked(kReadyButtonRect);
}

bool_t CScene_BanPick::IsLocalPlayerChampionPicked() const
{
	const GameContext& context = m_bServerLobbyActive
		? CGameSessionClient::Instance().GetLobbyContext()
		: CGameInstance::Get()->Get_GameContext();

	const i32_t slotId = FindLocalHumanSlot(context);
	if (slotId < 0)
		return false;

	return IsRosterChampionSupported(context.Roster[slotId].champion);
}
