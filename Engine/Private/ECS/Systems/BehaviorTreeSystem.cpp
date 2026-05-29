#include "WintersPCH.h"
#include "ECS/Systems/BehaviorTreeSystem.h"
#include "AI/Blackboard.h"
#include "ECS/World.h"
#include "ProfilerAPI.h"

NS_BEGIN(Engine)

void CBehaviorTreeSystem::Execute(CWorld& world, f32_t fTimeDelta)
{
    WINTERS_PROFILE_SCOPE("BTSystem::Execute");

    world.ForEach<BotComponent, BlackboardComponent>(
        function<void(EntityID, BotComponent&, BlackboardComponent&)>(
            [&](EntityID ID, BotComponent& bot, BlackboardComponent& bb)
            {
                if (!world.IsAlive(ID) || !bot.pBT)
                    return;

                bot.tickAccumulator += fTimeDelta;
                if (bot.tickAccumulator < TICK_INTERVAL)
                    return;

                const f32_t dt = bot.tickAccumulator;
                bot.tickAccumulator = 0.f;

                BTContext ctx{};
                ctx.pWorld = &world;
                ctx.self = ID;
                ctx.pBB = &bb.bb;
                ctx.dt = dt;

                bot.pBT->Tick(ctx);
            }));
}

NS_END
