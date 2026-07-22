# RHI Capability Truth Contract Plan

Session - RHI Step 2A: backend 이름 추정 capability를 제거하고 현재 runtime contract를 기계 판독 가능하게 증명한다.

- 작성일: 2026-07-21
- 상위 목표: LoL 제품 렌더러의 DX11 직접 경로를 제거하고 동일한 RHI renderer를 DX11/DX12/Vulkan에서 완성한다.
- 관련 문서: `.md/architecture/WINTERS_UNREAL_STYLE_MULTI_BACKEND_RHI_ARCHITECTURE.md`, `.md/plan/2026-07-21_RHI_BACKEND_REGISTRY_FACTORY_RESULT.md`
- 현재 단계: Step 2A — capability truth. Command no-op 정리는 별도 Step 2B 계획으로 분리한다.
- ceiling budget: 이번 slice는 capability 소유권과 probe 검증만 다룬다. renderer 이관, DX12 barrier, shader cook, Vulkan 구현은 건드리지 않는다.

## 1. 문제·제약

문제·제약: `IRHIDevice::GetCapabilities()`가 backend enum만 보고 `supportsCompute`, `supportsAsyncCompute`, `supportsBindless`를 true로 만든다. 그러나 현재 DX11/DX12 RHI의 compute shader/pipeline/dispatch 계약은 완성되지 않았고 DX12는 direct queue 하나만 소유한다. 이 상태에서는 backend 선택 성공이 구현 완료처럼 보인다.

더 직진하는 대안이 왜 실패하는가: DX12/Vulkan이라는 API 이름만으로 capability를 켜면 하드웨어 가능성과 Winters runtime 제공 기능이 섞인다. 반대로 모든 값을 영구적으로 false로 두면 이후 구현 완료를 증명할 수 없다. 따라서 device가 자기 runtime capability를 소유하고 probe가 그 값을 검증해야 한다.

핵심 메커니즘: `IRHIDevice::GetCapabilities()`를 pure virtual로 만들고 DX11/DX12 device가 보수적인 runtime contract를 소유한다. selection probe report에 capability를 기록해 자동 gate가 enum 추정 회귀를 막는다.

대조: Unreal 계열 RHI와 native API 모두 adapter/device 지원과 renderer가 실제로 노출하는 feature를 분리한다. 이번 slice는 Winters가 실제로 호출 가능한 runtime feature만 true로 보고한다. native hardware tier 상세 query는 해당 feature 구현 slice에서 별도 evidence로 추가한다.

대가: 현재 GPU가 compute/bindless를 하드웨어로 지원해도 Winters RHI 구현이 없으면 false로 보인다. 이것이 의도한 fail-closed 의미다.

## 2. 현재 코드 증거

- `Engine/Public/RHI/IRHIDevice.h:19-22`는 모든 backend에 `RHI_MakeDefaultCapabilities(GetBackend())`를 적용한다.
- `Engine/Public/RHI/RHICapabilities.h:56-66`은 DX12/Vulkan에 compute, async compute, bindless를 enum만으로 true로 설정한다.
- `Engine/Private/RHI/DX11/CDX11Device.cpp:688`와 `Engine/Private/RHI/DX12/DX12Device.cpp:967`의 RHI `Dispatch()`는 비어 있다.
- `Engine/Private/RHI/DX12/DX12Device.cpp:1891-1923`은 direct command queue 하나만 만든다.
- 현재 `IRHIDevice` 구현체는 `CDX11Device`, `CDX12Device` 두 개뿐이다.
- selection truth gate는 source/requested/module/selected/status/reason/fallback만 검사하며 capability는 검사하지 않는다.

## 3. 소유권과 판정 기준

### 3.1 capability 의미

`RHICapabilities`를 두 의미로 분리한다.

- `supports*`: **현재 Winters runtime이 해당 backend에서 안전하게 제공하고 검증한 기능**
- `apiRequires*`: native API가 backend 구현부에 요구하는 책임. 해당 기능이 이미 올바르게 구현됐다는 뜻이 아니다.
- `frameResourceSlotCount`: 실제 backend 코드가 소유한 frame별 resource 슬롯 수. swapchain latency나 GPU queue depth를 뜻하지 않는다.
- `max*`와 alignment: native 한도, Engine이 강제하는 한도, conformance-tested 한도 중 최솟값만 공개한다. 아직 그 교집합이 검증되지 않은 항목은 0으로 둔다.

