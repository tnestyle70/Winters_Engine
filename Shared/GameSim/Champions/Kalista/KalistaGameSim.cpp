#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"

#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr f32_t kKalistaESlowDurationSec = 2.0f;
    constexpr f32_t kKalistaESlowMoveSpeedMul = 0.55f;
    constexpr f32_t kKalistaWSentinelRange = 12.0f;
    constexpr f32_t kKalistaWSentinelLifetimeSec = 12.0f;
    constexpr f32_t kKalistaWSentinelSpeed = 3.5f;
    constexpr f32_t kKalistaWSentinelSightRange = 10.0f;
    constexpr f32_t kKalistaWSentinelRadius = 0.45f;
    constexpr f32_t kKalistaWSentinelHalfAngleCos = 0.8660254f;

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

    f32_t ResolveKalistaWSkillEffectParam(
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
            static_cast<u8_t>(eSkillSlot::W),
            param,
            fallbackValue);
    }

    Vec3 ResolveKalistaWDirection(const GameplayHookContext& ctx, const Vec3& origin)
    {
        if (ctx.pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(
                ctx.pCommand->direction,
                Vec3{},
                0.0001f);
            if (dir.x * dir.x + dir.z * dir.z > 0.0001f)
                return dir;

            dir = WintersMath::NormalizeXZ(
                Vec3{
                    ctx.pCommand->groundPos.x - origin.x,
                    0.f,
                    ctx.pCommand->groundPos.z - origin.z },
                Vec3{},
                0.0001f);
            if (dir.x * dir.x + dir.z * dir.z > 0.0001f)
                return dir;
        }

        if (ctx.pWorld && ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            const Vec3 rot = ctx.pWorld->GetComponent<TransformComponent>(
                ctx.casterEntity).GetRotation();
            return Vec3{ std::sinf(rot.y), 0.f, std::cosf(rot.y) };
        }

        return Vec3{ 0.f, 0.f, 1.f };
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

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const TickContext& tc = *ctx.pTickCtx;
        const Vec3 origin = world.GetComponent<TransformComponent>(
            ctx.casterEntity).GetPosition();
        const Vec3 forward = ResolveKalistaWDirection(ctx, origin);
        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            ctx.casterEntity,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::W));
        const f32_t patrolRange = range > 0.f
            ? range
            : ResolveKalistaWSkillEffectParam(
                world,
                ctx.casterEntity,
                tc,
                eSkillEffectParamId::Range,
                kKalistaWSentinelRange);
        const f32_t lifetimeSec = ResolveKalistaWSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::EffectDurationSec,
            kKalistaWSentinelLifetimeSec);
        const f32_t speed = ResolveKalistaWSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::Speed,
            kKalistaWSentinelSpeed);
        const f32_t sightRange = ResolveKalistaWSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::SummonSightRange,
            kKalistaWSentinelSightRange);
        const f32_t radius = ResolveKalistaWSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::SummonRadius,
            kKalistaWSentinelRadius);
        const f32_t halfAngleCos = ResolveKalistaWSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::HalfAngleCos,
            kKalistaWSentinelHalfAngleCos);

        Vec3 end{
            origin.x + forward.x * patrolRange,
            origin.y,
            origin.z + forward.z * patrolRange
        };
        if (tc.pWalkable)
        {
            Vec3 clamped = end;
            if (tc.pWalkable->TryClampMoveSegmentXZ(origin, end, radius, clamped))
                end = clamped;
        }

        const EntityID sentinel = world.CreateEntity();

        KalistaSentinelComponent state{};
        state.owner = ctx.casterEntity;
        state.team = ctx.casterTeam;
        state.start = origin;
        state.end = end;
        state.forward = forward;
        state.lifetimeSec = lifetimeSec;
        state.patrolSpeed = speed;
        state.sightRange = sightRange;
        state.halfAngleCos = halfAngleCos;
        world.AddComponent<KalistaSentinelComponent>(sentinel, state);

        TransformComponent transform{};
        transform.SetPosition(origin);
        transform.SetRotation(Vec3{ 0.f, std::atan2f(forward.x, forward.z), 0.f });
        world.AddComponent<TransformComponent>(sentinel, transform);

        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Ward;
        spatial.team = static_cast<u8_t>(ctx.casterTeam);
        spatial.radius = radius;
        world.AddComponent<SpatialAgentComponent>(sentinel, spatial);

        VisionSourceComponent vision{};
        vision.sightRange = sightRange;
        world.AddComponent<VisionSourceComponent>(sentinel, vision);

        VisionConeComponent cone{};
        cone.forward = forward;
        cone.halfAngleCos = halfAngleCos;
        world.AddComponent<VisionConeComponent>(sentinel, cone);

        VisibilityComponent visibility{};
        world.AddComponent<VisibilityComponent>(sentinel, visibility);

        if (tc.pEntityMap)
        {
            NetEntityIdComponent net{};
            net.netId = tc.pEntityMap->IssueNew(sentinel);
            world.AddComponent<NetEntityIdComponent>(sentinel, net);
        }

        std::cout << "[KalistaSim] W sentinel caster=" << ctx.casterEntity << "\n";
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
            MakeGameplayHookId(eChampion::KALISTA, GameplayHookVariant::W_OnCastAccepted), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KALISTA, GameplayHookVariant::E_OnCastAccepted), &OnE);

        s_bRegistered = true;
        std::cout << "[KalistaSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> expired;
        world.ForEach<KalistaSentinelComponent, TransformComponent>(
            std::function<void(EntityID, KalistaSentinelComponent&, TransformComponent&)>(
                [&](EntityID entity, KalistaSentinelComponent& state, TransformComponent& transform)
                {
                    state.elapsedSec += tc.fDt;
                    if (state.elapsedSec >= state.lifetimeSec)
                    {
                        expired.push_back(entity);
                        return;
                    }

                    const f32_t dx = state.end.x - state.start.x;
                    const f32_t dz = state.end.z - state.start.z;
                    const f32_t distance = std::sqrt(dx * dx + dz * dz);
                    if (distance <= 0.001f || state.patrolSpeed <= 0.001f)
                        return;
                    const Vec3 baseForward = WintersMath::NormalizeXZ(
                        Vec3{ dx, 0.f, dz },
                        Vec3{ 0.f, 0.f, 1.f },
                        0.0001f);

                    const f32_t oneWaySec = distance / state.patrolSpeed;
                    const f32_t cycleSec = oneWaySec * 2.f;
                    f32_t phaseSec = std::fmod(state.elapsedSec, cycleSec);
                    const bool_t bForward = phaseSec <= oneWaySec;
                    f32_t t = bForward
                        ? phaseSec / oneWaySec
                        : 1.f - ((phaseSec - oneWaySec) / oneWaySec);
                    t = std::clamp(t, 0.f, 1.f);

                    Vec3 pos{
                        state.start.x + dx * t,
                        state.start.y,
                        state.start.z + dz * t
                    };

                    if (tc.pWalkable)
                    {
                        Vec3 clamped = pos;
                        if (tc.pWalkable->TryClampMoveSegmentXZ(
                            transform.GetPosition(),
                            pos,
                            kKalistaWSentinelRadius,
                            clamped))
                        {
                            pos = clamped;
                        }
                    }

                    const Vec3 travelForward = bForward
                        ? baseForward
                        : Vec3{ -baseForward.x, 0.f, -baseForward.z };
                    state.forward = travelForward;
                    transform.SetPosition(pos);
                    transform.SetRotation(Vec3{
                        0.f,
                        std::atan2f(travelForward.x, travelForward.z),
                        0.f });

                    if (world.HasComponent<VisionConeComponent>(entity))
                    {
                        auto& cone = world.GetComponent<VisionConeComponent>(entity);
                        cone.forward = travelForward;
                        cone.halfAngleCos = state.halfAngleCos;
                    }
                    if (world.HasComponent<VisionSourceComponent>(entity))
                        world.GetComponent<VisionSourceComponent>(entity).sightRange = state.sightRange;
                }));

        for (EntityID entity : expired)
            world.DestroyEntity(entity);
    }
}
