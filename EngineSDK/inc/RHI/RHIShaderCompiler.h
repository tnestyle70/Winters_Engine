#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

#include <string>
#include <vector>

WINTERS_ENGINE bool RHI_CompileHlslShader(
    const char* pSource,
    const char* pEntryPoint,
    const char* pTarget,
    std::vector<u8_t>& outBytecode,
    std::string* pOutErrors = nullptr);
