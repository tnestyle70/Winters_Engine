#include "Shared/GameSim/Systems/Item/ItemEffectSystem.h"

#include "Shared/GameSim/Components/ChampionComponent.h"
#include "Shared/GameSim/Components/DamageRequestComponent.h"
#include "Shared/GameSim/Components/HealthComponent.h"
#include "Shared/GameSim/Components/InventoryComponent.h"
#include "Shared/GameSim/Components/ItemRuntimeComponent.h"
#include "Shared/GameSim/Components/StatComponent.h"
#include "Shared/GameSim/Core/Determinism/DeterministicTime.h"
#include "Shared/GameSim/Core/World/World.h"
#include "Shared/GameSim/Definitions/ItemDef.h"
#include "Shared/GameSim/Systems/CommandExecutor/ICommandExecutor.h"

#include <algorithm>
#include <cmath>

namespace
{
    u64_t SecondsToTicksCeil(f32_t seconds)
    {
        if (!std::isfinite(seconds) || seconds <= 0.f)
            return 1u;
        return (std::max)(
            1ull,
            static_cast<u64_t>(std::ceil(
                seconds * static_cast<f32_t>(DeterministicTime::kTicksPerSecond))));
    }

    ItemRuntimeComponent& ResolveRuntime(CWorld& world, u32_t entity)
    {
        if (!world.HasComponent<ItemRuntimeComponent>(entity))
            world.AddComponent<ItemRuntimeComponent>(entity, ItemRuntimeComponent{});
        return world.GetComponent<ItemRuntimeComponent>(entity);
    }

    ItemRuntimeSlotState& SyncSlotState(
        ItemRuntimeComponent& runtime,
        u8_t slot,
        u16_t itemId,
        const ItemDef& item)
    {
        ItemRuntimeSlotState& state = runtime.slots[slot];
        if (state.itemId == itemId)
            return state;

        state = {};
        state.itemId = itemId;
        if (item.manaflow.bValid)
            state.manaflowCharges = item.manaflow.maxCharges;
        return state;
    }

    void RechargeManaflow(
        ItemRuntimeSlotState& state,
        const ItemManaflowDef& manaflow,
        u64_t tickIndex)
    {
        if (state.manaflowCharges >= manaflow.maxCharges)
        {
            state.manaflowCharges = manaflow.maxCharges;
            state.nextManaflowChargeTick = 0u;
            return;
        }
        if (state.nextManaflowChargeTick == 0u ||
            tickIndex < state.nextManaflowChargeTick)
        {
            return;
        }

        const u64_t rechargeTicks = SecondsToTicksCeil(manaflow.rechargeSec);
        const u64_t restored = 1u +
            (tickIndex - state.nextManaflowChargeTick) / rechargeTicks;
        state.manaflowCharges = static_cast<u8_t>((std::min)(
            static_cast<u64_t>(manaflow.maxCharges),
            static_cast<u64_t>(state.manaflowCharges) + restored));
        state.nextManaflowChargeTick =
            state.manaflowCharges < manaflow.maxCharges
                ? state.nextManaflowChargeTick + restored * rechargeTicks
                : 0u;
    }

    bool_t IsAttackOrAbilityHit(const DamageRequest& request)
    {
        return request.eSourceKind == eDamageSourceKind::BasicAttack ||
            request.eSourceKind == eDamageSourceKind::Skill;
    }
}

void CItemEffectSystem::OnAbilityCastAccepted(
    CWorld& world,
    const TickContext& tc,
    u32_t sourceEntity)
{
    if (sourceEntity == NULL_ENTITY ||
        !world.IsAlive(sourceEntity) ||
        !world.HasComponent<InventoryComponent>(sourceEntity))
    {
        return;
    }

    const InventoryComponent& inventory =
        world.GetComponent<InventoryComponent>(sourceEntity);
    ItemRuntimeComponent& runtime = ResolveRuntime(world, sourceEntity);
    for (u8_t slot = 0u;
        slot < inventory.count && slot < InventoryComponent::kMaxSlots;
        ++slot)
    {
        const ItemDef* item = CItemRegistry::Instance().Find(inventory.itemIds[slot]);
        if (!item || !item->spellblade.bValid)
            continue;

        ItemRuntimeSlotState& state =
            SyncSlotState(runtime, slot, item->itemId, *item);
        if (tc.tickIndex >= state.spellbladeReadyTick)
            state.bSpellbladeArmed = true;
        break;
    }
}

