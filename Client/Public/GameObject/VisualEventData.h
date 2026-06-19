#pragma once

#include "WintersTypes.h"

#include <cstdint>

enum class eVisualEventKind : uint8_t
{
    None = 0,
    Cast = 1,
    Recovery = 2,
    KeySwap = 3,
    CastAccepted = 4,
};

struct VisualEventData
{
    u8_t kind = static_cast<u8_t>(eVisualEventKind::None);
    f32_t frame = 0.f;
    u32_t hookId = 0;
};

using eChampionVisualEventKind = eVisualEventKind;
using ChampionActionVisualEventData = VisualEventData;
