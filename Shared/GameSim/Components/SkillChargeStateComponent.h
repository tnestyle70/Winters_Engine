#pragma once

#include "Shared/GameSim/Definitions/LoLMatchContext.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

struct SkillChargeStateComponent
{
    bool_t bActive = false;
    u8_t localSlot = 0u;
    u8_t sourceSlot = 0u;
    eChampion sourceChampion = eChampion::NONE;
    u64_t startTick = 0u;
    u64_t maxReleaseTick = 0u;
    Vec3 aimDirection{};
    f32_t chargeRatio = 0.f;
};

static_assert(std::is_trivially_copyable_v<SkillChargeStateComponent>);
