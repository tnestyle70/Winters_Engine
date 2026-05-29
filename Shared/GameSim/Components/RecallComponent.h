#pragma once

#include "WintersTypes.h"

#include <type_traits>

struct RecallComponent
{
	f32_t fRemainingSec = 0.f;
	f32_t fDurationSec = 2.f;
	bool_t bActive = false;
};

static_assert(std::is_trivially_copyable_v<RecallComponent>,
	"RecallComponent must remain POD for deterministic server simulation.");