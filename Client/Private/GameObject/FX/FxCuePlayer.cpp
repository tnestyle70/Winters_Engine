#include "GameObject/FX/FxCuePlayer.h"

#include "GameObject/FX/FxBeamComponent.h"
#include "GameObject/FX/FxBeamSystem.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxMeshSystem.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "GameObject/FX/FxSystem.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace
{
    bool FileExistsFile(const wchar_t* path)
    {
        const DWORD attr = GetFileAttributesW(path);
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool DirectoryExists(const wchar_t* path)
    {
        const DWORD attr = GetFileAttributesW(path);
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    void EnsureTrailingSlash(std::wstring& path)
    {
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
            path.push_back(L'\\');
    }

    bool TryFindWorkspaceFxRootFrom(const std::wstring& startDir, std::wstring& outRoot)
    {
        std::wstring base = startDir;
        EnsureTrailingSlash(base);

        for (int depth = 0; depth < 8 && !base.empty(); ++depth)
        {
            const std::wstring solutionPath = base + L"Winters.sln";
            const std::wstring fxRoot = base + L"Data\\LoL\\FX\\";
            if (FileExistsFile(solutionPath.c_str()) && DirectoryExists(fxRoot.c_str()))
            {
                outRoot = fxRoot;
                return true;
            }

            std::wstring trimmed = base;
            while (!trimmed.empty() && (trimmed.back() == L'\\' || trimmed.back() == L'/'))
                trimmed.pop_back();

            const size_t slash = trimmed.find_last_of(L"\\/");
            if (slash == std::wstring::npos)
                break;

            base = trimmed.substr(0, slash + 1);
        }

        return false;
    }

    bool TryFindWorkspaceFxRoot(std::wstring& outRoot)
    {
        wchar_t exePath[MAX_PATH] = {};
        const DWORD n = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
        {
            std::wstring exeDir = exePath;
            const size_t slash = exeDir.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
            {
                exeDir.resize(slash + 1);
                if (TryFindWorkspaceFxRootFrom(exeDir, outRoot))
                    return true;
            }
        }

        wchar_t cwd[MAX_PATH] = {};
        const DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
        if (cwdLen > 0 && cwdLen < MAX_PATH)
            return TryFindWorkspaceFxRootFrom(cwd, outRoot);

        return false;
    }

    bool_t IsCueBillboardType(eFxRenderType type)
    {
        return type == eFxRenderType::Billboard ||
            type == eFxRenderType::GroundDecal ||
            type == eFxRenderType::ShockwaveRing;
    }

    bool_t IsZeroVec3(const Vec3& v)
    {
        return v.x == 0.f && v.y == 0.f && v.z == 0.f;
    }

    void LogMissingCue(const char* pszCueName)
    {
        char szBuffer[192]{};
        sprintf_s(szBuffer, "[FxCuePlayer] Missing cue: %s\n", pszCueName ? pszCueName : "(null)");
        OutputDebugStringA(szBuffer);
    }

    void LogSkippedCueEmitter(const char* pszCueName, const FxEmitterDesc& emitter)
    {
        char szBuffer[256]{};
        sprintf_s(szBuffer, "[FxCuePlayer] Skipped cue emitter cue=%s emitter=%s type=%u\n",
            pszCueName ? pszCueName : "(null)",
            emitter.strName.empty() ? "(unnamed)" : emitter.strName.c_str(),
            static_cast<u32_t>(emitter.renderType));
        OutputDebugStringA(szBuffer);
    }

    Vec3 ApplyCueOffset(const Vec3& origin, const Vec3& offset, const Vec3& forward)
    {
        const Vec3 dir = WintersMath::NormalizeXZOrZero(forward);
        if (dir.x == 0.f && dir.z == 0.f)
            return { origin.x + offset.x, origin.y + offset.y, origin.z + offset.z };

        const Vec3 right{ dir.z, 0.f, -dir.x };
        return {
            origin.x + right.x * offset.x + dir.x * offset.z,
            origin.y + offset.y,
            origin.z + right.z * offset.x + dir.z * offset.z
        };
    }

    Vec3 LerpVec3(const Vec3& a, const Vec3& b, f32_t t)
    {
        return {
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t
        };
    }

    f32_t SegmentLengthXZ(const Vec3& a, const Vec3& b)
    {
        const f32_t dx = b.x - a.x;
        const f32_t dz = b.z - a.z;
        return std::sqrtf(dx * dx + dz * dz);
    }

    Vec3 ResolveCueAnchorWorldPos(
        const FxEmitterDesc& emitter,
        const FxCueContext& ctx,
        const Vec3& forward)
    {
        if (ctx.bOverrideEndWorldPos && emitter.fSegmentT >= 0.f)
        {
            const f32_t t = std::clamp(emitter.fSegmentT, 0.f, 1.f);
            return ApplyCueOffset(LerpVec3(ctx.vWorldPos, ctx.vEndWorldPos, t),
                emitter.vAttachOffset,
                forward);
        }

        return ctx.attachTo != NULL_ENTITY
            ? ctx.vWorldPos
            : ApplyCueOffset(ctx.vWorldPos, emitter.vAttachOffset, forward);
    }

    Vec3 ResolveCueEndWorldPos(
        const Vec3& start,
        const FxEmitterDesc& emitter,
        const FxCueContext& ctx,
        const Vec3& forward)
    {
        if (ctx.bOverrideEndWorldPos)
            return ApplyCueOffset(ctx.vEndWorldPos, emitter.vAttachOffset, forward);

        const Vec3 localEnd = !IsZeroVec3(emitter.vEndOffset)
            ? emitter.vEndOffset
            : Vec3{ 0.f, 0.f, (emitter.fHeight > 0.f) ? emitter.fHeight : 1.f };
        return ApplyCueOffset(start, localEnd, forward);
    }

    void PreloadDefaultCueDirectoriesOnce()
    {
        static bool_t s_bLoaded = false;
        if (s_bLoaded)
            return;

        s_bLoaded = true;
        std::wstring fxRoot;
        if (TryFindWorkspaceFxRoot(fxRoot))
            CFxSystem::GetAssetRegistry().LoadDirectory(fxRoot);
        else
            CFxSystem::GetAssetRegistry().LoadDirectory(L"Data/LoL/FX");
    }

    FxBillboardComponent BuildCueBillboard(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const FxCueContext& ctx)
    {
        const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);

        FxBillboardComponent fx{};
        fx.hAsset = asset.handle;
        fx.iEmitterIndex = emitterIndex;
        fx.renderType = emitter.renderType;
        fx.vWorldPos = ResolveCueAnchorWorldPos(emitter, ctx, vForward);
        fx.attachTo = ctx.attachTo;
        fx.vAttachOffset = emitter.vAttachOffset;
        fx.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
        fx.SetTexturePath(emitter.strTexturePath);
        fx.fWidth = ctx.bOverrideSize ? ctx.fWidthOverride : emitter.fWidth;
        fx.fHeight = ctx.bOverrideSize ? ctx.fHeightOverride : emitter.fHeight;
        fx.fYaw = emitter.fYaw;
        fx.fStartRadius = emitter.fStartRadius;
        fx.fEndRadius = emitter.fEndRadius;
        fx.fThickness = emitter.fThickness;
        fx.fGrowDuration = emitter.fGrowDuration;
        fx.vColor = emitter.vColor;
        fx.fFadeIn = emitter.fFadeIn;
        fx.fStartDelay = emitter.fStartDelay;
        fx.fFadeOut = emitter.fFadeOut;
        fx.iAtlasCols = emitter.iAtlasCols;
        fx.iAtlasRows = emitter.iAtlasRows;
        fx.iAtlasFrameCount = emitter.iAtlasFrameCount;
        fx.fAtlasFps = emitter.fAtlasFps;
        fx.bAtlasLoop = emitter.bAtlasLoop;
        fx.fUvScrollU = emitter.fUvScrollU;
        fx.fUvScrollV = emitter.fUvScrollV;
        fx.fAlphaClip = emitter.fAlphaClip;
        fx.fErodeThreshold = emitter.fErodeThreshold;
        fx.SetMaterialFromDesc(emitter.material, emitter.depthMode);
        fx.blendMode = emitter.blendMode;
        fx.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
        fx.bBillboard = emitter.bBillboard && emitter.renderType == eFxRenderType::Billboard;
        fx.bBlockableByWindWall = emitter.bBlockableByWindWall;

        if (vForward.x != 0.f || vForward.z != 0.f)
            fx.fYaw += WintersMath::YawFromDirectionXZ(vForward);

        return fx;
    }

    FxBeamComponent BuildCueBeam(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const FxCueContext& ctx)
    {
        const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);
        const Vec3 start = ApplyCueOffset(ctx.vWorldPos, emitter.vAttachOffset, vForward);
        const Vec3 end = ResolveCueEndWorldPos(start, emitter, ctx, vForward);

        FxBeamComponent beam{};
        beam.hAsset = asset.handle;
        beam.iEmitterIndex = emitterIndex;
        beam.hStart = ctx.attachTo;
        beam.vStartWorldPos = start;
        beam.vEndWorldPos = end;
        beam.vStartOffset = emitter.vAttachOffset;
        beam.vEndOffset = { end.x - ctx.vWorldPos.x, end.y - ctx.vWorldPos.y, end.z - ctx.vWorldPos.z };
        beam.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
        beam.SetTexturePath(emitter.strTexturePath);
        beam.fWidth = ctx.bOverrideSize ? ctx.fWidthOverride : emitter.fWidth;
        beam.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
        beam.fStartDelay = emitter.fStartDelay;
        beam.fFadeIn = emitter.fFadeIn;
        beam.fFadeOut = emitter.fFadeOut;
        beam.fUvScrollSpeed = emitter.fUvScrollV;
        beam.fUvScrollU = emitter.fUvScrollU;
        beam.fUvScrollV = emitter.fUvScrollV;
        beam.vColor = emitter.vColor;
        beam.blendMode = emitter.blendMode;
        beam.bBlockableByWindWall = emitter.bBlockableByWindWall;
        beam.fAlphaClip = emitter.fAlphaClip;
        beam.fErodeThreshold = emitter.fErodeThreshold;
        beam.SetMaterialFromDesc(emitter.material, emitter.depthMode);
        return beam;
    }

    FxRibbonComponent BuildCueRibbon(
        const FxAsset& asset,
        const FxEmitterDesc& emitter,
        u32_t emitterIndex,
        const FxCueContext& ctx)
    {
        const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);
        const Vec3 start = ApplyCueOffset(ctx.vWorldPos, emitter.vAttachOffset, vForward);
        const Vec3 end = ResolveCueEndWorldPos(start, emitter, ctx, vForward);

        FxRibbonComponent ribbon{};
        ribbon.hAsset = asset.handle;
        ribbon.iEmitterIndex = emitterIndex;
        ribbon.attachTo = ctx.attachTo;
        ribbon.vStartOffset = emitter.vAttachOffset;
        ribbon.vEndOffset = { end.x - ctx.vWorldPos.x, end.y - ctx.vWorldPos.y, end.z - ctx.vWorldPos.z };
        ribbon.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
        ribbon.SetTexturePath(emitter.strTexturePath);
        ribbon.fWidth = ctx.bOverrideSize ? ctx.fWidthOverride : emitter.fWidth;
        ribbon.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
        ribbon.fStartDelay = emitter.fStartDelay;
        ribbon.fFadeIn = emitter.fFadeIn;
        ribbon.fFadeOut = emitter.fFadeOut;
        ribbon.fUvScrollU = emitter.fUvScrollU;
        ribbon.fUvScrollV = emitter.fUvScrollV;
        ribbon.vColor = emitter.vColor;
        ribbon.blendMode = emitter.blendMode;
        ribbon.bBlockableByWindWall = emitter.bBlockableByWindWall;
        ribbon.fAlphaClip = emitter.fAlphaClip;
        ribbon.fErodeThreshold = emitter.fErodeThreshold;
        ribbon.SetMaterialFromDesc(emitter.material, emitter.depthMode);

        const u32_t pointCount = std::clamp(emitter.iRibbonPointCount, 2u, FX_RIBBON_MAX_POINTS);
        for (u32_t pointIndex = 0; pointIndex < pointCount; ++pointIndex)
        {
            const f32_t t = (pointCount > 1u)
                ? static_cast<f32_t>(pointIndex) / static_cast<f32_t>(pointCount - 1u)
                : 0.f;
            ribbon.SetPoint(pointIndex, Vec3{
                start.x + (end.x - start.x) * t,
                start.y + (end.y - start.y) * t,
                start.z + (end.z - start.z) * t
            });
        }

        return ribbon;
    }

    FxMeshComponent BuildCueMesh(
        const FxEmitterDesc& emitter,
        const FxCueContext& ctx,
        FxAssetHandle handle,
        u32_t emitterIndex)
    {
        const Vec3 vForward = WintersMath::NormalizeXZOrZero(ctx.vForward);

        FxMeshComponent mesh{};
        mesh.hAsset = handle;
        mesh.iEmitterIndex = emitterIndex;
        mesh.vWorldPos = ResolveCueAnchorWorldPos(emitter, ctx, vForward);
        mesh.attachTo = ctx.attachTo;
        mesh.vAttachOffset = emitter.vAttachOffset;
        mesh.vVelocity = ctx.bOverrideVelocity ? ctx.vVelocity : emitter.vVelocity;
        mesh.vScale = emitter.vScale;
        mesh.fWorldYawSpinSpeed = emitter.fWorldYawSpinSpeed;
        if (ctx.bOverrideEndWorldPos && emitter.bScaleZToSegment)
        {
            const f32_t segmentLength = SegmentLengthXZ(ctx.vWorldPos, ctx.vEndWorldPos);
            mesh.vScale.z *= (segmentLength > 0.001f) ? segmentLength : 1.f;
        }
        mesh.vRotation = emitter.vRotation;
        if (vForward.x != 0.f || vForward.z != 0.f)
            mesh.vRotation.y += WintersMath::YawFromDirectionXZ(vForward);
        mesh.SetModelPath(emitter.strModelPath);
        mesh.SetTexturePath(emitter.strTexturePath);
        mesh.SetErodeTexturePath(emitter.strErodeTexturePath);
        mesh.fLifetime = ctx.bOverrideLifetime ? ctx.fLifetimeOverride : emitter.fLifetime;
        mesh.fStartDelay = emitter.fStartDelay;
        mesh.blendMode = emitter.blendMode;
        mesh.fFadeIn = emitter.fFadeIn;
        mesh.fFadeOut = emitter.fFadeOut;
        mesh.SetMaterialFromDesc(emitter.material, emitter.depthMode);
        mesh.bBlockableByWindWall = emitter.bBlockableByWindWall;
        return mesh;
    }
}

