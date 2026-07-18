#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/ZedSimComponent.h"
#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

namespace
{
    constexpr u8_t kZedRSourceShadowStage = 3u;
    constexpr u8_t kZedRLethalMarkerShowStage = 4u;
    constexpr u8_t kZedRLethalMarkerHideStage = 5u;
    u16_t CastFrameVariantForZedSlot(u8_t slot)
    {
        switch (static_cast<eSkillSlot>(slot))
        {
        case eSkillSlot::Q: return GameplayHookVariant::Q_CastFrame;
        case eSkillSlot::W: return GameplayHookVariant::W_CastFrame;
        case eSkillSlot::E: return GameplayHookVariant::E_CastFrame;
        case eSkillSlot::R: return GameplayHookVariant::R_CastFrame;
        default: return GameplayHookVariant::BA_CastFrame;
        }
    }

    u16_t MakeZedSkillId(u8_t slot)
    {
        return static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::ZED) << 8) |
            static_cast<u32_t>(slot));
    }

    ZedSimComponent& EnsureZedState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<ZedSimComponent>(caster))
            world.AddComponent<ZedSimComponent>(caster, ZedSimComponent{});

        return world.GetComponent<ZedSimComponent>(caster);
    }

    f32_t ResolveZedSkillRange(
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
            eChampion::ZED,
            static_cast<u8_t>(slot));
    }

    f32_t ResolveZedSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            *ctx.pWorld,
            ctx.casterEntity,
            *ctx.pTickCtx,
            eChampion::ZED,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveZedSkillEffectParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue)
    {
        return GameplayDefinitionQuery::ResolveSkillEffectParam(
            world,
            caster,
            tc,
            eChampion::ZED,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    eTeam ResolveTeam(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<ChampionComponent>(entity))
            return world.GetComponent<ChampionComponent>(entity).team;
        if (entity != NULL_ENTITY && world.HasComponent<MinionComponent>(entity))
            return world.GetComponent<MinionComponent>(entity).team;
        if (entity != NULL_ENTITY && world.HasComponent<StructureComponent>(entity))
            return world.GetComponent<StructureComponent>(entity).team;
        return eTeam::Neutral;
    }

    Vec3 ResolvePosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();

        return Vec3{};
    }

    Vec3 ResolveForward(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<TransformComponent>(caster))
            return Vec3{ 0.f, 0.f, 1.f };

        const f32_t modelYaw = world.GetComponent<TransformComponent>(caster).GetRotation().y;
        return WintersMath::DirectionFromYawXZ(
            modelYaw - GetDefaultChampionVisualYawOffset(eChampion::ZED));
    }

    ZedShadowState* ResolveShadowState(ZedSimComponent& state, eSkillSlot sourceSlot)
    {
        switch (sourceSlot)
        {
        case eSkillSlot::W:
            return &state.wShadow;
        case eSkillSlot::R:
            return &state.rShadow;
        default:
            return nullptr;
        }
    }

    const ZedShadowState* ResolveShadowState(const ZedSimComponent& state, eSkillSlot sourceSlot)
    {
        switch (sourceSlot)
        {
        case eSkillSlot::W:
            return &state.wShadow;
        case eSkillSlot::R:
            return &state.rShadow;
        default:
            return nullptr;
        }
    }

    void SetZedShadowState(
        CWorld& world,
        EntityID caster,
        eSkillSlot sourceSlot,
        const Vec3& position,
        const Vec3& direction,
        f32_t durationSec)
    {
        ZedSimComponent& state = EnsureZedState(world, caster);
        ZedShadowState* pShadow = ResolveShadowState(state, sourceSlot);
        if (!pShadow)
            return;

        pShadow->bActive = true;
        pShadow->vPosition = position;
        pShadow->vDirection = WintersMath::NormalizeXZ(
            direction,
            ResolveForward(world, caster),
            0.0001f);
        pShadow->fRemainingSec = durationSec;
    }

    Vec3 ResolveEntityForward(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return Vec3{ 0.f, 0.f, 1.f };

        eChampion championId = eChampion::NONE;
        if (world.HasComponent<ChampionComponent>(entity))
            championId = world.GetComponent<ChampionComponent>(entity).id;

        const f32_t modelYaw = world.GetComponent<TransformComponent>(entity).GetRotation().y;
        return WintersMath::DirectionFromYawXZ(
            modelYaw - GetDefaultChampionVisualYawOffset(championId));
    }

    Vec3 ResolveDeathMarkLandingPosition(
        CWorld& world,
        const TickContext* pTickCtx,
        EntityID caster,
        EntityID target,
        const Vec3& fallbackDirection,
        f32_t behindPadding)
    {
        const Vec3 targetPos = ResolvePosition(world, target);
        Vec3 behindDir = ResolveEntityForward(world, target);
        behindDir = WintersMath::NormalizeXZ(
            Vec3{ -behindDir.x, 0.f, -behindDir.z },
            Vec3{ -fallbackDirection.x, 0.f, -fallbackDirection.z },
            0.0001f);

        const f32_t landingDistance =
            GameplayStateQuery::ResolveGameplayRadius(world, caster) +
            GameplayStateQuery::ResolveGameplayRadius(world, target) +
            behindPadding;

        Vec3 landing{
            targetPos.x + behindDir.x * landingDistance,
            targetPos.y,
            targetPos.z + behindDir.z * landingDistance
        };

        if (pTickCtx && pTickCtx->pWalkable)
        {
            Vec3 resolved = landing;
            if (pTickCtx->pWalkable->TryResolveMoveTarget(targetPos, landing, resolved))
                landing = resolved;

            f32_t height = 0.f;
            if (pTickCtx->pWalkable->TrySampleHeight(landing.x, landing.z, height))
                landing.y = height + 0.05f;
        }

        return landing;
    }

    Vec3 ResolveDirectionFromCommand(CWorld& world, const GameCommand* pCommand, EntityID caster)
    {
        if (pCommand)
        {
            Vec3 dir = WintersMath::NormalizeXZ(pCommand->direction, Vec3{}, 0.0001f);
            if (dir.x != 0.f || dir.z != 0.f)
                return dir;

            if (world.HasComponent<TransformComponent>(caster))
            {
                const Vec3 origin = world.GetComponent<TransformComponent>(caster).GetPosition();
                dir = WintersMath::DirectionXZ(origin, pCommand->groundPos, Vec3{});
                if (dir.x != 0.f || dir.z != 0.f)
                    return dir;
            }
        }

        return ResolveForward(world, caster);
    }

    Vec3 ResolveClampedGroundTarget(
        CWorld& world,
        const TickContext* pTickCtx,
        EntityID caster,
        const GameCommand* pCommand,
        const Vec3& direction,
        f32_t range)
    {
        const Vec3 origin = ResolvePosition(world, caster);
        Vec3 target{
            origin.x + direction.x * range,
            origin.y,
            origin.z + direction.z * range
        };

        if (pCommand &&
            (std::fabs(pCommand->groundPos.x) +
                std::fabs(pCommand->groundPos.y) +
                std::fabs(pCommand->groundPos.z)) > 0.001f)
        {
            const f32_t dx = pCommand->groundPos.x - origin.x;
            const f32_t dz = pCommand->groundPos.z - origin.z;
            const f32_t distSq = dx * dx + dz * dz;
            if (distSq <= range * range)
            {
                target = pCommand->groundPos;
            }
            else if (distSq > 0.0001f)
            {
                const f32_t invDist = 1.f / std::sqrt(distSq);
                target = {
                    origin.x + dx * invDist * range,
                    pCommand->groundPos.y,
                    origin.z + dz * invDist * range
                };
            }
        }

        if (pTickCtx && pTickCtx->pWalkable)
        {
            Vec3 resolved = target;
            if (pTickCtx->pWalkable->TryResolveMoveTarget(origin, target, resolved))
                target = resolved;

            f32_t height = 0.f;
            if (pTickCtx->pWalkable->TrySampleHeight(target.x, target.z, height))
                target.y = height + 0.05f;
        }

        return target;
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

        const Vec3 dir = WintersMath::NormalizeXZ(direction, Vec3{ 0.f, 0.f, 1.f }, 0.0001f);
        auto& transform = world.GetComponent<TransformComponent>(entity);
        const Vec3 rot = transform.GetRotation();
        transform.SetRotation(Vec3{
            rot.x,
            ResolveChampionVisualYawNear(eChampion::ZED, dir, rot.y),
            rot.z });
    }

    void EmitZedEffect(
        CWorld& world,
        EntityID source,
        EntityID target,
        u8_t slot,
        u8_t rank,
        u8_t stage,
        const Vec3& position,
        const Vec3& direction,
        u16_t durationMs,
        u64_t startTick)
    {
        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = source;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::ZED, CastFrameVariantForZedSlot(slot));
        event.skillId = MakeZedSkillId(slot);
        event.slot = slot;
        event.rank = rank;
        event.flags = static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (static_cast<u16_t>(rank & 0x0fu) << 8) |
            static_cast<u16_t>(slot));
        event.position = position;
        event.direction = direction;
        event.durationMs = durationMs;
        event.startTick = startTick;
        EnqueueReplicatedEvent(world, event);
    }

    DamageRequest BuildZedPhysicalDamageRequest(
        EntityID source,
        EntityID target,
        eTeam sourceTeam,
        f32_t amount,
        u8_t slot,
        u8_t rank)
    {
        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = slot == static_cast<u8_t>(eSkillSlot::R)
            ? 0u
            : MakeZedSkillId(slot);
        request.rank = rank;
        request.iSourceSlot = slot;
        request.eSourceKind = eDamageSourceKind::Skill;
        request.flags = DamageFlag_None;
        return request;
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
        if (target == NULL_ENTITY ||
            !world.IsAlive(target) ||
            !GameplayStateQuery::CanReceiveDamage(world, source, target))
        {
            return;
        }

        EnqueueDamageRequest(
            world,
            BuildZedPhysicalDamageRequest(
                source,
                target,
                sourceTeam,
                amount,
                slot,
                rank));
    }

    void SpawnZedShuriken(
        CWorld& world,
        EntityID caster,
        eTeam sourceTeam,
        const Vec3& origin,
        const Vec3& direction,
        f32_t speed,
        f32_t maxDistance,
        f32_t radius,
        f32_t baseDamage,
        f32_t damagePerRank,
        u8_t rank)
    {
        const Vec3 dir = WintersMath::NormalizeXZ(direction, Vec3{ 0.f, 0.f, 1.f }, 0.0001f);
        const EntityID projectileEntity = world.CreateEntity();

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = caster;
        projectile.sourceHandle = world.GetEntityHandle(caster);
        projectile.sourceTeam = sourceTeam;
        projectile.kind = eProjectileKind::ZedShuriken;
        projectile.skillId = MakeZedSkillId(static_cast<u8_t>(eSkillSlot::Q));
        projectile.rank = rank;
        projectile.currentPos = origin;
        projectile.direction = dir;
        projectile.speed = speed;
        projectile.maxDistance = maxDistance;
        projectile.hitRadius = radius;
        projectile.damage = baseDamage + damagePerRank * static_cast<f32_t>(rank > 0 ? rank - 1u : 0u);
        world.AddComponent<SkillProjectileComponent>(projectileEntity, projectile);
    }

    void AddTargetOnce(std::vector<EntityID>& targets, EntityID target)
    {
        if (target == NULL_ENTITY)
            return;

        if (std::find(targets.begin(), targets.end(), target) == targets.end())
            targets.push_back(target);
    }

    void CollectEnemiesInCircle(
        CWorld& world,
        EntityID source,
        eTeam sourceTeam,
        const Vec3& center,
        f32_t radius,
        std::vector<EntityID>& outTargets)
    {
        const f32_t radiusSq = radius * radius;
        world.ForEach<HealthComponent, TransformComponent>(
            std::function<void(EntityID, HealthComponent&, TransformComponent&)>(
                [&](EntityID entity, HealthComponent& health, TransformComponent& transform)
                {
                    if (entity == source ||
                        !world.IsAlive(entity) ||
                        health.bIsDead ||
                        health.fCurrent <= 0.f)
                    {
                        return;
                    }

                    const eTeam targetTeam = ResolveTeam(world, entity);
                    if (targetTeam == sourceTeam && targetTeam != eTeam::Neutral)
                        return;

                    if (!GameplayStateQuery::CanReceiveDamage(world, source, entity))
                        return;

                    if (WintersMath::DistanceSqXZ(center, transform.GetPosition()) <= radiusSq)
                        AddTargetOnce(outTargets, entity);
                }));
    }

    void ApplyVanishStatus(CWorld& world, EntityID caster, f32_t durationSec)
    {
        StatusEffectApplyDesc vanish{};
        vanish.effectId = eStatusEffectId::ZedDeathMark;
        vanish.stackPolicy = eStatusStackPolicy::RefreshDuration;
        vanish.sourceEntity = caster;
        vanish.stackGroup = MakeZedSkillId(static_cast<u8_t>(eSkillSlot::R));
        vanish.stateFlags =
            kGameplayStateInvisibleFlag |
            kGameplayStateUntargetableFlag |
            kGameplayStateCannotMoveFlag |
            kGameplayStateCannotAttackFlag |
            kGameplayStateCannotCastFlag;
        vanish.fDurationSec = durationSec;
        GameplayStatus::ApplyStatusEffect(world, caster, vanish);

        ZedVanishComponent vanishState{};
        vanishState.fRemainingSec = durationSec;
        if (world.HasComponent<ZedVanishComponent>(caster))
            world.GetComponent<ZedVanishComponent>(caster) = vanishState;
        else
            world.AddComponent<ZedVanishComponent>(caster, vanishState);
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        if (!world.HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        const Vec3 dir = ResolveDirectionFromCommand(world, ctx.pCommand, ctx.casterEntity);
        RotateToward(world, ctx.casterEntity, dir);
        ClearMove(world, ctx.casterEntity);

        const f32_t range = ResolveZedSkillRange(ctx, eSkillSlot::Q);
        const f32_t speed = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Speed);
        const f32_t radius = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Radius);
        const f32_t baseDamage = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage);
        const f32_t damagePerRank = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::DamagePerRank);
        const Vec3 casterPos = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        SpawnZedShuriken(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            Vec3{ casterPos.x, casterPos.y + 1.15f, casterPos.z },
            dir,
            speed,
            range,
            radius,
            baseDamage,
            damagePerRank,
            ctx.skillRank);

        constexpr eSkillSlot kShadowSlots[] = { eSkillSlot::W, eSkillSlot::R };
        for (eSkillSlot shadowSlot : kShadowSlots)
        {
            Vec3 shadowPos{};
            Vec3 shadowDir{};
            if (!ZedGameSim::TryGetShadowSource(
                    world,
                    ctx.casterEntity,
                    static_cast<u8_t>(shadowSlot),
                    shadowPos,
                    shadowDir))
            {
                continue;
            }

            SpawnZedShuriken(
                world,
                ctx.casterEntity,
                ctx.casterTeam,
                Vec3{ shadowPos.x, shadowPos.y + 1.15f, shadowPos.z },
                dir,
                speed,
                range,
                radius,
                baseDamage,
                damagePerRank,
                ctx.skillRank);
        }

    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        CWorld& world = *ctx.pWorld;
        if (ctx.pCommand && ctx.pCommand->itemId == 2u)
        {
            GameCommand swapCommand = *ctx.pCommand;
            if (ctx.pTickCtx)
                ZedGameSim::ApplyLivingShadowMove(world, *ctx.pTickCtx, swapCommand);
            return;
        }
        if (!ctx.pTickCtx)
            return;

        const Vec3 dir = ResolveDirectionFromCommand(world, ctx.pCommand, ctx.casterEntity);
        const f32_t range = ResolveZedSkillRange(ctx, eSkillSlot::W);
        const f32_t shadowDurationSec = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::EffectDurationSec);
        const Vec3 target = ResolveClampedGroundTarget(
            world,
            ctx.pTickCtx,
            ctx.casterEntity,
            ctx.pCommand,
            dir,
            range);

        SetZedShadowState(
            world,
            ctx.casterEntity,
            eSkillSlot::W,
            target,
            dir,
            shadowDurationSec);

        RotateToward(world, ctx.casterEntity, dir);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 dir = ResolveDirectionFromCommand(world, ctx.pCommand, ctx.casterEntity);
        RotateToward(world, ctx.casterEntity, dir);
        const eTeam sourceTeam = ResolveTeam(world, ctx.casterEntity);
        std::vector<EntityID> targets;
        targets.reserve(16);
        const f32_t radius = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::Radius);
        const f32_t baseDamage = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::BaseDamage);
        const f32_t damagePerRank = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::DamagePerRank);
        const f32_t slowDurationSec = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::SlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::MoveSpeedMul);

        const Vec3 casterPos = ResolvePosition(world, ctx.casterEntity);
        EmitZedEffect(
            world,
            ctx.casterEntity,
            NULL_ENTITY,
            static_cast<u8_t>(eSkillSlot::E),
            ctx.skillRank,
            1u,
            casterPos,
            dir,
            500u,
            ctx.pTickCtx->tickIndex);
        CollectEnemiesInCircle(world, ctx.casterEntity, sourceTeam, casterPos, radius, targets);

        constexpr eSkillSlot kShadowSlots[] = { eSkillSlot::W, eSkillSlot::R };
        for (eSkillSlot shadowSlot : kShadowSlots)
        {
            Vec3 shadowPos{};
            Vec3 shadowDir{};
            if (!ZedGameSim::TryGetShadowSource(
                    world,
                    ctx.casterEntity,
                    static_cast<u8_t>(shadowSlot),
                    shadowPos,
                    shadowDir))
            {
                continue;
            }

            CollectEnemiesInCircle(world, ctx.casterEntity, sourceTeam, shadowPos, radius, targets);
            EmitZedEffect(
                world,
                ctx.casterEntity,
                NULL_ENTITY,
                static_cast<u8_t>(eSkillSlot::E),
                ctx.skillRank,
                2u,
                shadowPos,
                dir,
                500u,
                ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0ull);
        }

        const f32_t damage =
            baseDamage + damagePerRank * static_cast<f32_t>(ctx.skillRank > 0 ? ctx.skillRank - 1u : 0u);
        for (EntityID target : targets)
        {
            EnqueuePhysicalDamage(
                world,
                ctx.casterEntity,
                target,
                sourceTeam,
                damage,
                static_cast<u8_t>(eSkillSlot::E),
                ctx.skillRank);
            GameplayStatus::ApplySlow(
                world,
                *ctx.pTickCtx,
                target,
                ctx.casterEntity,
                eChampion::ZED,
                eSkillSlot::E,
                slowDurationSec,
                slowMoveSpeedMul);
        }

    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        if (ctx.pCommand->itemId == 2u)
        {
            GameCommand swapCommand = *ctx.pCommand;
            ZedGameSim::ApplyLivingShadowMove(world, *ctx.pTickCtx, swapCommand);
            return;
        }

        const EntityID target = ctx.pCommand->targetEntity;
        if (!ZedGameSim::CanCastDeathMark(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target))
        {
            return;
        }

        Vec3 dir = ResolveDirectionFromCommand(world, ctx.pCommand, ctx.casterEntity);
        const Vec3 casterPos = ResolvePosition(world, ctx.casterEntity);
        const Vec3 targetPos = ResolvePosition(world, target);
        const Vec3 toTarget = WintersMath::DirectionXZ(casterPos, targetPos, dir);
        if (toTarget.x != 0.f || toTarget.z != 0.f)
            dir = toTarget;

        const f32_t shadowDurationSec = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::EffectDurationSec);
        const f32_t behindPadding = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Gap);
        const f32_t vanishDurationSec = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::VanishDurationSec);
        const f32_t markDurationSec = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::MarkDurationSec);
        const f32_t missingHealthDamageRatio = ResolveZedSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::MissingHealthDamageRatio);
        EmitZedEffect(
            world,
            ctx.casterEntity,
            target,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            1u,
            targetPos,
            dir,
            static_cast<u16_t>(std::clamp(
                markDurationSec * 1000.f,
                0.f,
                65535.f)),
            ctx.pTickCtx->tickIndex);
        SetZedShadowState(
            world,
            ctx.casterEntity,
            eSkillSlot::R,
            casterPos,
            dir,
            shadowDurationSec);
        EmitZedEffect(
            world,
            ctx.casterEntity,
            target,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            kZedRSourceShadowStage,
            casterPos,
            dir,
            static_cast<u16_t>(shadowDurationSec * 1000.f),
            ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0ull);

        const Vec3 landingPos = ResolveDeathMarkLandingPosition(
            world,
            ctx.pTickCtx,
            ctx.casterEntity,
            target,
            dir,
            behindPadding);
        world.GetComponent<TransformComponent>(ctx.casterEntity).SetPosition(landingPos);
        PositionDiscontinuityComponent& discontinuity =
            world.HasComponent<PositionDiscontinuityComponent>(ctx.casterEntity)
                ? world.GetComponent<PositionDiscontinuityComponent>(ctx.casterEntity)
                : world.AddComponent<PositionDiscontinuityComponent>(
                    ctx.casterEntity,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = ctx.pTickCtx->tickIndex;

#if defined(_DEBUG)
        char landingTrace[256]{};
        std::snprintf(
            landingTrace,
            sizeof(landingTrace),
            "[ZedShadow][R1] caster=%u target=%u source=(%.2f,%.2f,%.2f) behind=(%.2f,%.2f,%.2f) lifetime=%.2f\n",
            static_cast<u32_t>(ctx.casterEntity),
            static_cast<u32_t>(target),
            casterPos.x,
            casterPos.y,
            casterPos.z,
            landingPos.x,
            landingPos.y,
            landingPos.z,
            shadowDurationSec);
        WintersOutputAIDebugStringA(landingTrace);
#endif

        const Vec3 faceDir = WintersMath::DirectionXZ(landingPos, targetPos, dir);
        if (faceDir.x != 0.f || faceDir.z != 0.f)
            dir = faceDir;

        RotateToward(world, ctx.casterEntity, dir);
        ClearMove(world, ctx.casterEntity);
        ApplyVanishStatus(world, ctx.casterEntity, vanishDurationSec);

        ZedDeathMarkComponent mark{};
        mark.entitySource = ctx.casterEntity;
        mark.rank = ctx.skillRank;
        mark.fRemainingSec = markDurationSec;
        mark.fMissingHealthDamageRatio = missingHealthDamageRatio;
        if (world.HasComponent<ZedDeathMarkComponent>(target))
            world.GetComponent<ZedDeathMarkComponent>(target) = mark;
        else
            world.AddComponent<ZedDeathMarkComponent>(target, mark);

    }
}

