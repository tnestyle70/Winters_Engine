#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"
#include <memory>

namespace Engine
{
    class WINTERS_ENGINE CSSAOPass final
    {
    public:
        ~CSSAOPass();
        CSSAOPass(const CSSAOPass&) = delete;
        CSSAOPass& operator=(const CSSAOPass&) = delete;
        CSSAOPass(CSSAOPass&&) noexcept = default;
        CSSAOPass& operator=(CSSAOPass&&) noexcept = default;

        static std::unique_ptr<CSSAOPass> Create(IRHIDevice* pDevice, u32_t width, u32_t height);

        void Execute(IRHIDevice* pDevice,
            void* pDepthSRVNative,
            void* pNormalSRVNative,
            const Mat4& matViewProj);

        void* GetOutputSRVNative() const;

        bool_t GetEnabled() const { return m_bEnabled; }
        void SetEnabled(bool_t bEnabled) { m_bEnabled = bEnabled; }

        f32_t GetRadius() const { return m_fRadius; }
        void SetRadius(f32_t value) { m_fRadius = (value < 0.05f) ? 0.05f : value; }

        f32_t GetIntensity() const { return m_fIntensity; }
        void SetIntensity(f32_t value) { m_fIntensity = (value < 0.1f) ? 0.1f : value; }

        f32_t GetThicknessHeuristic() const { return m_fThicknessHeuristic; }
        void SetThicknessHeuristic(f32_t value)
        {
            m_fThicknessHeuristic = (value < 0.0f) ? 0.0f : ((value > 0.5f) ? 0.5f : value);
        }

    private:
        CSSAOPass();
        bool_t Initialize(IRHIDevice* pDevice, u32_t width, u32_t height);

        struct Impl;
        std::unique_ptr<Impl> m_pImpl;

        bool_t m_bEnabled = true;
        f32_t m_fRadius = 1.25f;
        f32_t m_fIntensity = 1.5f;
        f32_t m_fThicknessHeuristic = 0.05f;
    };
}
