#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/RuneComponent.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Registries/ChampionStats/ChampionStatsRegistry.h"
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"

#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

f32_t CStatSystem::Clamp(f32_t value, f32_t minValue, f32_t maxValue)
{
    if (value < minValue)
        return minValue;
    if (value > maxValue)
        return maxValue;
    return value;
}

StatComponent CStatSystem::BuildBaseStats(const ChampionStatsDef& def, u8_t level)
{
    const u8_t resolvedLevel = (level > 0) ? level : 1;

    StatComponent stat{};
    stat.championId = def.championId;
    stat.level = resolvedLevel;
    stat.hpMax = CCombatFormula::ResolveStatAtLevel(def.baseHp, def.hpPerLevel, resolvedLevel);
    stat.manaMax = CCombatFormula::ResolveStatAtLevel(def.baseMana, def.manaPerLevel, resolvedLevel);

    stat.baseAd = CCombatFormula::ResolveStatAtLevel(def.baseAd, def.adPerLevel, resolvedLevel);
    stat.bonusAd = 0.f;
    stat.ad = stat.baseAd + stat.bonusAd;
    stat.ap = CCombatFormula::ResolveStatAtLevel(def.baseAp, def.apPerLevel, resolvedLevel);

    stat.baseArmor = CCombatFormula::ResolveStatAtLevel(def.baseArmor, def.armorPerLevel, resolvedLevel);
    stat.bonusArmor = 0.f;
    stat.armor = stat.baseArmor + stat.bonusArmor;

    stat.baseMr = CCombatFormula::ResolveStatAtLevel(def.baseMr, def.mrPerLevel, resolvedLevel);
    stat.bonusMr = 0.f;
    stat.mr = stat.baseMr + stat.bonusMr;

    stat.baseAttackSpeed = def.baseAttackSpeed;
    stat.attackSpeedRatio = (def.attackSpeedRatio > 0.f) ? def.attackSpeedRatio : def.baseAttackSpeed;
    stat.attackSpeedGrowth = def.attackSpeedPerLevel;
    stat.bonusAttackSpeed = 0.f;
    stat.attackSpeed = CCombatFormula::ResolveAttackSpeed(
        stat.baseAttackSpeed,
        stat.attackSpeedRatio,
        stat.attackSpeedGrowth,
        stat.bonusAttackSpeed,
        resolvedLevel);

    stat.attackRange = def.baseAttackRange;
    stat.moveSpeed = def.baseMoveSpeed;
    stat.bDirty = false;
    return stat;
}