void CItemEffectSystem::PrepareOnHitDamage(
    CWorld& world,
    const TickContext& tc,
    DamageRequest& request,
    ItemOnHitResolution& outResolution)
{
    outResolution = {};
    if (request.type != eDamageType::Physical ||
        (request.flags & DamageFlag_OnHit) == 0u ||
        !IsAttackOrAbilityHit(request) ||
        request.source == NULL_ENTITY ||
        !world.IsAlive(request.source) ||
        !world.HasComponent<InventoryComponent>(request.source) ||
        !world.HasComponent<StatComponent>(request.source) ||
        !world.HasComponent<ChampionComponent>(request.source))
    {
        return;
    }

    const InventoryComponent& inventory =
        world.GetComponent<InventoryComponent>(request.source);
    ItemRuntimeComponent& runtime = ResolveRuntime(world, request.source);

    if (request.eSourceKind == eDamageSourceKind::BasicAttack &&
        request.target != NULL_ENTITY &&
        world.HasComponent<ChampionComponent>(request.target))
    {
        for (u8_t slot = 0u;
            slot < inventory.count && slot < InventoryComponent::kMaxSlots;
            ++slot)
        {
            const ItemDef* item =
                CItemRegistry::Instance().Find(inventory.itemIds[slot]);
            if (!item || !item->lightshieldStrike.bValid)
                continue;

            ItemRuntimeSlotState& state =
                SyncSlotState(runtime, slot, item->itemId, *item);
            if (tc.tickIndex < state.lightshieldReadyTick)
                break;

            const StatComponent& stat =
                world.GetComponent<StatComponent>(request.source);
            f32_t missingHealth = 0.f;
            if (world.HasComponent<HealthComponent>(request.source))
            {
                const HealthComponent& health =
                    world.GetComponent<HealthComponent>(request.source);
                missingHealth = (std::max)(
                    0.f,
                    health.fMaximum - health.fCurrent);
            }

            request.flags |=
                DamageFlag_CanCrit |
                DamageFlag_ForceCrit |
                DamageFlag_ShowCriticalIndicator;
            request.critDamageMultiplierOverride =
                item->lightshieldStrike.critDamageMultiplier;
            outResolution.lightshieldHeal = (std::max)(
                0.f,
                stat.baseAd * item->lightshieldStrike.healBaseAdRatio +
                    missingHealth *
                        item->lightshieldStrike.healMissingHealthRatio);
            outResolution.lightshieldItemId = item->itemId;
            outResolution.lightshieldInventorySlot = slot;
            outResolution.bLightshieldTriggered = true;
            break;
        }
    }

    for (u8_t slot = 0u;
        slot < inventory.count && slot < InventoryComponent::kMaxSlots;
        ++slot)
    {
        const ItemDef* item = CItemRegistry::Instance().Find(inventory.itemIds[slot]);
        if (!item || !item->spellblade.bValid)
            continue;

        ItemRuntimeSlotState& state =
            SyncSlotState(runtime, slot, item->itemId, *item);
        if (!state.bSpellbladeArmed || tc.tickIndex < state.spellbladeReadyTick)
            break;

        const StatComponent& stat = world.GetComponent<StatComponent>(request.source);
        const f32_t bonusDamage = (std::max)(
            0.f,
            stat.baseAd * item->spellblade.baseAdRatio +
                stat.critChance * item->spellblade.critChanceFlatScale);
        request.flatAmount += bonusDamage;

        outResolution.manaRestore =
            stat.resourceKind == eChampionResourceKind::Mana
                ? (std::max)(0.f, bonusDamage * item->spellblade.manaRestoreRatio)
                : 0.f;
        outResolution.itemId = item->itemId;
        outResolution.inventorySlot = slot;
        outResolution.bSpellbladeTriggered = true;
        break;
    }
}

