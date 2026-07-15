#pragma once

#include "Shared/GameSim/Components/GameplayComponents.h"
#include "Shared/GameSim/Core/Ecs/Entity.h"
#include "WintersTypes.h"

#include <cstddef>
#include <type_traits>

struct AnnieSimComponent
{
    u8_t stunStacks = 0;
    bool_t bStunReady = false;
    u8_t reservedShieldAlignment[2]{};
    f32_t fEShieldRemainingSec = 0.f;
    f32_t fEShieldAmount = 0.f;
    f32_t fEShieldMaxAmount = 0.f;
    EntityID tibbersEntity = NULL_ENTITY;
};

struct AnnieTibbersComponent
{
    EntityID owner = NULL_ENTITY;
    eTeam ownerTeam = eTeam::Neutral;
    u8_t reservedRemainingAlignment[3]{};
    f32_t fRemainingSec = 45.f;
    EntityID commandTarget = NULL_ENTITY;
    Vec3 commandPosition{};
    bool_t bHasCommandPosition = false;
    u8_t reservedSequenceAlignment[3]{};
    u32_t commandSeq = 0u;
};

static_assert(std::is_trivially_copyable_v<AnnieSimComponent>);
static_assert(std::is_trivially_copyable_v<AnnieTibbersComponent>);
static_assert(sizeof(AnnieSimComponent) == 20u);
static_assert(offsetof(AnnieSimComponent, fEShieldRemainingSec) == 4u);
static_assert(sizeof(AnnieTibbersComponent) == 36u);
static_assert(offsetof(AnnieTibbersComponent, fRemainingSec) == 8u);
static_assert(offsetof(AnnieTibbersComponent, commandSeq) == 32u);
