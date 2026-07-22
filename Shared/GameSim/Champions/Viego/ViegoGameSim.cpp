#include "Shared/GameSim/Champions/Viego/ViegoGameSim.h"
#include "Shared/GameSim/Champions/Fiora/FioraGameSim.h"
#include "Shared/GameSim/Champions/Jax/JaxGameSim.h"
#include "Shared/GameSim/Champions/Yasuo/YasuoGameSim.h"
#include "Shared/GameSim/Champions/Yone/YoneGameSim.h"

#include "Shared/GameSim/Components/FormOverrideComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/NetEntityIdComponent.h"
#include "Shared/GameSim/Components/PoseActionStateHelpers.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Replication/EntityIdMap.h"
#include "Shared/GameSim/Core/Ecs/SpatialAgentComponent.h"

#include "Shared/GameSim/Components/AreaAuraComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/AsheSimComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/FioraSimComponent.h"
#include "Shared/GameSim/Components/IreliaSimComponent.h"
#include "Shared/GameSim/Components/JaxSimComponent.h"
#include "Shared/GameSim/Components/KalistaPassiveDashComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/LeeSinSimComponent.h"
#include "Shared/GameSim/Components/MasterYiComponent.h"
#include "Shared/GameSim/Components/MoveTargetComponent.h"
#include "Shared/GameSim/Components/SkillProjectileComponent.h"
#include "Shared/GameSim/Components/SkillChargeStateComponent.h"
#include "Shared/GameSim/Components/SylasSimComponent.h"
#include "Shared/GameSim/Components/ViegoSimComponent.h"
#include "Shared/GameSim/Components/YoneSimComponent.h"
#include "Shared/GameSim/Components/ZedSimComponent.h"
#include "Shared/GameSim/Definitions/ChampionGameplayDef.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Definitions/SkillGameplayDef.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
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
    struct ViegoDashComponent
    {
        Vec3 start{};
        Vec3 end{};
        f32_t elapsedSec = 0.f;
        f32_t durationSec = 0.f;
    };

    // Chrono Break: 익명 네임스페이스 컴포넌트는 소유 TU에서 자기등록한다.
    const bool_t s_bViegoDashKeyframeRegistered = []()
    {
        SimCheckpoint::KeyframeComponentRegistry::Get()
            .Register<ViegoDashComponent>("ViegoDashComponent");
        return true;
    }();

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

    void ClearPendingRanks(ViegoSimComponent& state)
    {
        for (u8_t slot = 0; slot < SkillRankComponent::kSlotCount; ++slot)
            state.pendingSkillRanks[slot] = 0u;
        state.bPendingHasSkillRanks = false;
    }

    void DestroyViegoSoul(CWorld& world, const TickContext& tc, EntityID soulEntity)
    {
        if (soulEntity == NULL_ENTITY || !world.IsAlive(soulEntity))
            return;

        if (tc.pEntityMap && world.HasComponent<NetEntityIdComponent>(soulEntity))
        {
            const NetEntityId netId =
                world.GetComponent<NetEntityIdComponent>(soulEntity).netId;
            if (netId != NULL_NET_ENTITY)
                tc.pEntityMap->Unbind(netId);
        }

        world.DestroyEntity(soulEntity);
    }

    template <typename T>
    void RemoveBorrowedComponent(CWorld& world, EntityID caster)
    {
        if (world.HasComponent<T>(caster))
            world.RemoveComponent<T>(caster);
    }

    void ClearBorrowedChampionRuntime(
        CWorld& world,
        EntityID caster,
        eChampion borrowedChampion,
        const TickContext* pTickCtx)
    {
        switch (borrowedChampion)
        {
        case eChampion::ANNIE: RemoveBorrowedComponent<AnnieSimComponent>(world, caster); break;
        case eChampion::ASHE: RemoveBorrowedComponent<AsheSimComponent>(world, caster); break;
        case eChampion::FIORA: FioraGameSim::CancelRuntime(world, caster); break;
        case eChampion::IRELIA: RemoveBorrowedComponent<IreliaSimComponent>(world, caster); break;
        case eChampion::JAX: JaxGameSim::CancelRuntime(world, caster); break;
        case eChampion::KALISTA:
            RemoveBorrowedComponent<KalistaPassiveDashComponent>(world, caster);
            break;
        case eChampion::KINDRED: RemoveBorrowedComponent<KindredSimComponent>(world, caster); break;
        case eChampion::LEESIN:
            RemoveBorrowedComponent<LeeSinSimComponent>(world, caster);
            RemoveBorrowedComponent<LeeSinDashComponent>(world, caster);
            break;
        case eChampion::MASTERYI: RemoveBorrowedComponent<MasterYiSimComponent>(world, caster); break;
        case eChampion::RIVEN: RemoveBorrowedComponent<RivenStateComponent>(world, caster); break;
        case eChampion::SYLAS:
            RemoveBorrowedComponent<SylasSimComponent>(world, caster);
            RemoveBorrowedComponent<SylasDashComponent>(world, caster);
            break;
        case eChampion::YASUO: YasuoGameSim::CancelRuntime(world, caster); break;
        case eChampion::YONE:
            YoneGameSim::CancelRuntime(world, caster, pTickCtx);
            break;
        case eChampion::ZED:
        {
            RemoveBorrowedComponent<ZedSimComponent>(world, caster);
            RemoveBorrowedComponent<ZedVanishComponent>(world, caster);
            std::vector<EntityID> marks;
            world.ForEach<ZedDeathMarkComponent>(
                std::function<void(EntityID, ZedDeathMarkComponent&)>(
                    [&](EntityID entity, ZedDeathMarkComponent& mark)
                    {
                        if (mark.entitySource == caster)
                            marks.push_back(entity);
                    }));
            for (EntityID entity : marks)
                world.RemoveComponent<ZedDeathMarkComponent>(entity);
            break;
        }
        default: break;
        }
    }

    void TickStoredSkillState(SkillStateComponent& skillState, f32_t dt)
    {
        for (u8_t slotIndex = static_cast<u8_t>(eSkillSlot::Q);
            slotIndex <= static_cast<u8_t>(eSkillSlot::E);
            ++slotIndex)
        {
            auto& slot = skillState.slots[slotIndex];
            if (slot.cooldownRemaining > 0.f)
            {
                slot.cooldownRemaining = (std::max)(0.f, slot.cooldownRemaining - dt);
                if (slot.cooldownRemaining <= 0.f)
                    slot.cooldownDuration = 0.f;
            }
            else
            {
                slot.cooldownDuration = 0.f;
            }

            if (slot.currentStage == 1u && slot.stageWindow > 0.f)
            {
                slot.stageWindow = (std::max)(0.f, slot.stageWindow - dt);
                if (slot.stageWindow <= 0.f)
                    slot.currentStage = 0u;
            }
        }
    }

    void ClearViegoPossession(
        CWorld& world,
        EntityID caster,
        ViegoSimComponent& state,
        const TickContext* pTickCtx = nullptr)
    {
        ClearBorrowedChampionRuntime(
            world,
            caster,
            state.possessionChampion,
            pTickCtx);

        if (state.bHasOriginalSkillRanks &&
            world.HasComponent<SkillRankComponent>(caster))
        {
            auto& ranks = world.GetComponent<SkillRankComponent>(caster);
            for (u8_t slot = static_cast<u8_t>(eSkillSlot::Q);
                slot <= static_cast<u8_t>(eSkillSlot::E);
                ++slot)
            {
                ranks.ranks[slot] = state.originalSkillRanks.ranks[slot];
            }
        }

        if (state.bHasOriginalSkillState &&
            world.HasComponent<SkillStateComponent>(caster))
        {
            auto& skillState = world.GetComponent<SkillStateComponent>(caster);
            for (u8_t slot = static_cast<u8_t>(eSkillSlot::Q);
                slot <= static_cast<u8_t>(eSkillSlot::E);
                ++slot)
            {
                skillState.slots[slot] = state.originalSkillState.slots[slot];
            }
        }

        state.bPossessionActive = false;
        state.bPossessionPending = false;
        state.pendingPossessionChampion = eChampion::END;
        state.pendingPossessedTarget = NULL_ENTITY;
        ClearPendingRanks(state);
        state.possessionApplyTimerSec = 0.f;
        state.possessedTarget = NULL_ENTITY;
        state.possessionChampion = eChampion::END;
        state.bHasOriginalSkillRanks = false;
        state.bHasOriginalSkillState = false;

        if (world.HasComponent<FormOverrideComponent>(caster))
            world.RemoveComponent<FormOverrideComponent>(caster);
    }

    void ApplyViegoPossession(
        CWorld& world,
        EntityID caster,
        ViegoSimComponent& state,
        const TickContext* pTickCtx)
    {
        if (!IsValidPossessionChampion(state.pendingPossessionChampion))
        {
            ClearViegoPossession(world, caster, state, pTickCtx);
            return;
        }

        if (!state.bPossessionActive)
        {
            if (world.HasComponent<SkillRankComponent>(caster))
            {
                state.originalSkillRanks = world.GetComponent<SkillRankComponent>(caster);
                state.bHasOriginalSkillRanks = true;
            }
            if (world.HasComponent<SkillStateComponent>(caster))
            {
                state.originalSkillState = world.GetComponent<SkillStateComponent>(caster);
                state.bHasOriginalSkillState = true;
            }
        }

        if (world.HasComponent<SkillRankComponent>(caster))
        {
            auto& ranks = world.GetComponent<SkillRankComponent>(caster);
            for (u8_t slot = static_cast<u8_t>(eSkillSlot::Q);
                slot <= static_cast<u8_t>(eSkillSlot::E);
                ++slot)
            {
                const u8_t stolenRank = state.bPendingHasSkillRanks
                    ? state.pendingSkillRanks[slot]
                    : 0u;
                ranks.ranks[slot] = (std::max)(static_cast<u8_t>(1u), stolenRank);
            }
        }

        if (world.HasComponent<SkillStateComponent>(caster))
        {
            auto& skillState = world.GetComponent<SkillStateComponent>(caster);
            for (u8_t slot = static_cast<u8_t>(eSkillSlot::Q);
                slot <= static_cast<u8_t>(eSkillSlot::E);
                ++slot)
            {
                skillState.slots[slot] = SkillSlotRuntime{};
            }
        }

        FormOverrideComponent form{};
        form.baseChampion = eChampion::VIEGO;
        form.visualChampion = state.pendingPossessionChampion;
        form.skillChampion = state.pendingPossessionChampion;
        form.skillSlotMask = static_cast<u8_t>(
            (1u << static_cast<u8_t>(eSkillSlot::BasicAttack)) |
            (1u << static_cast<u8_t>(eSkillSlot::Q)) |
            (1u << static_cast<u8_t>(eSkillSlot::W)) |
            (1u << static_cast<u8_t>(eSkillSlot::E)));
        form.fRemainingSec = -1.f;
        form.bActive = true;

        if (world.HasComponent<FormOverrideComponent>(caster))
            world.GetComponent<FormOverrideComponent>(caster) = form;
        else
            world.AddComponent<FormOverrideComponent>(caster, form);

        state.bPossessionActive = true;
        state.bPossessionPending = false;
        state.possessedTarget = state.pendingPossessedTarget;
        state.possessionChampion = state.pendingPossessionChampion;
        state.pendingPossessionChampion = eChampion::END;
        state.pendingPossessedTarget = NULL_ENTITY;
        ClearPendingRanks(state);
        state.possessionApplyTimerSec = 0.f;
    }

    f32_t ResolveViegoSkillRange(
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
            eChampion::VIEGO,
            static_cast<u8_t>(slot));
    }

    f32_t ResolveViegoSkillEffectParam(
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

    Vec3 ResolveGroundDashEnd(
        const GameplayHookContext& ctx,
        const Vec3& origin,
        const Vec3& fallbackDirection,
        f32_t maxRange)
    {
        Vec3 end = ctx.pCommand
            ? ctx.pCommand->groundPos
            : Vec3{
                origin.x + fallbackDirection.x * maxRange,
                origin.y,
                origin.z + fallbackDirection.z * maxRange };
        end.y = origin.y;

        const Vec3 delta{ end.x - origin.x, 0.f, end.z - origin.z };
        const f32_t distanceSq = delta.x * delta.x + delta.z * delta.z;
        const f32_t maxRangeSq = maxRange * maxRange;
        if (maxRange > 0.f && distanceSq > maxRangeSq)
        {
            const f32_t inverseDistance = 1.f / std::sqrt(distanceSq);
            end.x = origin.x + delta.x * inverseDistance * maxRange;
            end.z = origin.z + delta.z * inverseDistance * maxRange;
        }

        if (ctx.pTickCtx && ctx.pTickCtx->pWalkable && ctx.pWorld)
        {
            Vec3 clampedEnd = end;
            const f32_t casterRadius = GameplayStateQuery::ResolveGameplayRadius(
                *ctx.pWorld,
                ctx.casterEntity);
            if (ctx.pTickCtx->pWalkable->TryClampMoveSegmentXZ(
                origin,
                end,
                casterRadius,
                clampedEnd))
            {
                end = clampedEnd;
            }
            else
            {
                end = origin;
            }
        }

        return end;
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
        u8_t rank,
        f32_t skillDamageScale = 1.f)
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
        request.skillDamageScale = skillDamageScale;
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
        u8_t rank,
        f32_t skillDamageScale = 1.f)
    {
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInSegment(
                world,
                source,
                start,
                end,
                radius);
        for (EntityID target : targets)
            EnqueuePhysicalDamage(
                world, source, target, sourceTeam, amount, slot, rank, skillDamageScale);
    }

    void ApplyLineStun(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        const Vec3& start,
        const Vec3& end,
        f32_t radius,
        f32_t stunDurationSec)
    {
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInSegment(
                world,
                source,
                start,
                end,
                radius);
        for (EntityID target : targets)
        {
            GameplayStatus::ApplyStun(
                world,
                tc,
                target,
                source,
                eChampion::VIEGO,
                eSkillSlot::W,
                stunDurationSec);
        }
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
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                world,
                source,
                origin,
                radius);
        for (EntityID target : targets)
            EnqueuePhysicalDamage(world, source, target, sourceTeam, amount, slot, rank);
    }

    void ApplyCircleSlow(
        CWorld& world,
        const TickContext& tc,
        EntityID source,
        const Vec3& origin,
        f32_t radius,
        f32_t slowDurationSec,
        f32_t moveSpeedMul)
    {
        const std::vector<EntityID> targets =
            GameplayStateQuery::CollectEnemyMobileUnitsInCircle(
                world,
                source,
                origin,
                radius);
        for (EntityID target : targets)
        {
            GameplayStatus::ApplySlow(
                world,
                tc,
                target,
                source,
                eChampion::VIEGO,
                eSkillSlot::R,
                slowDurationSec,
                moveSpeedMul);
        }
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
        projectile.sourceHandle = world.GetEntityHandle(caster);
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
        const f32_t range = ResolveViegoSkillRange(ctx, eSkillSlot::Q);
        const f32_t radius = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::Radius);
        const f32_t damage = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::Q,
            eSkillEffectParamId::BaseDamage);
        Vec3 end{ origin.x + dir.x * range, origin.y, origin.z + dir.z * range };
        if (ctx.pTickCtx->pWalkable)
        {
            Vec3 clampedEnd = end;
            const f32_t casterRadius = GameplayStateQuery::ResolveGameplayRadius(
                *ctx.pWorld,
                ctx.casterEntity);
            if (ctx.pTickCtx->pWalkable->TryClampMoveSegmentXZ(
                origin,
                end,
                casterRadius,
                clampedEnd))
            {
                end = clampedEnd;
            }
            else
            {
                end = origin;
            }
        }
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
        f32_t dashRange = ResolveViegoSkillRange(ctx, eSkillSlot::W);
        const f32_t dashDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::DashDurationSec);
        const f32_t radius = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::Radius);
        f32_t damage = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::BaseDamage);
        f32_t skillDamageScale = 1.f;
        f32_t stunDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::W,
            eSkillEffectParamId::StunDurationSec);
        if (ctx.pWorld->HasComponent<SkillChargeStateComponent>(ctx.casterEntity))
        {
            const f32_t ratio =
                ctx.pWorld->GetComponent<SkillChargeStateComponent>(
                    ctx.casterEntity).chargeRatio;
            if (const SkillGameplayDef* skill = GameplayDefinitionQuery::FindSkill(
                *ctx.pWorld,
                ctx.casterEntity,
                *ctx.pTickCtx,
                eChampion::VIEGO,
                static_cast<u8_t>(eSkillSlot::W)))
            {
                const f32_t rangeScale = ResolveSkillChargeValue(
                    skill->charge.minRangeScale,
                    skill->charge.maxRangeScale,
                    ratio);
                skillDamageScale = ResolveSkillChargeValue(
                    skill->charge.minDamageScale,
                    skill->charge.maxDamageScale,
                    ratio);
                dashRange *= rangeScale;
                stunDurationSec = ResolveSkillChargeValue(
                    skill->charge.minStunSec,
                    skill->charge.maxStunSec,
                    ratio);
            }
        }

        Vec3 vEnd
        {
            vOrigin.x + vDir.x * dashRange,
            vOrigin.y,
            vOrigin.z + vDir.z * dashRange
        };
        if (ctx.pTickCtx->pWalkable)
        {
            Vec3 clampedEnd = vEnd;
            const f32_t casterRadius = GameplayStateQuery::ResolveGameplayRadius(
                *ctx.pWorld,
                ctx.casterEntity);
            if (ctx.pTickCtx->pWalkable->TryClampMoveSegmentXZ(
                vOrigin,
                vEnd,
                casterRadius,
                clampedEnd))
            {
                vEnd = clampedEnd;
            }
            else
            {
                vEnd = vOrigin;
            }
        }

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
            ctx.skillRank,
            skillDamageScale
        );
        ApplyLineStun(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
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
            eSkillEffectParamId::Radius);
        const f32_t mistDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::EffectDurationSec,
            state.mistDurationSec);
        const f32_t tickIntervalSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::TickIntervalSec);
        const f32_t refreshDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::E,
            eSkillEffectParamId::RefreshDurationSec);

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
            ClearViegoPossession(
                *ctx.pWorld,
                ctx.casterEntity,
                viegoState,
                ctx.pTickCtx);
        }

        const Vec3 origin = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        const Vec3 dir = ResolveDirection(ctx);
        const f32_t range = ResolveViegoSkillRange(ctx, eSkillSlot::R);
        const f32_t radius = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::Radius);
        const f32_t damage = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::BaseDamage);
        const f32_t dashDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::DashDurationSec);
        const f32_t slowDurationSec = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::SlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveViegoSkillEffectParam(
            ctx,
            eSkillSlot::R,
            eSkillEffectParamId::MoveSpeedMul);
        const Vec3 end = ResolveGroundDashEnd(ctx, origin, dir, range);
        const Vec3 landingDirection = WintersMath::NormalizeXZ(Vec3{
            end.x - origin.x,
            0.f,
            end.z - origin.z });
        RotateToward(
            *ctx.pWorld,
            ctx.casterEntity,
            (landingDirection.x != 0.f || landingDirection.z != 0.f)
                ? landingDirection
                : dir);
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
            end,
            radius,
            slowDurationSec,
            slowMoveSpeedMul);

        std::cout << "[ViegoSim] R dash caster=" << ctx.casterEntity << "\n";
    }
}

