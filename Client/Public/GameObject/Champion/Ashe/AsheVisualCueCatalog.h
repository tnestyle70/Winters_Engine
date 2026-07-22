#pragma once

#include "Defines.h"

namespace Ashe::VisualCue
{
    constexpr const char* kBAArrow = "Ashe.BA.Arrow";
    constexpr const char* kBAHit = "Ashe.BA.Hit";
    constexpr const char* kWCast = "Ashe.W.Cast";
    constexpr const char* kEHawkshot = "Ashe.E.Hawkshot";
    constexpr const char* kRCharge = "Ashe.R.Cast";

    constexpr f32_t kBAArrowSpeed = 18.f;
    constexpr f32_t kBAArrowLifetime = 0.4f;
    constexpr f32_t kWCastLifetime = 0.45f;
    constexpr f32_t kEHawkshotTravelSeconds = 0.70f;
    constexpr f32_t kRChargeLifetime = 0.45f;
}
