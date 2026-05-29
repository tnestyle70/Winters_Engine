#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/RHITypes.h"

enum class eRHIFeatureTier : u32_t
{
    LegacyDX11 = 0,
    ExplicitDesktop,
    MobileTiled,
    Console,
};

struct WINTERS_ENGINE RHICapabilities
{
    eRHIBackend backend = eRHIBackend::DX11;
    eRHIFeatureTier featureTier = eRHIFeatureTier::LegacyDX11;

    bool_t supportsCompute = false;
    bool_t supportsAsyncCompute = false;
    bool_t supportsRayTracing = false;
    bool_t supportsBindless = false;
    bool_t supportsMeshShader = false;
    bool_t supportsVariableRateShading = false;
    bool_t supportsUnifiedMemory = false;
    bool_t supportsTimelineSemaphore = false;

    bool_t prefersRenderPassLoadStore = false;
    bool_t isTileBasedGPU = false;
    bool_t requiresExplicitResourceStates = false;

    u32_t maxFramesInFlight = 2;
    u32_t maxColorAttachments = 8;
    u32_t constantBufferAlignment = 256;
    u32_t textureUploadAlignment = 256;
    u32_t maxSampledTexturesPerStage = 16;
    u32_t maxSamplersPerStage = 16;
};

inline RHICapabilities RHI_MakeDefaultCapabilities(eRHIBackend backend)
{
    RHICapabilities caps{};
    caps.backend = backend;

    switch (backend)
    {
    case eRHIBackend::DX11:
        caps.featureTier = eRHIFeatureTier::LegacyDX11;
        caps.supportsCompute = true;
        caps.maxFramesInFlight = 1;
        caps.maxSampledTexturesPerStage = 16;
        caps.maxSamplersPerStage = 16;
        break;

    case eRHIBackend::DX12:
    case eRHIBackend::Vulkan:
        caps.featureTier = eRHIFeatureTier::ExplicitDesktop;
        caps.supportsCompute = true;
        caps.supportsAsyncCompute = true;
        caps.supportsBindless = true;
        caps.requiresExplicitResourceStates = true;
        caps.prefersRenderPassLoadStore = true;
        caps.maxFramesInFlight = 3;
        caps.maxSampledTexturesPerStage = 128;
        caps.maxSamplersPerStage = 32;
        break;

    case eRHIBackend::Metal:
        caps.featureTier = eRHIFeatureTier::MobileTiled;
        caps.supportsCompute = true;
        caps.supportsAsyncCompute = true;
        caps.supportsUnifiedMemory = true;
        caps.requiresExplicitResourceStates = true;
        caps.prefersRenderPassLoadStore = true;
        caps.isTileBasedGPU = true;
        caps.maxFramesInFlight = 3;
        caps.maxSampledTexturesPerStage = 96;
        caps.maxSamplersPerStage = 16;
        break;

    case eRHIBackend::Xbox:
    case eRHIBackend::PS5:
        caps.featureTier = eRHIFeatureTier::Console;
        caps.supportsCompute = true;
        caps.supportsAsyncCompute = true;
        caps.supportsBindless = true;
        caps.supportsVariableRateShading = true;
        caps.requiresExplicitResourceStates = true;
        caps.prefersRenderPassLoadStore = true;
        caps.maxFramesInFlight = 3;
        caps.maxSampledTexturesPerStage = 128;
        caps.maxSamplersPerStage = 32;
        break;

    default:
        break;
    }

    return caps;
}
