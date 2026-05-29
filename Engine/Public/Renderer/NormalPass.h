#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"
#include <memory>

class DX11Shader;
class DX11Pipeline;

namespace Engine
{
    class WINTERS_ENGINE CNormalPass final
    {
    public:
        ~CNormalPass();
        CNormalPass(const CNormalPass&) = delete;
        CNormalPass& operator=(const CNormalPass&) = delete;
        CNormalPass(CNormalPass&&) noexcept = default;
        CNormalPass& operator=(CNormalPass&&) noexcept = default;

        static std::unique_ptr<CNormalPass> Create(IRHIDevice* pDevice, u32_t width, u32_t height);

        void Begin(IRHIDevice* pDevice);
        void End(IRHIDevice* pDevice);

        void* GetDepthSRVNative() const;
        void* GetNormalSRVNative() const;

        DX11Shader* GetStaticShader() const;
        DX11Pipeline* GetStaticPipeline() const;
        DX11Shader* GetSkinnedShader() const;
        DX11Pipeline* GetSkinnedPipeline() const;

    private:
        CNormalPass();
        bool_t Initialize(IRHIDevice* pDevice, u32_t width, u32_t height);

        struct Impl;
        std::unique_ptr<Impl> m_pImpl;
    };
}
