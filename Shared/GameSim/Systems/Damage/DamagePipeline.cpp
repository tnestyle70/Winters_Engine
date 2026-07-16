#include "Shared/GameSim/Systems/Damage/DamagePipeline.h"

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/RecallComponent.h"
#include "Shared/GameSim/Components/AnnieSimComponent.h"
#include "Shared/GameSim/Components/KindredSimComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Components/ViegoSoulComponent.h"
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
#include "Shared/GameSim/Systems/GameplayStateQuery/GameplayStateQuery.h"
#include "Shared/GameSim/Systems/Shield/ShieldSystem.h"

#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace
{
    bool_t TryGetTeam(CWorld& world, EntityID entity, eTeam& outTeam)
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

    void MirrorHealth(CWorld& world, EntityID entity, f32_t current, f32_t maximum)
    {
        if (world.HasComponent<ChampionComponent>(entity))
        {
            auto& champion = world.GetComponent<ChampionComponent>(entity);
            champion.hp = current;
            champion.maxHp = maximum;
        }
        if (world.HasComponent<MinionComponent>(entity))
        {
            auto& minion = world.GetComponent<MinionComponent>(entity);
            minion.hp = current;
            minion.maxHp = maximum;
        }
        if (world.HasComponent<StructureComponent>(entity))
        {
            auto& structure = world.GetComponent<StructureComponent>(entity);
            structure.hp = current;
            structure.maxHp = maximum;
        }
        if (world.HasComponent<TurretComponent>(entity))
        {
            auto& turret = world.GetComponent<TurretComponent>(entity);
            turret.hp = current;
            turret.maxHp = maximum;
        }
        if (world.HasComponent<JungleComponent>(entity))
        {
            auto& jungle = world.GetComponent<JungleComponent>(entity);
            jungle.hp = current;
            jungle.maxHp = maximum;
        }
    }

    f32_t ResolveTargetMaxHp(CWorld& world, EntityID target)
    {
        if (target != NULL_ENTITY && world.HasComponent<HealthComponent>(target))
            return world.GetComponent<HealthComponent>(target).fMaximum;
        return 0.f;
    }

    f32_t ResolveTargetMissingHp(CWorld& world, EntityID target)
    {
        if (target != NULL_ENTITY && world.HasComponent<HealthComponent>(target))
        {
            const auto& hp = world.GetComponent<HealthComponent>(target);
            return std::max(0.f, hp.fMaximum - hp.fCurrent);
        }
        return 0.f;
    }

    f32_t ResolveActiveHealthFloor(CWorld& world, EntityID target, f32_t maxHealth)
    {
        if (target == NULL_ENTITY ||
            !world.HasComponent<KindredHealthFloorComponent>(target))
        {
            return 0.f;
        }

        const KindredHealthFloorComponent& floor =
            world.GetComponent<KindredHealthFloorComponent>(target);
        if (floor.fRemainingSec <= 0.f || floor.fMinHealth <= 0.f)
            return 0.f;
        if (floor.sourceEntity != NULL_ENTITY && !world.IsAlive(floor.sourceEntity))
            return 0.f;

        return std::clamp(floor.fMinHealth, 0.f, std::max(0.f, maxHealth));
    }

    u32_t ResolveDamageFlags(const DamageRequest& req)
    {
        return req.flags;
    }

    eDamageType ResolveDamageType(const DamageRequest& req)
    {
        return req.type;
    }

    f32_t ApplyCritIfNeeded(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& req,
        u32_t flags,
        f32_t amount,
        bool_t& outCrit)
    {
        outCrit = false;
        if ((flags & DamageFlag_CanCrit) == 0u)
            return amount;
        if (req.source == NULL_ENTITY || !world.HasComponent<StatComponent>(req.source))
            return amount;

        const auto& sourceStat = world.GetComponent<StatComponent>(req.source);
        const f32_t chance = std::clamp(sourceStat.critChance, 0.f, 1.f);
        if (chance <= 0.f)
            return amount;

        bool_t bCrit = false;
        if (tc.pRng)
            bCrit = tc.pRng->RollChance(chance);

        if (!bCrit)
            return amount;

        outCrit = true;
        return amount * std::max(1.f, sourceStat.critDamage);
    }

    f32_t ApplyTypedResistance(
        CWorld& world,
        const DamageRequest& req,
        eDamageType damageType,
        f32_t amount)
    {
        if (damageType == eDamageType::True)
            return amount;
        if (req.target == NULL_ENTITY || !world.HasComponent<StatComponent>(req.target))
            return amount;

        const auto& targetStat = world.GetComponent<StatComponent>(req.target);
        const StatComponent* pSourceStat = nullptr;
        if (req.source != NULL_ENTITY && world.HasComponent<StatComponent>(req.source))
            pSourceStat = &world.GetComponent<StatComponent>(req.source);

        ResistBreakdown pen{};
        if (pSourceStat && damageType == eDamageType::Physical)
        {
            pen.percentPen = pSourceStat->armorPenPercent;
            pen.percentBonusPen = pSourceStat->bonusArmorPenPercent;
            pen.flatPen = pSourceStat->lethality + pSourceStat->armorPen;

            const f32_t resistance = CCombatFormula::EffectiveResistance(
                targetStat.baseArmor,
                targetStat.bonusArmor,
                pen);
            return CCombatFormula::ApplyResistance(amount, resistance);
        }

        if (pSourceStat && damageType == eDamageType::Magic)
        {
            pen.percentPen = pSourceStat->magicPenPercent;
            pen.flatPen = pSourceStat->flatMagicPen + pSourceStat->mrPen;

            const f32_t resistance = CCombatFormula::EffectiveResistance(
                targetStat.baseMr,
                targetStat.bonusMr,
                pen);
            return CCombatFormula::ApplyResistance(amount, resistance);
        }

        const f32_t fallbackResist = (damageType == eDamageType::Physical)
            ? targetStat.armor
            : targetStat.mr;
        return CCombatFormula::ApplyResistance(amount, fallbackResist);
    }

    f32_t ApplyAnnieEShield(CWorld& world, EntityID target, f32_t amount, bool_t& outShielded)
    {
        if (amount <= 0.f ||
            target == NULL_ENTITY ||
            !world.HasComponent<AnnieSimComponent>(target))
        {
            return amount;
        }

        AnnieSimComponent& state = world.GetComponent<AnnieSimComponent>(target);
        if (state.fEShieldRemainingSec <= 0.f || state.fEShieldAmount <= 0.f)
            return amount;

        const f32_t absorbed = std::min(amount, state.fEShieldAmount);
        state.fEShieldAmount -= absorbed;
        if (state.fEShieldAmount <= 0.f)
        {
            state.fEShieldAmount = 0.f;
            state.fEShieldRemainingSec = 0.f;
        }

        outShielded = true;
        return amount - absorbed;
    }
    void TryActivateYasuoPassiveShield(
        CWorld& world,
        const TickContext& tc,
        EntityID target,
        f32_t incomingDamage)
    {
        if (incomingDamage <= 0.f ||
            target == NULL_ENTITY ||
            !world.HasComponent<ChampionComponent>(target) ||
            !world.HasComponent<YasuoStateComponent>(target))
        {
            return;
        }

        ChampionComponent& champion = world.GetComponent<ChampionComponent>(target);
        if (champion.id != eChampion::YASUO)
            return;

        YasuoStateComponent& state = world.GetComponent<YasuoStateComponent>(target);
        if (state.fPassiveShieldRemaining > 0.f ||
            state.fPassiveFlowMax <= 0.f ||
            state.fPassiveFlow < state.fPassiveFlowMax)
        {
            return;
        }

        constexpr f32_t kPassiveShieldDurationSec = 3.f;
        if (CShieldSystem::Grant(
                world,
                tc,
                target,
                state.fPassiveFlowMax,
                kPassiveShieldDurationSec))
        {
            state.fPassiveFlow = 0.f;
        }
    }
}

