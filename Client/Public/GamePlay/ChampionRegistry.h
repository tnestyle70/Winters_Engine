#pragma once

#include "Defines.h"
#include "GameObject/ChampionDef.h"
#include "GameContext.h"

#include <cstddef>
#include <functional>
#include <unordered_map>

class CChampionRegistry
{
public:
	using IterFn = std::function<void(eChampion, const ChampionDef&)>;

	static CChampionRegistry& Instance();

	void Add(eChampion id, const ChampionDef& def);
	const ChampionDef* Find(eChampion id) const;
	void ForEach(const IterFn& fn) const;

	std::size_t Count() const { return m_Map.size(); }

private:
	CChampionRegistry() = default;
	~CChampionRegistry() = default;
	CChampionRegistry(const CChampionRegistry&) = delete;
	CChampionRegistry& operator=(const CChampionRegistry&) = delete;

	std::unordered_map<eChampion, ChampionDef> m_Map{};
};
