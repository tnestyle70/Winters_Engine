Session - CEngineApp의 concrete DX11/DX12 Device 생성 책임을 private RHI Backend Module/Registry/Factory로 이동하고 backend 미등록·preflight·생성·identity 실패를 구조화한다.
좌표: 신규 좌표 후보 RHI-P0B · P0 backend module/factory와 structured probe
관련: 2026-07-21_RHI_BACKEND_SELECTION_TRUTH_GATE_PLAN.md, WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_ARCHITECTURE.md

## 1. 결정 기록

① 문제·제약: `CEngineApp.cpp`가 `CDX11Device`와 `CDX12Device`를 직접 include·생성하고 switch/fallback/probe를 함께 소유한다. Vulkan 미등록과 Device 생성 실패도 같은 `device_initialization_failed`로 뭉개진다.
② 순진한 해법과 실패: switch를 helper 함수로만 옮기면 concrete header 위치만 바뀌고 module 존재, surface preflight, factory, 실제 Device identity를 분리할 수 없다. Unreal Module Loader 전체를 복제하는 것도 현재 backend 2개에는 과하다.
③ 메커니즘: Engine private에 작은 `IRHIBackendModule`과 정적 `CRHIBackendRegistry`를 둔다. module은 backend id/name, Win32 create-desc preflight, concrete Device factory를 소유하고 registry는 미등록·preflight 거절·생성 실패·identity 불일치를 구조화해 반환한다.
④ 대조: `CEngineApp`의 Device 생성부는 Auto→DX11 제품 정책과 fallback 후보 순서만 소유하고 concrete constructor를 모른다. 다만 legacy shader/cache bootstrap의 `CDX11Device` downcast 2곳은 Step 3 이관 전까지 남는다. DX11/DX12 module만 등록하며 Vulkan은 enum만 있으므로 `module_not_registered`로 fail-closed 한다.
⑤ 대가: 정적 registry는 동적 DLL module loader가 아니며 preflight도 adapter/driver를 실제 질의하지 않는다. 실제 capability는 현재 enum 추정치라 이번 slice에서 report하지 않고 Step 2의 native query/conformance로 남긴다.

## 2. 반영해야 하는 코드

### 2-1. 새 파일: C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHIBackendRegistry.h

```cpp
#pragma once

#include "EngineConfig.h"
#include "RHI/IRHIDevice.h"

#include <memory>
#include <string>

struct RHIBackendCreateDesc
{
    void* nativeWindow = nullptr;
    u32_t width = 0;
    u32_t height = 0;
    bool_t vsync = true;
    bool_t fullscreen = false;
};

struct RHIBackendProbeResult
{
    bool_t ready = false;
    std::string reason = "backend_probe_rejected";
};

enum class eRHIBackendCreateStatus : u32_t
{
    Success = 0,
    ModuleNotRegistered,
    ProbeRejected,
    DeviceCreationFailed,
    BackendIdentityMismatch,
};

struct RHIBackendCreateResult
{
    eRHIBackendCreateStatus status = eRHIBackendCreateStatus::ModuleNotRegistered;
    std::string moduleName = "None";
    std::string reason = "backend_module_not_registered";
    std::unique_ptr<IRHIDevice> pDevice{};

    bool_t Succeeded() const
    {
        return status == eRHIBackendCreateStatus::Success && pDevice != nullptr;
    }
};

class IRHIBackendModule
{
public:
    virtual ~IRHIBackendModule() = default;

    virtual eEngineRHIBackend GetEngineBackend() const = 0;
    virtual eRHIBackend GetRuntimeBackend() const = 0;
    virtual const char* GetName() const = 0;
    virtual RHIBackendProbeResult Probe(const RHIBackendCreateDesc& desc) const = 0;
    virtual std::unique_ptr<IRHIDevice> CreateDevice(
        const RHIBackendCreateDesc& desc) const = 0;
};

class CRHIBackendRegistry final
{
public:
    static const IRHIBackendModule* FindModule(eEngineRHIBackend backend);
    static RHIBackendCreateResult CreateDevice(
        eEngineRHIBackend backend,
        const RHIBackendCreateDesc& desc);
};

const char* RHIBackendCreateStatusName(eRHIBackendCreateStatus status);
```

### 2-2. 새 파일: C:/Users/user/Desktop/Winters/Engine/Private/RHI/RHIBackendRegistry.cpp

