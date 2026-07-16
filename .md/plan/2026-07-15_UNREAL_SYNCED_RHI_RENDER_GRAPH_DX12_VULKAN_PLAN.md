Session - Unreal Engine 5.7.4의 Dynamic RHI·D3D12 barrier·Vulkan barrier2·RDG 실코드와 Winters 현재 구현을 맞춰, 강제 backend 선택을 진실화하고 DX12 state transition 및 snapshot-only 미니 Render Graph를 완성하며 Vulkan은 loader와 compiled backend 경계를 거짓 없이 분리한다.

> [!NOTE]
> 이 파일은 읽기용 해설서가 아니라, 구현자가 그대로 비교·적용할 수 있도록 전체 코드와 교체 블록을 보존한 **코드 적용 사양서**다. 흐름을 먼저 이해하려면 [읽기용 아키텍처 해설](../architecture/WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_ARCHITECTURE.md)을 열고, 이 문서는 실제 구현 단계에서 필요한 항목만 찾아보는 편이 좋다. VS Code에서는 `Ctrl+Shift+V`로 Markdown 미리보기, `Ctrl+K V`로 옆쪽 미리보기, `Ctrl+Shift+O`로 제목 탐색을 사용할 수 있다.

빠른 이동: [검증 하네스](#1-1-cusersuserdesktopwinterstoolsharnessrun-s17rhivalidationps1) · [Backend 모듈](#1-2-cusersuserdesktopwintersengineprivaterhiirhibackendmoduleh) · [Vulkan probe](#1-3-cusersuserdesktopwintersengineprivaterhivulkanvulkanruntimeprobeh) · [Backend registry](#1-5-cusersuserdesktopwintersengineprivaterhirhibackendregistryh) · [Capabilities](#1-7-cusersuserdesktopwintersenginepublicrhirhicapabilitiesh) · [Render Graph](#1-9-cusersuserdesktopwintersengineprivaterendererrendergraphrendergraphh) · [Scene renderer](#1-11-cusersuserdesktopwintersengineprivaterendererrhiscenerenderercpp) · [DX12 transition](#1-16-cusersuserdesktopwintersengineprivaterhidx12dx12devicecpp) · [검증](#2-검증)

## 1. 반영해야 하는 코드

### 1-1. C:/Users/user/Desktop/Winters/Tools/Harness/Run-S17RhiValidation.ps1

기존 코드:

```powershell
        @{ Name = "WintersElden_probe_dx12"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe"); Cwd = "EldenRingClient\Bin\$Configuration" },
```

아래로 교체:

```powershell
        @{ Name = "WintersElden_probe_dx12"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe", "--rhi=dx12"); Cwd = "EldenRingClient\Bin\$Configuration" },
```

### 1-2. C:/Users/user/Desktop/Winters/Engine/Private/RHI/IRHIBackendModule.h

새 파일:

```cpp
#pragma once

#include "EngineConfig.h"
#include "RHI/IRHIDevice.h"
#include "RHI/RHISurface.h"

#include <memory>

enum class eRHIBackendProbeCode : u32_t
{
    Supported = 0,
    InvalidSurface,
    RuntimeUnavailable,
    BackendNotCompiled,
    DeviceCreationFailed,
};

struct RHIBackendProbeResult
{
    bool_t bSupported = false;
    eRHIBackendProbeCode Code = eRHIBackendProbeCode::BackendNotCompiled;
    char Message[192]{};
};

struct RHIBackendCreateRequest
{
    RHISurfaceDesc Surface{};
};

class IRHIBackendModule
{
public:
    virtual ~IRHIBackendModule() = default;

    virtual eEngineRHIBackend GetEngineBackend() const = 0;
    virtual eRHIBackend GetRHIBackend() const = 0;
    virtual const char* GetName() const = 0;
    virtual RHIBackendProbeResult ProbeSupport(
        const RHIBackendCreateRequest& request) const = 0;
    virtual std::unique_ptr<IRHIDevice> CreateDevice(
        const RHIBackendCreateRequest& request) const = 0;
};
```

### 1-3. C:/Users/user/Desktop/Winters/Engine/Private/RHI/Vulkan/VulkanRuntimeProbe.h

새 파일:

```cpp
#pragma once

#include "WintersTypes.h"

struct VulkanRuntimeProbeResult
{
    bool_t bLoaderFound = false;
    bool_t bVersionQuerySucceeded = false;
    u32_t MajorVersion = 0;
    u32_t MinorVersion = 0;
    u32_t PatchVersion = 0;
    char Message[192]{};
};

VulkanRuntimeProbeResult ProbeVulkanRuntime();
```

### 1-4. C:/Users/user/Desktop/Winters/Engine/Private/RHI/Vulkan/VulkanRuntimeProbe.cpp

새 파일:

```cpp
#include "WintersPCH.h"

#include "RHI/Vulkan/VulkanRuntimeProbe.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cstdio>

namespace
{
    using EnumerateInstanceVersionFunction = int32_t(WINAPI*)(u32_t*);

    constexpr int32_t kVulkanSuccess = 0;

    void DecodeVulkanVersion(
        u32_t packedVersion,
        u32_t& majorVersion,
        u32_t& minorVersion,
        u32_t& patchVersion)
    {
        majorVersion = packedVersion >> 22u;
        minorVersion = (packedVersion >> 12u) & 0x3FFu;
        patchVersion = packedVersion & 0xFFFu;
    }
}

VulkanRuntimeProbeResult ProbeVulkanRuntime()
{
    VulkanRuntimeProbeResult result{};

    HMODULE hLoader = LoadLibraryW(L"vulkan-1.dll");
    if (!hLoader)
    {
        sprintf_s(
            result.Message,
            "Vulkan loader not found (GetLastError=%lu)",
            static_cast<unsigned long>(GetLastError()));
        return result;
    }

    result.bLoaderFound = true;

    const auto enumerateInstanceVersion =
        reinterpret_cast<EnumerateInstanceVersionFunction>(
            GetProcAddress(hLoader, "vkEnumerateInstanceVersion"));

    if (!enumerateInstanceVersion)
    {
        result.MajorVersion = 1;
        result.MinorVersion = 0;
        result.PatchVersion = 0;
        result.bVersionQuerySucceeded = true;
        sprintf_s(result.Message, "Vulkan 1.0 loader found");
        FreeLibrary(hLoader);
        return result;
    }

    u32_t packedVersion = 0;
    const int32_t queryResult = enumerateInstanceVersion(&packedVersion);
    if (queryResult != kVulkanSuccess)
    {
        sprintf_s(
            result.Message,
            "vkEnumerateInstanceVersion failed (VkResult=%d)",
            queryResult);
        FreeLibrary(hLoader);
        return result;
    }

    DecodeVulkanVersion(
        packedVersion,
        result.MajorVersion,
        result.MinorVersion,
        result.PatchVersion);
    result.bVersionQuerySucceeded = true;
    sprintf_s(
        result.Message,
        "Vulkan loader %u.%u.%u found",
        result.MajorVersion,
        result.MinorVersion,
        result.PatchVersion);

    FreeLibrary(hLoader);
    return result;
}
```

### 1-5. C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHIBackendRegistry.h

새 파일:

```cpp
#pragma once

#include "EngineConfig.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIBackendModule.h"
#include "RHI/RHISurface.h"

#include <memory>

struct RHIBackendSelectionRequest
{
    eEngineRHIBackend Requested = eEngineRHIBackend::Auto;
    RHISurfaceDesc Surface{};
    bool_t bAllowAutoFallback = true;
};

struct RHIBackendSelectionResult
{
    std::unique_ptr<IRHIDevice> pDevice{};
    eEngineRHIBackend Requested = eEngineRHIBackend::Auto;
    eEngineRHIBackend Selected = eEngineRHIBackend::Null;
    bool_t bUsedFallback = false;
    RHIBackendProbeResult Probe{};
};

class CRHIBackendRegistry final
{
public:
    static RHIBackendSelectionResult Select(
        const RHIBackendSelectionRequest& request);
};
```

### 1-6. C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHIBackendRegistry.cpp

새 파일:

```cpp
#include "WintersPCH.h"

#include "RHI/RHIBackendRegistry.h"

#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX12/DX12Device.h"
#include "RHI/Vulkan/VulkanRuntimeProbe.h"

#include <cstdio>
#include <cstring>
#include <utility>

namespace
{
    RHIBackendProbeResult MakeProbeResult(
        bool_t bSupported,
        eRHIBackendProbeCode code,
        const char* pMessage)
    {
        RHIBackendProbeResult result{};
        result.bSupported = bSupported;
        result.Code = code;
        sprintf_s(
            result.Message,
            "%s",
            pMessage ? pMessage : "No diagnostic message");
        return result;
    }

    const char* GetEngineBackendName(eEngineRHIBackend backend)
    {
        switch (backend)
        {
        case eEngineRHIBackend::Auto: return "Auto";
        case eEngineRHIBackend::DX11: return "DX11";
        case eEngineRHIBackend::DX12: return "DX12";
        case eEngineRHIBackend::Vulkan: return "Vulkan";
        case eEngineRHIBackend::Metal: return "Metal";
        case eEngineRHIBackend::Xbox: return "Xbox";
        case eEngineRHIBackend::PS5: return "PS5";
        case eEngineRHIBackend::Null: return "Null";
        default: return "Unknown";
        }
    }

    bool_t ValidateWin32Surface(
        const RHIBackendCreateRequest& request,
        RHIBackendProbeResult& result)
    {
        if (request.Surface.type != eRHIPlatformSurfaceType::Win32HWND ||
            !request.Surface.nativeHandle ||
            request.Surface.width == 0 ||
            request.Surface.height == 0)
        {
            result = MakeProbeResult(
                false,
                eRHIBackendProbeCode::InvalidSurface,
                "Win32 backend requires a valid HWND and non-zero extent");
            return false;
        }

        return true;
    }

    class CDX11BackendModule final : public IRHIBackendModule
    {
    public:
        eEngineRHIBackend GetEngineBackend() const override
        {
            return eEngineRHIBackend::DX11;
        }

        eRHIBackend GetRHIBackend() const override
        {
            return eRHIBackend::DX11;
        }

        const char* GetName() const override
        {
            return "DX11";
        }

        RHIBackendProbeResult ProbeSupport(
            const RHIBackendCreateRequest& request) const override
        {
            RHIBackendProbeResult result{};
            if (!ValidateWin32Surface(request, result))
                return result;

            return MakeProbeResult(
                true,
                eRHIBackendProbeCode::Supported,
                "DX11 module compiled for Win32");
        }

        std::unique_ptr<IRHIDevice> CreateDevice(
            const RHIBackendCreateRequest& request) const override
        {
            DeviceDesc desc{};
            desc.hwnd = static_cast<HWND>(request.Surface.nativeHandle);
            desc.width = request.Surface.width;
            desc.height = request.Surface.height;
            desc.vsync = request.Surface.vsync;
            desc.fullscreen = request.Surface.fullscreen;
            return CDX11Device::Create(desc);
        }
    };

    class CDX12BackendModule final : public IRHIBackendModule
    {
    public:
        eEngineRHIBackend GetEngineBackend() const override
        {
            return eEngineRHIBackend::DX12;
        }

        eRHIBackend GetRHIBackend() const override
        {
            return eRHIBackend::DX12;
        }

        const char* GetName() const override
        {
            return "DX12";
        }

        RHIBackendProbeResult ProbeSupport(
            const RHIBackendCreateRequest& request) const override
        {
            RHIBackendProbeResult result{};
            if (!ValidateWin32Surface(request, result))
                return result;

            return MakeProbeResult(
                true,
                eRHIBackendProbeCode::Supported,
                "DX12 module compiled for Win32; device creation is the adapter probe");
        }

        std::unique_ptr<IRHIDevice> CreateDevice(
            const RHIBackendCreateRequest& request) const override
        {
            DX12DeviceDesc desc{};
            desc.hwnd = static_cast<HWND>(request.Surface.nativeHandle);
            desc.width = request.Surface.width;
            desc.height = request.Surface.height;
            desc.vsync = request.Surface.vsync;
            desc.fullscreen = request.Surface.fullscreen;
            return CDX12Device::Create(desc);
        }
    };

    class CVulkanBackendModule final : public IRHIBackendModule
    {
    public:
        eEngineRHIBackend GetEngineBackend() const override
        {
            return eEngineRHIBackend::Vulkan;
        }

        eRHIBackend GetRHIBackend() const override
        {
            return eRHIBackend::Vulkan;
        }

        const char* GetName() const override
        {
            return "Vulkan";
        }

        RHIBackendProbeResult ProbeSupport(
            const RHIBackendCreateRequest& request) const override
        {
            RHIBackendProbeResult surfaceResult{};
            if (!ValidateWin32Surface(request, surfaceResult))
                return surfaceResult;

            const VulkanRuntimeProbeResult runtime = ProbeVulkanRuntime();
            if (!runtime.bLoaderFound || !runtime.bVersionQuerySucceeded)
            {
                return MakeProbeResult(
                    false,
                    eRHIBackendProbeCode::RuntimeUnavailable,
                    runtime.Message);
            }

            char message[192]{};
            sprintf_s(
                message,
                "%s, but Winters Vulkan backend is not compiled",
                runtime.Message);
            return MakeProbeResult(
                false,
                eRHIBackendProbeCode::BackendNotCompiled,
                message);
        }

        std::unique_ptr<IRHIDevice> CreateDevice(
            const RHIBackendCreateRequest&) const override
        {
            return nullptr;
        }
    };

    IRHIBackendModule* FindModule(eEngineRHIBackend backend)
    {
        static CDX11BackendModule dx11Module;
        static CDX12BackendModule dx12Module;
        static CVulkanBackendModule vulkanModule;

        switch (backend)
        {
        case eEngineRHIBackend::DX11: return &dx11Module;
        case eEngineRHIBackend::DX12: return &dx12Module;
        case eEngineRHIBackend::Vulkan: return &vulkanModule;
        default: return nullptr;
        }
    }

    void LogProbe(
        eEngineRHIBackend requested,
        eEngineRHIBackend candidate,
        const RHIBackendProbeResult& probe)
    {
        char message[512]{};
        sprintf_s(
            message,
            "[RHIBackendRegistry] requested=%s candidate=%s supported=%u code=%u reason=%s\n",
            GetEngineBackendName(requested),
            GetEngineBackendName(candidate),
            probe.bSupported ? 1u : 0u,
            static_cast<u32_t>(probe.Code),
            probe.Message);
        OutputDebugStringA(message);
    }
}

RHIBackendSelectionResult CRHIBackendRegistry::Select(
    const RHIBackendSelectionRequest& request)
{
    RHIBackendSelectionResult result{};
    result.Requested = request.Requested;

    RHIBackendCreateRequest createRequest{};
    createRequest.Surface = request.Surface;

    const auto TryCandidate = [&](
        eEngineRHIBackend candidate,
        bool_t bFallback) -> bool_t
    {
        IRHIBackendModule* pModule = FindModule(candidate);
        if (!pModule)
        {
            result.Probe = MakeProbeResult(
                false,
                eRHIBackendProbeCode::BackendNotCompiled,
                "Requested backend module is not compiled");
            LogProbe(request.Requested, candidate, result.Probe);
            return false;
        }

        result.Probe = pModule->ProbeSupport(createRequest);
        LogProbe(request.Requested, candidate, result.Probe);
        if (!result.Probe.bSupported)
            return false;

        std::unique_ptr<IRHIDevice> pDevice = pModule->CreateDevice(createRequest);
        if (!pDevice)
        {
            result.Probe = MakeProbeResult(
                false,
                eRHIBackendProbeCode::DeviceCreationFailed,
                "Backend probe passed but device creation failed");
            LogProbe(request.Requested, candidate, result.Probe);
            return false;
        }

        result.pDevice = std::move(pDevice);
        result.Selected = candidate;
        result.bUsedFallback = bFallback;

        char message[256]{};
        sprintf_s(
            message,
            "[RHIBackendRegistry] selected=%s fallback=%u\n",
            pModule->GetName(),
            bFallback ? 1u : 0u);
        OutputDebugStringA(message);
        return true;
    };

    if (request.Requested != eEngineRHIBackend::Auto)
    {
        TryCandidate(request.Requested, false);
        return result;
    }

    if (TryCandidate(eEngineRHIBackend::DX11, false))
        return result;

    if (request.bAllowAutoFallback)
        TryCandidate(eEngineRHIBackend::DX12, true);

    return result;
}
```

### 1-7. C:/Users/user/Desktop/Winters/Engine/Public/RHI/RHICapabilities.h

기존 코드:

```cpp
inline RHICapabilities RHI_MakeDefaultCapabilities(eRHIBackend backend)
{
    RHICapabilities caps{};
    caps.backend = backend;

    switch (backend)
    {
    case eRHIBackend::DX11:
        caps.featureTier = eRHIFeatureTier::LegacyDX11;
        caps.supportsCompute = true;
        caps.maxFramesInFlight = 1;
        caps.maxSampledTexturesPerStage = 16;
        caps.maxSamplersPerStage = 16;
        break;

    case eRHIBackend::DX12:
    case eRHIBackend::Vulkan:
        caps.featureTier = eRHIFeatureTier::ExplicitDesktop;
        caps.supportsCompute = true;
        caps.supportsAsyncCompute = true;
        caps.supportsBindless = true;
        caps.requiresExplicitResourceStates = true;
        caps.prefersRenderPassLoadStore = true;
        caps.maxFramesInFlight = 3;
        caps.maxSampledTexturesPerStage = 128;
        caps.maxSamplersPerStage = 32;
        break;

    case eRHIBackend::Metal:
        caps.featureTier = eRHIFeatureTier::MobileTiled;
        caps.supportsCompute = true;
        caps.supportsAsyncCompute = true;
        caps.supportsUnifiedMemory = true;
        caps.requiresExplicitResourceStates = true;
        caps.prefersRenderPassLoadStore = true;
        caps.isTileBasedGPU = true;
        caps.maxFramesInFlight = 3;
        caps.maxSampledTexturesPerStage = 96;
        caps.maxSamplersPerStage = 16;
        break;

    case eRHIBackend::Xbox:
    case eRHIBackend::PS5:
        caps.featureTier = eRHIFeatureTier::Console;
        caps.supportsCompute = true;
        caps.supportsAsyncCompute = true;
        caps.supportsBindless = true;
        caps.supportsVariableRateShading = true;
        caps.requiresExplicitResourceStates = true;
        caps.prefersRenderPassLoadStore = true;
        caps.maxFramesInFlight = 3;
        caps.maxSampledTexturesPerStage = 128;
        caps.maxSamplersPerStage = 32;
        break;

    default:
        break;
    }

    return caps;
}
```

아래로 교체:

```cpp
inline RHICapabilities RHI_MakeDefaultCapabilities(eRHIBackend backend)
{
    RHICapabilities caps{};
    caps.backend = backend;

    switch (backend)
    {
    case eRHIBackend::DX11:
        caps.featureTier = eRHIFeatureTier::LegacyDX11;
        caps.maxFramesInFlight = 1;
        break;

    case eRHIBackend::DX12:
    case eRHIBackend::Vulkan:
        caps.featureTier = eRHIFeatureTier::ExplicitDesktop;
        caps.requiresExplicitResourceStates = true;
        break;

    case eRHIBackend::Metal:
        caps.featureTier = eRHIFeatureTier::MobileTiled;
        break;

    case eRHIBackend::Xbox:
    case eRHIBackend::PS5:
        caps.featureTier = eRHIFeatureTier::Console;
        break;

    default:
        break;
    }

    return caps;
}
```

### 1-8. C:/Users/user/Desktop/Winters/Engine/Public/RHI/IRHIDevice.h

기존 코드:

```cpp
    virtual RHICapabilities GetCapabilities() const
    {
        return RHI_MakeDefaultCapabilities(GetBackend());
    }
```

아래로 교체:

```cpp
    virtual RHICapabilities GetCapabilities() const = 0;
```

### 1-9. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/RenderGraph/RenderGraph.h

새 파일:

```cpp
#pragma once

#include "RHI/RHIDescriptors.h"

#include <functional>
#include <memory>

class IRHICommandList;

class CRenderGraphPassBuilder final
{
public:
    void ReadBuffer(
        RHIBufferHandle handle,
        eRHIResourceState state);
    void WriteBuffer(
        RHIBufferHandle handle,
        eRHIResourceState state);
    void ReadTexture(
        RHITextureHandle handle,
        eRHIResourceState state);
    void WriteTexture(
        RHITextureHandle handle,
        eRHIResourceState state);
    void SetSideEffect();

private:
    friend class CRenderGraph;

    explicit CRenderGraphPassBuilder(void* pPassNode)
        : m_pPassNode(pPassNode)
    {
    }

    void* m_pPassNode = nullptr;
};

class CRenderGraph final
{
public:
    using SetupFunction =
        std::function<void(CRenderGraphPassBuilder&)>;
    using ExecuteFunction =
        std::function<void(IRHICommandList&)>;

    CRenderGraph();
    ~CRenderGraph();

    CRenderGraph(const CRenderGraph&) = delete;
    CRenderGraph& operator=(const CRenderGraph&) = delete;

    void BeginFrame(IRHICommandList* pCommandList);
    bool_t AddPass(
        const char* pName,
        SetupFunction setup,
        ExecuteFunction execute);
    bool_t Execute();

    u32_t GetCompiledPassCount() const;
    u32_t GetExecutedPassCount() const;
    u32_t GetCulledPassCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

### 1-10. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/RenderGraph/RenderGraph.cpp

새 파일:

```cpp
#include "WintersPCH.h"

#include "Renderer/RenderGraph/RenderGraph.h"

#include "ProfilerAPI.h"
#include "RHI/IRHICommandList.h"

#include <algorithm>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
    constexpr u32_t kInvalidPassIndex = UINT32_MAX;

    enum class eRenderGraphResourceType : u8_t
    {
        Buffer = 0,
        Texture,
    };

    enum class eRenderGraphAccessType : u8_t
    {
        Read = 0,
        Write,
    };

    struct RenderGraphResourceKey
    {
        eRenderGraphResourceType Type = eRenderGraphResourceType::Buffer;
        u64_t Value = 0;

        bool operator==(const RenderGraphResourceKey& other) const
        {
            return Type == other.Type && Value == other.Value;
        }
    };

    struct RenderGraphResourceKeyHash
    {
        size_t operator()(const RenderGraphResourceKey& key) const
        {
            const u64_t typeBits =
                static_cast<u64_t>(key.Type) << 61u;
            return static_cast<size_t>(key.Value ^ typeBits);
        }
    };

    struct RenderGraphAccess
    {
        RenderGraphResourceKey Key{};
        eRenderGraphAccessType Type = eRenderGraphAccessType::Read;
        eRHIResourceState State = eRHIResourceState::Common;
    };

    struct RenderGraphPassNode
    {
        std::string Name{};
        std::vector<RenderGraphAccess> Accesses{};
        std::vector<u32_t> Dependencies{};
        CRenderGraph::ExecuteFunction Execute{};
        bool_t bSideEffect = false;
        bool_t bLive = false;
    };

    struct RenderGraphResourceHistory
    {
        u32_t LastWriter = kInvalidPassIndex;
        std::vector<u32_t> Readers{};
    };

    RenderGraphResourceKey MakeBufferKey(RHIBufferHandle handle)
    {
        return { eRenderGraphResourceType::Buffer, handle.ToU64() };
    }

    RenderGraphResourceKey MakeTextureKey(RHITextureHandle handle)
    {
        return { eRenderGraphResourceType::Texture, handle.ToU64() };
    }

    void AddAccess(
        RenderGraphPassNode& pass,
        const RenderGraphResourceKey& key,
        eRenderGraphAccessType accessType,
        eRHIResourceState state)
    {
        if (key.Value == 0)
            return;

        for (RenderGraphAccess& access : pass.Accesses)
        {
            if (access.Key == key)
            {
                if (access.State != state)
                {
                    OutputDebugStringA(
                        "[RenderGraph] One pass declared conflicting states for one resource; latest declaration wins\n");
                }

                access.State = state;
                if (accessType == eRenderGraphAccessType::Write)
                    access.Type = eRenderGraphAccessType::Write;
                return;
            }
        }

        pass.Accesses.push_back({ key, accessType, state });
    }

    void AddDependency(RenderGraphPassNode& pass, u32_t dependency)
    {
        if (dependency == kInvalidPassIndex)
            return;

        if (std::find(
                pass.Dependencies.begin(),
                pass.Dependencies.end(),
                dependency) == pass.Dependencies.end())
        {
            pass.Dependencies.push_back(dependency);
        }
    }

    void MarkPassLive(
        std::vector<RenderGraphPassNode>& passes,
        u32_t passIndex)
    {
        if (passIndex >= passes.size() || passes[passIndex].bLive)
            return;

        passes[passIndex].bLive = true;
        for (u32_t dependency : passes[passIndex].Dependencies)
            MarkPassLive(passes, dependency);
    }

    void ResetAfterExecute(
        std::vector<RenderGraphPassNode>& passes,
        IRHICommandList*& pCommandList)
    {
        passes.clear();
        pCommandList = nullptr;
    }
}