- DX11: graphics resource/draw contract는 현재 제공한다. RHI compute contract, async compute, bindless는 제공하지 않는다.
- DX12: graphics resource/draw contract는 현재 제공한다. compute PSO/dispatch, 별도 compute queue, bindless contract는 아직 제공하지 않는다.
- `apiRequiresExplicitResourceStates`는 API/implementation 책임이므로 DX12만 true다.
- `supportsResourceTransitions`는 DX12 transition implementation이 비어 있으므로 이번 단계에서는 false다.
- Vulkan은 module이 아직 없으므로 capability report가 `None/0`이어야 한다.

### 3.2 required / optional 경계

- Required: 현재 LoL RHI renderer가 사용하는 buffer, texture, shader, sampler, graphics pipeline, bind group, frame command list, draw.
- Optional: compute dispatch, async compute, bindless, ray tracing, mesh shader, VRS, GPU marker/timestamp.
- Step 2A는 optional flag의 거짓 양성만 제거한다.
- Step 2B에서 command method별 semantic no-op과 미구현 no-op을 분리한다.

## 4. 반영 코드

### 4.1 `Engine/Public/RHI/RHICapabilities.h`

기존 `RHICapabilities`와 `RHI_MakeDefaultCapabilities(eRHIBackend)` 전체를 아래로 교체한다. backend enum만으로 기능을 만들어내는 public helper는 남기지 않는다.

```cpp
struct WINTERS_ENGINE RHICapabilities
{
    eRHIBackend backend = eRHIBackend::DX11;
    eRHIFeatureTier featureTier = eRHIFeatureTier::LegacyDX11;

    bool_t supportsCompute = false;
    bool_t supportsAsyncCompute = false;
    bool_t supportsRayTracing = false;
    bool_t supportsBindless = false;
    bool_t supportsMeshShader = false;
    bool_t supportsVariableRateShading = false;
    bool_t supportsUnifiedMemory = false;
    bool_t supportsTimelineSemaphore = false;
    bool_t supportsResourceTransitions = false;

    bool_t prefersRenderPassLoadStore = false;
    bool_t isTileBasedGPU = false;
    bool_t apiRequiresExplicitResourceStates = false;

    u32_t frameResourceSlotCount = 0;
    u32_t maxColorAttachments = 0;
    u32_t constantBufferAlignment = 0;
    u32_t textureUploadAlignment = 0;
    u32_t maxSampledTexturesPerStage = 0;
    u32_t maxSamplersPerStage = 0;
};
```

### 4.2 `Engine/Public/RHI/IRHIDevice.h`

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

### 4.3 `Engine/Private/RHI/DX11/CDX11Device.h`

기존 코드:

```cpp
    eRHIBackend             GetBackend() const override { return eRHIBackend::DX11; }
    void* GetNativeHandle(eNativeHandleType type) const override
```

아래로 교체:

```cpp
    eRHIBackend GetBackend() const override { return eRHIBackend::DX11; }
    RHICapabilities GetCapabilities() const override { return m_Capabilities; }
    void* GetNativeHandle(eNativeHandleType type) const override
```

기존 코드:

```cpp
    bool Initialize(const DeviceDesc& desc);
    bool    CreateDeviceAndSwapChain(const DeviceDesc& desc);
```

아래로 교체:

```cpp
    bool Initialize(const DeviceDesc& desc);
    void InitializeCapabilities();
    bool CreateDeviceAndSwapChain(const DeviceDesc& desc);
```

`m_pFrameCommandList` 아래에 추가:

```cpp
    RHICapabilities m_Capabilities{};
```

### 4.4 `Engine/Private/RHI/DX11/CDX11Device.cpp`

`CDX11Device::Initialize()`에서 annotation 초기화 다음, `return true;` 전에 추가:

```cpp
    InitializeCapabilities();
```

`CDX11Device::CreateDeviceAndSwapChain()` 바로 위에 추가:

```cpp
void CDX11Device::InitializeCapabilities()
{
    m_Capabilities = {};
    m_Capabilities.backend = eRHIBackend::DX11;
    m_Capabilities.featureTier = eRHIFeatureTier::LegacyDX11;
    m_Capabilities.frameResourceSlotCount = 1;
}
```

판정 근거: DX11 native API는 compute를 지원할 수 있지만 현재 Winters `CreateShader`/pipeline/`Dispatch` RHI contract는 compute를 제공하지 않으므로 기본 false를 유지한다. `frameResourceSlotCount=1`은 swapchain latency 주장이 아니라 backend가 frame별 resource 배열을 별도로 갖지 않고 immediate context 한 슬롯을 쓰는 현재 코드 구조의 진술이다. 검증하지 않은 최대치와 alignment는 0이다.

