#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

class IRHICommandList;

class WINTERS_ENGINE IRHIQueue
{
public:
    virtual ~IRHIQueue() = default;

    virtual void Execute(IRHICommandList* const* ppCommandLists, u32_t commandListCount) = 0;
    virtual void Signal(u32_t frameIndex) = 0;
    virtual void WaitForFrame(u32_t frameIndex) = 0;
    virtual void Flush() = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) const = 0;
};
