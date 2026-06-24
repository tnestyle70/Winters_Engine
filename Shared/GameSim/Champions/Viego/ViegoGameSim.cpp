#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"

#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "ECS/Components/SpatialAgentComponent.h"

#include "Shared/GameSim/Components/AreaAuraComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
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
    constexpr f32_t kViegoQRange = 5.5f;
    constexpr f32_t kViegoQRadius = 0.9f;
    constexpr f32_t kViegoQDamage = 65.f;

    constexpr f32_t kViegoWDashRange = 4.f;
    constexpr f32_t kViegoDashDurationSec = 0.26f;
    constexpr f32_t kViegoWDamage = 55.f;
    constexpr f32_t kViegoWStunDurationSec = 0.75f;

    constexpr f32_t kViegoEMistRadius = 6.0f;
    constexpr f32_t kViegoEMistTickSec = 0.1f;
    constexpr f32_t kViegoEMistRefreshSec = 0.25f;

    constexpr f32_t kViegoRRange = 6.0f;
    constexpr f32_t kViegoRRadius = 2.0f;
    constexpr f32_t kViegoRDamage = 150.f;
    constexpr f32_t kViegoRDashDurationSec = 0.18f;
    constexpr f32_t kViegoRSlowDurationSec = 1.0f;
    constexpr f32_t kViegoRSlowMoveSpeedMul = 0.60f;

    constexpr f32_t kViegoSoulLifetimeSec = 5.f;
    constexpr f32_t kViegoSoulRadius = 0.85f;

    struct ViegoDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t durationSec = kViegoRDashDurationSec;
    };

    ViegoSimComponent& EnsureViegoState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<ViegoSimComponent>(caster))
            world.AddComponent<ViegoSimComponent>(caster, ViegoSimComponent{});

        return world.GetComponent<ViegoSimComponent>(caster);
    }

    bool_t IsValidPossessionChampion(eChampion champion)
    {
        return champion != eChampion::NONE && champion != eChampion::END;
    }

    void ClearViegoPossession(CWorld& world, EntityID caster, ViegoSimComponent& state)
    {
        state.bPossessionActive = false;
        state.bPossessionPending = false;
        state.pendingPossessionChampion = eChampion::END;
        state.pendingPossessedTarget = NULL_ENTITY;
        state.possessionApplyTimerSec = 0.f;
        state.possessedTarget = NULL_ENTITY;
        state.possessionTimerSec = 0.f;

        if (world.HasComponent<FormOverrideComponent>(caster))
            world.RemoveComponent<FormOverrideComponent>(caster);
    }

    void ApplyViegoPossession(CWorld& world, EntityID caster, ViegoSimComponent& state)
    {
        if (!IsValidPossessionChampion(state.pendingPossessionChampion))
        {
            ClearViegoPossession(world, caster, state);
            return;
        }

        const f32_t durationSec = state.possessionDurationSec > 0.f
            ? state.possessionDurationSec
            : 5.f;

        FormOverrideComponent form{};
        form.baseChampion = eChampion::VIEGO;
        form.visualChampion = state.pendingPossessionChampion;
        form.skillChampion = state.pendingPossessionChampion;
        form.skillSlotMask = FormOverrideComponent{}.skillSlotMask;
        form.fRemainingSec = durationSec;
        form.bActive = true;

        if (world.HasComponent<FormOverrideComponent>(caster))
            world.GetComponent<FormOverrideComponent>(caster) = form;
        else
            world.AddComponent<FormOverrideComponent>(caster, form);

        state.bPossessionActive = true;
        state.bPossessionPending = false;
        state.possessedTarget = state.pendingPossessedTarget;
        state.possessionTimerSec = durationSec;
        state.pendingPossessionChampion = eChampion::END;
        state.pendingPossessedTarget = NULL_ENTITY;
        state.possessionApplyTimerSec = 0.f;
    }

    f32_t ResolveViegoSkillRange(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        f32_t fallbackValue)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return GameplayDefinitionQuery::ResolveSkillRange(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::VIEGO,
            static_cast<u8_t>(slot));
    }

    f32_t ResolveViegoSkillEffectParam(
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
            eChampion::VIEGO,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    constexpr u16_t MakeStatusStackGroup(eChampion champion, u8_t slot)
    {
        return static_cast<u16_t>(
            (static_cast<u32_t>(champion) << 8) | static_cast<u32_t>(slot));
    }

    Vec3 ResolveDirection(const GameplayHookContext& ctx)
    {
        if (ctx.pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(ctx.pCommand->direction);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;

            if (ctx.pWorld && ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            {
                const Vec3 origin =
                    ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
                dir = WintersMath::NormalizeXZ(Vec3{
                    ctx.pCommand->groundPos.x - origin.x,
                    0.f,
                    ctx.pCommand->groundPos.z - origin.z
                });
                if (dir.x != 0.f || dir.z != 0.f)
                    return dir;
            }
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
            ResolveChampionVisualYawNear(eChampion::VIEGO, dir, rot.y),
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
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::VIEGO) << 8) | slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    void EnqueueLineDamage(
        CWorld& world,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& start,
        const Vec3& end,
        f32_t radius,
        f32_t amount,
        u8_t slot,
        u8_t rank)
    {
        const f32_t radiusSq = radius * radius;
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == source || champion.team == sourceTeam)
                        return;
                    if (WintersMath::DistanceSqPointToSegmentXZ(transform.GetPosition(), start, end) > radiusSq)
                        return;

                    EnqueuePhysicalDamage(world, source, entity, sourceTeam, amount, slot, rank);
                }));
    }

    void ApplyLineStun(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& start,
        const Vec3& end,
        f32_t radius,
        f32_t stunDurationSec)
    {
        const f32_t radiusSq = radius * radius;
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == source || champion.team == sourceTeam)
                        return;
                    if (WintersMath::DistanceSqPointToSegmentXZ(transform.GetPosition(), start, end) > radiusSq)
                        return;

                    GameplayStatus::ApplyStun(
                        world,
                        tc,
                        entity,
                        source,
                        eChampion::VIEGO,
                        eSkillSlot::W,
                        stunDurationSec);
                }));
    }

    void EnqueueCircleDamage(
        CWorld& world,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& origin,
        f32_t radius,
        f32_t amount,
        u8_t slot,
        u8_t rank)
    {
        const f32_t radiusSq = radius * radius;
        std::vector<EntityID> targets;
        targets.reserve(8);
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == source || champion.team == sourceTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(transform.GetPosition(), origin) <= radiusSq)
                        targets.push_back(entity);
                }));

        for (EntityID target : targets)
            EnqueuePhysicalDamage(world, source, target, sourceTeam, amount, slot, rank);
    }

    void ApplyCircleSlow(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& origin,
        f32_t radius,
        f32_t slowDurationSec,
        f32_t moveSpeedMul)
    {
        const f32_t radiusSq = radius * radius;
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (target == source || champion.team == sourceTeam)
                        return;
                    if (WintersMath::DistanceSqXZ(transform.GetPosition(), origin) > radiusSq)
                        return;

                    GameplayStatus::ApplySlow(
                        world,
                        tc,
                        target,
                        source,
                        eChampion::VIEGO,
                        eSkillSlot::R,
                        slowDurationSec,
                        moveSpeedMul);
                }));
    }

    EntityID SpawnProjectile(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& direction,
        eProjectileKind kind,
        f32_t speed,
        f32_t maxDistance,
        f32_t radius,
        f32_t damage,
        u8_t slot,
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
        projectile.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::VIEGO) << 8) | slot);
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

    void StartDash(CWorld& world, EntityID caster, const Vec3& end,
        f32_t fDurationSec)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return;

        ViegoDashComponent dash{};
        dash.start = world.GetComponent<TransformComponent>(caster).GetPosition();
        dash.end = end;
        dash.durationSec = fDurationSec;

        if (world.HasComponent<ViegoDashComponent>(caster))
            world.GetComponent<ViegoDashComponent>(caster) = dash;
        else
            world.AddComponent<ViegoDashComponent>(caster, dash);

        ClearMove(world, caster);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = ResolveDirection(ctx);
        const f32_t range = ResolveViegoSkillRange(ctx, eSkillSlot::Q, kViegoQRange);
        const f32_t radius = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Radius,
            kViegoQRadius);
        const f32_t damage = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage,
            kViegoQDamage);
        const Vec3 end{ origin.x + dir.x * range, origin.y, origin.z + dir.z * range };
        RotateToward(*ctx.pWorld, ctx.casterEntity, dir);
        EnqueueLineDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.casterTeam,
            origin,
            end,
            radius,
            damage,
            static_cast<u8_t>(eSkillSlot::Q),
            ctx.skillRank);

        std::cout << "[ViegoSim] Q line caster=" << ctx.casterEntity << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        const Vec3& vDir = ResolveDirection(ctx);

        RotateToward(*ctx.pWorld, ctx.casterEntity, vDir);

        const bool_t bReleaseStage = ctx.pCommand && ctx.pCommand->itemId >= 2u;
        if (!bReleaseStage)
            return;
        if (!ctx.pTickCtx)
            return;

        const Vec3& vOrigin =
            ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const f32_t dashRange = ResolveViegoSkillRange(ctx, eSkillSlot::W, kViegoWDashRange);
        const f32_t dashDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::DashDurationSec,
            kViegoDashDurationSec);
        const f32_t radius = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Radius,
            0.75f);
        const f32_t damage = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::BaseDamage,
            kViegoWDamage);
        const f32_t stunDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::StunDurationSec,
            kViegoWStunDurationSec);

        const Vec3 vEnd
        {
            vOrigin.x + vDir.x * dashRange,
            vOrigin.y,
            vOrigin.z + vDir.z * dashRange
        };

        StartDash(*ctx.pWorld, ctx.casterEntity, vEnd, dashDurationSec);

        EnqueueLineDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.casterTeam,
            vOrigin,
            vEnd,
            radius,
            damage,
            static_cast<u8_t>(eSkillSlot::W),
            ctx.skillRank
        );
        ApplyLineStun(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterTeam,
            vOrigin,
            vEnd,
            radius,
            stunDurationSec);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        ViegoSimComponent& state = EnsureViegoState(*ctx.pWorld, ctx.casterEntity);
        state.bMistActive = false;
        state.mistTimerSec = 0.f;
        const f32_t mistRadius = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::Radius,
            kViegoEMistRadius);
        const f32_t mistDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::EffectDurationSec,
            state.mistDurationSec);
        const f32_t tickIntervalSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::TickIntervalSec,
            kViegoEMistTickSec);
        const f32_t refreshDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::RefreshDurationSec,
            kViegoEMistRefreshSec);

        AreaAuraComponent aura{};
        aura.owner = ctx.casterEntity;
        aura.ownerTeam = ctx.casterTeam;
        aura.shape = eAreaAuraShape::Circle;
        aura.applyMode = eAreaAuraApplyMode::OwnerOnly;
        aura.vCenter = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        aura.fRadius = mistRadius;
        aura.fRemainingSec = mistDurationSec;
        aura.fTickIntervalSec = tickIntervalSec;
        aura.fTickAccumulatorSec = tickIntervalSec;
        aura.status.effectId = eStatusEffectId::ViegoMist;
        aura.status.stackPolicy = eStatusStackPolicy::RefreshDuration;
        aura.status.sourceEntity = ctx.casterEntity;
        aura.status.stackGroup = MakeStatusStackGroup(
            eChampion::VIEGO,
            static_cast<u8_t>(eSkillSlot::E));
        aura.status.stateFlags = kGameplayStateInvisibleFlag;
        aura.status.fDurationSec = refreshDurationSec;
        aura.status.fMoveSpeedMul = 1.f;
        aura.bApplyStatus = true;

        const EntityID auraEntity = ctx.pWorld->CreateEntity();
        ctx.pWorld->AddComponent<AreaAuraComponent>(auraEntity, aura);

        std::cout << "[ViegoSim] E mist caster=" << ctx.casterEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        ViegoSimComponent& viegoState = EnsureViegoState(*ctx.pWorld, ctx.casterEntity);
        if (viegoState.bPossessionActive ||
            viegoState.bPossessionPending ||
            ctx.pWorld->HasComponent<FormOverrideComponent>(ctx.casterEntity))
        {
            ClearViegoPossession(*ctx.pWorld, ctx.casterEntity, viegoState);
        }

        const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = ResolveDirection(ctx);
        const f32_t range = ResolveViegoSkillRange(ctx, eSkillSlot::R, kViegoRRange);
        const f32_t radius = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Radius,
            kViegoRRadius);
        const f32_t damage = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage,
            kViegoRDamage);
        const f32_t dashDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::DashDurationSec,
            kViegoRDashDurationSec);
        const f32_t slowDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::SlowDurationSec,
            kViegoRSlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::MoveSpeedMul,
            kViegoRSlowMoveSpeedMul);
        const Vec3 end{ origin.x + dir.x * range, origin.y, origin.z + dir.z * range };
        RotateToward(*ctx.pWorld, ctx.casterEntity, dir);
        StartDash(*ctx.pWorld, ctx.casterEntity, end, dashDurationSec);
        EnqueueCircleDamage(
            *ctx.pWorld,
            ctx.casterEntity,
            ctx.casterTeam,
            end,
            radius,
            damage,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank);
        ApplyCircleSlow(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterTeam,
            end,
            radius,
            slowDurationSec,
            slowMoveSpeedMul);

        std::cout << "[ViegoSim] R dash caster=" << ctx.casterEntity << "\n";
    }
}

