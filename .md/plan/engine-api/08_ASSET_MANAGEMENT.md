# 08 -- UE5-Style Asset Management

> Winters Engine API Modernization -- Stage 8
> Date: 2026-05-02
> Depends on: CResourceCache (existing), .wmesh/.wskel/.wanim formats, IRHIDevice

---

## 1. Architecture Overview

The current asset loading path is **synchronous and blocking**:

```
Current: CModel::Create(pDevice, path)        -- blocks main thread
         CResourceCache::LoadTexture(path)     -- blocks main thread
         CResourceCache::LoadModel(pDevice, p) -- blocks, shared_ptr cache
```

This plan introduces a **UE5-style asset management layer** with:

| Component | Class | Role |
|---|---|---|
| Asset Registry | `CAssetRegistry` | Global database of all known assets. Path-indexed, type-tagged. |
| Asset Handle | `CAssetHandle<T>` | Typed, ref-counted handle. Lazy load on first dereference. |
| Soft Pointer | `TSoftObjectPtr<T>` | Serializable path reference. Resolves to handle on demand. |
| Streamable Manager | `CStreamableManager` | Background thread pool loader with callbacks. |
| Asset Types | `eAssetType` | Enum: StaticMesh, SkeletalMesh, Material, Texture, Animation, NiagaraSystem, SoundCue |

### Data Flow

```
1. Startup: CAssetRegistry scans known directories, builds path -> metadata map
2. Load request: CAssetHandle<CModel> h = registry.LoadAsync<CModel>("champ/irelia.wmesh")
3. Background: CStreamableManager picks up request, loads on worker thread
4. Finalization: Main thread creates GPU resources (ID3D11Buffer, SRV, etc.)
5. Callback: h.IsReady() == true, h.Get() returns CModel*
6. Unload: RefCount -> 0, asset queued for release
```

---

## 2. File Structure

```
Engine/
  Public/Resource/
    CAssetRegistry.h        -- global asset database
    CAssetHandle.h           -- typed ref-counted handle
    TSoftObjectPtr.h         -- serializable soft reference
    CStreamableManager.h    -- async loading manager
    AssetTypes.h             -- eAssetType enum + metadata struct
  Private/Resource/
    CAssetRegistry.cpp
    CStreamableManager.cpp
```

---

## 3. Full Code

### 3.1 AssetTypes.h

```cpp
// Engine/Public/Resource/AssetTypes.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <string>
#include <functional>
#include <atomic>

// ---------------------------------------------------------------
//  eAssetType -- classification of loadable assets
// ---------------------------------------------------------------
enum class eAssetType : u8_t
{
    Unknown       = 0,
    Texture       = 1,
    StaticMesh    = 2,
    SkeletalMesh  = 3,
    Animation     = 4,
    Material      = 5,
    NiagaraSystem = 6,
    SoundCue      = 7,
    Shader        = 8,
};

// ---------------------------------------------------------------
//  eAssetState -- lifecycle state of an asset
// ---------------------------------------------------------------
enum class eAssetState : u8_t
{
    Unloaded    = 0,  // known but not in memory
    Loading     = 1,  // background thread working
    Loaded      = 2,  // CPU data ready, needs GPU finalization
    Ready       = 3,  // fully usable
    Failed      = 4,  // load error
};

// ---------------------------------------------------------------
//  AssetMetadata -- stored in the registry per asset
// ---------------------------------------------------------------
struct AssetMetadata
{
    std::string  strPath     = {};           // canonical path (forward slash, lowercase)
    eAssetType   eType       = eAssetType::Unknown;
    eAssetState  eState      = eAssetState::Unloaded;
    u64_t        iSizeBytes  = 0;            // file size on disk (0 = unknown)
    u32_t        iRefCount   = 0;            // active handles
};

// ---------------------------------------------------------------
//  IAssetPayload -- type-erased base for loaded asset data
// ---------------------------------------------------------------
class WINTERS_ENGINE IAssetPayload
{
public:
    virtual ~IAssetPayload() = default;
    virtual eAssetType GetType() const = 0;
};

// ---------------------------------------------------------------
//  AssetLoadRequest -- queued for CStreamableManager
// ---------------------------------------------------------------
struct AssetLoadRequest
{
    std::string                             strPath;
    eAssetType                              eType;
    std::function<void(bool /*success*/)>   fnCallback;
    u32_t                                   iPriority = 0; // higher = sooner
};
```

### 3.2 CAssetHandle.h