```cpp
#include "WintersPCH.h"
#include "RHI/RHIBackendRegistry.h"

#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX12/DX12Device.h"

#include <array>
#include <utility>

namespace
{
    RHIBackendProbeResult ProbeWin32CreateDesc(const RHIBackendCreateDesc& desc)
    {
        if (!desc.nativeWindow)
            return { false, "native_window_missing" };
        if (desc.width == 0 || desc.height == 0)
            return { false, "surface_extent_invalid" };
        return { true, "module_ready" };
    }

    class CDX11BackendModule final : public IRHIBackendModule
    {
    public:
        eEngineRHIBackend GetEngineBackend() const override
        {
            return eEngineRHIBackend::DX11;
        }

        eRHIBackend GetRuntimeBackend() const override
        {
            return eRHIBackend::DX11;
        }

        const char* GetName() const override
        {
            return "DX11";
        }

        RHIBackendProbeResult Probe(const RHIBackendCreateDesc& desc) const override
        {
            return ProbeWin32CreateDesc(desc);
        }

        std::unique_ptr<IRHIDevice> CreateDevice(
            const RHIBackendCreateDesc& desc) const override
        {
            DeviceDesc deviceDesc{};
            deviceDesc.hwnd = static_cast<HWND>(desc.nativeWindow);
            deviceDesc.width = desc.width;
            deviceDesc.height = desc.height;
            deviceDesc.vsync = desc.vsync;
            deviceDesc.fullscreen = desc.fullscreen;
            return CDX11Device::Create(deviceDesc);
        }
    };

    class CDX12BackendModule final : public IRHIBackendModule
    {
    public:
        eEngineRHIBackend GetEngineBackend() const override
        {
            return eEngineRHIBackend::DX12;
        }

        eRHIBackend GetRuntimeBackend() const override
        {
            return eRHIBackend::DX12;
        }

        const char* GetName() const override
        {
            return "DX12";
        }

        RHIBackendProbeResult Probe(const RHIBackendCreateDesc& desc) const override
        {
            return ProbeWin32CreateDesc(desc);
        }

        std::unique_ptr<IRHIDevice> CreateDevice(
            const RHIBackendCreateDesc& desc) const override
        {
            DX12DeviceDesc deviceDesc{};
            deviceDesc.hwnd = static_cast<HWND>(desc.nativeWindow);
            deviceDesc.width = desc.width;
            deviceDesc.height = desc.height;
            deviceDesc.vsync = desc.vsync;
            deviceDesc.fullscreen = desc.fullscreen;
            return CDX12Device::Create(deviceDesc);
        }
    };

    const CDX11BackendModule g_DX11BackendModule{};
    const CDX12BackendModule g_DX12BackendModule{};

    const std::array<const IRHIBackendModule*, 2> g_BackendModules = {
        &g_DX11BackendModule,
        &g_DX12BackendModule,
    };
}

const IRHIBackendModule* CRHIBackendRegistry::FindModule(eEngineRHIBackend backend)
{
    for (const IRHIBackendModule* pModule : g_BackendModules)
    {
        if (pModule && pModule->GetEngineBackend() == backend)
            return pModule;
    }
    return nullptr;
}

RHIBackendCreateResult CRHIBackendRegistry::CreateDevice(
    eEngineRHIBackend backend,
    const RHIBackendCreateDesc& desc)
{
    RHIBackendCreateResult result{};
    const IRHIBackendModule* pModule = FindModule(backend);
    if (!pModule)
        return result;

    result.moduleName = pModule->GetName();

    const RHIBackendProbeResult probe = pModule->Probe(desc);
    if (!probe.ready)
    {
        result.status = eRHIBackendCreateStatus::ProbeRejected;
        result.reason = probe.reason;
        return result;
    }

    result.pDevice = pModule->CreateDevice(desc);
    if (!result.pDevice)
    {
        result.status = eRHIBackendCreateStatus::DeviceCreationFailed;
        result.reason = "native_device_creation_failed";
        return result;
    }

    if (result.pDevice->GetBackend() != pModule->GetRuntimeBackend())
    {
        result.pDevice.reset();
        result.status = eRHIBackendCreateStatus::BackendIdentityMismatch;
        result.reason = "created_device_backend_mismatch";
        return result;
    }

    result.status = eRHIBackendCreateStatus::Success;
    result.reason = "device_created";
    return result;
}

const char* RHIBackendCreateStatusName(eRHIBackendCreateStatus status)
{
    switch (status)
    {
    case eRHIBackendCreateStatus::Success: return "success";
    case eRHIBackendCreateStatus::ModuleNotRegistered: return "module_not_registered";
    case eRHIBackendCreateStatus::ProbeRejected: return "probe_rejected";
    case eRHIBackendCreateStatus::DeviceCreationFailed: return "device_creation_failed";
    case eRHIBackendCreateStatus::BackendIdentityMismatch: return "backend_identity_mismatch";
    default: return "unknown";
    }
}
```

