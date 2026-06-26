#pragma once

#include "Defines.h"
#include "GameObject/ChampionDef.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"

#include <vector>

struct ChampionCatalogEntry
{
	eChampion id = eChampion::END;
	const ChampionDef* pDef = nullptr;
	bool_t bSelectable = false;
	bool_t bPlayable = false;
	bool_t bBotAllowed = false;
	const char* displayName = nullptr;
};

class CChampionCatalog final
{
public:
	static CChampionCatalog& Instance();

	void RebuildFromRegistry();

	const ChampionCatalogEntry* Find(eChampion id) const;
	const std::vector<ChampionCatalogEntry>& GetSelectableChampions() const { return m_Selectable; }
	const std::vector<ChampionCatalogEntry>& GetPlayableChampions() const { return m_Playable; }
	const std::vector<ChampionCatalogEntry>& GetBotChampions() const { return m_Bot; }

	bool_t IsSelectable(eChampion id) const;
	bool_t IsPlayable(eChampion id) const;

private:
	CChampionCatalog() = default;
	~CChampionCatalog() = default;
	CChampionCatalog(const CChampionCatalog&) = delete;
	CChampionCatalog& operator=(const CChampionCatalog&) = delete;

	static bool_t IsValidChampionDef(const ChampionDef& def);
	static i32_t SortKey(eChampion id);

	std::vector<ChampionCatalogEntry> m_Entries{};
	std::vector<ChampionCatalogEntry> m_Selectable{};
	std::vector<ChampionCatalogEntry> m_Playable{};
	std::vector<ChampionCatalogEntry> m_Bot{};
};
