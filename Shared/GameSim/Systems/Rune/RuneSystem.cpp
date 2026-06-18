#include "Shared/GameSim/Systems/Rune/RuneSystem.h"

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"

namespace
{
	RuneRuntimeComponent& EnsureRuneRuntime(CWorld& world,
		EntityID entity)
	{
		if (!world.HasComponent<RuneRuntimeComponent>(entity))
			world.AddComponent<RuneRuntimeComponent>(entity, RuneRuntimeComponent{});
		return world.GetComponent<RuneRuntimeComponent>(entity);
	}

	bool_t AreEnemyChampions(CWorld& world, EntityID source, EntityID target)
	{
		if (source == NULL_ENTITY || target == NULL_ENTITY ||
			!world.IsAlive(source) || !world.IsAlive(target) ||
			!world.HasComponent<ChampionComponent>(source) ||
			!world.HasComponent<ChampionComponent>(target))
		{
			return false;
		}

		const auto& sourceChampion = world.GetComponent<ChampionComponent>(source);
		const auto& targetChampion = world.GetComponent<ChampionComponent>(target);
		return sourceChampion.team != targetChampion.team;
	}

	void MarkStatsDirty(CWorld& world, EntityID entity)
	{
		if (world.HasComponent<StatComponent>(entity))
			world.GetComponent<StatComponent>(entity).bDirty = true;
	}
}

bool_t CRuneSystem::HasRune(CWorld& world, EntityID entity, eRuneId runeId)
{
	if (runeId == eRuneId::None ||
		entity == NULL_ENTITY ||
		!world.HasComponent<RuneLoadoutComponent>(entity) ||
		!world.IsAlive(entity))
	{
		return false;
	}

	const RuneLoadoutComponent& loadout = world.GetComponent<RuneLoadoutComponent>(entity);
	for (u8_t i = 0; i < loadout.iCount && i < RuneLoadoutComponent::kMaxRunes; ++i)
	{
		if (loadout.eRunes[i] == runeId)
			return true;
	}

	return false;
}

void CRuneSystem::Execute(CWorld& world, const TickContext& tc)
{
	(void)world;
	(void)tc;
}

void CRuneSystem::OnBasicAttackHitChampion(CWorld& world,
	const TickContext& tc, EntityID source, EntityID target)
{
	(void)tc;

	if (!AreEnemyChampions(world, source, target) ||
		!HasRune(world, source, eRuneId::LethalTempo))
		return;

	RuneRuntimeComponent& runtime = EnsureRuneRuntime(world, source);
	if (runtime.iLethalTempoStacks >= RuneTuning::kLethalTempoMaxStacks)
		return;

	++runtime.iLethalTempoStacks;
	MarkStatsDirty(world, source);
}

void CRuneSystem::OnChampionDamageDealt(CWorld& world, const TickContext& tc,
	const DamageRequest& request, const DamageResult& result)
{
	(void)world;
	(void)tc;
	(void)request;
	(void)result;
}
