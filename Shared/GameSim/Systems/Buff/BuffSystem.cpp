#include "Shared/GameSim/Systems/Buff/BuffSystem.h"

#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/Ecs/TransformComponent.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/ManaComponent.h"
#include "Shared/GameSim/Components/ReplicatedEventComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Definitions/GameplayDefinitionPack.h"
#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/ReplicatedEventQueue/ReplicatedEventQueue.h"

#include <algorithm>
#include <vector>

namespace
{
    constexpr u8_t ToObjectiveIndex(eObjectiveBuffKind kind)
    {
        return static_cast<u8_t>(kind);
    }

    u64_t DurationTicks(f32_t seconds)
    {
        return (std::max)(1ull, DeterministicTime::SecToTick((std::max)(0.f, seconds)));
    }

    bool_t IsDead(CWorld& world, EntityID entity)
    {
        if (entity == NULL_ENTITY || !world.IsAlive(entity))
            return true;
        const HealthComponent* health = world.TryGetComponent<HealthComponent>(entity);
        return health && (health->bIsDead || health->fCurrent <= 0.f);
    }

    eTeam ResolveTeam(CWorld& world, EntityID entity)
    {
        if (const ChampionComponent* champion = world.TryGetComponent<ChampionComponent>(entity))
            return champion->team;
        if (const MinionComponent* minion = world.TryGetComponent<MinionComponent>(entity))
            return minion->team;
        return eTeam::Neutral;
    }

    void MirrorHealth(CWorld& world, EntityID entity, const HealthComponent& health)
    {
        if (ChampionComponent* champion = world.TryGetComponent<ChampionComponent>(entity))
        {
            champion->hp = health.fCurrent;
            champion->maxHp = health.fMaximum;
        }
        if (MinionComponent* minion = world.TryGetComponent<MinionComponent>(entity))
        {
            minion->hp = health.fCurrent;
            minion->maxHp = health.fMaximum;
        }
    }

    void UnapplyBaronMinion(CWorld& world, EntityID entity)
    {
        BaronEmpoweredMinionComponent* empowered =
            world.TryGetComponent<BaronEmpoweredMinionComponent>(entity);
        if (!empowered)
            return;

        if (HealthComponent* health = world.TryGetComponent<HealthComponent>(entity))
        {
            const f32_t ratio = health->fMaximum > 0.f
                ? std::clamp(health->fCurrent / health->fMaximum, 0.f, 1.f)
                : 0.f;
            health->fMaximum = (std::max)(0.f, empowered->baseMaxHp);
            health->fCurrent = health->fMaximum * ratio;
            MirrorHealth(world, entity, *health);
        }
        if (MinionStateComponent* state = world.TryGetComponent<MinionStateComponent>(entity))
            state->attackDamage = empowered->baseAttackDamage;
        world.RemoveComponent<BaronEmpoweredMinionComponent>(entity);
    }

    void ApplyBaronMinion(
        CWorld& world,
        EntityID entity,
        const ObjectiveGameplayDef& tuning)
    {
        if (!world.HasComponent<HealthComponent>(entity) ||
            !world.HasComponent<MinionStateComponent>(entity))
        {
            return;
        }

        if (world.HasComponent<BaronEmpoweredMinionComponent>(entity))
            UnapplyBaronMinion(world, entity);

        auto& health = world.GetComponent<HealthComponent>(entity);
        auto& state = world.GetComponent<MinionStateComponent>(entity);
        BaronEmpoweredMinionComponent empowered{};
        empowered.baseMaxHp = health.fMaximum;
        empowered.baseAttackDamage = state.attackDamage;
        empowered.hpMultiplier = tuning.baronMinionHpMultiplier;
        empowered.attackDamageMultiplier = tuning.baronMinionAttackDamageMultiplier;
        empowered.scaleMultiplier = tuning.baronMinionScaleMultiplier;

        const f32_t ratio = health.fMaximum > 0.f
            ? std::clamp(health.fCurrent / health.fMaximum, 0.f, 1.f)
            : 0.f;
        health.fMaximum = empowered.baseMaxHp * empowered.hpMultiplier;
        health.fCurrent = health.fMaximum * ratio;
        state.attackDamage = empowered.baseAttackDamage * empowered.attackDamageMultiplier;
        world.AddComponent<BaronEmpoweredMinionComponent>(entity, empowered);
        MirrorHealth(world, entity, health);
    }

