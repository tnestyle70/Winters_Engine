#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr f32_t kYoneQRange = 4.75f;
    constexpr f32_t kYoneQRadius = 0.85f;
    constexpr f32_t kYoneQDamage = 75.f;
    constexpr f32_t kYoneWRange = 6.0f;
    constexpr f32_t kYoneWRadius = 1.5f;
    constexpr f32_t kYoneWDamage = 65.f;
    constexpr f32_t kYoneEDashDistance = 4.0f;
    constexpr f32_t kYoneEDashDurationSec = 0.25f;
    constexpr f32_t kYoneRRange = 10.0f;
    constexpr f32_t kYoneRRadius = 1.7f;
    constexpr f32_t kYoneRDashDelaySec = 0.50f;
    constexpr f32_t kYoneRDashDurationSec = 0.16f;
    constexpr f32_t kYoneRDamage = 150.f;
    constexpr f32_t kYoneRAirborneDurationSec = 0.75f;

    enum class eYoneDashKind : u8_t
    {
        SoulOut,
        SoulReturn,
        Ultimate,
    };

    struct YoneDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t delaySec = 0.f;
        f32_t durationSec = 0.25f;
        eYoneDashKind kind = eYoneDashKind::SoulOut;
    };

    YoneSimComponent& EnsureYoneState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<YoneSimComponent>(caster))
            world.AddComponent<YoneSimComponent>(caster, YoneSimComponent{});

        return world.GetComponent<YoneSimComponent>(caster);
    }

    Vec3 ResolveDirection(const GameplayHookContext& ctx, const Vec3& origin)
    {
        if (ctx.pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;

            dir = WintersMath::NormalizeXZ(Vec3{
                ctx.pCommand->groundPos.x - origin.x,
                0.f,
                ctx.pCommand->groundPos.z - origin.z
            });
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<MoveTargetComponent>(entity))
            world.GetComponent<MoveTargetComponent>(entity).bHasTarget = false;
    }

    void RotateToward(CWorld& world, EntityID entity, const Vec3& direction)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;

        const Vec3 dir = WintersMath::NormalizeXZ(direction);
        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::YONE, dir, rot.y),
            rot.z });
    }

    void StartDash(CWorld& world, EntityID entity, const Vec3& end,
        f32_t durationSec, eYoneDashKind kind, f32_t delaySec = 0.f)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return;

        YoneDashComponent dash{};
        dash.start = world.GetComponent<TransformComponent>(entity).GetPosition();
        dash.end = end;
        dash.delaySec = std::max(0.f, delaySec);
        dash.durationSec = durationSec;
        dash.kind = kind;

        if (world.HasComponent<YoneDashComponent>(entity))
            world.GetComponent<YoneDashComponent>(entity) = dash;
        else
            world.AddComponent<YoneDashComponent>(entity, dash);

        ClearMove(world, entity);
    }

    void StartYoneEAction(CWorld& world, EntityID entity,
        const TickContext& tc, u8_t stage)
    {
        StartActionState(world, entity, eActionStateId::SkillE, tc.tickIndex, stage);
    }

    void EmitYoneEVisualEvent(CWorld& world, EntityID caster, const TickContext& tc, u8_t stage)
    {
        Vec3 position{};
        Vec3 direction{ 0.f, 0.f, 1.f };
        if (world.HasComponent<TransformComponent>(caster))
            position = world.GetComponent<TransformComponent>(caster).GetPosition();
        if (world.HasComponent<YoneSimComponent>(caster))
        {
            const auto& state = world.GetComponent<YoneSimComponent>(caster);
            direction = WintersMath::NormalizeXZ(Vec3{
                state.anchorPosition.x - position.x,
                0.f,
                state.anchorPosition.z - position.z
            });
        }

        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = caster;
        event.effectId = MakeGameplayHookId(eChampion::YONE, GameplayHookVariant::E_CastFrame);
        event.slot = static_cast<u8_t>(eSkillSlot::E);
        event.rank = 1;
        event.flags = static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (1u << 8) |
            static_cast<u16_t>(eSkillSlot::E));
        event.position = position;
        event.direction = direction;
        event.durationMs = 700;
        event.startTick = tc.tickIndex;
        EnqueueReplicatedEvent(world, event);
    }

    void StartSoulReturn(CWorld& world, EntityID caster, const TickContext* pTickCtx,
        bool_t bEmitReturnCue)
    {
        if (!world.HasComponent<YoneSimComponent>(caster) ||
            !world.HasComponent<TransformComponent>(caster))
        {
            return;
        }

        auto& state = world.GetComponent<YoneSimComponent>(caster);
        if (!state.bSoulUnboundActive || state.bReturning)
            return;

        state.bReturning = true;
        StartDash(world, caster, state.anchorPosition, kYoneEDashDurationSec, eYoneDashKind::SoulReturn);
        RotateToward(world, caster, Vec3{
            state.anchorPosition.x - world.GetComponent<TransformComponent>(caster).GetPosition().x,
            0.f,
            state.anchorPosition.z - world.GetComponent<TransformComponent>(caster).GetPosition().z
        });

        if (bEmitReturnCue && pTickCtx)
        {
            StartYoneEAction(world, caster, *pTickCtx, 2);
            EmitYoneEVisualEvent(world, caster, *pTickCtx, 2);
        }
    }

    void EnqueueLineDamage(CWorld& world, EntityID caster, eTeam casterTeam,
        const Vec3& start, const Vec3& end, f32_t radius, f32_t damage,
        u16_t skillId, u8_t rank)
    {
        const f32_t radiusSq = radius * radius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == caster || champion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqPointToSegmentXZ(transform.GetPosition(), start, end) > radiusSq)
                        return;

                    DamageRequest request{};
                    request.source = caster;
                    request.target = entity;
                    request.sourceTeam = casterTeam;
                    request.type = eDamageType::Physical;
                    request.flatAmount = damage;
                    request.skillId = skillId;
                    request.rank = rank;
                    request.flags = DamageFlag_OnHit;
                    EnqueueDamageRequest(world, request);
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID entity, MinionComponent& minion, TransformComponent& transform)
                {
                    if (minion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqPointToSegmentXZ(transform.GetPosition(), start, end) > radiusSq)
                        return;

                    DamageRequest request{};
                    request.source = caster;
                    request.target = entity;
                    request.sourceTeam = casterTeam;
                    request.type = eDamageType::Physical;
                    request.flatAmount = damage;
                    request.skillId = skillId;
                    request.rank = rank;
                    request.flags = DamageFlag_OnHit;
                    EnqueueDamageRequest(world, request);
                }));
    }

    void ApplyLineAirborne(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& start,
        const Vec3& end,
        f32_t radius)
    {
        const f32_t radiusSq = radius * radius;
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == caster || champion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqPointToSegmentXZ(transform.GetPosition(), start, end) > radiusSq)
                        return;

                    GameplayStatus::ApplyAirborne(
                        world,
                        tc,
                        target,
                        caster,
                        eChampion::YONE,
                        eSkillSlot::R,
                        kYoneRAirborneDurationSec);
                }));
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 direction = ResolveDirection(ctx, origin);
        const Vec3 end{ origin.x + direction.x * kYoneQRange, origin.y, origin.z + direction.z * kYoneQRange };
        RotateToward(world, ctx.casterEntity, direction);

        EnqueueLineDamage(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            end,
            kYoneQRadius,
            kYoneQDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YONE) << 8) | 1u),
            ctx.skillRank);

        std::cout << "[YoneSim] Q accepted caster=" << ctx.casterEntity << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 direction = ResolveDirection(ctx, origin);
        const Vec3 end{ origin.x + direction.x * kYoneWRange, origin.y, origin.z + direction.z * kYoneWRange };
        RotateToward(world, ctx.casterEntity, direction);

        EnqueueLineDamage(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            end,
            kYoneWRadius,
            kYoneWDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YONE) << 8) | 2u),
            ctx.skillRank);

        std::cout << "[YoneSim] W accepted caster=" << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        auto& transform = world.GetComponent<TransformComponent>(ctx.casterEntity);
        const Vec3 origin = transform.GetPosition();
        YoneSimComponent& state = EnsureYoneState(world, ctx.casterEntity);

        if (state.bSoulUnboundActive)
        {
            StartSoulReturn(world, ctx.casterEntity, ctx.pTickCtx, false);
            std::cout << "[YoneSim] E return caster=" << ctx.casterEntity << "\n";
            return;
        }

        const Vec3 direction = ResolveDirection(ctx, origin);
        const Vec3 end{
            origin.x + direction.x * kYoneEDashDistance,
            origin.y,
            origin.z + direction.z * kYoneEDashDistance
        };

        state.bSoulUnboundActive = true;
        state.bReturning = false;
        state.soulDurationSec = 5.f;
        state.soulTimerSec = state.soulDurationSec;
        state.anchorPosition = origin;

        StartDash(world, ctx.casterEntity, end, kYoneEDashDurationSec, eYoneDashKind::SoulOut);
        RotateToward(world, ctx.casterEntity, direction);

        std::cout << "[YoneSim] E out caster=" << ctx.casterEntity
            << " anchor=(" << origin.x << "," << origin.z << ")"
            << " end=(" << end.x << "," << end.z << ")\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 direction = ResolveDirection(ctx, origin);
        const Vec3 end{ origin.x + direction.x * kYoneRRange, origin.y, origin.z + direction.z * kYoneRRange };
        RotateToward(world, ctx.casterEntity, direction);
        StartDash(world, ctx.casterEntity, end, kYoneRDashDurationSec,
            eYoneDashKind::Ultimate, kYoneRDashDelaySec);

        EnqueueLineDamage(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            end,
            kYoneRRadius,
            kYoneRDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YONE) << 8) | 4u),
            ctx.skillRank);
        ApplyLineAirborne(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            end,
            kYoneRRadius);

        std::cout << "[YoneSim] R accepted caster=" << ctx.casterEntity << "\n";
    }
}

