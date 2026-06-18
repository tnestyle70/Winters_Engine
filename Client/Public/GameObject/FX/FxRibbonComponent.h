#pragma once
#include "Defines.h"
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "Renderer/BlendTypes.h"
#include <array>
#include <memory>
#include <string>
#include <utility>

constexpr u32_t FX_RIBBON_MAX_POINTS = 32;

struct FxRibbonComponent
{
    std::array<Vec3, FX_RIBBON_MAX_POINTS> points{};
    std::array<f32_t, FX_RIBBON_MAX_POINTS> pointAges{};
    u32_t iPointCount = 0;
    EntityID attachTo = NULL_ENTITY;
    Vec3 vStartOffset = { 0.f, 0.f, 0.f };
    Vec3 vEndOffset = { 0.f, 0.f, 1.f };
    FxAnchorDesc anchor{};
    FxLifecycleDesc lifecycle{};
    bool_t bAnchorResolvedLastFrame = false;
    Vec3 vVelocity = { 0.f, 0.f, 0.f };

    const wchar_t* texturePath = nullptr;
    std::shared_ptr<const wstring_t> texturePathOwner = {};
    FxAssetHandle hAsset{};
    u32_t iEmitterIndex = 0;

    f32_t fWidth = 0.35f;
    f32_t fLifetime = 1.f;
    f32_t fElapsed = 0.f;
    f32_t fStartDelay = 0.f;
    f32_t fFadeIn = 0.f;
    f32_t fFadeOut = 0.f;
    f32_t fUvScrollU = 0.f;
    f32_t fUvScrollV = 0.f;
    f32_t fTrailSampleInterval = 0.025f;
    f32_t fTrailSampleAccumulator = 0.f;
    f32_t fTrailHeadWidthScale = 1.f;
    f32_t fTrailTailWidthScale = 1.f;
    f32_t fTrailHeadAlphaScale = 1.f;
    f32_t fTrailTailAlphaScale = 1.f;
    f32_t fTrailJitterAmplitude = 0.f;
    f32_t fTrailJitterFrequency = 0.f;
    f32_t fTrailJitterSeed = 0.f;
    f32_t fAlphaClip = 0.05f;
    f32_t fErodeThreshold = 0.f;
    FxMaterialDesc material{};
    eFxDepthMode depthMode = eFxDepthMode::DepthTestWriteOn;
    bool_t bMaterialReady = false;
    Vec4 vColor = { 1.f, 1.f, 1.f, 1.f };
    eBlendPreset blendMode = eBlendPreset::Additive;

    bool bPendingDelete = false;
    bool bBlockableByWindWall = false;
    bool bHistoryTrail = false;

    void SetPoint(u32_t index, const Vec3& v)
    {
        if (index >= FX_RIBBON_MAX_POINTS)
            return;

        points[index] = v;
        if (iPointCount <= index)
            iPointCount = index + 1;
    }

    void SetMaterialFromDesc(const FxMaterialDesc& desc, eFxDepthMode mode)
    {
        material = desc;
        depthMode = mode;

        vColor = material.vTint;
        fUvScrollU = material.vUVScroll.x;
        fUvScrollV = material.vUVScroll.y;
        fAlphaClip = material.fAlphaClip;
        fErodeThreshold = material.fErodeThreshold;
        bMaterialReady = true;
    }

    void RefreshMaterialFromLegacyFields()
    {
        FxSetMaterialDrawFields(
            material,
            vColor,
            { 0.f, 0.f, 1.f, 1.f },
            { fUvScrollU, fUvScrollV },
            fAlphaClip,
            fErodeThreshold);
        bMaterialReady = true;
    }

    void SetTexturePath(const tchar_t* path)
    {
        if (!path || path[0] == 0)
        {
            texturePathOwner.reset();
            texturePath = nullptr;
            return;
        }

        texturePathOwner = std::make_shared<wstring_t>(path);
        texturePath = texturePathOwner->c_str();
    }

    void SetTexturePath(wstring_t path)
    {
        if (path.empty())
        {
            texturePathOwner.reset();
            texturePath = nullptr;
            return;
        }

        texturePathOwner = std::make_shared<wstring_t>(std::move(path));
        texturePath = texturePathOwner->c_str();
    }
};
