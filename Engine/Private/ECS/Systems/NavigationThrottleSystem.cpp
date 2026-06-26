#include "WintersPCH.h"

#include "ECS/Systems/NavigationThrottleSystem.h"

#include "ECS/World.h"
#include "ECS/Components/NavigationThrottleComponent.h"
#include "ECS/Components/NavAgentComponent.h"
#include "ECS/Components/NavigationControlComponent.h"
#include "ProfilerAPI.h"

#include <algorithm>

NS_BEGIN(Engine)

namespace
{
	void Accept_RepathTarget(NavRepathThrottleComponent& throttle,
		const NavAgentComponent& nav)
	{
		throttle.vLastAcceptedTarget = nav.vTarget;
		throttle.fCooldownRemaining = throttle.fMinRepathInterval;
		throttle.bHasAcceptedTarget = true;
	}
}

void CNavigationThrottleSystem::DescribeAccess(CSystemAccessBuilder& builder) const
{
	builder.Read<NavigationControlComponent>()
		.Write<NavAgentComponent>()
		.Write<NavRepathThrottleComponent>();
}

void CNavigationThrottleSystem::Execute(CWorld & world, f32_t fTimeDelta)
{
	WINTERS_PROFILE_SCOPE("NavThrottle::Execute");

	u64_t uTracked = 0;
	u64_t uAccepted = 0;
	u64_t uSuppressed = 0;

    world.ForEach<NavigationControlComponent, NavAgentComponent, NavRepathThrottleComponent>(
        function<void(EntityID, NavigationControlComponent&, NavAgentComponent&, NavRepathThrottleComponent&)>
        (
            [&](EntityID, NavigationControlComponent& control, NavAgentComponent& nav,
                NavRepathThrottleComponent& throttle)
            {
                if (throttle.fCooldownRemaining > 0.f)
                    throttle.fCooldownRemaining = std::max(0.f, throttle.fCooldownRemaining - fTimeDelta);

                if (!control.bThrottleRepath || !nav.bHasGoal)
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
	
	WINTERS_PROFILE_COUNT("NavThrottle::Tracked", uTracked);
	WINTERS_PROFILE_COUNT("NavThrottle::RepathAccepted", uAccepted);
	WINTERS_PROFILE_COUNT("NavThrottle::RepathSuppressed", uSuppressed);
}

NS_END
