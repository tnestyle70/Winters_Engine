#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Definitions/WardDefinitions.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <cmath>
#include "Shared/GameSim/Systems/Move/DashArrival.h"

#include <functional>
#include <iostream>
#include <vector>

namespace
{
    f32_t ResolveLeeSinSkillEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::LEESIN,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveLeeSinSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return ResolveLeeSinSkillEffectParam(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            param,
            fallbackValue);
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
        if (dir.x == 0.f && dir.z == 0.f)
            return;

        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::LEESIN, dir, rot.y),
            rot.z });
    }

    void EnqueuePhysicalDamage(
        CWorld& world,
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        f32_t amount,
        u8_t slot,
        u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::LEESIN) << 8) | slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    void StartTargetDash(
        CWorld& world,
        EntityID caster,
        EntityID target,
        f32_t gap,
        f32_t durationSec)
    {
        if (!world.HasComponent<TransformComponent>(caster) ||
            target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }

        const Vec3 start = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 dir = WintersMath::NormalizeXZ(Vec3{
            targetPos.x - start.x,
            0.f,
            targetPos.z - start.z
        });
        const f32_t dx = targetPos.x - start.x;
        const f32_t dz = targetPos.z - start.z;
        const f32_t dist = std::sqrt(dx * dx + dz * dz);
        const f32_t moveDist = std::max(0.f, dist - gap);

        LeeSinDashComponent dash{};
        dash.vStart = start;
        dash.vEnd = Vec3{
            start.x + dir.x * moveDist,
            start.y,
            start.z + dir.z * moveDist
        };
        dash.fDurationSec = durationSec;

        if (world.HasComponent<LeeSinDashComponent>(caster))
            world.GetComponent<LeeSinDashComponent>(caster) = dash;
        else
            world.AddComponent<LeeSinDashComponent>(caster, dash);

        RotateToward(world, caster, dir);
        ClearMove(world, caster);
    }

    bool_t IsAlliedWardTarget(CWorld& world, EntityID target, eTeam casterTeam)
    {
        if (world.HasComponent<VisionSensorComponent>(target) &&
            world.GetComponent<VisionSensorComponent>(target).ownerTeam == static_cast<u8_t>(casterTeam))
        {
            return true;
        }

        if (world.HasComponent<SpatialAgentComponent>(target))
        {
            const auto& spatial = world.GetComponent<SpatialAgentComponent>(target);
            return spatial.kind == eSpatialKind::Sensor &&
                spatial.team == static_cast<u8_t>(casterTeam);
        }

        return false;
    }

    bool_t IsSafeguardTarget(CWorld& world, EntityID caster, EntityID target, eTeam casterTeam)
    {
        if (target == NULL_ENTITY ||
            target == caster ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        if (IsAlliedWardTarget(world, target, casterTeam))
            return true;

        if (world.HasComponent<ChampionComponent>(target))
            return world.GetComponent<ChampionComponent>(target).team == casterTeam;
        if (world.HasComponent<MinionComponent>(target))
            return world.GetComponent<MinionComponent>(target).team == casterTeam;
        if (world.HasComponent<MinionStateComponent>(target))
            return world.GetComponent<MinionStateComponent>(target).team == casterTeam;

        return false;
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || ctx.pCommand->itemId != 2u)
            return;

        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY ||
            !ctx.pWorld->HasComponent<LeeSinQMarkComponent>(target))
        {
            std::cout << "[LeeSinSim] Q2 rejected missing mark caster="
                << ctx.casterEntity << " target=" << target << "\n";
            return;
        }

        const LeeSinQMarkComponent& mark =
            ctx.pWorld->GetComponent<LeeSinQMarkComponent>(target);
        if (mark.sourceEntity != ctx.casterEntity || mark.fRemainingSec <= 0.f)
        {
            std::cout << "[LeeSinSim] Q2 rejected stale mark caster="
                << ctx.casterEntity << " target=" << target << "\n";
            return;
        }

        const f32_t q2Damage = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage);
        const f32_t qDashGap = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Gap);
        const f32_t qDashDurationSec = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::DashDurationSec);

        StartTargetDash(*ctx.pWorld, ctx.casterEntity, target, qDashGap, qDashDurationSec);
        EnqueuePhysicalDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            q2Damage,
            static_cast<u8_t>(eSkillSlot::Q),
            ctx.skillRank);
        ctx.pWorld->RemoveComponent<LeeSinQMarkComponent>(target);

        std::cout << "[LeeSinSim] Q2 resonating strike caster="
            << ctx.casterEntity << " target=" << target << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;
        if (ctx.pCommand->itemId == 2u)
            return;

        const EntityID target = ctx.pCommand->targetEntity;
        if (!IsSafeguardTarget(*ctx.pWorld, ctx.casterEntity, target, ctx.casterTeam))
        {
            std::cout << "[LeeSinSim] W rejected invalid safeguard target caster="
                << ctx.casterEntity << " target=" << target << "\n";
            return;
        }

        const f32_t wRange = GameplayDefinitionQuery::ResolveSkillRange(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::LEESIN,
            static_cast<u8_t>(eSkillSlot::W));
        const f32_t effectiveRange =
            wRange +
            GameplayStateQuery::ResolveGameplayRadius(*ctx.pWorld, ctx.casterEntity) +
            GameplayStateQuery::ResolveGameplayRadius(*ctx.pWorld, target);
        if (ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity) &&
            ctx.pWorld->HasComponent<TransformComponent>(target) &&
            WintersMath::DistanceSqXZ(
                ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition(),
                ctx.pWorld->GetComponent<TransformComponent>(target).GetPosition()) >
                effectiveRange * effectiveRange)
        {
            std::cout << "[LeeSinSim] W rejected range caster="
                << ctx.casterEntity << " target=" << target << "\n";
            return;
        }

        const f32_t dashGap = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Gap);
        const f32_t dashDuration = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::DashDurationSec);
        StartTargetDash(*ctx.pWorld, ctx.casterEntity, target, dashGap, dashDuration);

        std::cout << "[LeeSinSim] W safeguard caster="
            << ctx.casterEntity << " target=" << target << "\n";
    }

    void ApplyTempestCrippleSlow(CWorld& world, const TickContext& tc, EntityID caster, eTeam casterTeam)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        const f32_t radius = ResolveLeeSinSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::E,
            eSkillEffectParamId::Radius);
        const f32_t slowDurationSec = ResolveLeeSinSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::E,
            eSkillEffectParamId::SlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveLeeSinSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::E,
            eSkillEffectParamId::MoveSpeedMul);
        const f32_t radiusSq = radius * radius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == caster || champion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > radiusSq)
                        return;

                    GameplayStatus::ApplySlow(
                        world,
                        tc,
                        target,
                        caster,
                        eChampion::LEESIN,
                        eSkillSlot::E,
                        slowDurationSec,
                        slowMoveSpeedMul);
                }));
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        ApplyTempestCrippleSlow(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterTeam);

        std::cout << "[LeeSinSim] E slow caster="
            << ctx.casterEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY || !ctx.pWorld->IsAlive(target))
            return;

        const f32_t rDamage = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage);
        const f32_t rAirborneDurationSec = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::AirborneDurationSec);

        EnqueuePhysicalDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            rDamage,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank);
        GameplayStatus::ApplyAirborne(
            *ctx.pWorld,
            *ctx.pTickCtx,
            target,
            ctx.casterEntity,
            eChampion::LEESIN,
            eSkillSlot::R,
            rAirborneDurationSec);

        std::cout << "[LeeSinSim] R dragon rage caster="
            << ctx.casterEntity << " target=" << target << "\n";
    }
}

