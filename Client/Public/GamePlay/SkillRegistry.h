#pragma once

#include "Defines.h"
#include "GameObject/SkillDef.h"
#include "GameContext.h"

#include <cstddef>
#include <unordered_map>

class CSkillRegistry
{
public:
	static CSkillRegistry& Instance();

	void Add(eChampion champ, u8_t slot, const SkillDef& def);
	const SkillDef* Find(eChampion champ, u8_t slot) const;

	std::size_t Count() const { return m_Map.size(); }

private:
	CSkillRegistry() = default;
	~CSkillRegistry() = default;
	CSkillRegistry(const CSkillRegistry&) = delete;
	CSkillRegistry& operator=(const CSkillRegistry&) = delete;

	std::unordered_map<u32_t, SkillDef> m_Map{};
};
