#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"

#include <string>
#include <vector>

namespace Engine
{
    enum class eWorldCellState : u8_t
    {
        Unloaded = 0,
        Queued,
        LoadedHidden,
        Visible
    };

    enum class eWorldCellTransitionReason : u8_t
    {
        None = 0,
        NoActiveSource,
        OutsideUnloadRadius,
        WithinUnloadRadius,
        WithinLoadRadius,
        WithinVisibleRadius,
        AssetsRequested,
        WaitingForAssets,
        RequiredAssetsReady,
        MissingRequiredAsset,
        Released
    };

    struct StreamingSourceComponent
    {
        Vec3  vPosition{ 0.f, 0.f, 0.f };
        f32_t fVisibleRadius = 160.f;
        f32_t fLoadRadius = 256.f;
        f32_t fUnloadRadius = 320.f;
        u32_t uPriority = 0u;
        bool_t bActive = true;
    };

    struct CellInstanceDesc
    {
        std::string strName;
        std::string strModel;
        std::string strKind;
        std::string strWmesh;
        std::string strWmat;

        Vec3 vPosition{ 0.f, 0.f, 0.f };
        Vec3 vRotationDeg{ 0.f, 0.f, 0.f };
        Vec3 vScale{ 1.f, 1.f, 1.f };

        u64_t layerBit = 1ull;
        bool_t bPlaceable = false;
        bool_t bRequired = true;
    };

    struct CellDescriptor
    {
        std::string strId;
        i32_t iCoordX = 0;
        i32_t iCoordZ = 0;

        Vec3 vBoundsMin{ 0.f, 0.f, 0.f };
        Vec3 vBoundsMax{ 0.f, 0.f, 0.f };

        std::string strFile;
        std::string strNavPath;
        std::string strHlodWmesh;

        std::vector<CellInstanceDesc> vecInstances;
    };

    struct WorldDescriptor
    {
        std::string strName;
        std::string strSourcePath;

        f32_t fCellSizeMeters = 256.f;
        Vec3 vOrigin{ 0.f, 0.f, 0.f };

        i32_t iGridDimX = 0;
        i32_t iGridDimZ = 0;
        i32_t iTileBaseX = 0;
        i32_t iTileBaseZ = 0;

        std::vector<CellDescriptor> vecCells;
    };

    inline const char* ToString(eWorldCellState eState)
    {
        switch (eState)
        {
        case eWorldCellState::Unloaded: return "Unloaded";
        case eWorldCellState::Queued: return "Queued";
        case eWorldCellState::LoadedHidden: return "LoadedHidden";
        case eWorldCellState::Visible: return "Visible";
        default: return "Unknown";
        }
    }

    inline const char* ToString(eWorldCellTransitionReason eReason)
    {
        switch (eReason)
        {
        case eWorldCellTransitionReason::None: return "None";
        case eWorldCellTransitionReason::NoActiveSource: return "NoActiveSource";
        case eWorldCellTransitionReason::OutsideUnloadRadius: return "OutsideUnloadRadius";
        case eWorldCellTransitionReason::WithinUnloadRadius: return "WithinUnloadRadius";
        case eWorldCellTransitionReason::WithinLoadRadius: return "WithinLoadRadius";
        case eWorldCellTransitionReason::WithinVisibleRadius: return "WithinVisibleRadius";
        case eWorldCellTransitionReason::AssetsRequested: return "AssetsRequested";
        case eWorldCellTransitionReason::WaitingForAssets: return "WaitingForAssets";
        case eWorldCellTransitionReason::RequiredAssetsReady: return "RequiredAssetsReady";
        case eWorldCellTransitionReason::MissingRequiredAsset: return "MissingRequiredAsset";
        case eWorldCellTransitionReason::Released: return "Released";
        default: return "Unknown";
        }
    }
}