### 4.5 `Engine/Private/RHI/DX12/DX12Device.h`

기존 코드:

```cpp
	eRHIBackend GetBackend() const override { return eRHIBackend::DX12; }
	void* GetNativeHandle(eNativeHandleType type) const override;
```

아래로 교체:

```cpp
	eRHIBackend GetBackend() const override { return eRHIBackend::DX12; }
	RHICapabilities GetCapabilities() const override { return m_Capabilities; }
	void* GetNativeHandle(eNativeHandleType type) const override;
```

기존 코드:

```cpp
	bool_t Initialize(const DX12DeviceDesc& desc);

	bool_t CreateDevice(const DX12DeviceDesc& desc);
```

아래로 교체:

```cpp
	bool_t Initialize(const DX12DeviceDesc& desc);
	void InitializeCapabilities();

	bool_t CreateDevice(const DX12DeviceDesc& desc);
```

`m_pFrameCommandList` 아래에 추가:

```cpp
	RHICapabilities m_Capabilities{};
```

### 4.6 `Engine/Private/RHI/DX12/DX12Device.cpp`

`CDX12Device::Initialize()`에서 descriptor heap 생성 성공 다음, ready trace 전에 추가:

```cpp
    InitializeCapabilities();
```

`CDX12Device::CreateDescriptorHeaps()` 바로 위에 추가:

```cpp
void CDX12Device::InitializeCapabilities()
{
    m_Capabilities = {};
    m_Capabilities.backend = eRHIBackend::DX12;
    m_Capabilities.featureTier = eRHIFeatureTier::ExplicitDesktop;
    m_Capabilities.apiRequiresExplicitResourceStates = true;
    m_Capabilities.frameResourceSlotCount = kFrameCount;
}
```

판정 근거: 현재 DX12 backend는 graphics direct queue와 graphics PSO만 runtime contract로 제공한다. compute/async/bindless/resource transition은 native API가 가능하거나 요구해도 아직 false다. `apiRequiresExplicitResourceStates=true`는 구현 책임을 나타내며 지원 완료 주장이 아니다. `frameResourceSlotCount=kFrameCount`는 실제 per-frame allocator/resource 배열의 크기만 보고한다. 나머지 최대치와 alignment는 conformance 전까지 0이다.

### 4.7 `Engine/Private/Framework/CEngineApp.cpp`

`RHIBackendName()` 아래에 추가:

```cpp
    const char* RHIFeatureTierName(eRHIFeatureTier tier)
    {
        switch (tier)
        {
        case eRHIFeatureTier::LegacyDX11: return "LegacyDX11";
        case eRHIFeatureTier::ExplicitDesktop: return "ExplicitDesktop";
        case eRHIFeatureTier::MobileTiled: return "MobileTiled";
        case eRHIFeatureTier::Console: return "Console";
        default: return "Unknown";
        }
    }
```

기존 `WriteRHISelectionProbeReport()` 함수 전체를 아래로 교체:

```cpp
    bool_t WriteRHISelectionProbeReport(
        const std::wstring& path,
        eEngineRHIBackend requested,
        const char* pModule,
        const char* pSelected,
        const char* pStatus,
        const char* pReason,
        bool_t bFallbackUsed,
        const char* pFallbackReason,
        const RHICapabilities* pCapabilities)
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

        const char* pCapabilityBackend = pCapabilities
            ? RHIBackendName(pCapabilities->backend)
            : "None";
        const char* pFeatureTier = pCapabilities
            ? RHIFeatureTierName(pCapabilities->featureTier)
            : "None";
        const char* pCompute = pCapabilities && pCapabilities->supportsCompute ? "yes" : "no";
        const char* pAsyncCompute =
            pCapabilities && pCapabilities->supportsAsyncCompute ? "yes" : "no";
        const char* pBindless = pCapabilities && pCapabilities->supportsBindless ? "yes" : "no";
        const char* pResourceTransitions =
            pCapabilities && pCapabilities->supportsResourceTransitions ? "yes" : "no";
        const char* pApiRequiresExplicitStates =
            pCapabilities && pCapabilities->apiRequiresExplicitResourceStates ? "yes" : "no";
        const u32_t frameResourceSlots =
            pCapabilities ? pCapabilities->frameResourceSlotCount : 0u;

        char report[896]{};
        sprintf_s(
            report,
            "source=%s\nrequested=%s\nmodule=%s\nselected=%s\n"
            "status=%s\nreason=%s\nfallback=%s\nfallback_reason=%s\n"
            "capability_backend=%s\nfeature_tier=%s\n"
            "supports_compute=%s\nsupports_async_compute=%s\n"
            "supports_bindless=%s\nsupports_resource_transitions=%s\n"
            "api_requires_explicit_states=%s\nframe_resource_slots=%u\n",
            pSource,
            EngineRHIBackendName(requested),
            pModule,
            pSelected,
            pStatus,
            pReason,
            bFallbackUsed ? "yes" : "no",
            pFallbackReason,
            pCapabilityBackend,
            pFeatureTier,
            pCompute,
            pAsyncCompute,
            pBindless,
            pResourceTransitions,
            pApiRequiresExplicitStates,
            frameResourceSlots);

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

window 생성 실패의 기존 호출 전체를 아래로 교체:

```cpp
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            "None",
            "None",
            "window_creation_failed",
            "window_creation_failed",
            false,
            "none",
            nullptr);