void CItemEffectSystem::OnDamageLanded(
    CWorld& world,
    const TickContext& tc,
    const DamageRequest& request,
    const ItemOnHitResolution& onHitResolution,
    bool_t bApplied)
{
    if (!bApplied ||
        !IsAttackOrAbilityHit(request) ||
        request.source == NULL_ENTITY ||
        request.target == NULL_ENTITY ||
        !world.IsAlive(request.source) ||
        !world.IsAlive(request.target) ||
        !world.HasComponent<InventoryComponent>(request.source))
    {
        return;
    }

    InventoryComponent& inventory =
        world.GetComponent<InventoryComponent>(request.source);
    ItemRuntimeComponent& runtime = ResolveRuntime(world, request.source);
    const StatComponent* sourceStat =
        world.TryGetComponent<StatComponent>(request.source);
    const bool_t bUsesMana = sourceStat &&
        sourceStat->resourceKind == eChampionResourceKind::Mana;

    if (onHitResolution.bLightshieldTriggered &&
        onHitResolution.lightshieldInventorySlot <
            InventoryComponent::kMaxSlots &&
        inventory.itemIds[onHitResolution.lightshieldInventorySlot] ==
            onHitResolution.lightshieldItemId)
    {
        const ItemDef* item = CItemRegistry::Instance().Find(
            onHitResolution.lightshieldItemId);
        ItemRuntimeSlotState& state =
            runtime.slots[onHitResolution.lightshieldInventorySlot];
        if (item && item->lightshieldStrike.bValid &&
            state.itemId == item->itemId &&
            tc.tickIndex >= state.lightshieldReadyTick)
        {
            if (world.HasComponent<HealthComponent>(request.source))
            {
                HealthComponent& health =
                    world.GetComponent<HealthComponent>(request.source);
                if (!health.bIsDead && health.fCurrent > 0.f)
                {
                    health.fCurrent = (std::min)(
                        health.fMaximum,
                        health.fCurrent + onHitResolution.lightshieldHeal);
                    if (world.HasComponent<ChampionComponent>(request.source))
                    {
                        ChampionComponent& champion =
                            world.GetComponent<ChampionComponent>(request.source);
                        champion.hp = health.fCurrent;
                        champion.maxHp = health.fMaximum;
                    }
                }
            }
            state.lightshieldReadyTick = tc.tickIndex +
                SecondsToTicksCeil(item->lightshieldStrike.cooldownSec);
        }
    }

    if (onHitResolution.bSpellbladeTriggered &&
        onHitResolution.inventorySlot < InventoryComponent::kMaxSlots &&
        inventory.itemIds[onHitResolution.inventorySlot] == onHitResolution.itemId)
    {
        const ItemDef* item = CItemRegistry::Instance().Find(onHitResolution.itemId);
        ItemRuntimeSlotState& state =
            runtime.slots[onHitResolution.inventorySlot];
        if (item && item->spellblade.bValid &&
            state.itemId == item->itemId &&
            state.bSpellbladeArmed &&
            tc.tickIndex >= state.spellbladeReadyTick)
        {
            if (bUsesMana && world.HasComponent<ChampionComponent>(request.source))
            {
                ChampionComponent& champion =
                    world.GetComponent<ChampionComponent>(request.source);
                champion.mana = (std::min)(
                    champion.maxMana,
                    champion.mana + onHitResolution.manaRestore);
            }
            state.bSpellbladeArmed = false;
            state.spellbladeReadyTick = tc.tickIndex +
                SecondsToTicksCeil(item->spellblade.cooldownSec);
        }
    }

    for (u8_t slot = 0u;
        slot < inventory.count && slot < InventoryComponent::kMaxSlots;
        ++slot)
    {
        if (!bUsesMana)
            break;

        const ItemDef* item = CItemRegistry::Instance().Find(inventory.itemIds[slot]);
        if (!item || !item->manaflow.bValid)
            continue;

        ItemRuntimeSlotState& state =
            SyncSlotState(runtime, slot, item->itemId, *item);
        RechargeManaflow(state, item->manaflow, tc.tickIndex);
        if (state.manaflowCharges == 0u ||
            state.bonusMana >= item->manaflow.maxBonusMana)
        {
            break;
        }

        const bool_t bChampionTarget =
            world.HasComponent<ChampionComponent>(request.target);
        const u16_t multiplier = bChampionTarget
            ? (std::max)(static_cast<u16_t>(1u),
                static_cast<u16_t>(item->manaflow.championMultiplier))
            : 1u;
        const u16_t grant = static_cast<u16_t>((std::min)(
            static_cast<u32_t>(item->manaflow.maxBonusMana - state.bonusMana),
            static_cast<u32_t>(item->manaflow.manaPerTrigger) * multiplier));

        --state.manaflowCharges;
        if (state.nextManaflowChargeTick == 0u)
        {
            state.nextManaflowChargeTick = tc.tickIndex +
                SecondsToTicksCeil(item->manaflow.rechargeSec);
        }
        state.bonusMana = static_cast<u16_t>(state.bonusMana + grant);

        if (world.HasComponent<StatComponent>(request.source))
            world.GetComponent<StatComponent>(request.source).bDirty = true;

        if (state.bonusMana >= item->manaflow.maxBonusMana &&
            item->manaflow.transformItemId != 0u &&
            CItemRegistry::Instance().Find(item->manaflow.transformItemId))
        {
            const u16_t previousItemId = inventory.itemIds[slot];
            inventory.itemIds[slot] = item->manaflow.transformItemId;
            state = {};
            state.itemId = inventory.itemIds[slot];
            if (world.HasComponent<StatComponent>(request.source))
            {
                StatComponent& stat = world.GetComponent<StatComponent>(request.source);
                stat.itemMaskHash ^=
                    static_cast<u32_t>(previousItemId) * 16777619u;
                stat.itemMaskHash ^=
                    static_cast<u32_t>(inventory.itemIds[slot]) * 16777619u;
                stat.bDirty = true;
            }
        }
        break;
    }
}

