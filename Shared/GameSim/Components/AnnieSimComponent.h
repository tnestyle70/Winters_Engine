#pragma once

#include "ECS/Components/GameplayComponents.h"
#include "ECS/Entity.h"
#include "WintersTypes.h"

#include <type_traits>

struct AnnieSimComponent
{
    u8_t stunStacks = 0;
    bool_t bStunReady = false;
    f32_t fEShieldRemainingSec = 0.f;
    f32_t fEShieldAmount = 0.f;
    f32_t fEShieldMaxAmount = 0.f;
    EntityID tibbersEntity = NULL_ENTITY;
};

struct AnnieTibbersComponent
{
    EntityID owner = NULL_ENTITY;
    eTeam ownerTeam = eTeam::Neutral;
    f32_t fRemainingSec = 45.f;
};

static_assert(std::is_trivially_copyable_v<AnnieSimComponent>);
static_assert(std::is_trivially_copyable_v<AnnieTibbersComponent>);
