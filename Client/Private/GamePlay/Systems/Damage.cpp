#include "GamePlay/Systems/Damage.h"

#include "GameInstance.h"
#include "ECS/World.h"
#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"

namespace
{
	f32_t GetRequiredExperienceForNextLevel(u8_t level)
	{
		if (level >= 18)
			return 0.f;
		return 280.f + static_cast<f32_t>(level - 1) * 100.f;
	}

	void AwardLocalChampionKillExperience(CWorld& world, EntityID source)
	{
		if (source == NULL_ENTITY || !world.HasComponent<ExperienceComponent>(source))
			return;

		auto& xp = world.GetComponent<ExperienceComponent>(source);
		xp.current += 300.f;
		xp.total += 300.f;

		while (xp.level < 18)
		{
			if (xp.requiredForNextLevel <= 0.f)
				xp.requiredForNextLevel = GetRequiredExperienceForNextLevel(xp.level);
			if (xp.current < xp.requiredForNextLevel)
				break;

			xp.current -= xp.requiredForNextLevel;
			++xp.level;
			xp.requiredForNextLevel = GetRequiredExperienceForNextLevel(xp.level);

			if (world.HasComponent<ChampionComponent>(source))
				world.GetComponent<ChampionComponent>(source).level = xp.level;
			if (world.HasComponent<SkillRankComponent>(source))
				++world.GetComponent<SkillRankComponent>(source).pointsAvailable;
		}

		if (xp.level >= 18)
		{
			xp.current = 0.f;
			xp.requiredForNextLevel = 0.f;
		}
	}
}

void ApplyDamage(CWorld& world, EntityID source, eTeam srcTeam,
	EntityID target, f32_t amount)
{
	if (target == NULL_ENTITY || target == source)
		return;
	if (!world.HasComponent<ChampionComponent>(target))
		return;

	auto& champion = world.GetComponent<ChampionComponent>(target);
	if (champion.team == srcTeam)
		return;

	champion.hp = (champion.hp > amount) ? (champion.hp - amount) : 0.f;
	const bool_t bKilled = champion.hp <= 0.f;

	if (world.HasComponent<HealthComponent>(target))
	{
		auto& hp = world.GetComponent<HealthComponent>(target);
		hp.fCurrent = champion.hp;
		hp.fMaximum = champion.maxHp;
		hp.bIsDead = bKilled;
	}

	if (bKilled)
		AwardLocalChampionKillExperience(world, source);

	if (world.HasComponent<TransformComponent>(target))
	{
		Vec3 damageTextPos = world.GetComponent<TransformComponent>(target).GetPosition();
		damageTextPos.y += 2.1f;
		CGameInstance::Get()->UI_Push_DamageNumber(
			damageTextPos,
			amount,
			0u,
			false,
			bKilled);
	}

}

void EnqueueDamage(CWorld& world, const DamageRequestComponent& req)
{
	EntityID entity = world.CreateEntity();
	world.AddComponent<DamageRequestComponent>(entity, req);
}