```

device 생성 실패의 기존 호출 전체를 아래로 교체:

```cpp
        WriteRHISelectionProbeReport(
            rhiProbeReportPath,
            config.rhiBackend,
            createResult.moduleName.c_str(),
            "None",
            RHIBackendCreateStatusName(createResult.status),
            createResult.reason.c_str(),
            bFallbackUsed,
            fallbackReason.c_str(),
            nullptr);
```

device 생성 성공의 기존 호출 전체를 아래로 교체:

```cpp
    const RHICapabilities capabilities = m_pDevice->GetCapabilities();
    WriteRHISelectionProbeReport(
        rhiProbeReportPath,
        config.rhiBackend,
        createResult.moduleName.c_str(),
        RHIBackendName(m_pDevice->GetBackend()),
        RHIBackendCreateStatusName(createResult.status),
        createResult.reason.c_str(),
        bFallbackUsed,
        fallbackReason.c_str(),
        &capabilities);
```

### 4.8 `Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1`

required key 목록에 다음을 추가한다.

```powershell
                'capability_backend', 'feature_tier',
                'supports_compute', 'supports_async_compute',
                'supports_bindless', 'supports_resource_transitions',
                'api_requires_explicit_states', 'frame_resource_slots'
```

기존 base field 비교 다음에 추가:

```powershell
            $expectedCapabilities = switch ($ExpectedSelected) {
                'DX11' {
                    @{
                        feature_tier = 'LegacyDX11'
                        capability_backend = 'DX11'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '1'
                    }
                }
                'DX12' {
                    @{
                        feature_tier = 'ExplicitDesktop'
                        capability_backend = 'DX12'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'yes'
                        frame_resource_slots = '2'
                    }
                }
                default {
                    @{
                        feature_tier = 'None'
                        capability_backend = 'None'
                        supports_compute = 'no'
                        supports_async_compute = 'no'
                        supports_bindless = 'no'
                        supports_resource_transitions = 'no'
                        api_requires_explicit_states = 'no'
                        frame_resource_slots = '0'
                    }
                }
            }
            foreach ($key in $expectedCapabilities.Keys) {
                if ($report[$key] -ne $expectedCapabilities[$key]) {
                    throw "$Name capability mismatch for ${key}: expected $($expectedCapabilities[$key]), got $($report[$key])"
                }
            }
