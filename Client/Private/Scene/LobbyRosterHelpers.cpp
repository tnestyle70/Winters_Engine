#include "Scene/LobbyRosterHelpers.h"

#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionModuleBootstrap.h"
#include "GamePlay/ChampionRegistry.h"
#include "Shared/GameSim/Definitions/MapSpawnPoints.h"

#include <Windows.h>

#include <cstdio>
#include <cwchar>
#include <cwctype>
#include <vector>

namespace
{
	constexpr u32_t kLocalSessionId = 1;
	constexpr u32_t kLocalNetId = 1;
	constexpr const wchar_t* kExplicitLocalOnlySmokeFlag = L"--local-only-smoke";

	bool_t HasExactCommandLineFlag(const wchar_t* pFlag)
	{
		if (!pFlag || pFlag[0] == L'\0')
			return false;

		const wchar_t* pCommandLine = ::GetCommandLineW();
		if (!pCommandLine)
			return false;

		const size_t flagLength = std::wcslen(pFlag);
		const wchar_t* pSearch = pCommandLine;
		while (const wchar_t* pHit = std::wcsstr(pSearch, pFlag))
		{
			const bool_t bAtTokenStart =
				pHit == pCommandLine ||
				std::iswspace(*(pHit - 1)) != 0 ||
				*(pHit - 1) == L'"';
			const wchar_t next = pHit[flagLength];
			const bool_t bAtTokenEnd =
				next == L'\0' ||
				std::iswspace(next) != 0 ||
				next == L'"';

			if (bAtTokenStart && bAtTokenEnd)
				return true;

			pSearch = pHit + flagLength;
		}

		return false;
	}

	const ChampionDef* FindRosterChampionDef(eChampion champion)
	{
		const ChampionCatalogEntry* pEntry = CChampionCatalog::Instance().Find(champion);
		if (pEntry && pEntry->pDef)
			return pEntry->pDef;

		const ChampionDef* pDef = CChampionRegistry::Instance().Find(champion);
		if (!pDef)
			pDef = FindChampionDef(champion);
		return pDef;
	}

	void WriteReason(char* pReason, size_t reasonBytes, const char* pText)
	{
		if (pReason && reasonBytes > 0)
			sprintf_s(pReason, reasonBytes, "%s", pText ? pText : "");
	}
}

bool_t IsExplicitLocalOnlySmokeRequested()
{
	return HasExactCommandLineFlag(kExplicitLocalOnlySmokeFlag);
}

eMatchLaunchRuntimeMode ResolveMatchLaunchRuntimeMode(bool_t bServerConnected)
{
	if (bServerConnected)
		return eMatchLaunchRuntimeMode::ServerConnected;
	if (IsExplicitLocalOnlySmokeRequested())
		return eMatchLaunchRuntimeMode::ExplicitLocalSmoke;
	return eMatchLaunchRuntimeMode::ServerRequiredUnavailable;
}

bool_t CanLaunchLocalGameplay(eMatchLaunchRuntimeMode eMode)
{
	return eMode == eMatchLaunchRuntimeMode::ExplicitLocalSmoke;
}

void EnsureLobbyChampionCatalogReady()
{
	static bool_t s_bBootstrapped = false;

	if (!s_bBootstrapped)
	{
		BootstrapChampionModules();
		RegisterAllLegacy();
		s_bBootstrapped = true;
	}

	CChampionCatalog::Instance().RebuildFromRegistry();
}

u8_t GetTeamFromSlotId(u32_t slotId)
{
	return static_cast<u8_t>(slotId < 5 ? 0 : 1);
}

const char* GetTeamLabel(u32_t slotId)
{
	return slotId < 5 ? "Blue" : "Red";
}

bool_t IsSlotOccupied(const GameRosterSlot& slot)
{
	return slot.bHuman || slot.bBot;
}

bool_t IsSlotEmpty(const GameRosterSlot& slot)
{
	return !IsSlotOccupied(slot);
}

bool_t IsRosterChampionSupported(eChampion champion)
{
	return CChampionCatalog::Instance().IsSelectable(champion);
}

const char* GetRosterChampionLabel(eChampion champion)
{
	const ChampionCatalogEntry* pEntry = CChampionCatalog::Instance().Find(champion);
	if (pEntry && pEntry->displayName)
		return pEntry->displayName;

	const ChampionDef* pDef = FindRosterChampionDef(champion);
	if (pDef && pDef->displayName)
		return pDef->displayName;
	return GetChampionDisplayName(champion);
}

