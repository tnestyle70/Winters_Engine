#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "FX/FxDepthMode.h"
#include "FX/FxMaterialDesc.h"
#include "FX/ParameterMap.h"
#include "RHI/RHIHandles.h"
#include "Renderer/BlendTypes.h"

#include <string>
#include <unordered_map>
#include <vector>

struct FxAssetTag {};
using FxAssetHandle = RHIHandle<FxAssetTag>;

enum class eFxRenderType : u8_t
{
    Billboard,
    Ribbon,
    Beam,
    GroundDecal,
    MeshParticle,
    ShockwaveRing,
};

enum class eFxAnchorType : u8_t
{
    Entity,
    World,
    Bone,
    Socket,
    Submesh,
    TargetSegment,
};

enum class eFxAnchorFallback : u8_t
{
    None,
    Entity,
    WorldPosition,
};

enum class eFxLifecycleMode : u8_t
{
    Burst,
    Timed,
    WhileState,
    ManualStop,
    LoopUntilSignal,
};

struct FxAnchorDesc
{
    eFxAnchorType eAnchorType = eFxAnchorType::Entity;
    eFxAnchorFallback eFallback = eFxAnchorFallback::Entity;
    std::string strAnchorName;
    Vec3 vAnchorOffset = { 0.f, 0.f, 0.f };
    bool_t bInheritRotation = false;
};

struct FxLifecycleDesc
{
    eFxLifecycleMode eLifecycleMode = eFxLifecycleMode::Timed;
    f32_t fStopFadeOut = 0.25f;
    bool_t bDetachOnStop = true;
    bool_t bKillWhenAnchorInvalid = false;
};

struct FxNodeDesc
{
    std::string strType;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<u8_t> bytecodeBlob;
};

struct FxEmitterDesc
{
    std::string strName;
    eFxRenderType renderType = eFxRenderType::Billboard;
    u32_t maxParticles = 1;
    f32_t spawnRate = 0.f;
    std::vector<FxNodeDesc> nodes;

    RHITextureHandle hMaterial{};
    RHIBufferHandle hMeshGeometry{};
    eBlendPreset blendMode = eBlendPreset::AlphaBlend;
    FxMaterialDesc  material{};
    eFxDepthMode depthMode = eFxDepthMode::DepthTestWriteOn;

    wstring_t strTexturePath;
    wstring_t strErodeTexturePath;
    std::string strModelPath;

    Vec3 vAttachOffset = { 0.f, 0.f, 0.f };
    FxAnchorDesc anchor{};
    FxLifecycleDesc lifecycle{};
    Vec3 vEndOffset = { 0.f, 0.f, 0.f };
    Vec3 vVelocity = { 0.f, 0.f, 0.f };
    Vec3 vScale = { 1.f, 1.f, 1.f };
    Vec3 vRotation = { 0.f, 0.f, 0.f };
    f32_t fWorldYawSpinSpeed = 0.f;
    Vec4 vColor = { 1.f, 1.f, 1.f, 1.f };
    f32_t fSegmentT = -1.f;
    bool_t bScaleZToSegment = false;

    f32_t fWidth = 1.f;
    f32_t fHeight = 1.f;
    f32_t fYaw = 0.f;
    f32_t fStartRadius = 0.f;
    f32_t fEndRadius = 0.f;
    f32_t fThickness = 0.2f;
    f32_t fGrowDuration = 0.f;
    f32_t fLifetime = 3.f;
    f32_t fFadeIn = 0.f;
    f32_t fStartDelay = 0.f;
    f32_t fFadeOut = 0.f;
    u32_t iRibbonPointCount = 2;
    bool_t bHistoryTrail = false;
    f32_t fTrailSampleInterval = 0.025f;
    f32_t fTrailHeadWidthScale = 1.f;
    f32_t fTrailTailWidthScale = 1.f;
    f32_t fTrailHeadAlphaScale = 1.f;
    f32_t fTrailTailAlphaScale = 1.f;
    f32_t fTrailJitterAmplitude = 0.f;
    f32_t fTrailJitterFrequency = 0.f;
    f32_t fTrailJitterSeed = 0.f;

    u32_t iAtlasCols = 1;
    u32_t iAtlasRows = 1;
    u32_t iAtlasFrameCount = 1;
    f32_t fAtlasFps = 0.f;
    bool_t bAtlasLoop = true;

    f32_t fUvScrollU = 0.f;
    f32_t fUvScrollV = 0.f;
    f32_t fAlphaClip = 0.05f;
    f32_t fErodeThreshold = 0.f;
    u32_t iStyleMode = 0;
    Vec4 vStyleColorA = { 1.f, 1.f, 1.f, 1.f };
    Vec4 vStyleColorB = { 0.f, 0.f, 0.f, 1.f };
    Vec4 vRimColor = { 1.f, 1.f, 1.f, 0.f };
    f32_t fRimPower = 3.f;
    f32_t fCellLow = 0.f;
    f32_t fCellHigh = 0.5f;
    Vec4 vMagicScrollA = { 0.f, 0.5f, 0.1f, 0.05f };
    Vec4 vMagicShape = { 2.5f, 0.06f, 1.0f, 0.035f };
    Vec4 vMagicCore = { 2.0f, 1.0f, 2.0f, 0.f };
    f32_t fMaterialRandom = 0.f;
    bool_t bBillboard = true;
    bool_t bDepthWrite = true;
    bool_t bBlockableByWindWall = false;
};