### 2-3. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
#include "ProfilerAPI.h"
#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/BlendStateCache.h"
#include "RHI/DX12/DX12Device.h"
```

아래로 교체:

```cpp
#include "ProfilerAPI.h"
#include "RHI/RHIBackendRegistry.h"
#include "RHI/DX11/CDX11Device.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/BlendStateCache.h"
```

기존 함수 전체:

```cpp
    bool_t WriteRHISelectionProbeReport(
        const std::wstring& path,
        eEngineRHIBackend requested,
        const char* pSelected,
        const char* pStatus)
    {
        if (path.empty() || !pSelected || !pStatus)
            return false;

        const std::wstring sourceValue = ReadProcessEnvironmentValue(
            L"WINTERS_INTERNAL_RHI_REQUEST_SOURCE");
        const char* pSource = "unknown";
        if (_wcsicmp(sourceValue.c_str(), L"command-line") == 0)
            pSource = "command-line";
        else if (_wcsicmp(sourceValue.c_str(), L"environment") == 0)
            pSource = "environment";
        else if (_wcsicmp(sourceValue.c_str(), L"default") == 0)
            pSource = "default";

        char report[320]{};
        sprintf_s(
            report,
            "source=%s\nrequested=%s\nselected=%s\nstatus=%s\n",
            pSource,
            EngineRHIBackendName(requested),
            pSelected,
            pStatus);

        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        DWORD bytesWritten = 0;
        const BOOL written = WriteFile(
            file,
            report,
            static_cast<DWORD>(std::strlen(report)),
            &bytesWritten,
            nullptr);
        CloseHandle(file);
        return written && bytesWritten == std::strlen(report);
    }
```

아래로 교체:

```cpp
    bool_t WriteRHISelectionProbeReport(
        const std::wstring& path,
        eEngineRHIBackend requested,
        const char* pModule,
        const char* pSelected,
        const char* pStatus,
        const char* pReason,
        bool_t bFallbackUsed,
        const char* pFallbackReason)
    {
        if (path.empty() || !pModule || !pSelected || !pStatus ||
            !pReason || !pFallbackReason)
        {
            return false;
        }

        const std::wstring sourceValue = ReadProcessEnvironmentValue(
            L"WINTERS_INTERNAL_RHI_REQUEST_SOURCE");
        const char* pSource = "unknown";
        if (_wcsicmp(sourceValue.c_str(), L"command-line") == 0)
            pSource = "command-line";
        else if (_wcsicmp(sourceValue.c_str(), L"environment") == 0)
            pSource = "environment";
        else if (_wcsicmp(sourceValue.c_str(), L"default") == 0)
            pSource = "default";

        char report[640]{};
        sprintf_s(
            report,
            "source=%s\nrequested=%s\nmodule=%s\nselected=%s\n"
            "status=%s\nreason=%s\nfallback=%s\nfallback_reason=%s\n",
            pSource,
            EngineRHIBackendName(requested),
            pModule,
            pSelected,
            pStatus,
            pReason,
            bFallbackUsed ? "yes" : "no",
            pFallbackReason);

        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        DWORD bytesWritten = 0;
        const BOOL written = WriteFile(
            file,
            report,
            static_cast<DWORD>(std::strlen(report)),
            &bytesWritten,
            nullptr);
        CloseHandle(file);
        return written && bytesWritten == std::strlen(report);
    }
```

기존 코드:

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

삭제.

기존 window 실패 report 호출:

```cpp
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            "None",
            "window_creation_failed");
```

아래로 교체:

```cpp
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            "None",
            "None",
            "window_creation_failed",
            "window_creation_failed",
            false,
            "none");
```

기존 `const auto tryDX11` 줄부터 첫 `if (!m_pDevice)` 블록의 `return false;`와 닫는 중괄호까지 아래로 교체한다. 뒤의 `#if 0` legacy message block은 함께 삭제한다.

