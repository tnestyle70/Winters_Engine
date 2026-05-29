#pragma once
#include "Engine_Defines.h"
#include "WintersTypes.h"

enum class eChampion : uint8_t
{
	NONE = 0,
	IRELIA = 1,
	YASUO = 2,
	KALISTA = 3,
	SYLAS = 4,
	VIEGO = 5,
	ANNIE = 6,
	ASHE = 7,
	FIORA = 8,
	GAREN = 9,
	RIVEN = 10,
	ZED = 11,
	EZREAL = 12,
	YONE = 13,
	JAX = 14,
	MASTERYI = 15,
	KINDRED = 16,
	LEESIN = 17,
	END = 255
};

inline constexpr u32_t kGameRosterSlotCount = 10;
inline constexpr u8_t kInvalidGameRosterSlot = 255;
inline constexpr u8_t kGameRosterDefaultBotLane = 1;

struct GameRosterSlot
{
	u8_t slotId = kInvalidGameRosterSlot;
	u8_t team = 0;
	bool_t bHuman = false;
	bool_t bBot = false;
	u32_t sessionId = 0;
	u32_t netId = 0;
	eChampion champion = eChampion::END;
	u8_t botDifficulty = 0;
	u8_t botLane = kGameRosterDefaultBotLane;
};

struct GameContext
{
	eChampion SelectedChampion = eChampion::END;
	bool_t bUseNetworkRoster = false;
	u32_t MySessionId = 0;
	u32_t MyNetId = 0;
	u8_t MySlotId = kInvalidGameRosterSlot;
	u8_t MyTeam = 0;
	GameRosterSlot Roster[kGameRosterSlotCount] = {};
};
