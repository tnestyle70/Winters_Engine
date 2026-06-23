#include "Shared/GameSim/Champions/Riven/RivenGameSim.h"

#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <functional>
#include <iostream>

namespace
{
    constexpr f32_t kRivenQStackWindowSec = 4.0f;
    constexpr f32_t kRivenQ3Radius = 2.25f;
    constexpr f32_t kRivenQ3AirborneDurationSec = 0.75f;
    constexpr f32_t kRivenWRadius = 2.5f;
    constexpr f32_t kRivenWStunDurationSec = 0.75f;

    f32_t ResolveRivenSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::RIVEN,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    RivenStateComponent& EnsureRivenState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<RivenStateComponent>(caster))
            world.AddComponent<RivenStateComponent>(caster, RivenStateComponent{});

        return world.GetComponent<RivenStateComponent>(caster);
    }

    void ForEachEnemyChampionInRadius(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        f32_t radius,
        const std::function<void(EntityID)>& visit)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        const f32_t radiusSq = radius * radius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == caster || champion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > radiusSq)
                        return;

                    visit(target);
                }));
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        RivenStateComponent& state = EnsureRivenState(world, ctx.casterEntity);
        const u8_t nextStack = static_cast<u8_t>(std::min<u32_t>(
            3u,
            static_cast<u32_t>(state.qStackCount) + 1u));
        const f32_t q3Radius = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Radius,
            kRivenQ3Radius);
        const f32_t q3AirborneDurationSec = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::AirborneDurationSec,
            kRivenQ3AirborneDurationSec);
        const f32_t qStackWindowSec = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::StackWindowSec,
            kRivenQStackWindowSec);

        if (nextStack >= 3u)
        {
            ForEachEnemyChampionInRadius(
                world,
                ctx.casterEntity,
                ctx.casterTeam,
                q3Radius,
                [&](EntityID target)
                {
                    GameplayStatus::ApplyAirborne(
                        world,
                        *ctx.pTickCtx,
                        target,
                        ctx.casterEntity,
                        eChampion::RIVEN,
                        eSkillSlot::Q,
                        q3AirborneDurationSec);
                });

            state.qStackCount = 0;
            state.qStackTimer = 0.f;
        }
        else
        {
            state.qStackCount = nextStack;
            state.qStackTimer = qStackWindowSec;
        }

        std::cout << "[RivenSim] Q caster=" << ctx.casterEntity
            << " stack=" << static_cast<u32_t>(state.qStackCount) << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const f32_t wRadius = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Radius,
            kRivenWRadius);
        const f32_t wStunDurationSec = ResolveRivenSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::StunDurationSec,
            kRivenWStunDurationSec);
        ForEachEnemyChampionInRadius(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            wRadius,
            [&](EntityID target)
            {
                GameplayStatus::ApplyStun(
                    world,
                    *ctx.pTickCtx,
                    target,
                    ctx.casterEntity,
                    eChampion::RIVEN,
                    eSkillSlot::W,
                    wStunDurationSec);
            });

        std::cout << "[RivenSim] W stun caster=" << ctx.casterEntity << "\n";
    }
}

namespace RivenGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::RIVEN, GameplayHookVariant::Q_OnCastAccepted), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::RIVEN, GameplayHookVariant::W_CastFrame), &OnW);

        s_bRegistered = true;
        std::cout << "[RivenSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<RivenStateComponent>(
            std::function<void(EntityID, RivenStateComponent&)>(
                [&](EntityID, RivenStateComponent& state)
                {
                    if (state.qStackTimer <= 0.f)
                        return;

                    state.qStackTimer = std::max(0.f, state.qStackTimer - tc.fDt);
                    if (state.qStackTimer <= 0.f)
                        state.qStackCount = 0;
                }));
    }
}
