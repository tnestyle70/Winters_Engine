#pragma once

#include "WintersAPI.h"
#include "Platform/PlatformTypes.h"
#include "RHI/RHISurface.h"

class WINTERS_ENGINE IPlatformSurface
{
public:
    virtual ~IPlatformSurface() = default;

    virtual eWintersPlatform Get_Platform() const = 0;
    virtual ePlatformLifecycleState Get_LifecycleState() const = 0;
    virtual PlatformNativeHandle Get_NativeHandle() const = 0;
    virtual RHISurfaceDesc Get_RHISurfaceDesc() const = 0;
};