namespace ViegoGameSim
{
    void ClearPossession(
        CWorld& world,
        EntityID viegoEntity,
        const TickContext* pTickCtx)
    {
        if (viegoEntity == NULL_ENTITY ||
            !world.HasComponent<ViegoSimComponent>(viegoEntity))
        {
            return;
        }

        ClearViegoPossession(
            world,
            viegoEntity,
            world.GetComponent<ViegoSimComponent>(viegoEntity),
            pTickCtx);
    }

    void TrySpawnSoulForKill(CWorld& world, const TickContext& tc,
        EntityID killer, EntityID deadChampion)
    {
        if (deadChampion == NULL_ENTITY ||
            world.HasComponent<ViegoSoulComponent>(deadChampion) ||
            !world.HasComponent<ChampionComponent>(deadChampion) ||
            !world.HasComponent<TransformComponent>(deadChampion))
            return;

        if (killer == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(killer) ||
            !world.HasComponent<HealthComponent>(killer))
        {
            return;
        }

        const auto& dead = world.GetComponent<ChampionComponent>(deadChampion);
        const auto& killerChampion = world.GetComponent<ChampionComponent>(killer);
        const auto& killerHealth = world.GetComponent<HealthComponent>(killer);
        if (killerChampion.id != eChampion::VIEGO ||
            killerChampion.team == dead.team ||
            killerHealth.bIsDead ||
            killerHealth.fCurrent <= 0.f)
        {
            return;
        }

        const ChampionGameplayDef* championDef =
            GameplayDefinitionQuery::FindChampion(world, NULL_ENTITY, tc, eChampion::VIEGO);
        if (!championDef || !championDef->passiveSoul.bValid)
            return;

        const EntityID soulEntity = world.CreateEntity();

        ViegoSoulComponent soul{};
        soul.deadChampion = deadChampion;
        soul.eligibleViego = killer;
        soul.champion = dead.id;
        soul.eligibleTeam = killerChampion.team;
        soul.fRemainingSec = championDef->passiveSoul.lifetimeSec;
        if (world.HasComponent<SkillRankComponent>(deadChampion))
        {
            const auto& ranks = world.GetComponent<SkillRankComponent>(deadChampion);
            for (u8_t slot = 0; slot < SkillRankComponent::kSlotCount; ++slot)
                soul.skillRanks[slot] = ranks.ranks[slot];
            soul.bHasSkillRanks = true;
        }
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
        spatial.kind = eSpatialKind::Character;
        spatial.team = static_cast<u8_t>(dead.team);
        spatial.radius = championDef->passiveSoul.radius;
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
            DestroyViegoSoul(world, tc, entity);


        std::vector<EntityID> finishedDashes;
        world.ForEach<ViegoDashComponent, TransformComponent>(
            std::function<void(EntityID, ViegoDashComponent&, TransformComponent&)>(
                [&](EntityID entity, ViegoDashComponent& dash, TransformComponent& transform)
                {
                    ClearMove(world, entity);

                    if (!GameplayStateQuery::CanMove(world, entity))
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
            if (world.HasComponent<ViegoDashComponent>(entity))
                SnapDashArrivalToWalkable(world, tc, entity,
                    world.GetComponent<ViegoDashComponent>(entity).start);
            world.RemoveComponent<ViegoDashComponent>(entity);
        }

        world.ForEach<ViegoSimComponent>(
            std::function<void(EntityID, ViegoSimComponent&)>(
                [&](EntityID entity, ViegoSimComponent& state)
                {
                    if (world.HasComponent<HealthComponent>(entity))
                    {
                        const auto& health = world.GetComponent<HealthComponent>(entity);
                        if (health.bIsDead || health.fCurrent <= 0.f)
                        {
                            ClearViegoPossession(world, entity, state, &tc);
                            return;
                        }
                    }

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
                            ApplyViegoPossession(world, entity, state, &tc);
                    }

                    if (state.bPossessionActive &&
                        !world.HasComponent<FormOverrideComponent>(entity))
                    {
                        ClearViegoPossession(world, entity, state, &tc);
                    }
                    else if (state.bPossessionActive && state.bHasOriginalSkillState)
                    {
                        TickStoredSkillState(state.originalSkillState, tc.fDt);
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
