#pragma once

#include "GameObject/FX/FxBeamComponent.h"
#include "GameObject/FX/FxBillboardComponent.h"
#include "GameObject/FX/FxMeshComponent.h"
#include "GameObject/FX/FxRibbonComponent.h"
#include "FX/FxAsset.h"

class CWorld;

namespace Engine
{
    class CFxStaticMeshRenderer;
}

namespace LegacyFx
{
    FxAsset MakeAssetFromBillboard(const FxBillboardComponent& src,
        const char* pszAssetName);
    FxAsset MakeAssetFromMesh(const FxMeshComponent& src,
        const char* pszAssetName);
    FxAsset MakeAssetFromBeam(const FxBeamComponent& src,
        const char* pszAssetName);
    FxAsset MakeAssetFromRibbon(const FxRibbonComponent& src,
        const char* pszAssetName);

    FxAssetHandle FxAssetFromBillboard(CFxAssetRegistry& registry,
        const FxBillboardComponent& src,
        const char* pszAssetName);
    FxAssetHandle FxAssetFromMesh(CFxAssetRegistry& registry,
        const FxMeshComponent& src,
        const char* pszAssetName);
    FxAssetHandle FxAssetFromBeam(CFxAssetRegistry& registry,
        const FxBeamComponent& src,
        const char* pszAssetName);
    FxAssetHandle FxAssetFromRibbon(CFxAssetRegistry& registry,
        const FxRibbonComponent& src,
        const char* pszAssetName);

    FxBillboardComponent BillboardFromAsset(const CFxAssetRegistry& registry,
        FxAssetHandle handle,
        const Vec3& vWorldPos,
        EntityID attachTo = NULL_ENTITY);
    FxMeshComponent MeshFromAsset(const CFxAssetRegistry& registry,
        FxAssetHandle handle,
        const Vec3& vWorldPos,
        EntityID attachTo = NULL_ENTITY);
    FxBeamComponent BeamFromAsset(const CFxAssetRegistry& registry,
        FxAssetHandle handle,
        const Vec3& vWorldPos,
        EntityID attachTo = NULL_ENTITY);
    FxRibbonComponent RibbonFromAsset(const CFxAssetRegistry& registry,
        FxAssetHandle handle,
        const Vec3& vWorldPos,
        EntityID attachTo = NULL_ENTITY);

    EntityID SpawnBillboardFromAsset(CWorld& world,
        const CFxAssetRegistry& registry,
        FxAssetHandle handle,
        const Vec3& vWorldPos,
        EntityID attachTo = NULL_ENTITY);
}