struct CRenderGraph::Impl
{
    IRHICommandList* pCommandList = nullptr;
    std::vector<RenderGraphPassNode> Passes{};
    u32_t CompiledPassCount = 0;
    u32_t ExecutedPassCount = 0;
    u32_t CulledPassCount = 0;
};

void CRenderGraphPassBuilder::ReadBuffer(
    RHIBufferHandle handle,
    eRHIResourceState state)
{
    auto* pPass = static_cast<RenderGraphPassNode*>(m_pPassNode);
    if (pPass)
        AddAccess(*pPass, MakeBufferKey(handle), eRenderGraphAccessType::Read, state);
}

void CRenderGraphPassBuilder::WriteBuffer(
    RHIBufferHandle handle,
    eRHIResourceState state)
{
    auto* pPass = static_cast<RenderGraphPassNode*>(m_pPassNode);
    if (pPass)
        AddAccess(*pPass, MakeBufferKey(handle), eRenderGraphAccessType::Write, state);
}

void CRenderGraphPassBuilder::ReadTexture(
    RHITextureHandle handle,
    eRHIResourceState state)
{
    auto* pPass = static_cast<RenderGraphPassNode*>(m_pPassNode);
    if (pPass)
        AddAccess(*pPass, MakeTextureKey(handle), eRenderGraphAccessType::Read, state);
}

