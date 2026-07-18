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
    f32_t critDamageBonus = 0.f;
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

struct ItemSpellbladeDef
{
    bool_t bValid = false;
    f32_t cooldownSec = 0.f;
    f32_t baseAdRatio = 0.f;
    f32_t critChanceFlatScale = 0.f;
    f32_t manaRestoreRatio = 0.f;
};

struct ItemManaflowDef
{
    bool_t bValid = false;
    u8_t maxCharges = 0u;
    u8_t championMultiplier = 1u;
    u16_t manaPerTrigger = 0u;
    u16_t maxBonusMana = 0u;
    u16_t transformItemId = 0u;
    f32_t rechargeSec = 0.f;
};

enum class eItemActiveKind : u8_t
{
    None = 0,
    Ward,
    Stasis,
    Cleanse,
    KalistaOathsworn,
};

struct ItemLightshieldStrikeDef
{
    bool_t bValid = false;
    f32_t cooldownSec = 0.f;
    f32_t critDamageMultiplier = 0.f;
    f32_t healBaseAdRatio = 0.f;
    f32_t healMissingHealthRatio = 0.f;
};

struct ItemActiveDef
{
    bool_t bValid = false;
    eItemActiveKind kind = eItemActiveKind::None;
    f32_t cooldownSec = 0.f;
    f32_t durationSec = 0.f;
};

struct ItemDef
{
    u16_t itemId = 0;
    u16_t price = 0;
    bool_t bPurchasable = true;
    ItemStatModifier stats{};
    const char* displayName = nullptr;
    DamageFormulaDef onHitDamage{};
    ItemSpellbladeDef spellblade{};
    ItemManaflowDef manaflow{};
    ItemLightshieldStrikeDef lightshieldStrike{};
    ItemActiveDef active{};
    f32_t maxManaBonusAdRatio = 0.f;
};

class CItemRegistry final
{
public:
    static CItemRegistry& Instance();

    const ItemDef* Find(u16_t itemId) const;
    // 정의 팩 아이템 배열로 전체 교체 (nullptr/0 은 무시 = 기존 값 유지).
    bool_t LoadFromItemDefs(const ItemDef* items, std::size_t count);
    // 컴파일된 기본 표(kItems)로 복귀.
    void Clear();

private:
    CItemRegistry() = default;
    ~CItemRegistry() = default;
    CItemRegistry(const CItemRegistry&) = delete;
    CItemRegistry& operator=(const CItemRegistry&) = delete;

    std::vector<ItemDef> m_Items;
};
