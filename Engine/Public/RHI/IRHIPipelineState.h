#pragma once

#include "WintersAPI.h"
#include "RHI/RHIDescriptors.h"

class WINTERS_ENGINE IRHIPipelineState
{
public:
    virtual ~IRHIPipelineState() = default;

    virtual const RHIPipelineDesc& GetDesc() const = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) = 0;
};
