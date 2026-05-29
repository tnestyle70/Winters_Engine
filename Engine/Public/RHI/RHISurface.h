#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

enum class eRHIPlatformSurfaceType : u32_t
{
    Unknown = 0,
    Win32HWND,
    AndroidNativeWindow,
    IOSMetalLayer,
    XboxNative,
    PS5Native,
};

enum class eRHISurfaceLifecycleState : u32_t
{
    Unknown = 0,
    Active,
    Suspended,
    Lost,
    Resized,
    Destroyed,
};

struct WINTERS_ENGINE RHISurfaceDesc
{
    eRHIPlatformSurfaceType type = eRHIPlatformSurfaceType::Unknown;
    void* nativeHandle = nullptr;
    void* nativeDisplay = nullptr;
    u32_t width = 0;
    u32_t height = 0;
    bool_t vsync = true;
    bool_t fullscreen = false;
    eRHISurfaceLifecycleState lifecycleState = eRHISurfaceLifecycleState::Active;
};

inline RHISurfaceDesc RHI_ToSurfaceDesc(
    const RHIWindowHandle& window,
    eRHIPlatformSurfaceType type = eRHIPlatformSurfaceType::Win32HWND)
{
    RHISurfaceDesc desc{};
    desc.type = type;
    desc.nativeHandle = window.nativeWindow;
    desc.width = window.width;
    desc.height = window.height;
    desc.vsync = window.vsync;
    desc.fullscreen = window.fullscreen;
    desc.lifecycleState = eRHISurfaceLifecycleState::Active;
    return desc;
}

inline RHIWindowHandle RHI_ToWindowHandle(const RHISurfaceDesc& surface)
{
    RHIWindowHandle window{};
    window.nativeWindow = surface.nativeHandle;
    window.width = surface.width;
    window.height = surface.height;
    window.vsync = surface.vsync;
    window.fullscreen = surface.fullscreen;
    return window;
}
