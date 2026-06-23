#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionQuery.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectRequests.h"

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Components/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"

#include <algorithm>
#include <functional>
#include <iostream>

namespace
{
    constexpr f32_t kKindredWDurationSec = 4.0f;
    constexpr f32_t kKindredWRadius = 4.0f;
    constexpr f32_t kKindredWTickSec = 0.60f;
    constexpr f32_t kKindredWDamage = 35.f;
    constexpr f32_t kKindredEMarkDurationSec = 4.0f;
    constexpr f32_t kKindredESlowDurationSec = 1.0f;
    constexpr f32_t kKindredESlowMoveSpeedMul = 0.65f;
    constexpr f32_t kKindredEPounceBonusDamage = 80.f;
    constexpr f32_t kKindredRDurationSec = 4.0f;
    constexpr f32_t kKindredRRadius = 6.0f;
    constexpr f32_t kKindredRMinHealth = 1.0f;
    constexpr f32_t kKindredRHealByRank[3] = { 250.f, 325.f, 400.f };
    constexpr f32_t kKindredRHealBaseAmount = 250.f;
    constexpr f32_t kKindredRHealAmountPerRank = 75.f;

    f32_t ResolveKindredSkillEffectParam(
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
            eChampion::KINDRED,
            static_cast<u8_t>(slot),
            param,
            fallbackValue);
    }

    f32_t ResolveKindredSkillEffectParam(
        const GameplayHookContext& ctx,
        eSkillSlot slot,
        eSkillEffectParamId param,
        f32_t fallbackValue)
    {
        if (!ctx.pWorld || !ctx.pTickCtx)
        {
            return fallbackValue;
        }

        return ResolveKindredSkillEffectParam(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            slot,
            param,
            fallbackValue);
    }

    KindredSimComponent& EnsureKindredState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<KindredSimComponent>(caster))
            world.AddComponent<KindredSimComponent>(caster, KindredSimComponent{});

        return world.GetComponent<KindredSimComponent>(caster);
    }

    f32_t ResolveKindredRHealAmount(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        u8_t rank)
    {
        const u8_t clampedRank = (rank == 0u)
            ? 1u
            : static_cast<u8_t>(std::min<u32_t>(rank, 3u));
        const f32_t baseAmount = ResolveKindredSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSkillEffectParamId::HealBaseAmount,
            kKindredRHealBaseAmount);
        const f32_t amountPerRank = ResolveKindredSkillEffectParam(
            world,
            tc,
            caster,
            eSkillSlot::R,
            eSkillEffectParamId::HealAmountPerRank,
            kKindredRHealAmountPerRank);
        return baseAmount + amountPerRank * static_cast<f32_t>(clampedRank - 1u);
    }

    bool_t HasLiveHealth(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.HasComponent<HealthComponent>(entity))
            return false;

        const HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        return !health.bIsDead && health.fCurrent > 0.f;
    }

    void MirrorChampionHealth(CWorld& world, EntityID entity, const HealthComponent& health)
    {
        if (!world.HasComponent<ChampionComponent>(entity))
            return;

        ChampionComponent& champion = world.GetComponent<ChampionComponent>(entity);
        champion.hp = health.fCurrent;
        champion.maxHp = health.fMaximum;
    }

    void ClearKindredRHealthFloor(CWorld& world, EntityID caster)
    {
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent&, TransformComponent&)
                {
                    if (!world.HasComponent<KindredHealthFloorComponent>(entity))
                        return;

                    const KindredHealthFloorComponent& floor =
                        world.GetComponent<KindredHealthFloorComponent>(entity);
                    if (floor.sourceEntity == caster)
                        world.RemoveComponent<KindredHealthFloorComponent>(entity);
                }));
    }

    void RefreshKindredRHealthFloor(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        const KindredSimComponent& state)
    {
        const f32_t radius = ResolveKindredSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::Radius, kKindredRRadius);
        const f32_t minHealth = ResolveKindredSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::MinHealthAmount, kKindredRMinHealth);
        const f32_t radiusSq = radius * radius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent&, TransformComponent& transform)
                {
                    const bool_t bInside =
                        WintersMath::DistanceSqXZ(state.vRCenter, transform.GetPosition()) <= radiusSq;
                    if (!bInside || !HasLiveHealth(world, entity))
                    {
                        if (world.HasComponent<KindredHealthFloorComponent>(entity))
                        {
                            const KindredHealthFloorComponent& floor =
                                world.GetComponent<KindredHealthFloorComponent>(entity);
                            if (floor.sourceEntity == caster)
                                world.RemoveComponent<KindredHealthFloorComponent>(entity);
                        }
                        return;
                    }

                    auto& floor = world.HasComponent<KindredHealthFloorComponent>(entity)
                        ? world.GetComponent<KindredHealthFloorComponent>(entity)
                        : world.AddComponent<KindredHealthFloorComponent>(
                            entity,
                            KindredHealthFloorComponent{});
                    floor.sourceEntity = caster;
                    floor.fRemainingSec = state.fRRemainingSec;
                    floor.fMinHealth = minHealth;

                    auto& health = world.GetComponent<HealthComponent>(entity);
                    if (health.fCurrent < floor.fMinHealth)
                    {
                        health.fCurrent = std::min(health.fMaximum, floor.fMinHealth);
                        health.bIsDead = false;
                        MirrorChampionHealth(world, entity, health);
                    }
                }));
    }

    void HealKindredRTargets(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        const KindredSimComponent& state)
    {
        if (state.fRHealAmount <= 0.f)
            return;

        const f32_t radius = ResolveKindredSkillEffectParam(
            world, tc, caster, eSkillSlot::R, eSkillEffectParamId::Radius, kKindredRRadius);
        const f32_t radiusSq = radius * radius;
        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent&, TransformComponent& transform)
                {
                    if (WintersMath::DistanceSqXZ(state.vRCenter, transform.GetPosition()) > radiusSq)
                        return;
                    if (!HasLiveHealth(world, entity))
                        return;

                    auto& health = world.GetComponent<HealthComponent>(entity);
                    health.fCurrent = std::min(
                        health.fMaximum,
                        health.fCurrent + state.fRHealAmount);
                    health.bIsDead = false;
                    MirrorChampionHealth(world, entity, health);
                }));
    }

    void TickKindredR(CWorld& world, const TickContext& tc, EntityID caster, KindredSimComponent& state)
    {
        if (state.fRRemainingSec <= 0.f)
            return;

        state.fRRemainingSec = std::max(0.f, state.fRRemainingSec - tc.fDt);
        if (state.fRRemainingSec > 0.f)
        {
            RefreshKindredRHealthFloor(world, tc, caster, state);
            return;
        }

        HealKindredRTargets(world, tc, caster, state);
        ClearKindredRHealthFloor(world, caster);
        state.fRHealAmount = 0.f;

        std::cout << "[KindredSim] R lamb respite ended caster="
            << caster << "\n";
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
        request.skillId = static_cast<u16_t>((static_cast<u32_t>(eChampion::KINDRED) << 8) | slot);
        request.rank = rank;
        request.flags = DamageFlag_OnHit;
        EnqueueDamageRequest(world, request);
    }

    EntityID FindWolfTarget(
        CWorld& world,
        EntityID caster,
        eTeam casterTeam,
        const Vec3& center,
        f32_t radius)
    {
        EntityID bestTarget = NULL_ENTITY;
        f32_t bestDistanceSq = radius * radius;

        world.ForEach<ChampionComponent, TransformComponent>(
            std::function<void(EntityID, ChampionComponent&, TransformComponent&)>(
                [&](EntityID entity, ChampionComponent& champion, TransformComponent& transform)
                {
                    if (entity == caster || champion.team == casterTeam)
                        return;

                    const f32_t distanceSq = WintersMath::DistanceSqXZ(center, transform.GetPosition());
                    if (distanceSq <= bestDistanceSq)
                    {
                        bestDistanceSq = distanceSq;
                        bestTarget = entity;
                    }
                }));

        world.ForEach<MinionComponent, TransformComponent>(
            std::function<void(EntityID, MinionComponent&, TransformComponent&)>(
                [&](EntityID entity, MinionComponent& minion, TransformComponent& transform)
                {
                    if (minion.team == casterTeam)
                        return;

                    const f32_t distanceSq = WintersMath::DistanceSqXZ(center, transform.GetPosition());
                    if (distanceSq <= bestDistanceSq)
                    {
                        bestDistanceSq = distanceSq;
                        bestTarget = entity;
                    }
                }));

        return bestTarget;
    }

    void ApplyMountingDreadSlow(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target)
    {
        const f32_t slowDurationSec = ResolveKindredSkillEffectParam(
            world, tc, caster, eSkillSlot::E, eSkillEffectParamId::SlowDurationSec, kKindredESlowDurationSec);
        const f32_t slowMoveSpeedMul = ResolveKindredSkillEffectParam(
            world, tc, caster, eSkillSlot::E, eSkillEffectParamId::MoveSpeedMul, kKindredESlowMoveSpeedMul);
        GameplayStatus::ApplySlow(
            world,
            tc,
            target,
            caster,
            eChampion::KINDRED,
            eSkillSlot::E,
            slowDurationSec,
            slowMoveSpeedMul);
    }

    u16_t BuildKindredEEffectFlags(u8_t stage, u8_t rank)
    {
        return static_cast<u16_t>(
            (static_cast<u16_t>(stage & 0x0fu) << 12) |
            (static_cast<u16_t>(rank & 0x0fu) << 8) |
            static_cast<u16_t>(eSkillSlot::E));
    }

    Vec3 ResolveEntityPosition(CWorld& world, EntityID entity)
    {
        if (entity != NULL_ENTITY && world.HasComponent<TransformComponent>(entity))
            return world.GetComponent<TransformComponent>(entity).GetPosition();
        return {};
    }

    Vec3 ResolveEffectDirection(CWorld& world, EntityID caster, EntityID target)
    {
        if (caster == NULL_ENTITY || target == NULL_ENTITY ||
            !world.HasComponent<TransformComponent>(caster) ||
            !world.HasComponent<TransformComponent>(target))
        {
            return {};
        }

        const Vec3 casterPos = world.GetComponent<TransformComponent>(caster).GetPosition();
        const Vec3 targetPos = world.GetComponent<TransformComponent>(target).GetPosition();
        return WintersMath::DirectionXZ(casterPos, targetPos);
    }

    void EmitMountingDreadStackEffect(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        u8_t stackCount)
    {
        const u8_t clampedStack =
            static_cast<u8_t>(std::min<u32_t>(3u, stackCount));
        if (clampedStack == 0u || target == NULL_ENTITY)
            return;

        ReplicatedEventComponent event{};
        event.kind = eReplicatedEventKind::EffectTrigger;
        event.sourceEntity = caster;
        event.targetEntity = target;
        event.effectId = MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::E_CastFrame);
        event.skillId = static_cast<u16_t>(
            (static_cast<u32_t>(eChampion::KINDRED) << 8) |
            static_cast<u8_t>(eSkillSlot::E));
        event.slot = static_cast<u8_t>(eSkillSlot::E);
        event.rank = 1;
        event.flags = BuildKindredEEffectFlags(
            static_cast<u8_t>(clampedStack + 1u),
            event.rank);
        event.position = ResolveEntityPosition(world, target);
        event.direction = ResolveEffectDirection(world, caster, target);
        event.durationMs = clampedStack >= 3u ? 850u : 620u;
        event.startTick = tc.tickIndex;

        EnqueueReplicatedEvent(world, event);
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        Vec3 center = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        if (ctx.pCommand)
            center = ctx.pCommand->groundPos;

        KindredSimComponent& state = EnsureKindredState(*ctx.pWorld, ctx.casterEntity);
        state.fWRemainingSec = ResolveKindredSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::EffectDurationSec, kKindredWDurationSec);
        state.fWTickAccumulatorSec = ResolveKindredSkillEffectParam(
            ctx, eSkillSlot::W, eSkillEffectParamId::TickIntervalSec, kKindredWTickSec);
        state.vWCenter = center;

        std::cout << "[KindredSim] W wolf frenzy caster="
            << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || !ctx.pTickCtx ||
            ctx.pCommand->targetEntity == NULL_ENTITY)
            return;

        KindredSimComponent& state = EnsureKindredState(*ctx.pWorld, ctx.casterEntity);
        state.markedTarget = ctx.pCommand->targetEntity;
        state.mountingDreadHitCount = 0;
        state.fEMarkRemainingSec = ResolveKindredSkillEffectParam(
            ctx, eSkillSlot::E, eSkillEffectParamId::MarkDurationSec, kKindredEMarkDurationSec);
        ApplyMountingDreadSlow(
            *ctx.pWorld,
            *ctx.pTickCtx,
            ctx.casterEntity,
            ctx.pCommand->targetEntity);

        std::cout << "[KindredSim] E mounting dread mark caster="
            << ctx.casterEntity << " target=" << ctx.pCommand->targetEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pTickCtx ||
            !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        Vec3 center = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        if (ctx.pCommand)
            center = ctx.pCommand->groundPos;

        KindredSimComponent& state = EnsureKindredState(*ctx.pWorld, ctx.casterEntity);
        state.fRRemainingSec = ResolveKindredSkillEffectParam(
            ctx, eSkillSlot::R, eSkillEffectParamId::EffectDurationSec, kKindredRDurationSec);
        state.fRHealAmount = ResolveKindredRHealAmount(
            *ctx.pWorld, *ctx.pTickCtx, ctx.casterEntity, ctx.skillRank);
        state.vRCenter = center;
        RefreshKindredRHealthFloor(*ctx.pWorld, *ctx.pTickCtx, ctx.casterEntity, state);

        std::cout << "[KindredSim] R lamb respite caster="
            << ctx.casterEntity << " center=("
            << center.x << ", " << center.y << ", " << center.z << ")\n";
    }
}

