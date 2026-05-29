#include "GamePlay/ChampionCatalog.h"
#include "GamePlay/ChampionRegistry.h"

#include <algorithm>

namespace
{
	constexpr eChampion kChampionCatalogOrder[] =
	{
		eChampion::EZREAL,
		eChampion::FIORA,
		eChampion::JAX,
		eChampion::KINDRED,
		eChampion::LEESIN,
		eChampion::MASTERYI,
		eChampion::ANNIE,
		eChampion::ASHE,
		eChampion::YONE,
		eChampion::IRELIA,
		eChampion::YASUO,
		eChampion::KALISTA,
		eChampion::SYLAS,
		eChampion::VIEGO,
		eChampion::GAREN,
		eChampion::ZED,
		eChampion::RIVEN,
	};
}

CChampionCatalog& CChampionCatalog::Instance()
{
	static CChampionCatalog s_inst;
	return s_inst;
}

void CChampionCatalog::RebuildFromRegistry()
{
	m_Entries.clear();
	m_Selectable.clear();
	m_Playable.clear();
	m_Bot.clear();

	CChampionRegistry::Instance().ForEach(
		[this](eChampion id, const ChampionDef& def)
		{
			if (!IsValidChampionDef(def))
				return;

			ChampionCatalogEntry entry{};
			entry.id = id;
			entry.pDef = &def;
			entry.bSelectable = true;
			entry.bPlayable = true;
			entry.bBotAllowed = true;
			entry.displayName = def.displayName ? def.displayName : GetChampionDisplayName(id);
			m_Entries.push_back(entry);
		});

	std::sort(m_Entries.begin(), m_Entries.end(),
		[](const ChampionCatalogEntry& lhs, const ChampionCatalogEntry& rhs)
		{
			return SortKey(lhs.id) < SortKey(rhs.id);
		});

	for (const ChampionCatalogEntry& entry : m_Entries)
	{
		if (entry.bSelectable)
			m_Selectable.push_back(entry);
		if (entry.bPlayable)
			m_Playable.push_back(entry);
		if (entry.bBotAllowed)
			m_Bot.push_back(entry);
	}
}

const ChampionCatalogEntry* CChampionCatalog::Find(eChampion id) const
{
	for (const ChampionCatalogEntry& entry : m_Entries)
	{
		if (entry.id == id)
			return &entry;
	}
	return nullptr;
}

bool_t CChampionCatalog::IsSelectable(eChampion id) const
{
	const ChampionCatalogEntry* pEntry = Find(id);
	return pEntry && pEntry->bSelectable;
}

bool_t CChampionCatalog::IsPlayable(eChampion id) const
{
	const ChampionCatalogEntry* pEntry = Find(id);
	return pEntry && pEntry->bPlayable;
}

bool_t CChampionCatalog::IsValidChampionDef(const ChampionDef& def)
{
	if (def.id == eChampion::END || def.id == eChampion::NONE)
		return false;
	if (!def.fbxPath || def.fbxPath[0] == '\0')
		return false;
	if (!def.shaderPath)
		return false;
	return true;
}

i32_t CChampionCatalog::SortKey(eChampion id)
{
	constexpr u32_t count = static_cast<u32_t>(sizeof(kChampionCatalogOrder) / sizeof(kChampionCatalogOrder[0]));
	for (u32_t i = 0; i < count; ++i)
	{
		if (kChampionCatalogOrder[i] == id)
			return static_cast<i32_t>(i);
	}
	return 1000 + static_cast<i32_t>(id);
}
