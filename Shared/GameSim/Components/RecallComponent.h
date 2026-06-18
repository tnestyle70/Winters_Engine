#pragma once

#include "WintersTypes.h"

#include <type_traits>

inline constexpr f32_t kRecallDurationSec = 2.f;

struct RecallComponent
{
	f32_t fRemainingSec = 0.f;
	f32_t fDurationSec = kRecallDurationSec;
	bool_t bActive = false;
};

static_assert(std::is_trivially_copyable_v<RecallComponent>,
	"RecallComponent must remain POD for deterministic server simulation.");