namespace YoneGameSim
{
    u8_t ResolveEStage(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<YoneSimComponent>(caster) &&
            world.GetComponent<YoneSimComponent>(caster).bSoulUnboundActive)
        {
            return 2;
        }

        return 1;
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YONE, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YONE, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YONE, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YONE, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[YoneSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<YoneDashComponent, TransformComponent>(
            std::function<void(EntityID, YoneDashComponent&, TransformComponent&)>(
                [&](EntityID entity, YoneDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

                    if (dash.delaySec > 0.f)
                    {
                        dash.delaySec = std::max(0.f, dash.delaySec - tc.fDt);
                        if (dash.delaySec > 0.f)
                            return;

                        dash.start = transform.GetPosition();
                        dash.elapsedSec = 0.f;
                    }

                    dash.elapsedSec += tc.fDt;
                    f32_t t = dash.durationSec > 0.01f
                        ? dash.elapsedSec / dash.durationSec
                        : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }

                    const Vec3 position{
                        dash.start.x + (dash.end.x - dash.start.x) * t,
                        dash.start.y + (dash.end.y - dash.start.y) * t,
                        dash.start.z + (dash.end.z - dash.start.z) * t
                    };
                    Vec3 guardedPosition = position;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable)
                    {
                        const Vec3 currentPos = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(currentPos, position, 0.5f, guardedPosition))
                        {
                            guardedPosition = currentPos;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(guardedPosition, position) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }
                    }

                    transform.SetPosition(guardedPosition);
                    if (bDashBlocked && t < 1.f)
                        finishedDashes.push_back(entity);
                }));

        for (EntityID entity : finishedDashes)
        {
            if (world.HasComponent<YoneDashComponent>(entity))
            {
                const auto dash = world.GetComponent<YoneDashComponent>(entity);
                if (dash.kind == eYoneDashKind::SoulReturn &&
                    world.HasComponent<YoneSimComponent>(entity) &&
                    world.HasComponent<TransformComponent>(entity))
                {
                    auto& state = world.GetComponent<YoneSimComponent>(entity);
                    world.GetComponent<TransformComponent>(entity).SetPosition(state.anchorPosition);
                    state.bSoulUnboundActive = false;
                    state.bReturning = false;
                    state.soulTimerSec = 0.f;
                }
            }

            world.RemoveComponent<YoneDashComponent>(entity);
        }

        world.ForEach<YoneSimComponent>(
            std::function<void(EntityID, YoneSimComponent&)>(
                [&](EntityID entity, YoneSimComponent& state)
                {
                    if (!state.bSoulUnboundActive || state.bReturning)
                        return;

                    state.soulTimerSec = std::max(0.f, state.soulTimerSec - tc.fDt);
                    if (state.soulTimerSec <= 0.f)
                        StartSoulReturn(world, entity, &tc, true);
                }));
    }
}
