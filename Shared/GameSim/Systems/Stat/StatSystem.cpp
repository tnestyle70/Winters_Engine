#include "Shared/GameSim/Systems/Stat/StatSystem.h"

#include "Shared/GameSim/Core/Debug/SimDebugOutput.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Components/BuffComponent.h"
#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/ChampionDefinitionComponent.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/ItemRuntimeComponent.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"
#include "Shared/GameSim/Definitions/ChampionRuntimeDefaults.h"
#include "Shared/GameSim/Systems/Combat/CombatFormula.h"
#include "Shared/GameSim/Systems/DeterministicEntityIterator/DeterministicEntityIterator.h"
#include "Shared/GameSim/Systems/Buff/BuffSystem.h"

#include <algorithm>
#include <cmath>

#ifdef min
#undef min
#endif

namespace
{
    // Item definitions keep LoL-facing movement points (25, 45, ...), while
    // authoritative simulation positions use the repository-wide 0.01 world scale.
    constexpr f32_t kLoLDistanceToWorldScale = 0.01f;

    // Practice 오버라이드는 정의 팩/레지스트리 원본을 바꾸지 않고
    // 이 엔티티의 스탯 재계산 입력에만 오버레이된다 (서버 스냅샷으로 회귀).
    template <typename TStats>
    void ApplyPracticeChampionStatOverrides(
        const CWorld& world,
        EntityID entity,
        TStats& def)
    {
        const PracticeChampionStatOverrideComponent* pOverrides =
            world.TryGetComponent<PracticeChampionStatOverrideComponent>(entity);
        if (!pOverrides || pOverrides->count == 0u)
            return;

        const u8_t count = (std::min)(
            pOverrides->count, PracticeChampionStatOverrideComponent::kMaxEntries);
        for (u8_t i = 0u; i < count; ++i)
        {
            const PracticeChampionStatOverrideEntry& entry = pOverrides->entries[i];
            switch (static_cast<eChampionStatOverrideId>(entry.statId))
            {
            case eChampionStatOverrideId::BaseHp: def.baseHp = entry.value; break;
            case eChampionStatOverrideId::HpPerLevel: def.hpPerLevel = entry.value; break;
            case eChampionStatOverrideId::BaseMana: def.baseMana = entry.value; break;
            case eChampionStatOverrideId::ManaPerLevel: def.manaPerLevel = entry.value; break;
            case eChampionStatOverrideId::BaseAd: def.baseAd = entry.value; break;
            case eChampionStatOverrideId::AdPerLevel: def.adPerLevel = entry.value; break;
            case eChampionStatOverrideId::BaseAp: def.baseAp = entry.value; break;
            case eChampionStatOverrideId::ApPerLevel: def.apPerLevel = entry.value; break;
            case eChampionStatOverrideId::BaseArmor: def.baseArmor = entry.value; break;
            case eChampionStatOverrideId::ArmorPerLevel: def.armorPerLevel = entry.value; break;
            case eChampionStatOverrideId::BaseMr: def.baseMr = entry.value; break;
            case eChampionStatOverrideId::MrPerLevel: def.mrPerLevel = entry.value; break;
            case eChampionStatOverrideId::BaseAttackSpeed: def.baseAttackSpeed = entry.value; break;
            case eChampionStatOverrideId::AttackSpeedPerLevel: def.attackSpeedPerLevel = entry.value; break;
            case eChampionStatOverrideId::BaseMoveSpeed: def.baseMoveSpeed = entry.value; break;
            case eChampionStatOverrideId::BaseAttackRange: def.baseAttackRange = entry.value; break;
            default: break;
            }
        }
    }

    bool_t TryResolvePracticeEffectiveAttackSpeed(
        const CWorld& world,
        EntityID entity,
        f32_t& outAttackSpeed)
    {
        const PracticeChampionStatOverrideComponent* pOverrides =
            world.TryGetComponent<PracticeChampionStatOverrideComponent>(entity);
        if (!pOverrides)
            return false;

        const u8_t count = (std::min)(
            pOverrides->count,
            PracticeChampionStatOverrideComponent::kMaxEntries);
        for (u8_t i = 0u; i < count; ++i)
        {
            const PracticeChampionStatOverrideEntry& entry = pOverrides->entries[i];
            if (static_cast<eChampionStatOverrideId>(entry.statId) ==
                eChampionStatOverrideId::EffectiveAttackSpeed)
            {
                outAttackSpeed = entry.value;
                return true;
            }
        }
        return false;
    }

