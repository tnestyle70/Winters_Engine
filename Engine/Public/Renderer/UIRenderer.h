#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"

#include <memory>

enum class eUISamplerMode : u8_t
{
    LinearClamp,
    PointClamp,
};

class CUIRenderer final
{
public:
    ~CUIRenderer();

    CUIRenderer(const CUIRenderer&) = delete;
    CUIRenderer& operator=(const CUIRenderer&) = delete;

    static std::unique_ptr<CUIRenderer> Create(IRHIDevice* pDevice);

    bool_t IsReady() const;
    void Begin(u32_t iScreenWidth, u32_t iScreenHeight,
        eUISamplerMode eSamplerMode = eUISamplerMode::LinearClamp);
    void End();

    void DrawImage(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor);

    void DrawImageCircle(void* pTextureSRV,
        f32_t fX, f32_t fY, f32_t fW, f32_t fH,
        const Vec4& vUVRect,
        const Vec4& vColor,
        u32_t iSegmentCount = 48);

private:
    CUIRenderer();

    bool_t Initialize(IRHIDevice* pDevice);
    void Shutdown();

    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