bool_t CItemEffectSystem::IsActiveReady(
    CWorld& world,
    const TickContext& tc,
    u32_t sourceEntity,
    u8_t inventorySlot,
    const ItemDef& item)
{
    if (sourceEntity == NULL_ENTITY ||
        !world.IsAlive(sourceEntity) ||
        inventorySlot >= InventoryComponent::kMaxSlots ||
        !item.active.bValid ||
        !world.HasComponent<InventoryComponent>(sourceEntity) ||
        world.GetComponent<InventoryComponent>(sourceEntity)
            .itemIds[inventorySlot] != item.itemId)
    {
        return false;
    }

    ItemRuntimeComponent& runtime = ResolveRuntime(world, sourceEntity);
    ItemRuntimeSlotState& state =
        SyncSlotState(runtime, inventorySlot, item.itemId, item);
    return tc.tickIndex >= state.activeReadyTick;
}

void CItemEffectSystem::CommitActiveCooldown(
    CWorld& world,
    const TickContext& tc,
    u32_t sourceEntity,
    u8_t inventorySlot,
    const ItemDef& item)
{
    if (sourceEntity == NULL_ENTITY ||
        !world.IsAlive(sourceEntity) ||
        inventorySlot >= InventoryComponent::kMaxSlots)
    {
        return;
    }

    ItemRuntimeComponent& runtime = ResolveRuntime(world, sourceEntity);
    ItemRuntimeSlotState& state =
        SyncSlotState(runtime, inventorySlot, item.itemId, item);
    state.activeReadyTick = item.active.cooldownSec > 0.f
        ? tc.tickIndex + SecondsToTicksCeil(item.active.cooldownSec)
        : tc.tickIndex;
}

void CItemEffectSystem::SwapRuntimeSlots(
    CWorld& world,
    u32_t sourceEntity,
    u8_t sourceSlot,
    u8_t targetSlot)
{
    if (sourceEntity == NULL_ENTITY ||
        sourceSlot >= InventoryComponent::kMaxSlots ||
        targetSlot >= InventoryComponent::kMaxSlots ||
        sourceSlot == targetSlot ||
        !world.HasComponent<ItemRuntimeComponent>(sourceEntity))
    {
        return;
    }

    ItemRuntimeComponent& runtime =
        world.GetComponent<ItemRuntimeComponent>(sourceEntity);
    std::swap(runtime.slots[sourceSlot], runtime.slots[targetSlot]);
}