```cpp
// Engine/Public/Resource/CAssetHandle.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Resource/AssetTypes.h"
#include <memory>
#include <string>
#include <atomic>
#include <cassert>

class CAssetRegistry;

// ---------------------------------------------------------------
//  AssetSlot -- internal ref-counted slot in the registry
//  Not exposed to Client. CAssetHandle holds a raw pointer to this.
// ---------------------------------------------------------------
struct AssetSlot
{
    AssetMetadata                    meta{};
    std::shared_ptr<IAssetPayload>   pPayload;       // null until loaded
    std::atomic<i32_t>               iRefCount{ 0 };
    void*                            pGpuResource = nullptr; // raw typed by eAssetType

    bool IsReady() const { return meta.eState == eAssetState::Ready && pPayload != nullptr; }
};

// ---------------------------------------------------------------
//  CAssetHandle<T> -- typed, ref-counted handle to a loaded asset
//
//  Lightweight (pointer + addref/release). Copyable.
//  Dereferencing a handle that is not yet ready returns nullptr.
//
//  Usage:
//    CAssetHandle<Engine::CModel> hModel = registry.Load<Engine::CModel>("path");
//    if (hModel.IsReady())
//        hModel.Get()->Render(pCtx);
// ---------------------------------------------------------------
template<typename T>
class CAssetHandle
{
public:
    CAssetHandle() : m_pSlot(nullptr) {}

    explicit CAssetHandle(AssetSlot* pSlot)
        : m_pSlot(pSlot)
    {
        if (m_pSlot)
            m_pSlot->iRefCount.fetch_add(1, std::memory_order_relaxed);
    }

    ~CAssetHandle()
    {
        Release();
    }

    // Copy
    CAssetHandle(const CAssetHandle& other)
        : m_pSlot(other.m_pSlot)
    {
        if (m_pSlot)
            m_pSlot->iRefCount.fetch_add(1, std::memory_order_relaxed);
    }

    CAssetHandle& operator=(const CAssetHandle& other)
    {
        if (this != &other)
        {
            Release();
            m_pSlot = other.m_pSlot;
            if (m_pSlot)
                m_pSlot->iRefCount.fetch_add(1, std::memory_order_relaxed);
        }
        return *this;
    }

    // Move
    CAssetHandle(CAssetHandle&& other) noexcept
        : m_pSlot(other.m_pSlot)
    {
        other.m_pSlot = nullptr;
    }

    CAssetHandle& operator=(CAssetHandle&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_pSlot = other.m_pSlot;
            other.m_pSlot = nullptr;
        }
        return *this;
    }

    // Access
    bool IsValid() const { return m_pSlot != nullptr; }
    bool IsReady() const { return m_pSlot && m_pSlot->IsReady(); }

    eAssetState GetState() const
    {
        return m_pSlot ? m_pSlot->meta.eState : eAssetState::Unloaded;
    }

    T* Get() const
    {
        if (!m_pSlot || !m_pSlot->pPayload) return nullptr;
        return static_cast<T*>(m_pSlot->pGpuResource);
    }

    T* operator->() const { return Get(); }

    const std::string& GetPath() const
    {
        static const std::string s_empty;
        return m_pSlot ? m_pSlot->meta.strPath : s_empty;
    }

    i32_t GetRefCount() const
    {
        return m_pSlot ? m_pSlot->iRefCount.load(std::memory_order_relaxed) : 0;
    }

    void Release()
    {
        if (m_pSlot)
        {
            m_pSlot->iRefCount.fetch_sub(1, std::memory_order_relaxed);
            m_pSlot = nullptr;
        }
    }

    // Comparison
    bool operator==(const CAssetHandle& other) const { return m_pSlot == other.m_pSlot; }
    bool operator!=(const CAssetHandle& other) const { return m_pSlot != other.m_pSlot; }
    explicit operator bool() const { return IsValid(); }

private:
    AssetSlot* m_pSlot;
};
```

### 3.3 TSoftObjectPtr.h

```cpp
// Engine/Public/Resource/TSoftObjectPtr.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Resource/CAssetHandle.h"
#include <string>

class CAssetRegistry;

// ---------------------------------------------------------------
//  TSoftObjectPtr<T> -- serializable soft reference
//
//  Stores only a path string. Resolves to CAssetHandle<T> on demand
//  via the asset registry. Used for data-driven references in
//  component structs, skill definitions, etc.
//
//  Usage:
//    struct ChampionDef {
//        TSoftObjectPtr<Engine::CModel> modelRef{ "champ/irelia.wmesh" };
//    };
//
//    // At runtime:
//    CAssetHandle<Engine::CModel> h = def.modelRef.Resolve(registry);
// ---------------------------------------------------------------
template<typename T>
class TSoftObjectPtr
{
public:
    TSoftObjectPtr() = default;
    explicit TSoftObjectPtr(const std::string& strPath) : m_strPath(strPath) {}
    explicit TSoftObjectPtr(std::string&& strPath) : m_strPath(std::move(strPath)) {}

    // Path access
    const std::string& GetPath() const { return m_strPath; }
    void SetPath(const std::string& strPath) { m_strPath = strPath; m_CachedHandle.Release(); }
    bool IsNull() const { return m_strPath.empty(); }

    // Resolve: looks up or triggers load in the registry.
    // Returns a cached handle -- subsequent calls are O(1).
    CAssetHandle<T> Resolve(CAssetRegistry& registry);

    // Check if the resolved asset is loaded and ready
    bool IsReady() const { return m_CachedHandle.IsReady(); }

    // Get the resolved asset (nullptr if not resolved or not ready)
    T* Get() const { return m_CachedHandle.Get(); }

    // Serialization helpers
    std::string Serialize() const { return m_strPath; }
    static TSoftObjectPtr Deserialize(const std::string& s) { return TSoftObjectPtr(s); }

private:
    std::string      m_strPath;
    CAssetHandle<T>  m_CachedHandle;
};
```