    void RefreshBaronAura(CWorld& world, const ObjectiveGameplayDef& tuning)
    {
        struct Source { eTeam team; Vec3 position; };
        std::vector<Source> sources;
        const auto champions =
            DeterministicEntityIterator<ChampionComponent>::CollectSorted(world);
        for (EntityID entity : champions)
        {
            if (!world.HasComponent<ObjectiveBuffComponent>(entity) ||
                !world.HasComponent<TransformComponent>(entity) ||
                IsDead(world, entity) ||
                !CBuffSystem::HasObjectiveBuff(world, entity, eObjectiveBuffKind::Baron))
            {
                continue;
            }
            sources.push_back({
                world.GetComponent<ChampionComponent>(entity).team,
                world.GetComponent<TransformComponent>(entity).GetPosition(),
            });
        }

        const f32_t radiusSq = tuning.baronAuraRadius * tuning.baronAuraRadius;
        const auto minions =
            DeterministicEntityIterator<MinionComponent>::CollectSorted(world);
        for (EntityID entity : minions)
        {
            if (!world.HasComponent<MinionStateComponent>(entity) ||
                !world.HasComponent<TransformComponent>(entity))
            {
                continue;
            }
            const auto& minion = world.GetComponent<MinionComponent>(entity);
            bool_t bShouldEmpower = minion.roleType <= 3u &&
                !world.HasComponent<AnnieTibbersComponent>(entity) &&
                !IsDead(world, entity);
            if (bShouldEmpower)
            {
                bShouldEmpower = false;
                const Vec3 position = world.GetComponent<TransformComponent>(entity).GetPosition();
                for (const Source& source : sources)
                {
                    if (source.team == minion.team &&
                        WintersMath::DistanceSqXZ(position, source.position) <= radiusSq)
                    {
                        bShouldEmpower = true;
                        break;
                    }
                }
            }

            const bool_t bEmpowered = world.HasComponent<BaronEmpoweredMinionComponent>(entity);
            if (bShouldEmpower && !bEmpowered)
                ApplyBaronMinion(world, entity, tuning);
            else if (!bShouldEmpower && bEmpowered)
                UnapplyBaronMinion(world, entity);
        }
    }

    void TickBurn(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        ObjectiveBurnState& burn,
        u32_t flag,
        f32_t intervalSec,
        f32_t amount)
    {
        if (burn.source == NULL_ENTITY || burn.expireTick <= tc.tickIndex || IsDead(world, target))
        {
            burn = {};
            return;
        }
        if (burn.nextTick > tc.tickIndex)
            return;

        DamageRequest request{};
        request.source = burn.source;
        request.target = target;
        request.sourceTeam = ResolveTeam(world, burn.source);
        request.type = eDamageType::True;
        request.flatAmount = (std::max)(0.f, amount);
        request.eSourceKind = eDamageSourceKind::Rune;
        request.flags = flag;
        EnqueueDamageRequest(world, request);
        burn.nextTick += DurationTicks(intervalSec);
    }
}

bool CBuffSystem::AddOrRefresh(BuffComponent& component, const BuffInstance& instance)
{
    for (u8_t i = 0; i < component.count && i < BuffComponent::kMaxBuffs; ++i)
    {
        BuffInstance& existing = component.buffs[i];
        if (existing.buffDefId == instance.buffDefId && existing.source == instance.source)
        {
            existing = instance;
            return true;
        }
    }

    if (component.count >= BuffComponent::kMaxBuffs)
        return false;

    component.buffs[component.count++] = instance;
    return true;
}