void CRenderGraphPassBuilder::WriteTexture(
    RHITextureHandle handle,
    eRHIResourceState state)
{
    auto* pPass = static_cast<RenderGraphPassNode*>(m_pPassNode);
    if (pPass)
        AddAccess(*pPass, MakeTextureKey(handle), eRenderGraphAccessType::Write, state);
}

void CRenderGraphPassBuilder::SetSideEffect()
{
    auto* pPass = static_cast<RenderGraphPassNode*>(m_pPassNode);
    if (pPass)
        pPass->bSideEffect = true;
}

CRenderGraph::CRenderGraph()
    : m_pImpl(std::make_unique<Impl>())
{
}

CRenderGraph::~CRenderGraph() = default;

void CRenderGraph::BeginFrame(IRHICommandList* pCommandList)
{
    m_pImpl->pCommandList = pCommandList;
    m_pImpl->Passes.clear();
    m_pImpl->CompiledPassCount = 0;
    m_pImpl->ExecutedPassCount = 0;
    m_pImpl->CulledPassCount = 0;
}

bool_t CRenderGraph::AddPass(
    const char* pName,
    SetupFunction setup,
    ExecuteFunction execute)
{
    if (!m_pImpl->pCommandList || !pName || !pName[0] || !setup || !execute)
    {
        OutputDebugStringA(
            "[RenderGraph] AddPass requires a frame command list, name, setup, and execute function\n");
        return false;
    }

    RenderGraphPassNode pass{};
    pass.Name = pName;
    pass.Execute = std::move(execute);
    m_pImpl->Passes.push_back(std::move(pass));

    RenderGraphPassNode& storedPass = m_pImpl->Passes.back();
    CRenderGraphPassBuilder builder(&storedPass);
    setup(builder);
    return true;
}

