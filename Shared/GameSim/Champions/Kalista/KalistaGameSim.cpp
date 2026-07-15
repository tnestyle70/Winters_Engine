#include "Shared/GameSim/Champions/Kalista/KalistaGameSim.h"

#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Components/ActionStateComponent.h"
#include "Shared/GameSim/Components/ChampionAIComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/KalistaBondComponent.h"
#include "Shared/GameSim/Components/KalistaRendComponent.h"
#include "Shared/GameSim/Components/KalistaSentinelComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillStateComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/SpatialAgentComponent.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/Ecs/VisionComponents.h"
#include "Shared/GameSim/Core/World/World.h"
#include "WintersMath.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    f32_t ResolveKalistaQSkillEffectParam(
        CWorld& world,
        EntityID caster,
        const TickContext& tc,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::Q),
            param,
            fallbackValue);
    }

    f32_t ResolveKalistaESkillEffectParam(
        CWorld& world,
        EntityID caster,
        const TickContext& tc,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
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
        f32_t fallbackValue = 0.f)
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

    f32_t ResolveKalistaRSkillEffectParam(
        CWorld& world,
        EntityID caster,
        const TickContext& tc,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::R),
            param,
            fallbackValue);
    }

    f32_t ResolveKalistaWSummonPolicyParam(
        CWorld& world,
        EntityID caster,
        const TickContext& tc,
        eSummonPolicyParamId param,
        f32_t fallbackValue = 0.f)
    {
        return GameplayDefinitionQuery::ResolveSummonPolicyParam(
            world,
            caster,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::W),
            param,
            fallbackValue);
    }

    Vec3 ResolveKalistaCastDirection(const GameplayHookContext& ctx, const Vec3& origin)
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

    eTeam ResolveKalistaProjectileTeam(CWorld& world, EntityID source)
    {
        const eTeam team = GameplayStateQuery::ResolveEntityTeam(world, source);
        return team == eTeam::TEAM_END ? eTeam::Neutral : team;
    }

    bool_t IsKalistaEntity(CWorld& world, EntityID entity)
    {
        if (world.HasComponent<ChampionComponent>(entity))
        {
            return world.GetComponent<ChampionComponent>(entity).id ==
                eChampion::KALISTA;
        }
        return world.HasComponent<StatComponent>(entity) &&
            world.GetComponent<StatComponent>(entity).championId ==
                eChampion::KALISTA;
    }

    EntityID SpawnKalistaProjectile(
        CWorld& world,
        EntityID source,
        EntityID target,
        eProjectileKind kind,
        u8_t slot,
        u8_t rank,
        const Vec3& origin,
        const Vec3& direction,
        f32_t speed,
        f32_t maxDistance,
        f32_t hitRadius,
        f32_t damage,
        eDamageType damageType,
        eDamageSourceKind damageSourceKind,
        u32_t damageFlags,
        f32_t totalAdRatio = 0.f,
        f32_t bonusAdRatio = 0.f,
        f32_t apRatio = 0.f)
    {
        if (source == NULL_ENTITY || !world.IsAlive(source) ||
            !std::isfinite(speed) || speed <= 0.f ||
            !std::isfinite(maxDistance) || maxDistance <= 0.f)
        {
            return NULL_ENTITY;
        }

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = source;
        projectile.targetEntity = target;
        projectile.sourceHandle = world.GetEntityHandle(source);
        projectile.targetHandle = target != NULL_ENTITY
            ? world.GetEntityHandle(target)
            : NULL_ENTITY_HANDLE;
        projectile.sourceTeam = ResolveKalistaProjectileTeam(world, source);
        projectile.kind = kind;
        projectile.unitHitPolicy = eProjectileUnitHitPolicy::Destroy;
        projectile.targetKindMask =
            ProjectileTarget_Champion |
            ProjectileTarget_MinionOrSummon |
            ProjectileTarget_JungleMonster |
            ProjectileTarget_Structure;
        projectile.maxUniqueHits = 1u;
        projectile.bCollidesWithTerrain = false;
        projectile.bPersistAfterSourceDeath = true;
        projectile.bApplyDamageOnHit = true;
        projectile.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::KALISTA) << 8) | slot);
        projectile.rank = rank == 0u ? 1u : rank;
        projectile.currentPos = origin;
        projectile.direction = WintersMath::NormalizeXZ(
            direction,
            Vec3{ 0.f, 0.f, 1.f },
            0.0001f);
        projectile.speed = speed;
        projectile.maxDistance = maxDistance;
        projectile.hitRadius = (std::max)(0.05f, hitRadius);
        projectile.damage = damage;
        projectile.totalAdRatio = totalAdRatio;
        projectile.bonusAdRatio = bonusAdRatio;
        projectile.apRatio = apRatio;
        projectile.damageType = damageType;
        projectile.damageSourceKind = damageSourceKind;
        projectile.sourceSlot = slot;
        projectile.damageFlags = damageFlags;

        const EntityHandle hProjectile = world.CreateEntityHandle();
        if (!hProjectile.IsValid())
            return NULL_ENTITY;

        const EntityID projectileEntity = hProjectile.GetIndex();
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);
        return projectileEntity;
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        const TickContext& tc = *ctx.pTickCtx;
        Vec3 origin = world.GetComponent<TransformComponent>(
            ctx.casterEntity).GetPosition();
        origin.y += 1.f;
        const Vec3 direction = ResolveKalistaCastDirection(ctx, origin);
        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            ctx.casterEntity,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::Q));
        if (range <= 0.f)
            range = 16.5f;

        const f32_t speed = ResolveKalistaQSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::Speed,
            27.f);
        const f32_t hitRadius = ResolveKalistaQSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::Radius,
            0.6f);
        const f32_t damage = ResolveKalistaQSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::BaseDamage,
            70.f);

        (void)SpawnKalistaProjectile(
            world,
            ctx.casterEntity,
            NULL_ENTITY,
            eProjectileKind::KalistaPierce,
            static_cast<u8_t>(eSkillSlot::Q),
            ctx.skillRank,
            origin,
            direction,
            speed,
            range,
            hitRadius,
            damage,
            eDamageType::Physical,
            eDamageSourceKind::Skill,
            DamageFlag_OnHit);
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

    void ApplyRendStunToTarget(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        const f32_t stunDurationSec = ResolveKalistaESkillEffectParam(
            world,
            caster,
            tc,
            eSkillEffectParamId::StunDurationSec,
            1.f);

        GameplayStatus::ApplyStun(
            world,
            tc,
            target,
            caster,
            eChampion::KALISTA,
            eSkillSlot::E,
            stunDurationSec);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        if (!world.HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const TickContext& tc = *ctx.pTickCtx;
        const f32_t range = ResolveRendRange(world, ctx.casterEntity, tc);
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                world,
                ctx.casterEntity,
                origin,
                range);

        // 데미지 = 기본 + 창당 데미지 × 박힌 창 수. 시전자 본인 창만 환산·소모한다.
        const f32_t baseDamage = ResolveKalistaESkillEffectParam(
            world, ctx.casterEntity, tc, eSkillEffectParamId::BaseDamage, 20.f);
        const f32_t damagePerSpear = ResolveKalistaESkillEffectParam(
            world, ctx.casterEntity, tc, eSkillEffectParamId::DamagePerSpear, 30.f);
        const eTeam casterTeam =
            GameplayStateQuery::ResolveEntityTeam(world, ctx.casterEntity);

        u32_t rippedTargets = 0u;
        for (EntityID target : targets)
        {
            ApplyRendStunToTarget(
                world,
                tc,
                ctx.casterEntity,
                target);

            if (!world.HasComponent<KalistaRendStackComponent>(target))
                continue;
            const KalistaRendStackComponent stack =
                world.GetComponent<KalistaRendStackComponent>(target);
            if (stack.sourceEntity != ctx.casterEntity || stack.stackCount == 0u)
                continue;

            DamageRequest request{};
            request.source = ctx.casterEntity;
            request.target = target;
            request.sourceTeam = casterTeam;
            request.type = eDamageType::Physical;
            request.flatAmount =
                baseDamage + damagePerSpear * static_cast<f32_t>(stack.stackCount);
            request.skillId = static_cast<u16_t>(
                (static_cast<u32_t>(eChampion::KALISTA) << 8) |
                static_cast<u8_t>(eSkillSlot::E));
            request.rank = ctx.skillRank;
            EnqueueDamageRequest(world, request);

            world.RemoveComponent<KalistaRendStackComponent>(target);
            ++rippedTargets;
        }

        std::cout << "[KalistaSim] E rend caster=" << ctx.casterEntity
                  << " ripped=" << rippedTargets << "\n";
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
        const Vec3 forward = ResolveKalistaCastDirection(ctx, origin);
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
                eSkillEffectParamId::Range);
        const f32_t lifetimeSec = ResolveKalistaWSummonPolicyParam(
            world,
            ctx.casterEntity,
            tc,
            eSummonPolicyParamId::DurationSec);
        const f32_t speed = ResolveKalistaWSummonPolicyParam(
            world,
            ctx.casterEntity,
            tc,
            eSummonPolicyParamId::MoveSpeed);
        const f32_t sightRange = ResolveKalistaWSummonPolicyParam(
            world,
            ctx.casterEntity,
            tc,
            eSummonPolicyParamId::SightRange);
        const f32_t radius = ResolveKalistaWSummonPolicyParam(
            world,
            ctx.casterEntity,
            tc,
            eSummonPolicyParamId::Radius);
        const f32_t halfAngleCos = ResolveKalistaWSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::HalfAngleCos);

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
        state.radius = radius;
        state.halfAngleCos = halfAngleCos;
        world.AddComponent<KalistaSentinelComponent>(sentinel, state);

        TransformComponent transform{};
        transform.SetPosition(origin);
        transform.SetRotation(Vec3{ 0.f, std::atan2f(forward.x, forward.z), 0.f });
        world.AddComponent<TransformComponent>(sentinel, transform);

        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Sensor;
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

    bool_t IsAliveChampion(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY ||
            !world.IsAlive(entity) ||
            !world.HasComponent<ChampionComponent>(entity) ||
            !world.HasComponent<TransformComponent>(entity))
        {
            return false;
        }

        if (!world.HasComponent<HealthComponent>(entity))
            return true;

        const auto& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    void ClearMove(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<MoveTargetComponent>(entity))
            return;

        auto& move = world.GetComponent<MoveTargetComponent>(entity);
        move.bHasTarget = false;
        move.pathCount = 0;
        move.pathIndex = 0;
    }

    bool_t IsCarriedByAnotherKalista(
        CWorld& world,
        EntityID owner,
        EntityID candidate)
    {
        bool_t bClaimed = false;
        world.ForEach<KalistaFateCallComponent>(
            std::function<void(EntityID, KalistaFateCallComponent&)>(
                [&](EntityID otherOwner, KalistaFateCallComponent& state)
                {
                    if (otherOwner != owner && state.entityCarried == candidate)
                        bClaimed = true;
                }));
        return bClaimed;
    }

    EntityID FindFateCallAlly(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        f32_t range)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return NULL_ENTITY;

        const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
        const f32_t rangeSq = range * range;

        const eChampion actualChampion =
            world.HasComponent<ChampionComponent>(caster)
                ? world.GetComponent<ChampionComponent>(caster).id
                : eChampion::NONE;
        if (actualChampion == eChampion::KALISTA)
        {
            if (!world.HasComponent<KalistaOathswornComponent>(caster))
                return NULL_ENTITY;

            const auto& oath = world.GetComponent<KalistaOathswornComponent>(caster);
            const EntityID candidate = oath.entityAlly;
            if (oath.eStage != eKalistaOathswornStage::Bound ||
                !IsAliveChampion(world, candidate) ||
                world.GetComponent<ChampionComponent>(candidate).team != casterTeam ||
                world.HasComponent<ForcedMotionComponent>(candidate) ||
                IsCarriedByAnotherKalista(world, caster, candidate) ||
                WintersMath::DistanceSqXZ(
                    origin,
                    world.GetComponent<TransformComponent>(candidate).GetPosition()) > rangeSq)
            {
                return NULL_ENTITY;
            }
            return candidate;
        }

        f32_t bestDistanceSq = rangeSq;
        EntityID best = NULL_ENTITY;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID candidate, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (candidate == caster ||
                        champion.team != casterTeam ||
                        !IsAliveChampion(world, candidate) ||
                        world.HasComponent<ForcedMotionComponent>(candidate) ||
                        IsCarriedByAnotherKalista(world, caster, candidate))
                    {
                        return;
                    }

                    const f32_t distanceSq = WintersMath::DistanceSqXZ(
                        origin,
                        transform.GetPosition());
                    if (distanceSq > rangeSq ||
                        (best != NULL_ENTITY &&
                            (distanceSq > bestDistanceSq ||
                                (distanceSq == bestDistanceSq && candidate >= best))))
                    {
                        return;
                    }

                    best = candidate;
                    bestDistanceSq = distanceSq;
                }));
        return best;
    }

    bool_t ApplyFateCallLock(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID carried,
        f32_t durationSec,
        bool_t bHidden,
        bool_t bEmitFeedback = true)
    {
        StatusEffectApplyDesc carry{};
        carry.effectId = eStatusEffectId::KalistaFateCallUntargetable;
        carry.stackPolicy = eStatusStackPolicy::RefreshDuration;
        carry.sourceEntity = caster;
        carry.stackGroup = GameplayStatus::MakeStatusStackGroup(
            eChampion::KALISTA,
            eSkillSlot::R);
        carry.stateFlags =
            kGameplayStateUntargetableFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        if (bHidden)
            carry.stateFlags |= kGameplayStateInvisibleFlag;
        carry.fDurationSec = durationSec;
        carry.fMoveSpeedMul = 1.f;
        if (bEmitFeedback)
            return GameplayStatus::TryApplyStatusEffect(world, carried, carry, tc);
        return GameplayStatus::TryApplyStatusEffect(world, carried, carry);
    }

    void ReleaseFateCall(
        CWorld& world,
        EntityID caster,
        const KalistaFateCallComponent& state)
    {
        if (state.entityCarried != NULL_ENTITY && world.IsAlive(state.entityCarried))
        {
            GameplayStatus::RemoveStatusEffect(
                world,
                state.entityCarried,
                eStatusEffectId::KalistaFateCallUntargetable,
                caster);
            if (world.HasComponent<KalistaFateCallCarriedComponent>(state.entityCarried))
                world.RemoveComponent<KalistaFateCallCarriedComponent>(state.entityCarried);
        }
    }

    void StartFateCallCarry(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !IsAliveChampion(*ctx.pWorld, ctx.casterEntity) ||
            ctx.pWorld->HasComponent<KalistaFateCallComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        const TickContext& tc = *ctx.pTickCtx;
        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            ctx.casterEntity,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            range = 12.f;

        const EntityID ally = FindFateCallAlly(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            range);
        if (ally == NULL_ENTITY)
            return;

        const f32_t stageWindowSec = GameplayDefinitionQuery::ResolveSkillStageWindowSec(
            world,
            ctx.casterEntity,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::R));
        const f32_t carryDurationSec = ResolveKalistaRSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::EffectDurationSec,
            stageWindowSec > 0.f ? stageWindowSec : 4.f);

        KalistaFateCallComponent state{};
        state.entityCarried = ally;
        state.eStage = eKalistaFateCallStage::Pulling;
        state.vPullStart =
            world.GetComponent<TransformComponent>(ally).GetPosition();
        state.fPullElapsedSec = 0.f;
        state.fRemainingSec = carryDurationSec;
        state.fLaunchDurationSec = ResolveKalistaRSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::DashDurationSec,
            0.45f);
        state.fCollisionRadius = ResolveKalistaRSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::Radius,
            2.5f);
        state.fAirborneDurationSec = ResolveKalistaRSkillEffectParam(
            world,
            ctx.casterEntity,
            tc,
            eSkillEffectParamId::AirborneDurationSec,
            1.0f);
        if (!ApplyFateCallLock(
            world,
            tc,
            ctx.casterEntity,
            ally,
            carryDurationSec,
            false))
        {
            return;
        }

        world.AddComponent<KalistaFateCallComponent>(ctx.casterEntity, state);
        world.AddComponent<KalistaFateCallCarriedComponent>(
            ally,
            KalistaFateCallCarriedComponent{ ctx.casterEntity, false });

        ClearMove(world, ally);
    }

    Vec3 ResolveChampionForward(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return Vec3{ 0.f, 0.f, 1.f };

        const eChampion champion = world.HasComponent<ChampionComponent>(entity)
            ? world.GetComponent<ChampionComponent>(entity).id
            : eChampion::NONE;
        const f32_t visualYaw =
            world.GetComponent<TransformComponent>(entity).GetRotation().y;
        return WintersMath::DirectionFromYawXZ(
            visualYaw - GetDefaultChampionVisualYawOffset(champion));
    }

    void StartFateCallThrowAction(
        CWorld& world,
        const TickContext& tc,
        EntityID owner)
    {
        if (world.HasComponent<ActionStateComponent>(owner))
        {
            const auto& existing = world.GetComponent<ActionStateComponent>(owner);
            const bool_t bCommandActionAlreadyStarted =
                existing.actionId == static_cast<u16_t>(eActionStateId::SkillR) &&
                existing.startTick == tc.tickIndex &&
                existing.stage == 2u &&
                existing.sourceChampion == eChampion::KALISTA;
            if (bCommandActionAlreadyStarted)
                return;
        }

        bool_t bHadQueuedMove = false;
        u32_t queuedMoveSequence = 0u;
        Vec3 queuedMoveTarget{};
        Vec3 queuedMoveDirection{};
        if (world.HasComponent<ActionStateComponent>(owner))
        {
            const auto& previous = world.GetComponent<ActionStateComponent>(owner);
            bHadQueuedMove = previous.bHasQueuedMove;
            queuedMoveSequence = previous.queuedMoveSequence;
            queuedMoveTarget = previous.queuedMoveTarget;
            queuedMoveDirection = previous.queuedMoveDirection;
        }

        ActionStateComponent& action = StartActionState(
            world,
            owner,
            eActionStateId::SkillR,
            tc.tickIndex,
            2u);
        action.sourceChampion = eChampion::KALISTA;
        action.sourceSlot = static_cast<u8_t>(eSkillSlot::R);
        action.movePolicy = GameplayDefinitionQuery::ResolveSkillActionMovePolicy(
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::R),
            2u);
        action.lockEndTick = tc.tickIndex +
            GameplayDefinitionQuery::ResolveSkillActionLockTicks(
                world,
                owner,
                tc,
                eChampion::KALISTA,
                static_cast<u8_t>(eSkillSlot::R),
                2u);
        action.bHasQueuedMove = bHadQueuedMove;
        action.queuedMoveSequence = queuedMoveSequence;
        action.queuedMoveTarget = queuedMoveTarget;
        action.queuedMoveDirection = queuedMoveDirection;
    }

    bool_t BeginFateCallLaunch(
        CWorld& world,
        const TickContext& tc,
        EntityID owner,
        const Vec3& targetPosition)
    {
        if (!world.HasComponent<KalistaFateCallComponent>(owner) ||
            !world.HasComponent<TransformComponent>(owner))
        {
            return false;
        }

        auto& state = world.GetComponent<KalistaFateCallComponent>(owner);
        if (state.eStage != eKalistaFateCallStage::Carrying ||
            !IsAliveChampion(world, state.entityCarried))
        {
            return false;
        }

        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            owner,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            range = 12.f;

        state.vLaunchStart = world.GetComponent<TransformComponent>(
            state.entityCarried).GetPosition();
        Vec3 direction = WintersMath::DirectionXZ(
            state.vLaunchStart,
            targetPosition,
            ResolveChampionForward(world, owner));
        const f32_t requestedDistance = std::sqrt((std::max)(
            0.f,
            WintersMath::DistanceSqXZ(state.vLaunchStart, targetPosition)));
        const f32_t launchDistance = requestedDistance > 0.05f
            ? (std::min)(range, requestedDistance)
            : range;
        state.vLaunchEnd = Vec3{
            state.vLaunchStart.x + direction.x * launchDistance,
            state.vLaunchStart.y,
            state.vLaunchStart.z + direction.z * launchDistance };
        if (tc.pWalkable)
        {
            Vec3 clamped = state.vLaunchEnd;
            const f32_t carriedRadius =
                GameplayStateQuery::ResolveGameplayRadius(world, state.entityCarried);
            if (tc.pWalkable->TryClampMoveSegmentXZ(
                state.vLaunchStart,
                state.vLaunchEnd,
                carriedRadius,
                clamped))
            {
                state.vLaunchEnd = clamped;
            }
            else
            {
                state.vLaunchEnd = state.vLaunchStart;
            }

            f32_t surfaceY = state.vLaunchEnd.y;
            if (tc.pWalkable->TrySampleHeight(
                state.vLaunchEnd.x,
                state.vLaunchEnd.z,
                surfaceY))
            {
                state.vLaunchEnd.y = surfaceY;
            }
        }

        state.eStage = eKalistaFateCallStage::Launching;
        state.fLaunchElapsedSec = 0.f;
        if (world.HasComponent<KalistaFateCallCarriedComponent>(state.entityCarried))
        {
            world.GetComponent<KalistaFateCallCarriedComponent>(
                state.entityCarried).bHidden = false;
        }
        ApplyFateCallLock(
            world,
            tc,
            owner,
            state.entityCarried,
            state.fLaunchDurationSec + tc.fDt,
            false,
            false);

        StartFateCallThrowAction(world, tc, owner);

        if (world.HasComponent<ChampionComponent>(owner) &&
            world.GetComponent<ChampionComponent>(owner).id == eChampion::KALISTA &&
            world.HasComponent<SkillStateComponent>(owner))
        {
            auto& rSlot = world.GetComponent<SkillStateComponent>(owner).slots[
                static_cast<u8_t>(eSkillSlot::R)];
            rSlot.currentStage = 0u;
            rSlot.stageWindow = 0.f;
        }
        return true;
    }

    void StartFateCallLaunch(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        Vec3 target = ctx.pCommand
            ? ctx.pCommand->groundPos
            : world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        if (ctx.pCommand &&
            WintersMath::DistanceSqXZ(target,
                world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition()) <= 0.0025f)
        {
            const Vec3 forward = ResolveChampionForward(world, ctx.casterEntity);
            const Vec3 origin =
                world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
            target = Vec3{ origin.x + forward.x * 12.f, origin.y,
                origin.z + forward.z * 12.f };
        }
        BeginFateCallLaunch(world, *ctx.pTickCtx, ctx.casterEntity, target);
    }

    void OnR(GameplayHookContext& ctx)
    {
        const eChampion actualChampion =
            ctx.pWorld &&
            ctx.pWorld->HasComponent<ChampionComponent>(ctx.casterEntity)
                ? ctx.pWorld->GetComponent<ChampionComponent>(ctx.casterEntity).id
                : eChampion::NONE;
        if (ctx.pCommand && ctx.pCommand->itemId >= 2u &&
            actualChampion != eChampion::KALISTA)
            StartFateCallLaunch(ctx);
        else
            StartFateCallCarry(ctx);
    }
}