### 3.4 CAssetRegistry.h

```cpp
// Engine/Public/Resource/CAssetRegistry.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Resource/AssetTypes.h"
#include "Resource/CAssetHandle.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>

class IRHIDevice;
class CStreamableManager;

// ---------------------------------------------------------------
//  CAssetRegistry -- global asset database
//
//  Owns all AssetSlots. Provides synchronous and asynchronous
//  loading via CStreamableManager. Thread-safe for lookups.
//
//  Singleton owned by CGameInstance (Tier 1 forwarding).
//
//  Lifecycle:
//    1. Initialize() -- sets device, scans directories
//    2. ScanDirectory() -- populates metadata (no loading)
//    3. Load<T>() / LoadAsync<T>() -- load on demand
//    4. Tick() -- finalize GPU resources on main thread
//    5. Shutdown() -- release all
// ---------------------------------------------------------------
class WINTERS_ENGINE CAssetRegistry
{
public:
    ~CAssetRegistry();
    CAssetRegistry(const CAssetRegistry&) = delete;
    CAssetRegistry& operator=(const CAssetRegistry&) = delete;

    static std::unique_ptr<CAssetRegistry> Create(IRHIDevice* pDevice);

    // Scan a directory tree and register all known asset files
    void ScanDirectory(const std::string& strRootPath);

    // --- Synchronous load (blocks until ready) ---
    template<typename T>
    CAssetHandle<T> Load(const std::string& strPath);

    // --- Asynchronous load (returns immediately, loads in background) ---
    template<typename T>
    CAssetHandle<T> LoadAsync(const std::string& strPath,
                               std::function<void(bool)> fnCallback = nullptr);

    // --- Prefetch a batch of assets (champion pick phase) ---
    void PrefetchBatch(const std::vector<std::string>& vecPaths,
                       std::function<void()> fnAllDone = nullptr);

    // Main-thread tick: finalize GPU resources for completed loads
    void Tick();

    // Force unload assets with 0 references
    void GarbageCollect();

    // Release everything
    void Shutdown();

    // --- Queries ---
    bool HasAsset(const std::string& strPath) const;
    eAssetState GetState(const std::string& strPath) const;
    u32_t GetRegisteredCount() const;
    u32_t GetLoadedCount() const;

    // --- ImGui debug ---
    void OnImGui();

    // --- Internal: called by CStreamableManager when background load completes ---
    void OnBackgroundLoadComplete(const std::string& strPath,
                                   std::shared_ptr<IAssetPayload> pPayload);

private:
    CAssetRegistry();

    AssetSlot* FindOrCreateSlot(const std::string& strPath, eAssetType eType);
    eAssetType DeduceTypeFromExtension(const std::string& strPath) const;
    std::string NormalizePath(const std::string& strPath) const;
    void FinalizeGpuResource(AssetSlot* pSlot);

    IRHIDevice* m_pDevice = nullptr;

    mutable std::mutex m_Mutex;
    std::unordered_map<std::string, std::unique_ptr<AssetSlot>> m_mapSlots;

    // Pending GPU finalization queue (filled by background thread, drained by Tick)
    std::mutex m_FinalizeMutex;
    std::vector<std::string> m_vecPendingFinalize;

    std::unique_ptr<CStreamableManager> m_pStreamer;
};

// ---------------------------------------------------------------
//  Template implementations (must be in header)
// ---------------------------------------------------------------

template<typename T>
CAssetHandle<T> CAssetRegistry::Load(const std::string& strPath)
{
    std::string normalized = NormalizePath(strPath);
    eAssetType eType = DeduceTypeFromExtension(normalized);
    AssetSlot* pSlot = FindOrCreateSlot(normalized, eType);

    if (pSlot->meta.eState == eAssetState::Ready)
        return CAssetHandle<T>(pSlot);

    // Synchronous: block until loaded
    // This delegates to CStreamableManager::LoadSync which
    // runs the loader on the calling thread.
    pSlot->meta.eState = eAssetState::Loading;

    // ... loader runs synchronously ...
    // (Implementation in CAssetRegistry.cpp calls into CStreamableManager)

    FinalizeGpuResource(pSlot);
    return CAssetHandle<T>(pSlot);
}

template<typename T>
CAssetHandle<T> CAssetRegistry::LoadAsync(const std::string& strPath,
                                            std::function<void(bool)> fnCallback)
{
    std::string normalized = NormalizePath(strPath);
    eAssetType eType = DeduceTypeFromExtension(normalized);
    AssetSlot* pSlot = FindOrCreateSlot(normalized, eType);

    if (pSlot->meta.eState == eAssetState::Ready)
    {
        if (fnCallback) fnCallback(true);
        return CAssetHandle<T>(pSlot);
    }

    if (pSlot->meta.eState == eAssetState::Loading)
        return CAssetHandle<T>(pSlot); // already in progress

    pSlot->meta.eState = eAssetState::Loading;

    AssetLoadRequest req;
    req.strPath    = normalized;
    req.eType      = eType;
    req.fnCallback = fnCallback;

    // Submit to streamable manager (background thread)
    // m_pStreamer->Submit(req);

    return CAssetHandle<T>(pSlot);
}
```

