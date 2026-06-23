#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"

#include <memory>
#include <string>

namespace Engine
{
    using AssetHandle = u32_t;
    constexpr AssetHandle kInvalidAssetHandle = 0u;

    enum class eAssetKind : u8_t
    {
        Mesh = 0,
        Material,
        Texture,
        WorldCell,
        Unknown
    };

    enum class eAssetState : u8_t
    {
        Unloaded = 0,
        Queued,
        Ready,
        Failed
    };

    struct AssetLoadRequest
    {
        std::string strPath;
        u64_t uPathHash = 0ull;
        eAssetKind eKind = eAssetKind::Mesh;
        u32_t uPriority = 0u;

        // S0-S3 baseline: no disk load. Ready requests become ready deterministically.
        bool_t bReadyImmediately = true;
    };

    struct AssetStreamingStats
    {
        u32_t uHandleCount = 0u;
        u32_t uUnloaded = 0u;
        u32_t uQueued = 0u;
        u32_t uReady = 0u;
        u32_t uFailed = 0u;
        u32_t uTotalRefs = 0u;
    };

    class WINTERS_ENGINE CAssetStreamingSystem final
    {
    public:
        ~CAssetStreamingSystem();

        CAssetStreamingSystem(const CAssetStreamingSystem&) = delete;
        CAssetStreamingSystem& operator=(const CAssetStreamingSystem&) = delete;

        static std::unique_ptr<CAssetStreamingSystem> Create();

        AssetHandle Request(const AssetLoadRequest& req);
        void AddRef(AssetHandle hAsset);
        void Release(AssetHandle hAsset);

        eAssetState GetState(AssetHandle hAsset) const;
        bool_t IsReady(AssetHandle hAsset) const;
        AssetStreamingStats Stats() const;
        static const char* GetStateName(eAssetState eState);

    private:
        CAssetStreamingSystem();

        struct Impl;
        Impl* m_pImpl = nullptr;
    };
}
