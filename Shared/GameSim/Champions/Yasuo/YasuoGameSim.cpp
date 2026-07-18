#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"

#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ProjectileBarrierComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/Move/DashArrival.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iostream>
#include <type_traits>
#include <vector>

#include "Shared/GameSim/Core/Checkpoint/KeyframeComponentRegistry.h"

namespace
{
    struct YasuoDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t durationSec = 0.f;
    };

    struct YasuoEqInputBufferComponent
    {
        EntityHandle hCaster = NULL_ENTITY_HANDLE;
        Vec3 vDirection{};
        Vec3 vGroundPos{};
        u64_t uIssuedAtTick = 0u;
        u64_t uRewindTicks = 0u;
        u32_t uCommandSequence = 0u;
        u32_t uSourceSessionId = 0u;
        u32_t uEActionSequence = 0u;
        EntityID uTargetEntity = NULL_ENTITY;
        u16_t uItemId = 0u;
        bool_t bPending = false;
        bool_t bExecuting = false;
        u8_t reservedTail[4]{};
    };

    struct YasuoSweepingBladeLockoutComponent
    {
        EntityHandle hSource = NULL_ENTITY_HANDLE;
        EntityHandle hTarget = NULL_ENTITY_HANDLE;
        u64_t uExpireTick = 0u;
    };

    static_assert(std::is_trivially_copyable_v<YasuoEqInputBufferComponent>);
    static_assert(sizeof(YasuoEqInputBufferComponent) == 72u);
    static_assert(std::is_trivially_copyable_v<YasuoSweepingBladeLockoutComponent>);

    // Chrono Break: 익명 네임스페이스 컴포넌트는 소유 TU에서 자기등록한다.
    const bool_t s_bYasuoDashKeyframeRegistered = []()
    {
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<YasuoDashComponent>("YasuoDashComponent");
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<ProjectileBarrierComponent>("ProjectileBarrierComponent");
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<YasuoEqInputBufferComponent>("YasuoEqInputBufferComponent");
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<YasuoSweepingBladeLockoutComponent>(
                "YasuoSweepingBladeLockoutComponent");
        return true;
    }();

    YasuoStateComponent& EnsureYasuoState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<YasuoStateComponent>(caster))
            world.AddComponent<YasuoStateComponent>(caster, YasuoStateComponent{});

        return world.GetComponent<YasuoStateComponent>(caster);
    }

    void ClearYasuoEqInputBuffer(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<YasuoEqInputBufferComponent>(caster))
            world.RemoveComponent<YasuoEqInputBufferComponent>(caster);
    }

    bool_t IsYasuoEAction(const ActionStateComponent& action)
    {
        return action.sourceChampion == eChampion::YASUO &&
            action.sourceSlot == static_cast<u8_t>(eSkillSlot::E) &&
            action.movePolicy == eSkillActionMovePolicy::ForcedMotion;
    }

    u64_t SecondsToTicksCeil(f32_t seconds)
    {
        if (!std::isfinite(seconds) || seconds <= 0.f)
            return 0u;

        return static_cast<u64_t>(std::ceil(
            static_cast<f64_t>(seconds) *
            static_cast<f64_t>(DeterministicTime::kTicksPerSecond)));
    }

    EntityID FindSweepingBladeLockoutRelation(
        CWorld& world,
        EntityID source,
        EntityID target)
    {
        if (source == NULL_ENTITY || target == NULL_ENTITY)
            return NULL_ENTITY;

        const EntityHandle hSource = world.GetEntityHandle(source);
        const EntityHandle hTarget = world.GetEntityHandle(target);
        if (!hSource.IsValid() || !hTarget.IsValid())
            return NULL_ENTITY;

        const auto relations =
            DeterministicEntityIterator<
                YasuoSweepingBladeLockoutComponent>::CollectSorted(world);
        for (EntityID relationEntity : relations)
        {
            const YasuoSweepingBladeLockoutComponent& lockout =
                world.GetComponent<YasuoSweepingBladeLockoutComponent>(relationEntity);
            if (lockout.hSource == hSource && lockout.hTarget == hTarget)
                return relationEntity;
        }

        return NULL_ENTITY;
    }

    f32_t ResolveYasuoSkillEffectParam(
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
            eChampion::YASUO,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    void AttachSweepingBladeLockout(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target)
    {
        const EntityHandle hSource = world.GetEntityHandle(source);
        const EntityHandle hTarget = world.GetEntityHandle(target);
        if (!hSource.IsValid() || !hTarget.IsValid())
            return;

        const f32_t durationSec = ResolveYasuoSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::E,
            eSkillEffectParamId::MarkDurationSec,
            10.f);
        const u64_t durationTicks = (std::max)(
            1ull,
            SecondsToTicksCeil(durationSec));

        const EntityID existing =
            FindSweepingBladeLockoutRelation(world, source, target);
        if (existing != NULL_ENTITY)
        {
            YasuoSweepingBladeLockoutComponent& lockout =
                world.GetComponent<YasuoSweepingBladeLockoutComponent>(existing);
            lockout.uExpireTick = tc.tickIndex + durationTicks;
            return;
        }

        const EntityHandle hRelation = world.CreateEntityHandle();
        if (!hRelation.IsValid())
            return;

        YasuoSweepingBladeLockoutComponent lockout{};
        lockout.hSource = hSource;
        lockout.hTarget = hTarget;
        lockout.uExpireTick = tc.tickIndex + durationTicks;
        world.AddComponent<YasuoSweepingBladeLockoutComponent>(
            hRelation.GetIndex(),
            lockout);
    }

    f32_t ResolveYasuoSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return ResolveYasuoSkillEffectParam(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            param,
            fallbackValue);
    }

    Vec3 ResolveCommandDirection(const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            const Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    Vec3 ResolveCasterPosition(CWorld& world, EntityID caster)
    {
        if (caster != NULL_ENTITY && world.HasComponent<TransformComponent>(caster))
            return world.GetComponent<TransformComponent>(caster).GetPosition();

        return Vec3{};
    }

    void EnqueueAreaDamage(CWorld& world, EntityID caster, eTeam casterTeam,
        const Vec3& origin, f32_t radius, f32_t damage, u16_t skillId, u8_t rank)
    {
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                world,
                caster,
                origin,
                radius);
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
            request.flags = DamageFlag_OnHit;
            EnqueueDamageRequest(world, request);
        }
    }

    void EnqueueTargetDamage(CWorld& world, EntityID caster, EntityID target,
        eTeam casterTeam, f32_t damage, u16_t skillId, u8_t rank)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return;

        DamageRequest request{};
        request.source = caster;
        request.target = target;
        request.sourceTeam = casterTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = damage;
        request.skillId = skillId;
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    bool_t IsAirborne(CWorld& world, EntityID target)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return false;

        return world.HasComponent<GameplayStateComponent>(target) &&
            (world.GetComponent<GameplayStateComponent>(target).stateFlags &
                kGameplayStateAirborneFlag) != 0u;
    }

    bool_t StartDashThroughTarget(
        CWorld& world,
        EntityID caster,
        EntityID target,
        f32_t dashThroughDistance,
        f32_t dashMaxDistance,
        f32_t dashDurationSec)
    {
        if (caster == NULL_ENTITY ||
            target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        auto& casterTransform = world.GetComponent<TransformComponent>(caster);
        const Vec3 casterPos = casterTransform.GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 delta{ targetPos.x - casterPos.x, 0.f, targetPos.z - casterPos.z };
        const f32_t distSq = delta.x * delta.x + delta.z * delta.z;
        if (distSq <= 0.0001f)
            return false;

        const f32_t dist = std::sqrt(distSq);
        const Vec3 dir{ delta.x / dist, 0.f, delta.z / dist };
        const f32_t dashDistance = std::min(dist + dashThroughDistance, dashMaxDistance);
        const Vec3 dashEnd{
            casterPos.x + dir.x * dashDistance,
            casterPos.y,
            casterPos.z + dir.z * dashDistance
        };

        YasuoDashComponent dash{};
        dash.start = casterPos;
        dash.end = dashEnd;
        dash.durationSec = dashDurationSec;
        if (world.HasComponent<YasuoDashComponent>(caster))
            world.GetComponent<YasuoDashComponent>(caster) = dash;
        else
            world.AddComponent<YasuoDashComponent>(caster, dash);

        const Vec3 rot = casterTransform.GetRotation();
        casterTransform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::YASUO, dir, rot.y),
            rot.z });
        if (world.HasComponent<MoveTargetComponent>(caster))
            world.GetComponent<MoveTargetComponent>(caster).bHasTarget = false;
        return true;
    }

    bool_t PlaceCasterForUltimate(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        f32_t landingDistance)
    {
        if (caster == NULL_ENTITY ||
            target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        auto& casterTransform = world.GetComponent<TransformComponent>(caster);
        const Vec3 casterPos = casterTransform.GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        const Vec3 delta{ targetPos.x - casterPos.x, 0.f, targetPos.z - casterPos.z };
        const Vec3 dir = WintersMath::NormalizeXZ(delta);

        Vec3 landPos{
            targetPos.x - dir.x * landingDistance,
            casterPos.y,
            targetPos.z - dir.z * landingDistance
        };
        if (tc.pWalkable)
        {
            Vec3 guardedLandPos = landPos;
            if (tc.pWalkable->TryClampMoveSegmentXZ(casterPos, landPos, 0.5f, guardedLandPos))
                landPos = guardedLandPos;
            else
                landPos = casterPos;
        }

        casterTransform.SetPosition(landPos);
        PositionDiscontinuityComponent& discontinuity =
            world.HasComponent<PositionDiscontinuityComponent>(caster)
                ? world.GetComponent<PositionDiscontinuityComponent>(caster)
                : world.AddComponent<PositionDiscontinuityComponent>(
                    caster,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = tc.tickIndex;
        const Vec3 rot = casterTransform.GetRotation();
        casterTransform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::YASUO, dir, rot.y),
            rot.z });
        if (world.HasComponent<MoveTargetComponent>(caster))
            world.GetComponent<MoveTargetComponent>(caster).bHasTarget = false;
        return true;
    }

    EntityID SpawnYasuoSkillProjectile(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& direction,
        eProjectileKind kind,
        f32_t speed,
        f32_t maxDistance,
        f32_t radius,
        f32_t damage,
        u8_t rank)
    {
        if (caster == NULL_ENTITY || !world.HasComponent<TransformComponent>(caster))
            return NULL_ENTITY;

        Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        origin.y += 1.0f;

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = caster;
        projectile.sourceHandle = world.GetEntityHandle(caster);
        projectile.sourceTeam = casterTeam;
        projectile.kind = kind;
        if (kind == eProjectileKind::Wind ||
            kind == eProjectileKind::Tornado)
        {
            projectile.unitHitPolicy = eProjectileUnitHitPolicy::Pierce;
            projectile.maxUniqueHits = kMaxPiercingProjectileHits;
        }
        projectile.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::YASUO) << 8) |
            static_cast<u32_t>(eSkillSlot::Q));
        projectile.rank = rank;
        projectile.currentPos = origin;
        projectile.direction = WintersMath::NormalizeXZ(direction);
        projectile.speed = speed;
        projectile.maxDistance = maxDistance;
        projectile.hitRadius = radius;
        projectile.damage = damage;

        const EntityID projectileEntity = world.CreateEntity();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);

        return projectileEntity;
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        YasuoStateComponent& state = EnsureYasuoState(world, ctx.casterEntity);
        const u8_t stage = YasuoGameSim::ResolveQVariantStage(world, ctx.casterEntity);
        const Vec3 direction = ResolveCommandDirection(ctx);
        const f32_t qSpeed = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::Speed);
        const f32_t qLifetimeSec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::EffectDurationSec);
        const f32_t qRadius = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::Radius);
        const f32_t qDamage = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::BaseDamage);
        const f32_t tornadoSpeed = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::TornadoSpeed);
        const f32_t tornadoLifetimeSec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::TornadoDurationSec);
        const f32_t tornadoRadius = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::TornadoRadius);
        const f32_t tornadoDamage = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::TornadoDamage);
        const f32_t dashAreaRadius = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::DashAreaRadius);
        const f32_t dashAreaDamage = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::DashAreaDamage);

        switch (stage)
        {
        case 4:
        {
            const Vec3 center = ResolveCasterPosition(world, ctx.casterEntity);
            EnqueueAreaDamage(
                world,
                ctx.casterEntity,
                ctx.casterTeam,
                center,
                dashAreaRadius,
                dashAreaDamage,
                static_cast<u16_t>((static_cast<u32_t>(eChampion::YASUO) << 8) | 1u),
                ctx.skillRank);
            if (state.qStackCount >= 2u)
            {
                const std::vector<EntityID> targets =
                    GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                        world,
                        ctx.casterEntity,
                        center,
                        dashAreaRadius);
                for (EntityID target : targets)
                {
                    YasuoGameSim::ApplyTornadoAirborne(
                        world,
                        *ctx.pTickCtx,
                        ctx.casterEntity,
                        target);
                }
                state.qStackCount = 0u;
                state.qStackTimer = 0.f;
            }
            break;
        }

        case 3:
            SpawnYasuoSkillProjectile(
                world,
                ctx.casterEntity,
                ctx.casterTeam,
                direction,
                eProjectileKind::Tornado,
                tornadoSpeed,
                tornadoSpeed * tornadoLifetimeSec,
                tornadoRadius,
                tornadoDamage,
                ctx.skillRank);
            state.qStackCount = 0;
            state.qStackTimer = 0.f;
            break;

        case 2:
        case 1:
        default:
            SpawnYasuoSkillProjectile(
                world,
                ctx.casterEntity,
                ctx.casterTeam,
                direction,
                eProjectileKind::Wind,
                qSpeed,
                qSpeed * qLifetimeSec,
                qRadius,
                qDamage,
                ctx.skillRank);
            break;
        }

        std::cout << "[YasuoSim] Q accepted caster=" << ctx.casterEntity
            << " stage=" << static_cast<u32_t>(stage)
            << " stack=" << static_cast<u32_t>(state.qStackCount) << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        const Vec3 direction = ResolveCommandDirection(ctx);
        const Vec3 origin = ResolveCasterPosition(world, ctx.casterEntity);
        const u8_t rank = ctx.skillRank > 0u ? ctx.skillRank : 1u;
        const f32_t wHalfLength = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::RectLength, 1.6f);
        const f32_t wHalfLengthPerRank = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::RectLengthPerRank, 0.35f);
        const f32_t wHalfThickness = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::RectWidth, 0.5f);
        const f32_t wFormationDelaySec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::FormationDelaySec, 0.25f);
        const f32_t wDurationSec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::EffectDurationSec, 4.f);

        ProjectileBarrierComponent barrier{};
        barrier.sourceEntity = ctx.casterEntity;
        barrier.sourceTeam = ctx.casterTeam;
        barrier.origin = origin;
        barrier.previousCenter = origin;
        barrier.center = origin;
        barrier.direction = direction;
        barrier.halfLength = wHalfLength +
            wHalfLengthPerRank * static_cast<f32_t>(rank - 1u);
        barrier.halfThickness = wHalfThickness;
        barrier.spawnTick = ctx.pTickCtx->tickIndex;
        barrier.formationEndTick = ctx.pTickCtx->tickIndex +
            static_cast<u64_t>(
                std::ceil(wFormationDelaySec / DeterministicTime::kFixedDt));
        barrier.expireTick = ctx.pTickCtx->tickIndex +
            static_cast<u64_t>(
                std::ceil(wDurationSec / DeterministicTime::kFixedDt));

        const EntityID barrierEntity = world.CreateEntity();
        world.AddComponent<ProjectileBarrierComponent>(barrierEntity, barrier);

        std::cout << "[YasuoSim] W accepted caster=" << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        YasuoStateComponent& state = EnsureYasuoState(world, ctx.casterEntity);
        const f32_t eActiveWindowSec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::EffectDurationSec);
        const f32_t eDashThroughDistance = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::Gap);
        const f32_t eDashMaxDistance = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::DashDistance);
        const f32_t eDashDurationSec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::DashDurationSec);
        const f32_t eDamage = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::BaseDamage);
        state.bEActive = true;
        state.eActiveTimer = eActiveWindowSec;

        const bool_t bDashed = StartDashThroughTarget(
            world,
            ctx.casterEntity,
            target,
            eDashThroughDistance,
            eDashMaxDistance,
            eDashDurationSec);
        AttachSweepingBladeLockout(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target);
        EnqueueTargetDamage(
            world,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            eDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YASUO) << 8) | 3u),
            ctx.skillRank);

        std::cout << "[YasuoSim] E active caster=" << ctx.casterEntity
            << " target=" << target
            << " dashed=" << (bDashed ? 1 : 0)
            << " window=" << eActiveWindowSec << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const f32_t holdAirborneSec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::R, eSkillEffectParamId::AirborneDurationSec);
        const f32_t landingDistance = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::R, eSkillEffectParamId::Gap);
        const f32_t rDamage = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::R, eSkillEffectParamId::BaseDamage);
        EntityID target = ctx.pCommand ? ctx.pCommand->targetEntity : NULL_ENTITY;
        if (!IsAirborne(world, target))
            target = YasuoGameSim::FindAirborneTarget(world, ctx.casterEntity, ctx.casterTeam, 14.f);

        if (target == NULL_ENTITY)
        {
            std::cout << "[YasuoSim] R ignored caster=" << ctx.casterEntity
                << " reason=no-airborne\n";
            return;
        }

        if (world.HasComponent<YasuoStateComponent>(ctx.casterEntity))
        {
            YasuoStateComponent& state =
                world.GetComponent<YasuoStateComponent>(ctx.casterEntity);
            state.fPassiveFlow = state.fPassiveFlowMax;
        }

        GameplayStatus::ApplyAirborne(
            world,
            *ctx.pTickCtx,
            target,
            ctx.casterEntity,
            eChampion::YASUO,
            eSkillSlot::R,
            holdAirborneSec);
        const bool_t bPlaced =
            PlaceCasterForUltimate(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target,
                landingDistance);
        EnqueueTargetDamage(
            world,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            rDamage,
            static_cast<u16_t>((static_cast<u32_t>(eChampion::YASUO) << 8) | 4u),
            ctx.skillRank);

        std::cout << "[YasuoSim] R accepted caster=" << ctx.casterEntity
            << " target=" << target
            << " placed=" << (bPlaced ? 1 : 0) << "\n";
    }
}