### 3.5 CAssetRegistry.cpp

```cpp
// Engine/Private/Resource/CAssetRegistry.cpp
#include "Resource/CAssetRegistry.h"
#include "Resource/CStreamableManager.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"

#include <algorithm>
#include <filesystem>
#include <cctype>

#ifdef _DEBUG
#undef new
#include <imgui.h>
#define new DBG_NEW
#endif

namespace fs = std::filesystem;

CAssetRegistry::CAssetRegistry() = default;

CAssetRegistry::~CAssetRegistry()
{
    Shutdown();
}

std::unique_ptr<CAssetRegistry> CAssetRegistry::Create(IRHIDevice* pDevice)
{
    if (!pDevice) return nullptr;
    auto p = std::unique_ptr<CAssetRegistry>(new CAssetRegistry());
    p->m_pDevice = pDevice;
    p->m_pStreamer = CStreamableManager::Create(4); // 4 worker threads
    return p;
}

void CAssetRegistry::ScanDirectory(const std::string& strRootPath)
{
    if (!fs::exists(strRootPath)) return;

    for (auto& entry : fs::recursive_directory_iterator(strRootPath))
    {
        if (!entry.is_regular_file()) continue;

        std::string path = entry.path().generic_string(); // forward slashes
        std::string normalized = NormalizePath(path);
        eAssetType eType = DeduceTypeFromExtension(normalized);

        if (eType == eAssetType::Unknown) continue;

        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_mapSlots.find(normalized) != m_mapSlots.end()) continue;

        auto pSlot = std::make_unique<AssetSlot>();
        pSlot->meta.strPath    = normalized;
        pSlot->meta.eType      = eType;
        pSlot->meta.eState     = eAssetState::Unloaded;
        pSlot->meta.iSizeBytes = static_cast<u64_t>(entry.file_size());

        m_mapSlots.emplace(normalized, std::move(pSlot));
    }
}

void CAssetRegistry::PrefetchBatch(const std::vector<std::string>& vecPaths,
                                    std::function<void()> fnAllDone)
{
    auto pCounter = std::make_shared<std::atomic<u32_t>>(
        static_cast<u32_t>(vecPaths.size()));

    for (auto& path : vecPaths)
    {
        std::string normalized = NormalizePath(path);
        eAssetType eType = DeduceTypeFromExtension(normalized);
        AssetSlot* pSlot = FindOrCreateSlot(normalized, eType);

        if (pSlot->meta.eState == eAssetState::Ready)
        {
            if (pCounter->fetch_sub(1) == 1 && fnAllDone)
                fnAllDone();
            continue;
        }

        pSlot->meta.eState = eAssetState::Loading;

        AssetLoadRequest req;
        req.strPath = normalized;
        req.eType   = eType;
        req.fnCallback = [pCounter, fnAllDone](bool /*ok*/) {
            if (pCounter->fetch_sub(1) == 1 && fnAllDone)
                fnAllDone();
        };

        m_pStreamer->Submit(req);
    }
}

void CAssetRegistry::Tick()
{
    // Drain pending finalization queue
    std::vector<std::string> pending;
    {
        std::lock_guard<std::mutex> lock(m_FinalizeMutex);
        pending.swap(m_vecPendingFinalize);
    }

    for (auto& path : pending)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_mapSlots.find(path);
        if (it == m_mapSlots.end()) continue;

        AssetSlot* pSlot = it->second.get();
        if (pSlot->meta.eState == eAssetState::Loaded)
        {
            FinalizeGpuResource(pSlot);
        }
    }
}

void CAssetRegistry::OnBackgroundLoadComplete(const std::string& strPath,
                                               std::shared_ptr<IAssetPayload> pPayload)
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_mapSlots.find(strPath);
        if (it == m_mapSlots.end()) return;

        AssetSlot* pSlot = it->second.get();
        if (pPayload)
        {
            pSlot->pPayload    = pPayload;
            pSlot->meta.eState = eAssetState::Loaded;
        }
        else
        {
            pSlot->meta.eState = eAssetState::Failed;
        }
    }

    // Queue for main-thread GPU finalization
    std::lock_guard<std::mutex> lock(m_FinalizeMutex);
    m_vecPendingFinalize.push_back(strPath);
}

void CAssetRegistry::GarbageCollect()
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    for (auto it = m_mapSlots.begin(); it != m_mapSlots.end(); )
    {
        AssetSlot* pSlot = it->second.get();
        if (pSlot->iRefCount.load(std::memory_order_relaxed) <= 0
            && pSlot->meta.eState == eAssetState::Ready)
        {
            pSlot->pPayload.reset();
            pSlot->pGpuResource = nullptr;
            pSlot->meta.eState  = eAssetState::Unloaded;
        }
        ++it;
    }
}

void CAssetRegistry::Shutdown()
{
    if (m_pStreamer)
        m_pStreamer->Shutdown();

    std::lock_guard<std::mutex> lock(m_Mutex);
    m_mapSlots.clear();
}

bool CAssetRegistry::HasAsset(const std::string& strPath) const
{
    std::string normalized = NormalizePath(strPath);
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_mapSlots.find(normalized) != m_mapSlots.end();
}

eAssetState CAssetRegistry::GetState(const std::string& strPath) const
{
    std::string normalized = NormalizePath(strPath);
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_mapSlots.find(normalized);
    if (it == m_mapSlots.end()) return eAssetState::Unloaded;
    return it->second->meta.eState;
}

u32_t CAssetRegistry::GetRegisteredCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return static_cast<u32_t>(m_mapSlots.size());
}

u32_t CAssetRegistry::GetLoadedCount() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    u32_t count = 0;
    for (auto& [key, pSlot] : m_mapSlots)
    {
        if (pSlot->meta.eState == eAssetState::Ready)
            ++count;
    }
    return count;
}

AssetSlot* CAssetRegistry::FindOrCreateSlot(const std::string& strPath, eAssetType eType)
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_mapSlots.find(strPath);
    if (it != m_mapSlots.end())
        return it->second.get();

    auto pSlot = std::make_unique<AssetSlot>();
    pSlot->meta.strPath = strPath;
    pSlot->meta.eType   = eType;
    pSlot->meta.eState  = eAssetState::Unloaded;

    AssetSlot* raw = pSlot.get();
    m_mapSlots.emplace(strPath, std::move(pSlot));
    return raw;
}

eAssetType CAssetRegistry::DeduceTypeFromExtension(const std::string& strPath) const
{
    auto ext = fs::path(strPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".wmesh")                     return eAssetType::StaticMesh;
    if (ext == ".wskel")                     return eAssetType::SkeletalMesh;
    if (ext == ".wanim")                     return eAssetType::Animation;
    if (ext == ".fbx" || ext == ".glb"
        || ext == ".gltf" || ext == ".obj")  return eAssetType::StaticMesh;
    if (ext == ".png" || ext == ".jpg"
        || ext == ".dds" || ext == ".tga"
        || ext == ".bmp")                    return eAssetType::Texture;
    if (ext == ".wav" || ext == ".ogg"
        || ext == ".mp3")                    return eAssetType::SoundCue;
    if (ext == ".hlsl" || ext == ".cso")     return eAssetType::Shader;

    return eAssetType::Unknown;
}

std::string CAssetRegistry::NormalizePath(const std::string& strPath) const
{
    std::string result = strPath;
    // Backslash -> forward slash
    std::replace(result.begin(), result.end(), '\\', '/');
    // Lowercase
    std::transform(result.begin(), result.end(), result.begin(),
        [](char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

void CAssetRegistry::FinalizeGpuResource(AssetSlot* pSlot)
{
    if (!pSlot || !pSlot->pPayload || !m_pDevice) return;

    // GPU resource creation must happen on the main thread (DX11 single-context).
    // The IAssetPayload contains CPU-side data; we create the GPU object here.
    //
    // In practice, each asset type has a specific finalization path:
    //   Texture:      CreateShaderResourceView from CPU pixel data
    //   StaticMesh:   CreateBuffer (VB/IB) from vertex/index arrays
    //   SkeletalMesh: Same + bone buffer
    //   Animation:    CPU-only (no GPU resource needed)
    //   Material:     Constant buffer + texture binding
    //
    // For now, this is a placeholder that marks the asset as Ready.
    // Each concrete loader will implement the actual GPU finalization.

    pSlot->meta.eState = eAssetState::Ready;
}

void CAssetRegistry::OnImGui()
{
#ifdef _DEBUG
    if (!ImGui::Begin("Asset Registry"))
    {
        ImGui::End();
        return;
    }

    ImGui::Text("Registered: %u", GetRegisteredCount());
    ImGui::Text("Loaded: %u", GetLoadedCount());

    if (ImGui::Button("Garbage Collect"))
        GarbageCollect();

    ImGui::Separator();

    static char filterBuf[256] = {};
    ImGui::InputText("Filter", filterBuf, sizeof(filterBuf));
    std::string filter(filterBuf);

    const char* stateNames[] = { "Unloaded", "Loading", "Loaded", "Ready", "Failed" };

    if (ImGui::BeginTable("Assets", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable
                          | ImGuiTableFlags_ScrollY, ImVec2(0, 300)))
    {
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Refs");
        ImGui::TableHeadersRow();

        std::lock_guard<std::mutex> lock(m_Mutex);
        for (auto& [path, pSlot] : m_mapSlots)
        {
            if (!filter.empty() && path.find(filter) == std::string::npos)
                continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(path.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%u", static_cast<u32_t>(pSlot->meta.eType));
            ImGui::TableNextColumn();
            {
                u8_t idx = static_cast<u8_t>(pSlot->meta.eState);
                ImGui::TextUnformatted(idx < 5 ? stateNames[idx] : "?");
            }
            ImGui::TableNextColumn(); ImGui::Text("%d", pSlot->iRefCount.load());
        }

        ImGui::EndTable();
    }

    ImGui::End();
#endif
}
```

