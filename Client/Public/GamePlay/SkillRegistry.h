#pragma once

#include "Defines.h"
#include "GameObject/ChampionVisualData.h"
#include "GameObject/SkillDef.h"
#include "GameObject/SkillVisualData.h"
#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "Shared/GameSim/Definitions/ChampionGameData.h"
#include "Shared/GameSim/Definitions/SkillAtomData.h"

#include <cstddef>
#include <unordered_map>

class CSkillRegistry
{
public:
	static CSkillRegistry& Instance();

	void Add(eChampion champ, u8_t slot, const SkillDef& def);
	const SkillDef* Find(eChampion champ, u8_t slot) const;
	bool_t ResolveGameAtoms(eChampion champ, u8_t slot, SkillGameAtomBundle& outData) const;
	bool_t ResolveGameData(eChampion champ, u8_t slot, ChampionGameDataSkill& outData) const;
	bool_t ResolveSkillVisualData(eChampion champ, u8_t slot, SkillVisualData& outData) const;
	bool_t ResolveVisualData(eChampion champ, u8_t slot, ChampionActionVisualData& outData) const;
	bool_t ApplyVisualTimingOverride(
		eChampion champion,
		u8_t slot,
		u8_t stage,
		f32_t playbackSpeed,
		f32_t castFrame,
		f32_t recoveryFrame);
	bool_t AuditDataDrivenContracts() const;

	std::size_t Count() const { return m_LegacyMap.size(); }

private:
	CSkillRegistry() = default;
	~CSkillRegistry() = default;
	CSkillRegistry(const CSkillRegistry&) = delete;
	CSkillRegistry& operator=(const CSkillRegistry&) = delete;

	std::unordered_map<u32_t, SkillDef> m_LegacyMap{};
	std::unordered_map<u32_t, SkillVisualData> m_VisualAtoms{};
};
