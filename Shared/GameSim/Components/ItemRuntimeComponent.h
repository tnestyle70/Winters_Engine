#pragma once

#include "Shared/GameSim/Components/InventoryComponent.h"
#include "WintersTypes.h"

struct ItemRuntimeSlotState
{
    u16_t itemId = 0u;
    u16_t bonusMana = 0u;
    u8_t manaflowCharges = 0u;
    bool_t bSpellbladeArmed = false;
    u16_t reservedAlignment = 0u;
    u64_t nextManaflowChargeTick = 0u;
    u64_t spellbladeReadyTick = 0u;
    u64_t lightshieldReadyTick = 0u;
    u64_t activeReadyTick = 0u;
};

struct ItemRuntimeComponent
{
    ItemRuntimeSlotState slots[InventoryComponent::kMaxSlots]{};
};