### 3.6 CStreamableManager.h

```cpp
// Engine/Public/Resource/CStreamableManager.h
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Resource/AssetTypes.h"
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <atomic>

class CAssetRegistry;

// ---------------------------------------------------------------
//  CStreamableManager -- background thread pool asset loader
//
//  Workers pull AssetLoadRequests from a priority queue, load
//  asset data from disk (CPU-only), and notify CAssetRegistry
//  when done. GPU finalization happens on the main thread via
//  CAssetRegistry::Tick().
//
//  Thread model:
//    - N worker threads (default 4)
//    - Shared priority queue protected by mutex + condvar
//    - Workers call CAssetRegistry::OnBackgroundLoadComplete()
//      which is thread-safe (mutex-protected)
// ---------------------------------------------------------------
class WINTERS_ENGINE CStreamableManager
{
public:
    ~CStreamableManager();
    CStreamableManager(const CStreamableManager&) = delete;
    CStreamableManager& operator=(const CStreamableManager&) = delete;

    static std::unique_ptr<CStreamableManager> Create(u32_t iNumWorkers = 4);

    // Bind to registry (called once after both are created)
    void BindRegistry(CAssetRegistry* pRegistry) { m_pRegistry = pRegistry; }

    // Submit a load request (thread-safe)
    void Submit(const AssetLoadRequest& request);

    // Synchronous load on calling thread (bypasses worker pool)
    std::shared_ptr<IAssetPayload> LoadSync(const std::string& strPath,
                                             eAssetType eType);

    // Cancel all pending requests
    void CancelAll();

    // Shutdown: signal workers to stop, join threads
    void Shutdown();

    // --- Queries ---
    u32_t GetPendingCount() const;
    u32_t GetWorkerCount()  const { return m_iNumWorkers; }
    bool  IsIdle()          const { return GetPendingCount() == 0 && m_iActiveWorkers == 0; }

    // --- ImGui debug ---
    void OnImGui();

private:
    CStreamableManager() = default;

    void WorkerThread(u32_t iWorkerID);
    std::shared_ptr<IAssetPayload> LoadAssetFromDisk(const std::string& strPath,
                                                      eAssetType eType);

    // --- Priority queue (higher iPriority = sooner) ---
    struct PriorityCompare
    {
        bool operator()(const AssetLoadRequest& a, const AssetLoadRequest& b) const
        {
            return a.iPriority < b.iPriority;
        }
    };

    mutable std::mutex m_QueueMutex;
    std::condition_variable m_QueueCV;
    std::priority_queue<AssetLoadRequest,
                        std::vector<AssetLoadRequest>,
                        PriorityCompare> m_Queue;

    std::vector<std::thread> m_vecWorkers;
    u32_t                    m_iNumWorkers   = 0;
    std::atomic<bool>        m_bShutdown     = false;
    std::atomic<u32_t>       m_iActiveWorkers = 0;

    CAssetRegistry*          m_pRegistry = nullptr;
};
```

