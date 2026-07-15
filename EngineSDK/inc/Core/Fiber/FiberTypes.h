#pragma once

#include "WintersTypes.h"

enum class eJobExecutionMode : u8_t
{
	ThreadOnly = 0,
	FiberShell,
	FiberFull,
};

using NativeFiberHandle = void*;

#if defined(_MSC_VER)
#define WINTERS_FIBER_CALL __stdcall
#else
#define WINTERS_FIBER_CALL
#endif
