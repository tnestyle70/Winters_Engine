#include "Shared/GameSim/Champions/LeeSin/LeeSinGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Definitions/WardDefinitions.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/Shield/ShieldSystem.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/SpatialAgentComponent.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/Ecs/VisionComponents.h"
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
        request.iSourceSlot = slot;
        request.eSourceKind = eDamageSourceKind::Skill;
        EnqueueDamageRequest(world, request);
    }

    void StartTargetDash(
        CWorld& world,
        EntityID caster,
        EntityID target,
        f32_t gap,
        f32_t durationSec,
        bool_t bIgnoreTerrainDuringTransit = false)
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
        dash.bIgnoreTerrainDuringTransit = bIgnoreTerrainDuringTransit;

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

        const f32_t q2Damage = GameplayDefinitionQuery::ResolveSkillEffectParamRanked(
            *ctx.pWorld, ctx.casterEntity, *ctx.pTickCtx, eChampion::LEESIN,
            static_cast<u8_t>(eSkillSlot::Q), ctx.skillRank,
            eSkillEffectParamId::BaseDamage, 95.f);
        const f32_t qDashGap = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Gap);
        const f32_t qDashDurationSec = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::DashDurationSec);

        StartTargetDash(
            *ctx.pWorld,
            ctx.casterEntity,
            target,
            qDashGap,
            qDashDurationSec,
            true);
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
        if (!LeeSinGameSim::CanCastSafeguard(
                *ctx.pWorld,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target))
        {
            std::cout << "[LeeSinSim] W rejected invalid safeguard target caster="
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
        const f32_t shieldAmount = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::ShieldBaseAmount,
            80.f);
        const f32_t shieldDurationSec = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::ShieldDurationSec,
            3.f);
        CShieldSystem::Grant(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            shieldAmount,
            shieldDurationSec);
        StartTargetDash(*ctx.pWorld, ctx.casterEntity, target, dashGap, dashDuration);

        std::cout << "[LeeSinSim] W safeguard caster="
            << ctx.casterEntity << " target=" << target
            << " shield=" << shieldAmount << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const TickContext& tc = *ctx.pTickCtx;
        if (ctx.pCommand->itemId != 2u)
        {
            if (!world.HasComponent<TransformComponent>(ctx.casterEntity))
                return;

            const Vec3 origin =
                world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            const f32_t radius = ResolveLeeSinSkillEffectParam(
                ctx,
                eSkillSlot::E,
                eSkillEffectParamId::Radius);
            const f32_t resolvedStageWindowSec =
                GameplayDefinitionQuery::ResolveSkillStageWindowSec(
                    world,
                    ctx.casterEntity,
                    tc,
                    eChampion::LEESIN,
                    static_cast<u8_t>(eSkillSlot::E));
            const f32_t stageWindowSec =
                resolvedStageWindowSec > 0.f ? resolvedStageWindowSec : 3.f;
            const std::vector<EntityID> targets =
                GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                    world,
                    ctx.casterEntity,
                    origin,
                    radius);
            for (EntityID target : targets)
            {
                EnqueuePhysicalDamage(
                    world,
                    ctx.casterEntity,
                    target,
                    ctx.casterTeam,
                    0.f,
                    static_cast<u8_t>(eSkillSlot::E),
                    ctx.skillRank);
                LeeSinTempestMarkComponent mark{};
                mark.sourceEntity = ctx.casterEntity;
                mark.fRemainingSec = stageWindowSec;
                if (world.HasComponent<LeeSinTempestMarkComponent>(target))
                    world.GetComponent<LeeSinTempestMarkComponent>(target) = mark;
                else
                    world.AddComponent<LeeSinTempestMarkComponent>(target, mark);
            }

            std::cout << "[LeeSinSim] E1 tempest caster="
                << ctx.casterEntity << " targets=" << targets.size() << "\n";
            return;
        }

        const f32_t slowDurationSec = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::SlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::MoveSpeedMul);
        const std::vector<EntityID> markedTargets =
            DeterministicEntityIterator<LeeSinTempestMarkComponent>::CollectSorted(world);
        u32_t slowedCount = 0u;
        for (EntityID target : markedTargets)
        {
            const LeeSinTempestMarkComponent& mark =
                world.GetComponent<LeeSinTempestMarkComponent>(target);
            if (mark.sourceEntity != ctx.casterEntity ||
                mark.fRemainingSec <= 0.f ||
                !GameplayStateQuery::CanReceiveCrowdControl(
                    world,
                    ctx.casterEntity,
                    target))
            {
                continue;
            }

            GameplayStatus::ApplySlow(
                world,
                tc,
                target,
                ctx.casterEntity,
                eChampion::LEESIN,
                eSkillSlot::E,
                slowDurationSec,
                slowMoveSpeedMul);
            world.RemoveComponent<LeeSinTempestMarkComponent>(target);
            ++slowedCount;
        }

        std::cout << "[LeeSinSim] E2 cripple caster="
            << ctx.casterEntity << " targets=" << slowedCount << "\n";
    }

    Vec3 ResolveDragonRageLanding(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        const Vec3 casterPos =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos =
            world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 direction = WintersMath::NormalizeXZ(
            Vec3{
                targetPos.x - casterPos.x,
                0.f,
                targetPos.z - casterPos.z },
            Vec3{ 0.f, 0.f, 1.f });
        const Vec3 desired{
            targetPos.x + direction.x * 10.f,
            targetPos.y,
            targetPos.z + direction.z * 10.f };
        if (!tc.pWalkable)
            return desired;

        Vec3 landing = targetPos;
        const f32_t radius =
            GameplayStateQuery::ResolveGameplayRadius(world, target);
        if (!tc.pWalkable->TryClampMoveSegmentXZ(
                targetPos,
                desired,
                radius,
                landing))
        {
            landing = targetPos;
        }
        if (!tc.pWalkable->IsWalkableXZ(landing))
        {
            Vec3 resolved = landing;
            if (tc.pWalkable->TryResolveMoveTarget(
                    targetPos,
                    landing,
                    resolved))
            {
                landing = resolved;
            }
        }
        f32_t surfaceY = landing.y;
        if (tc.pWalkable->TrySampleHeight(landing.x, landing.z, surfaceY))
            landing.y = surfaceY;
        return landing;
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (!LeeSinGameSim::CanCastDragonRage(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target))
        {
            return;
        }

        const f32_t rDamage = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage);
        const f32_t rAirborneDurationSec = ResolveLeeSinSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::AirborneDurationSec);

        EnqueuePhysicalDamage(
            world,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            rDamage,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank);
        const Vec3 landing = ResolveDragonRageLanding(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target);
        GameplayStatus::ApplyAirborne(
            world,
            *ctx.pTickCtx,
            target,
            ctx.casterEntity,
            eChampion::LEESIN,
            eSkillSlot::R,
            rAirborneDurationSec,
            2.1f,
            &landing);

        std::cout << "[LeeSinSim] R dragon rage caster="
            << ctx.casterEntity << " target=" << target << "\n";
    }
}

