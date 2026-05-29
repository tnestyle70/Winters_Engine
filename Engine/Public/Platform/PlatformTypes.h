#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

enum class eWintersPlatform : u32_t
{
    Unknown = 0,
    Windows,
    Android,
    IOS,
    Xbox,
    PS5,
};

enum class ePlatformLifecycleState : u32_t
{
    Unknown = 0,
    Created,
    Active,
    Suspended,
    SurfaceLost,
    Destroyed,
};

struct WINTERS_ENGINE PlatformWindowDesc
{
    const tchar_t* title = nullptr;
    u32_t width = 1280;
    u32_t height = 720;
    bool_t fullscreen = false;
};

struct WINTERS_ENGINE PlatformNativeHandle
{
    eWintersPlatform platform = eWintersPlatform::Unknown;
    void* window = nullptr;
    void* display = nullptr;
    void* module = nullptr;
};
