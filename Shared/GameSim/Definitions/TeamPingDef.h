#pragma once

#include "WintersTypes.h"

enum class eTeamPingKind : u8_t
{
	None = 0,
	OnMyWay = 1,
	Danger = 2,
	Assist = 3,
	Missing = 4,
};

inline constexpr f32_t kTeamPingDangerRadius = 12.f;
