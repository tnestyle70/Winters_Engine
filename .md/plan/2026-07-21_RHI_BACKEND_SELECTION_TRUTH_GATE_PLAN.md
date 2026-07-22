Session - LoL 실행의 RHI 요청 출처와 실제 선택 backend를 일치시키고 명시적 DX11·DX12·Vulkan 요청의 silent fallback을 제거한다.
좌표: 신규 좌표 후보 RHI-P0A · 축: C7 권위와 정합성, C8 검증이 병목
관련: 2026-07-13_WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_PLAN.md, 2026-07-15_UNREAL_SYNCED_RHI_RENDER_GRAPH_DX12_VULKAN_PLAN.md

## 1. 결정 기록

① 문제·제약: 현재 LoL은 `--rhi=dx11|dx12|null` 3개 substring만 읽고 기본 DX11과 `allowRHIFallback=true`를 사용해, 명시적 DX12/Vulkan 검증이 실제 DX11 실행으로 바뀔 수 있다(환경변수 선택 0개, Vulkan parser 0개).
② 순진한 해법의 실패: 환경변수 문자열과 `--rhi=dx12`만 추가하면 modal 초기화 실패도 “프로세스 생존” smoke를 통과하고, requested/selected를 수집하지 않아 backend identity를 증명하지 못한다.
③ 메커니즘: 정확한 argv 토큰 우선, 없으면 `WINTERS_RHI`, 둘 다 없으면 `Auto`로 요청한다. 전용 probe가 source/requested/selected/status를 기록하고 LoL의 명시적 요청은 fallback을 끈다.
④ 대조: 전역 `EngineConfig::allowRHIFallback` 계약과 Elden 호출자는 바꾸지 않는다. Registry/Vulkan Device 전에 LoL 선택 의미와 검증 하네스만 먼저 닫는다.
⑤ 대가: probe용 환경변수와 작은 key-value 보고 경로가 추가된다. CLI가 환경변수보다 우선하지만 report에 source와 requested가 남으며, Vulkan 요청은 backend 구현 전까지 의도적으로 실패한다.

## 2. 반영해야 하는 코드

### 2-1. C:/Users/user/Desktop/Winters/Client/Private/main.cpp

기존 코드:

```cpp
#include <Windows.h>
```

아래에 추가:

```cpp
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")
```

기존 코드:

```cpp
    eEngineRHIBackend ParseRequestedRHIBackend()
    {
        if (HasCommandLineFlag(L"--rhi=dx11", L"/rhi:dx11"))
            return eEngineRHIBackend::DX11;
        if (HasCommandLineFlag(L"--rhi=dx12", L"/rhi:dx12"))
            return eEngineRHIBackend::DX12;
        if (HasCommandLineFlag(L"--rhi=null", L"/rhi:null"))
            return eEngineRHIBackend::Null;

        return eEngineRHIBackend::DX11;
    }
```

아래로 교체:

