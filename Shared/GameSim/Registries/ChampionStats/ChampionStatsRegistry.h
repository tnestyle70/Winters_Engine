#pragma once

#include "Shared/GameSim/Definitions/ChampionStatsDef.h"

#include <cstddef>
#include <unordered_map>

class CChampionStatsRegistry
{
public:
    static CChampionStatsRegistry& Instance();

    void Add(eChampion ID, const ChampionStatsDef& def);
    const ChampionStatsDef* Find(eChampion ID) const;
    ChampionStatsDef Resolve(eChampion ID) const;
    std::size_t Count() const { return m_Map.size(); }

private:
    CChampionStatsRegistry() = default;
    ~CChampionStatsRegistry() = default;
    CChampionStatsRegistry(const CChampionStatsRegistry&) = delete;
    CChampionStatsRegistry& operator=(const CChampionStatsRegistry&) = delete;

    std::unordered_map<eChampion, ChampionStatsDef> m_Map{};
};
