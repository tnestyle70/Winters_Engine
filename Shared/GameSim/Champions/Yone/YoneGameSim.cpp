#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillRankComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/Move/DashArrival.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"

namespace
{
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
        f32_t durationSec = 0.f;
        eYoneDashKind kind = eYoneDashKind::SoulOut;
        bool_t bUltimateImpactPending = false;
        f32_t impactHalfWidth = 0.f;
        f32_t impactDamage = 0.f;
        f32_t impactAirborneDurationSec = 0.f;
        f32_t impactGatherBehindDistance = 0.f;
        Vec3 impactDirection{ 0.f, 0.f, 1.f };
        u8_t impactRank = 1u;
    };

    // Chrono Break: 익명 네임스페이스 컴포넌트는 소유 TU에서 자기등록한다.
    const bool_t s_bYoneDashKeyframeRegistered = []()
    {
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<YoneDashComponent>("YoneDashComponent");
        return true;
    }();

    constexpr f32_t kYoneUltimateAirborneArcHeight = 2.1f;
    constexpr f32_t kYoneSoulEchoRatioByRank[5] =
    {
        0.25f,
        0.275f,
        0.30f,
        0.325f,
        0.35f,
    };

    YoneSimComponent& EnsureYoneState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<YoneSimComponent>(caster))
            world.AddComponent<YoneSimComponent>(caster, YoneSimComponent{});

        return world.GetComponent<YoneSimComponent>(caster);
    }

    f32_t ResolveYoneSkillRange(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return GameplayDefinitionQuery::ResolveSkillRange(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::YONE,
            static_cast<u8_t>(slot));
    }

    f32_t ResolveYoneSkillEffectParam(
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
            eChampion::YONE,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveYoneSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return ResolveYoneSkillEffectParam(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            param,
            fallbackValue);
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

    Vec3 ResolveNavSafeEnd(
        const TickContext& tc,
        const Vec3& start,
        const Vec3& desiredEnd)
    {
        if (!tc.pWalkable)
            return desiredEnd;

        Vec3 guardedEnd = desiredEnd;
        if (!tc.pWalkable->TryClampMoveSegmentXZ(
                start,
                desiredEnd,
                0.5f,
                guardedEnd))
        {
            return start;
        }

        return guardedEnd;
    }

    void StartYoneEAction(CWorld& world, EntityID entity,
        const TickContext& tc, u8_t stage)
    {
        bool_t bHasQueuedMove = false;
        u32_t queuedMoveSequence = 0u;
        Vec3 queuedMoveTarget{};
        Vec3 queuedMoveDirection{};
        if (world.HasComponent<ActionStateComponent>(entity))
        {
            const auto& previous = world.GetComponent<ActionStateComponent>(entity);
            bHasQueuedMove = previous.bHasQueuedMove;
            queuedMoveSequence = previous.queuedMoveSequence;
            queuedMoveTarget = previous.queuedMoveTarget;
            queuedMoveDirection = previous.queuedMoveDirection;
        }

        auto& action =
            StartActionState(world, entity, eActionStateId::SkillE, tc.tickIndex, stage);
        action.commandSequence = 0u;
        action.sourceChampion = eChampion::YONE;
        action.sourceSlot = static_cast<u8_t>(eSkillSlot::E);
        action.movePolicy = eSkillActionMovePolicy::ForcedMotion;
        action.lockEndTick = tc.tickIndex +
            GameplayDefinitionQuery::ResolveSkillActionLockTicks(
                world,
                entity,
                tc,
                eChampion::YONE,
                static_cast<u8_t>(eSkillSlot::E),
                stage);
        action.bHasQueuedMove = bHasQueuedMove;
        action.queuedMoveSequence = queuedMoveSequence;
        action.queuedMoveTarget = queuedMoveTarget;
        action.queuedMoveDirection = queuedMoveDirection;
    }

    void EmitYoneEVisualEvent(
        CWorld& world,
        EntityID caster,
        const TickContext& tc,
        u8_t stage,
        EntityID target = NULL_ENTITY,
        f32_t durationSec = 0.7f)
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
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::YONE, GameplayHookVariant::E_CastFrame);
        event.slot = static_cast<u8_t>(eSkillSlot::E);
        event.rank = 1;
        event.flags = static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (1u << 8) |
            static_cast<u16_t>(eSkillSlot::E));
        event.position = position;
        event.direction = direction;
        event.durationMs = static_cast<u16_t>(std::clamp(
            durationSec * 1000.f,
            0.f,
            65535.f));
        event.startTick = tc.tickIndex;
        EnqueueReplicatedEvent(world, event);
    }

    f32_t ResolveYoneSoulEchoRatio(u8_t rank)
    {
        const u8_t clampedRank = std::clamp<u8_t>(rank, 1u, 5u);
        return kYoneSoulEchoRatioByRank[clampedRank - 1u];
    }

    void ClearYoneSoulMarks(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        bool_t bDetonate)
    {
        const auto targets =
            DeterministicEntityIterator<YoneSoulMarkComponent>::CollectSorted(world);
        const eTeam casterTeam =
            GameplayStateQuery::ResolveEntityTeam(world, caster);
        for (EntityID target : targets)
        {
            if (!world.IsAlive(target) ||
                !world.HasComponent<YoneSoulMarkComponent>(target))
            {
                continue;
            }

            const YoneSoulMarkComponent mark =
                world.GetComponent<YoneSoulMarkComponent>(target);
            if (mark.sourceEntity != caster)
                continue;

            if (bDetonate &&
                mark.storedPostMitigationDamage > 0.f &&
                GameplayStateQuery::CanReceiveDamage(world, caster, target))
            {
                DamageRequest echo{};
                echo.source = caster;
                echo.target = target;
                echo.sourceTeam = casterTeam;
                echo.type = eDamageType::True;
                echo.flatAmount = mark.storedPostMitigationDamage *
                    ResolveYoneSoulEchoRatio(mark.sourceERank);
                echo.skillId = static_cast<u16_t>(
                    (static_cast<u32_t>(eChampion::YONE) << 8u) |
                    static_cast<u8_t>(eSkillSlot::E));
                echo.rank = mark.sourceERank;
                echo.iSourceSlot = static_cast<u8_t>(eSkillSlot::E);
                echo.eSourceKind = eDamageSourceKind::Skill;
                echo.flags = DamageFlag_YoneSoulEcho;
                EnqueueDamageRequest(world, echo);
            }

            EmitYoneEVisualEvent(world, caster, tc, 4u, target, 0.f);
            world.RemoveComponent<YoneSoulMarkComponent>(target);
        }
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

        if (!pTickCtx)
            return;

        ClearYoneSoulMarks(world, *pTickCtx, caster, true);

        const f32_t dashDurationSec = ResolveYoneSkillEffectParam(
            world,
            *pTickCtx,
            caster,
            eSkillSlot::E,
            eSkillEffectParamId::DashDurationSec);

        // Soul return replaces any active knock-up/gather trajectory. Leaving
        // ForcedMotionComponent alive would let StatusEffectSystem overwrite
        // the anchor snap on the following tick.
        if (world.HasComponent<ForcedMotionComponent>(caster))
            world.RemoveComponent<ForcedMotionComponent>(caster);

        state.bReturning = true;
        StartDash(world, caster, state.anchorPosition, dashDurationSec, eYoneDashKind::SoulReturn);
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

    void EnqueueDamageForTargets(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const std::vector<EntityID>& targets,
        f32_t damage,
        u16_t skillId,
        u8_t rank)
    {
        for (EntityID target : targets)
        {
            DamageRequest request{};
            request.source = caster;
            request.target = target;
            request.sourceTeam = casterTeam;
            request.type = eDamageType::Physical;
            request.flatAmount = damage;
            request.skillId = skillId;
            request.rank = rank;
            request.iSourceSlot = static_cast<u8_t>(skillId & 0xffu);
            request.eSourceKind = eDamageSourceKind::Skill;
            request.flags = DamageFlag_OnHit;
            EnqueueDamageRequest(world, request);
        }
    }

    void EnqueueLineDamage(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& start,
        const Vec3& end,
        f32_t halfWidth,
        f32_t damage,
        u16_t skillId,
        u8_t rank)
    {
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInSegment(
                world,
                caster,
                start,
                end,
                halfWidth);
        EnqueueDamageForTargets(
            world,
            caster,
            casterTeam,
            targets,
            damage,
            skillId,
            rank);
    }

    Vec3 ResolveYoneUltimateGatherPoint(
        const TickContext& tc,
        const YoneDashComponent& dash)
    {
        Vec3 rawDirection{
            dash.end.x - dash.start.x,
            0.f,
            dash.end.z - dash.start.z
        };
        if (rawDirection.x * rawDirection.x + rawDirection.z * rawDirection.z <= 0.0001f)
            rawDirection = dash.impactDirection;
        const Vec3 direction = WintersMath::NormalizeXZ(rawDirection);
        const Vec3 desiredGatherPoint{
            dash.end.x - direction.x * dash.impactGatherBehindDistance,
            dash.end.y,
            dash.end.z - direction.z * dash.impactGatherBehindDistance
        };
        return ResolveNavSafeEnd(tc, dash.end, desiredGatherPoint);
    }

    void ApplyYoneUltimateImpact(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        YoneDashComponent& dash)
    {
        if (!dash.bUltimateImpactPending)
            return;

        dash.bUltimateImpactPending = false;
        if (caster == NULL_ENTITY || !world.IsAlive(caster))
            return;

        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInSegment(
                world,
                caster,
                dash.start,
                dash.end,
                dash.impactHalfWidth);
        const eTeam casterTeam = GameplayStateQuery::ResolveEntityTeam(world, caster);
        EnqueueDamageForTargets(
            world,
            caster,
            casterTeam,
            targets,
            dash.impactDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YONE) << 8) | 4u),
            dash.impactRank);

        const Vec3 gatherPoint = ResolveYoneUltimateGatherPoint(tc, dash);
        for (EntityID target : targets)
        {
            GameplayStatus::ApplyAirborne(
                world,
                tc,
                target,
                caster,
                eChampion::YONE,
                eSkillSlot::R,
                dash.impactAirborneDurationSec,
                kYoneUltimateAirborneArcHeight,
                &gatherPoint);
        }
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 direction = ResolveDirection(ctx, origin);
        const f32_t range = ResolveYoneSkillRange(ctx, eSkillSlot::Q);
        const f32_t radius = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Radius);
        const f32_t damage = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage);
        const Vec3 end{ origin.x + direction.x * range, origin.y, origin.z + direction.z * range };
        RotateToward(world, ctx.casterEntity, direction);

        EnqueueLineDamage(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            end,
            radius,
            damage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YONE) << 8) | 1u),
            ctx.skillRank);

        std::cout << "[YoneSim] Q accepted caster=" << ctx.casterEntity << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 direction = ResolveDirection(ctx, origin);
        const f32_t range = ResolveYoneSkillRange(ctx, eSkillSlot::W);
        const f32_t radius = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Radius);
        const f32_t damage = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::BaseDamage);
        const Vec3 end{ origin.x + direction.x * range, origin.y, origin.z + direction.z * range };
        RotateToward(world, ctx.casterEntity, direction);

        EnqueueLineDamage(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            end,
            radius,
            damage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YONE) << 8) | 2u),
            ctx.skillRank);

        std::cout << "[YoneSim] W accepted caster=" << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
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
        const f32_t dashDistance = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::DashDistance);
        const f32_t dashDurationSec = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::DashDurationSec);
        const f32_t soulDurationSec = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::EffectDurationSec);
        const Vec3 end{
            origin.x + direction.x * dashDistance,
            origin.y,
            origin.z + direction.z * dashDistance
        };

        state.bSoulUnboundActive = true;
        state.bReturning = false;
        state.sourceERank = std::max<u8_t>(1u, ctx.skillRank);
        state.soulDurationSec = soulDurationSec;
        state.soulTimerSec = state.soulDurationSec;
        state.anchorPosition = origin;

        StartDash(world, ctx.casterEntity, end, dashDurationSec, eYoneDashKind::SoulOut);
        RotateToward(world, ctx.casterEntity, direction);

        std::cout << "[YoneSim] E out caster=" << ctx.casterEntity
            << " anchor=(" << origin.x << "," << origin.z << ")"
            << " end=(" << end.x << "," << end.z << ")\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 direction = ResolveDirection(ctx, origin);
        const f32_t range = ResolveYoneSkillRange(ctx, eSkillSlot::R);
        const f32_t radius = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Radius);
        const f32_t dashDelaySec = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::DashDelaySec);
        const f32_t dashDurationSec = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::DashDurationSec);
        const f32_t damage = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage);
        const f32_t airborneDurationSec = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::AirborneDurationSec);
        const f32_t gatherBehindDistance = ResolveYoneSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Gap,
            0.75f);
        const Vec3 desiredEnd{
            origin.x + direction.x * range,
            origin.y,
            origin.z + direction.z * range
        };
        const Vec3 end = ResolveNavSafeEnd(*ctx.pTickCtx, origin, desiredEnd);
        RotateToward(world, ctx.casterEntity, direction);
        StartDash(world, ctx.casterEntity, end, dashDurationSec,
            eYoneDashKind::Ultimate, dashDelaySec);
        if (world.HasComponent<YoneDashComponent>(ctx.casterEntity))
        {
            auto& dash = world.GetComponent<YoneDashComponent>(ctx.casterEntity);
            dash.bUltimateImpactPending = true;
            dash.impactHalfWidth = radius;
            dash.impactDamage = damage;
            dash.impactAirborneDurationSec = airborneDurationSec;
            dash.impactGatherBehindDistance = gatherBehindDistance;
            dash.impactDirection = direction;
            dash.impactRank = ctx.skillRank;
        }

        std::cout << "[YoneSim] R accepted caster=" << ctx.casterEntity << "\n";
    }
}