f32_t BuildRawDamage(CWorld& world, const DamageRequest& req)
{
    f32_t flat = (req.flatAmount != 0.f) ? req.flatAmount : req.amount;
    f32_t totalAdRatio = req.adRatioOverride;
    f32_t bonusAdRatio = req.bonusAdRatioOverride;
    f32_t apRatio = req.apRatioOverride;
    f32_t targetMaxHpRatio = req.targetMaxHpRatioOverride;
    f32_t targetMissingHpRatio = req.targetMissingHpRatioOverride;

    if (req.source != NULL_ENTITY && world.HasComponent<StatComponent>(req.source))
    {
        const auto& sourceStat = world.GetComponent<StatComponent>(req.source);
        flat += sourceStat.ad * totalAdRatio;
        flat += sourceStat.bonusAd * bonusAdRatio;
        flat += sourceStat.ap * apRatio;
    }

    flat += ResolveTargetMaxHp(world, req.target) * targetMaxHpRatio;
    flat += ResolveTargetMissingHp(world, req.target) * targetMissingHpRatio;
    return flat;
}

f32_t ApplyResistance(f32_t amount, f32_t resistance)
{
    return CCombatFormula::ApplyResistance(amount, resistance);
}

DamageResult ApplyDamageRequest(CWorld& world, const TickContext& tc, const DamageRequest& req)
{
    DamageResult result{};
    if (req.target == NULL_ENTITY || req.target == req.source)
        return result;
    if (!world.IsAlive(req.target) || !world.HasComponent<HealthComponent>(req.target))
        return result;
    if (world.HasComponent<ViegoSoulComponent>(req.target))
        return result;

    const auto& targetHealth = world.GetComponent<HealthComponent>(req.target);
    if (targetHealth.bIsDead || targetHealth.fCurrent <= 0.f)
        return result;
    if (!GameplayStateQuery::CanReceiveDamage(world, req.source, req.target))
        return result;

    eTeam targetTeam = eTeam::Neutral;
    if (TryGetTeam(world, req.target, targetTeam) &&
        targetTeam == req.sourceTeam &&
        targetTeam != eTeam::Neutral)
    {
        return result;
    }

    const u32_t flags = ResolveDamageFlags(req);
    const eDamageType damageType = ResolveDamageType(req);

    f32_t amount = BuildRawDamage(world, req);
    amount = ApplyCritIfNeeded(world, tc, req, flags, amount, result.bWasCrit);
    amount = ApplyTypedResistance(world, req, damageType, amount);
    TryActivateYasuoPassiveShield(world, tc, req.target, amount);
    amount = CShieldSystem::Absorb(
        world, tc, req.target, amount, result.bWasShielded);
    amount = ApplyAnnieEShield(world, req.target, amount, result.bWasShielded);
    amount = std::max(0.f, amount);

    auto& hp = world.GetComponent<HealthComponent>(req.target);
    const f32_t previousHealth = hp.fCurrent;
    f32_t nextHealth = (hp.fCurrent > amount) ? (hp.fCurrent - amount) : 0.f;
    const f32_t healthFloor = ResolveActiveHealthFloor(world, req.target, hp.fMaximum);
    if (healthFloor > 0.f)
        nextHealth = std::max(nextHealth, healthFloor);

    hp.fCurrent = std::min(hp.fMaximum, nextHealth);
    result.finalAmount = std::max(0.f, previousHealth - hp.fCurrent);
    result.bKilled = (hp.fCurrent <= 0.f);
    hp.bIsDead = result.bKilled;

    MirrorHealth(world, req.target, hp.fCurrent, hp.fMaximum);

    // 챔피언 피해는 진행 중인 리콜을 끊는다 (LoL 파리티).
    if (result.finalAmount > 0.f && world.HasComponent<RecallComponent>(req.target))
        world.RemoveComponent<RecallComponent>(req.target);

    // 흡혈: 실제 적용 데미지 기준, 시전자 생존 시에만 회복.
    if ((flags & DamageFlag_CanLifesteal) != 0u &&
        result.finalAmount > 0.f &&
        req.source != NULL_ENTITY &&
        world.IsAlive(req.source) &&
        world.HasComponent<StatComponent>(req.source) &&
        world.HasComponent<HealthComponent>(req.source))
    {
        const auto& sourceStat = world.GetComponent<StatComponent>(req.source);
        auto& sourceHp = world.GetComponent<HealthComponent>(req.source);
        if (sourceStat.lifesteal > 0.f && !sourceHp.bIsDead)
        {
            sourceHp.fCurrent = std::min(
                sourceHp.fMaximum,
                sourceHp.fCurrent + result.finalAmount * sourceStat.lifesteal);
            MirrorHealth(world, req.source, sourceHp.fCurrent, sourceHp.fMaximum);
        }
    }

    return result;
}

void EnqueueDamageRequest(CWorld& world, const DamageRequest& req)
{
    EntityID entity = world.CreateEntity();
    world.AddComponent<DamageRequestComponent>(entity, req);
}