namespace ViegoGameSim
{
    void TrySpawnSoulForKill(CWorld& world, const TickContext& tc,
        EntityID, EntityID deadChampion)
    {
        if (deadChampion == NULL_ENTITY ||
            world.HasComponent<ViegoSoulComponent>(deadChampion) ||
            !world.HasComponent<ChampionComponent>(deadChampion) ||
            !world.HasComponent<TransformComponent>(deadChampion))
            return;

        const auto& dead = world.GetComponent<ChampionComponent>(deadChampion);
        eTeam eligibleTeam = eTeam::TEAM_END;

        world.ForEach<ChampionComponent>(
            std::function<void(EntityID, ChampionComponent&)>(
                [&](EntityID entity, ChampionComponent& champion)
                {
                    if (eligibleTeam != eTeam::TEAM_END ||
                        champion.id != eChampion::VIEGO ||
                        champion.team == dead.team)
                        return;

                    if (world.HasComponent<HealthComponent>(entity))
                    {
                        const auto& hp = world.GetComponent<HealthComponent>(entity);
                        if (hp.bIsDead || hp.fCurrent <= 0.f)
                            return;
                    }

                    eligibleTeam = champion.team;
                }
            )
        );
        if (eligibleTeam == eTeam::TEAM_END)
            return;

        const EntityID soulEntity = world.CreateEntity();

        ViegoSoulComponent soul{};
        soul.deadChampion = deadChampion;
        soul.champion = dead.id;
        soul.eligibleTeam = eligibleTeam;
        soul.fRemainingSec = kViegoSoulLifetimeSec;
        world.AddComponent<ViegoSoulComponent>(soulEntity, soul);

        ChampionComponent soulChampion{};
        soulChampion.id = dead.id;
        soulChampion.team = dead.team;
        soulChampion.hp = 1.f;
        soulChampion.maxHp = 1.f;
        world.AddComponent<ChampionComponent>(soulEntity, soulChampion);

        HealthComponent hp{};
        hp.fCurrent = 1.f;
        hp.fMaximum = 1.f;
        world.AddComponent<HealthComponent>(soulEntity, hp);

        TransformComponent tf = world.GetComponent<TransformComponent>(deadChampion);
        world.AddComponent<TransformComponent>(soulEntity, tf);

        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Champion;
        spatial.team = static_cast<u8_t>(dead.team);
        spatial.radius = kViegoSoulRadius;
        world.AddComponent<SpatialAgentComponent>(soulEntity, spatial);

        world.AddComponent<TargetableTag>(soulEntity);

        SetPoseState(world, soulEntity, ePoseStateId::Idle, tc.tickIndex, true);

        if (tc.pEntityMap)
        {
            NetEntityIdComponent net{};
            net.netId = tc.pEntityMap->IssueNew(soulEntity);
            world.AddComponent<NetEntityIdComponent>(soulEntity, net);
        }
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> expiredSouls;
        world.ForEach<ViegoSoulComponent>(
            std::function<void(EntityID, ViegoSoulComponent&)>(
                [&](EntityID entity, ViegoSoulComponent& soul)
                {
                    soul.fRemainingSec = std::max(0.f, soul.fRemainingSec - tc.fDt);
                    if (world.HasComponent<HealthComponent>(entity))
                    {
                        const auto& hp = world.GetComponent<HealthComponent>(entity);
                        if (hp.bIsDead || hp.fCurrent <= 0.f)
                        {
                            expiredSouls.push_back(entity);
                            return;
                        }
                    }
                    if (soul.fRemainingSec <= 0.f)
                        expiredSouls.push_back(entity);
                }
            )
        );
        for (EntityID entity : expiredSouls)
            world.DestroyEntity(entity);


        std::vector<EntityID> finishedDashes;
        world.ForEach<ViegoDashComponent, TransformComponent>(
            std::function<void(EntityID, ViegoDashComponent&, TransformComponent&)>(
                [&](EntityID entity, ViegoDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

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
            world.RemoveComponent<ViegoDashComponent>(entity);

        world.ForEach<ViegoSimComponent>(
            std::function<void(EntityID, ViegoSimComponent&)>(
                [&](EntityID entity, ViegoSimComponent& state)
                {
                    if (state.bMistActive)
                    {
                        state.mistTimerSec = std::max(0.f, state.mistTimerSec - tc.fDt);
                        if (state.mistTimerSec <= 0.f)
                            state.bMistActive = false;
                    }

                    if (state.bPossessionPending)
                    {
                        state.possessionApplyTimerSec =
                            std::max(0.f, state.possessionApplyTimerSec - tc.fDt);
                        if (state.possessionApplyTimerSec <= 0.f)
                            ApplyViegoPossession(world, entity, state);
                    }

                    if (state.bPossessionActive)
                    {
                        state.possessionTimerSec = std::max(0.f, state.possessionTimerSec - tc.fDt);
                        if (state.possessionTimerSec <= 0.f)
                        {
                            ClearViegoPossession(world, entity, state);
                        }
                    }
                }));
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::VIEGO, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::VIEGO, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::VIEGO, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::VIEGO, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[ViegoSim] hooks registered\n";
    }
}