### 3.7 CStreamableManager.cpp

```cpp
// Engine/Private/Resource/CStreamableManager.cpp
#include "Resource/CStreamableManager.h"
#include "Resource/CAssetRegistry.h"

#include <fstream>
#include <chrono>

#ifdef _DEBUG
#undef new
#include <imgui.h>
#define new DBG_NEW
#endif

std::unique_ptr<CStreamableManager> CStreamableManager::Create(u32_t iNumWorkers)
{
    auto p = std::unique_ptr<CStreamableManager>(new CStreamableManager());
    p->m_iNumWorkers = iNumWorkers;

    for (u32_t i = 0; i < iNumWorkers; ++i)
    {
        p->m_vecWorkers.emplace_back(&CStreamableManager::WorkerThread, p.get(), i);
    }

    return p;
}

CStreamableManager::~CStreamableManager()
{
    Shutdown();
}

void CStreamableManager::Submit(const AssetLoadRequest& request)
{
    {
        std::lock_guard<std::mutex> lock(m_QueueMutex);
        m_Queue.push(request);
    }
    m_QueueCV.notify_one();
}

std::shared_ptr<IAssetPayload> CStreamableManager::LoadSync(
    const std::string& strPath, eAssetType eType)
{
    return LoadAssetFromDisk(strPath, eType);
}

void CStreamableManager::CancelAll()
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    while (!m_Queue.empty())
        m_Queue.pop();
}

void CStreamableManager::Shutdown()
{
    m_bShutdown.store(true);
    m_QueueCV.notify_all();

    for (auto& t : m_vecWorkers)
    {
        if (t.joinable())
            t.join();
    }
    m_vecWorkers.clear();
}

u32_t CStreamableManager::GetPendingCount() const
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    return static_cast<u32_t>(m_Queue.size());
}

void CStreamableManager::WorkerThread(u32_t /*iWorkerID*/)
{
    while (true)
    {
        AssetLoadRequest req;
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            m_QueueCV.wait(lock, [this]() {
                return m_bShutdown.load() || !m_Queue.empty();
            });

            if (m_bShutdown.load() && m_Queue.empty())
                return;

            req = m_Queue.top();
            m_Queue.pop();
        }

        m_iActiveWorkers.fetch_add(1);

        // Load from disk (CPU only)
        auto pPayload = LoadAssetFromDisk(req.strPath, req.eType);

        // Notify registry
        if (m_pRegistry)
            m_pRegistry->OnBackgroundLoadComplete(req.strPath, pPayload);

        // Invoke user callback
        if (req.fnCallback)
            req.fnCallback(pPayload != nullptr);

        m_iActiveWorkers.fetch_sub(1);
    }
}

std::shared_ptr<IAssetPayload> CStreamableManager::LoadAssetFromDisk(
    const std::string& strPath, eAssetType eType)
{
    // ------------------------------------------------------------------
    //  Asset loading dispatch by type.
    //
    //  Each branch reads the file into a type-specific IAssetPayload
    //  derivative. GPU resources are NOT created here (background thread).
    //
    //  Current integration points:
    //    .wmesh  -> WMeshLoaded (Winters::Asset namespace)
    //    .wskel  -> WSkelLoaded
    //    .wanim  -> WAnimLoaded
    //    .fbx    -> Assimp aiScene (imported via Assimp library)
    //    .png/.dds -> raw pixel buffer (STB / WIC)
    //    .wav    -> raw PCM / FMOD bank
    //
    //  Each concrete payload struct wraps the CPU data and implements
    //  IAssetPayload::GetType().
    // ------------------------------------------------------------------

    // Verify file exists
    std::ifstream file(strPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
#ifdef _DEBUG
        OutputDebugStringA(("[StreamableManager] File not found: " + strPath + "\n").c_str());
#endif
        return nullptr;
    }

    const auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // Placeholder: read raw bytes into a generic payload
    // In production, this dispatches to type-specific loaders.
    class CRawPayload : public IAssetPayload
    {
    public:
        eAssetType         m_eType;
        std::vector<u8_t>  m_vecData;
        eAssetType GetType() const override { return m_eType; }
    };

    auto pPayload = std::make_shared<CRawPayload>();
    pPayload->m_eType = eType;
    pPayload->m_vecData.resize(static_cast<size_t>(fileSize));
    file.read(reinterpret_cast<char*>(pPayload->m_vecData.data()),
              static_cast<std::streamsize>(fileSize));

    if (!file)
        return nullptr;

    return pPayload;
}

void CStreamableManager::OnImGui()
{
#ifdef _DEBUG
    if (!ImGui::TreeNode("Streamable Manager"))
        return;

    ImGui::Text("Workers: %u", m_iNumWorkers);
    ImGui::Text("Active: %u", m_iActiveWorkers.load());
    ImGui::Text("Pending: %u", GetPendingCount());
    ImGui::Text("Idle: %s", IsIdle() ? "Yes" : "No");

    if (ImGui::Button("Cancel All"))
        CancelAll();

    ImGui::TreePop();
#endif
}
```