namespace KalistaGameSim
{
    bool_t TryLaunchBasicAttackProjectile(
        CWorld& world,
        const TickContext&,
        EntityID attacker,
        EntityID target,
        const DamageRequest& damageRequest)
    {
        if (!world.IsAlive(attacker) ||
            !world.IsAlive(target) ||
            !IsKalistaEntity(world, attacker) ||
            !world.HasComponent<TransformComponent>(attacker) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        Vec3 origin = world.GetComponent<TransformComponent>(
            attacker).GetPosition();
        origin.y += 1.f;
        const Vec3 targetPos = world.GetComponent<TransformComponent>(
            target).GetPosition();
        const Vec3 direction = WintersMath::DirectionXZ(
            origin,
            targetPos,
            Vec3{ 0.f, 0.f, 1.f });
        const f32_t damage = damageRequest.flatAmount != 0.f
            ? damageRequest.flatAmount
            : damageRequest.amount;

        return SpawnKalistaProjectile(
            world,
            attacker,
            target,
            eProjectileKind::KalistaBasicAttack,
            static_cast<u8_t>(eSkillSlot::BasicAttack),
            damageRequest.rank,
            origin,
            direction,
            30.f,
            (std::numeric_limits<f32_t>::max)(),
            0.6f,
            damage,
            damageRequest.type,
            eDamageSourceKind::BasicAttack,
            damageRequest.flags,
            damageRequest.adRatioOverride,
            damageRequest.bonusAdRatioOverride,
            damageRequest.apRatioOverride) != NULL_ENTITY;
    }

    void ApplyRendStackOnHit(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target)
    {
        if (source == NULL_ENTITY || target == NULL_ENTITY ||
            !world.IsAlive(target))
        {
            return;
        }

        const f32_t windowSec = ResolveKalistaESkillEffectParam(
            world,
            source,
            tc,
            eSkillEffectParamId::StackWindowSec,
            4.f);

        KalistaRendStackComponent stack{};
        if (world.HasComponent<KalistaRendStackComponent>(target))
            stack = world.GetComponent<KalistaRendStackComponent>(target);
        stack.sourceEntity = source;
        if (stack.stackCount < 255u)
            ++stack.stackCount;
        stack.fRemainingSec = windowSec;

        if (world.HasComponent<KalistaRendStackComponent>(target))
            world.GetComponent<KalistaRendStackComponent>(target) = stack;
        else
            world.AddComponent<KalistaRendStackComponent>(target, stack);
    }

    bool_t CanBeginOathswornContract(
        CWorld& world,
        EntityID kalista,
        EntityID ally)
    {
        if (!IsAliveChampion(world, kalista) ||
            !IsAliveChampion(world, ally) ||
            kalista == ally ||
            !world.HasComponent<ChampionComponent>(kalista) ||
            world.GetComponent<ChampionComponent>(kalista).id !=
                eChampion::KALISTA ||
            world.GetComponent<ChampionComponent>(kalista).team !=
                world.GetComponent<ChampionComponent>(ally).team ||
            world.HasComponent<KalistaOathswornComponent>(kalista) ||
            world.HasComponent<KalistaOathswornByComponent>(ally) ||
            !GameplayStateQuery::CanCast(world, kalista) ||
            !GameplayStateQuery::CanBeTargetedBy(world, kalista, ally) ||
            !world.HasComponent<TransformComponent>(kalista) ||
            !world.HasComponent<TransformComponent>(ally))
        {
            return false;
        }

        const Vec3 kalistaPosition =
            world.GetComponent<TransformComponent>(kalista).GetPosition();
        const Vec3 allyPosition =
            world.GetComponent<TransformComponent>(ally).GetPosition();
        return WintersMath::DistanceSqXZ(kalistaPosition, allyPosition) <=
            kKalistaOathswornContractRange * kKalistaOathswornContractRange;
    }

    bool_t TryBeginOathswornContract(
        CWorld& world,
        const TickContext& tc,
        EntityID kalista,
        EntityID ally)
    {
        if (!CanBeginOathswornContract(world, kalista, ally))
            return false;

        StatusEffectApplyDesc ritual{};
        ritual.effectId = eStatusEffectId::KalistaOathswornRitual;
        ritual.stackPolicy = eStatusStackPolicy::RefreshDuration;
        ritual.sourceEntity = kalista;
        ritual.stackGroup = GameplayStatus::MakeStatusStackGroup(
            eChampion::KALISTA,
            eSkillSlot::BasicAttack);
        ritual.stateFlags =
            kGameplayStateUntargetableFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        ritual.fDurationSec = 1.5f;
        ritual.fMoveSpeedMul = 1.f;
        if (!GameplayStatus::TryApplyStatusEffect(world, ally, ritual, tc))
            return false;

        KalistaOathswornComponent oath{};
        oath.entityAlly = ally;
        oath.eStage = eKalistaOathswornStage::Binding;
        oath.fRemainingSec = ritual.fDurationSec;
        world.AddComponent<KalistaOathswornComponent>(kalista, oath);
        world.AddComponent<KalistaOathswornByComponent>(
            ally,
            KalistaOathswornByComponent{ kalista });

        ClearMove(world, ally);
        StartActionState(world, ally, eActionStateId::DeathStart, tc.tickIndex);
        SetPoseState(world, ally, ePoseStateId::Idle, tc.tickIndex, true);
        return true;
    }

    bool_t TryLaunchCarriedAlly(
        CWorld& world,
        const TickContext& tc,
        EntityID carried,
        const Vec3& targetPosition)
    {
        if (!world.HasComponent<KalistaFateCallCarriedComponent>(carried))
            return false;

        const EntityID owner =
            world.GetComponent<KalistaFateCallCarriedComponent>(carried).entityOwner;
        const bool_t bValidOwner =
            owner != NULL_ENTITY &&
            world.IsAlive(owner) &&
            world.HasComponent<KalistaFateCallComponent>(owner) &&
            world.GetComponent<KalistaFateCallComponent>(owner).entityCarried ==
                carried;
        if (!bValidOwner)
        {
            GameplayStatus::RemoveStatusEffect(
                world,
                carried,
                eStatusEffectId::KalistaFateCallUntargetable,
                owner);
            world.RemoveComponent<KalistaFateCallCarriedComponent>(carried);
            return false;
        }

        if (world.GetComponent<KalistaFateCallComponent>(owner).eStage !=
            eKalistaFateCallStage::Carrying)
        {
            return false;
        }
        return BeginFateCallLaunch(world, tc, owner, targetPosition);
    }

    bool_t CanCastFateCall(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        u8_t stage)
    {
        if (!IsAliveChampion(world, caster))
            return false;

        const eChampion actualChampion =
            world.HasComponent<ChampionComponent>(caster)
                ? world.GetComponent<ChampionComponent>(caster).id
                : eChampion::NONE;

        if (stage >= 2u)
        {
            if (actualChampion == eChampion::KALISTA)
                return false;
            if (!world.HasComponent<KalistaFateCallComponent>(caster))
                return false;

            const auto& state = world.GetComponent<KalistaFateCallComponent>(caster);
            return state.eStage == eKalistaFateCallStage::Carrying &&
                IsAliveChampion(world, state.entityCarried);
        }

        if (world.HasComponent<KalistaFateCallComponent>(caster))
            return false;

        f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::KALISTA,
            static_cast<u8_t>(eSkillSlot::R));
        if (range <= 0.f)
            range = 12.f;
        const eTeam casterTeam = GameplayStateQuery::ResolveEntityTeam(world, caster);
        const EntityID ally = FindFateCallAlly(world, caster, casterTeam, range);
        if (ally == NULL_ENTITY)
            return false;
        if (!world.HasComponent<StatusEffectComponent>(ally))
            return true;

        const auto& effects = world.GetComponent<StatusEffectComponent>(ally);
        if (effects.count < kMaxStatusEffectInstances)
            return true;
        for (u8_t i = 0u; i < effects.count; ++i)
        {
            if (effects.active[i].effectId ==
                    eStatusEffectId::KalistaFateCallUntargetable &&
                effects.active[i].sourceEntity == caster)
            {
                return true;
            }
        }
        return false;
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KALISTA, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KALISTA, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KALISTA, GameplayHookVariant::E_OnCastAccepted), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KALISTA, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[KalistaSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        // Rend 스택 유지 시간 만료 처리.
        std::vector<EntityID> expiredRend;
        world.ForEach<KalistaRendStackComponent>(
            std::function<void(EntityID, KalistaRendStackComponent&)>(
                [&](EntityID entity, KalistaRendStackComponent& stack)
                {
                    stack.fRemainingSec =
                        (std::max)(0.f, stack.fRemainingSec - tc.fDt);
                    if (stack.fRemainingSec <= 0.f)
                        expiredRend.push_back(entity);
                }));
        for (EntityID entity : expiredRend)
        {
            if (world.IsAlive(entity) &&
                world.HasComponent<KalistaRendStackComponent>(entity))
            {
                world.RemoveComponent<KalistaRendStackComponent>(entity);
            }
        }

        std::vector<EntityID> invalidOaths;
        world.ForEach<KalistaOathswornComponent>(
            std::function<void(EntityID, KalistaOathswornComponent&)>(
                [&](EntityID owner, KalistaOathswornComponent& oath)
                {
                    if (oath.entityAlly == NULL_ENTITY ||
                        !world.IsAlive(oath.entityAlly) ||
                        !world.HasComponent<KalistaOathswornByComponent>(
                            oath.entityAlly) ||
                        world.GetComponent<KalistaOathswornByComponent>(
                            oath.entityAlly).entityKalista != owner)
                    {
                        invalidOaths.push_back(owner);
                        return;
                    }

                    if (oath.eStage != eKalistaOathswornStage::Binding)
                        return;

                    oath.fRemainingSec = (std::max)(
                        0.f,
                        oath.fRemainingSec - tc.fDt);
                    if (oath.fRemainingSec > 0.f)
                        return;

                    oath.eStage = eKalistaOathswornStage::Bound;
                    GameplayStatus::RemoveStatusEffect(
                        world,
                        oath.entityAlly,
                        eStatusEffectId::KalistaOathswornRitual,
                        owner);
                    StartActionState(
                        world,
                        oath.entityAlly,
                        eActionStateId::None,
                        tc.tickIndex);
                    SetPoseState(
                        world,
                        oath.entityAlly,
                        ePoseStateId::Idle,
                        tc.tickIndex,
                        true);
                }));

        for (EntityID owner : invalidOaths)
        {
            if (!world.HasComponent<KalistaOathswornComponent>(owner))
                continue;
            const EntityID ally =
                world.GetComponent<KalistaOathswornComponent>(owner).entityAlly;
            if (ally != NULL_ENTITY && world.IsAlive(ally))
            {
                GameplayStatus::RemoveStatusEffect(
                    world,
                    ally,
                    eStatusEffectId::KalistaOathswornRitual,
                    owner);
                if (world.HasComponent<KalistaOathswornByComponent>(ally))
                    world.RemoveComponent<KalistaOathswornByComponent>(ally);
            }
            world.RemoveComponent<KalistaOathswornComponent>(owner);
        }

        std::vector<EntityID> orphanedOathswornAllies;
        world.ForEach<KalistaOathswornByComponent>(
            std::function<void(EntityID, KalistaOathswornByComponent&)>(
                [&](EntityID ally, KalistaOathswornByComponent& reverseBond)
                {
                    const EntityID owner = reverseBond.entityKalista;
                    if (owner == NULL_ENTITY ||
                        !world.IsAlive(owner) ||
                        !world.HasComponent<KalistaOathswornComponent>(owner) ||
                        world.GetComponent<KalistaOathswornComponent>(
                            owner).entityAlly != ally)
                    {
                        orphanedOathswornAllies.push_back(ally);
                    }
                }));

        for (EntityID ally : orphanedOathswornAllies)
        {
            if (!world.IsAlive(ally) ||
                !world.HasComponent<KalistaOathswornByComponent>(ally))
            {
                continue;
            }

            const EntityID formerOwner =
                world.GetComponent<KalistaOathswornByComponent>(
                    ally).entityKalista;
            GameplayStatus::RemoveStatusEffect(
                world,
                ally,
                eStatusEffectId::KalistaOathswornRitual,
                formerOwner);
            world.RemoveComponent<KalistaOathswornByComponent>(ally);
            StartActionState(world, ally, eActionStateId::None, tc.tickIndex);
            SetPoseState(world, ally, ePoseStateId::Idle, tc.tickIndex, true);
        }

        std::vector<EntityID> completedFateCalls;
        world.ForEach<KalistaFateCallComponent>(
            std::function<void(EntityID, KalistaFateCallComponent&)>(
                [&](EntityID owner, KalistaFateCallComponent& state)
                {
                    if (!IsAliveChampion(world, owner) ||
                        !IsAliveChampion(world, state.entityCarried))
                    {
                        completedFateCalls.push_back(owner);
                        return;
                    }

                    auto& carriedTransform = world.GetComponent<TransformComponent>(
                        state.entityCarried);
                    ClearMove(world, state.entityCarried);

                    if (state.eStage == eKalistaFateCallStage::Pulling)
                    {
                        ApplyFateCallLock(
                            world,
                            tc,
                            owner,
                            state.entityCarried,
                            tc.fDt * 2.f,
                            false,
                            false);
                        state.fPullElapsedSec += tc.fDt;
                        const f32_t pullDurationSec = (std::max)(
                            state.fPullDurationSec,
                            tc.fDt);
                        const f32_t t = std::clamp(
                            state.fPullElapsedSec / pullDurationSec,
                            0.f,
                            1.f);
                        const Vec3 ownerPosition =
                            world.GetComponent<TransformComponent>(owner).GetPosition();
                        carriedTransform.SetPosition(Vec3{
                            state.vPullStart.x +
                                (ownerPosition.x - state.vPullStart.x) * t,
                            state.vPullStart.y +
                                (ownerPosition.y - state.vPullStart.y) * t,
                            state.vPullStart.z +
                                (ownerPosition.z - state.vPullStart.z) * t });

                        if (t >= 1.f)
                        {
                            state.eStage = eKalistaFateCallStage::Carrying;
                            if (world.HasComponent<ChampionAIComponent>(
                                state.entityCarried))
                            {
                                state.fRemainingSec = (std::min)(
                                    state.fRemainingSec,
                                    3.f);
                            }
                            if (world.HasComponent<KalistaFateCallCarriedComponent>(
                                state.entityCarried))
                            {
                                world.GetComponent<KalistaFateCallCarriedComponent>(
                                    state.entityCarried).bHidden = true;
                            }
                            ApplyFateCallLock(
                                world,
                                tc,
                                owner,
                                state.entityCarried,
                                tc.fDt * 2.f,
                                true,
                                false);
                        }
                        return;
                    }

                    if (state.eStage == eKalistaFateCallStage::Carrying)
                    {
                        const bool_t bCarriedByAI =
                            world.HasComponent<ChampionAIComponent>(
                                state.entityCarried);
                        if (bCarriedByAI)
                        {
                            state.fRemainingSec = (std::min)(
                                state.fRemainingSec,
                                3.f);
                            state.fRemainingSec = (std::max)(
                                0.f,
                                state.fRemainingSec - tc.fDt);
                            if (state.fRemainingSec <= 0.f)
                            {
                                const Vec3 ownerPosition =
                                    world.GetComponent<TransformComponent>(owner).GetPosition();
                                const Vec3 forward = ResolveChampionForward(world, owner);
                                BeginFateCallLaunch(
                                    world,
                                    tc,
                                    owner,
                                    Vec3{
                                        ownerPosition.x + forward.x * 12.f,
                                        ownerPosition.y,
                                        ownerPosition.z + forward.z * 12.f });
                                return;
                            }
                        }

                        ApplyFateCallLock(
                            world,
                            tc,
                            owner,
                            state.entityCarried,
                            tc.fDt * 2.f,
                            true,
                            false);
                        if (world.HasComponent<KalistaFateCallCarriedComponent>(
                            state.entityCarried))
                        {
                            world.GetComponent<KalistaFateCallCarriedComponent>(
                                state.entityCarried).bHidden = true;
                        }
                        carriedTransform.SetPosition(
                            world.GetComponent<TransformComponent>(owner).GetPosition());
                        return;
                    }

                    if (state.eStage != eKalistaFateCallStage::Launching)
                    {
                        completedFateCalls.push_back(owner);
                        return;
                    }

                    ApplyFateCallLock(
                        world,
                        tc,
                        owner,
                        state.entityCarried,
                        tc.fDt * 2.f,
                        false,
                        false);
                    if (world.HasComponent<KalistaFateCallCarriedComponent>(
                        state.entityCarried))
                    {
                        world.GetComponent<KalistaFateCallCarriedComponent>(
                            state.entityCarried).bHidden = false;
                    }
                    const Vec3 previous = carriedTransform.GetPosition();
                    state.fLaunchElapsedSec += tc.fDt;
                    const f32_t launchDurationSec = (std::max)(
                        state.fLaunchDurationSec,
                        tc.fDt);
                    const f32_t t = std::clamp(
                        state.fLaunchElapsedSec / launchDurationSec,
                        0.f,
                        1.f);
                    const Vec3 next{
                        state.vLaunchStart.x +
                            (state.vLaunchEnd.x - state.vLaunchStart.x) * t,
                        state.vLaunchStart.y +
                            (state.vLaunchEnd.y - state.vLaunchStart.y) * t,
                        state.vLaunchStart.z +
                            (state.vLaunchEnd.z - state.vLaunchStart.z) * t };

                    carriedTransform.SetPosition(next);
                    const Vec3 launchDirection{
                        state.vLaunchEnd.x - state.vLaunchStart.x,
                        0.f,
                        state.vLaunchEnd.z - state.vLaunchStart.z };
                    if (launchDirection.x * launchDirection.x +
                        launchDirection.z * launchDirection.z > 0.0001f)
                    {
                        const Vec3 rotation = carriedTransform.GetRotation();
                        const eChampion carriedChampion =
                            world.GetComponent<ChampionComponent>(
                                state.entityCarried).id;
                        carriedTransform.SetRotation(Vec3{
                            rotation.x,
                            ResolveChampionVisualYawNear(
                                carriedChampion,
                                launchDirection,
                                rotation.y),
                            rotation.z });
                    }

                    const std::vector<EntityID> targets =
                        GameplayStateQuery::CollectEnemyMobileUnitsInSegment(
                            world,
                            owner,
                            previous,
                            next,
                            state.fCollisionRadius);
                    if (!targets.empty() || t >= 1.f)
                    {
                        for (EntityID target : targets)
                        {
                            GameplayStatus::ApplyAirborne(
                                world,
                                tc,
                                target,
                                owner,
                                eChampion::KALISTA,
                                eSkillSlot::R,
                                state.fAirborneDurationSec,
                                state.fAirborneArcHeight);
                        }
                        completedFateCalls.push_back(owner);
                    }
                }));

        for (EntityID owner : completedFateCalls)
        {
            if (!world.HasComponent<KalistaFateCallComponent>(owner))
                continue;
            const KalistaFateCallComponent state =
                world.GetComponent<KalistaFateCallComponent>(owner);
            ReleaseFateCall(world, owner, state);
            world.RemoveComponent<KalistaFateCallComponent>(owner);
        }

        std::vector<EntityID> orphanedCarriedAllies;
        world.ForEach<KalistaFateCallCarriedComponent>(
            std::function<void(EntityID, KalistaFateCallCarriedComponent&)>(
                [&](EntityID carried, KalistaFateCallCarriedComponent& reverseCarry)
                {
                    const EntityID owner = reverseCarry.entityOwner;
                    const bool_t bValidOwner =
                        owner != NULL_ENTITY &&
                        world.IsAlive(owner) &&
                        world.HasComponent<KalistaFateCallComponent>(owner) &&
                        world.GetComponent<KalistaFateCallComponent>(
                            owner).entityCarried == carried;
                    if (!bValidOwner)
                        orphanedCarriedAllies.push_back(carried);
                }));

        for (EntityID carried : orphanedCarriedAllies)
        {
            if (!world.IsAlive(carried) ||
                !world.HasComponent<KalistaFateCallCarriedComponent>(carried))
            {
                continue;
            }

            const EntityID formerOwner =
                world.GetComponent<KalistaFateCallCarriedComponent>(
                    carried).entityOwner;
            GameplayStatus::RemoveStatusEffect(
                world,
                carried,
                eStatusEffectId::KalistaFateCallUntargetable,
                formerOwner);
            world.RemoveComponent<KalistaFateCallCarriedComponent>(carried);
            ClearMove(world, carried);
            StartActionState(world, carried, eActionStateId::None, tc.tickIndex);
            SetPoseState(world, carried, ePoseStateId::Idle, tc.tickIndex, true);
        }

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
                            state.radius,
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
        {
            if (tc.pEntityMap && world.HasComponent<NetEntityIdComponent>(entity))
            {
                const NetEntityId netId =
                    world.GetComponent<NetEntityIdComponent>(entity).netId;
                if (netId != NULL_NET_ENTITY)
                    tc.pEntityMap->Unbind(netId);
            }
            world.DestroyEntity(entity);
        }
    }
}
