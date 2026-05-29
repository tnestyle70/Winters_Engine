#pragma once

#include "WintersMath.h"
#include "WintersTypes.h"

#include <type_traits>

struct KalistaPassiveDashComponent
{
    bool_t bPending = false;
    bool_t bActive = false;
    bool_t bHasQueuedMove = false;
    u8_t slot = 0;
    u8_t _pad0 = 0;
    u32_t sourceActionSeq = 0;
    u64_t triggerTick = 0;
    Vec3 direction{ 0.f, 0.f, 1.f };
    Vec3 queuedMoveTarget{};
    Vec3 startPos{};
    Vec3 endPos{};
    f32_t distance = 2.f;
    f32_t durationSec = 1.2f;
    f32_t elapsedSec = 0.f;
};

static_assert(std::is_trivially_copyable_v<KalistaPassiveDashComponent>,
    "KalistaPassiveDashComponent must be trivially_copyable for sim determinism.");
