#pragma once

#include "WintersTypes.h"
#include "Renderer/LightData.h"
#include "FX/FxMaterialDesc.h"

#include <DirectXMath.h>

struct CBPerFrame
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT3 cameraWorld;
    f32_t _padding0 = 0.f;
    DirectX::XMFLOAT3 lightDirWorld;
    f32_t lightIntensity = 1.f;
    DirectX::XMFLOAT3 lightColor;
    u32_t pointLightCount = 0;
    Engine::PointLightData pointLights[Engine::kMaxForwardPointLights] = {};
    DirectX::XMFLOAT2 screenSize = {};
    DirectX::XMFLOAT2 _padding1 = {};
};
static_assert(sizeof(CBPerFrame) % 16 == 0);

struct CBPerObject
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 worldInvTranspose;
    DirectX::XMFLOAT4 materialOverrideColor = { 1.f, 1.f, 1.f, 1.f };
    DirectX::XMFLOAT4 vMaterialOverrideParams = { 0.f, 0.f, 0.f, 0.f };
};
static_assert(sizeof(CBPerObject) % 16 == 0);

struct CBFxParams
{
    DirectX::XMFLOAT4 vTint;
    DirectX::XMFLOAT4 vUVRect;
    DirectX::XMFLOAT2 vUVScroll;
    f32_t fAlphaClip;
    f32_t fErodeThreshold;
    DirectX::XMFLOAT4 vStyleColorA;
    DirectX::XMFLOAT4 vStyleColorB;
    DirectX::XMFLOAT4 vRimColor;
    DirectX::XMFLOAT4 vStyleParams;
    DirectX::XMFLOAT4 vTimeParams;
    DirectX::XMFLOAT4 vMagicScrollA;
    DirectX::XMFLOAT4 vMagicShape;
    DirectX::XMFLOAT4 vMagicCore;
};
static_assert(sizeof(CBFxParams) % 16 == 0);

inline CBFxParams MakeFxParamsFromMaterial(
    const FxMaterialDesc& material,
    const Vec4& vTint,
    const Vec4& vUVRect,
    const Vec2& vUVScroll,
    f32_t fElapsed,
    f32_t fNormalizedAge)
{
    CBFxParams params{};
    params.vTint = vTint.ToXMFLOAT4();
    params.vUVRect = vUVRect.ToXMFLOAT4();
    params.vUVScroll = { vUVScroll.x, vUVScroll.y };
    params.fAlphaClip = material.fAlphaClip;
    params.fErodeThreshold = material.fErodeThreshold;
    params.vStyleColorA = material.vStyleColorA.ToXMFLOAT4();
    params.vStyleColorB = material.vStyleColorB.ToXMFLOAT4();
    params.vRimColor = material.vRimColor.ToXMFLOAT4();
    params.vStyleParams = {
        static_cast<f32_t>(material.iStyleMode),
        material.fRimPower,
        material.fCellLow,
        material.fCellHigh
    };
    params.vTimeParams = {
        fElapsed,
        fNormalizedAge,
        material.fMaterialRandom,
        0.f
    };
    params.vMagicScrollA = material.vMagicScrollA.ToXMFLOAT4();
    params.vMagicShape = material.vMagicShape.ToXMFLOAT4();
    params.vMagicCore = material.vMagicCore.ToXMFLOAT4();
    return params;
}
