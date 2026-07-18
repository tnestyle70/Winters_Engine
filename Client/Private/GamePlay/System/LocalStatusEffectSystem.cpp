#include "GamePlay/Systems/LocalStatusEffectSystem.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"

#include <functional>
#include <vector>

std::unique_ptr<CLocalStatusEffectSystem> CLocalStatusEffectSystem::Create()
{
    return std::unique_ptr<CLocalStatusEffectSystem>(new CLocalStatusEffectSystem());
}

void CLocalStatusEffectSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
    // RemoveComponent<T>는 스토어 로컬 연산이라 Write<T>로 충분하다.
    // ForEach로 발견된 엔티티만 제거하므로 스토어 lazy-insert 경로는 타지 않는다.
    builder.Write<StunComponent>()
        .Write<SlowComponent>()
        .Write<DisarmComponent>();
}

void CLocalStatusEffectSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("LocalStatus::Execute");

    std::vector<EntityID> vecRemoveStun;
    std::vector<EntityID> vecRemoveSlow;
    std::vector<EntityID> vecRemoveDisarm;

    world.ForEach<StunComponent>(
        std::function<void(EntityID, StunComponent&)>(
            [&](EntityID entity, StunComponent& stun)
            {
                stun.fRemaining -= fTimeDelta;
                if (stun.fRemaining <= 0.f)
                    vecRemoveStun.push_back(entity);
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