namespace ZedGameSim
{
	bool_t CanTriggerPassiveBasicAttack(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target)
	{
		if (caster == NULL_ENTITY ||
			target == NULL_ENTITY ||
			!world.IsAlive(caster) ||
			!world.IsAlive(target) ||
			!world.HasComponent<HealthComponent>(target) ||
			world.HasComponent<StructureComponent>(target) ||
			!GameplayStateQuery::CanReceiveDamage(world, caster, target))
		{
			return false;
		}

		const HealthComponent& health = world.GetComponent<HealthComponent>(target);
		if (health.bIsDead || health.fMaximum <= 0.f || health.fCurrent <= 0.f)
			return false;

		const f32_t threshold = std::clamp(
			ResolveZedSkillEffectParam(
				world,
				tc,
				caster,
				eSkillSlot::BasicAttack,
				eSkillEffectParamId::TargetHealthThresholdRatio,
				0.f),
			0.f,
			1.f);
		return health.fCurrent / health.fMaximum <= threshold;
	}

	void EnqueuePassiveBasicAttackDamage(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target)
	{
		if (caster == NULL_ENTITY ||
			target == NULL_ENTITY ||
			!world.IsAlive(caster) ||
			!world.IsAlive(target) ||
			!GameplayStateQuery::CanReceiveDamage(world, caster, target))
		{
			return;
		}

		const f32_t ratio = std::max(
			0.f,
			ResolveZedSkillEffectParam(
				world,
				tc,
				caster,
				eSkillSlot::BasicAttack,
				eSkillEffectParamId::MissingHealthDamageRatio,
				0.f));
		if (ratio <= 0.f)
			return;

		DamageRequest request{};
		request.source = caster;
		request.target = target;
		request.sourceTeam = ResolveTeam(world, caster);
		request.type = eDamageType::Physical;
		request.iSourceSlot = static_cast<u8_t>(eSkillSlot::BasicAttack);
		request.eSourceKind = eDamageSourceKind::BasicAttack;
		request.targetMissingHpRatioOverride = ratio;
		request.flags = DamageFlag_ShowCriticalIndicator;
		EnqueueDamageRequest(world, request);
	}

