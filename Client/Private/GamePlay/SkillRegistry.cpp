#include "GamePlay/SkillRegistry.h"

#include "Client/Private/Data/LoLVisualDefinitionPack.h"
#include "GameObject/SkillDefVisualDataAdapter.h"
#include "Shared/GameSim/Generated/ChampionGameData.generated.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

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

	SkillDef ApplyAuthoredGameplayData(eChampion champion, u8_t slot, SkillDef def)
	{
		const ChampionGameData* pChampion =
			ChampionGameDataGenerated::FindChampion(champion);
		if (!pChampion || slot >= kChampionGameDataSkillSlotCount)
			return def;

		const ChampionGameDataSkill& skill = pChampion->skills[slot];
		if (!skill.bValid)
			return def;

		def.inputActivation = skill.inputActivation;
		def.targetMode = skill.stages[0].targetMode;
		def.cooldownSec = skill.cooldownSecByRank[0];
		def.rangeMax = skill.rangeMax;
		def.manaCost = skill.manaCostByRank[0];
		def.stageCount = skill.stageCount;
		def.stageWindowSec = skill.stageWindowSec;
		def.lockDurationSec = skill.stages[0].lockDurationSec;
		def.commandLockSec = skill.stages[0].commandLockSec;
		def.movePolicy = skill.stages[0].movePolicy;
		def.bCreatesActionState = skill.stages[0].bCreatesActionState;
		def.bPresentationLoopWhileActive =
			skill.stages[0].bPresentationLoopWhileActive;
		if (skill.stageCount >= 2u)
		{
			def.stage2TargetMode = skill.stages[1].targetMode;
			def.stage2LockSec = skill.stages[1].lockDurationSec;
			def.stage2CommandLockSec = skill.stages[1].commandLockSec;
			def.stage2MovePolicy = skill.stages[1].movePolicy;
			def.bStage2CreatesActionState = skill.stages[1].bCreatesActionState;
			def.bStage2PresentationLoopWhileActive =
				skill.stages[1].bPresentationLoopWhileActive;
		}
		def.charge = skill.charge;
		def.skillId = skill.skillId;
		def.scalingTableId = skill.scalingTableId;
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

	SkillGameAtomBundle BuildAuthoredGameplayAtoms(
		eChampion champion,
		const ChampionGameDataSkill& skill)
	{
		SkillGameAtomBundle data{};
		if (!skill.bValid || skill.slot >= kChampionGameDataSkillSlotCount)
			return data;

		data.bValid = true;
		data.slot.bValid = true;
		data.slot.champion = champion;
		data.slot.slot = skill.slot;
		data.slot.skillId = skill.skillId;
		data.input.activation = skill.inputActivation;
		data.target.bValid = true;
		data.target.shape[0] =
			SkillDefAdapters::ToTargetShape(skill.stages[0].targetMode);
		data.target.shape[1] =
			SkillDefAdapters::ToTargetShape(skill.stages[1].targetMode);
		data.target.resolvePolicy =
			skill.stageCount >= 2u && data.target.shape[0] != data.target.shape[1]
				? eTargetResolvePolicy::StageDependent
				: eTargetResolvePolicy::Direct;

		data.cost.rankCount = skill.rankCount;
		data.cooldown.rankCount = skill.rankCount;
		for (u8_t rank = 0u; rank < skill.rankCount && rank < kSkillRankValueMax; ++rank)
		{
			data.cost.manaCostByRank[rank] = skill.manaCostByRank[rank];
			data.cooldown.cooldownSecByRank[rank] = skill.cooldownSecByRank[rank];
		}
		data.cost.manaCost = skill.manaCostByRank[0];
		data.cooldown.cooldownSec = skill.cooldownSecByRank[0];
		data.range.rangeMax = skill.rangeMax;
		data.stage.stageCount = skill.stageCount;
		data.stage.stageWindowSec = skill.stageWindowSec;
		for (u8_t stage = 0u; stage < skill.stageCount && stage < kSkillAtomStageMax; ++stage)
		{
			data.stage.lockDurationSec[stage] = skill.stages[stage].lockDurationSec;
			data.stage.commandLockSec[stage] = skill.stages[stage].commandLockSec;
			data.stage.movePolicy[stage] = skill.stages[stage].movePolicy;
			data.stage.bCreatesActionState[stage] =
				skill.stages[stage].bCreatesActionState;
			data.stage.bPresentationLoopWhileActive[stage] =
				skill.stages[stage].bPresentationLoopWhileActive;
			data.facing.mode[stage] = skill.stages[stage].facingMode;
		}
		data.charge = skill.charge;
		data.effect.scalingTableId = skill.scalingTableId;

		return data;
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
		ApplyAuthoredVisualTiming(
			champ,
			slot,
			ApplyAuthoredGameplayData(champ, slot, def)));
	const auto [it, inserted] = m_LegacyMap.try_emplace(key, legacy);
	if (!inserted)
	{
		return;
	}

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
	const ChampionGameData* pChampion =
		ChampionGameDataGenerated::FindChampion(champ);
	if (!pChampion || slot >= kChampionGameDataSkillSlotCount ||
		!pChampion->skills[slot].bValid)
	{
		return false;
	}

	outData = BuildAuthoredGameplayAtoms(
		champ,
		pChampion->skills[slot]);
	return true;
}

