#include "Shared/GameSim/Champions/Annie/AnnieGameSim.h"

#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/NetAnimationComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/CoreComponents.h"
#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/MinionPerformanceComponents.h"
#include "ECS/Components/SpatialAgentComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/VisionComponents.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    constexpr u8_t kStunThreshold = 4;
    constexpr f32_t kStunDurationSec = 1.25f;

    constexpr f32_t kQBaseDamage = 80.f;
    constexpr f32_t kQDamagePerRank = 35.f;
    constexpr f32_t kWBaseDamage = 70.f;
    constexpr f32_t kWDamagePerRank = 45.f;
    constexpr f32_t kWRange = 6.0f;
    constexpr f32_t kWHalfAngleCos = 0.76604444f;
    constexpr f32_t kRBaseDamage = 150.f;
    constexpr f32_t kRDamagePerRank = 75.f;
    constexpr f32_t kRRange = 6.0f;
    constexpr f32_t kRRadius = 3.0f;

    constexpr f32_t kEShieldDurationSec = 3.0f;
    constexpr f32_t kEShieldBaseAmount = 50.f;
    constexpr f32_t kEShieldAmountPerRank = 45.f;
    constexpr f32_t kEShieldArmorPerRank = 5.f;
    constexpr f32_t kEShieldMoveMul = 1.10f;
    constexpr u32_t kEShieldBuffDefId = (static_cast<u32_t>(eChampion::ANNIE) << 16) | 3u;

    constexpr u8_t kTibbersRoleType = 4;
    // 0xff = any-lane: 서버 미니언 AI의 lane 타겟 필터를 우회한다 (소환수 전용).
    constexpr u8_t kTibbersLane = 0xff;
    constexpr f32_t kTibbersDurationSec = 45.f;
    constexpr f32_t kTibbersMoveSpeed = 5.2f;
    constexpr f32_t kTibbersAttackRange = 2.2f;
    constexpr f32_t kTibbersSightRange = 14.f;
    constexpr f32_t kTibbersAttackCooldownSec = 1.0f;
    constexpr f32_t kTibbersBaseAttackDamage = 40.f;
    constexpr f32_t kTibbersAttackDamagePerRank = 15.f;
    constexpr f32_t kTibbersBaseHp = 1000.f;
    constexpr f32_t kTibbersHpPerRank = 250.f;
    constexpr f32_t kTibbersRadius = 0.9f;

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
        eSkillSlot slot)
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
            kStunDurationSec);
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
        EntityID caster, const GameCommand* pCommand)
    {
        Vec3 origin{};
        if (world.HasComponent<TransformComponent>(caster))
            origin = world.GetComponent<TransformComponent>(caster).GetPosition();

        Vec3 pos = pCommand ? pCommand->groundPos : origin;
        const Vec3 delta{ pos.x - origin.x, 0.f, pos.z - origin.z };
        const f32_t distSq = delta.x * delta.x + delta.z * delta.z;
        if (distSq > kRRange * kRRange)
        {
            const f32_t invDist = 1.f / std::sqrtf(distSq);
            pos.x = origin.x + delta.x * invDist * kRRange;
            pos.z = origin.z + delta.z * invDist * kRRange;
        }
        else if (distSq <= 0.0001f)
        {
            const Vec3 forward = ResolveForward(world, caster, pCommand);
            pos.x = origin.x + forward.x * kRRange;
            pos.z = origin.z + forward.z * kRRange;
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

    void CollectCircleTargets(CWorld& world, const Vec3& center, f32_t radius,
        EntityID source, eTeam sourceTeam, std::vector<EntityID>& outTargets)
    {
        const f32_t radiusSq = radius * radius;
        auto tryAdd = [&](EntityID target, const Vec3& pos)
        {
            if (!IsEnemyDamageTarget(world, source, target, sourceTeam))
                return;

            const f32_t dx = pos.x - center.x;
            const f32_t dz = pos.z - center.z;
            if (dx * dx + dz * dz <= radiusSq)
                outTargets.push_back(target);
        };

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent&, TransformComponent& tf)
                {
                    tryAdd(target, tf.GetPosition());
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID target, MinionComponent&, TransformComponent& tf)
                {
                    tryAdd(target, tf.GetPosition());
                }));
    }

    void CollectConeTargets(CWorld& world, const Vec3& origin, const Vec3& forward,
        f32_t range, EntityID source, eTeam sourceTeam, std::vector<EntityID>& outTargets)
    {
        const f32_t rangeSq = range * range;
        auto tryAdd = [&](EntityID target, const Vec3& pos)
        {
            if (!IsEnemyDamageTarget(world, source, target, sourceTeam))
                return;

            const f32_t dx = pos.x - origin.x;
            const f32_t dz = pos.z - origin.z;
            const f32_t distSq = dx * dx + dz * dz;
            if (distSq <= 0.0001f || distSq > rangeSq)
                return;

            const f32_t invDist = 1.f / std::sqrtf(distSq);
            const f32_t dot = (dx * invDist) * forward.x + (dz * invDist) * forward.z;
            if (dot >= kWHalfAngleCos)
                outTargets.push_back(target);
        };

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID target, ChampionComponent&, TransformComponent& tf)
                {
                    tryAdd(target, tf.GetPosition());
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID target, MinionComponent&, TransformComponent& tf)
                {
                    tryAdd(target, tf.GetPosition());
                }));
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
        EntityID owner, eTeam ownerTeam, const Vec3& pos, u8_t rank)
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
        state.moveSpeed = kTibbersMoveSpeed;
        state.attackRange = kTibbersAttackRange;
        state.sightRange = kTibbersSightRange;
        state.attackDamage =
            kTibbersBaseAttackDamage + kTibbersAttackDamagePerRank * static_cast<f32_t>(rank);
        state.attackCooldownMax = kTibbersAttackCooldownSec;
        state.targetScanInterval = 0.16f;
        state.targetScanCooldown = 0.f;
        state.animUpdateInterval = 1.f / 15.f;
        world.AddComponent<MinionStateComponent>(tibbers, state);

        const f32_t maxHp = kTibbersBaseHp + kTibbersHpPerRank * static_cast<f32_t>(rank);
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
        spatial.kind = eSpatialKind::Minion;
        spatial.team = TeamByte(ownerTeam);
        spatial.radius = kTibbersRadius;
        world.AddComponent<SpatialAgentComponent>(tibbers, spatial);

        ColliderComponent collider{};
        collider.vHalfExtents = { kTibbersRadius, 1.5f, kTibbersRadius };
        collider.vOffset = { 0.f, 0.75f, 0.f };
        collider.bIsTrigger = false;
        world.AddComponent<ColliderComponent>(tibbers, collider);

        VisionSourceComponent vision{};
        vision.sightRange = kTibbersSightRange;
        world.AddComponent<VisionSourceComponent>(tibbers, vision);
        world.AddComponent<VisibilityComponent>(tibbers, BuildVisibleToAll());
        world.AddComponent<TargetableTag>(tibbers, TargetableTag{});
        world.AddComponent<MinionNavThrottleComponent>(tibbers, MinionNavThrottleComponent{});

        NetAnimationComponent anim{};
        anim.animId = static_cast<u16_t>(eNetAnimId::Run);
        anim.animStartTick = tc.tickIndex;
        world.AddComponent<NetAnimationComponent>(tibbers, anim);

        if (tc.pEntityMap)
        {
            NetEntityIdComponent net{};
            net.netId = tc.pEntityMap->IssueNew(tibbers);
            world.AddComponent<NetEntityIdComponent>(tibbers, net);
        }

        AnnieTibbersComponent tibbersState{};
        tibbersState.owner = owner;
        tibbersState.ownerTeam = ownerTeam;
        tibbersState.fRemainingSec = kTibbersDurationSec;
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
        AnnieSimComponent& state = EnsureAnnieState(world, ctx.casterEntity);
        const bool_t bShouldStun = ConsumeStunReady(state);
        const EntityID target = ctx.pCommand->targetEntity;

        ApplyMagicDamage(
            world,
            *ctx.pTickCtx,
            ctx.casterEntity,
            target,
            ctx.casterTeam,
            static_cast<u8_t>(eSkillSlot::Q),
            ctx.skillRank,
            RankedDamage(kQBaseDamage, kQDamagePerRank, ctx.skillRank));
        if (bShouldStun)
            ApplyStun(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::Q);

        AddStunStack(state);
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
        const bool_t bShouldStun = ConsumeStunReady(state);
        const Vec3 origin = world.GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 forward = ResolveForward(world, ctx.casterEntity, ctx.pCommand);

        std::vector<EntityID> targets;
        targets.reserve(8);
        CollectConeTargets(world, origin, forward, kWRange, ctx.casterEntity, ctx.casterTeam, targets);

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
                RankedDamage(kWBaseDamage, kWDamagePerRank, ctx.skillRank));
            if (bShouldStun)
                ApplyStun(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::W);
        }

        AddStunStack(state);
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld)
            return;

        CWorld& world = *ctx.pWorld;
        AnnieSimComponent& state = EnsureAnnieState(world, ctx.casterEntity);
        const f32_t shieldAmount =
            kEShieldBaseAmount + kEShieldAmountPerRank * static_cast<f32_t>(ctx.skillRank);

        state.fEShieldRemainingSec = kEShieldDurationSec;
        state.fEShieldAmount = shieldAmount;
        state.fEShieldMaxAmount = shieldAmount;

        BuffComponent& buffs = world.HasComponent<BuffComponent>(ctx.casterEntity)
            ? world.GetComponent<BuffComponent>(ctx.casterEntity)
            : world.AddComponent<BuffComponent>(ctx.casterEntity, BuffComponent{});

        BuffInstance buff{};
        buff.buffDefId = kEShieldBuffDefId;
        buff.source = ctx.casterEntity;
        buff.fDurationRemaining = kEShieldDurationSec;
        buff.stackCount = 1;
        buff.flatArmorPerStack = kEShieldArmorPerRank * static_cast<f32_t>(ctx.skillRank);
        buff.flatMrPerStack = kEShieldArmorPerRank * static_cast<f32_t>(ctx.skillRank);
        buff.moveSpeedMul = kEShieldMoveMul;
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
        const bool_t bShouldStun = ConsumeStunReady(state);
        const Vec3 center =
            ResolveGroundCastPosition(world, *ctx.pTickCtx, ctx.casterEntity, ctx.pCommand);

        std::vector<EntityID> targets;
        targets.reserve(8);
        CollectCircleTargets(world, center, kRRadius, ctx.casterEntity, ctx.casterTeam, targets);

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
                RankedDamage(kRBaseDamage, kRDamagePerRank, ctx.skillRank));
            if (bShouldStun)
                ApplyStun(world, *ctx.pTickCtx, ctx.casterEntity, target, eSkillSlot::R);
        }

        SpawnTibbersMinion(world, *ctx.pTickCtx, ctx.casterEntity, ctx.casterTeam, center, ctx.skillRank);
        AddStunStack(state);
    }
}

namespace AnnieGameSim
{
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
