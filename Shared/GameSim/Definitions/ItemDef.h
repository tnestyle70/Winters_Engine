#pragma once

#include "Shared/GameSim/Definitions/DamageTypes.h"
#include "WintersTypes.h"

#include <cstddef>
#include <vector>

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
    DamageFormulaDef onHitDamage{};
};

class CItemRegistry final
{
public:
    static CItemRegistry& Instance();

    const ItemDef* Find(u16_t itemId) const;
    // 정의 팩 아이템 배열로 전체 교체 (nullptr/0 은 무시 = 기존 값 유지).
    void LoadFromItemDefs(const ItemDef* items, std::size_t count);
    // 컴파일된 기본 표(kItems)로 복귀.
    void ResetToDefaults();

private:
    CItemRegistry();
    ~CItemRegistry() = default;
    CItemRegistry(const CItemRegistry&) = delete;
    CItemRegistry& operator=(const CItemRegistry&) = delete;

    std::vector<ItemDef> m_Items;
};
