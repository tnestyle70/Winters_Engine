#include "GamePlay/ChampionRegistry.h"

CChampionRegistry& CChampionRegistry::Instance()
{
	static CChampionRegistry s_inst;
	return s_inst;
}

void CChampionRegistry::Add(eChampion id, const ChampionDef& def)
{
	m_Map.try_emplace(id, def);
}

const ChampionDef* CChampionRegistry::Find(eChampion id) const
{
	auto it = m_Map.find(id);
	return (it != m_Map.end()) ? &it->second : nullptr;
}

void CChampionRegistry::ForEach(const IterFn& fn) const
{
	for (const auto& [ID, def] : m_Map)
		fn(ID, def);
}
