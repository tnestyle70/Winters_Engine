#pragma once

#include "../Definitions/ChampionStatsDef.h"
#include "../Definitions/LoLMatchContext.h"
#include "WintersTypes.h"

#include <cstddef>

struct StatComponent
{
    eChampion championId = eChampion::NONE;
    u8_t level = 1;
    eChampionResourceKind resourceKind = eChampionResourceKind::Mana;
    u8_t reservedIdentityAlignment = 0u;
    u32_t buffMaskHash = 0;
    u32_t itemMaskHash = 0;

    f32_t hpMax = 0.f;
    f32_t manaMax = 0.f;
    f32_t resourceRegenPerSec = 0.f;

    f32_t baseAd = 0.f;
    f32_t bonusAd = 0.f;
    f32_t ad = 0.f;
    f32_t ap = 0.f;

    f32_t baseArmor = 0.f;
    f32_t bonusArmor = 0.f;
    f32_t armor = 0.f;

    f32_t baseMr = 0.f;
    f32_t bonusMr = 0.f;
    f32_t mr = 0.f;

    f32_t baseAttackSpeed = 0.f;
    f32_t attackSpeedRatio = 0.f;
    f32_t attackSpeedGrowth = 0.f;
    f32_t bonusAttackSpeed = 0.f;
    f32_t attackSpeed = 0.f;

    f32_t attackRange = 0.f;
    f32_t moveSpeed = 0.f;

    f32_t critChance = 0.f;
    f32_t critDamage = 1.75f;
    f32_t lifesteal = 0.f;
    f32_t spellVamp = 0.f;

    f32_t armorPen = 0.f;
    f32_t armorPenPercent = 0.f;
    f32_t bonusArmorPenPercent = 0.f;
    f32_t lethality = 0.f;

    f32_t mrPen = 0.f;
    f32_t magicPenPercent = 0.f;
    f32_t flatMagicPen = 0.f;

    f32_t cdr = 0.f;
    f32_t abilityHaste = 0.f;

    bool_t bDirty = true;
    u8_t reservedTail[3]{};
};

static_assert(sizeof(StatComponent) == 148u);
static_assert(offsetof(StatComponent, buffMaskHash) == 4u);
static_assert(offsetof(StatComponent, abilityHaste) == 140u);
static_assert(offsetof(StatComponent, bDirty) == 144u);
