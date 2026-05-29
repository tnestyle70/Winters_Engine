#pragma once

#include "Engine_Defines.h"

struct EzrealStateComponent
{
	f32_t fTeleportDistance = 4.75f;
	f32_t fGlobalLifetime = 6.0f;
	f32_t fGlobalSpeed = 25.f;
	uint8_t baCounter = 0;
};
