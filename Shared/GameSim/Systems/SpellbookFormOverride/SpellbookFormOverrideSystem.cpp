#include "Shared/GameSim/Systems/SpellbookFormOverride/SpellbookFormOverrideSystem.h"

#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/SpellbookOverrideComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
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
		if (!spellbook.bActive || spellbook.fRemainingSec <= 0.f)
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
		if (!form.bActive || form.fRemainingSec <= 0.f)
		{
			world.RemoveComponent<FormOverrideComponent>(entity);
			continue;
		}

		form.fRemainingSec -= tc.fDt;
		if (form.fRemainingSec <= 0.f)
			world.RemoveComponent<FormOverrideComponent>(entity);
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
