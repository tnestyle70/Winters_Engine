#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"
#include "Renderer/BlendTypes.h"
#include "Renderer/FxShaderConstants.h"

#include <memory>

class WINTERS_ENGINE CRHIFxSpriteRenderer final
{
public:
    ~CRHIFxSpriteRenderer();

    CRHIFxSpriteRenderer(const CRHIFxSpriteRenderer&) = delete;
    CRHIFxSpriteRenderer& operator=(const CRHIFxSpriteRenderer&) = delete;

    static std::unique_ptr<CRHIFxSpriteRenderer> Create(IRHIDevice* pDevice);
    void Draw(IRHIDevice* pDevice,
        RHITextureHandle hTexture,
        const Mat4& matWorld,
        const Mat4& matViewProjection,
        const CBFxParams& fxParams,
        eBlendPreset eBlend);

    void Draw(IRHIDevice* pDevice,
              RHITextureHandle hTexture,
              const Mat4& matWorld,
              const Mat4& matViewProjection,
              const Vec4& vTint,
              const Vec4& vUVRect,
              const Vec2& vUVScroll,
              f32_t fAlphaClip,
              f32_t fErodeThreshold,
              eBlendPreset eBlend);

private:
    CRHIFxSpriteRenderer();
    bool_t Initialize(IRHIDevice* pDevice);
    void Shutdown();

    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