```cpp
    struct RHIRequestParseResult
    {
        eEngineRHIBackend backend = eEngineRHIBackend::Auto;
        bool_t valid = true;
        const wchar_t* pSource = L"default";
        std::wstring value = L"auto";
        std::wstring error{};
    };

    std::wstring ReadEnvironmentValue(const wchar_t* pName)
    {
        if (!pName)
            return {};

        const DWORD required = ::GetEnvironmentVariableW(pName, nullptr, 0);
        if (required == 0)
            return {};

        std::wstring value(required, L'\0');
        const DWORD written = ::GetEnvironmentVariableW(
            pName,
            value.data(),
            required);
        if (written == 0 || written >= required)
            return {};

        value.resize(written);
        return value;
    }

    const wchar_t* FindRHIArgumentValue(const wchar_t* pArgument)
    {
        if (!pArgument)
            return nullptr;

        constexpr wchar_t kLongPrefix[] = L"--rhi=";
        constexpr wchar_t kShortPrefix[] = L"/rhi:";
        constexpr size_t kLongPrefixLength = (sizeof(kLongPrefix) / sizeof(wchar_t)) - 1;
        constexpr size_t kShortPrefixLength = (sizeof(kShortPrefix) / sizeof(wchar_t)) - 1;

        if (_wcsnicmp(pArgument, kLongPrefix, kLongPrefixLength) == 0)
            return pArgument + kLongPrefixLength;
        if (_wcsnicmp(pArgument, kShortPrefix, kShortPrefixLength) == 0)
            return pArgument + kShortPrefixLength;
        return nullptr;
    }

    bool_t TryParseRHIBackendName(
        const std::wstring& value,
        eEngineRHIBackend& outBackend)
    {
        if (_wcsicmp(value.c_str(), L"auto") == 0)
            outBackend = eEngineRHIBackend::Auto;
        else if (_wcsicmp(value.c_str(), L"dx11") == 0)
            outBackend = eEngineRHIBackend::DX11;
        else if (_wcsicmp(value.c_str(), L"dx12") == 0)
            outBackend = eEngineRHIBackend::DX12;
        else if (_wcsicmp(value.c_str(), L"vulkan") == 0)
            outBackend = eEngineRHIBackend::Vulkan;
        else
            return false;

        return true;
    }

    RHIRequestParseResult ParseRequestedRHIBackend()
    {
        RHIRequestParseResult result{};
        int argumentCount = 0;
        LPWSTR* ppArguments = ::CommandLineToArgvW(
            GetCommandLineText(),
            &argumentCount);
        if (!ppArguments)
        {
            result.valid = false;
            result.error = L"CommandLineToArgvW failed.";
            return result;
        }

        u32_t rhiArgumentCount = 0;
        for (int i = 1; i < argumentCount; ++i)
        {
            const wchar_t* pValue = FindRHIArgumentValue(ppArguments[i]);
            if (!pValue)
                continue;

            ++rhiArgumentCount;
            result.pSource = L"command-line";
            result.value = pValue;
        }
        ::LocalFree(ppArguments);

        if (rhiArgumentCount > 1)
        {
            result.valid = false;
            result.error = L"Specify exactly one --rhi=<value> argument.";
            return result;
        }

        if (rhiArgumentCount == 1 && result.value.empty())
        {
            result.valid = false;
            result.error = L"The --rhi value must not be empty.";
            return result;
        }

        if (rhiArgumentCount == 0)
        {
            const std::wstring environmentValue =
                ReadEnvironmentValue(L"WINTERS_RHI");
            if (!environmentValue.empty())
            {
                result.pSource = L"environment";
                result.value = environmentValue;
            }
        }

        if (!TryParseRHIBackendName(result.value, result.backend))
        {
            result.valid = false;
            result.error = L"Use auto, dx11, dx12, or vulkan.";
        }

        return result;
    }
```

기존 코드:

```cpp
    CGameApp gameApp;

    EngineConfig config;
    config.windowTitle = L"LOL";
    config.rhiBackend = ParseRequestedRHIBackend();
    config.allowRHIFallback = true;
```

아래로 교체:

```cpp
    const RHIRequestParseResult rhiRequest = ParseRequestedRHIBackend();
    if (!rhiRequest.valid)
    {
        std::wstring message = L"Invalid RHI request: ";
        message += rhiRequest.value;
        message += L"\n";
        message += rhiRequest.error;
        ::OutputDebugStringW((L"[WintersGame] " + message + L"\n").c_str());

        if (ReadEnvironmentValue(L"WINTERS_RHI_PROBE_PATH").empty())
        {
            ::MessageBoxW(
                nullptr,
                message.c_str(),
                L"[Winters] Invalid RHI request",
                MB_OK | MB_ICONERROR);
        }
        return 2;
    }

    wchar_t rhiRequestTrace[192]{};
    swprintf_s(
        rhiRequestTrace,
        L"[WintersGame] RHI request source=%ls value=%ls\n",
        rhiRequest.pSource,
        rhiRequest.value.c_str());
    ::OutputDebugStringW(rhiRequestTrace);
    ::SetEnvironmentVariableW(
        L"WINTERS_INTERNAL_RHI_REQUEST_SOURCE",
        rhiRequest.pSource);

    CGameApp gameApp;

    EngineConfig config;
    config.windowTitle = L"LOL";
    config.rhiBackend = rhiRequest.backend;
    config.allowRHIFallback = rhiRequest.backend == eEngineRHIBackend::Auto;
```

