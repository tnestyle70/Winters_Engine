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
    bool_t supportsResourceTransitions = false;

    bool_t prefersRenderPassLoadStore = false;
    bool_t isTileBasedGPU = false;
    bool_t apiRequiresExplicitResourceStates = false;

    u32_t frameResourceSlotCount = 0;
    u32_t maxColorAttachments = 0;
    u32_t constantBufferAlignment = 0;
    u32_t textureUploadAlignment = 0;
    u32_t maxSampledTexturesPerStage = 0;
    u32_t maxSamplersPerStage = 0;
};
