#pragma once

#include "GameContext.h"
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
void ClearSlot(GameContext& context, u32_t slotId);
i32_t FindLocalHumanSlot(const GameContext& context);
void JoinLocalPlayerSlot(GameContext& context, u32_t slotId);
eChampion GetDefaultBotChampion(u32_t slotId);
u8_t GetDefaultBotLane(u32_t slotId);
const char* GetBotLaneLabel(u8_t lane);
void AddBotToSlot(GameContext& context, u32_t slotId, eChampion champion);
void SetBotSlotLane(GameContext& context, u32_t slotId, u8_t lane);
void AssignChampionToSlot(GameContext& context, u32_t slotId, eChampion champion);
void FillEmptySlotsWithBots(GameContext& context);
void ClearBotSlots(GameContext& context);
void InitializeLocalCustomRoom(GameContext& context);
bool_t ValidateRosterForStart(const GameContext& context, char* pReason, size_t reasonBytes);
void FinalizeRosterForStart(GameContext& context);
void CountRoster(const GameContext& context, u32_t& outHumans, u32_t& outBots, u32_t& outOccupied);
