#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"

#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "Shared/GameSim/Core/Ecs/CoreComponents.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/NavigationThrottleComponent.h"
#include "Shared/GameSim/Core/Ecs/SpatialAgentComponent.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/Ecs/VisionComponents.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr u8_t kStunThreshold = 4;
    constexpr u32_t kEShieldBuffDefId = (static_cast<u32_t>(eChampion::ANNIE) << 16) | 3u;

    constexpr u8_t kTibbersRoleType = 4;
    // 0xff = any-lane: Tibbers bypasses lane-gated minion AI paths.
    constexpr u8_t kTibbersLane = 0xff;

    f32_t ResolveAnnieSkillEffectParam(
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
            eChampion::ANNIE,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveAnnieSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue = 0.f)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return ResolveAnnieSkillEffectParam(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            param,
            fallbackValue);
    }

    f32_t ResolveAnnieSummonPolicyParam(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        eSkillSlot slot,
        eSummonPolicyParamId param,
        f32_t fallbackValue = 0.f)
    {
        return GameplayDefinitionQuery::ResolveSummonPolicyParam(
            world,
            caster,
            tc,
            eChampion::ANNIE,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    struct TibbersSpawnTuning
    {
        f32_t durationSec = 0.f;
        f32_t moveSpeed = 0.f;
        f32_t attackRange = 0.f;
        f32_t sightRange = 0.f;
        f32_t attackCooldownSec = 0.f;
        f32_t baseAttackDamage = 0.f;
        f32_t attackDamagePerRank = 0.f;
        f32_t baseHp = 0.f;
        f32_t hpPerRank = 0.f;
        f32_t radius = 0.f;
    };

    TibbersSpawnTuning ResolveTibbersSpawnTuning(
        CWorld& world,
        const TickContext& tc,
        EntityID caster)
    {
        TibbersSpawnTuning tuning{};
        tuning.durationSec = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::DurationSec);
        tuning.moveSpeed = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::MoveSpeed);
        tuning.attackRange = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::AttackRange);
        tuning.sightRange = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::SightRange);
        tuning.attackCooldownSec = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::AttackCooldownSec);
        tuning.baseAttackDamage = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::BaseAttackDamage);
        tuning.attackDamagePerRank = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::AttackDamagePerRank);
        tuning.baseHp = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::BaseHp);
        tuning.hpPerRank = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::HpPerRank);
        tuning.radius = ResolveAnnieSummonPolicyParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSummonPolicyParamId::Radius);
        return tuning;
    }

    u16_t SkillIdForSlot(u8_t slot)
    {
        return static_cast<u16_t>((static_cast<u32_t>(eChampion::ANNIE) << 8) | slot);
    }

    u8_t TeamByte(eTeam team)
    {
        return static_cast<u8_t>(team);
    }

    VisibilityComponent BuildVisibleToAll()
    {
        VisibilityComponent visibility{};
        visibility.teamVisibilityMask =
            static_cast<u8_t>((1u << TeamByte(eTeam::Blue)) | (1u << TeamByte(eTeam::Red)));
        return visibility;
    }

    AnnieSimComponent& EnsureAnnieState(CWorld& world, EntityID entity)
    {
        if (!world.HasComponent<AnnieSimComponent>(entity))
            world.AddComponent<AnnieSimComponent>(entity, AnnieSimComponent{});
        return world.GetComponent<AnnieSimComponent>(entity);
    }

    bool_t IsAliveDamageTarget(CWorld& world, EntityID target)
    {
        if (target == NULL_ENTITY || !world.IsAlive(target))
            return false;
        if (!world.HasComponent<HealthComponent>(target))
            return false;

        const HealthComponent& health = world.GetComponent<HealthComponent>(target);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    bool_t TryResolveTeam(CWorld& world, EntityID entity, eTeam& outTeam)
    {
        if (world.HasComponent<ChampionComponent>(entity))
        {
            outTeam = world.GetComponent<ChampionComponent>(entity).team;
            return true;
        }
        if (world.HasComponent<MinionComponent>(entity))
        {
            outTeam = world.GetComponent<MinionComponent>(entity).team;
            return true;
        }
        if (world.HasComponent<MinionStateComponent>(entity))
        {
            outTeam = world.GetComponent<MinionStateComponent>(entity).team;
            return true;
        }
        if (world.HasComponent<StructureComponent>(entity))
        {
            outTeam = world.GetComponent<StructureComponent>(entity).team;
            return true;
        }
        if (world.HasComponent<TurretComponent>(entity))
        {
            outTeam = world.GetComponent<TurretComponent>(entity).team;
            return true;
        }
        if (world.HasComponent<JungleComponent>(entity))
        {
            outTeam = eTeam::Neutral;
            return true;
        }
        return false;
    }

    bool_t IsEnemyDamageTarget(CWorld& world, EntityID source, EntityID target, eTeam sourceTeam)
    {
        if (target == source || !IsAliveDamageTarget(world, target))
            return false;

        eTeam targetTeam = eTeam::Neutral;
        if (TryResolveTeam(world, target, targetTeam) &&
            targetTeam == sourceTeam &&
            targetTeam != eTeam::Neutral)
        {
            return false;
        }

        return true;
    }

    void ApplyMagicDamage(CWorld& world, const TickContext& tc, EntityID source,
        EntityID target, eTeam sourceTeam, u8_t slot, u8_t rank, f32_t amount)
    {
        (void)tc;
        if (!IsEnemyDamageTarget(world, source, target, sourceTeam))
            return;

        DamageRequest request{};
        request.source = source;
        request.target = target;
        request.sourceTeam = sourceTeam;
        request.type = eDamageType::Magic;
        request.flatAmount = amount;
        request.skillId = SkillIdForSlot(slot);
        request.rank = rank;
        EnqueueDamageRequest(world, request);
    }

    void ApplyStun(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        EntityID target,
        eSkillSlot slot,
        f32_t durationSec)
    {
        if (!IsAliveDamageTarget(world, target))
            return;

        GameplayStatus::ApplyStun(
            world,
            tc,
            target,
            source,
            eChampion::ANNIE,
            slot,
            durationSec);
    }

    bool_t ConsumeStunReady(AnnieSimComponent& state)
    {
        if (!state.bStunReady)
            return false;

        state.bStunReady = false;
        state.stunStacks = 0;
        return true;
    }

    void AddStunStack(AnnieSimComponent& state)
    {
        if (state.bStunReady)
            return;

        ++state.stunStacks;
        if (state.stunStacks >= kStunThreshold)
        {
            state.stunStacks = 0;
            state.bStunReady = true;
        }
    }

    Vec3 ResolveForward(CWorld& world, EntityID caster, const GameCommand* pCommand)
    {
        if (pCommand)
        {
            const f32_t lenSq =
                pCommand->direction.x * pCommand->direction.x +
                pCommand->direction.z * pCommand->direction.z;
            if (lenSq > 0.0001f)
            {
                const f32_t invLen = 1.f / std::sqrtf(lenSq);
                return Vec3{ pCommand->direction.x * invLen, 0.f, pCommand->direction.z * invLen };
            }
        }

        if (world.HasComponent<TransformComponent>(caster))
        {
            const f32_t yaw = world.GetComponent<TransformComponent>(caster).GetRotation().y;
            return Vec3{ std::sinf(yaw), 0.f, std::cosf(yaw) };
        }

        return Vec3{ 0.f, 0.f, 1.f };
    }

    Vec3 ResolveGroundCastPosition(CWorld& world, const TickContext& tc,
        EntityID caster, const GameCommand* pCommand, f32_t maxRange)
    {
        Vec3 origin{};
        if (world.HasComponent<TransformComponent>(caster))
            origin = world.GetComponent<TransformComponent>(caster).GetPosition();

        Vec3 pos = pCommand ? pCommand->groundPos : origin;
        const Vec3 delta{ pos.x - origin.x, 0.f, pos.z - origin.z };
        const f32_t distSq = delta.x * delta.x + delta.z * delta.z;
        if (distSq > maxRange * maxRange)
        {
            const f32_t invDist = 1.f / std::sqrtf(distSq);
            pos.x = origin.x + delta.x * invDist * maxRange;
            pos.z = origin.z + delta.z * invDist * maxRange;
        }
        else if (distSq <= 0.0001f)
        {
            const Vec3 forward = ResolveForward(world, caster, pCommand);
            pos.x = origin.x + forward.x * maxRange;
            pos.z = origin.z + forward.z * maxRange;
        }

        if (tc.pWalkable)
        {
            Vec3 resolved{};
            if (tc.pWalkable->TryResolveMoveTarget(origin, pos, resolved))
                pos = resolved;

            f32_t y = pos.y;
            if (tc.pWalkable->TrySampleHeight(pos.x, pos.z, y))
                pos.y = y;
        }

        return pos;
    }

    void DestroyTibbers(CWorld& world, const TickContext& tc, EntityID tibbers)
    {
        if (tibbers == NULL_ENTITY || !world.IsAlive(tibbers))
            return;

        if (tc.pEntityMap && world.HasComponent<NetEntityIdComponent>(tibbers))
        {
            const NetEntityId netId = world.GetComponent<NetEntityIdComponent>(tibbers).netId;
            if (netId != NULL_NET_ENTITY)
                tc.pEntityMap->Unbind(netId);
        }

        world.DestroyEntity(tibbers);
    }

    EntityID SpawnTibbersMinion(CWorld& world, const TickContext& tc,
        EntityID owner,
        eTeam ownerTeam,
        const Vec3& pos,
        u8_t rank,
        const TibbersSpawnTuning& tuning)
    {
        AnnieSimComponent& annie = EnsureAnnieState(world, owner);
        if (annie.tibbersEntity != NULL_ENTITY && world.IsAlive(annie.tibbersEntity))
            DestroyTibbers(world, tc, annie.tibbersEntity);

        EntityID tibbers = world.CreateEntity();

        TransformComponent transform{};
        transform.SetPosition(pos);
        transform.SetScale(0.012f);
        world.AddComponent<TransformComponent>(tibbers, transform);

        MinionStateComponent state{};
        state.current = MinionStateComponent::Idle;
        state.currentWaypoint = 0;
        state.team = ownerTeam;
        state.type = kTibbersRoleType;
        state.lane = kTibbersLane;
        state.moveSpeed = tuning.moveSpeed;
        state.attackRange = tuning.attackRange;
        state.sightRange = tuning.sightRange;
        state.attackDamage =
            tuning.baseAttackDamage + tuning.attackDamagePerRank * static_cast<f32_t>(rank);
        state.attackCooldownMax = tuning.attackCooldownSec;
        state.targetScanInterval = 0.16f;
        state.targetScanCooldown = 0.f;
        state.animUpdateInterval = 1.f / 15.f;
        world.AddComponent<MinionStateComponent>(tibbers, state);

        const f32_t maxHp = tuning.baseHp + tuning.hpPerRank * static_cast<f32_t>(rank);
        HealthComponent health{};
        health.fCurrent = maxHp;
        health.fMaximum = maxHp;
        health.bIsDead = false;
        world.AddComponent<HealthComponent>(tibbers, health);

        MinionComponent minion{};
        minion.team = ownerTeam;
        minion.laneType = kTibbersLane;
        minion.roleType = kTibbersRoleType;
        minion.hp = maxHp;
        minion.maxHp = maxHp;
        world.AddComponent<MinionComponent>(tibbers, minion);

        world.AddComponent<VelocityComponent>(tibbers, VelocityComponent{});

        SpatialAgentComponent spatial{};
        spatial.kind = eSpatialKind::Unit;
        spatial.team = TeamByte(ownerTeam);
        spatial.radius = tuning.radius;
        world.AddComponent<SpatialAgentComponent>(tibbers, spatial);

        ColliderComponent collider{};
        collider.vHalfExtents = { tuning.radius, 1.5f, tuning.radius };
        collider.vOffset = { 0.f, 0.75f, 0.f };
        collider.bIsTrigger = false;
        world.AddComponent<ColliderComponent>(tibbers, collider);

        VisionSourceComponent vision{};
        vision.sightRange = tuning.sightRange;
        world.AddComponent<VisionSourceComponent>(tibbers, vision);
        world.AddComponent<VisibilityComponent>(tibbers, BuildVisibleToAll());
        world.AddComponent<TargetableTag>(tibbers, TargetableTag{});
        world.AddComponent<NavRepathThrottleComponent>(tibbers, NavRepathThrottleComponent{});

        SetPoseState(world, tibbers, ePoseStateId::Run, tc.tickIndex, true);

        if (tc.pEntityMap)
        {
            NetEntityIdComponent net{};
            net.netId = tc.pEntityMap->IssueNew(tibbers);
            world.AddComponent<NetEntityIdComponent>(tibbers, net);
        }

        AnnieTibbersComponent tibbersState{};
        tibbersState.owner = owner;
        tibbersState.ownerTeam = ownerTeam;
        tibbersState.fRemainingSec = tuning.durationSec;
        world.AddComponent<AnnieTibbersComponent>(tibbers, tibbersState);

        annie.tibbersEntity = tibbers;
        return tibbers;
    }

    f32_t RankedDamage(f32_t baseAmount, f32_t perRank, u8_t rank)
    {
        const f32_t rankValue = static_cast<f32_t>((std::max<u8_t>)(rank, 1));
        return baseAmount + perRank * rankValue;
    }

    void OnQ(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx ||
            ctx.pCommand->targetEntity == NULL_ENTITY)
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        const EntityID target = ctx.pCommand->targetEntity;
        if (!GameplayStateQuery::CanReceiveCrowdControl(
            world,
            ctx.casterEntity,
            target))
        {
            return;
        }

        AnnieSimComponent& state = EnsureAnnieState(world, ctx.casterEntity);
        AddStunStack(state);
        const bool_t bShouldStun = ConsumeStunReady(state);
        const f32_t baseDamage = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage);
        const f32_t damagePerRank = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::DamagePerRank);
        const f32_t stunDurationSec = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::StunDurationSec);

        ApplyMagicDamage(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            static_cast<u8_t>(eSkillSlot::Q),
            ctx.skillRank,
            RankedDamage(baseDamage, damagePerRank, ctx.skillRank));
        if (bShouldStun)
            ApplyStun(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::Q, stunDurationSec);

    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
        {
            return;
        }

        CWorld& world = *ctx.pWorld;
        AnnieSimComponent& state = EnsureAnnieState(world, ctx.casterEntity);
        AddStunStack(state);
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 forward = ResolveForward(world, ctx.casterEntity, ctx.pCommand);
        const f32_t baseDamage = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::BaseDamage);
        const f32_t damagePerRank = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::DamagePerRank);
        const f32_t range = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Range);
        const f32_t halfAngleCos = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::HalfAngleCos);
        const f32_t stunDurationSec = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::StunDurationSec);

        const f32_t halfAngleRad = std::acos(
            std::clamp(halfAngleCos, -1.f, 1.f));
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCone(
                world,
                ctx.casterEntity,
                origin,
                forward,
                range,
                halfAngleRad);
        const bool_t bShouldStun =
            !targets.empty() && ConsumeStunReady(state);

        for (EntityID target : targets)
        {
            ApplyMagicDamage(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target,
                ctx.casterTeam,
                static_cast<u8_t>(eSkillSlot::W),
                ctx.skillRank,
                RankedDamage(baseDamage, damagePerRank, ctx.skillRank));
            if (bShouldStun)
                ApplyStun(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::W, stunDurationSec);
        }

    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        AnnieSimComponent& state = EnsureAnnieState(world, ctx.casterEntity);
        const f32_t shieldDurationSec = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::ShieldDurationSec);
        const f32_t shieldBaseAmount = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::ShieldBaseAmount);
        const f32_t shieldAmountPerRank = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::ShieldAmountPerRank);
        const f32_t shieldArmorPerRank = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::ShieldArmorPerRank);
        const f32_t moveSpeedMul = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::MoveSpeedMul);
        const f32_t shieldAmount =
            shieldBaseAmount + shieldAmountPerRank * static_cast<f32_t>(ctx.skillRank);

        state.fEShieldRemainingSec = shieldDurationSec;
        state.fEShieldAmount = shieldAmount;
        state.fEShieldMaxAmount = shieldAmount;

        BuffComponent& buffs = world.HasComponent<BuffComponent>(ctx.casterEntity)
            ? world.GetComponent<BuffComponent>(ctx.casterEntity)
            : world.AddComponent<BuffComponent>(ctx.casterEntity, BuffComponent{});

        BuffInstance buff{};
        buff.buffDefId = kEShieldBuffDefId;
        buff.source = ctx.casterEntity;
        buff.fDurationRemaining = shieldDurationSec;
        buff.stackCount = 1;
        buff.flatArmorPerStack = shieldArmorPerRank * static_cast<f32_t>(ctx.skillRank);
        buff.flatMrPerStack = shieldArmorPerRank * static_cast<f32_t>(ctx.skillRank);
        buff.moveSpeedMul = moveSpeedMul;
        CBuffSystem::AddOrRefresh(buffs, buff);

        if (world.HasComponent<StatComponent>(ctx.casterEntity))
            world.GetComponent<StatComponent>(ctx.casterEntity).bDirty = true;

        AddStunStack(state);
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
            return;

        CWorld& world = *ctx.pWorld;
        AnnieSimComponent& state = EnsureAnnieState(world, ctx.casterEntity);
        AddStunStack(state);
        const f32_t baseDamage = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage);
        const f32_t damagePerRank = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::DamagePerRank);
        const f32_t range = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Range);
        const f32_t radius = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Radius);
        const f32_t stunDurationSec = ResolveAnnieSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::StunDurationSec);
        const TibbersSpawnTuning tibbersTuning =
            ResolveTibbersSpawnTuning(world, *ctx.pTickCtx, ctx.casterEntity);
        const Vec3 center =
            ResolveGroundCastPosition(world, *ctx.pTickCtx, ctx.casterEntity, ctx.pCommand, range);

        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                world,
                ctx.casterEntity,
                center,
                radius);
        const bool_t bShouldStun =
            !targets.empty() && ConsumeStunReady(state);

        for (EntityID target : targets)
        {
            ApplyMagicDamage(
                world,
                *ctx.pTickCtx,
                ctx.casterEntity,
                target,
                ctx.casterTeam,
                static_cast<u8_t>(eSkillSlot::R),
                ctx.skillRank,
                RankedDamage(baseDamage, damagePerRank, ctx.skillRank));
            if (bShouldStun)
                ApplyStun(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::R, stunDurationSec);
        }

        SpawnTibbersMinion(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.casterTeam,
            center,
            ctx.skillRank,
            tibbersTuning);
    }
}