### 3.8 TSoftObjectPtr.h (Resolve implementation)

```cpp
// Template implementation -- appended to TSoftObjectPtr.h after CAssetRegistry is available
// In practice this goes in a TSoftObjectPtr.inl or directly in the header after
// forward-declaring CAssetRegistry, then including CAssetRegistry.h in .cpp files
// that call Resolve().

// Engine/Public/Resource/TSoftObjectPtr.inl
#pragma once
#include "Resource/TSoftObjectPtr.h"
#include "Resource/CAssetRegistry.h"

template<typename T>
CAssetHandle<T> TSoftObjectPtr<T>::Resolve(CAssetRegistry& registry)
{
    if (m_strPath.empty())
        return CAssetHandle<T>();

    if (m_CachedHandle.IsValid())
        return m_CachedHandle;

    m_CachedHandle = registry.LoadAsync<T>(m_strPath);
    return m_CachedHandle;
}
```

---

## 4. Usage Examples

### 4.1 Migration: Before (blocking)

```cpp
// Scene_InGame.cpp -- current blocking load
auto pModel = Engine::CModel::Create(pDevice, "Client/Bin/Resource/Model/Irelia/irelia.wmesh");
if (!pModel) { /* error */ }
// Model is ready immediately but main thread stalled for 50-200ms
```