	bool_t CanCastDeathMark(
		CWorld& world,
		const TickContext& tc,
		EntityID caster,
		EntityID target)
	{
		if (caster == NULL_ENTITY ||
			target == NULL_ENTITY ||
			!world.IsAlive(caster) ||
			!world.IsAlive(target) ||
			!world.HasComponent<ChampionComponent>(caster) ||
			!world.HasComponent<ChampionComponent>(target) ||
			!world.HasComponent<HealthComponent>(target) ||
			!world.HasComponent<TransformComponent>(caster) ||
			!world.HasComponent<TransformComponent>(target) ||
			!GameplayStateQuery::CanBeTargetedBy(world, caster, target) ||
			!GameplayStateQuery::CanReceiveDamage(world, caster, target))
		{
			return false;
		}

		const auto& targetHealth = world.GetComponent<HealthComponent>(target);
		if (targetHealth.bIsDead || targetHealth.fCurrent <= 0.f)
			return false;

		const auto& casterChampion = world.GetComponent<ChampionComponent>(caster);
		const auto& targetChampion = world.GetComponent<ChampionComponent>(target);
		if (casterChampion.team == targetChampion.team)
			return false;

		f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
			world,
			caster,
			tc,
			eChampion::ZED,
			static_cast<u8_t>(eSkillSlot::R));
		range += GameplayStateQuery::ResolveGameplayRadius(world, caster);
		range += GameplayStateQuery::ResolveGameplayRadius(world, target);

