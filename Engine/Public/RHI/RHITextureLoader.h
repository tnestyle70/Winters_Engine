#pragma once

#include "WintersAPI.h"
#include "RHI/IRHIDevice.h"

WINTERS_ENGINE RHITextureHandle RHI_CreateTextureFromFile(
    IRHIDevice* pDevice,
    const wchar_t* pFilePath,
    const char* pDebugName = nullptr);
