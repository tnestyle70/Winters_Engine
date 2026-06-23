#pragma once

#include "WintersAPI.h"
#include "WintersCore.h"
#include "RHI/RHIDescriptors.h"

#include <memory>

class IRHIDevice;

class WINTERS_ENGINE CRHIMaterialResource final
{
public:
    ~CRHIMaterialResource();

    CRHIMaterialResource(const CRHIMaterialResource&) = delete;
    CRHIMaterialResource& operator=(const CRHIMaterialResource&) = delete;

    static std::unique_ptr<CRHIMaterialResource> CreateCheckerboard(
        IRHIDevice* pDevice,
        u32_t size,
        u32_t colorA,
        u32_t colorB,
        const char* debugName = nullptr);

    RHITextureHandle GetAlbedoTexture() const { return m_hAlbedoTexture; }
    RHISamplerHandle GetSampler() const { return m_hSampler; }
    bool_t IsReady() const;

    void Shutdown();

private:
    CRHIMaterialResource() = default;

private:
    IRHIDevice* m_pDevice = nullptr;
    RHITextureHandle m_hAlbedoTexture{};
    RHISamplerHandle m_hSampler{};
};