bool_t CSkillRegistry::ResolveGameData(
	eChampion champ,
	u8_t slot,
	ChampionGameDataSkill& outData) const
{
	const ChampionGameData* pChampion =
		ChampionGameDataGenerated::FindChampion(champ);
	if (!pChampion || slot >= kChampionGameDataSkillSlotCount ||
		!pChampion->skills[slot].bValid)
	{
		return false;
	}

	outData = pChampion->skills[slot];
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
		if (!def.stage2AnimKey)
			return false;
		def.stage2VisualPlaySpeed = playbackSpeed;
		def.stage2VisualCastFrame = castFrame;
		def.stage2VisualRecoveryFrame = recoveryFrame;
	}

	m_VisualAtoms[key] = SkillDefAdapters::BuildSkillVisualData(def);
	return true;
}

bool_t CSkillRegistry::AuditDataDrivenContracts() const
{
#if !defined(_DEBUG)
	return true;
#else
	static bool_t s_bAudited = false;
	if (s_bAudited)
		return true;
	s_bAudited = true;

	const auto Near = [](f32_t left, f32_t right)
	{
		return std::fabs(left - right) <= 0.0001f;
	};
	const auto TargetShape = [](eTargetMode mode)
	{
		switch (mode)
		{
		case eTargetMode::UnitTarget:   return eTargetShape::Unit;
		case eTargetMode::GroundTarget: return eTargetShape::Ground;
		case eTargetMode::Direction:
			return eTargetShape::Direction;
		case eTargetMode::Self:
		default:                        return eTargetShape::Self;
		}
	};

	const ChampionGameData* pTable = ChampionGameDataGenerated::GetChampionTable();
	const std::size_t championCount = ChampionGameDataGenerated::GetChampionCount();
	u32_t checkedSkills = 0u;
	u32_t missingSkills = 0u;
	u32_t numericMismatches = 0u;
	u32_t targetMismatches = 0u;
	u32_t conditionalTargets = 0u;
	u32_t stageTargetSchemaLosses = 0u;
	u32_t rankTableCollapses = 0u;
	u32_t twoStageSkills = 0u;
	u32_t twoStageVisuals = 0u;
	u32_t presentationOnlyStage2Visuals = 0u;
	u32_t presentationTimingMismatches = 0u;

	for (std::size_t championIndex = 0u; championIndex < championCount; ++championIndex)
	{
		const ChampionGameData& authoredChampion = pTable[championIndex];
		for (u8_t slot = 0u; slot < kChampionGameDataSkillSlotCount; ++slot)
		{
			const ChampionGameDataSkill& authored = authoredChampion.skills[slot];
			SkillGameAtomBundle runtime{};
			const SkillDef* pRegistered = Find(authoredChampion.champion, slot);
			if (!authored.bValid || !pRegistered ||
				!ResolveGameAtoms(authoredChampion.champion, slot, runtime))
			{
				char message[176]{};
				sprintf_s(
					message,
					"[DataContract][Client] missing champ=%u slot=%u authored=%u registered=%u\n",
					static_cast<u32_t>(authoredChampion.champion),
					static_cast<u32_t>(slot),
					authored.bValid ? 1u : 0u,
					pRegistered ? 1u : 0u);
				OutputDebugStringA(message);
				++missingSkills;
				continue;
			}

			++checkedSkills;
			SkillVisualData visual{};
			if (!ResolveSkillVisualData(authoredChampion.champion, slot, visual))
			{
				++missingSkills;
				continue;
			}
			if (visual.stageCount >= 2u)
			{
				++twoStageVisuals;
				if (authored.stageCount < 2u)
				{
					const bool_t bValidPresentationTiming =
						std::isfinite(pRegistered->stage2VisualPlaySpeed) &&
						pRegistered->stage2VisualPlaySpeed > 0.f &&
						std::isfinite(pRegistered->stage2VisualCastFrame) &&
						pRegistered->stage2VisualCastFrame >= 0.f &&
						std::isfinite(pRegistered->stage2VisualRecoveryFrame) &&
						pRegistered->stage2VisualRecoveryFrame >=
							pRegistered->stage2VisualCastFrame &&
						pRegistered->stage2VisualRecoveryFrame > 0.f;
					if (!bValidPresentationTiming)
						++presentationTimingMismatches;
					++presentationOnlyStage2Visuals;
				}
			}
			const u8_t expectedVisualStageCount =
				pRegistered->stage2AnimKey ? 2u : authored.stageCount;
			if (visual.stageCount != expectedVisualStageCount)
			{
				char message[192]{};
				sprintf_s(
					message,
					"[DataContract][Client] visual-stage-mismatch champ=%u slot=%u visual=%u expected=%u gameplay=%u\n",
					static_cast<u32_t>(authoredChampion.champion),
					static_cast<u32_t>(slot),
					static_cast<u32_t>(visual.stageCount),
					static_cast<u32_t>(expectedVisualStageCount),
					static_cast<u32_t>(authored.stageCount));
				OutputDebugStringA(message);
				++numericMismatches;
			}
			const bool_t bNumericMatch =
				Near(runtime.cooldown.cooldownSecByRank[0], authored.cooldownSecByRank[0]) &&
				Near(runtime.cost.manaCostByRank[0], authored.manaCostByRank[0]) &&
				Near(runtime.range.rangeMax, authored.rangeMax) &&
				runtime.stage.stageCount == authored.stageCount &&
				Near(runtime.stage.stageWindowSec, authored.stageWindowSec) &&
				runtime.input.activation == authored.inputActivation &&
				Near(runtime.stage.lockDurationSec[0], authored.stages[0].lockDurationSec) &&
				Near(runtime.stage.commandLockSec[0], authored.stages[0].commandLockSec) &&
				runtime.stage.movePolicy[0] == authored.stages[0].movePolicy &&
				(authored.stageCount < 2u ||
					(Near(runtime.stage.lockDurationSec[1], authored.stages[1].lockDurationSec) &&
					 Near(runtime.stage.commandLockSec[1], authored.stages[1].commandLockSec) &&
					 runtime.stage.movePolicy[1] == authored.stages[1].movePolicy));
			if (!bNumericMatch)
			{
				char message[224]{};
				sprintf_s(
					message,
					"[DataContract][Client] numeric-mismatch champ=%u slot=%u runtimeStage=%u authoredStage=%u runtimeWindow=%.3f authoredWindow=%.3f\n",
					static_cast<u32_t>(authoredChampion.champion),
					static_cast<u32_t>(slot),
					static_cast<u32_t>(runtime.stage.stageCount),
					static_cast<u32_t>(authored.stageCount),
					runtime.stage.stageWindowSec,
					authored.stageWindowSec);
				OutputDebugStringA(message);
				++numericMismatches;
			}

			if (runtime.target.shape[0] != TargetShape(authored.stages[0].targetMode) ||
				(authored.stageCount >= 2u &&
				 runtime.target.shape[1] != TargetShape(authored.stages[1].targetMode)))
			{
				char message[240]{};
				sprintf_s(
					message,
					"[DataContract][Client] target-mismatch champ=%u slot=%u runtimeStage1=%u runtimeStage2=%u authoredStage1=%u authoredStage2=%u\n",
					static_cast<u32_t>(authoredChampion.champion),
					static_cast<u32_t>(slot),
					static_cast<u32_t>(runtime.target.shape[0]),
					static_cast<u32_t>(runtime.target.shape[1]),
					static_cast<u32_t>(authored.stages[0].targetMode),
					static_cast<u32_t>(authored.stages[1].targetMode));
				OutputDebugStringA(message);
				++targetMismatches;
			}

			bool_t bRankValuesDiffer = false;
			for (u8_t rank = 1u; rank < authored.rankCount; ++rank)
			{
				bRankValuesDiffer = bRankValuesDiffer ||
					!Near(authored.cooldownSecByRank[rank], authored.cooldownSecByRank[0]) ||
					!Near(authored.manaCostByRank[rank], authored.manaCostByRank[0]);
			}
			if (bRankValuesDiffer)
				++rankTableCollapses;

			if (authored.stageCount >= 2u)
			{
				++twoStageSkills;
				char message[256]{};
				sprintf_s(
					message,
					"[DataContract][TwoStage] champ=%u slot=%u activation=%u runtimeShape1=%u runtimeShape2=%u window=%.3f lock1=%.3f lock2=%.3f\n",
					static_cast<u32_t>(authoredChampion.champion),
					static_cast<u32_t>(slot),
					static_cast<u32_t>(authored.inputActivation),
					static_cast<u32_t>(runtime.target.shape[0]),
					static_cast<u32_t>(runtime.target.shape[1]),
					runtime.stage.stageWindowSec,
					runtime.stage.lockDurationSec[0],
					runtime.stage.lockDurationSec[1]);
				OutputDebugStringA(message);
			}
		}
	}

	const bool_t bPass = championCount == 17u &&
		checkedSkills == 85u &&
		m_LegacyMap.size() == 85u &&
		missingSkills == 0u &&
		numericMismatches == 0u &&
		targetMismatches == 0u &&
		twoStageSkills == 13u &&
		twoStageVisuals == 16u &&
		presentationOnlyStage2Visuals == 3u &&
		presentationTimingMismatches == 0u;
	char summary[512]{};
	sprintf_s(
		summary,
		"[DataContract][Client] %s champions=%zu checked=%u registry=%zu missing=%u numericMismatch=%u targetMismatch=%u gameplayTwoStage=%u visualTwoStage=%u presentationOnlyStage2=%u presentationTimingMismatch=%u conditionalSchemaLoss=%u perStageTargetSchemaLoss=%u rankTableCollapse=%u\n",
		bPass ? "PASS" : "FAIL",
		championCount,
		checkedSkills,
		m_LegacyMap.size(),
		missingSkills,
		numericMismatches,
		targetMismatches,
		twoStageSkills,
		twoStageVisuals,
		presentationOnlyStage2Visuals,
		presentationTimingMismatches,
		conditionalTargets,
		stageTargetSchemaLosses,
		rankTableCollapses);
	OutputDebugStringA(summary);
	return bPass;
#endif
}
