#include "WintersPCH.h"

#include "World/AssetStreamingSystem.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Engine
{
    namespace
    {
        u64_t HashPath(const std::string& strPath)
        {
            u64_t uHash = 1469598103934665603ull;
            for (const char ch : strPath)
            {
                uHash ^= static_cast<u8_t>(ch);
                uHash *= 1099511628211ull;
            }
            return uHash ? uHash : 1ull;
        }
    }

    struct CAssetStreamingSystem::Impl
    {
        struct Entry
        {
            AssetHandle hAsset = kInvalidAssetHandle;
            AssetLoadRequest req;
            eAssetState eState = eAssetState::Unloaded;
            u32_t uRefCount = 0u;
        };

        AssetHandle hNext = 1u;
        std::vector<Entry> vecEntries;
        std::unordered_map<std::string, AssetHandle> mapHandleByPath;

        Entry* Find(AssetHandle hAsset)
        {
            auto it = std::find_if(vecEntries.begin(), vecEntries.end(),
                [hAsset](const Entry& entry) { return entry.hAsset == hAsset; });
            return it == vecEntries.end() ? nullptr : &(*it);
        }

        const Entry* Find(AssetHandle hAsset) const
        {
            auto it = std::find_if(vecEntries.begin(), vecEntries.end(),
                [hAsset](const Entry& entry) { return entry.hAsset == hAsset; });
            return it == vecEntries.end() ? nullptr : &(*it);
        }
    };

    CAssetStreamingSystem::CAssetStreamingSystem()
        : m_pImpl(new Impl())
    {
    }

    CAssetStreamingSystem::~CAssetStreamingSystem()
    {
        delete m_pImpl;
        m_pImpl = nullptr;
    }

    std::unique_ptr<CAssetStreamingSystem> CAssetStreamingSystem::Create()
    {
        return std::unique_ptr<CAssetStreamingSystem>(new CAssetStreamingSystem());
    }

    AssetHandle CAssetStreamingSystem::Request(const AssetLoadRequest& req)
    {
        if (!m_pImpl || req.strPath.empty())
            return kInvalidAssetHandle;

        const u64_t uHash = req.uPathHash ? req.uPathHash : HashPath(req.strPath);
        auto found = m_pImpl->mapHandleByPath.find(req.strPath);
        if (found != m_pImpl->mapHandleByPath.end())
        {
            Impl::Entry* pEntry = m_pImpl->Find(found->second);
            if (!pEntry)
                return kInvalidAssetHandle;

            ++pEntry->uRefCount;
            if (pEntry->eState == eAssetState::Unloaded)
                pEntry->eState = req.bReadyImmediately ? eAssetState::Ready : eAssetState::Queued;
            return found->second;
        }

        Impl::Entry entry{};
        entry.hAsset = m_pImpl->hNext++;
        entry.req = req;
        entry.req.uPathHash = uHash;
        entry.eState = req.bReadyImmediately ? eAssetState::Ready : eAssetState::Queued;
        entry.uRefCount = 1u;

        m_pImpl->mapHandleByPath.emplace(req.strPath, entry.hAsset);
        m_pImpl->vecEntries.push_back(entry);
        return entry.hAsset;
    }

    void CAssetStreamingSystem::AddRef(AssetHandle hAsset)
    {
        if (!m_pImpl || hAsset == kInvalidAssetHandle)
            return;

        Impl::Entry* pEntry = m_pImpl->Find(hAsset);
        if (!pEntry)
            return;

        ++pEntry->uRefCount;
    }

    void CAssetStreamingSystem::Release(AssetHandle hAsset)
    {
        if (!m_pImpl || hAsset == kInvalidAssetHandle)
            return;

        Impl::Entry* pEntry = m_pImpl->Find(hAsset);
        if (!pEntry || pEntry->uRefCount == 0u)
            return;

        --pEntry->uRefCount;
        if (pEntry->uRefCount == 0u)
            pEntry->eState = eAssetState::Unloaded;
    }

    eAssetState CAssetStreamingSystem::GetState(AssetHandle hAsset) const
    {
        if (!m_pImpl || hAsset == kInvalidAssetHandle)
            return eAssetState::Unloaded;

        const Impl::Entry* pEntry = m_pImpl->Find(hAsset);
        return pEntry ? pEntry->eState : eAssetState::Unloaded;
    }

    bool_t CAssetStreamingSystem::IsReady(AssetHandle hAsset) const
    {
        return GetState(hAsset) == eAssetState::Ready;
    }

    const char* CAssetStreamingSystem::GetStateName(eAssetState eState)
    {
        switch (eState)
        {
        case eAssetState::Unloaded: return "Unloaded";
        case eAssetState::Queued: return "Queued";
        case eAssetState::Ready: return "Ready";
        case eAssetState::Failed: return "Failed";
        default: return "Unknown";
        }
    }

    AssetStreamingStats CAssetStreamingSystem::Stats() const
    {
        AssetStreamingStats stats{};
        if (!m_pImpl)
            return stats;

        stats.uHandleCount = static_cast<u32_t>(m_pImpl->vecEntries.size());

        for (const Impl::Entry& entry : m_pImpl->vecEntries)
        {
            stats.uTotalRefs += entry.uRefCount;
            switch (entry.eState)
            {
            case eAssetState::Unloaded:
                ++stats.uUnloaded;
                break;
            case eAssetState::Queued:
                ++stats.uQueued;
                break;
            case eAssetState::Ready:
                ++stats.uReady;
                break;
            case eAssetState::Failed:
                ++stats.uFailed;
                break;
            default:
                break;
            }
        }

        return stats;
    }
}