bool_t CRenderGraph::Execute()
{
    if (!m_pImpl->pCommandList)
    {
        OutputDebugStringA("[RenderGraph] Execute called without a frame command list\n");
        return false;
    }

    std::unordered_map<
        RenderGraphResourceKey,
        RenderGraphResourceHistory,
        RenderGraphResourceKeyHash> histories;

    for (u32_t passIndex = 0;
         passIndex < static_cast<u32_t>(m_pImpl->Passes.size());
         ++passIndex)
    {
        RenderGraphPassNode& pass = m_pImpl->Passes[passIndex];
        pass.Dependencies.clear();
        pass.bLive = false;

        for (const RenderGraphAccess& access : pass.Accesses)
        {
            RenderGraphResourceHistory& history = histories[access.Key];

            if (access.Type == eRenderGraphAccessType::Read)
            {
                AddDependency(pass, history.LastWriter);
                history.Readers.push_back(passIndex);
                continue;
            }

            AddDependency(pass, history.LastWriter);
            for (u32_t reader : history.Readers)
                AddDependency(pass, reader);

            history.Readers.clear();
            history.LastWriter = passIndex;
        }
    }

    for (u32_t passIndex = 0;
         passIndex < static_cast<u32_t>(m_pImpl->Passes.size());
         ++passIndex)
    {
        if (m_pImpl->Passes[passIndex].bSideEffect)
            MarkPassLive(m_pImpl->Passes, passIndex);
    }

    std::vector<std::vector<u32_t>> dependents(m_pImpl->Passes.size());
    std::vector<u32_t> indegrees(m_pImpl->Passes.size(), 0);
    std::deque<u32_t> ready;
    u32_t livePassCount = 0;

    for (u32_t passIndex = 0;
         passIndex < static_cast<u32_t>(m_pImpl->Passes.size());
         ++passIndex)
    {
        const RenderGraphPassNode& pass = m_pImpl->Passes[passIndex];
        if (!pass.bLive)
            continue;

        ++livePassCount;
        for (u32_t dependency : pass.Dependencies)
        {
            if (dependency < m_pImpl->Passes.size() &&
                m_pImpl->Passes[dependency].bLive)
            {
                dependents[dependency].push_back(passIndex);
                ++indegrees[passIndex];
            }
        }
    }

    for (u32_t passIndex = 0;
         passIndex < static_cast<u32_t>(m_pImpl->Passes.size());
         ++passIndex)
    {
        if (m_pImpl->Passes[passIndex].bLive && indegrees[passIndex] == 0)
            ready.push_back(passIndex);
    }

    std::vector<u32_t> executionOrder;
    executionOrder.reserve(livePassCount);

    while (!ready.empty())
    {
        const u32_t passIndex = ready.front();
        ready.pop_front();
        executionOrder.push_back(passIndex);

        for (u32_t dependent : dependents[passIndex])
        {
            if (--indegrees[dependent] == 0)
                ready.push_back(dependent);
        }
    }

    if (executionOrder.size() != livePassCount)
    {
        OutputDebugStringA("[RenderGraph] Dependency cycle detected\n");
        ResetAfterExecute(m_pImpl->Passes, m_pImpl->pCommandList);
        return false;
    }

    m_pImpl->CompiledPassCount =
        static_cast<u32_t>(m_pImpl->Passes.size());
    m_pImpl->CulledPassCount =
        m_pImpl->CompiledPassCount - livePassCount;

    for (u32_t passIndex : executionOrder)
    {
        RenderGraphPassNode& pass = m_pImpl->Passes[passIndex];
        for (const RenderGraphAccess& access : pass.Accesses)
        {
            if (access.Key.Type == eRenderGraphResourceType::Buffer)
            {
                m_pImpl->pCommandList->TransitionResource(
                    RHIBufferHandle::FromU64(access.Key.Value),
                    access.State);
            }
            else
            {
                m_pImpl->pCommandList->TransitionResource(
                    RHITextureHandle::FromU64(access.Key.Value),
                    access.State);
            }
        }

        pass.Execute(*m_pImpl->pCommandList);
        ++m_pImpl->ExecutedPassCount;
    }

    WINTERS_PROFILE_COUNT(
        "RenderGraph::CompiledPasses",
        m_pImpl->CompiledPassCount);
    WINTERS_PROFILE_COUNT(
        "RenderGraph::ExecutedPasses",
        m_pImpl->ExecutedPassCount);
    WINTERS_PROFILE_COUNT(
        "RenderGraph::CulledPasses",
        m_pImpl->CulledPassCount);

    ResetAfterExecute(m_pImpl->Passes, m_pImpl->pCommandList);
    return true;
}

