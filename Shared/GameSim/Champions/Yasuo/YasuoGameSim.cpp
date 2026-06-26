#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Systems/Move/DashArrival.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr f32_t kYasuoQHitDelaySec = 0.25f;
    constexpr f32_t kYasuoAirborneLift = 2.1f;

    struct YasuoDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t durationSec = 0.f;
    };

    struct YasuoAirborneComponent
    {
        EntityID sourceEntity = NULL_ENTITY;
        f32_t baseY = 0.f;
        f32_t elapsedSec = 0.f;
        f32_t durationSec = 0.f;
        f32_t lift = kYasuoAirborneLift;
    };

    YasuoStateComponent& EnsureYasuoState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<YasuoStateComponent>(caster))
            world.AddComponent<YasuoStateComponent>(caster, YasuoStateComponent{});

        return world.GetComponent<YasuoStateComponent>(caster);
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
        const f32_t radiusSq = radius * radius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == caster || champion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > radiusSq)
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
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID target, MinionComponent& minion, TransformComponent& transform)
                {
                    if (minion.team == casterTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(origin, transform.GetPosition()) > radiusSq)
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
                }));
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

        if (world.HasComponent<YasuoAirborneComponent>(target))
            return true;

        return world.HasComponent<GameplayStateComponent>(target) &&
            (world.GetComponent<GameplayStateComponent>(target).stateFlags &
                kGameplayStateAirborneFlag) != 0u;
    }

    void ApplyAirborne(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        eSkillSlot slot,
        f32_t durationSec)
    {
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return;
        }

        auto& transform = world.GetComponent<TransformComponent>(target);
        const Vec3 pos = transform.GetPosition();

        if (world.HasComponent<YasuoAirborneComponent>(target))
        {
            auto& airborne = world.GetComponent<YasuoAirborneComponent>(target);
            airborne.sourceEntity = source;
            const f32_t remaining = airborne.durationSec - airborne.elapsedSec;
            if (remaining < durationSec)
                airborne.durationSec = airborne.elapsedSec + durationSec;
            airborne.lift = kYasuoAirborneLift;
        }
        else
        {
            YasuoAirborneComponent airborne{};
            airborne.sourceEntity = source;
            airborne.baseY = pos.y;
            airborne.durationSec = durationSec;
            airborne.lift = kYasuoAirborneLift;
            world.AddComponent<YasuoAirborneComponent>(target, airborne);
        }

        if (world.HasComponent<MoveTargetComponent>(target))
            world.GetComponent<MoveTargetComponent>(target).bHasTarget = false;

        GameplayStatus::ApplyAirborne(
            world,
            tc,
            target,
            source,
            eChampion::YASUO,
            slot,
            durationSec);
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
        projectile.sourceTeam = casterTeam;
        projectile.kind = kind;
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
        const f32_t qStackWindowSec = ResolveYasuoSkillEffectParam(
            ctx, eSkillSlot::Q, eSkillEffectParamId::StackWindowSec);
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
            EnqueueAreaDamage(
                world,
                ctx.casterEntity,
                ctx.casterTeam,
                ResolveCasterPosition(world, ctx.casterEntity),
                dashAreaRadius,
                dashAreaDamage,
                static_cast<u16_t>((static_cast<u32_t>(eChampion::YASUO) << 8) | 1u),
                ctx.skillRank);
            break;

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
            state.qStackCount = std::min<u8_t>(
                2u,
                static_cast<u8_t>(state.qStackCount + 1u));
            state.qStackTimer = qStackWindowSec;
            break;
        }

        std::cout << "[YasuoSim] Q accepted caster=" << ctx.casterEntity
            << " stage=" << static_cast<u32_t>(stage)
            << " stack=" << static_cast<u32_t>(state.qStackCount)
            << " delay=" << kYasuoQHitDelaySec << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
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

        ApplyAirborne(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target,
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
    u8_t ResolveQVariantStage(CWorld& world, EntityID caster)
    {
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

    EntityID FindAirborneTarget(CWorld& world, EntityID caster, eTeam casterTeam, f32_t radius)
    {
        if (caster == NULL_ENTITY || !world.HasComponent<TransformComponent>(caster))
            return NULL_ENTITY;

        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        const f32_t radiusSq = radius * radius;
        EntityID best = NULL_ENTITY;
        f32_t bestDistSq = radiusSq;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == caster || champion.team == casterTeam)
                        return;
                    if (!IsAirborne(world, entity))
                        return;

                    const f32_t distSq = WintersMath::DistanceSqXZ(origin, transform.GetPosition());
                    if (distSq <= bestDistSq)
                    {
                        bestDistSq = distSq;
                        best = entity;
                    }
                }));

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
        ApplyAirborne(
            world,
            tc,
            source,
            target,
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

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> finishedDashes;
        world.ForEach<YasuoDashComponent, TransformComponent>(
            std::function<void(EntityID, YasuoDashComponent&, TransformComponent&)>(
                [&](EntityID entity, YasuoDashComponent& dash, TransformComponent& transform)
                {
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
            if (world.HasComponent<YasuoDashComponent>(entity))
                SnapDashArrivalToWalkable(world, tc, entity,
                    world.GetComponent<YasuoDashComponent>(entity).start);
            world.RemoveComponent<YasuoDashComponent>(entity);
        }

        std::vector<EntityID> finishedAirborne;
        world.ForEach<YasuoAirborneComponent, TransformComponent>(
            std::function<void(EntityID, YasuoAirborneComponent&, TransformComponent&)>(
                [&](EntityID entity, YasuoAirborneComponent& airborne, TransformComponent& transform)
                {
                    airborne.elapsedSec += tc.fDt;
                    const f32_t duration = std::max(0.01f, airborne.durationSec);
                    f32_t t = airborne.elapsedSec / duration;
                    if (t >= 1.f)
                    {
                        t = 1.f;
                        finishedAirborne.push_back(entity);
                    }

                    const f32_t arc = std::sin(t * WintersMath::kPi);
                    Vec3 pos = transform.GetPosition();
                    pos.y = airborne.baseY + airborne.lift * arc;
                    transform.SetPosition(pos);
                }));

        for (EntityID entity : finishedAirborne)
        {
            EntityID airborneSource = NULL_ENTITY;
            if (world.HasComponent<YasuoAirborneComponent>(entity) &&
                world.HasComponent<TransformComponent>(entity))
            {
                const auto& airborne = world.GetComponent<YasuoAirborneComponent>(entity);
                airborneSource = airborne.sourceEntity;
                auto& transform = world.GetComponent<TransformComponent>(entity);
                Vec3 pos = transform.GetPosition();
                pos.y = airborne.baseY;
                transform.SetPosition(pos);
            }
            world.RemoveComponent<YasuoAirborneComponent>(entity);
            if (world.HasComponent<StunComponent>(entity) &&
                world.GetComponent<StunComponent>(entity).sourceEntity == airborneSource)
            {
                world.RemoveComponent<StunComponent>(entity);
            }
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

                    if (state.fPassiveShieldTimer > 0.f)
                    {
                        state.fPassiveShieldTimer = std::max(0.f, state.fPassiveShieldTimer - tc.fDt);
                        if (state.fPassiveShieldTimer <= 0.f)
                            state.fPassiveShieldRemaining = 0.f;
                    }

                    if (world.HasComponent<ChampionComponent>(entity))
                        world.GetComponent<ChampionComponent>(entity).shield = state.fPassiveShieldRemaining;
                }));
    }
}