void CStatSystem::Recompute(CWorld& world, EntityID entity, StatComponent& stat)
{
    const ChampionStatsDef def = CChampionStatsRegistry::Instance().Resolve(stat.championId);

    const u8_t level = (stat.level > 0) ? stat.level : 1;
    const u32_t oldBuffHash = stat.buffMaskHash;
    const u32_t oldItemHash = stat.itemMaskHash;
    stat = BuildBaseStats(def, level);
    stat.buffMaskHash = oldBuffHash;
    stat.itemMaskHash = oldItemHash;

    f32_t fItemMoveSpeedMul = 1.f;
    if (world.HasComponent<InventoryComponent>(entity))
    {
        const InventoryComponent& inventory = world.GetComponent<InventoryComponent>(entity);
        for (u8_t i = 0; i < inventory.count && i < InventoryComponent::kMaxSlots; ++i)
        {
            const ItemDef* pItem = CItemRegistry::Instance().Find(inventory.itemIds[i]);
            if (!pItem)
                continue;

            stat.bonusAd += pItem->stats.flatAd;
            stat.ap += pItem->stats.flatAp;
            stat.hpMax += pItem->stats.flatHealth;
            stat.manaMax += pItem->stats.flatMana;
            stat.bonusArmor += pItem->stats.flatArmor;
            stat.bonusMr += pItem->stats.flatMr;
            stat.bonusAttackSpeed += pItem->stats.bonusAttackSpeed;
            stat.critChance += pItem->stats.critChance;
            stat.abilityHaste += pItem->stats.abilityHaste;
            stat.armorPenPercent += pItem->stats.armorPenPercent;
            stat.bonusArmorPenPercent += pItem->stats.bonusArmorPenPercent;
            stat.lethality += pItem->stats.lethality;
            stat.magicPenPercent += pItem->stats.magicPenPercent;
            stat.flatMagicPen += pItem->stats.flatMagicPen;
            stat.moveSpeed += pItem->stats.flatMoveSpeed;
            stat.lifesteal += pItem->stats.lifeSteal;
            fItemMoveSpeedMul *= (1.f + pItem->stats.percentMoveSpeed);
        }
    }
    stat.moveSpeed *= fItemMoveSpeedMul;

    if (world.HasComponent<BuffComponent>(entity))
    {
        const auto& buffs = world.GetComponent<BuffComponent>(entity);
        f32_t moveSpeedMul = 1.f;
        for (u8_t i = 0; i < buffs.count && i < BuffComponent::kMaxBuffs; ++i)
        {
            const BuffInstance& buff = buffs.buffs[i];
            const f32_t stacks = static_cast<f32_t>(buff.stackCount);
            stat.bonusAd += buff.flatAdPerStack * stacks;
            stat.ap += buff.flatApPerStack * stacks;
            stat.bonusArmor += buff.flatArmorPerStack * stacks;
            stat.bonusMr += buff.flatMrPerStack * stacks;
            moveSpeedMul *= buff.moveSpeedMul;
        }
        stat.moveSpeed *= moveSpeedMul;
    }

    if (world.HasComponent<RuneRuntimeComponent>(entity))
    {
        const RuneRuntimeComponent& runes = world.GetComponent<RuneRuntimeComponent>(entity);
        stat.bonusAttackSpeed +=
            static_cast<f32_t>(runes.iLethalTempoStacks) *
            RuneTuning::kLethalTempoAttackSpeedPerStack;
    }

    stat.ad = stat.baseAd + stat.bonusAd;
    stat.armor = stat.baseArmor + stat.bonusArmor;
    stat.mr = stat.baseMr + stat.bonusMr;
    stat.attackSpeed = CCombatFormula::ResolveAttackSpeed(
        stat.baseAttackSpeed,
        stat.attackSpeedRatio,
        stat.attackSpeedGrowth,
        stat.bonusAttackSpeed,
        stat.level);

    stat.abilityHaste = std::max(0.f, stat.abilityHaste);
    stat.cdr = Clamp(stat.cdr, 0.f, 0.4f);
    stat.attackSpeed = Clamp(stat.attackSpeed, 0.2f, 3.003f);

    if (world.HasComponent<ChampionComponent>(entity))
    {
        auto& champion = world.GetComponent<ChampionComponent>(entity);
        champion.id = stat.championId;
        champion.level = stat.level;
        champion.maxHp = stat.hpMax;
        champion.maxMana = stat.manaMax;
        champion.moveSpeed = stat.moveSpeed;
        if (champion.hp > champion.maxHp)
            champion.hp = champion.maxHp;
        if (champion.mana > champion.maxMana)
            champion.mana = champion.maxMana;
    }

    if (world.HasComponent<HealthComponent>(entity))
    {
        auto& hp = world.GetComponent<HealthComponent>(entity);
        hp.fMaximum = stat.hpMax;
        if (hp.fCurrent > hp.fMaximum)
            hp.fCurrent = hp.fMaximum;
        hp.bIsDead = (hp.fCurrent <= 0.f);
    }
}

void CStatSystem::Execute(CWorld& world)
{
    const auto entities = DeterministicEntityIterator<StatComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& stat = world.GetComponent<StatComponent>(entity);
        if (stat.bDirty)
            Recompute(world, entity, stat);
    }
}