u32_t CRenderGraph::GetCompiledPassCount() const
{
    return m_pImpl->CompiledPassCount;
}

u32_t CRenderGraph::GetExecutedPassCount() const
{
    return m_pImpl->ExecutedPassCount;
}

u32_t CRenderGraph::GetCulledPassCount() const
{
    return m_pImpl->CulledPassCount;
}
```

### 1-11. C:/Users/user/Desktop/Winters/Engine/Private/Renderer/RHISceneRenderer.cpp

기존 코드:

```cpp
#include "Renderer/RHISceneRenderer.h"

#include "RHI/IRHICommandList.h"
```

아래로 교체:

```cpp
#include "Renderer/RHISceneRenderer.h"

#include "Renderer/RenderGraph/RenderGraph.h"
#include "RHI/IRHICommandList.h"
```

`struct CRHISceneRenderer::Impl` 안의 기존 코드:

```cpp
    IRHIDevice* pDevice = nullptr;
    RHIShaderHandle hColorVS{};
```

아래로 교체:

```cpp
    IRHIDevice* pDevice = nullptr;
    std::unique_ptr<CRenderGraph> pRenderGraph{};
    RHIShaderHandle hColorVS{};
```

기존 코드:

```cpp
    Impl& impl = *pRenderer->m_pImpl;
    impl.pDevice = pDevice;

    const bool_t useSM51 = pDevice->GetBackend() == eRHIBackend::DX12;
```

아래로 교체:

```cpp
    Impl& impl = *pRenderer->m_pImpl;
    impl.pDevice = pDevice;
    impl.pRenderGraph = std::make_unique<CRenderGraph>();

    const bool_t useSM51 = pDevice->GetBackend() == eRHIBackend::DX12;
```

기존 코드:

```cpp
    return m_pImpl &&
        m_pImpl->pDevice &&
        m_pImpl->hColorVS.IsValid() &&
```

아래로 교체:

```cpp
    return m_pImpl &&
        m_pImpl->pDevice &&
        m_pImpl->pRenderGraph &&
        m_pImpl->hColorVS.IsValid() &&
