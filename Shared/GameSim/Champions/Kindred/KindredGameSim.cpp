#include "Shared/GameSim/Champions/Kindred/KindredGameSim.h"

#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/GameplayHookRegistry/GameplayHookRegistry.h"
#include "Shared/GameSim/Systems/StatusEffect/StatusEffectSystem.h"

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

    KindredSimComponent& EnsureKindredState(CWorld& world, EntityID caster)
    {
        if (!world.HasComponent<KindredSimComponent>(caster))
            world.AddComponent<KindredSimComponent>(caster, KindredSimComponent{});

        return world.GetComponent<KindredSimComponent>(caster);
    }

    f32_t ResolveKindredRHealAmount(u8_t rank)
    {
        const u8_t clampedRank = (rank == 0u)
            ? 1u
            : static_cast<u8_t>(std::min<u32_t>(rank, 3u));
        return kKindredRHealByRank[clampedRank - 1u];
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

    void RefreshKindredRHealthFloor(CWorld& world, EntityID caster, const KindredSimComponent& state)
    {
        const f32_t radiusSq = kKindredRRadius * kKindredRRadius;

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
                    floor.fMinHealth = kKindredRMinHealth;

                    auto& health = world.GetComponent<HealthComponent>(entity);
                    if (health.fCurrent < floor.fMinHealth)
                    {
                        health.fCurrent = std::min(health.fMaximum, floor.fMinHealth);
                        health.bIsDead = false;
                        MirrorChampionHealth(world, entity, health);
                    }
                }));
    }

    void HealKindredRTargets(CWorld& world, const KindredSimComponent& state)
    {
        if (state.fRHealAmount <= 0.f)
            return;

        const f32_t radiusSq = kKindredRRadius * kKindredRRadius;
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
            RefreshKindredRHealthFloor(world, caster, state);
            return;
        }

        HealKindredRTargets(world, state);
        ClearKindredRHealthFloor(world, caster);
        state.fRHealAmount = 0.f;

        std::cout << "[KindredSim] R lamb respite ended caster="
            << caster << "\n";
    }

    constexpr u16_t MakeStatusStackGroup(eChampion champion, u8_t slot)
    {
        return static_cast<u16_t>(
            (static_cast<u32_t>(champion) << 8) | static_cast<u32_t>(slot));
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

    EntityID FindWolfTarget(CWorld& world, EntityID caster, eTeam casterTeam, const Vec3& center)
    {
        EntityID bestTarget = NULL_ENTITY;
        f32_t bestDistanceSq = kKindredWRadius * kKindredWRadius;

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

    void ApplyMountingDreadSlow(CWorld& world, EntityID caster, EntityID target)
    {
        StatusEffectApplyDesc slow{};
        slow.effectId = eStatusEffectId::GenericSlow;
        slow.stackPolicy = eStatusStackPolicy::RefreshDuration;
        slow.sourceEntity = caster;
        slow.stackGroup = MakeStatusStackGroup(
            eChampion::KINDRED,
            static_cast<u8_t>(eSkillSlot::E));
        slow.stateFlags = kGameplayStateSlowedFlag;
        slow.fDurationSec = kKindredESlowDurationSec;
        slow.fMoveSpeedMul = kKindredESlowMoveSpeedMul;
        GameplayStatus::ApplyStatusEffect(world, target, slow);
    }

    void OnW(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        Vec3 center = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        if (ctx.pCommand)
            center = ctx.pCommand->groundPos;

        KindredSimComponent& state = EnsureKindredState(*ctx.pWorld, ctx.casterEntity);
        state.fWRemainingSec = kKindredWDurationSec;
        state.fWTickAccumulatorSec = kKindredWTickSec;
        state.vWCenter = center;

        std::cout << "[KindredSim] W wolf frenzy caster="
            << ctx.casterEntity << "\n";
    }

    void OnE(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pCommand || ctx.pCommand->targetEntity == NULL_ENTITY)
            return;

        KindredSimComponent& state = EnsureKindredState(*ctx.pWorld, ctx.casterEntity);
        state.markedTarget = ctx.pCommand->targetEntity;
        state.mountingDreadHitCount = 0;
        state.fEMarkRemainingSec = kKindredEMarkDurationSec;
        ApplyMountingDreadSlow(*ctx.pWorld, ctx.casterEntity, ctx.pCommand->targetEntity);

        std::cout << "[KindredSim] E mounting dread mark caster="
            << ctx.casterEntity << " target=" << ctx.pCommand->targetEntity << "\n";
    }

    void OnR(GameplayHookContext& ctx)
    {
        if (!ctx.pWorld || !ctx.pWorld->HasComponent<TransformComponent>(ctx.casterEntity))
            return;

        Vec3 center = ctx.pWorld->GetComponent<TransformComponent>(ctx.casterEntity).GetPosition();
        if (ctx.pCommand)
            center = ctx.pCommand->groundPos;

        KindredSimComponent& state = EnsureKindredState(*ctx.pWorld, ctx.casterEntity);
        state.fRRemainingSec = kKindredRDurationSec;
        state.fRHealAmount = ResolveKindredRHealAmount(ctx.skillRank);
        state.vRCenter = center;
        RefreshKindredRHealthFloor(*ctx.pWorld, ctx.casterEntity, state);

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

                    state.fWTickAccumulatorSec += kKindredWTickSec;
                    const EntityID target =
                        FindWolfTarget(world, entity, champion.team, state.vWCenter);
                    if (target != NULL_ENTITY)
                    {
                        EnqueuePhysicalDamage(
                            world,
                            entity,
                            target,
                            champion.team,
                            kKindredWDamage,
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
        if (state.mountingDreadHitCount < 3u)
            return baseDamage;

        state.markedTarget = NULL_ENTITY;
        state.mountingDreadHitCount = 0;
        state.fEMarkRemainingSec = 0.f;

        std::cout << "[KindredSim] E wolf pounce caster="
            << caster << " target=" << target << "\n";
        return baseDamage + kKindredEPounceBonusDamage;
    }
}