bool_t CBuffSystem::PruneExpiredTickBuffs(
    CWorld& world,
    const TickContext& tc)
{
    bool_t bAnyChanged = false;
    const auto entities =
        DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& component = world.GetComponent<BuffComponent>(entity);
        bool_t bChanged = false;
        u8_t uWrite = 0u;
        for (u8_t uRead = 0u;
            uRead < component.count && uRead < BuffComponent::kMaxBuffs;
            ++uRead)
        {
            const BuffInstance buff = component.buffs[uRead];
            if (buff.uExpireTick != 0u &&
                tc.tickIndex >= buff.uExpireTick)
            {
                bChanged = true;
                continue;
            }
            component.buffs[uWrite++] = buff;
        }

        if (uWrite != component.count)
        {
            for (u8_t i = uWrite;
                i < component.count && i < BuffComponent::kMaxBuffs;
                ++i)
            {
                component.buffs[i] = {};
            }
            component.count = uWrite;
        }

        if (bChanged && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
        bAnyChanged = bAnyChanged || bChanged;
    }
    return bAnyChanged;
}

void CBuffSystem::AdvanceDurationsAfterStat(
    CWorld& world,
    const TickContext& tc)
{
    const auto entities =
        DeterministicEntityIterator<BuffComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& component = world.GetComponent<BuffComponent>(entity);
        bool_t bChanged = false;
        u8_t uWrite = 0u;
        for (u8_t uRead = 0u;
            uRead < component.count && uRead < BuffComponent::kMaxBuffs;
            ++uRead)
        {
            BuffInstance buff = component.buffs[uRead];
            bool_t bKeep = true;
            if (buff.uExpireTick != 0u)
            {
                const u64_t uRemainingTicks = buff.uExpireTick > tc.tickIndex
                    ? buff.uExpireTick - tc.tickIndex
                    : 0u;
                buff.fDurationRemaining =
                    static_cast<f32_t>(uRemainingTicks) *
                    DeterministicTime::kFixedDt;
                bKeep = uRemainingTicks != 0u;
            }
            else
            {
                if (buff.fDurationRemaining > 0.f)
                    buff.fDurationRemaining -= tc.fDt;
                bKeep = buff.fDurationRemaining > 0.f;
            }

            if (bKeep)
                component.buffs[uWrite++] = buff;
            else
                bChanged = true;
        }

        if (uWrite != component.count)
        {
            for (u8_t i = uWrite;
                i < component.count && i < BuffComponent::kMaxBuffs;
                ++i)
            {
                component.buffs[i] = {};
            }
            component.count = uWrite;
        }

        if (bChanged && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
    }
}

const ObjectiveGameplayDef& CBuffSystem::ResolveObjectiveTuning(const TickContext& tc)
{
    static const ObjectiveGameplayDef kDefaults{};
    if (!tc.pDefinitions)
        return kDefaults;
    const EconomyGameplayDef* economy = tc.pDefinitions->FindEconomy();
    return economy ? economy->objectives : kDefaults;
}

bool_t CBuffSystem::HasObjectiveBuff(
    CWorld& world,
    EntityID entity,
    eObjectiveBuffKind kind)
{
    const ObjectiveBuffComponent* buffs = world.TryGetComponent<ObjectiveBuffComponent>(entity);
    const u8_t index = ToObjectiveIndex(kind);
    return buffs && index < ToObjectiveIndex(eObjectiveBuffKind::Count) &&
        buffs->expireTicks[index] != 0u;
}

void CBuffSystem::AddOrRefreshObjectiveBuff(
    CWorld& world,
    EntityID entity,
    eObjectiveBuffKind kind,
    const TickContext& tc)
{
    if (entity == NULL_ENTITY || !world.IsAlive(entity) || IsDead(world, entity))
        return;
    ObjectiveBuffComponent* buffs = world.TryGetComponent<ObjectiveBuffComponent>(entity);
    if (!buffs)
    {
        world.AddComponent<ObjectiveBuffComponent>(entity, {});
        buffs = &world.GetComponent<ObjectiveBuffComponent>(entity);
    }
    buffs->expireTicks[ToObjectiveIndex(kind)] =
        tc.tickIndex + DurationTicks(ResolveObjectiveTuning(tc).buffDurationSec);
    if (kind == eObjectiveBuffKind::Elder && world.HasComponent<StatComponent>(entity))
        world.GetComponent<StatComponent>(entity).bDirty = true;
}