```

## 5. 검증

관측:

- DX11 report: `LegacyDX11`, compute/async/bindless/transition `no`, API explicit-state requirement `no`, frame resource slots `1`.
- DX12 report: `ExplicitDesktop`, compute/async/bindless/transition `no`, API explicit-state requirement `yes`, frame resource slots `2`.
- Vulkan 미등록 report: capability `None/0`.
- normal DX11 product 실행이 probe 조기 종료를 사용하지 않고 `--run-seconds=2` frame loop까지 진입한 뒤 종료 코드 0. 이 smoke만으로 roster/map/minion/champion/UI/FX의 시각 parity를 주장하지 않는다.

검증 명령:

```powershell
git diff --check -- `
  Engine/Public/RHI/RHICapabilities.h `
  Engine/Public/RHI/IRHIDevice.h `
  Engine/Private/RHI/DX11/CDX11Device.h `
  Engine/Private/RHI/DX11/CDX11Device.cpp `
  Engine/Private/RHI/DX12/DX12Device.h `
  Engine/Private/RHI/DX12/DX12Device.cpp `
  Engine/Private/Framework/CEngineApp.cpp `
  Tools/Harness/Run-RHIBackendSelectionTruthGate.ps1 `
  .md/plan/2026-07-21_RHI_CAPABILITY_TRUTH_CONTRACT_PLAN.md
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Engine\Include\Engine.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' Client\Include\Client.vcxproj /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
[scriptblock]::Create((Get-Content -LiteralPath Tools\Harness\Run-RHIBackendSelectionTruthGate.ps1 -Raw)) | Out-Null
powershell -NoProfile -ExecutionPolicy Bypass -File Tools\Harness\Run-RHIBackendSelectionTruthGate.ps1 -Configuration Debug

$sdkPairs = @(
  @('Engine\Public\RHI\IRHIDevice.h', 'EngineSDK\inc\RHI\IRHIDevice.h'),
  @('Engine\Public\RHI\RHICapabilities.h', 'EngineSDK\inc\RHI\RHICapabilities.h')
)
foreach ($pair in $sdkPairs) {
  $sourceHash = (Get-FileHash -LiteralPath $pair[0] -Algorithm SHA256).Hash
  $sdkHash = (Get-FileHash -LiteralPath $pair[1] -Algorithm SHA256).Hash
  if ($sourceHash -ne $sdkHash) {
    throw "EngineSDK header mismatch: $($pair[0]) != $($pair[1])"
  }
}

$repoRoot = (Resolve-Path '.').Path
$product = Start-Process `
  -FilePath (Join-Path $repoRoot 'Client\Bin\Debug\WintersGame.exe') `
  -WorkingDirectory $repoRoot `
  -ArgumentList @('--rhi=dx11', '--run-seconds=2') `
  -PassThru -WindowStyle Hidden
if (-not $product.WaitForExit(15000)) {
  Stop-Process -Id $product.Id -Force -ErrorAction SilentlyContinue
  throw 'DX11 product smoke timed out'
}
if ($product.ExitCode -ne 0) {
  throw "DX11 product smoke failed: exit=$($product.ExitCode)"
}
```

추가 정적 gate:

```powershell
rg -n "RHI_MakeDefaultCapabilities|supportsCompute = true|supportsAsyncCompute = true|supportsBindless = true" Engine/Public/RHI Engine/Private/RHI/DX11 Engine/Private/RHI/DX12
```

위 `rg`는 결과 0줄이 통과다. plan/result 문서의 예시 코드는 검색 범위에 포함하지 않는다.

미검증:

- DX12 resource barrier의 실제 동작
- compute pipeline/dispatch
- indirect draw
- Vulkan native capability query

이 항목은 이번 slice의 성공으로 주장하지 않는다.

## 6. 독립 서브 에이전트 비평

비평 주체: `/root/rhi_registry_critique` read-only review.

초기 판정: P0 0, P1 5.

- 수용 — runtime support와 native/internal 숫자 의미 혼합: `supports*`, `apiRequires*`, `frameResourceSlotCount`로 의미를 분리하고 검증되지 않은 limit/alignment는 0으로 변경했다.
- 수용 — probe-only를 product smoke로 오인: 별도 timed product launch를 추가하고 관측 주장을 frame-loop exit 0으로 축소했다.
- 수용 — EngineSDK 공개 header parity 누락: Engine build 후 SHA-256 byte parity gate를 추가했다.
- 수용 — CEngineApp 변경이 prose-only: 함수 전체와 세 call site의 정확한 교체 코드를 기록했다.
- 수용 — capability backend 오귀속 가능: `capability_backend`를 report schema와 expected map에 추가했다.
- 수용(P2) — dirty worktree와 PowerShell syntax 위험: path-scoped `git diff --check`와 parse-only gate를 추가했다.

재비평 추가 지적: product smoke working directory가 F5 계약과 달랐던 P1 1건을 수용해 repo root cwd와 absolute executable path로 수정했다.

최종 재비평: **PASS — P0 0, P1 0, 비차단 P2 0. 구현 시작 가능.**

## 7. 다음 handoff

Step 2B 계획에서 아래를 메서드 단위로 분류한다.

1. DX11에서 의미상 정상 no-op인 resource transition
2. DX12에서 반드시 barrier가 필요한 resource transition
3. frame lifecycle에 흡수된 `Begin/End`
4. 실제 미구현인 compute dispatch와 render pass
5. 미지원 호출을 silent success가 아닌 명시적 결과/validation failure로 바꾸는 방법