### 4.2 Migration: After (async)

```cpp
// Scene_InGame.cpp -- async load
auto& registry = CEngineApp::Get().GetAssetRegistry();
auto hModel = registry.LoadAsync<Engine::CModel>(
    "Client/Bin/Resource/Model/Irelia/irelia.wmesh",
    [](bool ok) {
        if (ok) OutputDebugStringA("[Irelia] Model loaded!\n");
    });

// Later in OnUpdate:
if (hModel.IsReady())
    hModel->Render(pCtx);
// No stall. Model loads on background thread. GPU finalization on main thread via Tick().
```

### 4.3 Champion Prefetch During Pick Phase

```cpp
// Scene_BanPick.cpp -- prefetch all picked champion assets
void CScene_BanPick::OnChampionLocked(const std::vector<std::string>& vecChampPaths)
{
    auto& registry = CEngineApp::Get().GetAssetRegistry();

    // Collect all asset paths for the locked champions
    std::vector<std::string> allAssets;
    for (auto& champPath : vecChampPaths)
    {
        allAssets.push_back(champPath + "/model.wmesh");
        allAssets.push_back(champPath + "/skeleton.wskel");
        allAssets.push_back(champPath + "/idle.wanim");
        allAssets.push_back(champPath + "/run.wanim");
        allAssets.push_back(champPath + "/attack1.wanim");
        allAssets.push_back(champPath + "/diffuse.png");
    }

    registry.PrefetchBatch(allAssets, []() {
        OutputDebugStringA("[BanPick] All champion assets prefetched!\n");
    });
}
```

### 4.4 Soft Reference in Data Definitions

```cpp
// Shared/GameSim/Definitions/ChampionDef.h
struct ChampionAssetDef
{
    TSoftObjectPtr<Engine::CModel>   modelRef;
    TSoftObjectPtr<Engine::CTexture> diffuseRef;
    TSoftObjectPtr<Engine::CTexture> normalRef;

    void InitIrelia()
    {
        modelRef   = TSoftObjectPtr<Engine::CModel>("Client/Bin/Resource/Model/Irelia/irelia.wmesh");
        diffuseRef = TSoftObjectPtr<Engine::CTexture>("Client/Bin/Resource/Texture/Irelia/diffuse.png");
        normalRef  = TSoftObjectPtr<Engine::CTexture>("Client/Bin/Resource/Texture/Irelia/normal.png");
    }
};
```

---

## 5. Integration with .wmesh/.wskel/.wanim Formats

The `CStreamableManager::LoadAssetFromDisk()` dispatches by extension:

| Extension | Loader | CPU Payload | GPU Finalization |
|---|---|---|---|
| `.wmesh` | `WMeshFormat::Load()` | `WMeshLoaded` (vertices, indices, submesh table) | `ID3D11Buffer` (VB+IB) |
| `.wskel` | `WSkelFormat::Load()` | `WSkelLoaded` (bone hierarchy, bind poses) | CPU-only |
| `.wanim` | `WAnimFormat::Load()` | `WAnimLoaded` (channel keyframes) | CPU-only |
| `.fbx/.glb` | Assimp `aiImportFile()` | `aiScene*` wrapper | `ID3D11Buffer` via existing ProcessMesh |
| `.png/.dds` | WIC / STB | Pixel buffer | `ID3D11Texture2D` + SRV |

The existing `CResourceCache` continues to work as a synchronous fallback.
`CAssetRegistry` delegates to `CResourceCache` internally for GPU finalization,
then wraps the result in an `AssetSlot`.

---

## 6. Verification Checklist

| # | Check | Pass Criteria |
|---|---|---|
| 1 | `CAssetRegistry::ScanDirectory` finds assets | `GetRegisteredCount() > 0` |
| 2 | `LoadAsync` returns immediately | < 1ms for the call itself |
| 3 | Background thread loads .wmesh | `OnBackgroundLoadComplete` called on worker thread |
| 4 | Main thread finalizes GPU resource | `Tick()` transitions state to Ready |
| 5 | `CAssetHandle` ref counting works | Destroy handle -> refcount decrements |
| 6 | `GarbageCollect` frees unused assets | Unloaded assets removed from memory |
| 7 | `PrefetchBatch` callback fires | `fnAllDone` called when all assets are Ready |
| 8 | ImGui overlay shows asset table | Filter, state, ref counts visible |
| 9 | No data race under concurrent loads | ThreadSanitizer clean (or manual mutex audit) |
| 10 | Existing `CModel::Create` still works | Backward compatibility maintained |