void CBuffSystem::TickObjectiveEffects(CWorld& world, const TickContext& tc)
{
    const ObjectiveGameplayDef& tuning = ResolveObjectiveTuning(tc);
    const auto buffed = DeterministicEntityIterator<ObjectiveBuffComponent>::CollectSorted(world);
    for (EntityID entity : buffed)
    {
        auto& buffs = world.GetComponent<ObjectiveBuffComponent>(entity);
        const bool_t bHadElder = buffs.expireTicks[ToObjectiveIndex(eObjectiveBuffKind::Elder)] != 0u;
        if (IsDead(world, entity))
        {
            buffs = {};
        }
        else
        {
            for (u8_t index = 0u; index < ToObjectiveIndex(eObjectiveBuffKind::Count); ++index)
            {
                if (buffs.expireTicks[index] != 0u && buffs.expireTicks[index] <= tc.tickIndex)
                    buffs.expireTicks[index] = 0u;
            }

            if (buffs.expireTicks[ToObjectiveIndex(eObjectiveBuffKind::Blue)] != 0u)
            {
                if (ManaComponent* mana = world.TryGetComponent<ManaComponent>(entity))
                {
                    mana->fCurrent = (std::min)(mana->fMaximum,
                        mana->fCurrent + tuning.blueManaRegenPerSec * tc.fDt);
                    if (ChampionComponent* champion = world.TryGetComponent<ChampionComponent>(entity))
                        champion->mana = mana->fCurrent;
                }
            }
            if (buffs.expireTicks[ToObjectiveIndex(eObjectiveBuffKind::Red)] != 0u)
            {
                if (HealthComponent* health = world.TryGetComponent<HealthComponent>(entity))
                {
                    health->fCurrent = (std::min)(health->fMaximum,
                        health->fCurrent + tuning.redHealthRegenPerSec * tc.fDt);
                    MirrorHealth(world, entity, *health);
                }
            }
        }
        const bool_t bHasElder = buffs.expireTicks[ToObjectiveIndex(eObjectiveBuffKind::Elder)] != 0u;
        if (bHadElder != bHasElder && world.HasComponent<StatComponent>(entity))
            world.GetComponent<StatComponent>(entity).bDirty = true;
    }

    const auto burning = DeterministicEntityIterator<ObjectiveBurnComponent>::CollectSorted(world);
    for (EntityID entity : burning)
    {
        auto& burns = world.GetComponent<ObjectiveBurnComponent>(entity);
        f32_t elderAmount = 0.f;
        if (const HealthComponent* health = world.TryGetComponent<HealthComponent>(entity))
            elderAmount = health->fMaximum * tuning.elderBurnTargetMaxHpRatioPerTick;
        TickBurn(world, tc, entity, burns.elder, DamageFlag_ElderBurn,
            tuning.elderBurnTickIntervalSec, elderAmount);
        TickBurn(world, tc, entity, burns.red, DamageFlag_RedBurn,
            tuning.redBurnTickIntervalSec, tuning.redBurnDamagePerTick);
    }

    RefreshBaronAura(world, tuning);
}

void CBuffSystem::CleanupDeadObjectiveState(CWorld& world)
{
    const auto buffed = DeterministicEntityIterator<ObjectiveBuffComponent>::CollectSorted(world);
    for (EntityID entity : buffed)
    {
        if (!IsDead(world, entity))
            continue;
        if (HasObjectiveBuff(world, entity, eObjectiveBuffKind::Elder) &&
            world.HasComponent<StatComponent>(entity))
        {
            world.GetComponent<StatComponent>(entity).bDirty = true;
        }
        world.GetComponent<ObjectiveBuffComponent>(entity) = {};
    }
    const auto burning = DeterministicEntityIterator<ObjectiveBurnComponent>::CollectSorted(world);
    for (EntityID entity : burning)
    {
        if (IsDead(world, entity))
            world.GetComponent<ObjectiveBurnComponent>(entity) = {};
    }
}

