#pragma once

#include "WintersAPI.h"
#include "RHI/RHIDescriptors.h"

class WINTERS_ENGINE IRHIBindGroupLayout
{
public:
    virtual ~IRHIBindGroupLayout() = default;

    virtual const RHIBindGroupLayoutDesc& GetDesc() const = 0;
};

class WINTERS_ENGINE IRHIBindGroup
{
public:
    virtual ~IRHIBindGroup() = default;

    virtual const RHIBindGroupDesc& GetDesc() const = 0;
    virtual void* GetNativeHandle(eNativeHandleType type) = 0;
};
