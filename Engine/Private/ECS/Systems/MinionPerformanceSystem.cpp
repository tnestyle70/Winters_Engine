#include "WintersPCH.h"

#include "ECS/Systems/MinionPerformanceSystem.h"

#include "ECS/World.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/MinionPerformanceComponents.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ProfilerAPI.h"

#include <algorithm>

NS_BEGIN(Engine)

namespace
{
	void Accept_RepathTarget(MinionNavThrottleComponent& throttle,
		const NavAgentComponent& nav)
	{
		throttle.vLastAcceptedTarget = nav.vTarget;
		throttle.fCooldownRemaining = throttle.fMinRepathInterval;
		throttle.bHasAcceptedTarget = true;
	}
}

void CMinionPerformanceSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.Read<MinionStateComponent>()
		.Write<NavAgentComponent>()
		.Write<MinionNavThrottleComponent>();
}

void CMinionPerformanceSystem::Execute(CWorld & world, f32_t fTimeDelta)
{
	WINTERS_PROFILE_SCOPE("MinionPerf::Execute");

	u64_t uTracked = 0;
	u64_t uAccepted = 0;
	u64_t uSuppressed = 0;

    world.ForEach<MinionStateComponent, NavAgentComponent, MinionNavThrottleComponent>(
        function<void(EntityID, MinionStateComponent&, NavAgentComponent&, MinionNavThrottleComponent&)>
        (
            [&](EntityID, MinionStateComponent& ms, NavAgentComponent& nav,
                MinionNavThrottleComponent& throttle)
            {
                if (throttle.fCooldownRemaining > 0.f)
                    throttle.fCooldownRemaining = std::max(0.f, throttle.fCooldownRemaining - fTimeDelta);

                if (ms.current != MinionStateComponent::Chase || !nav.bHasGoal)
                {
                    throttle.bHasAcceptedTarget = false;
                    throttle.fCooldownRemaining = 0.f;
                    return;
                }

                ++uTracked;
                if (!nav.bPathDirty)
                    return;

                if (nav.pathCellsX.empty() || nav.pathCellsY.empty())
                {
                    Accept_RepathTarget(throttle, nav);
                    ++uAccepted;
                    return;
                }

                const f32_t fDx = nav.vTarget.x - throttle.vLastAcceptedTarget.x;
                const f32_t fDz = nav.vTarget.z - throttle.vLastAcceptedTarget.z;
                const f32_t fDistSq = fDx * fDx + fDz * fDz;
                const f32_t fThresholdSq =
                    throttle.fTargetMoveThreshold * throttle.fTargetMoveThreshold;

                if (throttle.bHasAcceptedTarget
                    && throttle.fCooldownRemaining > 0.f
                    && fDistSq < fThresholdSq)
                {
                    nav.bPathDirty = false;
                    ++uSuppressed;
                    return;
                }

                Accept_RepathTarget(throttle, nav);
                ++uAccepted;
            }
        )
    );
	
	WINTERS_PROFILE_COUNT("MinionPerf::Tracked", uTracked);
	WINTERS_PROFILE_COUNT("MinionPerf::RepathAccepted", uAccepted);
	WINTERS_PROFILE_COUNT("MinionPerf::RepathSuppressed", uSuppressed);
}

NS_END