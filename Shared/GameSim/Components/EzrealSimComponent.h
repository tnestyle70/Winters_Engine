#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

struct EzrealPendingCastComponent
{
    EntityHandle hCaster = NULL_ENTITY_HANDLE;
    Vec3 vOrigin{};
    Vec3 vGroundTarget{};
    Vec3 vDirection{};
    u64_t uLaunchTick = 0u;
    f32_t fPaidManaCost = 0.f;
    u8_t uSlot = 0u;
    u8_t uRank = 1u;
    bool_t bHasGroundTarget = false;
};

inline constexpr u32_t kEzrealRisingSpellForceBuffDefId = 0x455A5001u;

struct EzrealEssenceFluxMarkComponent
{
    EntityHandle hSource = NULL_ENTITY_HANDLE;
    EntityHandle hTarget = NULL_ENTITY_HANDLE;
    u32_t uSourceNet = 0u;
    u32_t uTargetNet = 0u;
    u64_t uExpireTick = 0u;
    u8_t uRank = 1u;
};

static_assert(std::is_trivially_copyable_v<EzrealPendingCastComponent>);
static_assert(std::is_trivially_copyable_v<EzrealEssenceFluxMarkComponent>);
