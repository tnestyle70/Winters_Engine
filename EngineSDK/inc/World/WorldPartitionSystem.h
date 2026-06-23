#pragma once

#include "WintersAPI.h"
#include "WintersMath.h"
#include "WintersTypes.h"
#include "World/AssetStreamingSystem.h"
#include "World/WorldPartitionTypes.h"

#include <memory>
#include <string>
#include <vector>

namespace Engine
{
    struct WorldCellRuntime
    {
        const CellDescriptor* pDesc = nullptr;
        eWorldCellState eState = eWorldCellState::Unloaded;
        eWorldCellState eDesiredState = eWorldCellState::Unloaded;
        eWorldCellTransitionReason eLastReason = eWorldCellTransitionReason::None;
        std::vector<AssetHandle> vecAssetHandles;
        std::vector<AssetHandle> vecInstanceMeshHandles;
        std::vector<AssetHandle> vecInstanceMaterialHandles;
        bool_t bHandlesRequested = false;
        bool_t bRequiredReady = false;
        u32_t uTransitionCount = 0u;
        u32_t uMissingRequiredAssets = 0u;
        u32_t uMissingOptionalAssets = 0u;
    };

    struct CellStateCounts
    {
        u32_t uUnloaded = 0u;
        u32_t uQueued = 0u;
        u32_t uLoadedHidden = 0u;
        u32_t uVisible = 0u;
    };

    struct WorldPartitionDebugStats
    {
        CellStateCounts stateCounts;
        u32_t uCellCount = 0u;
        u32_t uSourceCount = 0u;
        u32_t uTotalTransitions = 0u;
        u32_t uMissingRequiredAssets = 0u;
        u32_t uMissingOptionalAssets = 0u;
        u32_t uSkippedNotPlaceableInstances = 0u;
        u32_t uSkippedMissingAssetInstances = 0u;
        u32_t uSkippedNotReadyInstances = 0u;
        u32_t uVisibleInstances = 0u;
    };

    struct VisibleInstance
    {
        const CellDescriptor* pCell = nullptr;
        const CellInstanceDesc* pInstance = nullptr;
        Mat4 matWorld;
    };

    class WINTERS_ENGINE CWorldPartitionSystem final
    {
    public:
        ~CWorldPartitionSystem();

        CWorldPartitionSystem(const CWorldPartitionSystem&) = delete;
        CWorldPartitionSystem& operator=(const CWorldPartitionSystem&) = delete;

        static std::unique_ptr<CWorldPartitionSystem> Create(CAssetStreamingSystem* pStreaming);

        bool_t LoadWorld(const std::string& strWorldJsonPath);
        void Unload();

        void SetSource(u32_t uSourceId, const StreamingSourceComponent& src);
        void RemoveSource(u32_t uSourceId);

        bool_t WorldToCellId(const Vec3& vWorld, std::string& outCellId) const;
        void Update(f32_t fDeltaTime);

        void CollectVisibleInstances(std::vector<VisibleInstance>& out) const;

        CellStateCounts GetStateCounts() const;
        WorldPartitionDebugStats GetDebugStats() const;
        const WorldDescriptor& GetDescriptor() const;
        const std::vector<WorldCellRuntime>& GetCells() const;

    private:
        CWorldPartitionSystem();
        bool_t Initialize(CAssetStreamingSystem* pStreaming);

        struct Impl;
        Impl* m_pImpl = nullptr;
    };
}