namespace YoneGameSim
{
    void CancelRuntime(
        CWorld& world,
        EntityID caster,
        const TickContext* pTickCtx)
    {
        if (pTickCtx)
        {
            ClearYoneSoulMarks(world, *pTickCtx, caster, false);
        }
        else
        {
            const auto markedTargets =
                DeterministicEntityIterator<YoneSoulMarkComponent>::CollectSorted(world);
            for (EntityID target : markedTargets)
            {
                if (world.IsAlive(target) &&
                    world.HasComponent<YoneSoulMarkComponent>(target) &&
                    world.GetComponent<YoneSoulMarkComponent>(target).sourceEntity == caster)
                {
                    world.RemoveComponent<YoneSoulMarkComponent>(target);
                }
            }
        }
        if (world.HasComponent<YoneDashComponent>(caster))
            world.RemoveComponent<YoneDashComponent>(caster);
        if (world.HasComponent<YoneSimComponent>(caster))
            world.RemoveComponent<YoneSimComponent>(caster);
    }

    u8_t ResolveEStage(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<YoneSimComponent>(caster) &&
            world.GetComponent<YoneSimComponent>(caster).bSoulUnboundActive)
        {
            return 2;
        }

        return 1;
    }

    void OnDamageResolved(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        const DamageResult& result)
    {
        if (result.finalAmount <= 0.f ||
            request.source == NULL_ENTITY ||
            request.target == NULL_ENTITY ||
            (request.flags & DamageFlag_YoneSoulEcho) != 0u ||
            (request.eSourceKind != eDamageSourceKind::BasicAttack &&
                request.eSourceKind != eDamageSourceKind::Skill) ||
            (request.type != eDamageType::Physical &&
                request.type != eDamageType::Magic) ||
            !world.IsAlive(request.source) ||
            !world.IsAlive(request.target) ||
            !world.HasComponent<ChampionComponent>(request.source) ||
            !world.HasComponent<ChampionComponent>(request.target) ||
            !world.HasComponent<YoneSimComponent>(request.source))
        {
            return;
        }

        const ChampionComponent& sourceChampion =
            world.GetComponent<ChampionComponent>(request.source);
        const ChampionComponent& targetChampion =
            world.GetComponent<ChampionComponent>(request.target);
        const YoneSimComponent& state =
            world.GetComponent<YoneSimComponent>(request.source);
        if (sourceChampion.id != eChampion::YONE ||
            sourceChampion.team == targetChampion.team ||
            !state.bSoulUnboundActive ||
            state.bReturning)
        {
            return;
        }

        const bool_t bExistingMark =
            world.HasComponent<YoneSoulMarkComponent>(request.target) &&
            world.GetComponent<YoneSoulMarkComponent>(request.target)
                    .sourceEntity == request.source;
        YoneSoulMarkComponent mark = bExistingMark
            ? world.GetComponent<YoneSoulMarkComponent>(request.target)
            : YoneSoulMarkComponent{};
        mark.sourceEntity = request.source;
        mark.storedPostMitigationDamage += result.finalAmount;
        mark.remainingSec = state.soulTimerSec;
        mark.sourceERank = state.sourceERank;

        if (world.HasComponent<YoneSoulMarkComponent>(request.target))
            world.GetComponent<YoneSoulMarkComponent>(request.target) = mark;
        else
            world.AddComponent<YoneSoulMarkComponent>(request.target, mark);

        if (!bExistingMark)
        {
            EmitYoneEVisualEvent(
                world,
                request.source,
                tc,
                3u,
                request.target,
                mark.remainingSec);
        }

        if (result.bKilled)
        {
            EmitYoneEVisualEvent(
                world,
                request.source,
                tc,
                4u,
                request.target,
                0.f);
            world.RemoveComponent<YoneSoulMarkComponent>(request.target);
        }
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
        const auto yoneEntities =
            DeterministicEntityIterator<YoneSimComponent>::CollectSorted(world);
        for (EntityID entity : yoneEntities)
        {
            if (!world.IsAlive(entity) ||
                !world.HasComponent<YoneSimComponent>(entity))
            {
                continue;
            }

            const bool_t bDead =
                world.HasComponent<HealthComponent>(entity) &&
                (world.GetComponent<HealthComponent>(entity).bIsDead ||
                    world.GetComponent<HealthComponent>(entity).fCurrent <= 0.f);
            if (!bDead)
                continue;

            ClearYoneSoulMarks(world, tc, entity, false);
            if (world.HasComponent<YoneDashComponent>(entity))
                world.RemoveComponent<YoneDashComponent>(entity);
            world.GetComponent<YoneSimComponent>(entity) = YoneSimComponent{};
        }

        const auto markedTargets =
            DeterministicEntityIterator<YoneSoulMarkComponent>::CollectSorted(world);
        for (EntityID target : markedTargets)
        {
            if (!world.IsAlive(target) ||
                !world.HasComponent<YoneSoulMarkComponent>(target))
            {
                continue;
            }

            const YoneSoulMarkComponent mark =
                world.GetComponent<YoneSoulMarkComponent>(target);
            const bool_t bTargetDead =
                world.HasComponent<HealthComponent>(target) &&
                (world.GetComponent<HealthComponent>(target).bIsDead ||
                    world.GetComponent<HealthComponent>(target).fCurrent <= 0.f);
            const bool_t bSourceInactive =
                mark.sourceEntity == NULL_ENTITY ||
                !world.IsAlive(mark.sourceEntity) ||
                !world.HasComponent<YoneSimComponent>(mark.sourceEntity) ||
                !world.GetComponent<YoneSimComponent>(mark.sourceEntity)
                    .bSoulUnboundActive;
            if (!bTargetDead && !bSourceInactive)
                continue;

            if (mark.sourceEntity != NULL_ENTITY &&
                world.IsAlive(mark.sourceEntity))
            {
                EmitYoneEVisualEvent(
                    world,
                    mark.sourceEntity,
                    tc,
                    4u,
                    target,
                    0.f);
            }
            world.RemoveComponent<YoneSoulMarkComponent>(target);
        }

        std::vector<EntityID> finishedDashes;
        world.ForEach<YoneDashComponent, TransformComponent>(
            std::function<void(EntityID, YoneDashComponent&, TransformComponent&)>(
                [&](EntityID entity, YoneDashComponent& dash, TransformComponent& transform)
                {
                    const bool_t bSoulReturn =
                        dash.kind == eYoneDashKind::SoulReturn;
                    if (!bSoulReturn &&
                        (!GameplayStateQuery::CanMove(world, entity) ||
                            world.HasComponent<ForcedMotionComponent>(entity)))
                    {
                        finishedDashes.push_back(entity);
                        return;
                    }

                    ClearMove(world, entity);

                    if (dash.delaySec > 0.f)
                    {
                        dash.delaySec = std::max(0.f, dash.delaySec - tc.fDt);
                        if (dash.delaySec > 0.f)
                            return;

                        dash.start = transform.GetPosition();
                        dash.elapsedSec = 0.f;
                    }

                    ApplyYoneUltimateImpact(world, tc, entity, dash);

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
            const bool_t bFinishedSoulReturn =
                world.HasComponent<YoneDashComponent>(entity) &&
                world.GetComponent<YoneDashComponent>(entity).kind ==
                    eYoneDashKind::SoulReturn;
            if (world.HasComponent<ForcedMotionComponent>(entity) &&
                !bFinishedSoulReturn)
            {
                world.RemoveComponent<YoneDashComponent>(entity);
                continue;
            }
            if (world.HasComponent<YoneDashComponent>(entity))
            {
                const auto dash = world.GetComponent<YoneDashComponent>(entity);
                if (dash.kind == eYoneDashKind::SoulReturn &&
                    world.HasComponent<YoneSimComponent>(entity) &&
                    world.HasComponent<TransformComponent>(entity))
                {
                    auto& state = world.GetComponent<YoneSimComponent>(entity);
                    auto& transform = world.GetComponent<TransformComponent>(entity);
                    const bool_t bAnchorCorrection =
                        WintersMath::DistanceSqXZ(
                            transform.GetPosition(),
                            state.anchorPosition) > 0.0001f;
                    transform.SetPosition(state.anchorPosition);
                    if (bAnchorCorrection)
                    {
                        PositionDiscontinuityComponent& discontinuity =
                            world.HasComponent<PositionDiscontinuityComponent>(entity)
                                ? world.GetComponent<PositionDiscontinuityComponent>(entity)
                                : world.AddComponent<PositionDiscontinuityComponent>(
                                    entity,
                                    PositionDiscontinuityComponent{});
                        discontinuity.uTick = tc.tickIndex;
                    }
                    state.bSoulUnboundActive = false;
                    state.bReturning = false;
                    state.soulTimerSec = 0.f;
                }
                else
                {
                    SnapDashArrivalToWalkable(world, tc, entity, dash.start);
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