inline void FxEmitterSetMaterialFromLegacyFields(FxEmitterDesc& emitter)
{
    emitter.material.vTint = emitter.vColor;
    emitter.material.vUVScroll = { emitter.fUvScrollU, emitter.fUvScrollV };
    emitter.material.fAlphaClip = emitter.fAlphaClip;
    emitter.material.fErodeThreshold = emitter.fErodeThreshold;
    emitter.material.iStyleMode = emitter.iStyleMode;
    emitter.material.vStyleColorA = emitter.vStyleColorA;
    emitter.material.vStyleColorB = emitter.vStyleColorB;
    emitter.material.vRimColor = emitter.vRimColor;
    emitter.material.fRimPower = emitter.fRimPower;
    emitter.material.fCellLow = emitter.fCellLow;
    emitter.material.fCellHigh = emitter.fCellHigh;
    emitter.material.vMagicScrollA = emitter.vMagicScrollA;
    emitter.material.vMagicShape = emitter.vMagicShape;
    emitter.material.vMagicCore = emitter.vMagicCore;
    emitter.material.fMaterialRandom = emitter.fMaterialRandom;

    emitter.depthMode = FxDepthModeFromDepthWrite(emitter.bDepthWrite);
}

inline void FxEmitterApplyMaterialToLegacyFields(FxEmitterDesc& emitter)
{
    emitter.vColor = emitter.material.vTint;
    emitter.fUvScrollU = emitter.material.vUVScroll.x;
    emitter.fUvScrollV = emitter.material.vUVScroll.y;
    emitter.fAlphaClip = emitter.material.fAlphaClip;
    emitter.fErodeThreshold = emitter.material.fErodeThreshold;
    emitter.iStyleMode = emitter.material.iStyleMode;
    emitter.vStyleColorA = emitter.material.vStyleColorA;
    emitter.vStyleColorB = emitter.material.vStyleColorB;
    emitter.vRimColor = emitter.material.vRimColor;
    emitter.fRimPower = emitter.material.fRimPower;
    emitter.fCellLow = emitter.material.fCellLow;
    emitter.fCellHigh = emitter.material.fCellHigh;
    emitter.vMagicScrollA = emitter.material.vMagicScrollA;
    emitter.vMagicShape = emitter.material.vMagicShape;
    emitter.vMagicCore = emitter.material.vMagicCore;
    emitter.fMaterialRandom = emitter.material.fMaterialRandom;

    emitter.bDepthWrite = FxDepthModeWritesDepth(emitter.depthMode);
}

struct FxAsset
{
    FxAssetHandle handle{};
    std::string strName;
    std::vector<FxEmitterDesc> emitters;
    CFxParameterMap initialUserParams;
};

struct FxAssetLoadResult
{
    FxAsset asset{};
    bool_t bSucceeded = false;
    std::string strError;
};

WINTERS_ENGINE FxAssetLoadResult LoadFxAssetFromFile(const wstring_t& path);

class CFxAssetRegistry final
{
public:
    WINTERS_ENGINE CFxAssetRegistry();
    ~CFxAssetRegistry() = default;

    CFxAssetRegistry(const CFxAssetRegistry&) = delete;
    CFxAssetRegistry& operator=(const CFxAssetRegistry&) = delete;

    WINTERS_ENGINE FxAssetHandle Register(FxAsset asset);
    WINTERS_ENGINE FxAssetHandle RegisterOrReplaceByName(FxAsset asset);

    WINTERS_ENGINE const FxAsset* Find(FxAssetHandle handle) const;
    WINTERS_ENGINE FxAsset* FindMutable(FxAssetHandle handle);
    WINTERS_ENGINE FxAssetHandle FindByName(const std::string& name) const;

    WINTERS_ENGINE void UnregisterAll();
    WINTERS_ENGINE u32_t GetAssetCount() const;

    WINTERS_ENGINE FxAssetHandle LoadFromFile(const wstring_t& path);
    WINTERS_ENGINE bool_t ReloadFromFile(FxAssetHandle handle);
    WINTERS_ENGINE u32_t LoadDirectory(const wstring_t& directoryPath);
    WINTERS_ENGINE const wstring_t* GetAssetPath(FxAssetHandle handle) const;

private:
    struct Slot
    {
        FxAsset asset;
        u32_t generation = 1;
        bool_t bAlive = false;
        wstring_t path;
    };

    Slot* ResolveSlot(FxAssetHandle handle);
    const Slot* ResolveSlot(FxAssetHandle handle) const;

    std::vector<Slot> m_Slots;
    std::unordered_map<std::string, FxAssetHandle> m_NameToHandle;
};
