#include "WintersPCH.h"
#include "ECS/Systems/MCTSSystem.h"
#include "AI/Blackboard.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"

NS_BEGIN(Engine)

void CMCTSSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("MCTSSystem::Execute");

    m_fAccumDt += fTimeDelta;
    if (m_fAccumDt < TICK_INTERVAL)
        return;
    m_fAccumDt = 0.f;

    if (!m_pPlanner)
        return;

    world.ForEach<BotComponent, BlackboardComponent>(
        function<void(EntityID, BotComponent&, BlackboardComponent&)>(
            [&](EntityID id, BotComponent& bot, BlackboardComponent& bb)
            {
                if (!world.IsAlive(id) || bot.difficulty < 2)
                    return;

                const eMCTSAction best = m_pPlanner->Plan(world, id, ITERATIONS);
                bb.bb.Set("macroGoal", static_cast<i32_t>(best));
            }));
}

NS_END
