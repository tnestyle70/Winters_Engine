#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "RHI/IRHIDevice.h"
#include "Renderer/BlendTypes.h"
#include "Renderer/FxShaderConstants.h"
#include "FX/FxDepthMode.h"
#include <memory>

class DX11Shader;
class DX11Pipeline;
class CBlendStateCache;
namespace Engine { class CTexture; }

class WINTERS_ENGINE CPlaneRenderer final
{
private:
    CPlaneRenderer();

public:
    ~CPlaneRenderer();
    CPlaneRenderer(const CPlaneRenderer&) = delete;
    CPlaneRenderer& operator=(const CPlaneRenderer&) = delete;

    static std::unique_ptr<CPlaneRenderer> Create(
        IRHIDevice* pDevice,
        DX11Shader* pMeshShader,
        DX11Pipeline* pMeshPipeline);

    void SetTexture(Engine::CTexture* pTex);
    void SetWorld(const Mat4& world);
    void SetBlendCache(CBlendStateCache* pCache,
        eBlendPreset ePreset = eBlendPreset::AlphaBlend);
    void SetDepthMode(eFxDepthMode eMode);

    void Render(IRHIDevice* pDevice, const Mat4& matViewProj);
    bool_t BeginBatch(IRHIDevice* pDevice, const Mat4& matViewProj);
    void RenderBatched();
    void EndBatch();

public:
    void SetFxParams(const CBFxParams& params);
    void SetFxParams(const Vec4& vTint,
        const Vec4& vUVRect,
        const Vec2& vUVScroll,
        f32_t fAlphaClip,
        f32_t fErodeThreshold);
    void ResetFxParams();

private:
    HRESULT Initialize(IRHIDevice* pDevice,
        DX11Shader* pMeshShader,
        DX11Pipeline* pMeshPipeline);

    struct Impl;
    std::unique_ptr<Impl> m_pImpl;

    Engine::CTexture* m_pTexture = nullptr;
    CBlendStateCache* m_pBlendCache = nullptr;
    eBlendPreset m_eBlend = eBlendPreset::AlphaBlend;
    eFxDepthMode m_eDepthMode = eFxDepthMode::DepthTestWriteOff;

    Vec4 m_vFxTint = { 1.f, 1.f, 1.f, 1.f };
    Vec4 m_vFxUVRect = { 0.f, 0.f, 1.f, 1.f };
    Vec2 m_vFxUVScroll = { 0.f, 0.f };
    f32_t m_fFxAlphaClip = 0.05f;
    f32_t m_fFxErodeThreshold = 0.f;
    CBFxParams m_FxParams{};
    Mat4 m_World{};
};