```cpp
    RHIBackendCreateDesc rhiCreateDesc{};
    rhiCreateDesc.nativeWindow = m_Window.GetHandle();
    rhiCreateDesc.width = config.windowWidth;
    rhiCreateDesc.height = config.windowHeight;
    rhiCreateDesc.vsync = ShouldUsePresentationVSync(config);
    rhiCreateDesc.fullscreen = config.fullscreen;

    const eEngineRHIBackend primaryBackend =
        config.rhiBackend == eEngineRHIBackend::Auto
        ? eEngineRHIBackend::DX11
        : config.rhiBackend;

    RHIBackendCreateResult createResult =
        CRHIBackendRegistry::CreateDevice(primaryBackend, rhiCreateDesc);
    bool_t bFallbackUsed = false;
    std::string fallbackReason = "none";

    if (!createResult.Succeeded() && config.allowRHIFallback &&
        primaryBackend != eEngineRHIBackend::DX11)
    {
        bFallbackUsed = true;
        fallbackReason = createResult.reason;
        OutputDebugStringA("[CEngineApp] Falling back to DX11 backend\n");
        createResult = CRHIBackendRegistry::CreateDevice(
            eEngineRHIBackend::DX11,
            rhiCreateDesc);
    }

    if (createResult.Succeeded())
    {
        m_pDevice = std::move(createResult.pDevice);

        char selectedTrace[192]{};
        sprintf_s(
            selectedTrace,
            "[CEngineApp] RHI module=%s selected=%s reason=%s\n",
            createResult.moduleName.c_str(),
            RHIBackendName(m_pDevice->GetBackend()),
            createResult.reason.c_str());
        OutputDebugStringA(selectedTrace);
    }

    if (!m_pDevice)
    {
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            createResult.moduleName.c_str(),
            "None",
            RHIBackendCreateStatusName(createResult.status),
            createResult.reason.c_str(),
            bFallbackUsed,
            fallbackReason.c_str());
        if (!bRHIProbeOnly)
        {
            MessageBoxW(m_Window.GetHandle(),
                L"RHI device initialization failed.\n"
                L"Check the selected backend and graphics driver support.",
                L"[CEngineApp] RHI initialization failed", MB_OK | MB_ICONERROR);
        }
        return false;
    }
```

기존 성공 report 호출:

```cpp
    WriteRHISelectionProbeReport(
        rhiProbeReportPath,
        config.rhiBackend,
        RHIBackendName(m_pDevice->GetBackend()),
        "success");
```

아래로 교체:

```cpp
    WriteRHISelectionProbeReport(
        rhiProbeReportPath,
        config.rhiBackend,
        createResult.moduleName.c_str(),
        RHIBackendName(m_pDevice->GetBackend()),
        RHIBackendCreateStatusName(createResult.status),
        createResult.reason.c_str(),
        bFallbackUsed,
        fallbackReason.c_str());
```

### 2-4. C:/Users/user/Desktop/Winters/Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1

`Invoke-ProbeCase` 매개변수에 아래를 `ExpectedRequested` 바로 다음에 추가한다.

```powershell
        [string]$ExpectedModule,
```

`ExpectedStatus` 바로 다음에 아래를 추가한다.

```powershell
        [string]$ExpectedReason,
        [string]$ExpectedFallback,
        [string]$ExpectedFallbackReason,
```

기존 report 필수 key 및 비교 블록:

```powershell
            foreach ($key in @('source', 'requested', 'selected', 'status')) {
                if (-not $report.ContainsKey($key)) {
                    throw "$Name report missing key: $key"
                }
            }
            if ($report.source -ne $ExpectedSource -or
                $report.requested -ne $ExpectedRequested -or
                $report.selected -ne $ExpectedSelected -or
                $report.status -ne $ExpectedStatus) {
                throw "$Name report mismatch: $($report | Out-String)"
            }
```

아래로 교체:

```powershell
            foreach ($key in @(
                'source', 'requested', 'module', 'selected',
                'status', 'reason', 'fallback', 'fallback_reason')) {
                if (-not $report.ContainsKey($key)) {
                    throw "$Name report missing key: $key"
                }
            }
            if ($report.source -ne $ExpectedSource -or
                $report.requested -ne $ExpectedRequested -or
                $report.module -ne $ExpectedModule -or
                $report.selected -ne $ExpectedSelected -or
                $report.status -ne $ExpectedStatus -or
                $report.reason -ne $ExpectedReason -or
                $report.fallback -ne $ExpectedFallback -or
                $report.fallback_reason -ne $ExpectedFallbackReason) {
                throw "$Name report mismatch: $($report | Out-String)"
            }
```