void CBuffSystem::OnDamageResolved(
    CWorld& world,
    const TickContext& tc,
    const DamageRequest& request,
    DamageResult& result)
{
    if (!result.bApplied || result.finalAmount <= 0.f || request.source == NULL_ENTITY ||
        request.target == NULL_ENTITY || !world.IsAlive(request.source) ||
        !world.IsAlive(request.target))
    {
        return;
    }

    const ObjectiveGameplayDef& tuning = ResolveObjectiveTuning(tc);
    const bool_t bPeriodic = (request.flags & (DamageFlag_ElderBurn | DamageFlag_RedBurn)) != 0u;
    ObjectiveBurnComponent* burns = world.TryGetComponent<ObjectiveBurnComponent>(request.target);
    if (!burns && !bPeriodic &&
        (HasObjectiveBuff(world, request.source, eObjectiveBuffKind::Elder) ||
            (request.eSourceKind == eDamageSourceKind::BasicAttack &&
                HasObjectiveBuff(world, request.source, eObjectiveBuffKind::Red))))
    {
        world.AddComponent<ObjectiveBurnComponent>(request.target, {});
        burns = &world.GetComponent<ObjectiveBurnComponent>(request.target);
    }

    if (!bPeriodic && burns && HasObjectiveBuff(world, request.source, eObjectiveBuffKind::Elder))
    {
        burns->elder.source = request.source;
        burns->elder.expireTick = tc.tickIndex + DurationTicks(tuning.elderBurnDurationSec);
        burns->elder.nextTick = tc.tickIndex + DurationTicks(tuning.elderBurnTickIntervalSec);
    }
    if (!bPeriodic && burns && request.eSourceKind == eDamageSourceKind::BasicAttack &&
        HasObjectiveBuff(world, request.source, eObjectiveBuffKind::Red))
    {
        burns->red.source = request.source;
        burns->red.expireTick = tc.tickIndex + DurationTicks(tuning.redBurnDurationSec);
        burns->red.nextTick = tc.tickIndex + DurationTicks(tuning.redBurnTickIntervalSec);
    }

    if ((request.flags & DamageFlag_ElderExecute) == 0u &&
        HasObjectiveBuff(world, request.source, eObjectiveBuffKind::Elder) &&
        world.HasComponent<ChampionComponent>(request.target))
    {
        const HealthComponent* health = world.TryGetComponent<HealthComponent>(request.target);
        if (health && health->fMaximum > 0.f && health->fCurrent > 0.f &&
            health->fCurrent / health->fMaximum <= tuning.elderExecuteThresholdRatio &&
            PromoteDamageResultToExecution(world, request.target, result))
        {
            ReplicatedEventComponent event{};
            event.kind = eReplicatedEventKind::EffectTrigger;
            event.effectId = kObjectiveEffectElderExecute;
            event.sourceEntity = request.source;
            event.targetEntity = request.target;
            event.sourceTeam = static_cast<u8_t>(ResolveTeam(world, request.source));
            event.startTick = tc.tickIndex;
            EnqueueReplicatedEvent(world, event);
        }
    }
}

void CBuffSystem::ClearObjectiveState(CWorld& world)
{
    const auto buffed = DeterministicEntityIterator<ObjectiveBuffComponent>::CollectSorted(world);
    for (EntityID entity : buffed)
    {
        if (HasObjectiveBuff(world, entity, eObjectiveBuffKind::Elder) &&
            world.HasComponent<StatComponent>(entity))
        {
            world.GetComponent<StatComponent>(entity).bDirty = true;
        }
        world.GetComponent<ObjectiveBuffComponent>(entity) = {};
    }
    const auto burning = DeterministicEntityIterator<ObjectiveBurnComponent>::CollectSorted(world);
    for (EntityID entity : burning)
        world.GetComponent<ObjectiveBurnComponent>(entity) = {};
    UnapplyAllBaronEmpoweredMinions(world);
}

void CBuffSystem::UnapplyAllBaronEmpoweredMinions(CWorld& world)
{
    const auto minions =
        DeterministicEntityIterator<BaronEmpoweredMinionComponent>::CollectSorted(world);
    for (EntityID entity : minions)
        UnapplyBaronMinion(world, entity);
}
