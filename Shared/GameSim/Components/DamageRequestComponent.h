#pragma once

#include "WintersTypes.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Definitions/DamageTypes.h"

struct DamageRequest
{
    EntityID source = NULL_ENTITY;
    EntityID target = NULL_ENTITY;
    eTeam sourceTeam = eTeam::Neutral;
    eDamageType type = eDamageType::Physical;
    f32_t flatAmount = 0.f;
    f32_t amount = 0.f;
    u16_t skillId = 0;
    u8_t rank = 1;
    u8_t iSourceSlot = 0;
    eDamageSourceKind eSourceKind = eDamageSourceKind::Unknown;
    u8_t _pad = 0;
    f32_t skillDamageScale = 1.f;
    f32_t adRatioOverride = 0.f;
    f32_t bonusAdRatioOverride = 0.f;
    f32_t apRatioOverride = 0.f;
    f32_t targetMaxHpRatioOverride = 0.f;
    f32_t targetMissingHpRatioOverride = 0.f;
    f32_t critEligibleAmountOverride = 0.f;
    f32_t critDamageMultiplierOverride = 0.f;
    u32_t flags = 0;
};

using DamageRequestComponent = DamageRequest;