5개 report 생성 case의 호출은 아래로 교체한다. parser 단계에서 실패하는 나머지 3개 case는 새 예상 인자에 빈 문자열을 전달한다.

```powershell
Invoke-ProbeCase -Name 'default_auto' -EnvironmentRHI '' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'default' -ExpectedRequested 'Auto' `
    -ExpectedModule 'DX11' -ExpectedSelected 'DX11' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_dx11' -EnvironmentRHI 'dx11' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'DX11' `
    -ExpectedModule 'DX11' -ExpectedSelected 'DX11' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_dx12' -EnvironmentRHI 'dx12' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'DX12' `
    -ExpectedModule 'DX12' -ExpectedSelected 'DX12' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_vulkan' -EnvironmentRHI 'vulkan' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'Vulkan' `
    -ExpectedModule 'None' -ExpectedSelected 'None' `
    -ExpectedStatus 'module_not_registered' `
    -ExpectedReason 'backend_module_not_registered' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'cli_precedence' -EnvironmentRHI 'dx11' -Arguments @('--rhi=dx12') `
    -ExpectReport $true -ExpectedSource 'command-line' -ExpectedRequested 'DX12' `
    -ExpectedModule 'DX12' -ExpectedSelected 'DX12' -ExpectedStatus 'success' `
    -ExpectedReason 'device_created' -ExpectedFallback 'no' `
    -ExpectedFallbackReason 'none' -ExpectZeroExit $true
```

기존 parser 실패 3개 case:

```powershell
Invoke-ProbeCase -Name 'empty_cli' -EnvironmentRHI 'dx11' -Arguments @('--rhi=') `
    -ExpectReport $false -ExpectedSource '' -ExpectedRequested '' `
    -ExpectedSelected '' -ExpectedStatus '' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'duplicate_cli' -EnvironmentRHI '' `
    -Arguments @('--rhi=dx11', '--rhi=dx12') -ExpectReport $false `
    -ExpectedSource '' -ExpectedRequested '' -ExpectedSelected '' `
    -ExpectedStatus '' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'invalid_env' -EnvironmentRHI 'd3d11' -Arguments @() `
    -ExpectReport $false -ExpectedSource '' -ExpectedRequested '' `
    -ExpectedSelected '' -ExpectedStatus '' -ExpectZeroExit $false
```

아래로 교체:

```powershell
Invoke-ProbeCase -Name 'empty_cli' -EnvironmentRHI 'dx11' -Arguments @('--rhi=') `
    -ExpectReport $false -ExpectedSource '' -ExpectedRequested '' `
    -ExpectedModule '' -ExpectedSelected '' -ExpectedStatus '' -ExpectedReason '' `
    -ExpectedFallback '' -ExpectedFallbackReason '' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'duplicate_cli' -EnvironmentRHI '' `
    -Arguments @('--rhi=dx11', '--rhi=dx12') -ExpectReport $false `
    -ExpectedSource '' -ExpectedRequested '' -ExpectedModule '' `
    -ExpectedSelected '' -ExpectedStatus '' -ExpectedReason '' `
    -ExpectedFallback '' -ExpectedFallbackReason '' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'invalid_env' -EnvironmentRHI 'd3d11' -Arguments @() `
    -ExpectReport $false -ExpectedSource '' -ExpectedRequested '' `
    -ExpectedModule '' -ExpectedSelected '' -ExpectedStatus '' -ExpectedReason '' `
    -ExpectedFallback '' -ExpectedFallbackReason '' -ExpectZeroExit $false
```

### 2-5. 프로젝트 포함

확인 필요: 새 `RHIBackendRegistry.cpp/.h`를 `Engine/Include/Engine.vcxproj`의 owning RHI 항목에 포함한다. `.vcxproj.filters`는 가상 폴더 표시용이므로 빌드 성공의 근거로 사용하지 않으며, 기존 `02. RHI/00. Interface` 아래에만 배치한다.

## 3. 검증

