#include "GamePlay/ChampionRegistry.h"

CChampionRegistry& CChampionRegistry::Instance()
{
	static CChampionRegistry s_inst;
	return s_inst;
}

void CChampionRegistry::Add(eChampion id, const ChampionDef& def)
{
	auto [it, inserted] = m_Map.try_emplace(id, def);
	if (!inserted)
	{
		char buf[160];
		sprintf_s(buf, "[ChampionRegistry] DUPLICATE ID=%u\n", static_cast<u32_t>(id));
		OutputDebugStringA(buf);
	}
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