`null`은 public enum에는 남아 있지만 실제 Null device가 없으므로 LoL 사용자 입력 허용 목록에서는 제거한다.

### 2-2. C:/Users/user/Desktop/Winters/Engine/Private/Framework/CEngineApp.cpp

기존 코드:

```cpp
#include <chrono>
#include <thread>
#include <timeapi.h>
```

아래로 교체:

```cpp
#include <chrono>
#include <cstring>
#include <string>
#include <thread>
#include <timeapi.h>
```

기존 코드:

```cpp
    bool_t ShouldUsePresentationVSync(const EngineConfig& config)
    {
        return config.vsync && config.targetFPS == 0u;
    }
```

아래에 추가:

```cpp
    const char* EngineRHIBackendName(eEngineRHIBackend backend)
    {
        switch (backend)
        {
        case eEngineRHIBackend::Auto: return "Auto";
        case eEngineRHIBackend::DX12: return "DX12";
        case eEngineRHIBackend::DX11: return "DX11";
        case eEngineRHIBackend::Null: return "Null";
        case eEngineRHIBackend::Vulkan: return "Vulkan";
        case eEngineRHIBackend::Metal: return "Metal";
        case eEngineRHIBackend::Xbox: return "Xbox";
        case eEngineRHIBackend::PS5: return "PS5";
        default: return "Unknown";
        }
    }

    const char* RHIBackendName(eRHIBackend backend)
    {
        switch (backend)
        {
        case eRHIBackend::DX11: return "DX11";
        case eRHIBackend::DX12: return "DX12";
        case eRHIBackend::Vulkan: return "Vulkan";
        case eRHIBackend::Metal: return "Metal";
        case eRHIBackend::Xbox: return "Xbox";
        case eRHIBackend::PS5: return "PS5";
        default: return "Unknown";
        }
    }

    std::wstring ReadProcessEnvironmentValue(const wchar_t* pName)
    {
        if (!pName)
            return {};

        const DWORD required = GetEnvironmentVariableW(pName, nullptr, 0);
        if (required == 0)
            return {};

        std::wstring value(required, L'\0');
        const DWORD written = GetEnvironmentVariableW(
            pName,
            value.data(),
            required);
        if (written == 0 || written >= required)
            return {};

        value.resize(written);
        return value;
    }

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

기존 코드:

```cpp
    m_bProfileCaptureOnExit = config.profileCaptureOnExit;

    if (config.vsync && m_uTargetFPS > 0u)
```

아래로 교체:

```cpp
    m_bProfileCaptureOnExit = config.profileCaptureOnExit;

    const std::wstring rhiProbeReportPath = ReadProcessEnvironmentValue(
        L"WINTERS_RHI_PROBE_PATH");
    const bool_t bRHIProbeOnly = !rhiProbeReportPath.empty();

    char requestedTrace[160]{};
    sprintf_s(
        requestedTrace,
        "[CEngineApp] RHI requested=%s fallback=%s\n",
        EngineRHIBackendName(config.rhiBackend),
        config.allowRHIFallback ? "allowed" : "disabled");
    OutputDebugStringA(requestedTrace);

    if (config.vsync && m_uTargetFPS > 0u)
```

기존 코드:

```cpp
    if (!m_Window.Create(wndDesc))
    {
        OutputDebugStringA("[CEngineApp] Window creation failed\n");
        return false;
    }
```

아래로 교체:

```cpp
    if (!m_Window.Create(wndDesc))
    {
        OutputDebugStringA("[CEngineApp] Window creation failed\n");
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            "None",
            "window_creation_failed");
        return false;
    }