```

기존 코드:

```cpp
void CRHISceneRenderer::Render(IRHIDevice* pDevice, const RenderWorldSnapshot& snapshot)
{
    WINTERS_PROFILE_SCOPE("RHISceneRenderer::Render");
    if (!IsReady() || !pDevice || pDevice != m_pImpl->pDevice)
        return;

    IRHICommandList* pCommandList = pDevice->GetFrameCommandList();
    if (!pCommandList)
        return;

    const u32_t meshCount = static_cast<u32_t>(snapshot.meshes.size());
    WINTERS_PROFILE_COUNT("RHI::SceneMeshCandidates", meshCount);
    if (meshCount == 0 || !m_pImpl->EnsureDrawSlots(meshCount))
        return;

    SceneFrameConstants frameData{};
    frameData.viewProjection = snapshot.view.matViewProjection.m;
    pCommandList->UpdateBuffer(
        m_pImpl->hFrameConstants,
        &frameData,
        static_cast<u32_t>(sizeof(frameData)));

    u32_t submittedDraws = 0;
    u64_t submittedIndices = 0;
    for (u32_t i = 0; i < meshCount; ++i)
    {
        const RenderMeshItem& item = snapshot.meshes[i];
        const RHIMeshSlice& mesh = item.mesh;
        if (!mesh.hVertexBuffer.IsValid() ||
            !mesh.hIndexBuffer.IsValid() ||
            mesh.vertexStride == 0 ||
            mesh.indexCount == 0)
        {
            continue;
        }

        RHIPipelineHandle hPipeline{};
        if (mesh.vertexLayout == eRenderVertexLayout::PositionNormalUv)
        {
            hPipeline = item.bDepthWrite
                ? m_pImpl->hStaticPipeline
                : m_pImpl->hStaticNoDepthPipeline;
        }
        else
        {
            hPipeline = item.bDepthWrite
                ? m_pImpl->hColorPipeline
                : m_pImpl->hColorNoDepthPipeline;
        }
        if (!hPipeline.IsValid())
            continue;

        Impl::DrawSlot& slot = m_pImpl->drawSlots[i];
        SceneObjectConstants objectData{};
        objectData.world = item.matWorld.m;
        objectData.tint = item.vTint.ToXMFLOAT4();
        pCommandList->UpdateBuffer(
            slot.hObjectConstants,
            &objectData,
            static_cast<u32_t>(sizeof(objectData)));

        RHIBindGroupResource resources[4]{};
        resources[0].slot = 0;
        resources[0].type = eRHIBindingType::ConstantBuffer;
        resources[0].bufferHandle = m_pImpl->hFrameConstants;
        resources[1].slot = 1;
        resources[1].type = eRHIBindingType::ConstantBuffer;
        resources[1].bufferHandle = slot.hObjectConstants;
        resources[2].slot = 0;
        resources[2].type = eRHIBindingType::ShaderResource;
        resources[2].textureHandle = item.hAlbedoTexture.IsValid()
            ? item.hAlbedoTexture
            : m_pImpl->hDefaultTexture;
        resources[3].slot = 0;
        resources[3].type = eRHIBindingType::Sampler;
        resources[3].samplerHandle = item.hSampler.IsValid()
            ? item.hSampler
            : m_pImpl->hDefaultSampler;

        pDevice->UpdateBindGroup(
            slot.hBindGroup,
            resources,
            static_cast<u32_t>(std::size(resources)));
        pCommandList->SetPipeline(hPipeline);
        pCommandList->SetBindGroup(0, slot.hBindGroup);
        pCommandList->SetVertexBuffer(0, mesh.hVertexBuffer, mesh.vertexStride, 0);
        pCommandList->SetIndexBuffer(mesh.hIndexBuffer, 0, eRHIFormat::R32_UInt);
        pCommandList->DrawIndexed(
            mesh.indexCount,
            1,
            mesh.firstIndex,
            mesh.baseVertex,
            0);
        ++submittedDraws;
        submittedIndices += mesh.indexCount;
    }

    WINTERS_PROFILE_COUNT("RHI::SceneDrawCalls", submittedDraws);
    WINTERS_PROFILE_COUNT("RHI::SceneSubmittedIndices", submittedIndices);
}
```

아래로 교체:

```cpp
void CRHISceneRenderer::Render(IRHIDevice* pDevice, const RenderWorldSnapshot& snapshot)
{
    WINTERS_PROFILE_SCOPE("RHISceneRenderer::Render");
    if (!IsReady() || !pDevice || pDevice != m_pImpl->pDevice)
        return;

    IRHICommandList* pCommandList = pDevice->GetFrameCommandList();
    if (!pCommandList)
    {
        OutputDebugStringA("[RHISceneRenderer] Frame command list is unavailable\n");
        return;
    }

    const u32_t meshCount = static_cast<u32_t>(snapshot.meshes.size());
    WINTERS_PROFILE_COUNT("RHI::SceneMeshCandidates", meshCount);
    if (meshCount == 0)
        return;

    if (!m_pImpl->EnsureDrawSlots(meshCount))
    {
        OutputDebugStringA("[RHISceneRenderer] Draw-slot allocation failed\n");
        return;
    }

    SceneFrameConstants frameData{};
    frameData.viewProjection = snapshot.view.matViewProjection.m;
    pCommandList->UpdateBuffer(
        m_pImpl->hFrameConstants,
        &frameData,
        static_cast<u32_t>(sizeof(frameData)));

    m_pImpl->pRenderGraph->BeginFrame(pCommandList);
    const bool_t bPassAdded = m_pImpl->pRenderGraph->AddPass(
        "SceneMeshes",
        [&](CRenderGraphPassBuilder& builder)
        {
            builder.ReadBuffer(
                m_pImpl->hFrameConstants,
                eRHIResourceState::VertexConstant);

            for (u32_t i = 0; i < meshCount; ++i)
            {
                const RenderMeshItem& item = snapshot.meshes[i];
                const RHIMeshSlice& mesh = item.mesh;
                if (!mesh.hVertexBuffer.IsValid() ||
                    !mesh.hIndexBuffer.IsValid() ||
                    mesh.vertexStride == 0 ||
                    mesh.indexCount == 0)
                {
                    continue;
                }

                builder.ReadBuffer(
                    m_pImpl->drawSlots[i].hObjectConstants,
                    eRHIResourceState::VertexConstant);
                builder.ReadBuffer(
                    mesh.hVertexBuffer,
                    eRHIResourceState::VertexConstant);
                builder.ReadBuffer(
                    mesh.hIndexBuffer,
                    eRHIResourceState::IndexBuffer);
                builder.ReadTexture(
                    item.hAlbedoTexture.IsValid()
                        ? item.hAlbedoTexture
                        : m_pImpl->hDefaultTexture,
                    eRHIResourceState::ShaderResource);
            }

            builder.SetSideEffect();
        },
        [&](IRHICommandList& commandList)
        {
            u32_t submittedDraws = 0;
            u64_t submittedIndices = 0;

            for (u32_t i = 0; i < meshCount; ++i)
            {
                const RenderMeshItem& item = snapshot.meshes[i];
                const RHIMeshSlice& mesh = item.mesh;
                if (!mesh.hVertexBuffer.IsValid() ||
                    !mesh.hIndexBuffer.IsValid() ||
                    mesh.vertexStride == 0 ||
                    mesh.indexCount == 0)
                {
                    continue;
                }

                RHIPipelineHandle hPipeline{};
                if (mesh.vertexLayout == eRenderVertexLayout::PositionNormalUv)
                {
                    hPipeline = item.bDepthWrite
                        ? m_pImpl->hStaticPipeline
                        : m_pImpl->hStaticNoDepthPipeline;
                }
                else
                {
                    hPipeline = item.bDepthWrite
                        ? m_pImpl->hColorPipeline
                        : m_pImpl->hColorNoDepthPipeline;
                }
                if (!hPipeline.IsValid())
                    continue;

                Impl::DrawSlot& slot = m_pImpl->drawSlots[i];
                SceneObjectConstants objectData{};
                objectData.world = item.matWorld.m;
                objectData.tint = item.vTint.ToXMFLOAT4();
                commandList.UpdateBuffer(
                    slot.hObjectConstants,
                    &objectData,
                    static_cast<u32_t>(sizeof(objectData)));

                RHIBindGroupResource resources[4]{};
                resources[0].slot = 0;
                resources[0].type = eRHIBindingType::ConstantBuffer;
                resources[0].bufferHandle = m_pImpl->hFrameConstants;
                resources[1].slot = 1;
                resources[1].type = eRHIBindingType::ConstantBuffer;
                resources[1].bufferHandle = slot.hObjectConstants;
                resources[2].slot = 0;
                resources[2].type = eRHIBindingType::ShaderResource;
                resources[2].textureHandle = item.hAlbedoTexture.IsValid()
                    ? item.hAlbedoTexture
                    : m_pImpl->hDefaultTexture;
                resources[3].slot = 0;
                resources[3].type = eRHIBindingType::Sampler;
                resources[3].samplerHandle = item.hSampler.IsValid()
                    ? item.hSampler
                    : m_pImpl->hDefaultSampler;

                pDevice->UpdateBindGroup(
                    slot.hBindGroup,
                    resources,
                    static_cast<u32_t>(std::size(resources)));
                commandList.SetPipeline(hPipeline);
                commandList.SetBindGroup(0, slot.hBindGroup);
                commandList.SetVertexBuffer(
                    0,
                    mesh.hVertexBuffer,
                    mesh.vertexStride,
                    0);
                commandList.SetIndexBuffer(
                    mesh.hIndexBuffer,
                    0,
                    eRHIFormat::R32_UInt);
                commandList.DrawIndexed(
                    mesh.indexCount,
                    1,
                    mesh.firstIndex,
                    mesh.baseVertex,
                    0);
                ++submittedDraws;
                submittedIndices += mesh.indexCount;
            }

            WINTERS_PROFILE_COUNT("RHI::SceneDrawCalls", submittedDraws);
            WINTERS_PROFILE_COUNT(
                "RHI::SceneSubmittedIndices",
                submittedIndices);
        });

    if (!bPassAdded || !m_pImpl->pRenderGraph->Execute())
    {
        OutputDebugStringA("[RHISceneRenderer] Render Graph execution failed\n");
    }
}
```

`CRHISceneRenderer::Shutdown` 안의 기존 코드:

```cpp
    IRHIDevice* pDevice = m_pImpl->pDevice;
    if (pDevice)
```

아래로 교체:

```cpp
    m_pImpl->pRenderGraph.reset();

    IRHIDevice* pDevice = m_pImpl->pDevice;
    if (pDevice)
```

### 1-12. C:/Users/user/Desktop/Winters/Engine/Private/RHI/Vulkan/VulkanDevice.h 및 C:/Users/user/Desktop/Winters/Engine/Private/RHI/Vulkan/VulkanDevice.cpp

```text
CONFIRM_NEEDED - 현재 Winters workspace에는 Vulkan SDK 환경 변수, Vulkan-Headers, loader import library, VMA, DXC SPIR-V toolchain이 없다. `vulkaninfo.exe`와 runtime loader 존재만으로 compile 가능한 backend를 만들 수 없으며, 빈 `CVulkanDevice`나 모든 메서드가 빈 handle을 반환하는 fake backend는 추가하지 않는다.

