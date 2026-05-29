#pragma once

#include "WintersAPI.h"
#include "RHI/RHIDescriptors.h"

class WINTERS_ENGINE IRHIRenderPass
{
public:
    virtual ~IRHIRenderPass() = default;

    virtual const RHIRenderPassDesc& GetDesc() const = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) = 0;
};
