#pragma once
#include "Defines.h"   // Engine 타입 + STL 자동 (Client 헤더 컨벤션)
#include "WintersMath.h"
#include "ECS/Entity.h"
#include "FX/FxAsset.h"
#include "Renderer/BlendTypes.h"
#include <memory>
#include <string>
#include <utility>

// 월드 공간 3D 메쉬 이펙트 — FBX 기반.
// POD — CWorld Component Store 에 값 복사로 보관.
// FxBillboardComponent (2D 알파 쿼드) 와 공존 — 이쪽은 3D 메쉬 전용.
struct FxMeshComponent
{
    Vec3 vWorldPos = { 0.f, 0.f, 0.f };
    Vec3 vScale = { 1.f, 1.f, 1.f };
    Vec3 vVelocity = { 0.f, 0.f, 0.f };

    EntityID attachTo = NULL_ENTITY;
    Vec3     vAttachOffset = { 0.f, 0.f, 0.f };
    FxAnchorDesc anchor{};
    FxLifecycleDesc lifecycle{};
    bool_t bAnchorResolvedLastFrame = false;
    bool_t bInheritAnchorRotation = false;
    Mat4 matAnchorRotation = Mat4::Identity();

    // 3-axis rotation in radians: pitch(X), yaw(Y), roll(Z).
    Vec3 vRotation = { 0.f, 0.f, 0.f };
    f32_t fWorldYawSpin = 0.f;
    f32_t fWorldYawSpinSpeed = 0.f;

    // 경로 — 정적 리터럴만 허용 (FxBillboardComponent 와 동일 패턴)
    const char* modelPath = nullptr;   // SolutionDir 기준 상대 (예: ".../e_blade.fbx")
    const wchar_t* texturePath = nullptr;
    const wchar_t* erodeTexturePath = nullptr;
    std::shared_ptr<const std::string> modelPathOwner = {};
    std::shared_ptr<const wstring_t> texturePathOwner = {};
    std::shared_ptr<const wstring_t> erodeTexturePathOwner = {};
    FxAssetHandle hAsset{};
    u32_t iEmitterIndex = 0;

    f32_t fLifetime = 3.f;
    f32_t fElapsed = 0.f;
    f32_t fStartDelay = 0.f;

    //Material 
    Vec4 vColor = { 1.f, 1.f, 1.f, 1.f }; //RGBA 
    eBlendPreset blendMode = eBlendPreset::AlphaBlend;
    FxMaterialDesc material{};
    eFxDepthMode depthMode = eFxDepthMode::DepthTestWriteOn;
    bool bMaterialReady = false;

    f32_t fAlphaClip = 0.05f;
    f32_t fFadeIn = 0.f;
    f32_t fFadeOut = 0.f;
    f32_t fUvScrollU = 0.f;
    f32_t fUvScrollV = 0.f;
    f32_t fErodeThreshold = 0.f;
    u32_t iStyleMode = 0; // 0 legacy, 1 LoL brush+rim, 2 toon cell, 3 gradient
    Vec4 vStyleColorA = { 1.f, 1.f, 1.f, 1.f }; // rgb hot/top, a emission intensity
    Vec4 vStyleColorB = { 0.f, 0.f, 0.f, 1.f }; // rgb outline/bottom, a brush contrast
    Vec4 vRimColor = { 1.f, 1.f, 1.f, 0.f };    // rgb rim, a rim intensity
    f32_t fRimPower = 3.f;
    f32_t fCellLow = 0.f;
    f32_t fCellHigh = 0.5f;
    Vec4 vMagicScrollA = { 0.f, 0.5f, 0.1f, 0.05f }; // xy primary, zw secondary
    Vec4 vMagicShape = { 2.5f, 0.06f, 1.0f, 0.035f }; // contrast, edgeWidth, dissolveSpeed, distortStrength
    Vec4 vMagicCore = { 2.0f, 1.0f, 2.0f, 0.f }; // centerPower, coreIntensity, edgeIntensity, reserved
    f32_t fMaterialRandom = 0.f;
    bool bDepthWrite = true;

    // bool 사용 (FxBillboardComponent 와 일관, namespace Engine 밖 헤더의 bool_t 금지 컨벤션 §1.4.1 준수)
    bool bPendingDelete = false;
    bool bBlockableByWindWall = false;

    void SetModelPath(std::string path)
    {
        if (path.empty())
        {
            modelPathOwner.reset();
            modelPath = nullptr;
            return;
        }

        modelPathOwner = std::make_shared<std::string>(std::move(path));
        modelPath = modelPathOwner->c_str();
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

    void SetErodeTexturePath(const tchar_t* path)
    {
        if (!path || path[0] == 0)
        {
            erodeTexturePathOwner.reset();
            erodeTexturePath = nullptr;
            return;
        }
        erodeTexturePathOwner = std::make_shared<wstring_t>(path);
        erodeTexturePath = erodeTexturePathOwner->c_str();
    }

    void SetErodeTexturePath(wstring_t path)
    {
        if (path.empty())
        {
            erodeTexturePathOwner.reset();
            erodeTexturePath = nullptr;
            return;
        }

        erodeTexturePathOwner = std::make_shared<wstring_t>(std::move(path));
        erodeTexturePath = erodeTexturePathOwner->c_str();
    }

    void SetMaterialFromDesc(const FxMaterialDesc& desc, eFxDepthMode inDepthMode)
    {
        material = desc;
        depthMode = inDepthMode;
        bMaterialReady = true;

        vColor = material.vTint;
        fUvScrollU = material.vUVScroll.x;
        fUvScrollV = material.vUVScroll.y;
        fAlphaClip = material.fAlphaClip;
        fErodeThreshold = material.fErodeThreshold;
        iStyleMode = material.iStyleMode;
        vStyleColorA = material.vStyleColorA;
        vStyleColorB = material.vStyleColorB;
        vRimColor = material.vRimColor;
        fRimPower = material.fRimPower;
        fCellLow = material.fCellLow;
        fCellHigh = material.fCellHigh;
        vMagicScrollA = material.vMagicScrollA;
        vMagicShape = material.vMagicShape;
        vMagicCore = material.vMagicCore;
        fMaterialRandom = material.fMaterialRandom;
        bDepthWrite = FxDepthModeWritesDepth(depthMode);
    }

    void RefreshMaterialFromLegacyFields()
    {
        material.vTint = vColor;
        material.vUVScroll = { fUvScrollU, fUvScrollV };
        material.fAlphaClip = fAlphaClip;
        material.fErodeThreshold = fErodeThreshold;
        material.iStyleMode = iStyleMode;
        material.vStyleColorA = vStyleColorA;
        material.vStyleColorB = vStyleColorB;
        material.vRimColor = vRimColor;
        material.fRimPower = fRimPower;
        material.fCellLow = fCellLow;
        material.fCellHigh = fCellHigh;
        material.vMagicScrollA = vMagicScrollA;
        material.vMagicShape = vMagicShape;
        material.vMagicCore = vMagicCore;
        material.fMaterialRandom = fMaterialRandom;
        depthMode = FxDepthModeFromDepthWrite(bDepthWrite);
        bMaterialReady = true;
    }
};
