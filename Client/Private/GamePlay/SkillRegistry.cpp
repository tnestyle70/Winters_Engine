#include "GamePlay/SkillRegistry.h"

#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "GameObject/SkillDefVisualDataAdapter.h"

#include <cmath>

namespace
{
	// Keep false for normal network-authoritative gameplay. Enabling this
	// intentionally overrides authored cooldown/action-lock values for short
	// verification sessions and makes registered champion skills diverge from
	// the shared server runtime table.
	constexpr bool_t kUseClientFastSkillVerificationTiming = false;
	constexpr f32_t kVerificationSkillCooldownSec = 0.20f;
	constexpr f32_t kVerificationActionLockSec = 0.20f;

	constexpr u32_t MakeSkillKey(eChampion champ, u8_t slot)
	{
		return (static_cast<u32_t>(champ) << 8) | static_cast<u32_t>(slot);
	}

	SkillDef ApplyVerificationTiming(SkillDef def)
	{
		if constexpr (kUseClientFastSkillVerificationTiming)
		{
			def.cooldownSec = kVerificationSkillCooldownSec;
			def.lockDurationSec = kVerificationActionLockSec;
			if (def.stage2LockSec > 0.f)
				def.stage2LockSec = kVerificationActionLockSec;
		}

		return def;
	}

	SkillDef ApplyAuthoredVisualTiming(eChampion champion, u8_t slot, SkillDef def)
	{
		const ClientData::ChampionVisualDefinition* pVisual =
			ClientData::FindChampionVisualDefinition(champion);
		if (!pVisual || slot >= ClientData::kVisualSkillSlotCount)
			return def;

		const ClientData::SkillVisualDefinition& skill = pVisual->skills[slot];
		const ClientData::SkillVisualStageDef& stage1 = skill.stages[0];
		def.visualPlaySpeed = stage1.animationPlaybackSpeed;
		def.visualCastFrame = stage1.castFrame;
		def.visualRecoveryFrame = stage1.recoveryFrame;
		if (skill.stageCount >= 2u)
		{
			const ClientData::SkillVisualStageDef& stage2 = skill.stages[1];
			def.stage2VisualPlaySpeed = stage2.animationPlaybackSpeed;
			def.stage2VisualCastFrame = stage2.castFrame;
			def.stage2VisualRecoveryFrame = stage2.recoveryFrame;
		}
		return def;
	}
}

CSkillRegistry& CSkillRegistry::Instance()
{
	static CSkillRegistry s_inst;
	return s_inst;
}

void CSkillRegistry::Add(eChampion champ, u8_t slot, const SkillDef& def)
{
	const u32_t key = MakeSkillKey(champ, slot);
	SkillDef legacy = ApplyVerificationTiming(
		ApplyAuthoredVisualTiming(champ, slot, def));
	const auto [it, inserted] = m_LegacyMap.try_emplace(key, legacy);
	if (!inserted)
	{
		return;
	}

	m_GameAtoms.try_emplace(key, SkillDefAdapters::BuildSkillGameAtomBundle(it->second));
	m_VisualAtoms.try_emplace(key, SkillDefAdapters::BuildSkillVisualData(it->second));
}

const SkillDef* CSkillRegistry::Find(eChampion champ, u8_t slot) const
{
	const u32_t key = MakeSkillKey(champ, slot);
	auto it = m_LegacyMap.find(key);
	return (it != m_LegacyMap.end()) ? &it->second : nullptr;
}

bool_t CSkillRegistry::ResolveGameAtoms(
	eChampion champ,
	u8_t slot,
	SkillGameAtomBundle& outData) const
{
	const u32_t key = MakeSkillKey(champ, slot);
	auto it = m_GameAtoms.find(key);
	if (it == m_GameAtoms.end())
	{
		return false;
	}

	outData = it->second;
	return true;
}

bool_t CSkillRegistry::ResolveGameData(
	eChampion champ,
	u8_t slot,
	ChampionGameDataSkill& outData) const
{
	const SkillDef* pDef = Find(champ, slot);
	if (!pDef)
	{
		return false;
	}

	outData = SkillDefAdapters::BuildChampionGameDataSkill(*pDef);
	return true;
}

bool_t CSkillRegistry::ResolveSkillVisualData(
	eChampion champ,
	u8_t slot,
	SkillVisualData& outData) const
{
	const u32_t key = MakeSkillKey(champ, slot);
	auto it = m_VisualAtoms.find(key);
	if (it == m_VisualAtoms.end())
	{
		return false;
	}

	outData = it->second;
	return true;
}

bool_t CSkillRegistry::ResolveVisualData(
	eChampion champ,
	u8_t slot,
	ChampionActionVisualData& outData) const
{
	const SkillDef* pDef = Find(champ, slot);
	if (!pDef)
	{
		return false;
	}

	outData = SkillDefAdapters::BuildChampionActionVisualData(*pDef);
	return true;
}

bool_t CSkillRegistry::ApplyVisualTimingOverride(
	eChampion champion,
	u8_t slot,
	u8_t stage,
	f32_t playbackSpeed,
	f32_t castFrame,
	f32_t recoveryFrame)
{
	if (stage >= kSkillVisualStageMax ||
		!std::isfinite(playbackSpeed) || playbackSpeed <= 0.01f ||
		!std::isfinite(castFrame) || castFrame < 0.f ||
		!std::isfinite(recoveryFrame) || recoveryFrame < castFrame)
	{
		return false;
	}

	const u32_t key = MakeSkillKey(champion, slot);
	auto legacyIt = m_LegacyMap.find(key);
	if (legacyIt == m_LegacyMap.end())
		return false;

	SkillDef& def = legacyIt->second;
	if (stage == 0u)
	{
		def.visualPlaySpeed = playbackSpeed;
		def.visualCastFrame = castFrame;
		def.visualRecoveryFrame = recoveryFrame;
	}
	else
	{
		if (def.stageCount < 2u)
			return false;
		def.stage2VisualPlaySpeed = playbackSpeed;
		def.stage2VisualCastFrame = castFrame;
		def.stage2VisualRecoveryFrame = recoveryFrame;
	}

	m_VisualAtoms[key] = SkillDefAdapters::BuildSkillVisualData(def);
	return true;
}
