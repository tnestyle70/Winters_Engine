#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHIHandles.h"
#include "RHI/RHITypes.h"

class WINTERS_ENGINE IRHISwapChain
{
public:
    virtual ~IRHISwapChain() = default;

    virtual void Present(bool_t bVSync) = 0;
    virtual u32_t GetCurrentBackBufferIndex() const = 0;
    virtual RHITextureHandle GetCurrentBackBuffer() const = 0;
    virtual void Resize(u32_t width, u32_t height) = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) const = 0;
};