		const Vec3 casterPosition =
			world.GetComponent<TransformComponent>(caster).GetPosition();
		const Vec3 targetPosition =
			world.GetComponent<TransformComponent>(target).GetPosition();
		if (WintersMath::DistanceSqXZ(casterPosition, targetPosition) > range * range)
			return false;

		return !tc.pWalkable ||
			tc.pWalkable->SegmentWalkableXZ(casterPosition, targetPosition, 0.f);
	}

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::Q_CastFrame), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ZED, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<ZedSimComponent>(
            std::function<void(EntityID, ZedSimComponent&)>(
                [&](EntityID, ZedSimComponent& state)
                {
                    ZedShadowState* shadows[] = { &state.wShadow, &state.rShadow };
                    for (ZedShadowState* pShadow : shadows)
                    {
                        if (!pShadow->bActive)
                            continue;

                        pShadow->fRemainingSec =
                            std::max(0.f, pShadow->fRemainingSec - tc.fDt);
                        if (pShadow->fRemainingSec <= 0.f)
                            pShadow->bActive = false;
                    }
                }));

        std::vector<EntityID> expiredVanishes;
        world.ForEach<ZedVanishComponent>(
            std::function<void(EntityID, ZedVanishComponent&)>(
                [&](EntityID entity, ZedVanishComponent& vanish)
                {
                    vanish.fRemainingSec = std::max(0.f, vanish.fRemainingSec - tc.fDt);
                    if (vanish.fRemainingSec <= 0.f)
                        expiredVanishes.push_back(entity);
                }));
        for (EntityID entity : expiredVanishes)
        {
            if (world.HasComponent<ZedVanishComponent>(entity))
                world.RemoveComponent<ZedVanishComponent>(entity);
        }

        std::vector<EntityID> explodingMarks;
        world.ForEach<ZedDeathMarkComponent>(
            std::function<void(EntityID, ZedDeathMarkComponent&)>(
                [&](EntityID entity, ZedDeathMarkComponent& mark)
                {
                    const bool_t bCanPredict =
                        mark.entitySource != NULL_ENTITY &&
                        world.IsAlive(mark.entitySource) &&
                        world.IsAlive(entity) &&
                        world.HasComponent<HealthComponent>(entity);
                    if (bCanPredict)
                    {
                        const HealthComponent& health =
                            world.GetComponent<HealthComponent>(entity);
                        const f32_t missingHealth =
                            std::max(0.f, health.fMaximum - health.fCurrent);
                        const f32_t damage =
                            missingHealth * mark.fMissingHealthDamageRatio;
                        const DamageRequest previewRequest =
                            BuildZedPhysicalDamageRequest(
                                mark.entitySource,
                                entity,
                                ResolveTeam(world, mark.entitySource),
                                damage,
                                static_cast<u8_t>(eSkillSlot::R),
                                mark.rank);
                        const bool_t bLethal =
                            WouldNonCriticalDamageRequestKill(
                                world,
                                tc,
                                previewRequest);

                        if (bLethal != mark.bLethalMarkerVisible)
                        {
                            mark.bLethalMarkerVisible = bLethal;
                            const Vec3 targetPos = ResolvePosition(world, entity);
                            const Vec3 sourcePos =
                                ResolvePosition(world, mark.entitySource);
                            const Vec3 dir = WintersMath::DirectionXZ(
                                sourcePos,
                                targetPos,
                                ResolveForward(world, mark.entitySource));
                            const f32_t durationMs = std::clamp(
                                mark.fRemainingSec * 1000.f,
                                0.f,
                                65535.f);
                            EmitZedEffect(
                                world,
                                mark.entitySource,
                                entity,
                                static_cast<u8_t>(eSkillSlot::R),
                                mark.rank,
                                bLethal
                                    ? kZedRLethalMarkerShowStage
                                    : kZedRLethalMarkerHideStage,
                                targetPos,
                                dir,
                                bLethal
                                    ? static_cast<u16_t>(durationMs)
                                    : 0u,
                                tc.tickIndex);
                        }
                    }

                    mark.fRemainingSec = std::max(0.f, mark.fRemainingSec - tc.fDt);
                    if (mark.fRemainingSec <= 0.f ||
                        mark.entitySource == NULL_ENTITY ||
                        !world.IsAlive(mark.entitySource))
                    {
                        explodingMarks.push_back(entity);
                    }
                }));

        for (EntityID target : explodingMarks)
        {
            if (!world.HasComponent<ZedDeathMarkComponent>(target))
                continue;

            const ZedDeathMarkComponent mark = world.GetComponent<ZedDeathMarkComponent>(target);
            world.RemoveComponent<ZedDeathMarkComponent>(target);

            if (mark.entitySource == NULL_ENTITY ||
                !world.IsAlive(mark.entitySource) ||
                !world.HasComponent<HealthComponent>(target))
            {
                continue;
            }

            const HealthComponent& health = world.GetComponent<HealthComponent>(target);
            const f32_t missingHealth =
                std::max(0.f, health.fMaximum - health.fCurrent);
            const f32_t damage = missingHealth * mark.fMissingHealthDamageRatio;
            const Vec3 targetPos = ResolvePosition(world, target);
            const Vec3 sourcePos = ResolvePosition(world, mark.entitySource);
            const Vec3 dir = WintersMath::DirectionXZ(sourcePos, targetPos, ResolveForward(world, mark.entitySource));

            EmitZedEffect(
                world,
                mark.entitySource,
                target,
                static_cast<u8_t>(eSkillSlot::R),
                mark.rank,
                2u,
                targetPos,
                dir,
                800u,
                tc.tickIndex);

            if (damage > 0.f)
            {
                EnqueuePhysicalDamage(
                    world,
                    mark.entitySource,
                    target,
                    ResolveTeam(world, mark.entitySource),
                    damage,
                    static_cast<u8_t>(eSkillSlot::R),
                    mark.rank);
            }

        }
    }

    bool_t ApplyLivingShadowMove(CWorld& world, const TickContext& tc, GameCommand& cmd)
    {
        if (cmd.issuerEntity == NULL_ENTITY ||
            !world.HasComponent<ZedSimComponent>(cmd.issuerEntity) ||
            !world.HasComponent<TransformComponent>(cmd.issuerEntity))
        {
            return false;
        }

        ZedSimComponent& state = world.GetComponent<ZedSimComponent>(cmd.issuerEntity);
        ZedShadowState* pShadow = ResolveShadowState(
            state,
            static_cast<eSkillSlot>(cmd.slot));
        if (!pShadow || !pShadow->bActive || pShadow->fRemainingSec <= 0.f)
            return false;

        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const Vec3 previous = transform.GetPosition();
        const Vec3 destination = pShadow->vPosition;
        transform.SetPosition(destination);
        PositionDiscontinuityComponent& discontinuity =
            world.HasComponent<PositionDiscontinuityComponent>(cmd.issuerEntity)
                ? world.GetComponent<PositionDiscontinuityComponent>(cmd.issuerEntity)
                : world.AddComponent<PositionDiscontinuityComponent>(
                    cmd.issuerEntity,
                    PositionDiscontinuityComponent{});
        discontinuity.uTick = tc.tickIndex;
        pShadow->vPosition = previous;
        RotateToward(world, cmd.issuerEntity, pShadow->vDirection);
        ClearMove(world, cmd.issuerEntity);

#if defined(_DEBUG)
        char swapTrace[256]{};
        std::snprintf(
            swapTrace,
            sizeof(swapTrace),
            "[ZedShadow][Swap] caster=%u slot=%u from=(%.2f,%.2f,%.2f) to=(%.2f,%.2f,%.2f) remaining=%.2f\n",
            static_cast<u32_t>(cmd.issuerEntity),
            static_cast<u32_t>(cmd.slot),
            previous.x,
            previous.y,
            previous.z,
            destination.x,
            destination.y,
            destination.z,
            pShadow->fRemainingSec);
        WintersOutputAIDebugStringA(swapTrace);
#endif
        return true;
    }

    bool_t TryGetShadowSource(
        CWorld& world,
        EntityID caster,
        u8_t sourceSlot,
        Vec3& outPosition,
        Vec3& outDirection)
    {
        if (caster == NULL_ENTITY ||
            !world.HasComponent<ZedSimComponent>(caster))
        {
            return false;
        }

        const ZedSimComponent& state = world.GetComponent<ZedSimComponent>(caster);
        const ZedShadowState* pShadow = ResolveShadowState(
            state,
            static_cast<eSkillSlot>(sourceSlot));
        if (!pShadow || !pShadow->bActive || pShadow->fRemainingSec <= 0.f)
            return false;

        outPosition = pShadow->vPosition;
        outDirection = pShadow->vDirection;
        return true;
    }
}