    void ApplyPracticeItemStatOverrides(
        const CWorld& world,
        EntityID entity,
        u16_t itemId,
        ItemStatModifier& stats)
    {
        const PracticeItemStatOverrideComponent* pOverrides =
            world.TryGetComponent<PracticeItemStatOverrideComponent>(entity);
        if (!pOverrides || pOverrides->count == 0u)
            return;

        const u8_t count = (std::min)(
            pOverrides->count, PracticeItemStatOverrideComponent::kMaxEntries);
        for (u8_t i = 0u; i < count; ++i)
        {
            const PracticeItemStatOverrideEntry& entry = pOverrides->entries[i];
            if (entry.itemId != itemId)
                continue;

            switch (static_cast<eItemStatOverrideField>(entry.fieldId))
            {
            case eItemStatOverrideField::FlatAd: stats.flatAd = entry.value; break;
            case eItemStatOverrideField::FlatAp: stats.flatAp = entry.value; break;
            case eItemStatOverrideField::FlatHealth: stats.flatHealth = entry.value; break;
            case eItemStatOverrideField::FlatMana: stats.flatMana = entry.value; break;
            case eItemStatOverrideField::FlatArmor: stats.flatArmor = entry.value; break;
            case eItemStatOverrideField::FlatMr: stats.flatMr = entry.value; break;
            case eItemStatOverrideField::BonusAttackSpeed: stats.bonusAttackSpeed = entry.value; break;
            case eItemStatOverrideField::CritChance: stats.critChance = entry.value; break;
            case eItemStatOverrideField::CritDamageBonus: stats.critDamageBonus = entry.value; break;
            case eItemStatOverrideField::AbilityHaste: stats.abilityHaste = entry.value; break;
            case eItemStatOverrideField::PercentMoveSpeed: stats.percentMoveSpeed = entry.value; break;
            case eItemStatOverrideField::ArmorPenPercent: stats.armorPenPercent = entry.value; break;
            case eItemStatOverrideField::BonusArmorPenPercent: stats.bonusArmorPenPercent = entry.value; break;
            case eItemStatOverrideField::MagicPenPercent: stats.magicPenPercent = entry.value; break;
            case eItemStatOverrideField::FlatMoveSpeed: stats.flatMoveSpeed = entry.value; break;
            case eItemStatOverrideField::LifeSteal: stats.lifeSteal = entry.value; break;
            case eItemStatOverrideField::FlatMagicPen: stats.flatMagicPen = entry.value; break;
            case eItemStatOverrideField::Lethality: stats.lethality = entry.value; break;
            default: break;
            }
        }
    }

    template <typename TStats>
    StatComponent BuildBaseStatsFromValues(
        const TStats& def,
        eChampion legacyChampion,
        u8_t level)
    {
        const u8_t resolvedLevel = (level > 0) ? level : 1;

        StatComponent stat{};
        stat.championId = legacyChampion;
        stat.level = resolvedLevel;
        stat.hpMax = CCombatFormula::ResolveStatAtLevel(def.baseHp, def.hpPerLevel, resolvedLevel);
        stat.manaMax = CCombatFormula::ResolveStatAtLevel(def.baseMana, def.manaPerLevel, resolvedLevel);
        stat.resourceKind = def.resourceKind;
        stat.resourceRegenPerSec = def.resourceRegenPerSec;

        stat.baseAd = CCombatFormula::ResolveStatAtLevel(def.baseAd, def.adPerLevel, resolvedLevel);
        stat.bonusAd = 0.f;
        stat.ad = stat.baseAd;
        stat.ap = CCombatFormula::ResolveStatAtLevel(def.baseAp, def.apPerLevel, resolvedLevel);

        stat.baseArmor = CCombatFormula::ResolveStatAtLevel(def.baseArmor, def.armorPerLevel, resolvedLevel);
        stat.bonusArmor = 0.f;
        stat.armor = stat.baseArmor;

        stat.baseMr = CCombatFormula::ResolveStatAtLevel(def.baseMr, def.mrPerLevel, resolvedLevel);
        stat.bonusMr = 0.f;
        stat.mr = stat.baseMr;

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
}
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
    return BuildBaseStatsFromValues(def, def.championId, level);
}

