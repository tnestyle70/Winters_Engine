#include "GameObject/FX/LegacyFxAdapter.h"
#include "GameObject/FX/FxSystem.h"
#include "ECS/World.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace
{
    std::wstring CopyWidePath(const tchar_t* path)
    {
        return path ? std::wstring(path) : std::wstring();
    }

    std::string CopyNarrowPath(const char* path)
    {
        return path ? std::string(path) : std::string();
    }

    const FxEmitterDesc* FindFirstEmitter(const CFxAssetRegistry& registry,
        FxAssetHandle handle)
    {
        const FxAsset* pAsset = registry.Find(handle);
        if (!pAsset || pAsset->emitters.empty())
            return nullptr;

        return &pAsset->emitters.front();
    }

    bool IsZero(const Vec3& v)
    {
        return v.x == 0.f && v.y == 0.f && v.z == 0.f;
    }

    Vec3 Add(const Vec3& a, const Vec3& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    Vec3 Subtract(const Vec3& a, const Vec3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    Vec3 Scale(const Vec3& v, f32_t fScale)
    {
        return { v.x * fScale, v.y * fScale, v.z * fScale };
    }

    Vec3 ResolveForwardFromYaw(f32_t fYaw)
    {
        return { std::sinf(fYaw), 0.f, std::cosf(fYaw) };
    }

    Vec3 ResolveEndDelta(const FxEmitterDesc& emitter)
    {
        if (!IsZero(emitter.vEndOffset))
            return emitter.vEndOffset;

        const Vec3 vForward = ResolveForwardFromYaw(emitter.fYaw);
        const f32_t fLength = (emitter.fHeight > 0.f) ? emitter.fHeight : 1.f;
        return Scale(vForward, fLength);
    }
}

FxAsset LegacyFx::MakeAssetFromBillboard(const FxBillboardComponent& src,
    const char* pszAssetName)
{
    FxAsset asset{};
    asset.strName = pszAssetName ? pszAssetName : "Legacy_Billboard";

    FxEmitterDesc emitter{};
    emitter.strName = asset.strName + "_Emitter";
    emitter.renderType = (!src.bBillboard && src.renderType == eFxRenderType::Billboard)
        ? eFxRenderType::GroundDecal
        : src.renderType;
    emitter.maxParticles = 1;
    emitter.strTexturePath = CopyWidePath(src.texturePath);
    emitter.vAttachOffset = src.vAttachOffset;
    emitter.vVelocity = src.vVelocity;
    emitter.vColor = src.vColor;
    emitter.fWidth = src.fWidth;
    emitter.fHeight = src.fHeight;
    emitter.fYaw = src.fYaw;
    emitter.fStartRadius = src.fStartRadius;
    emitter.fEndRadius = src.fEndRadius;
    emitter.fThickness = src.fThickness;
    emitter.fGrowDuration = src.fGrowDuration;
    emitter.fLifetime = src.fLifetime;
    emitter.fFadeIn = src.fFadeIn;
    emitter.fFadeOut = src.fFadeOut;
    emitter.iAtlasCols = src.iAtlasCols;
    emitter.iAtlasRows = src.iAtlasRows;
    emitter.iAtlasFrameCount = src.iAtlasFrameCount;
    emitter.fAtlasFps = src.fAtlasFps;
    emitter.bAtlasLoop = src.bAtlasLoop;
    emitter.fUvScrollU = src.fUvScrollU;
    emitter.fUvScrollV = src.fUvScrollV;
    emitter.fAlphaClip = src.fAlphaClip;
    emitter.fErodeThreshold = src.fErodeThreshold;
    emitter.blendMode = src.blendMode;
    emitter.bBillboard = src.bBillboard;
    FxEmitterSetMaterialFromLegacyFields(emitter);
    if (src.bMaterialReady)
    {
        emitter.material = src.material;
        emitter.depthMode = src.depthMode;
        FxEmitterApplyMaterialToLegacyFields(emitter);
    }
    emitter.bBlockableByWindWall = src.bBlockableByWindWall;
    asset.emitters.push_back(std::move(emitter));
    return asset;
}

FxAsset LegacyFx::MakeAssetFromMesh(const FxMeshComponent& src,
    const char* pszAssetName)
{
    FxAsset asset{};
    asset.strName = pszAssetName ? pszAssetName : "Legacy_Mesh";

    FxEmitterDesc emitter{};
    emitter.strName = asset.strName + "_Emitter";
    emitter.renderType = eFxRenderType::MeshParticle;
    emitter.maxParticles = 1;
    emitter.strModelPath = CopyNarrowPath(src.modelPath);
    emitter.strTexturePath = CopyWidePath(src.texturePath);
    emitter.strErodeTexturePath = CopyWidePath(src.erodeTexturePath);
    emitter.vAttachOffset = src.vAttachOffset;
    emitter.vVelocity = src.vVelocity;
    emitter.vScale = src.vScale;
    emitter.vRotation = src.vRotation;
    emitter.vColor = src.vColor;
    emitter.fLifetime = src.fLifetime;
    emitter.blendMode = src.blendMode;
    emitter.fAlphaClip = src.fAlphaClip;
    emitter.fFadeIn = src.fFadeIn;
    emitter.fFadeOut = src.fFadeOut;
    emitter.fUvScrollU = src.fUvScrollU;
    emitter.fUvScrollV = src.fUvScrollV;
    emitter.fErodeThreshold = src.fErodeThreshold;
    emitter.iStyleMode = src.iStyleMode;
    emitter.vStyleColorA = src.vStyleColorA;
    emitter.vStyleColorB = src.vStyleColorB;
    emitter.vRimColor = src.vRimColor;
    emitter.fRimPower = src.fRimPower;
    emitter.fCellLow = src.fCellLow;
    emitter.fCellHigh = src.fCellHigh;
    emitter.vMagicScrollA = src.vMagicScrollA;
    emitter.vMagicShape = src.vMagicShape;
    emitter.vMagicCore = src.vMagicCore;
    emitter.fMaterialRandom = src.fMaterialRandom;
    emitter.bDepthWrite = src.bDepthWrite;
    FxEmitterSetMaterialFromLegacyFields(emitter);
    if (src.bMaterialReady)
    {
        emitter.material = src.material;
        emitter.depthMode = src.depthMode;
        FxEmitterApplyMaterialToLegacyFields(emitter);
    }
    emitter.bBlockableByWindWall = src.bBlockableByWindWall;
    asset.emitters.push_back(std::move(emitter));
    return asset;
}

FxAsset LegacyFx::MakeAssetFromBeam(const FxBeamComponent& src,
    const char* pszAssetName)
{
    FxAsset asset{};
    asset.strName = pszAssetName ? pszAssetName : "Legacy_Beam";

    FxEmitterDesc emitter{};
    emitter.strName = asset.strName + "_Emitter";
    emitter.renderType = eFxRenderType::Beam;
    emitter.maxParticles = 1;
    emitter.strTexturePath = CopyWidePath(src.texturePath);
    emitter.vAttachOffset = src.vStartOffset;
    emitter.vEndOffset = Subtract(src.vEndOffset, src.vStartOffset);
    emitter.vVelocity = src.vVelocity;
    emitter.vColor = src.vColor;
    emitter.fWidth = src.fWidth;
    emitter.fLifetime = src.fLifetime;
    emitter.fFadeIn = src.fFadeIn;
    emitter.fFadeOut = src.fFadeOut;
    emitter.fUvScrollU = src.fUvScrollU;
    emitter.fUvScrollV = src.fUvScrollV;
    emitter.fAlphaClip = src.fAlphaClip;
    emitter.fErodeThreshold = src.fErodeThreshold;
    emitter.blendMode = src.blendMode;
    FxEmitterSetMaterialFromLegacyFields(emitter);
    if (src.bMaterialReady)
    {
        emitter.material = src.material;
        emitter.depthMode = src.depthMode;
        FxEmitterApplyMaterialToLegacyFields(emitter);
    }
    emitter.bBlockableByWindWall = src.bBlockableByWindWall;
    asset.emitters.push_back(std::move(emitter));
    return asset;
}

FxAsset LegacyFx::MakeAssetFromRibbon(const FxRibbonComponent& src,
    const char* pszAssetName)
{
    FxAsset asset{};
    asset.strName = pszAssetName ? pszAssetName : "Legacy_Ribbon";

    FxEmitterDesc emitter{};
    emitter.strName = asset.strName + "_Emitter";
    emitter.renderType = eFxRenderType::Ribbon;
    emitter.maxParticles = (std::max)(2u, src.iPointCount);
    emitter.strTexturePath = CopyWidePath(src.texturePath);
    emitter.vAttachOffset = src.vStartOffset;
    emitter.vEndOffset = Subtract(src.vEndOffset, src.vStartOffset);
    emitter.vVelocity = src.vVelocity;
    emitter.vColor = src.vColor;
    emitter.fWidth = src.fWidth;
    emitter.fLifetime = src.fLifetime;
    emitter.fFadeIn = src.fFadeIn;
    emitter.fFadeOut = src.fFadeOut;
    emitter.fUvScrollU = src.fUvScrollU;
    emitter.fUvScrollV = src.fUvScrollV;
    emitter.fAlphaClip = src.fAlphaClip;
    emitter.fErodeThreshold = src.fErodeThreshold;
    emitter.iRibbonPointCount = std::clamp(src.iPointCount, 2u, FX_RIBBON_MAX_POINTS);
    emitter.blendMode = src.blendMode;
    FxEmitterSetMaterialFromLegacyFields(emitter);
    if (src.bMaterialReady)
    {
        emitter.material = src.material;
        emitter.depthMode = src.depthMode;
        FxEmitterApplyMaterialToLegacyFields(emitter);
    }
    emitter.bBlockableByWindWall = src.bBlockableByWindWall;
    asset.emitters.push_back(std::move(emitter));
    return asset;
}

FxAssetHandle LegacyFx::FxAssetFromBillboard(CFxAssetRegistry& registry,
    const FxBillboardComponent& src,
    const char* pszAssetName)
{
    return registry.RegisterOrReplaceByName(
        MakeAssetFromBillboard(src, pszAssetName));
}

FxAssetHandle LegacyFx::FxAssetFromMesh(CFxAssetRegistry& registry,
    const FxMeshComponent& src,
    const char* pszAssetName)
{
    return registry.RegisterOrReplaceByName(
        MakeAssetFromMesh(src, pszAssetName));
}

FxAssetHandle LegacyFx::FxAssetFromBeam(CFxAssetRegistry& registry,
    const FxBeamComponent& src,
    const char* pszAssetName)
{
    return registry.RegisterOrReplaceByName(
        MakeAssetFromBeam(src, pszAssetName));
}

FxAssetHandle LegacyFx::FxAssetFromRibbon(CFxAssetRegistry& registry,
    const FxRibbonComponent& src,
    const char* pszAssetName)
{
    return registry.RegisterOrReplaceByName(
        MakeAssetFromRibbon(src, pszAssetName));
}

FxBillboardComponent LegacyFx::BillboardFromAsset(const CFxAssetRegistry& registry,
    FxAssetHandle handle,
    const Vec3& vWorldPos,
    EntityID attachTo)
{
    FxBillboardComponent fx{};
    const FxEmitterDesc* pEmitter = FindFirstEmitter(registry, handle);
    if (!pEmitter)
        return fx;

    fx.hAsset = handle;
    fx.renderType = pEmitter->renderType;
    fx.vWorldPos = vWorldPos;
    fx.attachTo = attachTo;
    fx.vAttachOffset = pEmitter->vAttachOffset;
    fx.vVelocity = pEmitter->vVelocity;
    fx.SetTexturePath(pEmitter->strTexturePath);
    fx.vColor = pEmitter->vColor;
    fx.fWidth = pEmitter->fWidth;
    fx.fHeight = pEmitter->fHeight;
    fx.fYaw = pEmitter->fYaw;
    fx.fStartRadius = pEmitter->fStartRadius;
    fx.fEndRadius = pEmitter->fEndRadius;
    fx.fThickness = pEmitter->fThickness;
    fx.fGrowDuration = pEmitter->fGrowDuration;
    fx.fLifetime = pEmitter->fLifetime;
    fx.fFadeIn = pEmitter->fFadeIn;
    fx.fFadeOut = pEmitter->fFadeOut;
    fx.iAtlasCols = pEmitter->iAtlasCols;
    fx.iAtlasRows = pEmitter->iAtlasRows;
    fx.iAtlasFrameCount = pEmitter->iAtlasFrameCount;
    fx.fAtlasFps = pEmitter->fAtlasFps;
    fx.bAtlasLoop = pEmitter->bAtlasLoop;
    fx.fUvScrollU = pEmitter->fUvScrollU;
    fx.fUvScrollV = pEmitter->fUvScrollV;
    fx.fAlphaClip = pEmitter->fAlphaClip;
    fx.fErodeThreshold = pEmitter->fErodeThreshold;
    fx.blendMode = pEmitter->blendMode;
    fx.bBillboard = pEmitter->bBillboard;
    fx.SetMaterialFromDesc(pEmitter->material, pEmitter->depthMode);
    fx.bBlockableByWindWall = pEmitter->bBlockableByWindWall;
    return fx;
}

FxMeshComponent LegacyFx::MeshFromAsset(const CFxAssetRegistry& registry,
    FxAssetHandle handle,
    const Vec3& vWorldPos,
    EntityID attachTo)
{
    FxMeshComponent fx{};
    const FxEmitterDesc* pEmitter = FindFirstEmitter(registry, handle);
    if (!pEmitter)
        return fx;

    fx.hAsset = handle;
    fx.vWorldPos = vWorldPos;
    fx.attachTo = attachTo;
    fx.vAttachOffset = pEmitter->vAttachOffset;
    fx.vVelocity = pEmitter->vVelocity;
    fx.vScale = pEmitter->vScale;
    fx.vRotation = pEmitter->vRotation;
    fx.SetModelPath(pEmitter->strModelPath);
    fx.SetTexturePath(pEmitter->strTexturePath);
    fx.SetErodeTexturePath(pEmitter->strErodeTexturePath);
    fx.fLifetime = pEmitter->fLifetime;
    fx.blendMode = pEmitter->blendMode;
    fx.fFadeIn = pEmitter->fFadeIn;
    fx.fFadeOut = pEmitter->fFadeOut;
    fx.SetMaterialFromDesc(pEmitter->material, pEmitter->depthMode);
    fx.bBlockableByWindWall = pEmitter->bBlockableByWindWall;
    return fx;
}

FxBeamComponent LegacyFx::BeamFromAsset(const CFxAssetRegistry& registry,
    FxAssetHandle handle,
    const Vec3& vWorldPos,
    EntityID attachTo)
{
    FxBeamComponent fx{};
    const FxEmitterDesc* pEmitter = FindFirstEmitter(registry, handle);
    if (!pEmitter)
        return fx;

    fx.hAsset = handle;
    fx.iEmitterIndex = 0;
    fx.hStart = attachTo;
    const Vec3 vEndDelta = ResolveEndDelta(*pEmitter);
    const Vec3 vEndOffset = Add(pEmitter->vAttachOffset, vEndDelta);
    fx.vStartWorldPos = Add(vWorldPos, pEmitter->vAttachOffset);
    fx.vEndWorldPos = Add(fx.vStartWorldPos, vEndDelta);
    fx.vStartOffset = pEmitter->vAttachOffset;
    fx.vEndOffset = vEndOffset;
    fx.vVelocity = pEmitter->vVelocity;
    fx.SetTexturePath(pEmitter->strTexturePath);
    fx.fWidth = pEmitter->fWidth;
    fx.fLifetime = pEmitter->fLifetime;
    fx.fFadeIn = pEmitter->fFadeIn;
    fx.fFadeOut = pEmitter->fFadeOut;
    fx.fUvScrollU = pEmitter->fUvScrollU;
    fx.fUvScrollV = pEmitter->fUvScrollV;
    fx.fUvScrollSpeed = pEmitter->fUvScrollV;
    fx.vColor = pEmitter->vColor;
    fx.fAlphaClip = pEmitter->fAlphaClip;
    fx.fErodeThreshold = pEmitter->fErodeThreshold;
    fx.blendMode = pEmitter->blendMode;
    fx.SetMaterialFromDesc(pEmitter->material, pEmitter->depthMode);
    fx.bBlockableByWindWall = pEmitter->bBlockableByWindWall;
    return fx;
}

FxRibbonComponent LegacyFx::RibbonFromAsset(const CFxAssetRegistry& registry,
    FxAssetHandle handle,
    const Vec3& vWorldPos,
    EntityID attachTo)
{
    FxRibbonComponent fx{};
    const FxEmitterDesc* pEmitter = FindFirstEmitter(registry, handle);
    if (!pEmitter)
        return fx;

    fx.hAsset = handle;
    fx.iEmitterIndex = 0;
    fx.attachTo = attachTo;
    const Vec3 vEndDelta = ResolveEndDelta(*pEmitter);
    fx.vStartOffset = pEmitter->vAttachOffset;
    fx.vEndOffset = Add(pEmitter->vAttachOffset, vEndDelta);
    fx.vVelocity = pEmitter->vVelocity;
    fx.SetTexturePath(pEmitter->strTexturePath);
    fx.fWidth = pEmitter->fWidth;
    fx.fLifetime = pEmitter->fLifetime;
    fx.fFadeIn = pEmitter->fFadeIn;
    fx.fFadeOut = pEmitter->fFadeOut;
    fx.fUvScrollU = pEmitter->fUvScrollU;
    fx.fUvScrollV = pEmitter->fUvScrollV;
    fx.vColor = pEmitter->vColor;
    fx.fAlphaClip = pEmitter->fAlphaClip;
    fx.fErodeThreshold = pEmitter->fErodeThreshold;
    fx.blendMode = pEmitter->blendMode;
    fx.SetMaterialFromDesc(pEmitter->material, pEmitter->depthMode);
    fx.bBlockableByWindWall = pEmitter->bBlockableByWindWall;

    const u32_t uPointCount = std::clamp(pEmitter->iRibbonPointCount, 2u, FX_RIBBON_MAX_POINTS);
    for (u32_t i = 0; i < uPointCount; ++i)
    {
        const f32_t fT = (uPointCount > 1)
            ? static_cast<f32_t>(i) / static_cast<f32_t>(uPointCount - 1)
            : 0.f;
        const Vec3 vOffset = Add(pEmitter->vAttachOffset, Scale(vEndDelta, fT));
        fx.SetPoint(i, Add(vWorldPos, vOffset));
    }

    return fx;
}

EntityID LegacyFx::SpawnBillboardFromAsset(CWorld& world,
    const CFxAssetRegistry& registry,
    FxAssetHandle handle,
    const Vec3& vWorldPos,
    EntityID attachTo)
{
    FxBillboardComponent fx =
        BillboardFromAsset(registry, handle, vWorldPos, attachTo);
    if (!fx.texturePath)
        return NULL_ENTITY;

    return CFxSystem::Spawn(world, fx);
}
