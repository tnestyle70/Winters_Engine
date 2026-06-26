#pragma once

#include <cstdint>

enum class eChampion : std::uint8_t
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

inline constexpr std::uint32_t kGameRosterSlotCount = 10;
inline constexpr std::uint8_t kInvalidGameRosterSlot = 255;
inline constexpr std::uint8_t kGameRosterDefaultBotLane = 1;

struct GameRosterSlot
{
    std::uint8_t slotId = kInvalidGameRosterSlot;
    std::uint8_t team = 0;
    bool bHuman = false;
    bool bBot = false;
    std::uint32_t sessionId = 0;
    std::uint32_t netId = 0;
    eChampion champion = eChampion::END;
    std::uint8_t botDifficulty = 0;
    std::uint8_t botLane = kGameRosterDefaultBotLane;
};

struct MatchContext
{
    eChampion SelectedChampion = eChampion::END;
    bool bUseNetworkRoster = false;
    std::uint32_t MySessionId = 0;
    std::uint32_t MyNetId = 0;
    std::uint8_t MySlotId = kInvalidGameRosterSlot;
    std::uint8_t MyTeam = 0;
    GameRosterSlot Roster[kGameRosterSlotCount] = {};
};