StatComponent CStatSystem::BuildBaseStats(
    const ChampionStatBlock& stats,
    eChampion legacyChampion,
    u8_t level)
{
    return BuildBaseStatsFromValues(stats, legacyChampion, level);
}

void CStatSystem::Recompute(CWorld& world, EntityID entity, StatComponent& stat)
{
    ChampionStatsDef def = BuildDefaultChampionStatsDef(stat.championId);
    ApplyPracticeChampionStatOverrides(world, entity, def);

    const u8_t level = (stat.level > 0) ? stat.level : 1;
    const u32_t oldBuffHash = stat.buffMaskHash;
    const u32_t oldItemHash = stat.itemMaskHash;
    stat = BuildBaseStats(def, level);
    ApplyRuntimeModifiers(world, entity, stat, oldBuffHash, oldItemHash, nullptr);
}

void CStatSystem::Recompute(
    CWorld& world,
    EntityID entity,
    StatComponent& stat,
    const GameplayDefinitionPack& definitions)
{
    const u8_t level = (stat.level > 0) ? stat.level : 1;
    const u32_t oldBuffHash = stat.buffMaskHash;
    const u32_t oldItemHash = stat.itemMaskHash;

    const ChampionDefinitionComponent* identity =
        world.TryGetComponent<ChampionDefinitionComponent>(entity);
    const ChampionGameplayDef* definition =
        identity ? definitions.FindChampion(identity->championDefId) : nullptr;
    if (definition)
    {
        ChampionStatBlock statsCopy = definition->stats;
        ApplyPracticeChampionStatOverrides(world, entity, statsCopy);
        stat = BuildBaseStats(statsCopy, definition->legacyChampion, level);
    }
    else
    {
        static u32_t s_statPackMissLogCount = 0;
        if (s_statPackMissLogCount < 16u)
        {
            char msg[128]{};
            sprintf_s(msg,
                "[Data] required stat definition missing champ=%u\n",
                static_cast<u32_t>(stat.championId));
            WintersOutputAIDebugStringA(msg);
            ++s_statPackMissLogCount;
        }
        return;
    }

    ApplyRuntimeModifiers(world, entity, stat, oldBuffHash, oldItemHash, &definitions);
}

