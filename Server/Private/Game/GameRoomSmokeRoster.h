#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

struct LobbySlotState;

// Debug smoke roster: red Sylas dummy bot on a fixed slot.
// Shared by GameRoom lobby start, navgrid seeding, and champion spawn code.
bool_t ShouldUseRedSylasSmokeRoster();
bool_t ShouldUseAttackSpeedLabRoster();
Vec3 GetRedSylasSmokeDummyPosition();
bool_t IsRedSylasSmokeDummySlot(const LobbySlotState& slot);
u8_t GetRedSylasSmokePatrolPointCount();
Vec3 GetRedSylasSmokePatrolPoint(u8_t index);
f32_t ResolveServerChampionMaxHpForSlot(const LobbySlotState& slot, f32_t defaultMaxHp);
void EnsureRedSylasSmokeRoster(LobbySlotState* pSlots, u32_t slotCount);
bool_t EnsureAttackSpeedLabRoster(LobbySlotState* pSlots, u32_t slotCount);