namespace KindredGameSim
{
    void RegisterHooks()
    {
        static bool_t s_bRegistered = false;
        if (s_bRegistered)
            return;

        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::W_CastFrame), &OnW);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::E_CastFrame), &OnE);
        CGameplayHookRegistry::Instance().Register(
            MakeGameplayHookId(eChampion::KINDRED, GameplayHookVariant::R_CastFrame), &OnR);

        s_bRegistered = true;
        std::cout << "[KindredSim] hooks registered\n";
    }

    void Tick(CWorld& world, const TickContext& tc)
    {
        world.ForEach<KindredSimComponent, ChampionComponent>(
            std::function<void(EntityID, KindredSimComponent&, ChampionComponent&)>(
                [&](EntityID entity, KindredSimComponent& state, ChampionComponent& champion)
                {
                    TickKindredR(world, tc, entity, state);

                    if (state.fEMarkRemainingSec > 0.f)
                    {
                        state.fEMarkRemainingSec = std::max(0.f, state.fEMarkRemainingSec - tc.fDt);
                        if (state.fEMarkRemainingSec <= 0.f)
                        {
                            state.markedTarget = NULL_ENTITY;
                            state.mountingDreadHitCount = 0;
                        }
                    }

                    if (state.fWRemainingSec <= 0.f)
                        return;

                    state.fWRemainingSec = std::max(0.f, state.fWRemainingSec - tc.fDt);
                    state.fWTickAccumulatorSec -= tc.fDt;
                    if (state.fWTickAccumulatorSec > 0.f)
                        return;

                    const f32_t tickIntervalSec = ResolveKindredSkillEffectParam(
                        world, tc, entity, eSkillSlot::W,
                        eSkillEffectParamId::TickIntervalSec, kKindredWTickSec);
                    state.fWTickAccumulatorSec += tickIntervalSec;
                    const f32_t wolfRadius = ResolveKindredSkillEffectParam(
                        world, tc, entity, eSkillSlot::W,
                        eSkillEffectParamId::Radius, kKindredWRadius);
                    const EntityID target =
                        FindWolfTarget(world, entity, champion.team, state.vWCenter, wolfRadius);
                    if (target != NULL_ENTITY)
                    {
                        const f32_t wDamage = ResolveKindredSkillEffectParam(
                            world, tc, entity, eSkillSlot::W,
                            eSkillEffectParamId::BaseDamage, kKindredWDamage);
                        EnqueuePhysicalDamage(
                            world,
                            entity,
                            target,
                            champion.team,
                            wDamage,
                            static_cast<u8_t>(eSkillSlot::W),
                            1);
                    }
                }));
    }

    f32_t GetUltimateDurationSec()
    {
        return kKindredRDurationSec;
    }

    f32_t ConsumeBasicAttackDamage(
        CWorld& world,
        const TickContext& tc,
        EntityID caster,
        EntityID target,
        eTeam casterTeam,
        f32_t baseDamage)
    {
        (void)casterTeam;

        if (!world.HasComponent<KindredSimComponent>(caster))
            return baseDamage;

        KindredSimComponent& state = world.GetComponent<KindredSimComponent>(caster);
        if (state.markedTarget != target || state.fEMarkRemainingSec <= 0.f)
            return baseDamage;

        state.mountingDreadHitCount =
            static_cast<u8_t>(std::min<u32_t>(3u, state.mountingDreadHitCount + 1u));
        EmitMountingDreadStackEffect(world, tc, caster, target, state.mountingDreadHitCount);

        if (state.mountingDreadHitCount < 3u)
            return baseDamage;

        state.markedTarget = NULL_ENTITY;
        state.mountingDreadHitCount = 0;
        state.fEMarkRemainingSec = 0.f;

        const f32_t pounceBonusDamage = ResolveKindredSkillEffectParam(
            world, tc, caster, eSkillSlot::E,
            eSkillEffectParamId::BaseDamage, kKindredEPounceBonusDamage);

        std::cout << "[KindredSim] E wolf pounce caster="
            << caster << " target=" << target << "\n";
        return baseDamage + pounceBonusDamage;
    }
}
