#pragma once
#include "Defines.h"

namespace Winters::DevSmoke
{
	bool_t IsEnabled();
	void Log(const char* pFormat, ...);
}