```

기존 fallback 블록은 그대로 둔다. LoL main이 `Auto`일 때만 `allowRHIFallback=true`를 전달하므로 명시적 LoL DX11·DX12·Vulkan 요청에는 실행되지 않는다. Elden 등 다른 호출자의 기존 계약은 이번 slice에서 변경하지 않는다.

기존 코드:

```cpp
    if (!m_pDevice)
    {
        MessageBoxW(m_Window.GetHandle(),
            L"RHI device initialization failed.\n"
            L"Check the selected backend and graphics driver support.",
            L"[CEngineApp] RHI initialization failed", MB_OK | MB_ICONERROR);
        return false;
```

아래로 교체:

```cpp
    if (!m_pDevice)
    {
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            "None",
            "device_initialization_failed");
        if (!bRHIProbeOnly)
        {
            MessageBoxW(m_Window.GetHandle(),
                L"RHI device initialization failed.\n"
                L"Check the selected backend and graphics driver support.",
                L"[CEngineApp] RHI initialization failed", MB_OK | MB_ICONERROR);
        }
        return false;
```

위 교체는 기존 `#if 0` 이하 블록을 그대로 유지한다.

기존 코드:

```cpp
#endif
    }


    m_bSceneRuntimeEnabled = true;
```

아래로 교체:

```cpp
#endif
    }

    WriteRHISelectionProbeReport(
        rhiProbeReportPath,
        config.rhiBackend,
        RHIBackendName(m_pDevice->GetBackend()),
        "success");
    if (bRHIProbeOnly)
    {
        m_bRunning = false;
        return true;
    }

    m_bSceneRuntimeEnabled = true;
```

probe mode는 Device·SwapChain 생성과 실제 backend identity까지만 확인하고 ImGui/Scene/제품 bootstrap 전에 정상 종료한다. `CEngineApp::Run()`은 `m_bRunning=false`를 보고 loop 없이 `Shutdown()`하여 exit code 0을 반환한다.

### 2-3. 새 파일: C:/Users/user/Desktop/Winters/Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1

```powershell
[CmdletBinding()]
param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 3.0

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$exePath = Join-Path $repoRoot "Client\Bin\$Configuration\WintersGame.exe"
$workingDirectory = Split-Path -Parent $exePath

if (-not (Test-Path $exePath)) {
    throw "WintersGame.exe not found: $exePath"
}

function Read-ProbeReport {
    param([string]$Path)

    $result = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        $parts = $line -split '=', 2
        if ($parts.Count -eq 2) {
            $result[$parts[0]] = $parts[1]
        }
    }
    return $result
}

function Invoke-ProbeCase {
    param(
        [string]$Name,
        [AllowEmptyString()][string]$EnvironmentRHI,
        [string[]]$Arguments,
        [bool]$ExpectReport,
        [string]$ExpectedSource,
        [string]$ExpectedRequested,
        [string]$ExpectedSelected,
        [string]$ExpectedStatus,
        [bool]$ExpectZeroExit
    )

    $reportPath = Join-Path $env:TEMP (
        "winters_rhi_truth_{0}_{1}.txt" -f $Name, [guid]::NewGuid().ToString('N'))

    $previousRHI = $env:WINTERS_RHI
    $previousProbePath = $env:WINTERS_RHI_PROBE_PATH
    try {
        if ([string]::IsNullOrEmpty($EnvironmentRHI)) {
            Remove-Item Env:WINTERS_RHI -ErrorAction SilentlyContinue
        }
        else {
            $env:WINTERS_RHI = $EnvironmentRHI
        }
        $env:WINTERS_RHI_PROBE_PATH = $reportPath

        $startArgs = @{
            FilePath = $exePath
            WorkingDirectory = $workingDirectory
            PassThru = $true
            WindowStyle = 'Hidden'
        }
        if ($Arguments.Count -gt 0) {
            $startArgs.ArgumentList = $Arguments
        }

        $process = Start-Process @startArgs
        if (-not $process.WaitForExit(15000)) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
            if (Test-Path -LiteralPath $reportPath) {
                Write-Host "[RHITruthGate] $Name report before timeout:"
                Get-Content -LiteralPath $reportPath | Write-Host
            }
            throw "$Name timed out after 15 seconds"
        }
        if ($ExpectZeroExit -and $process.ExitCode -ne 0) {
            throw "$Name expected exit 0, got $($process.ExitCode)"
        }
        if (-not $ExpectZeroExit -and $process.ExitCode -eq 0) {
            throw "$Name expected non-zero exit, got 0"
        }

        if ($ExpectReport) {
            if (-not (Test-Path $reportPath)) {
                throw "$Name did not create probe report"
            }
            $report = Read-ProbeReport $reportPath
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
        }
        elseif (Test-Path $reportPath) {
            throw "$Name unexpectedly created a probe report"
        }

        Write-Host "[RHITruthGate] PASS $Name"
    }
    finally {
        if ($null -eq $previousRHI) {
            Remove-Item Env:WINTERS_RHI -ErrorAction SilentlyContinue
        }
        else {
            $env:WINTERS_RHI = $previousRHI
        }
        if ($null -eq $previousProbePath) {
            Remove-Item Env:WINTERS_RHI_PROBE_PATH -ErrorAction SilentlyContinue
        }
        else {
            $env:WINTERS_RHI_PROBE_PATH = $previousProbePath
        }
        Remove-Item -LiteralPath $reportPath -Force -ErrorAction SilentlyContinue
    }
}

Invoke-ProbeCase -Name 'default_auto' -EnvironmentRHI '' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'default' -ExpectedRequested 'Auto' `
    -ExpectedSelected 'DX11' -ExpectedStatus 'success' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_dx11' -EnvironmentRHI 'dx11' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'DX11' `
    -ExpectedSelected 'DX11' -ExpectedStatus 'success' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_dx12' -EnvironmentRHI 'dx12' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'DX12' `
    -ExpectedSelected 'DX12' -ExpectedStatus 'success' -ExpectZeroExit $true

Invoke-ProbeCase -Name 'env_vulkan' -EnvironmentRHI 'vulkan' -Arguments @() `
    -ExpectReport $true -ExpectedSource 'environment' -ExpectedRequested 'Vulkan' `
    -ExpectedSelected 'None' -ExpectedStatus 'device_initialization_failed' -ExpectZeroExit $false

Invoke-ProbeCase -Name 'cli_precedence' -EnvironmentRHI 'dx11' -Arguments @('--rhi=dx12') `
    -ExpectReport $true -ExpectedSource 'command-line' -ExpectedRequested 'DX12' `
    -ExpectedSelected 'DX12' -ExpectedStatus 'success' -ExpectZeroExit $true

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

Write-Host '[RHITruthGate] ALL PASS'
```

### 2-4. C:/Users/user/Desktop/Winters/Tools/Harness/Run-S17RhiValidation.ps1

기존 코드:

```powershell
        @{ Name = "WintersElden_probe_dx12"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe"); Cwd = "EldenRingClient\Bin\$Configuration" },
```

아래로 교체:

```powershell
        @{ Name = "WintersElden_probe_default"; Exe = "EldenRingClient\Bin\$Configuration\WintersElden.exe"; Args = @("--scene=probe"); Cwd = "EldenRingClient\Bin\$Configuration" },
```

현재 S17 생존 smoke는 requested/selected를 검증하지 않으므로 DX12라는 이름을 제거한다. 새 전용 truth gate만 backend identity의 증거로 사용하며, S17 통합은 다음 Registry/harness slice에서 진행한다.

## 3. 검증

예측:
- `WINTERS_RHI=dx11|dx12`는 report의 source/requested/selected/status를 정확히 남기고 exit 0이다.
- `WINTERS_RHI=vulkan`은 현재 backend 미구현으로 `selected=None`, non-zero exit이며 MessageBox 없이 끝난다.
- 환경 DX11 + CLI DX12는 CLI가 우선하고 report가 `source=command-line`, `selected=DX12`를 증명한다.
- 빈·중복 CLI와 invalid env는 CEngineApp 진입 전 exit 2이며 report를 만들지 않는다.
- Renderer/Resource 경로는 바꾸지 않으므로 probe가 아닌 정상 DX11 LoL 시각 결과는 불변이다.
- S17의 이름만 DX12인 생존 smoke가 제거된다.

검증 명령:

```powershell
git diff --check -- Client/Private/main.cpp Engine/Private/Framework/CEngineApp.cpp Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 Tools/Harness/Run-S17RhiValidation.ps1 .md/plan/2026-07-21_RHI_BACKEND_SELECTION_TRUTH_GATE_PLAN.md
```

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /t:Build /m:1 /p:Configuration=Debug /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /t:Build /m:1 /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Client 프로젝트는 Engine 프로젝트를 `ProjectReference`로 보유하지 않고 `WintersEngine.lib`만 링크한다. 따라서 Engine을 먼저 빌드해 `EngineSDK`를 갱신하고, 그 다음 Client를 빌드해 새 DLL을 `Client/Bin/Debug`에 배포해야 한다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug
```

정상 제품 회귀 수동 확인:

```powershell
Remove-Item Env:WINTERS_RHI_PROBE_PATH -ErrorAction SilentlyContinue
$env:WINTERS_RHI='dx11'
& 'Client/Bin/Debug/WintersGame.exe' --run-seconds=2
```

미검증:
- 이 slice는 DX11/DX12 제품 시각 parity를 증명하지 않는다. probe는 Device·SwapChain identity까지만 검증한다.
- Vulkan은 runtime probe와 device가 아직 없으므로 성공 실행 대상이 아니다.
- S17 전체 runtime smoke의 생존 판정은 backend identity 증거로 사용하지 않는다.

확인 필요:
- 구현 직전 목표 경로가 다른 세션 변경과 겹치지 않는지 path-scoped status를 다시 확인한다.

## 서브 에이전트 비평

### 1차 비평 — `/root/rhi_truth_gate_critique`

- P0: 0건.
- P1 `allowRHIFallback` 전역 계약 제거: 수용. 엔진 fallback 블록을 유지하고 LoL 명시 요청만 `false`를 전달하도록 수정했다.
- P1 substring CLI parser: 수용. `CommandLineToArgvW` exact token 순회와 빈·중복 실패를 추가했다.
- P1 S17 생존 판정의 거짓 DX12 PASS: 수용. 기존 항목은 `default`로 이름을 낮추고 backend identity는 새 probe report 하네스로 분리했다.
- P1 핵심 완료 조건 미검증: 수용. source/requested/selected/status와 exit code를 8개 case에서 assertion하는 전용 하네스를 계획했다.
- P2 Null 미구현: 수용. LoL 허용 입력에서 `null`을 제거했다.
- P2 Elden build 부재: 수용. S17 항목은 이름만 바로잡고 DX12 증거로 사용하지 않으며, 이번 검증 범위는 Client truth probe로 한정했다.

### 2차 델타 비평 — `/root/rhi_truth_gate_critique`

- P0: 0건.
- P1 기본 Auto case 누락: 수용. `default_auto`에서 source=default, requested=Auto, selected=DX11, status=success를 검증한다.
- P1 계획서 삽입 형식 위반: 수용. 실제 `#endif`와 `m_bSceneRuntimeEnabled` 구간 전체를 `아래로 교체`하는 정확한 앵커로 수정했다.
- P2 무기한 대기와 고정 report 경로: 수용. 15초 timeout과 case별 GUID report 경로를 사용한다.

### 3차 최종 델타 비평 — `/root/rhi_truth_gate_critique`

- 판정: PASS.
- P0: 0건.
- P1: 0건.
- `default_auto` case, 정확한 교체 앵커, GUID report 경로, 15초 timeout을 확인했다.
- 남은 P2는 invalid case가 parse-error 문구 자체까지 assertion하지 않는 점이다. 앞선 정상 case로 실행 파일과 probe 경로를 먼저 증명하고 invalid case는 non-zero와 report 미생성을 확인하므로 이번 slice의 구현 차단 사유로 보지 않는다.

독립 비평 통과선을 충족했으므로 계획 범위의 소스 구현을 시작할 수 있다.
