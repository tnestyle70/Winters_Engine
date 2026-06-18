#include "Shared/GameSim/Champions/Zed/ZedGameSim.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/ZedSimComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Registries/ChampionGameData/ChampionGameDataDB.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr f32_t kZedQDamage = 70.f;
    constexpr f32_t kZedQDamagePerRank = 25.f;
    constexpr f32_t kZedQSpeed = 24.f;
    constexpr f32_t kZedQRange = 9.0f;
    constexpr f32_t kZedQRadius = 0.45f;

    constexpr f32_t kZedWRange = 6.5f;
    constexpr f32_t kZedWShadowDurationSec = 5.f;
    constexpr f32_t kZedRShadowDurationSec = 5.f;
    constexpr f32_t kZedRBehindPadding = 0.75f;
    constexpr u8_t kZedRSourceShadowStage = 3u;

    constexpr f32_t kZedEDamage = 65.f;
    constexpr f32_t kZedEDamagePerRank = 20.f;
    constexpr f32_t kZedERadius = 2.75f;
    constexpr f32_t kZedESlowDurationSec = 1.5f;
    constexpr f32_t kZedESlowMoveSpeedMul = 0.60f;

    constexpr f32_t kZedRVanishDurationSec = 0.75f;
    constexpr f32_t kZedRMarkDurationSec = 3.f;
    constexpr f32_t kZedRMissingHealthDamageRatio = 0.30f;

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

        const f32_t visualYaw = world.GetComponent<TransformComponent>(caster).GetRotation().y;
        return WintersMath::DirectionFromYawXZ(
            visualYaw - ChampionGameDataDB::ResolveVisualYawOffset(eChampion::ZED));
    }

    void SetZedShadowState(
        CWorld& world,
        EntityID caster,
        const Vec3& position,
        const Vec3& direction,
        f32_t durationSec)
    {
        ZedSimComponent& state = EnsureZedState(world, caster);
        state.bShadowActive = true;
        state.vShadowPosition = position;
        state.vShadowDirection = WintersMath::NormalizeXZ(
            direction,
            ResolveForward(world, caster),
            0.0001f);
        state.fShadowRemainingSec = durationSec;
    }

    Vec3 ResolveEntityForward(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<TransformComponent>(entity))
            return Vec3{ 0.f, 0.f, 1.f };

        eChampion championId = eChampion::NONE;
        if (world.HasComponent<ChampionComponent>(entity))
            championId = world.GetComponent<ChampionComponent>(entity).id;

        const f32_t visualYaw = world.GetComponent<TransformComponent>(entity).GetRotation().y;
        return WintersMath::DirectionFromYawXZ(
            visualYaw - ChampionGameDataDB::ResolveVisualYawOffset(championId));
    }

    Vec3 ResolveDeathMarkLandingPosition(
        CWorld& world,
        const TickContext* pTickCtx,
        EntityID caster,
        EntityID target,
        const Vec3& fallbackDirection)
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
            kZedRBehindPadding;

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
        const Vec3& direction)
    {
        const Vec3 origin = ResolvePosition(world, caster);
        Vec3 target{
            origin.x + direction.x * kZedWRange,
            origin.y,
            origin.z + direction.z * kZedWRange
        };

        if (pCommand &&
            (std::fabs(pCommand->groundPos.x) +
                std::fabs(pCommand->groundPos.y) +
                std::fabs(pCommand->groundPos.z)) > 0.001f)
        {
            const f32_t dx = pCommand->groundPos.x - origin.x;
            const f32_t dz = pCommand->groundPos.z - origin.z;
            const f32_t distSq = dx * dx + dz * dz;
            if (distSq <= kZedWRange * kZedWRange)
            {
                target = pCommand->groundPos;
            }
            else if (distSq > 0.0001f)
            {
                const f32_t invDist = 1.f / std::sqrt(distSq);
                target = {
                    origin.x + dx * invDist * kZedWRange,
                    pCommand->groundPos.y,
                    origin.z + dz * invDist * kZedWRange
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

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Physical;
        request.flatAmount = amount;
        request.skillId = MakeZedSkillId(slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    void SpawnZedShuriken(
        CWorld& world,
        EntityID caster,
        eTeam sourceTeam,
        const Vec3& origin,
        const Vec3& direction,
        u8_t rank)
    {
        const Vec3 dir = WintersMath::NormalizeXZ(direction, Vec3{ 0.f, 0.f, 1.f }, 0.0001f);
        const EntityID projectileEntity = world.CreateEntity();

        TransformComponent transform{};
        transform.SetPosition(origin);
        world.AddComponent<TransformComponent>(projectileEntity, transform);

        SkillProjectileComponent projectile{};
        projectile.sourceEntity = caster;
        projectile.sourceTeam = sourceTeam;
        projectile.kind = eProjectileKind::ZedShuriken;
        projectile.skillId = MakeZedSkillId(static_cast<u8_t>(eSkillSlot::Q));
        projectile.rank = rank;
        projectile.currentPos = origin;
        projectile.direction = dir;
        projectile.speed = kZedQSpeed;
        projectile.maxDistance = kZedQRange;
        projectile.hitRadius = kZedQRadius;
        projectile.damage = kZedQDamage + kZedQDamagePerRank * static_cast<f32_t>(rank > 0 ? rank - 1u : 0u);
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

    void ApplyVanishStatus(CWorld& world, EntityID caster)
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
        vanish.fDurationSec = kZedRVanishDurationSec;
        GameplayStatus::ApplyStatusEffect(world, caster, vanish);

        ZedVanishComponent vanishState{};
        vanishState.fRemainingSec = kZedRVanishDurationSec;
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

        const Vec3 casterPos = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        SpawnZedShuriken(
            world,
            ctx.casterEntity,
            ctx.casterTeam,
            Vec3{ casterPos.x, casterPos.y + 1.15f, casterPos.z },
            dir,
            ctx.skillRank);

        Vec3 shadowPos{};
        Vec3 shadowDir{};
        if (ZedGameSim::TryGetShadowSource(world, ctx.casterEntity, shadowPos, shadowDir))
        {
            SpawnZedShuriken(
                world,
                ctx.casterEntity,
                ctx.casterTeam,
                Vec3{ shadowPos.x, shadowPos.y + 1.15f, shadowPos.z },
                dir,
                ctx.skillRank);
        }

        std::cout << "[ZedSim] Q shuriken caster="
            << ctx.casterEntity << " rank=" << static_cast<u32_t>(ctx.skillRank) << "\n";
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        CWorld& world = *ctx.pWorld;
        if (ctx.pCommand && ctx.pCommand->itemId == 2u)
        {
            GameCommand swapCommand = *ctx.pCommand;
            if (ctx.pTickCtx && ZedGameSim::ApplyLivingShadowMove(world, *ctx.pTickCtx, swapCommand))
            {
                std::cout << "[ZedSim] W swap caster="
                    << ctx.casterEntity << "\n";
            }
            return;
        }

        const Vec3 dir = ResolveDirectionFromCommand(world, ctx.pCommand, ctx.casterEntity);
        const Vec3 target = ResolveClampedGroundTarget(
            world,
            ctx.pTickCtx,
            ctx.casterEntity,
            ctx.pCommand,
            dir);

        SetZedShadowState(world, ctx.casterEntity, target, dir, kZedWShadowDurationSec);

        RotateToward(world, ctx.casterEntity, dir);
        ClearMove(world, ctx.casterEntity);

        std::cout << "[ZedSim] W shadow caster="
            << ctx.casterEntity << " pos=(" << target.x << "," << target.z << ")\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        CWorld& world = *ctx.pWorld;
        const Vec3 dir = ResolveDirectionFromCommand(world, ctx.pCommand, ctx.casterEntity);
        RotateToward(world, ctx.casterEntity, dir);
        ClearMove(world, ctx.casterEntity);

        const eTeam sourceTeam = ResolveTeam(world, ctx.casterEntity);
        std::vector<EntityID> targets;
        targets.reserve(16);

        const Vec3 casterPos = ResolvePosition(world, ctx.casterEntity);
        CollectEnemiesInCircle(world, ctx.casterEntity, sourceTeam, casterPos, kZedERadius, targets);

        Vec3 shadowPos{};
        Vec3 shadowDir{};
        if (ZedGameSim::TryGetShadowSource(world, ctx.casterEntity, shadowPos, shadowDir))
        {
            CollectEnemiesInCircle(world, ctx.casterEntity, sourceTeam, shadowPos, kZedERadius, targets);
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
            kZedEDamage + kZedEDamagePerRank * static_cast<f32_t>(ctx.skillRank > 0 ? ctx.skillRank - 1u : 0u);
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
                kZedESlowDurationSec,
                kZedESlowMoveSpeedMul);
        }

        std::cout << "[ZedSim] E slash caster="
            << ctx.casterEntity << " hits=" << targets.size() << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand)
            return;

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<TransformComponent>(ctx.casterEntity) ||
            !world.HasComponent<TransformComponent>(target) ||
            !GameplayStateQuery::CanBeTargetedBy(world, ctx.casterEntity, target))
        {
            std::cout << "[ZedSim] R rejected caster="
                << ctx.casterEntity << " target=" << target << "\n";
            return;
        }

        Vec3 dir = ResolveDirectionFromCommand(world, ctx.pCommand, ctx.casterEntity);
        const Vec3 casterPos = ResolvePosition(world, ctx.casterEntity);
        const Vec3 targetPos = ResolvePosition(world, target);
        const Vec3 toTarget = WintersMath::DirectionXZ(casterPos, targetPos, dir);
        if (toTarget.x != 0.f || toTarget.z != 0.f)
            dir = toTarget;

        SetZedShadowState(
            world,
            ctx.casterEntity,
            casterPos,
            dir,
            kZedRShadowDurationSec);
        EmitZedEffect(
            world,
            ctx.casterEntity,
            target,
            static_cast<u8_t>(eSkillSlot::R),
            ctx.skillRank,
            kZedRSourceShadowStage,
            casterPos,
            dir,
            static_cast<u16_t>(kZedRShadowDurationSec * 1000.f),
            ctx.pTickCtx ? ctx.pTickCtx->tickIndex : 0ull);

        const Vec3 landingPos = ResolveDeathMarkLandingPosition(
            world,
            ctx.pTickCtx,
            ctx.casterEntity,
            target,
            dir);
        world.GetComponent<TransformComponent>(ctx.casterEntity).SetPosition(landingPos);

        const Vec3 faceDir = WintersMath::DirectionXZ(landingPos, targetPos, dir);
        if (faceDir.x != 0.f || faceDir.z != 0.f)
            dir = faceDir;

        RotateToward(world, ctx.casterEntity, dir);
        ClearMove(world, ctx.casterEntity);
        ApplyVanishStatus(world, ctx.casterEntity);

        ZedDeathMarkComponent mark{};
        mark.entitySource = ctx.casterEntity;
        mark.rank = ctx.skillRank;
        mark.fRemainingSec = kZedRMarkDurationSec;
        mark.fMissingHealthDamageRatio = kZedRMissingHealthDamageRatio;
        if (world.HasComponent<ZedDeathMarkComponent>(target))
            world.GetComponent<ZedDeathMarkComponent>(target) = mark;
        else
            world.AddComponent<ZedDeathMarkComponent>(target, mark);

        std::cout << "[ZedSim] R death mark caster="
            << ctx.casterEntity << " target=" << target << "\n";
    }
}

namespace ZedGameSim
{
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
        std::cout << "[ZedSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        std::vector<EntityID> expiredShadows;
        world.ForEach<ZedSimComponent>(
            std::function<void(EntityID, ZedSimComponent&)>(
                [&](EntityID entity, ZedSimComponent& state)
                {
                    if (!state.bShadowActive)
                        return;

                    state.fShadowRemainingSec =
                        std::max(0.f, state.fShadowRemainingSec - tc.fDt);
                    if (state.fShadowRemainingSec <= 0.f)
                        expiredShadows.push_back(entity);
                }));

        for (EntityID entity : expiredShadows)
        {
            if (world.HasComponent<ZedSimComponent>(entity))
            {
                ZedSimComponent& state = world.GetComponent<ZedSimComponent>(entity);
                state.bShadowActive = false;
                state.fShadowRemainingSec = 0.f;
            }
        }

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

            std::cout << "[ZedSim] R mark pop source="
                << mark.entitySource << " target=" << target
                << " damage=" << damage << "\n";
        }
    }

    bool_t ApplyLivingShadowMove(CWorld& world, const TickContext& tc, GameCommand& cmd)
    {
        (void)tc;
        if (cmd.issuerEntity == NULL_ENTITY ||
            !world.HasComponent<ZedSimComponent>(cmd.issuerEntity) ||
            !world.HasComponent<TransformComponent>(cmd.issuerEntity))
        {
            return false;
        }

        ZedSimComponent& state = world.GetComponent<ZedSimComponent>(cmd.issuerEntity);
        if (!state.bShadowActive || state.fShadowRemainingSec <= 0.f)
            return false;

        auto& transform = world.GetComponent<TransformComponent>(cmd.issuerEntity);
        const Vec3 previous = transform.GetPosition();
        transform.SetPosition(state.vShadowPosition);
        state.vShadowPosition = previous;
        RotateToward(world, cmd.issuerEntity, state.vShadowDirection);
        ClearMove(world, cmd.issuerEntity);
        return true;
    }

    bool_t TryGetShadowSource(CWorld& world, EntityID caster, Vec3& outPosition, Vec3& outDirection)
    {
        if (caster == NULL_ENTITY ||
            !world.HasComponent<ZedSimComponent>(caster))
        {
            return false;
        }

        const ZedSimComponent& state = world.GetComponent<ZedSimComponent>(caster);
        if (!state.bShadowActive || state.fShadowRemainingSec <= 0.f)
            return false;

        outPosition = state.vShadowPosition;
        outDirection = state.vShadowDirection;
        return true;
    }
}