namespace LeeSinGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::LEESIN, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[LeeSinSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<LeeSinDashComponent, TransformComponent>(
            std::function<void(EntityID, LeeSinDashComponent&, TransformComponent&)>(
                [&](EntityID entity, LeeSinDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

                    dash.fElapsedSec += tc.fDt;
                    f32_t t = dash.fDurationSec > 0.01f
                        ? dash.fElapsedSec / dash.fDurationSec
                        : 1.f;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedDashes.push_back(entity);
                    }

                    const Vec3 position{
                        dash.vStart.x + (dash.vEnd.x - dash.vStart.x) * t,
                        dash.vStart.y + (dash.vEnd.y - dash.vStart.y) * t,
                        dash.vStart.z + (dash.vEnd.z - dash.vStart.z) * t
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
            if (world.HasComponent<LeeSinDashComponent>(entity))
                SnapDashArrivalToWalkable(world, tc, entity,
                    world.GetComponent<LeeSinDashComponent>(entity).vStart);
            world.RemoveComponent<LeeSinDashComponent>(entity);
        }

        std::vector<EntityID> expiredMarks;
        world.ForEach<LeeSinQMarkComponent>(
            std::function<void(EntityID, LeeSinQMarkComponent&)>(
                [&](EntityID entity, LeeSinQMarkComponent& mark)
                {
                    mark.fRemainingSec = std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f || !world.IsAlive(mark.sourceEntity))
                        expiredMarks.push_back(entity);
                }));

        for (EntityID entity : expiredMarks)
            world.RemoveComponent<LeeSinQMarkComponent>(entity);
    }

    void ApplySonicWaveMark(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
    {
        if (source == NULL_ENTITY || target == NULL_ENTITY || !world.IsAlive(target))
            return;

        const f32_t markDurationSec = ResolveLeeSinSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::Q,
            eSkillEffectParamId::MarkDurationSec);

        LeeSinQMarkComponent mark{};
        mark.sourceEntity = source;
        mark.fRemainingSec = markDurationSec;

        if (world.HasComponent<LeeSinQMarkComponent>(target))
            world.GetComponent<LeeSinQMarkComponent>(target) = mark;
        else
            world.AddComponent<LeeSinQMarkComponent>(target, mark);

        std::cout << "[LeeSinSim] Q1 sonic wave mark source="
            << source << " target=" << target << "\n";
    }
}
