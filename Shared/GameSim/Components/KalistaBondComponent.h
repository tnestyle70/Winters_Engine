#pragma once

#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

#include <type_traits>

inline constexpr u16_t kKalistaOathswornItemId = 3599u;
inline constexpr u8_t kKalistaOathswornInventorySlot = 5u;
inline constexpr f32_t kKalistaOathswornContractRange = 14.f;
// Fate's Call reuses the Move wire shape for its ground point, but the marker
// distinguishes a post-snapshot launch choice from delayed ordinary movement.
inline constexpr u16_t kKalistaFateCallLaunchCommandMarker =
    kKalistaOathswornItemId;

enum class eKalistaOathswornStage : u8_t
{
    Binding = 0,
    Bound,
};

struct KalistaOathswornComponent
{
    EntityID entityAlly = NULL_ENTITY;
    eKalistaOathswornStage eStage = eKalistaOathswornStage::Binding;
    f32_t fRemainingSec = 1.5f;
};

struct KalistaOathswornByComponent
{
    EntityID entityKalista = NULL_ENTITY;
};

struct KalistaFateCallCarriedComponent
{
    EntityID entityOwner = NULL_ENTITY;
    bool_t bHidden = false;
};

static_assert(std::is_trivially_copyable_v<KalistaOathswornComponent>);
static_assert(std::is_trivially_copyable_v<KalistaOathswornByComponent>);
static_assert(std::is_trivially_copyable_v<KalistaFateCallCarriedComponent>);
