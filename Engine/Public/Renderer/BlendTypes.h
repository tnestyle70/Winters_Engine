#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

enum class eBlendPreset : uint8_t
{
	Opaque,
	AlphaBlend,
	PremultipliedAlpha,
	Additive,
	Count,
};