전체 파일 본문을 작성하기 전에 다음을 확정해야 한다.
- Vulkan-Headers/loader/VMA의 vendor 경로와 버전 및 라이선스 파일
- `Engine.vcxproj`/CMake의 include, lib, runtime packaging 소유권
- HLSL -> SPIR-V compiler와 normalized binding reflection artifact
- Win32 surface/swapchain, queue family, frame-in-flight, descriptor pool, pipeline cache 계약
- `eRHIResourceState`에서 stage/access/layout으로 가는 단일 변환표

이 선행 packet이 실제 workspace에 들어온 뒤 `IRHIDevice`의 모든 pure virtual을 구현한 `CVulkanDevice` 전체 h/cpp와 conformance test를 별도 계획서에 완전 본문으로 작성한다.
```

### 1-13. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX11/CDX11Device.h

기존 코드:

```cpp
    eRHIBackend             GetBackend() const override { return eRHIBackend::DX11; }
    void* GetNativeHandle(eNativeHandleType type) const override
```

아래로 교체:

```cpp
    eRHIBackend GetBackend() const override { return eRHIBackend::DX11; }
    RHICapabilities GetCapabilities() const override
    {
        RHICapabilities caps = RHI_MakeDefaultCapabilities(eRHIBackend::DX11);
        caps.supportsCompute = false;
        caps.supportsAsyncCompute = false;
        caps.supportsBindless = false;
        caps.requiresExplicitResourceStates = false;
        caps.prefersRenderPassLoadStore = false;
        caps.maxFramesInFlight = 1;
        caps.maxSampledTexturesPerStage = 16;
        caps.maxSamplersPerStage = 16;
        return caps;
    }
    void* GetNativeHandle(eNativeHandleType type) const override
```

### 1-14. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.h

기존 코드:

```cpp
	eRHIBackend GetBackend() const override { return eRHIBackend::DX12; }
	void* GetNativeHandle(eNativeHandleType type) const override;
```

아래로 교체:

```cpp
	eRHIBackend GetBackend() const override { return eRHIBackend::DX12; }
	RHICapabilities GetCapabilities() const override
	{
		RHICapabilities caps = RHI_MakeDefaultCapabilities(eRHIBackend::DX12);
		caps.supportsCompute = false;
		caps.supportsAsyncCompute = false;
		caps.supportsBindless = false;
		caps.prefersRenderPassLoadStore = false;
		caps.requiresExplicitResourceStates = true;
		caps.maxFramesInFlight = kFrameCount;
		caps.maxSampledTexturesPerStage = 16;
		caps.maxSamplersPerStage = 16;
		return caps;
	}
	void* GetNativeHandle(eNativeHandleType type) const override;
```

### 1-15. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
#include "RHI/DX11/BlendStateCache.h"
#include "RHI/DX12/DX12Device.h"
```

아래로 교체:

```cpp
#include "RHI/DX11/BlendStateCache.h"
#include "RHI/RHIBackendRegistry.h"
```

anonymous namespace 안의 아래 기존 코드를 삭제:

```cpp
    std::unique_ptr<IRHIDevice> CreateDX11DeviceForWindow(CWin32Window& window, const EngineConfig& config)
    {
        DeviceDesc devDesc;
        devDesc.hwnd       = window.GetHandle();
        devDesc.width      = config.windowWidth;
        devDesc.height     = config.windowHeight;
        devDesc.vsync      = ShouldUsePresentationVSync(config);
        devDesc.fullscreen = config.fullscreen;
        return CDX11Device::Create(devDesc);
    }

    std::unique_ptr<IRHIDevice> CreateDX12DeviceForWindow(CWin32Window& window,
        const EngineConfig& config)
    {
        DX12DeviceDesc devDesc;
        devDesc.hwnd = window.GetHandle();
        devDesc.width = config.windowWidth;
        devDesc.height = config.windowHeight;
        devDesc.vsync = ShouldUsePresentationVSync(config);
        devDesc.fullscreen = config.fullscreen;
        return CDX12Device::Create(devDesc);
    }
```

기존 코드:

```cpp
    const auto tryDX11 = [&]() -> bool_t
    {
        m_pDevice = CreateDX11DeviceForWindow(m_Window, config);
        if (m_pDevice)
            OutputDebugStringA("[CEngineApp] RHI backend selected: DX11\n");
        return m_pDevice != nullptr;
    };

    const auto tryDX12 = [&]() -> bool_t
    {
        m_pDevice = CreateDX12DeviceForWindow(m_Window, config);
        if (m_pDevice)
            OutputDebugStringA("[CEngineApp] RHI backend selected: DX12\n");
        return m_pDevice != nullptr;
    };

    switch (config.rhiBackend)
    {
    case eEngineRHIBackend::DX12:
        tryDX12();
        break;
    case eEngineRHIBackend::DX11:
    case eEngineRHIBackend::Auto:
        tryDX11();
        break;
    default:
        OutputDebugStringA("[CEngineApp] Requested RHI backend is not implemented on this platform\n");
        break;
    }

    if (!m_pDevice && config.allowRHIFallback && config.rhiBackend != eEngineRHIBackend::DX11)
    {
        OutputDebugStringA("[CEngineApp] Falling back to DX11 legacy backend\n");
        tryDX11();
    }
```

아래로 교체:

```cpp
    RHIBackendSelectionRequest selectionRequest{};
    selectionRequest.Requested = config.rhiBackend;
    selectionRequest.bAllowAutoFallback = config.allowRHIFallback;
    selectionRequest.Surface.type = eRHIPlatformSurfaceType::Win32HWND;
    selectionRequest.Surface.nativeHandle = m_Window.GetHandle();
    selectionRequest.Surface.width = config.windowWidth;
    selectionRequest.Surface.height = config.windowHeight;
    selectionRequest.Surface.vsync = ShouldUsePresentationVSync(config);
    selectionRequest.Surface.fullscreen = config.fullscreen;
    selectionRequest.Surface.lifecycleState = eRHISurfaceLifecycleState::Active;

    RHIBackendSelectionResult selection =
        CRHIBackendRegistry::Select(selectionRequest);
    m_pDevice = std::move(selection.pDevice);
```

### 1-16. C:/Users/user/Desktop/Winters/Engine/Private/RHI/DX12/DX12Device.cpp

기존 코드:

```cpp
    D3D12_RESOURCE_STATES ToInitialBufferState(eRHIBufferUsage usage)
    {
        switch (usage)
        {
        case eRHIBufferUsage::Index:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case eRHIBufferUsage::Vertex:
        case eRHIBufferUsage::Constant:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        default:
            return D3D12_RESOURCE_STATE_COMMON;
        }
    }
```

아래에 추가:

```cpp
    D3D12_RESOURCE_STATES ToDX12ResourceState(eRHIResourceState state)
    {
        switch (state)
        {
        case eRHIResourceState::VertexConstant:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case eRHIResourceState::IndexBuffer:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case eRHIResourceState::RenderTarget:
            return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case eRHIResourceState::DepthRead:
            return D3D12_RESOURCE_STATE_DEPTH_READ;
        case eRHIResourceState::DepthWrite:
            return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case eRHIResourceState::ShaderResource:
            return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case eRHIResourceState::UnorderedAccess:
            return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case eRHIResourceState::CopyDest:
            return D3D12_RESOURCE_STATE_COPY_DEST;
        case eRHIResourceState::CopySource:
            return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case eRHIResourceState::Present:
            return D3D12_RESOURCE_STATE_PRESENT;
        case eRHIResourceState::Common:
        default:
            return D3D12_RESOURCE_STATE_COMMON;
        }
    }
```

`class CDX12FrameCommandList final : public IRHICommandList` 안의 기존 코드:

```cpp
    void TransitionResource(RHIBufferHandle, eRHIResourceState) override {}
    void TransitionResource(RHITextureHandle, eRHIResourceState) override {}
```