namespace AnnieGameSim
{
    bool_t CanCastDisintegrate(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        if (!GameplayStateQuery::CanReceiveCrowdControl(world, caster, target) ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return false;
        }

        const Vec3 casterPosition =
            world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPosition =
            world.GetComponent<TransformComponent>(target).GetPosition();
        const f32_t range = GameplayDefinitionQuery::ResolveSkillRange(
            world,
            caster,
            tc,
            eChampion::ANNIE,
            static_cast<u8_t>(eSkillSlot::Q));
        if (range <= 0.f ||
            WintersMath::DistanceSqXZ(casterPosition, targetPosition) >
                range * range)
        {
            return false;
        }
        return !tc.pWalkable ||
            tc.pWalkable->SegmentWalkableXZ(casterPosition, targetPosition, 0.f);
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<AnnieSimComponent>(
            std::function<void(EntityID, AnnieSimComponent&)>(
                [&](EntityID, AnnieSimComponent& state)
                {
                    if (state.fEShieldRemainingSec > 0.f)
                    {
                        state.fEShieldRemainingSec = (std::max)(0.f, state.fEShieldRemainingSec - tc.fDt);
                        if (state.fEShieldRemainingSec <= 0.f)
                            state.fEShieldAmount = 0.f;
                    }
                }));

        std::vector<EntityID> expiredTibbers;
        world.ForEach<AnnieTibbersComponent>(
            std::function<void(EntityID, AnnieTibbersComponent&)>(
                [&](EntityID tibbers, AnnieTibbersComponent& state)
                {
                    state.fRemainingSec -= tc.fDt;
                    if (state.fRemainingSec <= 0.f ||
                        state.owner == NULL_ENTITY ||
                        !world.IsAlive(state.owner))
                    {
                        expiredTibbers.push_back(tibbers);
                    }
                }));

        for (EntityID tibbers : expiredTibbers)
        {
            world.ForEach<AnnieSimComponent>(
                std::function<void(EntityID, AnnieSimComponent&)>(
                    [&](EntityID, AnnieSimComponent& state)
                    {
                        if (state.tibbersEntity == tibbers)
                            state.tibbersEntity = NULL_ENTITY;
                    }));
            DestroyTibbers(world, tc, tibbers);
        }
    }

    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ANNIE, GameplayHookVariant::Q_OnCastAccepted), &OnQ);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ANNIE, GameplayHookVariant::W_OnCastAccepted), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ANNIE, GameplayHookVariant::E_OnCastAccepted), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::ANNIE, GameplayHookVariant::R_OnCastAccepted), &OnR);

        s_bRegistered = true;
        std::cout << "[AnnieSim] hooks registered\n";
    }
}