const wchar_t* GetRosterChampionLoadscreenPath(eChampion champion)
{
	const ClientData::ChampionUiVisualDefinition* pVisual =
		ClientData::FindChampionUiVisualDefinition(champion);
	if (pVisual && pVisual->loadscreen.resourceRelativePath)
		return pVisual->loadscreen.resourceRelativePath;

	const ChampionDef* pDef = FindRosterChampionDef(champion);
	return pDef ? pDef->defaultTexturePath : nullptr;
}

const wchar_t* GetRosterChampionPortraitPath(eChampion champion)
{
	const ClientData::ChampionUiVisualDefinition* pVisual =
		ClientData::FindChampionUiVisualDefinition(champion);
	if (pVisual && pVisual->portrait.resourceRelativePath)
		return pVisual->portrait.resourceRelativePath;

	return nullptr;
}

void ClearSlot(MatchContext& context, u32_t slotId)
{
	if (slotId >= kGameRosterSlotCount)
		return;

	const bool_t bWasLocal =
		context.Roster[slotId].bHuman &&
		context.Roster[slotId].sessionId == context.MySessionId;

	context.Roster[slotId] = GameRosterSlot{};

	if (bWasLocal)
	{
		context.MySlotId = kInvalidGameRosterSlot;
		context.MyTeam = 0;
		context.SelectedChampion = eChampion::END;
	}
}

i32_t FindLocalHumanSlot(const MatchContext& context)
{
	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		const GameRosterSlot& slot = context.Roster[i];
		if (slot.bHuman && slot.sessionId == context.MySessionId)
			return static_cast<i32_t>(i);
	}
	return -1;
}

void JoinLocalPlayerSlot(MatchContext& context, u32_t slotId)
{
	if (slotId >= kGameRosterSlotCount)
		return;

	eChampion previousChampion = context.SelectedChampion;
	const i32_t previousSlot = FindLocalHumanSlot(context);
	if (previousSlot >= 0)
	{
		const GameRosterSlot& oldSlot = context.Roster[previousSlot];
		if (IsRosterChampionSupported(oldSlot.champion))
			previousChampion = oldSlot.champion;
		context.Roster[previousSlot] = GameRosterSlot{};
	}

	GameRosterSlot& slot = context.Roster[slotId];
	slot = GameRosterSlot{};
	slot.slotId = static_cast<u8_t>(slotId);
	slot.team = GetTeamFromSlotId(slotId);
	slot.bHuman = true;
	slot.bBot = false;
	slot.sessionId = kLocalSessionId;
	slot.netId = kLocalNetId;
	slot.champion = IsRosterChampionSupported(previousChampion)
		? previousChampion
		: eChampion::END;

	context.bUseNetworkRoster = true;
	context.MySessionId = kLocalSessionId;
	context.MyNetId = kLocalNetId;
	context.MySlotId = static_cast<u8_t>(slotId);
	context.MyTeam = slot.team;
	context.SelectedChampion = slot.champion;
}

eChampion GetDefaultBotChampion(u32_t slotId)
{
	const std::vector<ChampionCatalogEntry>& botChampions =
		CChampionCatalog::Instance().GetBotChampions();
	if (!botChampions.empty())
		return botChampions[slotId % static_cast<u32_t>(botChampions.size())].id;

	return eChampion::EZREAL;
}

u8_t GetDefaultBotLane(u32_t slotId)
{
	switch (slotId % 5u)
	{
	case 1:
		return kGameSimLaneTop;
	case 2:
		return kGameSimLaneMid;
	case 3:
	case 4:
		return kGameSimLaneBot;
	default:
		return kGameSimLaneMid;
	}
}

const char* GetBotLaneLabel(u8_t lane)
{
	switch (lane)
	{
	case kGameSimLaneTop:
		return "Top";
	case kGameSimLaneBot:
		return "Bot";
	case kGameSimLaneMid:
	default:
		return "Mid";
	}
}

void AddBotToSlot(MatchContext& context, u32_t slotId, eChampion champion)
{
	if (slotId >= kGameRosterSlotCount)
		return;
	if (!IsRosterChampionSupported(champion))
		champion = GetDefaultBotChampion(slotId);

	GameRosterSlot& slot = context.Roster[slotId];
	slot = GameRosterSlot{};
	slot.slotId = static_cast<u8_t>(slotId);
	slot.team = GetTeamFromSlotId(slotId);
	slot.bHuman = false;
	slot.bBot = true;
	slot.sessionId = 0;
	slot.netId = 0;
	slot.champion = champion;
	slot.botDifficulty = 2;
	slot.botLane = GetDefaultBotLane(slotId);
}

