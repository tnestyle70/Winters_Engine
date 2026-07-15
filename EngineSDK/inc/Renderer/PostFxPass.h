#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"

#include <memory>

namespace Engine
{
    struct WINTERS_ENGINE PostFxParams
    {
        f32_t fGamma = 1.f;
        f32_t fSaturation = 1.f;
        f32_t fGradeStrength = 0.f;
        Vec3 vTint{ 1.f, 1.f, 1.f };

        f32_t fVignetteStrength = 0.f;
        f32_t fVignetteInner = 0.35f;
        f32_t fVignetteOuter = 0.75f;

        bool_t bBloomEnabled = false;
        f32_t fBloomThreshold = 0.85f;
        f32_t fBloomIntensity = 0.6f;
        f32_t fBloomSoftKnee = 0.1f;
    };

    class WINTERS_ENGINE CPostFxPass final
    {
    public:
        ~CPostFxPass();

        CPostFxPass(const CPostFxPass&) = delete;
        CPostFxPass& operator=(const CPostFxPass&) = delete;
        CPostFxPass(CPostFxPass&&) noexcept = default;
        CPostFxPass& operator=(CPostFxPass&&) noexcept = default;

        static std::unique_ptr<CPostFxPass> Create(
            IRHIDevice* pDevice,
            u32_t width,
            u32_t height);

        void Execute(IRHIDevice* pDevice);

        bool_t GetEnabled() const { return m_bEnabled; }
        void SetEnabled(bool_t bEnabled) { m_bEnabled = bEnabled; }

        PostFxParams GetParams() const { return m_Params; }
        void SetParams(const PostFxParams& params);

    private:
        CPostFxPass();
        bool_t Initialize(IRHIDevice* pDevice, u32_t width, u32_t height);

        struct Impl;
        std::unique_ptr<Impl> m_pImpl;

        bool_t m_bEnabled = false;
        PostFxParams m_Params{};
    };
}
