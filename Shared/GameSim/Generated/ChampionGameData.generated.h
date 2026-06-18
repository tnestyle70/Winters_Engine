#pragma once

#include <cstddef>

#include "Shared/GameSim/Definitions/ChampionGameData.h"

namespace ChampionGameDataGenerated
{
    u32_t GetBuildHash();
    std::size_t GetChampionCount();
    const ChampionGameData* GetChampionTable();
    const ChampionGameData* FindChampion(eChampion champion);
}