아래로 교체:

```cpp
    void TransitionResource(
        RHIBufferHandle handle,
        eRHIResourceState newState) override
    {
        if (!m_Owner.m_pTables || !m_Owner.m_pCommandList)
        {
            OutputDebugStringA("[CDX12FrameCommandList] Buffer transition has no device tables or command list\n");
            return;
        }

        CDX12Buffer* pBuffer =
            m_Owner.m_pTables->bufferTable.Lookup(handle);
        if (!pBuffer)
        {
            OutputDebugStringA("[CDX12FrameCommandList] Buffer transition received a stale handle\n");
            return;
        }

        if (pBuffer->uploadHeap)
            return;

        ID3D12Resource* pResource =
            GetDX12BufferResource(*pBuffer, m_Owner.m_iFrameIndex);
        if (!pResource)
        {
            OutputDebugStringA("[CDX12FrameCommandList] Buffer transition has no native resource\n");
            return;
        }

        const D3D12_RESOURCE_STATES targetState =
            ToDX12ResourceState(newState);
        if (pBuffer->state == targetState)
            return;

        const D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
            pResource,
            pBuffer->state,
            targetState);
        m_Owner.m_pCommandList->ResourceBarrier(1, &barrier);
        pBuffer->state = targetState;
    }

    void TransitionResource(
        RHITextureHandle handle,
        eRHIResourceState newState) override
    {
        if (!m_Owner.m_pTables || !m_Owner.m_pCommandList)
        {
            OutputDebugStringA("[CDX12FrameCommandList] Texture transition has no device tables or command list\n");
            return;
        }

        CDX12Texture* pTexture =
            m_Owner.m_pTables->textureTable.Lookup(handle);
        if (!pTexture || !pTexture->pResource)
        {
            OutputDebugStringA("[CDX12FrameCommandList] Texture transition received a stale handle\n");
            return;
        }

        const D3D12_RESOURCE_STATES targetState =
            ToDX12ResourceState(newState);
        if (pTexture->state == targetState)
            return;

        const D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
            pTexture->pResource.Get(),
            pTexture->state,
            targetState);
        m_Owner.m_pCommandList->ResourceBarrier(1, &barrier);
        pTexture->state = targetState;
    }
```

### 1-17. C:/Users/user/Desktop/Winters/Client/Private/main.cpp

기존 코드:

```cpp
        if (HasCommandLineFlag(L"--rhi=dx12", L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (HasCommandLineFlag(L"--rhi=null", L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

아래로 교체:

```cpp
        if (HasCommandLineFlag(L"--rhi=dx12", L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (HasCommandLineFlag(L"--rhi=vulkan", L"/rhi:vulkan"))
            return eEngineRHIBackend::Vulkan;
        if (HasCommandLineFlag(L"--rhi=null", L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

### 1-18. C:/Users/user/Desktop/Winters/EldenRingClient/Private/main.cpp

기존 코드:

```cpp
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

아래로 교체:

```cpp
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=vulkan") || wcsstr(pCommandLine, L"/rhi:vulkan"))
            return eEngineRHIBackend::Vulkan;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

### 1-19. C:/Users/user/Desktop/Winters/EldenRingEditor/Private/main.cpp

기존 코드:

```cpp
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

아래로 교체:

```cpp
        if (wcsstr(pCommandLine, L"--rhi=dx11") || wcsstr(pCommandLine, L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (wcsstr(pCommandLine, L"--rhi=vulkan") || wcsstr(pCommandLine, L"/rhi:vulkan"))
            return eEngineRHIBackend::Vulkan;
        if (wcsstr(pCommandLine, L"--rhi=null") || wcsstr(pCommandLine, L"/rhi:null"))
            return eEngineRHIBackend::Null;
```

## 2. 검증

미검증:

- 이 문서는 runtime 코드 프리뷰이며 본 세션에서 Engine/Client 실행 코드는 적용하지 않았다.
- Vulkan backend는 구현 완료가 아니다. loader probe와 backend-not-compiled 진단만 본 계획에서 완전 코드로 고정했다.
- Render Graph 첫 slice는 single-thread pass dependency/state transition 원장이다. transient resource allocation, aliasing, async compute, pass merge는 범위 밖이다.

검증 명령:

```powershell
git diff --check
msbuild Winters.sln /t:Engine /p:Configuration=Debug /p:Platform=x64 /m
msbuild Winters.sln /t:EldenRingClient /p:Configuration=Debug-DX12 /p:Platform=x64 /m
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Run-S17RhiValidation.ps1 -ReportPath .md/build/2026-07-15_RHI_RENDER_GRAPH_SYNC_REPORT.md
```

자동 확인:

```powershell
rg -n -- "WintersElden_probe_dx12.*--rhi=dx12" Tools/Harness/Run-S17RhiValidation.ps1
rg -n "supportsCompute = true|supportsAsyncCompute = true|supportsBindless = true|supportsVariableRateShading = true" Engine/Public/RHI/RHICapabilities.h
rg -n "TransitionResource\(RHIBufferHandle, eRHIResourceState\) override \{\}|TransitionResource\(RHITextureHandle, eRHIResourceState\) override \{\}" Engine/Private/RHI/DX12/DX12Device.cpp
rg -n "ID3D11|ID3D12|d3d11.h|d3d12.h|vulkan.h" Engine/Public/RHI Engine/Public/Renderer Client/Public
```

기대 결과:

- S17 DX12 항목이 실제 `--rhi=dx12`를 전달한다.
- 공용 capability helper가 backend enum만으로 compute/async/bindless/VRS를 true로 만들지 않는다.
- DX12 buffer/texture transition no-op가 0건이다.
- 새 Render Graph와 backend registry가 public header에 native graphics type을 노출하지 않는다.

수동 확인:

- `WintersElden.exe --scene=probe --rhi=dx11`은 requested/selected가 DX11로 일치한다.
- `WintersElden.exe --scene=probe --rhi=dx12`는 requested/selected가 DX12로 일치하며 실패 시 DX11로 fallback하지 않는다.
- `WintersElden.exe --scene=probe --rhi=vulkan`은 loader 버전과 `BackendNotCompiled`을 구분해 출력하고 DX11 화면으로 조용히 진입하지 않는다.
- DX11과 DX12에서 동일 `RenderWorldSnapshot` static scene을 실행하고 `RenderGraph::CompiledPasses`, `ExecutedPasses`, `CulledPasses` 카운터를 확인한다.
- DX12 debug layer/PIX에서 scene mesh의 buffer/texture state mismatch와 resource barrier 오류가 0인지 확인한다.
- LoL 정상 F5의 roster, map, minion, champion, UI, FOW, FX를 숨기지 않는다. 이 packet은 static RHI scene만 다루므로 LoL 제품 parity 완료로 판정하지 않는다.

확인 필요:

- 새 `.h/.cpp` 파일이 `Engine/Include/Engine.vcxproj`와 `.vcxproj.filters`의 기존 RHI/Renderer 그룹에 포함되는지 확인.
- CMake `WintersWorkspaceMap`은 browsing map이므로 legacy Engine build 등록을 대신하지 않는지 확인.
- Vulkan full backend 전 Vulkan-Headers/loader/VMA/DXC dependency packet과 shader artifact contract를 확정.

후속 동기화:

- Engine public header 변경 후 `UpdateLib.bat`을 실행해 `EngineSDK/inc`를 동기화한다.
- 합격 보고서에는 requested/selected backend, adapter, capability dump, Render Graph pass counters, DX12 validation 결과를 함께 기록한다.
- 전체 작업 시간의 30%는 천장 작업으로 고정해, 이 기반 packet 후 동일 snapshot의 DX11/DX12 캡처 또는 면접/블로그 공개물 중 하나를 외부 마감과 함께 완료한다.
