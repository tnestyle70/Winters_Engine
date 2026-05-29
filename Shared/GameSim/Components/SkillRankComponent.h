#pragma once

#include "WintersTypes.h"

struct SkillRankComponent
{
    static constexpr u8_t kSlotCount = 5;

    u8_t ranks[kSlotCount] = {};
    u8_t pointsAvailable = 0;
};