namespace YasuoGameSim
{
    f32_t ResolvePassiveFlowDistancePerPoint(u8_t level)
    {
        if (level >= 13u)
            return 0.45f;
        if (level >= 7u)
            return 0.50f;
        return 0.55f;
    }

    f32_t ResolvePassiveShieldAmount(u8_t level)
    {
        constexpr f32_t kLevelOneShield = 125.f;
        constexpr f32_t kLevelEighteenShield = 600.f;
        constexpr f32_t kLevelEighteenGrowthMultiplier = 17.f;
        const u8_t resolvedLevel = (std::clamp)(
            level,
            static_cast<u8_t>(1u),
            static_cast<u8_t>(18u));
        return kLevelOneShield +
            (kLevelEighteenShield - kLevelOneShield) /
                kLevelEighteenGrowthMultiplier *
                CCombatFormula::GrowthMultiplier(resolvedLevel);
    }

    bool_t CanTriggerPassiveShieldFromSource(CWorld& world, EntityID source)
    {
        return source != NULL_ENTITY &&
            (world.HasComponent<ChampionComponent>(source) ||
                world.HasComponent<JungleComponent>(source));
    }

    bool_t CanCastSweepingBlade(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (caster == NULL_ENTITY ||
            target == NULL_ENTITY ||
            caster == target ||
            !world.IsAlive(caster) ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target) ||
            !GameplayStateQuery::CanReceiveEnemyAbilityHit(
                world,
                caster,
                target))
        {
            return false;
        }

        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::YASUO,
            static_cast<u8_t>(eSkillSlot::E));
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
        const f32_t distanceSq =
            WintersMath::DistanceSqXZ(casterPosition, targetPosition);
        if (distanceSq <= 0.0001f ||
            distanceSq >
            effectiveRange * effectiveRange)
        {
            return false;
        }

        const EntityID relationEntity =
            FindSweepingBladeLockoutRelation(world, caster, target);
        if (relationEntity == NULL_ENTITY)
            return true;

        const YasuoSweepingBladeLockoutComponent& lockout =
            world.GetComponent<YasuoSweepingBladeLockoutComponent>(relationEntity);
        return tc.tickIndex >= lockout.uExpireTick;
    }

    bool_t TryBufferQDuringE(
        CWorld& world,
        const TickContext& tc,
        const GameCommand& cmd)
    {
        if (cmd.kind != eCommandKind::CastSkill ||
            cmd.slot != static_cast<u8_t>(eSkillSlot::Q) ||
            cmd.itemId > 1u ||
            cmd.issuerEntity == NULL_ENTITY ||
            !world.IsAlive(cmd.issuerEntity) ||
            !world.HasComponent<ChampionComponent>(cmd.issuerEntity) ||
            world.GetComponent<ChampionComponent>(cmd.issuerEntity).id != eChampion::YASUO ||
            !world.HasComponent<ActionStateComponent>(cmd.issuerEntity) ||
            !world.HasComponent<YasuoStateComponent>(cmd.issuerEntity))
        {
            return false;
        }

        const ActionStateComponent& action =
            world.GetComponent<ActionStateComponent>(cmd.issuerEntity);
        if (!IsYasuoEAction(action) ||
            tc.tickIndex >= action.lockEndTick ||
            !world.GetComponent<YasuoStateComponent>(cmd.issuerEntity).bEActive)
        {
            return false;
        }

        YasuoEqInputBufferComponent& buffer =
            world.HasComponent<YasuoEqInputBufferComponent>(cmd.issuerEntity)
                ? world.GetComponent<YasuoEqInputBufferComponent>(cmd.issuerEntity)
                : world.AddComponent<YasuoEqInputBufferComponent>(
                    cmd.issuerEntity,
                    YasuoEqInputBufferComponent{});
        if (buffer.bPending || buffer.bExecuting)
            return true;

        buffer = {};
        buffer.hCaster = world.GetEntityHandle(cmd.issuerEntity);
        buffer.vDirection = cmd.direction;
        buffer.vGroundPos = cmd.groundPos;
        buffer.uIssuedAtTick = cmd.issuedAtTick;
        buffer.uRewindTicks = cmd.rewindTicks;
        buffer.uCommandSequence = cmd.sequenceNum;
        buffer.uSourceSessionId = cmd.sourceSessionId;
        buffer.uEActionSequence = action.sequence;
        buffer.uTargetEntity = cmd.targetEntity;
        buffer.uItemId = cmd.itemId;
        buffer.bPending = true;
        return true;
    }

    void CancelRuntime(CWorld& world, EntityID caster)
    {
        ClearYasuoEqInputBuffer(world, caster);

        const EntityHandle hCaster = world.GetEntityHandle(caster);
        if (hCaster.IsValid())
        {
            const auto relations =
                DeterministicEntityIterator<
                    YasuoSweepingBladeLockoutComponent>::CollectSorted(world);
            for (EntityID relationEntity : relations)
            {
                const YasuoSweepingBladeLockoutComponent& lockout =
                    world.GetComponent<YasuoSweepingBladeLockoutComponent>(relationEntity);
                if (lockout.hSource == hCaster)
                    world.DestroyEntity(relationEntity);
            }
        }

        if (world.HasComponent<YasuoDashComponent>(caster))
            world.RemoveComponent<YasuoDashComponent>(caster);

        if (world.HasComponent<YasuoStateComponent>(caster))
            world.RemoveComponent<YasuoStateComponent>(caster);
    }

    u8_t ResolveQVariantStage(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<YasuoEqInputBufferComponent>(caster) &&
            world.GetComponent<YasuoEqInputBufferComponent>(caster).bExecuting)
        {
            return 4;
        }

        if (!world.HasComponent<YasuoStateComponent>(caster))
            return 1;

        const auto& state = world.GetComponent<YasuoStateComponent>(caster);
        if (state.bEActive)
            return 4;
        if (state.qStackCount >= 2)
            return 3;
        if (state.qStackCount == 1)
            return 2;

        return 1;
    }

    void RegisterQHit(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eProjectileKind kind)
    {
        if (kind != eProjectileKind::Wind ||
            caster == NULL_ENTITY ||
            !world.IsAlive(caster))
        {
            return;
        }

        YasuoStateComponent& state = EnsureYasuoState(world, caster);
        state.qStackCount = std::min<u8_t>(
            2u,
            static_cast<u8_t>(state.qStackCount + 1u));
        state.qStackTimer = ResolveYasuoSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::Q,
            eSkillEffectParamId::StackWindowSec);
    }

    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam /*casterTeam*/, f32_t radius)
    {
        if (caster == NULL_ENTITY || !world.HasComponent<TransformComponent>(caster))
            return NULL_ENTITY;

        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        const f32_t radiusSq = radius * radius;
        EntityID best = NULL_ENTITY;
        f32_t bestDistSq = radiusSq;

        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                world,
                caster,
                origin,
                radius);
        for (EntityID entity : targets)
        {
            if (GameplayStateQuery::ResolveTargetKind(world, entity) !=
                    GameplayStateQuery::eGameplayTargetKind::Champion ||
                !IsAirborne(world, entity) ||
                !world.HasComponent<TransformComponent>(entity))
            {
                continue;
            }

            const f32_t distSq = WintersMath::DistanceSqXZ(
                origin,
                world.GetComponent<TransformComponent>(entity).GetPosition());
            if (distSq < bestDistSq ||
                (distSq == bestDistSq && (best == NULL_ENTITY || entity < best)))
            {
                bestDistSq = distSq;
                best = entity;
            }
        }

        return best;
    }

    void ApplyTornadoAirborne(CWorld& world, const TickContext& tc, EntityID source, EntityID target)
    {
        const f32_t airborneDurationSec = ResolveYasuoSkillEffectParam(
            world,
            tc,
            source,
            eSkillSlot::Q,
            eSkillEffectParamId::AirborneDurationSec);
        GameplayStatus::ApplyAirborne(
            world,
            tc,
            target,
            source,
            eChampion::YASUO,
            eSkillSlot::Q,
            airborneDurationSec);
        std::cout << "[YasuoSim] airborne target=" << target
            << " source=" << source
            << " duration=" << airborneDurationSec << "\n";
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YASUO, GameplayHookVariant::Q_OnCastAccepted), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YASUO, GameplayHookVariant::W_OnCastAccepted), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YASUO, GameplayHookVariant::E_OnCastAccepted), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::YASUO, GameplayHookVariant::R_OnCastAccepted), &OnR);

        s_bRegistered = true;
        std::cout << "[YasuoSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc, ICommandExecutor* pExecutor)
    {
        const auto lockoutRelations =
            DeterministicEntityIterator<
                YasuoSweepingBladeLockoutComponent>::CollectSorted(world);
        for (EntityID relationEntity : lockoutRelations)
        {
            if (!world.IsAlive(relationEntity) ||
                !world.HasComponent<YasuoSweepingBladeLockoutComponent>(relationEntity))
            {
                continue;
            }

            const YasuoSweepingBladeLockoutComponent lockout =
                world.GetComponent<YasuoSweepingBladeLockoutComponent>(relationEntity);
            const bool_t bExpired = tc.tickIndex >= lockout.uExpireTick;
            const bool_t bStaleEndpoint =
                world.ResolveEntity(lockout.hSource) == NULL_ENTITY ||
                world.ResolveEntity(lockout.hTarget) == NULL_ENTITY;
            if (bExpired || bStaleEndpoint)
                world.DestroyEntity(relationEntity);
        }

        std::vector<EntityID> expiredBarriers;
        world.ForEach<ProjectileBarrierComponent>(
            std::function<void(EntityID, ProjectileBarrierComponent&)>(
                [&](EntityID entity, ProjectileBarrierComponent& barrier)
                {
                    if (tc.tickIndex >= barrier.expireTick)
                    {
                        expiredBarriers.push_back(entity);
                        return;
                    }

                    f32_t forwardDistance = 0.f;
                    if (tc.tickIndex < barrier.formationEndTick)
                    {
                        const u64_t formationTicks = (std::max)(
                            1ull,
                            barrier.formationEndTick - barrier.spawnTick);
                        const f32_t t = static_cast<f32_t>(
                            tc.tickIndex - barrier.spawnTick) /
                            static_cast<f32_t>(formationTicks);
                        forwardDistance = 4.f * (std::clamp)(t, 0.f, 1.f);
                    }
                    else
                    {
                        const u64_t driftTicks = (std::max)(
                            1ull,
                            barrier.expireTick - barrier.formationEndTick);
                        const f32_t t = static_cast<f32_t>(
                            tc.tickIndex - barrier.formationEndTick) /
                            static_cast<f32_t>(driftTicks);
                        forwardDistance = 4.f + 0.5f * (std::clamp)(t, 0.f, 1.f);
                    }
                    barrier.previousCenter = barrier.center;
                    barrier.center = Vec3{
                        barrier.origin.x + barrier.direction.x * forwardDistance,
                        barrier.origin.y,
                        barrier.origin.z + barrier.direction.z * forwardDistance };
                }));
        for (EntityID entity : expiredBarriers)
            world.DestroyEntity(entity);

        std::vector<EntityID> finishedDashes;
        world.ForEach<YasuoDashComponent, TransformComponent>(
            std::function<void(EntityID, YasuoDashComponent&, TransformComponent&)>(
                [&](EntityID entity, YasuoDashComponent& dash, TransformComponent& transform)
                {
                    if (!GameplayStateQuery::CanMove(world, entity) ||
                        world.HasComponent<ForcedMotionComponent>(entity))
                    {
                        finishedDashes.push_back(entity);
                        return;
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

                    const Vec3 pos{
                        dash.start.x + (dash.end.x - dash.start.x) * t,
                        dash.start.y + (dash.end.y - dash.start.y) * t,
                        dash.start.z + (dash.end.z - dash.start.z) * t
                    };
                    Vec3 guardedPos = pos;
                    bool_t bDashBlocked = false;
                    if (tc.pWalkable)
                    {
                        const Vec3 currentPos = transform.GetLocalPosition();
                        if (!tc.pWalkable->TryClampMoveSegmentXZ(currentPos, pos, 0.5f, guardedPos))
                        {
                            guardedPos = currentPos;
                            bDashBlocked = true;
                        }
                        else if (WintersMath::DistanceSqXZ(guardedPos, pos) > 0.0001f)
                        {
                            bDashBlocked = true;
                        }
                    }

                    transform.SetPosition(guardedPos);
                    if (bDashBlocked && t < 1.f)
                        finishedDashes.push_back(entity);
                }));

        for (EntityID entity : finishedDashes)
        {
            if (world.HasComponent<ForcedMotionComponent>(entity))
            {
                world.RemoveComponent<YasuoDashComponent>(entity);
                continue;
            }
            if (world.HasComponent<YasuoDashComponent>(entity))
                SnapDashArrivalToWalkable(world, tc, entity,
                    world.GetComponent<YasuoDashComponent>(entity).start);
            world.RemoveComponent<YasuoDashComponent>(entity);
        }

        std::vector<EntityID> invalidEqBuffers;
        std::vector<EntityID> readyEqBuffers;
        world.ForEach<YasuoEqInputBufferComponent>(
            std::function<void(EntityID, YasuoEqInputBufferComponent&)>(
                [&](EntityID entity, YasuoEqInputBufferComponent& buffer)
                {
                    if (!buffer.bPending ||
                        !buffer.hCaster.IsValid() ||
                        !world.IsAlive(buffer.hCaster) ||
                        buffer.hCaster.GetIndex() != entity ||
                        !world.HasComponent<ChampionComponent>(entity) ||
                        world.GetComponent<ChampionComponent>(entity).id != eChampion::YASUO ||
                        !world.HasComponent<ActionStateComponent>(entity) ||
                        !GameplayStateQuery::CanCast(world, entity))
                    {
                        invalidEqBuffers.push_back(entity);
                        return;
                    }

                    const ActionStateComponent& action =
                        world.GetComponent<ActionStateComponent>(entity);
                    if (!IsYasuoEAction(action) ||
                        action.sequence != buffer.uEActionSequence)
                    {
                        invalidEqBuffers.push_back(entity);
                        return;
                    }
                    if (tc.tickIndex < action.lockEndTick ||
                        world.HasComponent<YasuoDashComponent>(entity) ||
                        !pExecutor)
                    {
                        return;
                    }
                    readyEqBuffers.push_back(entity);
                }));

        for (EntityID entity : invalidEqBuffers)
            ClearYasuoEqInputBuffer(world, entity);

        std::sort(readyEqBuffers.begin(), readyEqBuffers.end());
        for (EntityID entity : readyEqBuffers)
        {
            if (!world.HasComponent<YasuoEqInputBufferComponent>(entity))
                continue;

            YasuoEqInputBufferComponent& buffer =
                world.GetComponent<YasuoEqInputBufferComponent>(entity);
            buffer.bPending = false;
            buffer.bExecuting = true;

            GameCommand q{};
            q.kind = eCommandKind::CastSkill;
            q.issuerEntity = entity;
            q.issuedAtTick = buffer.uIssuedAtTick;
            q.sequenceNum = buffer.uCommandSequence;
            q.rewindTicks = buffer.uRewindTicks;
            q.slot = static_cast<u8_t>(eSkillSlot::Q);
            q.targetEntity = buffer.uTargetEntity;
            q.groundPos = buffer.vGroundPos;
            q.direction = buffer.vDirection;
            q.itemId = buffer.uItemId;
            q.sourceSessionId = buffer.uSourceSessionId;
            pExecutor->ExecuteCommand(world, tc, q);
            ClearYasuoEqInputBuffer(world, entity);
        }

        world.ForEach<YasuoStateComponent>(
            std::function<void(EntityID, YasuoStateComponent&)>(
                [&](EntityID entity, YasuoStateComponent& state)
                {
                    if (state.qStackTimer > 0.f)
                    {
                        state.qStackTimer = std::max(0.f, state.qStackTimer - tc.fDt);
                        if (state.qStackTimer <= 0.f)
                            state.qStackCount = 0;
                    }

                    if (state.bEActive)
                    {
                        state.eActiveTimer = std::max(0.f, state.eActiveTimer - tc.fDt);
                        if (state.eActiveTimer <= 0.f)
                            state.bEActive = false;
                    }

                    if (!world.HasComponent<ChampionComponent>(entity) ||
                        !world.HasComponent<TransformComponent>(entity) ||
                        world.GetComponent<ChampionComponent>(entity).id != eChampion::YASUO)
                    {
                        return;
                    }

                    const Vec3 position =
                        world.GetComponent<TransformComponent>(entity).GetPosition();
                    const PositionDiscontinuityComponent* discontinuity =
                        world.TryGetComponent<PositionDiscontinuityComponent>(entity);
                    const bool_t bNewDiscontinuity = discontinuity &&
                        discontinuity->uTick > state.uPassiveLastObservedDiscontinuityTick;
                    if (discontinuity)
                    {
                        state.uPassiveLastObservedDiscontinuityTick = (std::max)(
                            state.uPassiveLastObservedDiscontinuityTick,
                            discontinuity->uTick);
                    }
                    const bool_t bDead =
                        world.HasComponent<HealthComponent>(entity) &&
                        (world.GetComponent<HealthComponent>(entity).bIsDead ||
                            world.GetComponent<HealthComponent>(entity).fCurrent <= 0.f);

                    if (!state.bPassivePositionInitialized || bNewDiscontinuity || bDead)
                    {
                        state.fPassiveLastX = position.x;
                        state.fPassiveLastZ = position.z;
                        state.bPassivePositionInitialized = true;
                        return;
                    }

                    const f32_t dx = position.x - state.fPassiveLastX;
                    const f32_t dz = position.z - state.fPassiveLastZ;
                    state.fPassiveLastX = position.x;
                    state.fPassiveLastZ = position.z;
                    const f32_t distance = std::sqrt(dx * dx + dz * dz);
                    const u8_t level = world.GetComponent<ChampionComponent>(entity).level;
                    const f32_t distancePerPoint =
                        ResolvePassiveFlowDistancePerPoint(level);
                    if (std::isfinite(distance) && distance > 0.f && distancePerPoint > 0.f)
                    {
                        state.fPassiveFlow = (std::min)(
                            state.fPassiveFlowMax,
                            state.fPassiveFlow + distance / distancePerPoint);
                    }

                }));
    }
}
