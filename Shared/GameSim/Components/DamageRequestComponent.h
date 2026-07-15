#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"

#include <cstdint>

enum class eDamageType : uint8_t
{
    Physical = 0,
    Magic = 1,
    True = 2,
};

enum eDamageFlags : uint32_t
{
    DamageFlag_None = 0,
    DamageFlag_CanCrit = 1u << 0,
    DamageFlag_CanLifesteal = 1u << 1,
    DamageFlag_OnHit = 1u << 2,
};

enum class eDamageSourceKind : uint8_t
{
    Unknown = 0,
    BasicAttack = 1,
    Skill = 2,
    Item = 3,
    Rune = 4,
};

struct DamageRequest
{
    EntityID source = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    eTeam sourceTeam = eTeam::Neutral;
    eDamageType type = eDamageType::Physical;
    f32_t flatAmount = 0.f;
    f32_t amount = 0.f;
    uint16_t skillId = 0;
    uint8_t rank = 1;
    uint8_t iSourceSlot = 0;
    eDamageSourceKind eSourceKind = eDamageSourceKind::Unknown;
    uint8_t _pad = 0;
    f32_t adRatioOverride = 0.f;
    f32_t bonusAdRatioOverride = 0.f;
    f32_t apRatioOverride = 0.f;
    f32_t targetMaxHpRatioOverride = 0.f;
    f32_t targetMissingHpRatioOverride = 0.f;
    uint32_t flags = 0;
};

using DamageRequestComponent = DamageRequest;
