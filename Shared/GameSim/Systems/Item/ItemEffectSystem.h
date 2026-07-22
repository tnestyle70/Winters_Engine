#pragma once

#include "WintersTypes.h"

class CWorld;
struct DamageRequest;
struct ItemDef;
struct TickContext;

struct ItemOnHitResolution
{
    f32_t manaRestore = 0.f;
    u16_t itemId = 0u;
    u8_t inventorySlot = 0u;
    bool_t bSpellbladeTriggered = false;
    f32_t lightshieldHeal = 0.f;
    u16_t lightshieldItemId = 0u;
    u8_t lightshieldInventorySlot = 0u;
    bool_t bLightshieldTriggered = false;
};

class CItemEffectSystem final
{
public:
    static void OnAbilityCastAccepted(
        CWorld& world,
        const TickContext& tc,
        u32_t sourceEntity);

    static void PrepareOnHitDamage(
        CWorld& world,
        const TickContext& tc,
        DamageRequest& request,
        ItemOnHitResolution& outResolution);

    static void OnDamageLanded(
        CWorld& world,
        const TickContext& tc,
        const DamageRequest& request,
        const ItemOnHitResolution& onHitResolution,
        bool_t bApplied);

    static bool_t IsActiveReady(
        CWorld& world,
        const TickContext& tc,
        u32_t sourceEntity,
        u8_t inventorySlot,
        const ItemDef& item);

    static void CommitActiveCooldown(
        CWorld& world,
        const TickContext& tc,
        u32_t sourceEntity,
        u8_t inventorySlot,
        const ItemDef& item);

    static void SwapRuntimeSlots(
        CWorld& world,
        u32_t sourceEntity,
        u8_t sourceSlot,
        u8_t targetSlot);
};
