#pragma once

#include "WintersTypes.h"

struct ItemStatModifier
{
    f32_t flatAd = 0.f;
    f32_t flatAp = 0.f;
    f32_t flatHealth = 0.f;
    f32_t flatMana = 0.f;
    f32_t flatArmor = 0.f;
    f32_t flatMr = 0.f;
    f32_t bonusAttackSpeed = 0.f;
    f32_t critChance = 0.f;
    f32_t abilityHaste = 0.f;
    f32_t percentMoveSpeed = 0.f;
    f32_t armorPenPercent = 0.f;
    f32_t bonusArmorPenPercent = 0.f;
    f32_t lethality = 0.f;
    f32_t magicPenPercent = 0.f;
    f32_t flatMagicPen = 0.f;
    f32_t flatMoveSpeed = 0.f;
    f32_t lifeSteal = 0.f;
};

struct ItemDef
{
    u16_t itemId = 0;
    u16_t price = 0;
    ItemStatModifier stats{};
    const char* displayName = nullptr;
};

class CItemRegistry final
{
public:
    static CItemRegistry& Instance()
    {
        static CItemRegistry s_Instance;
        return s_Instance;
    }

    const ItemDef* Find(u16_t itemId) const
    {
        static const ItemDef kItems[] =
        {
            { 1001, 300, ItemStatModifier{ .flatMoveSpeed = 25.f }, "Boots" },
            { 1028, 400, ItemStatModifier{ .flatHealth = 150.f }, "Ruby Crystal" },
            { 1029, 300, ItemStatModifier{ .flatArmor = 15.f }, "Cloth Armor" },
            { 1033, 400, ItemStatModifier{ .flatMr = 20.f }, "Null-Magic Mantle" },
            { 1036, 350, ItemStatModifier{ .flatAd = 10.f }, "Long Sword" },
            { 1037, 875, ItemStatModifier{ .flatAd = 25.f }, "Pickaxe" },
            { 1038, 1300, ItemStatModifier{ .flatAd = 40.f }, "B. F. Sword" },
            { 1042, 250, ItemStatModifier{ .bonusAttackSpeed = 0.10f }, "Dagger" },
            { 1043, 700, ItemStatModifier{ .bonusAttackSpeed = 0.15f }, "Recurve Bow" },
            { 1052, 400, ItemStatModifier{ .flatAp = 20.f }, "Amplifying Tome" },
            { 1053, 900, ItemStatModifier{ .flatAd = 15.f, .lifeSteal = 0.07f }, "Vampiric Scepter" },
            { 1055, 450, ItemStatModifier{ .flatAd = 10.f, .flatHealth = 80.f }, "Doran's Blade" },
            { 1056, 400, ItemStatModifier{ .flatAp = 18.f, .flatHealth = 90.f }, "Doran's Ring" },
            { 1058, 1200, ItemStatModifier{ .flatAp = 65.f }, "Needlessly Large Rod" },
            { 3153, 3000, ItemStatModifier{ .flatAd = 40.f, .bonusAttackSpeed = 0.25f, .lifeSteal = 0.10f }, "Blade of the Ruined King" },
        };

        for (const ItemDef& item : kItems)
        {
            if (item.itemId == itemId)
                return &item;
        }

        return nullptr;
    }

private:
    CItemRegistry() = default;
};
