#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"

#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"

CChampionStatsRegistry& CChampionStatsRegistry::Instance()
{
    static CChampionStatsRegistry s_inst;
    return s_inst;
}

void CChampionStatsRegistry::Add(eChampion ID, const ChampionStatsDef& def)
{
    ChampionStatsDef copy = def;
    copy.championId = ID;
    m_Map[ID] = copy;
}

const ChampionStatsDef* CChampionStatsRegistry::Find(eChampion ID) const
{
    auto it = m_Map.find(ID);
    return (it != m_Map.end()) ? &it->second : nullptr;
}

ChampionStatsDef CChampionStatsRegistry::Resolve(eChampion ID) const
{
    if (const ChampionStatsDef* pDef = Find(ID))
        return *pDef;

    return BuildDefaultChampionStatsDef(ID);
}
