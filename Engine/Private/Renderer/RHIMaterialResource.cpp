#include "WintersPCH.h"

#include "Renderer/RHIMaterialResource.h"

#include "RHI/IRHIDevice.h"

#include <vector>

namespace
{
    std::vector<u32_t> MakeCheckerTexels(u32_t size, u32_t colorA, u32_t colorB)
    {
        std::vector<u32_t> texels(static_cast<size_t>(size) * size);

        for (u32_t y = 0; y < size; ++y)
        {
            for (u32_t x = 0; x < size; ++x)
            {
                const bool_t useA = (((x / 8u) + (y / 8u)) % 2u) == 0u;
                texels[static_cast<size_t>(y) * size + x] = useA ? colorA : colorB;
            }
        }

        return texels;
    }
}

CRHIMaterialResource::~CRHIMaterialResource()
{
    Shutdown();
}

std::unique_ptr<CRHIMaterialResource> CRHIMaterialResource::CreateCheckerboard(
    IRHIDevice* pDevice,
    u32_t size,
    u32_t colorA,
    u32_t colorB,
    const char* debugName)
{
    if (!pDevice || size == 0)
        return nullptr;

    auto pResource = std::unique_ptr<CRHIMaterialResource>(new CRHIMaterialResource());
    if (!pResource)
        return nullptr;

    pResource->m_pDevice = pDevice;

    const std::vector<u32_t> texels = MakeCheckerTexels(size, colorA, colorB);

    RHITextureDesc textureDesc{};
    textureDesc.width = size;
    textureDesc.height = size;
    textureDesc.mipLevels = 1;
    textureDesc.format = eRHIFormat::R8G8B8A8_UNorm;
    textureDesc.usageFlags = static_cast<u32_t>(eRHITextureUsage::ShaderResource);
    textureDesc.debugName = debugName ? debugName : "RHIMaterial.CheckerTexture";
    pResource->m_hAlbedoTexture = pDevice->CreateTexture(
        textureDesc,
        texels.data(),
        size * static_cast<u32_t>(sizeof(u32_t)));

    RHISamplerDesc samplerDesc{};
    samplerDesc.filter = eRHIFilter::Linear;
    samplerDesc.addressU = eRHIAddressMode::Wrap;
    samplerDesc.addressV = eRHIAddressMode::Wrap;
    samplerDesc.addressW = eRHIAddressMode::Wrap;
    samplerDesc.debugName = debugName ? debugName : "RHIMaterial.Sampler";
    pResource->m_hSampler = pDevice->CreateSampler(samplerDesc);

    if (!pResource->IsReady())
    {
        pResource->Shutdown();
        return nullptr;
    }

    return pResource;
}

bool_t CRHIMaterialResource::IsReady() const
{
    return m_pDevice && m_hAlbedoTexture.IsValid() && m_hSampler.IsValid();
}

void CRHIMaterialResource::Shutdown()
{
    if (!m_pDevice)
        return;

    if (m_hSampler.IsValid())
        m_pDevice->DestroySampler(m_hSampler);
    if (m_hAlbedoTexture.IsValid())
        m_pDevice->DestroyTexture(m_hAlbedoTexture);

    m_hSampler = {};
    m_hAlbedoTexture = {};
    m_pDevice = nullptr;
}
