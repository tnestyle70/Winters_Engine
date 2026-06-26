#include "Shared/GameSim/Systems/Recall/RecallSystem.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/AttackChaseComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/RespawnComponent.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Core/World/World.h"

namespace
{
	bool_t IsDead(CWorld& world, EntityID entity)
	{
		if (!world.HasComponent<HealthComponent>(entity))
			return false;

		const auto& health = world.GetComponent<HealthComponent>(entity);
		return health.bIsDead || health.fCurrent <= 0.f;
	}

	void RefillHealth(CWorld& world, EntityID entity)
	{
		if (!world.HasComponent<HealthComponent>(entity))
			return;

		auto& health = world.GetComponent<HealthComponent>(entity);
		health.fCurrent = health.fMaximum;
		health.bIsDead = false;

		if (world.HasComponent<ChampionComponent>(entity))
		{
			auto& champion = world.GetComponent<ChampionComponent>(entity);
			champion.hp = health.fCurrent;
			champion.maxHp = health.fMaximum;
		}
	}


	void ClearMoveTarget(CWorld& world, EntityID entity)
	{
		if (world.HasComponent<MoveTargetComponent>(entity))
			world.GetComponent<MoveTargetComponent>(entity) = MoveTargetComponent{};
	}

	void ClearAttackChase(CWorld& world, EntityID entity)
	{
		if (world.HasComponent<AttackChaseComponent>(entity))
			world.RemoveComponent<AttackChaseComponent>(entity);
	}

	void SetIdlePose(CWorld& world, EntityID entity, const TickContext& tc)
	{
		SetPoseState(world, entity, ePoseStateId::Idle, tc.tickIndex);
	}
}

void CRecallSystem::Execute(CWorld& world, const TickContext& tc)
{
	const auto entities = DeterministicEntityIterator<RecallComponent>::CollectSorted(world);

	for (EntityID entity : entities)
	{
		if (!world.HasComponent<RecallComponent>(entity))
			continue;

		auto& recall = world.GetComponent<RecallComponent>(entity);
		if (!recall.bActive)
		{
			world.RemoveComponent<RecallComponent>(entity);
			continue;
		}

		if (!world.IsAlive(entity) ||
			!world.HasComponent<RespawnComponent>(entity) ||
			!world.HasComponent<TransformComponent>(entity) ||
			IsDead(world, entity))
		{
			world.RemoveComponent<RecallComponent>(entity);
			continue;
		}

		recall.fRemainingSec -= tc.fDt;
		if (recall.fRemainingSec > 0.f)
			continue;

		const Vec3& vSpawnPos = world.GetComponent<RespawnComponent>(entity).spawnPos;
		world.GetComponent<TransformComponent>(entity).SetPosition(vSpawnPos);
		RefillHealth(world, entity);
		ClearMoveTarget(world, entity);
		ClearAttackChase(world, entity);
		SetIdlePose(world, entity, tc);
		world.RemoveComponent<RecallComponent>(entity);
	}
}
