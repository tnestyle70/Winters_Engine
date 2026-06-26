#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "GameObject/ChampionDef.h"
#include "WintersTypes.h"

#include <cstddef>

void EnsureLobbyChampionCatalogReady();
u8_t GetTeamFromSlotId(u32_t slotId);
const char* GetTeamLabel(u32_t slotId);
bool_t IsSlotOccupied(const GameRosterSlot& slot);
bool_t IsSlotEmpty(const GameRosterSlot& slot);
bool_t IsRosterChampionSupported(eChampion champion);
const char* GetRosterChampionLabel(eChampion champion);
const wchar_t* GetRosterChampionLoadscreenPath(eChampion champion);
const wchar_t* GetRosterChampionPortraitPath(eChampion champion);
void ClearSlot(MatchContext& context, u32_t slotId);
i32_t FindLocalHumanSlot(const MatchContext& context);
void JoinLocalPlayerSlot(MatchContext& context, u32_t slotId);
eChampion GetDefaultBotChampion(u32_t slotId);
u8_t GetDefaultBotLane(u32_t slotId);
const char* GetBotLaneLabel(u8_t lane);
void AddBotToSlot(MatchContext& context, u32_t slotId, eChampion champion);
void SetBotSlotLane(MatchContext& context, u32_t slotId, u8_t lane);
void AssignChampionToSlot(MatchContext& context, u32_t slotId, eChampion champion);
void FillEmptySlotsWithBots(MatchContext& context);
void ClearBotSlots(MatchContext& context);
void InitializeLocalCustomRoom(MatchContext& context);
bool_t ValidateRosterForStart(const MatchContext& context, char* pReason, size_t reasonBytes);
void FinalizeRosterForStart(MatchContext& context);
void CountRoster(const MatchContext& context, u32_t& outHumans, u32_t& outBots, u32_t& outOccupied);