u32_t CFxCuePlayer::PreloadDirectory(const wchar_t* wszDirectoryPath)
{
    if (!wszDirectoryPath || !wszDirectoryPath[0])
        return 0u;

    return CFxSystem::GetAssetRegistry().LoadDirectory(wszDirectoryPath);
}

FxAssetHandle CFxCuePlayer::FindCue(const char* pszCueName)
{
    if (!pszCueName || !pszCueName[0])
        return {};

    CFxAssetRegistry& registry = CFxSystem::GetAssetRegistry();
    FxAssetHandle handle = registry.FindByName(pszCueName);
    if (handle.IsValid())
        return handle;

    PreloadDefaultCueDirectoriesOnce();
    return registry.FindByName(pszCueName);
}

EntityID CFxCuePlayer::Play(CWorld& world, const char* pszCueName, const FxCueContext& ctx)
{
    return PlayAll(world, pszCueName, ctx, nullptr);
}

EntityID CFxCuePlayer::PlayAll(
    CWorld& world,
    const char* pszCueName,
    const FxCueContext& ctx,
    std::vector<EntityID>* pOutSpawned)
{
    const FxAssetHandle handle = FindCue(pszCueName);
    const FxAsset* pAsset = CFxSystem::GetAssetRegistry().Find(handle);
    if (!pAsset)
    {
        LogMissingCue(pszCueName);
        return NULL_ENTITY;
    }

    EntityID firstEntity = NULL_ENTITY;
    for (u32_t i = 0; i < pAsset->emitters.size(); ++i)
    {
        const FxEmitterDesc& emitter = pAsset->emitters[i];
        EntityID entity = NULL_ENTITY;

        if (IsCueBillboardType(emitter.renderType))
        {
            entity = CFxSystem::Spawn(
                world,
                BuildCueBillboard(*pAsset, emitter, i, ctx));
        }
        else if (emitter.renderType == eFxRenderType::Beam)
        {
            entity = CFxBeamSystem::Spawn(
                world,
                BuildCueBeam(*pAsset, emitter, i, ctx));
        }
        else if (emitter.renderType == eFxRenderType::Ribbon)
        {
            entity = CFxBeamSystem::Spawn(
                world,
                BuildCueRibbon(*pAsset, emitter, i, ctx));
        }
        else if (emitter.renderType == eFxRenderType::MeshParticle)
        {
            if (ctx.pFxMeshRenderer)
            {
                entity = CFxMeshSystem::Spawn(
                    world,
                    ctx.pFxMeshRenderer,
                    BuildCueMesh(emitter, ctx, pAsset->handle, i));
            }
            else
            {
                LogSkippedCueEmitter(pszCueName, emitter);
            }
        }
        else
        {
            LogSkippedCueEmitter(pszCueName, emitter);
        }

        if (entity != NULL_ENTITY && pOutSpawned)
            pOutSpawned->push_back(entity);
        if (firstEntity == NULL_ENTITY && entity != NULL_ENTITY)
            firstEntity = entity;
    }

    return firstEntity;
}