void CStatSystem::ApplyRuntimeModifiers(
    CWorld& world,
    EntityID entity,
    StatComponent& stat,
    u32_t oldBuffHash,
    u32_t oldItemHash,
    const GameplayDefinitionPack* pDefinitions)
{
    stat.buffMaskHash = oldBuffHash;
    stat.itemMaskHash = oldItemHash;

    f32_t fItemMoveSpeedMul = 1.f;
    if (world.HasComponent<InventoryComponent>(entity))
    {
        const InventoryComponent& inventory = world.GetComponent<InventoryComponent>(entity);
        const ItemRuntimeComponent* pItemRuntime =
            world.TryGetComponent<ItemRuntimeComponent>(entity);
        for (u8_t i = 0; i < inventory.count && i < InventoryComponent::kMaxSlots; ++i)
        {
            const ItemDef* pItem = CItemRegistry::Instance().Find(inventory.itemIds[i]);
            if (!pItem)
                continue;

            ItemStatModifier itemStats = pItem->stats;
            ApplyPracticeItemStatOverrides(world, entity, pItem->itemId, itemStats);

            stat.bonusAd += itemStats.flatAd;
            stat.ap += itemStats.flatAp;
            stat.hpMax += itemStats.flatHealth;
            if (stat.resourceKind == eChampionResourceKind::Mana)
            {
                stat.manaMax += itemStats.flatMana;
                if (pItemRuntime && pItemRuntime->slots[i].itemId == pItem->itemId)
                    stat.manaMax += static_cast<f32_t>(pItemRuntime->slots[i].bonusMana);
            }
            stat.bonusArmor += itemStats.flatArmor;
            stat.bonusMr += itemStats.flatMr;
            stat.bonusAttackSpeed += itemStats.bonusAttackSpeed;
            stat.critChance += itemStats.critChance;
            stat.critDamage += itemStats.critDamageBonus;
            stat.abilityHaste += itemStats.abilityHaste;
            stat.armorPenPercent += itemStats.armorPenPercent;
            stat.bonusArmorPenPercent += itemStats.bonusArmorPenPercent;
            stat.lethality += itemStats.lethality;
            stat.magicPenPercent += itemStats.magicPenPercent;
            stat.flatMagicPen += itemStats.flatMagicPen;
            stat.moveSpeed += itemStats.flatMoveSpeed * kLoLDistanceToWorldScale;
            stat.lifesteal += itemStats.lifeSteal;
            fItemMoveSpeedMul *= (1.f + itemStats.percentMoveSpeed);
        }

        for (u8_t i = 0; i < inventory.count && i < InventoryComponent::kMaxSlots; ++i)
        {
            const ItemDef* pItem = CItemRegistry::Instance().Find(inventory.itemIds[i]);
            if (pItem &&
                stat.resourceKind == eChampionResourceKind::Mana &&
                pItem->maxManaBonusAdRatio > 0.f)
            {
                stat.bonusAd += stat.manaMax * pItem->maxManaBonusAdRatio;
            }
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
            stat.bonusAttackSpeed += buff.bonusAttackSpeedPerStack * stacks;
            moveSpeedMul *= buff.moveSpeedMul;
        }
        stat.moveSpeed *= moveSpeedMul;
    }

    stat.ad = stat.baseAd + stat.bonusAd;
    if (CBuffSystem::HasObjectiveBuff(world, entity, eObjectiveBuffKind::Elder))
    {
        ObjectiveGameplayDef tuning{};
        if (pDefinitions)
        {
            if (const EconomyGameplayDef* economy = pDefinitions->FindEconomy())
                tuning = economy->objectives;
        }
        stat.ad *= tuning.elderAttackDamageMultiplier;
    }
    stat.armor = stat.baseArmor + stat.bonusArmor;
    stat.mr = stat.baseMr + stat.bonusMr;
    stat.attackSpeed = CCombatFormula::ResolveAttackSpeed(
        stat.baseAttackSpeed,
        stat.attackSpeedRatio,
        stat.attackSpeedGrowth,
        stat.bonusAttackSpeed,
        stat.level);

    f32_t fPracticeEffectiveAttackSpeed = 0.f;
    if (TryResolvePracticeEffectiveAttackSpeed(
        world,
        entity,
        fPracticeEffectiveAttackSpeed))
    {
        stat.attackSpeed = fPracticeEffectiveAttackSpeed;
    }

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

void CStatSystem::Execute(CWorld& world, const GameplayDefinitionPack& definitions)
{
    const auto entities = DeterministicEntityIterator<StatComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        auto& stat = world.GetComponent<StatComponent>(entity);
        if (stat.bDirty)
            Recompute(world, entity, stat, definitions);
    }
}

void CStatSystem::TickResourceRegeneration(CWorld& world, const TickContext& tc)
{
    const auto entities = DeterministicEntityIterator<StatComponent>::CollectSorted(world);
    for (EntityID entity : entities)
    {
        const StatComponent& stat = world.GetComponent<StatComponent>(entity);
        const bool_t bRegeneratesOverTime =
            stat.resourceKind == eChampionResourceKind::Mana ||
            stat.resourceKind == eChampionResourceKind::Energy;
        if (!bRegeneratesOverTime ||
            stat.resourceRegenPerSec <= 0.f ||
            !world.HasComponent<ChampionComponent>(entity) ||
            !world.HasComponent<HealthComponent>(entity))
        {
            continue;
        }

        const HealthComponent& health = world.GetComponent<HealthComponent>(entity);
        if (health.bIsDead || health.fCurrent <= 0.f)
            continue;

        ChampionComponent& champion = world.GetComponent<ChampionComponent>(entity);
        champion.maxMana = (std::max)(0.f, stat.manaMax);
        if (!std::isfinite(champion.mana))
            champion.mana = 0.f;
        champion.mana = (std::clamp)(
            champion.mana + stat.resourceRegenPerSec * tc.fDt,
            0.f,
            champion.maxMana);
    }
}
