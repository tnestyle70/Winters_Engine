#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"

#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Core/World/World.h"

namespace
{
	bool_t IsValidChampion(eChampion champion)
	{
		return champion != eChampion::NONE && champion != eChampion::END;
	}

	bool_t ContainsSlot(u8_t mask, u8_t slot)
	{
		return slot < 8u && (mask & static_cast<u8_t>(1u << slot)) != 0u;
	}

	u16_t ResolveCapturedUltimateHookVariant(eChampion champion)
	{
		switch (champion)
		{
		case eChampion::ANNIE:
		case eChampion::IRELIA:
		case eChampion::YASUO:
			return GameplayHookVariant::R_OnCastAccepted;
		default:
			return GameplayHookVariant::R_CastFrame;
		}
	}
}

void CSpellbookFormOverrideSystem::Execute(CWorld& world, const TickContext& tc)
{
	const auto spellbookEntities =
		DeterministicEntityIterator<SpellbookOverrideComponent>::CollectSorted(world);
	for (EntityID entity : spellbookEntities)
	{
		if (!world.HasComponent<SpellbookOverrideComponent>(entity))
			continue;

		auto& spellbook = world.GetComponent<SpellbookOverrideComponent>(entity);
		const bool_t bOwnerDead =
			world.HasComponent<HealthComponent>(entity) &&
			(world.GetComponent<HealthComponent>(entity).bIsDead ||
				world.GetComponent<HealthComponent>(entity).fCurrent <= 0.f);
		if (!spellbook.bActive ||
			bOwnerDead ||
			spellbook.fRemainingSec <= 0.f ||
			!CanDispatchCapturedUltimate(spellbook.sourceChampion))
		{
			world.RemoveComponent<SpellbookOverrideComponent>(entity);
			continue;
		}

		spellbook.fRemainingSec -= tc.fDt;
		if (spellbook.fRemainingSec <= 0.f)
			world.RemoveComponent<SpellbookOverrideComponent>(entity);
	}

	const auto formEntities =
		DeterministicEntityIterator<FormOverrideComponent>::CollectSorted(world);
	for (EntityID entity : formEntities)
	{
		if (!world.HasComponent<FormOverrideComponent>(entity))
			continue;

		auto& form = world.GetComponent<FormOverrideComponent>(entity);
		if (!form.bActive || form.fRemainingSec == 0.f)
		{
			world.RemoveComponent<FormOverrideComponent>(entity);
			continue;
		}

		if (form.fRemainingSec > 0.f)
		{
			form.fRemainingSec -= tc.fDt;
			if (form.fRemainingSec <= 0.f)
				world.RemoveComponent<FormOverrideComponent>(entity);
		}
	}
}

SkillOverrideResolveResult CSpellbookFormOverrideSystem::ResolveSkill(
	CWorld& world,
	EntityID caster,
	eChampion baseChampion,
	u8_t localSlot)
{
	SkillOverrideResolveResult result{};
	result.baseChampion = baseChampion;
	result.hookChampion = baseChampion;
	result.cooldownChampion = baseChampion;
	result.localSlot = localSlot;
	result.hookSlot = localSlot;
	result.cooldownSlot = localSlot;

	if (world.HasComponent<FormOverrideComponent>(caster))
	{
		const auto& form = world.GetComponent<FormOverrideComponent>(caster);
		if (form.bActive &&
			IsValidChampion(form.skillChampion) &&
			ContainsSlot(form.skillSlotMask, localSlot))
		{
			result.hookChampion = form.skillChampion;
			result.cooldownChampion = form.skillChampion;
			result.bOverridden = true;
		}
	}

	if (world.HasComponent<SpellbookOverrideComponent>(caster))
	{
		const auto& spellbook = world.GetComponent<SpellbookOverrideComponent>(caster);
		if (spellbook.bActive &&
			IsValidChampion(spellbook.sourceChampion) &&
			CanDispatchCapturedUltimate(spellbook.sourceChampion) &&
			spellbook.localSlot == localSlot)
		{
			result.hookChampion = spellbook.sourceChampion;
			result.hookSlot = spellbook.sourceSlot;
			result.cooldownChampion = baseChampion;
			result.cooldownSlot = localSlot;
			result.sourceRank = spellbook.sourceRank;
			result.bOverridden = true;
			result.bConsumeSpellbookOnAccept = true;
		}
	}

	return result;
}

SkillOverrideResolveResult CSpellbookFormOverrideSystem::ResolveBasicAttack(
	CWorld& world,
	EntityID caster,
	eChampion baseChampion)
{
	return ResolveSkill(
		world,
		caster,
		baseChampion,
		static_cast<u8_t>(eSkillSlot::BasicAttack));
}

eChampion CSpellbookFormOverrideSystem::ResolveVisualChampion(
	CWorld& world,
	EntityID caster,
	eChampion baseChampion)
{
	if (world.HasComponent<FormOverrideComponent>(caster))
	{
		const auto& form = world.GetComponent<FormOverrideComponent>(caster);
		if (form.bActive && IsValidChampion(form.visualChampion))
			return form.visualChampion;
	}

	return baseChampion;
}

bool_t CSpellbookFormOverrideSystem::CanDispatchCapturedUltimate(
	eChampion sourceChampion)
{
	if (!IsValidChampion(sourceChampion) || sourceChampion == eChampion::SYLAS)
		return false;

	const u16_t variant = ResolveCapturedUltimateHookVariant(sourceChampion);
	return CGameplayHookRegistry::Instance().Has(
		MakeGameplayHookId(sourceChampion, variant));
}

void CSpellbookFormOverrideSystem::ConsumeSpellbookOverride(
	CWorld& world,
	EntityID caster,
	u8_t localSlot)
{
	if (!world.HasComponent<SpellbookOverrideComponent>(caster))
		return;

	const auto& spellbook = world.GetComponent<SpellbookOverrideComponent>(caster);
	if (spellbook.localSlot == localSlot)
		world.RemoveComponent<SpellbookOverrideComponent>(caster);
}
