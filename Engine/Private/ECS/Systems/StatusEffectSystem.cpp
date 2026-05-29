#include "ECS/Systems/StatusEffectSystem.h"
#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ProfilerAPI.h"
#include <vector>

std::unique_ptr<CStatusEffectSystem> CStatusEffectSystem::Create()
{
	auto pInstance = unique_ptr<CStatusEffectSystem>(
		new CStatusEffectSystem());

	if (!pInstance)
		return nullptr;

	return pInstance;
}

void CStatusEffectSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
	WINTERS_PROFILE_SCOPE("Status::Execute");

	std::vector<EntityID> vecRemoveStun;
	std::vector<EntityID> vecRemoveSlow;
	std::vector<EntityID> vecRemoveDisarm;

	world.ForEach<StunComponent>(
		std::function<void(EntityID, StunComponent&)>(
			[&](EntityID e, StunComponent& s)
			{
				s.fRemaining -= fTimeDelta;
				if (s.fRemaining <= 0.f)
					vecRemoveStun.push_back(e);
			}));
	
	world.ForEach<SlowComponent>(
		std::function<void(EntityID, SlowComponent&)>(
			[&](EntityID entity, SlowComponent& slow)
			{
				slow.fRemaining -= fTimeDelta;
				if (slow.fRemaining <= 0.f)
					vecRemoveSlow.push_back(entity);
			}));

	world.ForEach<DisarmComponent>(
		std::function<void(EntityID, DisarmComponent&)>(
			[&](EntityID entity, DisarmComponent& disarm)
			{
				disarm.fRemaining -= fTimeDelta;
				if (disarm.fRemaining <= 0.f)
					vecRemoveDisarm.push_back(entity);
			}));

	for (EntityID entity : vecRemoveStun)
		world.RemoveComponent<StunComponent>(entity);
	for (EntityID entity : vecRemoveSlow)
		world.RemoveComponent<SlowComponent>(entity);
	for (EntityID entity : vecRemoveDisarm)
		world.RemoveComponent<DisarmComponent>(entity);
}