namespace LeeSinGameSim
{
    bool_t CanCastSafeguard(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (caster == NULL_ENTITY ||
            !world.IsAlive(caster) ||
            !world.HasComponent<ChampionComponent>(caster) ||
            !world.HasComponent<TransformComponent>(caster))
        {
            return false;
        }

        const eTeam casterTeam =
            world.GetComponent<ChampionComponent>(caster).team;
        if (!IsSafeguardTarget(world, caster, target, casterTeam))
            return false;

        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::LEESIN,
            static_cast<u8_t>(eSkillSlot::W));
        if (range <= 0.f)
            return false;

        const f32_t effectiveRange =
            range +
            GameplayStateQuery::ResolveGameplayRadius(world, caster) +
            GameplayStateQuery::ResolveGameplayRadius(world, target);
        const Vec3 casterPosition =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        return WintersMath::DistanceSqXZ(casterPosition, targetPosition) <=
            effectiveRange * effectiveRange;
    }

    bool_t CanCastDragonRage(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (caster == NULL_ENTITY ||
            target == NULL_ENTITY ||
            !world.IsAlive(caster) ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target) ||
            !GameplayStateQuery::CanReceiveCrowdControl(world, caster, target))
        {
            return false;
        }

        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::LEESIN,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            return false;

        const f32_t effectiveRange =
            range +
            GameplayStateQuery::ResolveGameplayRadius(world, caster) +
            GameplayStateQuery::ResolveGameplayRadius(world, target);
        const Vec3 casterPosition =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        if (WintersMath::DistanceSqXZ(casterPosition, targetPosition) >
            effectiveRange * effectiveRange)
        {
            return false;
        }
        return !tc.pWalkable ||
            tc.pWalkable->SegmentWalkableXZ(casterPosition, targetPosition, 0.f);
    }

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
                    if (!GameplayStateQuery::CanMove(world, entity) ||
                        world.HasComponent<ForcedMotionComponent>(entity))
                    {
                        finishedDashes.push_back(entity);
                        return;
                    }

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
                    if (tc.pWalkable && !dash.bIgnoreTerrainDuringTransit)
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
            if (world.HasComponent<ForcedMotionComponent>(entity))
            {
                world.RemoveComponent<LeeSinDashComponent>(entity);
                continue;
            }
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

        std::vector<EntityID> expiredTempestMarks;
        world.ForEach<LeeSinTempestMarkComponent>(
            std::function<void(EntityID, LeeSinTempestMarkComponent&)>(
                [&](EntityID entity, LeeSinTempestMarkComponent& mark)
                {
                    mark.fRemainingSec =
                        std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f ||
                        !world.IsAlive(mark.sourceEntity))
                    {
                        expiredTempestMarks.push_back(entity);
                    }
                }));

        for (EntityID entity : expiredTempestMarks)
            world.RemoveComponent<LeeSinTempestMarkComponent>(entity);
    }

    EntityID ResolveSonicWaveMarkTarget(CWorld& world, EntityID caster)
    {
        EntityID resolved = NULL_ENTITY;
        const std::vector<EntityID> targets =
            DeterministicEntityIterator<LeeSinQMarkComponent>::CollectSorted(world);
        for (EntityID target : targets)
        {
            const LeeSinQMarkComponent& mark =
                world.GetComponent<LeeSinQMarkComponent>(target);
            if (mark.sourceEntity != caster ||
                mark.fRemainingSec <= 0.f ||
                !world.IsAlive(target) ||
                !GameplayStateQuery::CanBeTargetedBy(world, caster, target))
            {
                continue;
            }
            if (resolved != NULL_ENTITY)
                return NULL_ENTITY;
            resolved = target;
        }
        return resolved;
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

        if (world.HasComponent<SkillStateComponent>(source))
        {
            auto& qSlot = world.GetComponent<SkillStateComponent>(source)
                .slots[static_cast<u8_t>(eSkillSlot::Q)];
            qSlot.currentStage = 1u;
            qSlot.stageWindow = markDurationSec;
        }

        std::cout << "[LeeSinSim] Q1 sonic wave mark source="
            << source << " target=" << target << "\n";
    }
}