void SetBotSlotLane(MatchContext& context, u32_t slotId, u8_t lane)
{
	if (slotId >= kGameRosterSlotCount)
		return;

	GameRosterSlot& slot = context.Roster[slotId];
	if (slot.bBot)
		slot.botLane = lane;
}

void AssignChampionToSlot(MatchContext& context, u32_t slotId, eChampion champion)
{
	if (slotId >= kGameRosterSlotCount || !IsRosterChampionSupported(champion))
		return;

	GameRosterSlot& slot = context.Roster[slotId];
	if (IsSlotEmpty(slot))
		AddBotToSlot(context, slotId, champion);
	else
		slot.champion = champion;

	if (slot.bHuman && slot.sessionId == context.MySessionId)
		context.SelectedChampion = champion;
}

void FillEmptySlotsWithBots(MatchContext& context)
{
	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		if (IsSlotEmpty(context.Roster[i]))
			AddBotToSlot(context, i, GetDefaultBotChampion(i));
	}
}

void ClearBotSlots(MatchContext& context)
{
	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		if (context.Roster[i].bBot)
			context.Roster[i] = GameRosterSlot{};
	}
}

void InitializeLocalCustomRoom(MatchContext& context)
{
	context.bUseNetworkRoster = true;
	context.MySessionId = kLocalSessionId;
	context.MyNetId = kLocalNetId;
	context.MySlotId = 0;
	context.MyTeam = 0;
	context.SelectedChampion = eChampion::END;

	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
		context.Roster[i] = GameRosterSlot{};

	JoinLocalPlayerSlot(context, 0);
}

bool_t ValidateRosterForStart(const MatchContext& context, char* pReason, size_t reasonBytes)
{
	if (pReason && reasonBytes > 0)
		pReason[0] = '\0';

	const i32_t localSlotId = FindLocalHumanSlot(context);
	if (localSlotId < 0)
	{
		WriteReason(pReason, reasonBytes, "local player has no slot");
		return false;
	}

	const GameRosterSlot& localSlot = context.Roster[localSlotId];
	if (!IsRosterChampionSupported(localSlot.champion))
	{
		WriteReason(pReason, reasonBytes, "local player champion is empty");
		return false;
	}

	u32_t occupiedCount = 0;
	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		const GameRosterSlot& slot = context.Roster[i];
		if (!IsSlotOccupied(slot))
			continue;

		++occupiedCount;
		if (!IsRosterChampionSupported(slot.champion))
		{
			if (pReason && reasonBytes > 0)
				sprintf_s(pReason, reasonBytes, "slot %u champion is empty", i + 1u);
			return false;
		}
	}

	if (occupiedCount == 0)
	{
		WriteReason(pReason, reasonBytes, "roster is empty");
		return false;
	}

	return true;
}

void FinalizeRosterForStart(MatchContext& context)
{
	context.bUseNetworkRoster = true;
	context.MySessionId = kLocalSessionId;
	context.MyNetId = kLocalNetId;

	const i32_t localSlotId = FindLocalHumanSlot(context);
	if (localSlotId >= 0)
	{
		const GameRosterSlot& localSlot = context.Roster[localSlotId];
		context.MySlotId = static_cast<u8_t>(localSlotId);
		context.MyTeam = localSlot.team;
		context.SelectedChampion = localSlot.champion;
	}

	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		GameRosterSlot& slot = context.Roster[i];
		if (!IsSlotOccupied(slot))
			continue;

		slot.slotId = static_cast<u8_t>(i);
		slot.team = GetTeamFromSlotId(i);
		if (slot.bHuman)
		{
			slot.bBot = false;
			slot.sessionId = kLocalSessionId;
			slot.netId = kLocalNetId;
		}
	}
}

void CountRoster(const MatchContext& context, u32_t& outHumans, u32_t& outBots, u32_t& outOccupied)
{
	outHumans = 0;
	outBots = 0;
	outOccupied = 0;

	for (u32_t i = 0; i < kGameRosterSlotCount; ++i)
	{
		const GameRosterSlot& slot = context.Roster[i];
		if (!IsSlotOccupied(slot))
			continue;

		++outOccupied;
		if (slot.bHuman)
			++outHumans;
		if (slot.bBot)
			++outBots;
	}
}
