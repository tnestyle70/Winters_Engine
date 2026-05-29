#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

inline constexpr u8_t kGameSimLaneTop = 0;
inline constexpr u8_t kGameSimLaneMid = 1;
inline constexpr u8_t kGameSimLaneBot = 2;

Vec3 GetGameSimRosterSpawnPosition(u8_t slotId, u8_t team);
Vec3 GetGameSimRosterSpawnPosition(u8_t slotId, u8_t team, bool_t bBot);
Vec3 GetGameSimMidLaneBotSpawnPosition(u8_t slotId, u8_t team);
u8_t GetGameSimRosterLane(u8_t slotId);
Vec3 GetGameSimLaneGatherPosition(u8_t lane, u8_t team);
