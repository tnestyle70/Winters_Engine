#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"

#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <functional>
#include <iostream>

namespace
{
    constexpr f32_t kKalistaESlowDurationSec = 2.0f;
    constexpr f32_t kKalistaESlowMoveSpeedMul = 0.55f;

    f32_t ResolveKalistaESkillEffectParam(
        CWorld& world,
        EntityID caster,
        const TickContext& tc,
        eSkillEffectParamId param,
        f32_t fallbackValue)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::E),
            param,
            fallbackValue);
    }

    f32_t ResolveRendRange(
        CWorld& world,
        EntityID caster,
        const TickContext& tc)
    {
        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::E));
        return range > 0.f ? range : 12.f;
    }

    void ApplyRendSlowToTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        const f32_t slowDurationSec = ResolveKalistaESkillEffectParam(
            world,
            caster,
            tc,
            eSkillEffectParamId::SlowDurationSec,
            kKalistaESlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveKalistaESkillEffectParam(
            world,
            caster,
            tc,
            eSkillEffectParamId::MoveSpeedMul,
            kKalistaESlowMoveSpeedMul);

        GameplayStatus::ApplySlow(
            world,
            tc,
            target,
            caster,
            eChampion::KALISTA,
            eSkillSlot::E,
            slowDurationSec,
            slowMoveSpeedMul);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        if (ctx.pCommand && ctx.pCommand->targetEntity != NULL_ENTITY)
        {
            ApplyRendSlowToTarget(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                ctx.pCommand->targetEntity);
            return;
        }

        if (!world.HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const f32_t range = ResolveRendRange(world, ctx.casterEntity, *ctx.pTickCtx);
        const f32_t rangeSq = range * range;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == ctx.casterEntity || champion.team == ctx.casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > rangeSq)
                        return;

                    ApplyRendSlowToTarget(
                        world,
                        *ctx.pTickCtx,
                        ctx.casterEntity,
                        target);
                }));

        std::cout << "[KalistaSim] E rend slow caster=" << ctx.casterEntity << "\n";
    }
}

namespace KalistaGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KALISTA, GameplayHookVariant::E_OnCastAccepted), &OnE);

        s_bRegistered = true;
        std::cout << "[KalistaSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        (void)world;
        (void)tc;
    }
}
