#pragma once

#include "WintersAPI.h"
#include "Platform/PlatformTypes.h"

class IPlatformSurface;

class WINTERS_ENGINE IPlatformWindow
{
public:
    virtual ~IPlatformWindow() = default;

    virtual eWintersPlatform Get_Platform() const = 0;
    virtual ePlatformLifecycleState Get_LifecycleState() const = 0;
    virtual PlatformNativeHandle Get_NativeHandle() const = 0;
    virtual IPlatformSurface* Get_Surface() = 0;
    virtual const IPlatformSurface* Get_Surface() const = 0;
    virtual u32_t Get_Width() const = 0;
    virtual u32_t Get_Height() const = 0;
};
