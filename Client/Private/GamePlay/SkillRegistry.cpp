#include "GamePlay/SkillRegistry.h"

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
}

CSkillRegistry& CSkillRegistry::Instance()
{
	static CSkillRegistry s_inst;
	return s_inst;
}

void CSkillRegistry::Add(eChampion champ, u8_t slot, const SkillDef& def)
{
	const u32_t key = MakeSkillKey(champ, slot);
	m_Map.try_emplace(key, ApplyVerificationTiming(def));
}

const SkillDef* CSkillRegistry::Find(eChampion champ, u8_t slot) const
{
	const u32_t key = MakeSkillKey(champ, slot);
	auto it = m_Map.find(key);
	return (it != m_Map.end()) ? &it->second : nullptr;
}
