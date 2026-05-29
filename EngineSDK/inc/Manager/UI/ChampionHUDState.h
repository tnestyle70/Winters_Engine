#pragma once

#include "Entity.h"
#include "GameContext.h"
#include "WintersTypes.h"

#include <array>

namespace Engine
{
    struct ChampionHUDState
    {
        EntityID LocalEntity = NULL_ENTITY;
        eChampion Champion = eChampion::IRELIA;

        f32_t Hp = 640.f;
        f32_t MaxHp = 640.f;
        f32_t Mp = 350.f;
        f32_t MaxMp = 350.f;
        f32_t XpRatio = 0.f;
        f32_t Shield = 0.f;
        f32_t PassiveValue = 0.f;
        f32_t PassiveMax = 100.f;
        f32_t PassiveShield = 0.f;
        f32_t PassiveShieldMax = 100.f;
        bool_t bUsesPassiveResource = false;
        f32_t XpCurrent = 0.f;
        f32_t XpRequired = 280.f;
        u32_t Gold = 10000;
        u8_t Level = 1;

        std::array<u16_t, 6> InventoryItemIds{};
        std::array<u8_t, 5> SkillRanks{};
        u8_t SkillPoints = 0;
        bool_t bShopOpen = false;
        bool_t bStunned = false;
        std::array<f32_t, 4> Cooldowns{};
        std::array<f32_t, 4> MaxCooldowns{};

        f32_t Ad = 0.f;
        f32_t Ap = 0.f;
        f32_t Armor = 0.f;
        f32_t Mr = 0.f;
        f32_t AttackSpeed = 0.f;
        f32_t AttackRange = 0.f;
        f32_t MoveSpeed = 0.f;
        f32_t CritChance = 0.f;
        f32_t AbilityHaste = 0.f;
    };
}
