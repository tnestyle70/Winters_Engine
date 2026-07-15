#include "Shared/GameSim/Definitions/ItemDef.h"

namespace
{
    // 협곡 실아이템만 등록한다 (Data Dragon 16.13.1 SR 구매가능 검증, 2026-07-14).
    // 패시브/액티브 효과 필드가 없으므로 스탯 근사치만 반영한다.
    // 값의 진실은 Data/LoL/ServerPrivate/Gameplay/ItemGameplayDefs.json 이고,
    // 아래 기본 표는 팩 미장착 시 폴백과 동일해야 한다.
    const ItemDef kItems[] =
    {
        { 1001, 300, ItemStatModifier{ .flatMoveSpeed = 25.f }, "Boots" },
        { 1011, 900, ItemStatModifier{ .flatHealth = 350.f }, "Giant's Belt" },
        { 1018, 600, ItemStatModifier{ .critChance = 0.15f }, "Cloak of Agility" },
        { 1026, 850, ItemStatModifier{ .flatAp = 45.f }, "Blasting Wand" },
        { 1027, 400, ItemStatModifier{ .flatMana = 300.f }, "Sapphire Crystal" },
        { 1028, 400, ItemStatModifier{ .flatHealth = 150.f }, "Ruby Crystal" },
        { 1029, 300, ItemStatModifier{ .flatArmor = 15.f }, "Cloth Armor" },
        { 1031, 800, ItemStatModifier{ .flatArmor = 40.f }, "Chain Vest" },
        { 1033, 400, ItemStatModifier{ .flatMr = 20.f }, "Null-Magic Mantle" },
        { 1036, 350, ItemStatModifier{ .flatAd = 10.f }, "Long Sword" },
        { 1037, 875, ItemStatModifier{ .flatAd = 25.f }, "Pickaxe" },
        { 1038, 1300, ItemStatModifier{ .flatAd = 40.f }, "B. F. Sword" },
        { 1042, 250, ItemStatModifier{ .bonusAttackSpeed = 0.10f }, "Dagger" },
        { 1043, 700, ItemStatModifier{ .bonusAttackSpeed = 0.15f }, "Recurve Bow" },
        { 1052, 400, ItemStatModifier{ .flatAp = 20.f }, "Amplifying Tome" },
        { 1053, 900, ItemStatModifier{ .flatAd = 15.f, .lifeSteal = 0.07f }, "Vampiric Scepter" },
        { 1054, 450, ItemStatModifier{ .flatHealth = 110.f }, "Doran's Shield" },
        { 1055, 450, ItemStatModifier{ .flatAd = 10.f, .flatHealth = 80.f }, "Doran's Blade" },
        { 1056, 400, ItemStatModifier{ .flatAp = 18.f, .flatHealth = 90.f }, "Doran's Ring" },
        { 1057, 900, ItemStatModifier{ .flatMr = 45.f }, "Negatron Cloak" },
        { 1058, 1200, ItemStatModifier{ .flatAp = 65.f }, "Needlessly Large Rod" },
        { 3006, 1100, ItemStatModifier{ .bonusAttackSpeed = 0.25f, .flatMoveSpeed = 45.f }, "Berserker's Greaves" },
        { 3020, 1100, ItemStatModifier{ .flatMagicPen = 15.f, .flatMoveSpeed = 45.f }, "Sorcerer's Shoes" },
        { 3031, 3450, ItemStatModifier{ .flatAd = 65.f, .critChance = 0.25f }, "Infinity Edge" },
        { 3047, 1200, ItemStatModifier{ .flatArmor = 25.f, .flatMoveSpeed = 45.f }, "Plated Steelcaps" },
        { 3065, 2700, ItemStatModifier{ .flatHealth = 450.f, .flatMr = 50.f, .abilityHaste = 10.f }, "Spirit Visage" },
        { 3072, 3400, ItemStatModifier{ .flatAd = 80.f, .lifeSteal = 0.15f }, "Bloodthirster" },
        { 3078, 3333, ItemStatModifier{ .flatAd = 45.f, .flatHealth = 300.f, .bonusAttackSpeed = 0.30f, .abilityHaste = 15.f }, "Trinity Force" },
        { 3089, 3500, ItemStatModifier{ .flatAp = 130.f }, "Rabadon's Deathcap" },
        { 3111, 1200, ItemStatModifier{ .flatMr = 20.f, .flatMoveSpeed = 45.f }, "Mercury's Treads" },
        { 3153, 3000, ItemStatModifier{ .flatAd = 40.f, .bonusAttackSpeed = 0.25f, .lifeSteal = 0.10f }, "Blade of the Ruined King" },
        { 3157, 3250, ItemStatModifier{ .flatAp = 105.f, .flatArmor = 50.f }, "Zhonya's Hourglass" },
        { 3158, 950, ItemStatModifier{ .abilityHaste = 15.f, .flatMoveSpeed = 45.f }, "Ionian Boots of Lucidity" },
        { 3742, 2900, ItemStatModifier{ .flatHealth = 350.f, .flatArmor = 45.f, .flatMoveSpeed = 5.f }, "Dead Man's Plate" },
    };
}

CItemRegistry& CItemRegistry::Instance()
{
    static CItemRegistry s_Instance;
    return s_Instance;
}

CItemRegistry::CItemRegistry()
{
    ResetToDefaults();
}

const ItemDef* CItemRegistry::Find(u16_t itemId) const
{
    for (const ItemDef& item : m_Items)
    {
        if (item.itemId == itemId)
            return &item;
    }

    return nullptr;
}

void CItemRegistry::LoadFromItemDefs(const ItemDef* items, std::size_t count)
{
    if (!items || count == 0u)
        return;
    m_Items.assign(items, items + count);
}

void CItemRegistry::ResetToDefaults()
{
    m_Items.assign(kItems, kItems + sizeof(kItems) / sizeof(kItems[0]));
}
