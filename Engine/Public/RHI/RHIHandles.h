#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

template<typename TTag>
struct RHIHandle
{
    u64_t value = 0;

    bool_t IsValid() const
    {
        return value != 0 && Generation() != 0;
    }

    u32_t Index() const
    {
        return static_cast<u32_t>(value & 0xFFFFFFFFull);
    }

    u32_t Generation() const
    {
        return static_cast<u32_t>((value >> 32) & 0xFFFFFFFFull);
    }

    u64_t ToU64() const
    {
        return value;
    }

    static RHIHandle Make(u32_t index, u32_t generation)
    {
        if (generation == 0)
            generation = 1;

        RHIHandle handle{};
        handle.value = (static_cast<u64_t>(generation) << 32) | static_cast<u64_t>(index);
        return handle;
    }

    static RHIHandle FromU64(u64_t packedValue)
    {
        RHIHandle handle{};
        handle.value = packedValue;
        return handle;
    }
};

struct RHIBufferTag {};
struct RHITextureTag {};
struct RHIShaderTag {};
struct RHISamplerTag {};
struct RHIPipelineTag {};
struct RHIRenderPassTag {};
struct RHIBindGroupTag {};
struct RHIBindGroupLayoutTag {};

using RHIBufferHandle = RHIHandle<RHIBufferTag>;
using RHITextureHandle = RHIHandle<RHITextureTag>;
using RHIShaderHandle = RHIHandle<RHIShaderTag>;
using RHISamplerHandle = RHIHandle<RHISamplerTag>;
using RHIPipelineHandle = RHIHandle<RHIPipelineTag>;
using RHIRenderPassHandle = RHIHandle<RHIRenderPassTag>;
using RHIBindGroupHandle = RHIHandle<RHIBindGroupTag>;
using RHIBindGroupLayoutHandle = RHIHandle<RHIBindGroupLayoutTag>;