예측:
- `CEngineApp.cpp`에는 `DX12Device.h`, `CDX11Device::Create`, `CDX12Device::Create`, `CreateDX11DeviceForWindow`, `CreateDX12DeviceForWindow`가 0건이고 새 registry cpp만 concrete Device를 생성한다. `CDX11Device.h`와 downcast 2곳은 legacy shader/cache bootstrap 때문에 의도적으로 남는다.
- 기본 Auto, 명시적 DX11, 명시적 DX12는 module/selected/status/reason/fallback 8개 key가 예상과 일치한다.
- Vulkan은 `device_initialization_failed`가 아니라 `module=None`, `module_not_registered`, `backend_module_not_registered`로 실패한다.
- Engine→Client 순서 Debug 빌드와 일반 DX11 2초 제품 smoke가 exit 0이다.
- 기존 renderer/resource의 DX11 직접 접근은 이번 slice 범위가 아니므로 아직 남는다.

검증 명령:

```powershell
rg -n "CDX11Device::Create|CDX12Device::Create|CreateDX11DeviceForWindow|CreateDX12DeviceForWindow|RHI/DX12/DX12Device.h" Engine/Private/Framework/CEngineApp.cpp
rg -n "CDX11Device::Create|CDX12Device::Create" Engine/Private/RHI/RHIBackendRegistry.cpp
```

```powershell
git diff --check -- Engine/Private/Framework/CEngineApp.cpp Engine/Private/RHI/RHIBackendRegistry.h Engine/Private/RHI/RHIBackendRegistry.cpp Engine/Include/Engine.vcxproj Engine/Include/Engine.vcxproj.filters Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 .md/plan/2026-07-21_RHI_BACKEND_REGISTRY_FACTORY_PLAN.md
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /t:Build /m:1 /p:Configuration=Debug /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /t:Build /m:1 /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug
```

```powershell
Remove-Item Env:WINTERS_RHI_PROBE_PATH -ErrorAction SilentlyContinue
$env:WINTERS_RHI='dx11'
$p = Start-Process -FilePath (Resolve-Path '.\Client\Bin\Debug\WintersGame.exe').Path -WorkingDirectory (Resolve-Path '.\Client\Bin\Debug').Path -ArgumentList '--run-seconds=2' -WindowStyle Hidden -Wait -PassThru
Remove-Item Env:WINTERS_RHI -ErrorAction SilentlyContinue
if ($p.ExitCode -ne 0) { throw "DX11 product smoke failed: $($p.ExitCode)" }
```

미검증:
- DX12 제품 화면 parity, Vulkan Device, 실제 adapter capability, shader package 지원은 증명하지 않는다.
- Registry의 preflight는 Win32 surface 입력 정합성만 검사하며 native API/driver 지원은 concrete Device 생성 결과로 판정한다.

확인 필요:
- 구현 직전 대상 경로가 다른 세션 변경과 겹치지 않는지 path-scoped status를 다시 확인한다.
- 독립 비평에서 P0/P1 0건이 될 때까지 구현하지 않는다.

## 서브 에이전트 비평

### 1차 비평 — `/root/rhi_registry_critique`

- P0: 0건.
- P1 raw non-owning reason 수명: 수용. `RHIBackendProbeResult::reason`, `RHIBackendCreateResult::moduleName/reason`을 `std::string` 소유값으로 바꾸고, fallback 전 첫 실패 이유를 `fallbackReason`에 복사한 뒤 결과를 덮어쓰도록 수정했다.
- primary 자체 감사: `CEngineApp`의 legacy shader/cache가 `CDX11Device` downcast 2곳을 계속 요구함을 발견했다. 이번 완료선을 concrete Device 생성 제거로 정정하고 `CDX11Device.h`는 Step 3까지 유지한다.

수정 후 델타 재비평 대기. P0/P1 잔존 0 확인 전 소스 구현 금지.

### 2차 최종 델타 비평 — `/root/rhi_registry_critique`

- 판정: PASS.
- P0: 0건.
- P1: 0건.
- owning diagnostic 수명, fallback 전 복사, `.c_str()`의 동기 호출 한정 사용을 확인했다.
- Device 생성 호출/DX12 header 제거와 legacy `CDX11Device` downcast 잔존 경계를 확인했다.
- 비차단 P2: fallback=yes, ProbeRejected, DeviceCreationFailed, BackendIdentityMismatch를 강제하는 전용 테스트는 후속 conformance coverage로 남긴다. 이번 slice에서 runtime parity 증거로 주장하지 않는다.

독립 비평 통과선을 충족했으므로 계획 범위의 소스 구현을 시작할 수 있다.
