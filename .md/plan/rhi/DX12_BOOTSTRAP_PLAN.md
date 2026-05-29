# RHI / DX12 Bootstrap 계획서 (Track 2 W7-9) — v3

> 2026-05-09 상태 정정: 이 문서는 2026-05-06 bootstrap 계획서라 중간 본문에 당시의 alias/Build.0 정책이 남아 있다. 현재 운영 기준은 `CLAUDE.md`, `TODO.md`, `.md/TODO/05-07/RHI/DX12_REMAINING_PLAN_AND_PROGRESS_2026_05_07.md`를 우선한다. 2026-05-09 기준 `DX12SmokeHost`는 `Winters.sln`에 등록되어 있고, `Client.vcxproj`도 `Debug-DX12` / `Release-DX12` 실제 config를 가진다. 현재 solution-level `Debug-DX12` Build.0는 Engine + Client + DX12SmokeHost다.

작성일: 2026-05-06 (codex 4차 검토 반영)

이력:
- v1 (2026-05-05) — 28 파일 박제 + Debug-DX12 config 박제 후 codex 1차 + 2차 검토 반영
- v2 (2026-05-06) — codex 3차 검토 P1×3 + P2×3 반영
- v3 (2026-05-06) — codex 4차 검토 P1×2 + P2×2 반영. 본 v3 가 권위. v1/v2 폐기 (status: deprecated, replaced-by v3)
  - P1: DX12SmokeHost 진입점 정정 (CEngineApp → WintersRun + IWintersApp), 위치 정정 (Engine/Tools → Tools/DX12SmokeHost)
  - P1: .sln Build.0 제거 예시 placeholder GUID → 실제 6 라인 명시 (E3333333/E2222222/E4444444 의 L35/39/43/47/51/55)
  - P2: M3 Live Object 검증 코드 패치 박제 (DX12Device.cpp 소멸자 ReportLiveObjects 추가)
  - P2: M0-3 검증 경로 정정 (Bin/Debug-DX12 → Engine/Bin/Debug-DX12)

계기:
- 5/5 `Engine/Private/RHI/DX12/` 28 파일 박제 + `Debug-DX12` config + D3D12MA ThirdPartyLib 추가
- 5/6 codex 3차 검토:
  - P1×3 — Debug-DX12 DLL 미배포 / AssetConverter main.cpp DX12 빌드 포함 / fence per-slot (이미 v1 M2-4 박제됨)
  - P2×3 — Feature level fallback 계획-구현 불일치 / Resize stub 합격 기준 / Server alias 가 Engine Debug-DX12 import lib 링크
- 5/6 codex 4차 검토:
  - P1×2 — DX12SmokeHost 진입점 (CEngineApp 미공개 export) / .sln Build.0 placeholder GUID
  - P2×2 — M3 Live Object 코드 호출 미박제 / M0-3 산출물 경로 불일치
- CLAUDE.md §1.A Track 2 W7-9 가 "박제 완료, codex 진입 대기" 표기지만 실제는 Scaffold + Bootstrap 본체 박제 완료, 빌드/런타임 검증 단계

관계 문서:
- 마스터 계획서: `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md` (RH-0~RH-6)
- W7-9 상세: `.md/plan/engine/2026-05-07_WEEK_7_9_DETAILED_BAKE.md`
- Twin Track 통합: `.md/plan/engine/2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md`
- 박제 함정: `.md/process/PLAN_AUTHORING_PITFALLS.md` (P-1~P-19, GATE A~H)

합격 기준:
- 시각 동일성 X
- compile-only + DX12 device bootstrap + clear/present + 정상 종료
- Debug-DX12 DLL 이 Client/Bin/Debug 또는 별도 호스트로 **실제 로드** 검증
- Engine Debug-DX12 빌드 0 errors + Debug baseline 동등 (50 warnings 이하)
- 시각 동일성은 W10-13 진입

codex 3차 검토 반영 핵심 (v2 신규):

```txt
P1-A: UpdateLib.bat L42-58 = Debug/Release 만 Engine DLL 배포 → Debug-DX12 DLL 이 Client/Bin/Debug 에 안 와서
       M1/M2 런타임 검증 시 Client 가 기존 DX11 DLL 로드. M0 단계에서 배포 경로 확정 선행.
P1-B: Engine.vcxproj L241-244 = AssetConverter main.cpp 의 ExcludedFromBuild 가 Debug/Release 만 커버.
       Debug-DX12 / Release-DX12 빌드 시 Engine DLL 에 컨버터 EXE 엔트리 소스 포함 = 소유권 mismatch.
P1-C: DX12Queue per-slot fence — v1 M2-4 박제 그대로 정합. 코드 변경 대상 (전역 monotonic).
P2-A: DX12Device.cpp L115/L130 = hardware + WARP 모두 D3D_FEATURE_LEVEL_12_0 only. v1 M1-2 의 "11_0 fallback" 박제는
       구현과 불일치. v2 = 코드 기준으로 12_0 only 박제 + 11_0 fallback 박제 제거 (W10-13 별도 옵션).
P2-B: DX12SwapChain.cpp L91-96 = Resize 가 OutputDebugStringA deferred stub. v1 M2-5 의 "Alt+Tab swapchain Resize"
       검증 항목은 코드와 충돌. v2 = M2-5 합격 기준에서 제외, M2-6 (Resize 옵션) 을 W10-13 이동 명시.
P2-C: Server.vcxproj L148-154 = LinkLibraryDependencies=true + ProjectReference. Debug-DX12 솔루션 빌드 시
       Engine/Bin/Debug-DX12/WintersEngine.lib 가 Server 링크 라인에 진입. PostBuild 는 Debug DLL 복사 = 링크와
       복사 산출물 mismatch. v1 §0-1 의 "Debug fallback" 표현 보정 + M0-6 신설.
```

---

## §0. 현재 박제 상태 (실측, 5/6)

### 0-1. 솔루션 + 컨피그

`Winters.sln` (L17-22):

```txt
GlobalSection(SolutionConfigurationPlatforms) = preSolution
    Debug|x64 = Debug|x64
    Debug-DX12|x64 = Debug-DX12|x64
    Release|x64 = Release|x64
    Release-DX12|x64 = Release-DX12|x64
EndGlobalSection
```

프로젝트별 매핑 (codex 3차 검토 P2-C 반영 정정):

```txt
Engine (sln L26-27)   Debug-DX12|x64       진짜 DX12 config (Engine/Bin/Debug-DX12/ 산출)
Server (sln L34-35)   Debug|x64            솔루션 alias — 단, ProjectReference + LinkLibraryDependencies=true
                                            (Server.vcxproj L148-154) 로 Debug-DX12 솔루션 빌드 시
                                            Engine/Bin/Debug-DX12/WintersEngine.lib 가 Server 링크 라인에
                                            진입할 수 있음 (P2-C, M0-6 에서 정합 검증)
Client (sln L42-43)   Debug|x64            솔루션 alias (DX12 미적용)
Tools  (sln L50-51)   Debug|x64            솔루션 alias (DX12 미적용)
```

**v1 표현 "Debug-DX12 선택 시 Engine 만 DX12 변경. Client/Server/Tools 는 자동 fallback (Debug|x64 빌드)" 는 절반만 맞음**:
- Configuration mapping 측면: Server/Client/Tools = Debug|x64 ✅
- Link 측면: Server 가 Engine ProjectReference 를 가져 LinkLibraryDependencies=true (Server.vcxproj L153) → MSBuild solution build 시 Engine 의 활성 config (Debug-DX12) 의 import lib 가 link line 에 자동 추가될 수 있음. PostBuild 는 Debug DLL 복사. **링크 산출물과 PostBuild 산출물이 다른 config** → M0-6 에서 정합 검증.

### 0-2. Engine.vcxproj Debug-DX12 config

파일: `Engine/Include/Engine.vcxproj`

Debug-DX12 ProjectConfiguration (L8-9):

```xml
<ProjectConfiguration Include="Debug-DX12|x64">
  <Configuration>Debug-DX12</Configuration>
  <Platform>x64</Platform>
</ProjectConfiguration>
```

OutDir / IntDir (L65-66):

```xml
<OutDir>$(ProjectDir)..\Bin\Debug-DX12\</OutDir>
<IntDir>$(ProjectDir)..\Bin\Intermediate\Debug-DX12\</IntDir>
```

PreprocessorDefinitions Debug-DX12 (L115):

```txt
WINTERS_ENGINE_EXPORTS;WIN32;_DEBUG;_WINDOWS;_USRDLL;WINTERS_PROFILING;WINTERS_RHI_BACKEND_DX12;%(PreprocessorDefinitions)
```

AdditionalDependencies Debug-DX12 (L132):

```txt
d3d12.lib;d3d11.lib;dxgi.lib;d3dcompiler.lib;dxguid.lib;assimp-vc143-mtd.lib;DirectXTK.lib;fmod_vc.lib;%(AdditionalDependencies)
```

`WINTERS_RHI_BACKEND_DX12` 매크로 + `d3d12.lib` + `dxgi.lib` 모두 박제됨.

### 0-3. Engine.vcxproj AssetConverter main.cpp ExcludedFromBuild (P1-B 신규 박제)

`Engine/Include/Engine.vcxproj` L241-244 실측 인용:

```xml
<ClCompile Include="..\Private\Tools\AssetConverter\main.cpp">
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">true</ExcludedFromBuild>
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Release|x64'">true</ExcludedFromBuild>
</ClCompile>
```

**문제**: 새로 추가된 Debug-DX12 / Release-DX12 config 의 ExcludedFromBuild 조건 미박제. Debug-DX12 빌드 시 Engine DLL 에 컨버터 EXE 의 `main()` 진입 소스가 포함됨 → 소유권 mismatch. 현재 Rebuild 가 통과한 것은 main.cpp 가 Engine DLL 안에서 dead code 로 컴파일되고 linker 가 export 안 함이라 (?) 내부 Engine 코드 충돌이 없기 때문이지만, 의도와 다른 상태.

**M0-5 에서 4 config 모두 ExcludedFromBuild 박제** — Debug, Release, Debug-DX12, Release-DX12.

### 0-4. UpdateLib.bat 배포 경로 (P1-A 신규 박제)

`UpdateLib.bat` L42-58 실측 인용:

```bat
REM -- Engine build artifacts .lib -> EngineSDK/lib (Debug/Release) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)

REM -- Engine DLL + PDB -> Client output dirs (Debug/Release) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" "%ROOT%\Client\Bin\Debug\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.pdb" "%ROOT%\Client\Bin\Debug\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.dll" "%ROOT%\Client\Bin\Release\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.pdb" "%ROOT%\Client\Bin\Release\"
)
```

**문제**: Debug-DX12 / Release-DX12 분기 0. Engine Debug-DX12 빌드 후 산출은 `Engine/Bin/Debug-DX12/WintersEngine.dll` 로 가지만 Client/Bin/Debug 에는 복사되지 않음. Client 가 Debug 빌드 alias 로 빌드되어 `Client/Bin/Debug/` 에서 EXE 가 실행되면 그곳의 **DX11 빌드된 WintersEngine.dll** 을 로드. M1-1 의 `[CDX12Device] Bootstrap device ready.` 로그가 절대 안 뜸.

**M0-4 에서 UpdateLib.bat 의 PostBuild 또는 별도 deployment 단계에서 Debug-DX12 → Client/Bin/Debug-DX12 또는 별도 smoke host 박제**.

### 0-5. Engine/Private/RHI/DX12/ 28 파일

```txt
DX12BindGroup.cpp/.h        DX12Device.cpp/.h           DX12RootSignature.cpp/.h
DX12Buffer.cpp/.h           DX12MemoryAllocator.cpp     DX12Sampler.cpp/.h
DX12CommandList.cpp/.h      DX12PipelineState.cpp/.h    DX12Shader.cpp/.h
DX12DescriptorHeap.cpp/.h   DX12Queue.cpp/.h            DX12SwapChain.cpp/.h
                            DX12RenderPass.cpp/.h       DX12Texture.cpp/.h
                            DX12ResourceBarrier.cpp
```

Public 헤더 0 (`Engine/Public/RHI/DX12/` 폴더 부재). Private 만 박제 = DX11 처럼 외부 노출 X.

### 0-6. CDX12Device 핵심 박제

파일: `Engine/Private/RHI/DX12/DX12Device.h` + `.cpp`

클래스 시그니처 (`DX12Device.h:29-109`):

```cpp
class CDX12Device final : public IRHIDevice
{
public:
    ~CDX12Device() override;
    static std::unique_ptr<CDX12Device> Create(const DX12DeviceDesc& desc);

    eRHIBackend GetBackend() const override { return eRHIBackend::DX12; }
    void* GetNativeHandle(eNativeHandleType type) const override;

    void BeginFrame(f32_t r, f32_t g, f32_t b, f32_t a) override;
    void EndFrame() override;

    IRHISwapChain* CreateSwapChain(const RHIWindowHandle& window) override;
    IRHIQueue* GetGraphicsQueue() override;

    RHIPipelineHandle CreatePipeline(const RHIPipelineDesc& desc) override;
    void DestroyPipeline(RHIPipelineHandle handle) override;
    // ... 6 메서드 동일 패턴 (RenderPass / BindGroup / Layout)

    IDXGIFactory6* GetDXGIFactory() const { return m_pFactory.Get(); }
    ID3D12Device* GetD3D12Device() const { return m_pDevice.Get(); }
    D3D12MA::Allocator* GetAllocator() const { return m_pAllocator; }
    CDX12Queue* GetDX12GraphicsQueue() const { return m_pGraphicsQueue.get(); }

private:
    CDX12Device() = default;

    bool_t Initialize(const DX12DeviceDesc& desc);
    bool_t CreateFactory();
    bool_t CreateAdapterAndDevice();
    bool_t CreateAllocator();
    bool_t CreateQueues();
    bool_t CreateFrameResources();
    bool_t CreateBackBufferViews();
    void   WaitIdle();

    Microsoft::WRL::ComPtr<IDXGIFactory6> m_pFactory;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> m_pAdapter;
    Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice;
    D3D12MA::Allocator* m_pAllocator = nullptr;

    std::unique_ptr<CDX12Queue> m_pGraphicsQueue;
    std::unique_ptr<CDX12Queue> m_pComputeQueue;
    std::unique_ptr<CDX12Queue> m_pCopyQueue;
    std::unique_ptr<CDX12SwapChain> m_pSwapChain;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pRTVHeap;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocators[3];
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
    D3D12_RESOURCE_STATES m_BackBufferStates[3] = {
        D3D12_RESOURCE_STATE_PRESENT, ...
    };

    u32_t m_RTVDescriptorSize = 0;
    u32_t m_FrameIndex = 0;
    f32_t m_ClearColor[4] = { 0.08f, 0.08f, 0.12f, 1.0f };
    bool_t m_bVSync = true;
    HWND m_hWnd = nullptr;
    u32_t m_Width = 1280;
    u32_t m_Height = 720;

    CRHIResourceTable<IRHIPipelineState, RHIPipelineTag> m_PipelineTable;
    CRHIResourceTable<IRHIRenderPass, RHIRenderPassTag> m_RenderPassTable;
    CRHIResourceTable<IRHIBindGroupLayout, RHIBindGroupLayoutTag> m_BindGroupLayoutTable;
    CRHIResourceTable<IRHIBindGroup, RHIBindGroupTag> m_BindGroupTable;
};
```

Initialize 시퀀스 (`DX12Device.cpp:44-76`):

```cpp
bool_t CDX12Device::Initialize(const DX12DeviceDesc& desc)
{
    m_hWnd = desc.hwnd;
    m_Width = desc.width;
    m_Height = desc.height;
    m_bVSync = desc.vsync;

    if (!CreateFactory())               return false;
    if (!CreateAdapterAndDevice())      return false;
    if (!CreateAllocator())             return false;
    if (!CreateQueues())                return false;

    RHIWindowHandle window{};
    window.nativeWindow = m_hWnd;
    window.width = m_Width;
    window.height = m_Height;
    window.vsync = m_bVSync;
    window.fullscreen = desc.fullscreen;

    if (!CreateSwapChain(window))       return false;
    if (!CreateFrameResources())        return false;
    if (!CreateBackBufferViews())       return false;

    OutputDebugStringA("[CDX12Device] Bootstrap device ready.\n");
    return true;
}
```

Debug Layer (`DX12Device.cpp:78-92` `CreateFactory`):

```cpp
UINT factoryFlags = 0;
#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug> pDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
    {
        pDebug->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
return SUCCEEDED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_pFactory)));
```

### 0-7. CDX12Device::CreateAdapterAndDevice 본체 (P2-A 신규 박제)

`Engine/Private/RHI/DX12/DX12Device.cpp:97-141` 실측 인용:

```cpp
bool_t CDX12Device::CreateAdapterAndDevice()
{
    for (UINT i = 0;; ++i)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter;
        if (FAILED(m_pFactory->EnumAdapterByGpuPreference(
            i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&pAdapter))))
        {
            break;
        }

        DXGI_ADAPTER_DESC3 desc{};
        pAdapter->GetDesc3(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(
            pAdapter.Get(),
            D3D_FEATURE_LEVEL_12_0,        // ★ 12_0 only — 11_0 fallback 0
            IID_PPV_ARGS(&m_pDevice))))
        {
            m_pAdapter = pAdapter;
            return true;
        }
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter> pWarpAdapter;
    if (SUCCEEDED(m_pFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter))))
    {
        if (SUCCEEDED(D3D12CreateDevice(
            pWarpAdapter.Get(),
            D3D_FEATURE_LEVEL_12_0,        // ★ WARP 도 12_0 only
            IID_PPV_ARGS(&m_pDevice))))
        {
            pWarpAdapter.As(&m_pAdapter);
            OutputDebugStringA("[CDX12Device] Using WARP fallback adapter.\n");
            return true;
        }
    }

    OutputDebugStringA("[CDX12Device] D3D12 device creation failed.\n");
    return false;
}
```

**v1 §3 M1-2 박제 ("12_0 시도, 실패 시 11_0 fallback") 와 코드 불일치**. v2 = 코드 기준으로 12_0 only 박제. 11_0 fallback 은 W10-13 진입 시 별도 옵션 (호환성 확장).

WARP 가 비활성이라는 v1 §3 M1-2 박제도 정정 — **WARP fallback 은 활성** (hardware 12_0 실패 시 WARP 12_0 시도, 둘 다 실패 시 device creation failed).

### 0-8. CDX12Device 소멸자 시퀀스 (`DX12Device.cpp:23-42`)

```cpp
CDX12Device::~CDX12Device()
{
    WaitIdle();

    m_pCommandList.Reset();
    for (auto& pAllocator : m_pCommandAllocators)
        pAllocator.Reset();

    m_pRTVHeap.Reset();
    m_pSwapChain.reset();
    m_pCopyQueue.reset();
    m_pComputeQueue.reset();
    m_pGraphicsQueue.reset();

    if (m_pAllocator)
    {
        m_pAllocator->Release();
        m_pAllocator = nullptr;
    }
}
```

### 0-9. CDX12SwapChain 박제 (DX12SwapChain.h 전문)

```cpp
class CDX12SwapChain final : public IRHISwapChain
{
public:
    ~CDX12SwapChain() override = default;
    static std::unique_ptr<CDX12SwapChain> Create(CDX12Device* pDevice, const RHIWindowHandle& window);

    void Present(bool_t bVSync) override;
    u32_t GetCurrentBackBufferIndex() const override { return m_BackBufferIndex; }
    RHITextureHandle GetCurrentBackBuffer() const override { return m_hBackBuffers[m_BackBufferIndex]; }
    void Resize(u32_t width, u32_t height) override;
    void* GetNativeHandle(eNativeHandleType type) const override;

    IDXGISwapChain4* GetSwapChain() const { return m_pSwapChain.Get(); }
    ID3D12Resource* GetBackBuffer(u32_t index) const { return m_pBackBuffers[index % kBackBufferCount].Get(); }
    bool_t IsTearingSupported() const { return m_bTearingSupported; }

    static constexpr u32_t kBackBufferCount = 3;

private:
    CDX12SwapChain() = default;
    bool_t Initialize(CDX12Device* pDevice, const RHIWindowHandle& window);

    Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pBackBuffers[kBackBufferCount];
    RHITextureHandle m_hBackBuffers[kBackBufferCount]{};
    u32_t m_BackBufferIndex = 0;
    u32_t m_Width = 0;
    u32_t m_Height = 0;
    bool_t m_bTearingSupported = false;
};
```

3 backbuffer + tearing support + IDXGISwapChain4 박제.

### 0-10. CDX12SwapChain::Resize 본체 (P2-B 신규 박제)

`Engine/Private/RHI/DX12/DX12SwapChain.cpp:91-96` 실측 인용:

```cpp
void CDX12SwapChain::Resize(u32_t width, u32_t height)
{
    (void)width;
    (void)height;
    OutputDebugStringA("[CDX12SwapChain] Resize is deferred to W10-13.\n");
}
```

**v1 §4 M2-5 / M2-6 박제 ("Alt+Tab → 화면 전환 시 swapchain Resize 정상") 와 코드 불일치**. Resize 는 코드 자체가 deferred stub 으로 박제됨 — 본 W7-9 합격 범위 외. v2 = M2-5 합격 기준에서 Resize 검증 제외, M2-6 = W10-13 이동 명시.

### 0-11. D3D12MA ThirdPartyLib (codex 정정)

Vendor 위치: `Engine/ThirdPartyLib/D3D12MA/{Inc, Src}/`

Wrapper 패턴 (`Engine/Private/RHI/DX12/DX12MemoryAllocator.cpp:1-7` 전문):

```cpp
#include "WintersPCH.h"

#if defined(WINTERS_RHI_BACKEND_DX12)

#include "../../../ThirdPartyLib/D3D12MA/Src/D3D12MemAlloc.cpp"

#endif
```

`DX12MemoryAllocator.cpp` 가 wrapper 로서 `D3D12MemAlloc.cpp` 를 직접 `#include` (vendor 패턴, MSVC unity build 와 동일 효과).

vcxproj 등재 확인 (`Engine/Include/Engine.vcxproj`):
- L119 (Debug-DX12) `<AdditionalIncludeDirectories>` = `...\ThirdPartyLib\D3D12MA\Inc;...` (`Inc`, `Include` 아님)
- L260 `<ItemGroup>` = `<ClCompile Include="..\Private\RHI\DX12\DX12MemoryAllocator.cpp" />`
- `D3D12MemAlloc.cpp` 직접 등재 X. wrapper 한 번만 빌드 → 중복 심볼 0

M0-1 에서 `D3D12MemAlloc.cpp` 직접 등재 박제 금지. 기존 wrapper 패턴 그대로 유지.

### 0-12. Server.vcxproj ProjectReference (P2-C 신규 박제)

`Server/Include/Server.vcxproj:147-156` 실측 인용:

```xml
<ItemGroup>
  <ProjectReference Include="..\..\Engine\Include\Engine.vcxproj">
    <Project>{E1111111-1111-1111-1111-111111111111}</Project>
    <Private>true</Private>
    <ReferenceOutputAssembly>true</ReferenceOutputAssembly>
    <CopyLocalSatelliteAssemblies>true</CopyLocalSatelliteAssemblies>
    <LinkLibraryDependencies>true</LinkLibraryDependencies>
    <UseLibraryDependencyInputs>false</UseLibraryDependencyInputs>
  </ProjectReference>
</ItemGroup>
```

**Debug-DX12 솔루션 빌드 시 동작**:
- Server config = Debug|x64 (sln L34-35 매핑, 변경 X)
- 그러나 `LinkLibraryDependencies=true` + `ReferenceOutputAssembly=true` → MSBuild 가 Engine 의 활성 config (= Debug-DX12) 의 import lib (`Engine/Bin/Debug-DX12/WintersEngine.lib`) 를 Server 링크 라인에 추가
- Server PostBuild 또는 Server PostBuildEvent 가 Debug DLL 을 복사하면 link-time = Debug-DX12 lib, run-time = Debug DLL → **export 시그니처 mismatch 가능** (export 변경 0 이면 동작하나, RHI 백엔드 매크로 차이로 inline 함수 시그니처 변경 가능)

**M0-6 에서 정합 검증** — Debug-DX12 솔루션 빌드 시 Server 가 어느 lib 를 링크하고 어느 DLL 을 로드하는지 명확화. 옵션:
- (a) Server 도 Debug-DX12 config 추가 (Engine 와 동일 config)
- (b) Server 의 Engine.vcxproj reference 에 명시적 config mapping 추가
- (c) Debug-DX12 솔루션 빌드 시 Server 자동 제외 (sln 의 Build.0 항목 제거)
- (d) Engine Debug-DX12 lib 의 export 시그니처가 Debug 와 ABI 호환임을 명시적 검증 (sizeof / vtable 동일)

W7-9 단계에서는 (c) 또는 (d) 권장. (a) 는 Server 가 DX12 인지 / DX11 인지 의미 모호 (Server 는 RHI 미사용이라 어느 쪽이든 무관 = ABI 호환 보장 필요).

---

## §1. 잔여 사고 (codex 3차 검토 반영)

```txt
D-1. Debug-DX12 config 빌드 미검증                              (P1, M0)
D-2. CDX12Device::Create 호출 진입점 부재   →  실재로 정정 (M1 = 검증)
D-3. Clear color present 검증 안 됨         →  실재로 정정 (M2 = 1 프레임 검증)
D-4. Alt+F4 정상 종료 검증 안 됨                                 (P2, M3)
D-5. Engine 외 (Client/Server/Tools) 자동 fallback 동작 검증     (P3, M0)
D-6. D3D12MA 박제안 = 중복 컴파일 위험                          (P1, M0 - 계획서 정정)
D-7. DX12Queue fence = frame slot per-index, 전역 monotonic 아님  (P1, M2)

[v2 신규 — codex 3차 검토 반영]
D-8.  UpdateLib.bat = Debug-DX12 DLL 미배포 → Client M1/M2 검증 시 DX11 DLL 로드 (P1, M0-4)
D-9.  Engine.vcxproj L241-244 = AssetConverter main.cpp ExcludedFromBuild Debug-DX12/Release-DX12 누락 (P1, M0-5)
D-10. DX12Device.cpp L115/L130 = Feature level 12_0 only — v1 의 11_0 fallback 박제 정정 (P2, §3 M1-2 정정)
D-11. DX12SwapChain.cpp L91-96 = Resize deferred stub — v1 의 Alt+Tab Resize 검증 박제 정정 (P2, §4 M2-5/6 정정)
D-12. Server.vcxproj L148-154 = LinkLibraryDependencies=true → Debug-DX12 솔루션 빌드 시 link/run config mismatch (P2, M0-6)
```

영향 요약:
- D-1: linker error 가능 (D3D12MA wrapper 가드 / dxgi/d3d12 lib 경로 / 28 파일 #if 가드)
- D-4: WaitIdle / 소멸자 시퀀스는 박제, fence signal 누설 가능성
- D-5: sln alias 정합성. WINTERS_RHI_BACKEND_DX12 미정의 시 28 파일 모두 #if 가드로 컴파일 제외
- D-6: 박제안의 직접 등재가 실제 wrapper 와 충돌 → 다중 정의 linker error. 본 §0-11 정정 + M0-1 박제안 제거로 해결
- D-7: Frame 0 → 1 → 0 시퀀스에서 fence value 가 frame slot 별로 독립 증가. frame 1 의 fence value 가 frame 0 의 다음 fence value 보다 작을 수 있음
- **D-8 (v2 신규)**: M1-1 의 `[CDX12Device] Bootstrap device ready.` 로그를 절대 못 봄. Client/Bin/Debug 의 WintersEngine.dll 이 DX11 빌드 그대로. **M0-4 가 M1 진입 전제 조건**.
- **D-9 (v2 신규)**: 현재 Rebuild 통과는 dead code 통과. 의도 mismatch — Engine DLL 에 컨버터 EXE 의 main 진입 소스 포함. M0-5 단순 보수.
- **D-10 (v2 신규)**: 계획서 박제와 코드 불일치 = 검증자가 11_0 fallback 동작 기대 → 실제로는 12_0 실패 시 WARP 12_0 → 둘 다 실패 시 fail. v2 = 코드 기준 박제.
- **D-11 (v2 신규)**: M2-5 / M2-6 의 Resize 검증 항목이 stub 코드와 충돌. v2 = M2-5 에서 Resize 검증 제외, M2-6 = W10-13 이동.
- **D-12 (v2 신규)**: Debug-DX12 솔루션 빌드 시 Server link line 에 Engine Debug-DX12 import lib 가 들어옴. ABI 호환 보장 필요 또는 sln Build.0 제거.

---

## §2. M0 - Compile-only 검증

### M0-1. Debug-DX12 Engine 빌드 통과 (codex 3차 검토 합격 기준 완화)

**v1 합격 기준**: "0 errors, 0 warnings"
**v2 합격 기준** (codex 3차 검토 P2 반영): **"0 errors, Debug baseline 동등 50 warnings 이하"**

```txt
msbuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64 /t:Engine /m
→ 0 errors
→ warnings <= Debug|x64 baseline (실측 50 warnings)
```

근거:
- 5/6 실측: Engine Debug-DX12 Rebuild = 성공, 0 errors, 50 warnings
- 5/6 실측: Engine Debug Rebuild = 성공, 0 errors, 50 warnings
- DX12 28 파일이 새로 추가되었지만 새 warning 0 = DX12 박제는 baseline 유지

검증 항목 (codex D-6 정정 - D3D12MA 직접 박제안 삭제, wrapper 패턴 확인만):

```txt
- WINTERS_RHI_BACKEND_DX12 매크로 정의됨 (Engine.vcxproj L115)
- d3d12.lib, dxgi.lib, d3dcompiler.lib, dxguid.lib linker 통과 (Engine.vcxproj L132)
- Engine/Private/RHI/DX12/DX12MemoryAllocator.cpp 가 vcxproj L260 등재 + D3D12MemAlloc.cpp 직접 #include 패턴 그대로 (DX12MemoryAllocator.cpp:5)
- D3D12MemAlloc.cpp 직접 등재 X (vcxproj <ItemGroup> 에 추가 시 다중 정의 linker error)
- AdditionalIncludeDirectories L119 = D3D12MA\Inc (Inc, Include 아님)
- Engine/Private/RHI/DX12/*.cpp 28 파일 모두 WINTERS_RHI_BACKEND_DX12 가드
- warning count <= 50 (Debug baseline 동등)
```

검증 명령:

```bash
# devenv 종료 후
cd /c/Users/user/Desktop/Winters
msbuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64 /t:Engine /m
dir Engine/Bin/Debug-DX12/   # WintersEngine.dll + .lib + .pdb 생성 확인
```

실패 시 대응:
- DX12 .cpp 가드 누락 시 각 파일 #if 둘러싸기 (현재 .h 는 `DX12SwapChain.h:3` 등에서 가드 확인)
- D3D12MA include 경로 오류 시 `D3D12MA\Inc` 로 수정 (`Include` X)
- D3D12MemAlloc.cpp 다중 정의 발생 시 절대 vcxproj 에 추가하지 말 것. `DX12MemoryAllocator.cpp` wrapper 가 이미 `#include` 함

### M0-2. Debug|x64 (DX11) 호환 확인

DX12 박제가 DX11 빌드 영향 없는지 확인:

```bash
msbuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
```

DX12 헤더가 `#if WINTERS_RHI_BACKEND_DX12` 가드되어 DX11 빌드 시 컴파일 0. WintersEngine.dll 사이즈 변화 없어야 정상.

### M0-3. sln alias 동작 검증 (codex 4차 검토 합격 기준 정정)

**v1 합격 기준**: "Debug-DX12 선택 시 Server/Client/Tools 가 자동으로 Debug|x64 빌드"
**v2 합격 기준**: alias 정상 + UpdateLib race + Server link line 정합
**v3 합격 기준** (최종): **Debug-DX12 / Release-DX12 솔루션 빌드 = Engine 만 빌드**. Server/Client/Tools 는 ActiveCfg 매핑만 남기고 Build.0 를 제거한다.

정정 이유:

1. Client PreBuild 의 `UpdateLib.bat` 는 `EngineSDK/inc` 를 `rd /S /Q` 후 `xcopy /S` 로 재생성한다. 병렬 솔루션 빌드(`/m`)에서는 Client 컴파일과 SDK 헤더 복사가 겹쳐 C1083 race 가 난다.
2. Server 는 `ProjectReference` + `LinkLibraryDependencies=true` 로 Engine Debug-DX12 import lib 를 링크 라인에 끌어온다. 하지만 PostBuild 는 Debug DLL 을 복사하므로 link/run mismatch 가 가능하다.
3. W7-9 단계의 목적은 Engine DX12 bootstrap 검증이다. Client/Server/Tools 일괄 빌드는 W10-13 Client DX12 config 단계로 미룬다.
4. 실행 파일 잠금 0 (devenv + WintersGame.exe + WintersServer.exe 종료 후 빌드)

```bash
# 빌드 전 사전 정리:
# - devenv 종료
# - taskkill /F /IM WintersGame.exe (있을 시)
# - taskkill /F /IM WintersServer.exe (있을 시)
# - taskkill /F /IM WintersAssetConverter.exe (있을 시)

msbuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64 /m:1
dir Engine/Bin/Debug-DX12/   # WintersEngine.dll + .lib + .pdb
```

검증 항목:

```txt
- Engine 만 Debug-DX12 폴더에 산출물 생성
- Client/Server/Tools 프로젝트는 Debug-DX12 솔루션 빌드에서 build 대상이 아님 (M0-6 Build.0 제거)
- /m:1 은 검증 안정성을 위해 사용. M0-6 적용 후에는 Client PreBuild UpdateLib.bat 가 실행되지 않아 EngineSDK/inc race 경로 자체가 사라져야 함
- 빌드 중 실행 파일 잠금 발생 시 사전 정리로 회피
- Server link line 자체가 Debug-DX12 솔루션 빌드에서 생성되지 않아야 함 (M0-6 옵션 a)
```

### M0-4. UpdateLib.bat Debug-DX12 DLL 배포 박제 (v2 신규, P1-A 해결)

**문제**: v1 박제 시점 UpdateLib.bat L42-58 = Debug/Release 만 복사. M1/M2 런타임 검증 시 Client/Bin/Debug 의 DX11 DLL 이 로드됨.

**선택 가능한 해결안 3종**:

#### 옵션 A — UpdateLib.bat 에 Debug-DX12 / Release-DX12 분기 추가 (권장)

`UpdateLib.bat` 의 L42-58 다음에 신규 블록 박제:

수정 전 (`UpdateLib.bat:42-58` 전문):

```bat
REM -- Engine build artifacts .lib -> EngineSDK/lib (Debug/Release) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)

REM -- Engine DLL + PDB -> Client output dirs (Debug/Release) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" "%ROOT%\Client\Bin\Debug\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.pdb" "%ROOT%\Client\Bin\Debug\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.dll" "%ROOT%\Client\Bin\Release\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.pdb" "%ROOT%\Client\Bin\Release\"
)
```

수정 후 (v2):

```bat
REM -- Engine build artifacts .lib -> EngineSDK/lib (Debug/Release/Debug-DX12/Release-DX12) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.lib" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.lib" "%ROOT%\EngineSDK\lib\"
)
REM v2 신규: Debug-DX12 lib (Server/Client 가 ProjectReference 로 link 시 사용)
if exist "%ROOT%\Engine\Bin\Debug-DX12\WintersEngine.lib" (
    if not exist "%ROOT%\EngineSDK\lib\Debug-DX12\" mkdir "%ROOT%\EngineSDK\lib\Debug-DX12\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug-DX12\WintersEngine.lib" "%ROOT%\EngineSDK\lib\Debug-DX12\"
)
if exist "%ROOT%\Engine\Bin\Release-DX12\WintersEngine.lib" (
    if not exist "%ROOT%\EngineSDK\lib\Release-DX12\" mkdir "%ROOT%\EngineSDK\lib\Release-DX12\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Release-DX12\WintersEngine.lib" "%ROOT%\EngineSDK\lib\Release-DX12\"
)

REM -- Engine DLL + PDB -> Client output dirs (Debug/Release/Debug-DX12/Release-DX12) --
if exist "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.dll" "%ROOT%\Client\Bin\Debug\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug\WintersEngine.pdb" "%ROOT%\Client\Bin\Debug\"
)
if exist "%ROOT%\Engine\Bin\Release\WintersEngine.dll" (
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.dll" "%ROOT%\Client\Bin\Release\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Release\WintersEngine.pdb" "%ROOT%\Client\Bin\Release\"
)
REM v2 신규: Debug-DX12 / Release-DX12 DLL+PDB → 별도 Client/Bin/Debug-DX12 디렉토리
if exist "%ROOT%\Engine\Bin\Debug-DX12\WintersEngine.dll" (
    if not exist "%ROOT%\Client\Bin\Debug-DX12\" mkdir "%ROOT%\Client\Bin\Debug-DX12\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug-DX12\WintersEngine.dll" "%ROOT%\Client\Bin\Debug-DX12\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Debug-DX12\WintersEngine.pdb" "%ROOT%\Client\Bin\Debug-DX12\"
)
if exist "%ROOT%\Engine\Bin\Release-DX12\WintersEngine.dll" (
    if not exist "%ROOT%\Client\Bin\Release-DX12\" mkdir "%ROOT%\Client\Bin\Release-DX12\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Release-DX12\WintersEngine.dll" "%ROOT%\Client\Bin\Release-DX12\"
    xcopy /Y /D "%ROOT%\Engine\Bin\Release-DX12\WintersEngine.pdb" "%ROOT%\Client\Bin\Release-DX12\"
)
```

전제: 이 옵션은 **Client 도 Debug-DX12 config 추가** 가 필요. Client.vcxproj 에 `Debug-DX12|x64` ProjectConfiguration 추가 + OutDir = `Client/Bin/Debug-DX12/` + Engine import lib path = `EngineSDK/lib/Debug-DX12/` (또는 ProjectReference 직접). 본 W7-9 단계 범위 외 (Client 가 DX12 RHI 사용은 W10-13 단계).

#### 옵션 B — DX12 Smoke Host 별도 신설 (M1/M2 단계만 권장)

```txt
Tools/DX12SmokeHost/
  main.cpp          // 최소 IWintersApp 구현 + WintersRun 호출 (Client 미사용)
  DX12SmokeHost.vcxproj  // Debug-DX12 config 만 보유, OutDir = Engine/Bin/Debug-DX12/
                          // Engine/Bin/Debug-DX12/WintersEngine.lib link, Engine.dll 동일 폴더 자동 로드
```

중요: SmokeHost 는 `CEngineApp` 를 직접 include/생성하지 않는다. 현재 DLL 공개 경계는 `Engine/Include/WintersEngine.h:34` 의 `WintersRun(IWintersApp*, const EngineConfig&)` 뿐이고, `CEngineApp` 는 export 클래스가 아니다.

`Tools/DX12SmokeHost/main.cpp` 최소 예시:

```cpp
#include <Windows.h>
#include "WintersEngine.h"

class CDX12SmokeApp final : public IWintersApp
{
public:
    bool OnInit() override
    {
        OutputDebugStringA("[DX12SmokeHost] OnInit\n");
        return true;
    }

    void OnShutdown() override
    {
        OutputDebugStringA("[DX12SmokeHost] OnShutdown\n");
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    EngineConfig config{};
    config.windowTitle = L"Winters DX12 Smoke";
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.iNumScenes = 1;
    config.rhiBackend = eEngineRHIBackend::DX12; // 현재 W7-9 는 compile-time macro 가 실제 선택권. 런타임 필드는 의미 표기용.

    CDX12SmokeApp app;
    return WintersRun(&app, config);
}
```

vcxproj 박제 포인트:

```xml
<OutDir>$(SolutionDir)Engine\Bin\$(Configuration)\</OutDir>
<IntDir>$(SolutionDir)Tools\Bin\Intermediate\DX12SmokeHost\$(Configuration)\</IntDir>
<AdditionalIncludeDirectories>$(SolutionDir)Engine\Include;$(SolutionDir)EngineSDK\inc;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
<AdditionalLibraryDirectories>$(SolutionDir)Engine\Bin\$(Configuration);%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
<AdditionalDependencies>WintersEngine.lib;%(AdditionalDependencies)</AdditionalDependencies>
<VcpkgEnableManifest>false</VcpkgEnableManifest>
<VcpkgEnabled>false</VcpkgEnabled>
```

SmokeHost 는 vcpkg 를 사용하지 않는다. `DX12SmokeHost.vcxproj` 에 `AfterTargets="Build"` 복사 타겟을 두고 Engine 의 런타임 의존 DLL 을 같은 `Engine/Bin/Debug-DX12/` 폴더로 복사한다.

```xml
<Target Name="CopyRuntimeDlls" AfterTargets="Build">
  <ItemGroup>
    <RuntimeDlls Include="$(MSBuildThisFileDirectory)..\..\Engine\ThirdPartyLib\Assimp\Bin\Debug\*.dll"
                 Condition="'$(Configuration)'=='Debug-DX12'" />
    <RuntimeDlls Include="$(MSBuildThisFileDirectory)..\..\Engine\ThirdPartyLib\DirectXTK\Bin\Debug\*.dll"
                 Condition="'$(Configuration)'=='Debug-DX12'" />
    <RuntimeDlls Include="$(MSBuildThisFileDirectory)..\..\Engine\ThirdPartyLib\FMOD\Bin\fmod.dll" />
  </ItemGroup>
  <Copy SourceFiles="@(RuntimeDlls)" DestinationFolder="$(OutDir)" SkipUnchangedFiles="true" />
</Target>
```

장점: Client/Server 변경 0. Engine 단독 검증 가능. M3 정상 종료까지 별도 host 로 검증.
단점: Host 신설 수고. W10-13 진입 시 Client 변경 필수 — 옵션 A 와 통합 불가피.

#### 옵션 C — Client/Bin/Debug 에 직접 덮어쓰기 (임시, 위험)

수동으로 `Engine/Bin/Debug-DX12/WintersEngine.dll` 을 `Client/Bin/Debug/` 에 복사 후 실행. 디버그 검증만, 실수로 DX11 DLL 이 다시 덮어쓰여질 위험. **권장 X**.

**v3 본 계획서 결정**:
- M1/M2 단계 = **옵션 B (DX12SmokeHost)** 신설 박제 — Client 변경 0 유지
- W10-13 진입 시 = **옵션 A (UpdateLib.bat 분기 + Client.vcxproj Debug-DX12 config)** 박제

### M0-4 합격 기준

- Engine Debug-DX12 빌드 후 산출물 = `Engine/Bin/Debug-DX12/WintersEngine.dll` + `WintersEngine.pdb` + `WintersEngine.lib`
- DX12SmokeHost.exe 가 동일 폴더에서 실행 (옵션 B) → Engine.dll 자동 로드
- 또는 옵션 A 박제 후 Client/Bin/Debug-DX12/WintersEngine.dll 존재
- M1-1 의 `[CDX12Device] Bootstrap device ready.` 로그 가시화

### M0-5. Engine.vcxproj AssetConverter main.cpp ExcludedFromBuild 4 config 보정 (v2 신규, P1-B 해결)

**문제**: `Engine/Include/Engine.vcxproj:241-244` 의 `ExcludedFromBuild` 가 Debug/Release 만 박제됨. Debug-DX12 / Release-DX12 빌드 시 main.cpp 가 Engine DLL 에 포함됨.

수정 전 (`Engine/Include/Engine.vcxproj:241-244` 전문):

```xml
<ClCompile Include="..\Private\Tools\AssetConverter\main.cpp">
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">true</ExcludedFromBuild>
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Release|x64'">true</ExcludedFromBuild>
</ClCompile>
```

수정 후 (v2, 4 config 모두):

```xml
<ClCompile Include="..\Private\Tools\AssetConverter\main.cpp">
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">true</ExcludedFromBuild>
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Release|x64'">true</ExcludedFromBuild>
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Debug-DX12|x64'">true</ExcludedFromBuild>
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Release-DX12|x64'">true</ExcludedFromBuild>
</ClCompile>
```

추가 검증: Engine.vcxproj 안의 다른 EXE 진입점 .cpp (있다면) 도 동일 패턴으로 4 config 박제. 현재는 main.cpp 1 개만 EXE 진입점으로 식별됨.

#### M0-5 합격 기준

- Debug-DX12 / Release-DX12 빌드 후 IntDir (`Engine/Bin/Intermediate/Debug-DX12/`) 에 `main.obj` 생성 0
- Engine DLL link 시 main 함수 부재 (`/SUBSYSTEM:WINDOWS` linker warning 0)
- AssetConverter EXE 빌드는 별도 vcxproj (`Tools/WintersAssetConverter/`) 가 main.cpp 직접 포함 — 영향 0 확인

### M0-6. Server alias config 정합 검증 (v2 신규, P2-C 해결)

**문제**: `Server/Include/Server.vcxproj:148-154` 의 `LinkLibraryDependencies=true` + `ProjectReference` 로 인해 Debug-DX12 솔루션 빌드 시 Server 가 Engine Debug-DX12 import lib 를 link line 에 포함. PostBuild 는 Debug DLL 을 복사 → link/run mismatch 가능.

**선택 가능 3종**:

#### 옵션 (a) — Server/Client/Tools 자동 제외 (sln Build.0 제거, 권장)

`Winters.sln` 의 `GlobalSection(ProjectConfigurationPlatforms)` 에서 **아래 Build.0 6개 라인만 삭제**한다. `.sln` 섹션 내부에 `#` 주석을 넣으면 안 된다.

삭제 대상 (현재 `Winters.sln:35,39,43,47,51,55`):

```txt
{E3333333-3333-3333-3333-333333333333}.Debug-DX12|x64.Build.0 = Debug|x64
{E3333333-3333-3333-3333-333333333333}.Release-DX12|x64.Build.0 = Release|x64
{E2222222-2222-2222-2222-222222222222}.Debug-DX12|x64.Build.0 = Debug|x64
{E2222222-2222-2222-2222-222222222222}.Release-DX12|x64.Build.0 = Release|x64
{E4444444-4444-4444-4444-444444444444}.Debug-DX12|x64.Build.0 = Debug|x64
{E4444444-4444-4444-4444-444444444444}.Release-DX12|x64.Build.0 = Release|x64
```

보존 대상 ActiveCfg 6개 라인:

```txt
{E3333333-3333-3333-3333-333333333333}.Debug-DX12|x64.ActiveCfg = Debug|x64
{E3333333-3333-3333-3333-333333333333}.Release-DX12|x64.ActiveCfg = Release|x64
{E2222222-2222-2222-2222-222222222222}.Debug-DX12|x64.ActiveCfg = Debug|x64
{E2222222-2222-2222-2222-222222222222}.Release-DX12|x64.ActiveCfg = Release|x64
{E4444444-4444-4444-4444-444444444444}.Debug-DX12|x64.ActiveCfg = Debug|x64
{E4444444-4444-4444-4444-444444444444}.Release-DX12|x64.ActiveCfg = Release|x64
```

→ Debug-DX12 / Release-DX12 솔루션 빌드 시 Server/Client/Tools 미빌드. ProjectReference link mismatch 0. Engine 은 `{E1111111-1111-1111-1111-111111111111}` 의 DX12 Build.0 를 유지한다.

장점: 가장 단순. Client/Tools 도 동일 패턴 적용 가능.
단점: Server 의 평소 Debug 빌드는 사용자가 Configuration=Debug 로 명시 빌드해야 함.

#### 옵션 (b) — Server 도 Debug-DX12 config 추가 (Engine 와 동일)

Server.vcxproj 에 Debug-DX12 ProjectConfiguration 추가 + Engine.vcxproj 와 동일 OutDir 패턴.

장점: Server 도 Debug-DX12 솔루션 일괄 빌드 가능.
단점: Server 가 RHI 미사용임에도 DX12 macro 정의 받음. 의도 모호.

#### 옵션 (c) — Engine import lib 의 ABI 호환 보장 + 명시

Engine 의 export 시그니처 (CGameInstance / IRHIDevice 등) 가 Debug 와 Debug-DX12 사이에서 ABI 호환임을 명시 검증. WINTERS_RHI_BACKEND_DX12 매크로가 export 헤더에 영향 안 주는지 확인.

검증:
```bash
dumpbin /EXPORTS Engine/Bin/Debug/WintersEngine.lib > debug_exports.txt
dumpbin /EXPORTS Engine/Bin/Debug-DX12/WintersEngine.lib > debug_dx12_exports.txt
diff debug_exports.txt debug_dx12_exports.txt
# diff 0 = ABI 호환 / diff > 0 = mismatch (mangling 차이)
```

**v3 본 계획서 결정**: **옵션 (a) — sln Build.0 제거 (Server/Client/Tools 모두)**. 가장 명확. Debug-DX12 / Release-DX12 솔루션 빌드 = Engine 만 빌드, Client/Server/Tools = 사용자가 별도 Debug/Release 빌드.

#### M0-6 합격 기준

- Debug-DX12 / Release-DX12 솔루션 빌드 시 Server/Client/Tools 자동 제외 (msbuild output 에 "skipping" 또는 미언급)
- Engine Debug-DX12 / Release-DX12 빌드 후 Server/Client/Tools 가 Debug/Release 폴더 산출물에 미영향
- (옵션 c 적용 시) Engine Debug 와 Debug-DX12 의 export 시그니처 diff 0

---

## §3. M1 - Device Bootstrap (codex D-2 정정 + D-10 보정)

### M1-1. CDX12Device::Create 진입점 (실재 본체 인용)

현재 상태: `CDX12Device::Create` (`DX12Device.cpp:14-21`) + 호출자 (`CEngineApp.cpp:64-82`) 모두 박제 완료. M1 = 빌드 + 런타임 검증, 박제 X.

기존 박제 본체 (`Engine/Private/Framework/CEngineApp.cpp:55-93`):

```cpp
    wndDesc.fullscreen = config.fullscreen;

    if (!m_Window.Create(wndDesc))
    {
        OutputDebugStringA("[CEngineApp] Window creation failed\n");
        return false;
    }


#if defined(WINTERS_RHI_BACKEND_DX12)
    DX12DeviceDesc devDesc;
    devDesc.hwnd       = m_Window.GetHandle();
    devDesc.width      = config.windowWidth;
    devDesc.height     = config.windowHeight;
    devDesc.vsync      = config.vsync;
    devDesc.fullscreen = config.fullscreen;

    m_pDevice = CDX12Device::Create(devDesc);
#else
    DeviceDesc devDesc;
    devDesc.hwnd       = m_Window.GetHandle();
    devDesc.width      = config.windowWidth;
    devDesc.height     = config.windowHeight;
    devDesc.vsync      = config.vsync;
    devDesc.fullscreen = config.fullscreen;

    m_pDevice = CDX11Device::Create(devDesc);
#endif
    if (!m_pDevice)
    {
        MessageBoxW(m_Window.GetHandle(),
            L"DX11 디바이스 초기화에 실패했습니다.\n"
            L"그래픽 카드가 D3D11을 지원하는지 확인하세요",
            L"[CEngineApp] DX11 초기화 실패", MB_OK | MB_ICONERROR);
        return false;
    }


    m_bDX11RuntimeEnabled = m_pDevice->GetBackend() == eRHIBackend::DX11;

#if !defined(WINTERS_RHI_BACKEND_DX12)
    if(!m_ImGui.Initialize(m_Window.GetHandle(), m_pDevice.get()))
    {
        OutputDebugStringA("[CEngineApp] ImGui initialization failed\n");
        return false;
    }
```

DX12 진입 시 ImGui 초기화 자동 skip (`#if !defined(WINTERS_RHI_BACKEND_DX12)` 가드, L95). DX12 ImGui backend 박제는 별도 작업 (W10-13 진입 시).

검증 항목 (박제 변경 X, 런타임 동작만 확인):

```txt
- M0-4 옵션 B 의 DX12SmokeHost.exe 또는 옵션 A 의 Client/Bin/Debug-DX12/WintersGame.exe 실행
- [CDX12Device] Bootstrap device ready. 로그 출력 (DX12Device.cpp:74)
- DX12 분기 진입 (런타임 디버거에서 m_pDevice->GetBackend() == eRHIBackend::DX12 확인)
- m_bDX11RuntimeEnabled == false (CEngineApp.cpp:93)
- DX11 fallback 메시지박스 안 뜸 (m_pDevice != nullptr)
- D3D12 Debug Layer 활성 (CreateFactory L82-89 의 EnableDebugLayer → 콘솔에 D3D12 INFO: ... 메시지 출력)
- ID3D12Device + IDXGIFactory6 + D3D12MA::Allocator 모두 nullptr 아님 (Bootstrap 로그 출력 = 8 단계 모두 통과)
```

한국어 메시지박스 인코딩 주의 (CEngineApp.cpp:86-87): 현재 표시는 깨져있음 (CP949 → UTF-8 mojibake). 별도 hygiene PR 대상 (본 계획서 범위 X).

### M1-2. CreateAdapterAndDevice — Hardware + WARP, 12_0 only (v2 정정, D-10 해결)

**v1 박제 (정정 대상)**: "12_0 시도, 실패 시 11_0 fallback. WARP fallback 비활성".

**v2 박제 (코드 기준)**: §0-7 인용 그대로 — `D3D_FEATURE_LEVEL_12_0` only (hardware + WARP), 11_0 fallback 박제 X. WARP fallback 활성 (hardware 12_0 실패 시 WARP 12_0 시도).

박제 본체: `DX12Device.cpp:97-141` (§0-7 전수 인용 참조).

#### 합격 기준 (v2 정정)

- DXGI_ADAPTER_DESC3 의 Description 출력 (예: "NVIDIA GeForce RTX 4080" / "AMD Radeon ...")
- `D3D12CreateDevice` 가 `D3D_FEATURE_LEVEL_12_0` 으로 성공
- Hardware 12_0 실패 시 WARP 12_0 시도 — WARP 성공 시 `[CDX12Device] Using WARP fallback adapter.` 로그 출력
- 둘 다 실패 시 `[CDX12Device] D3D12 device creation failed.` 로그 + `m_pDevice == nullptr` → DX11 메시지박스
- **11_0 fallback 검증 항목 박제 X** (현재 코드 미지원). 11_0 호환성은 W10-13 진입 시 별도 옵션.

### M1-3. CreateAllocator — D3D12MA

박제 본체 (`DX12Device.cpp:143-153` 인용):

```cpp
bool_t CDX12Device::CreateAllocator()
{
    if (!m_pDevice || !m_pAdapter)
        return false;

    D3D12MA::ALLOCATOR_DESC allocatorDesc{};
    allocatorDesc.pDevice = m_pDevice.Get();
    allocatorDesc.pAdapter = m_pAdapter.Get();

    return SUCCEEDED(D3D12MA::CreateAllocator(&allocatorDesc, &m_pAllocator));
}
```

합격 기준:
- `m_pAllocator != nullptr`
- `Stats::TotalBytes` 출력 가능 (디버깅용 로그)

### M1-4. CreateQueues — Graphics/Compute/Copy 3 queue

박제 본체 (`DX12Device.cpp:155-` + `DX12Queue.cpp:30-51`):

```cpp
// DX12Device.cpp:155-
bool_t CDX12Device::CreateQueues()
{
    m_pGraphicsQueue = CDX12Queue::Create(m_pDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_pComputeQueue = CDX12Queue::Create(m_pDevice.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
    m_pCopyQueue = CDX12Queue::Create(m_pDevice.Get(), D3D12_COMMAND_LIST_TYPE_COPY);
    // ... return all != nullptr
}

// DX12Queue.cpp:30-51 (Initialize)
bool_t CDX12Queue::Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type)
{
    if (!pDevice) return false;
    m_Type = type;

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = type;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    if (FAILED(pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pQueue))))
        return false;
    if (FAILED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence))))
        return false;

    m_hFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return m_hFenceEvent != nullptr;
}
```

합격 기준:
- `m_pGraphicsQueue != nullptr` && `m_pComputeQueue != nullptr` && `m_pCopyQueue != nullptr`
- 각 queue 의 fence 가 GPU 시그널 받음 (간단 stress test: 빈 command list signal/wait)
- 단, M2-4 변경 후 monotonic 보장 검증

---

## §4. M2 - SwapChain + Present

### M2-1. CDX12SwapChain 3 backbuffer 생성

박제 본체 (`DX12SwapChain.cpp:18-78` 인용):

```cpp
bool_t CDX12SwapChain::Initialize(CDX12Device* pDevice, const RHIWindowHandle& window)
{
    if (!pDevice || !window.nativeWindow || !pDevice->GetDXGIFactory() || !pDevice->GetDX12GraphicsQueue())
        return false;

    m_Width = window.width;
    m_Height = window.height;

    BOOL allowTearing = FALSE;
    Microsoft::WRL::ComPtr<IDXGIFactory5> pFactory5;
    if (SUCCEEDED(pDevice->GetDXGIFactory()->QueryInterface(IID_PPV_ARGS(&pFactory5))))
    {
        if (SUCCEEDED(pFactory5->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
        {
            m_bTearingSupported = allowTearing == TRUE;
        }
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = m_Width;
    desc.Height = m_Height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBackBufferCount;       // 3
    desc.SampleDesc.Count = 1;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = m_bTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain1;
    if (FAILED(pDevice->GetDXGIFactory()->CreateSwapChainForHwnd(
        pDevice->GetDX12GraphicsQueue()->GetQueue(),
        static_cast<HWND>(window.nativeWindow),
        &desc, nullptr, nullptr, &pSwapChain1)))
        return false;

    if (FAILED(pSwapChain1.As(&m_pSwapChain)))
        return false;

    pDevice->GetDXGIFactory()->MakeWindowAssociation(
        static_cast<HWND>(window.nativeWindow),
        DXGI_MWA_NO_ALT_ENTER);

    for (u32_t i = 0; i < kBackBufferCount; ++i)
    {
        if (FAILED(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pBackBuffers[i]))))
            return false;
    }

    m_BackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
    return true;
}
```

합격 기준:
- `m_pSwapChain != nullptr`
- 3 backbuffer 모두 `GetBuffer` 성공
- `m_bTearingSupported` 정확히 박제 (`CheckFeatureSupport` `DXGI_FEATURE_PRESENT_ALLOW_TEARING`)
- `MakeWindowAssociation(DXGI_MWA_NO_ALT_ENTER)` — Alt+Enter 자동 fullscreen 비활성

### M2-2. CreateFrameResources + CreateBackBufferViews

박제 본체: `DX12Device.cpp` 의 두 메서드 (실측 후 검증).

예상 동작:
- `CreateFrameResources`: 3 command allocator (`D3D12_COMMAND_LIST_TYPE_DIRECT`) + 1 graphics command list
- `CreateBackBufferViews`: RTV heap 3 슬롯 + 각 backbuffer 에 RTV 생성

합격 기준:
- `m_pCommandAllocators[0..2] != nullptr`
- `m_pCommandList != nullptr`
- `m_pRTVHeap != nullptr`
- `m_RTVDescriptorSize` 가 0 아닌 값 (보통 32 bytes on NVIDIA)

### M2-3. BeginFrame → Clear → EndFrame Present (codex D-3 정정, 본체 실재 인용)

박제 본체 (`Engine/Private/RHI/DX12/DX12Device.cpp:242-300`):

```cpp
void CDX12Device::BeginFrame(f32_t r, f32_t g, f32_t b, f32_t a)
{
    if (!m_pSwapChain || !m_pGraphicsQueue || !m_pCommandList)
        return;

    m_ClearColor[0] = r;
    m_ClearColor[1] = g;
    m_ClearColor[2] = b;
    m_ClearColor[3] = a;

    m_FrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
    m_pGraphicsQueue->WaitForFrame(m_FrameIndex);

    auto* pAllocator = m_pCommandAllocators[m_FrameIndex].Get();
    pAllocator->Reset();
    m_pCommandList->Reset(pAllocator, nullptr);

    ID3D12Resource* pBackBuffer = m_pSwapChain->GetBackBuffer(m_FrameIndex);
    D3D12_RESOURCE_STATES& state = m_BackBufferStates[m_FrameIndex];

    if (state != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = pBackBuffer;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = state;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        m_pCommandList->ResourceBarrier(1, &barrier);
        state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(m_FrameIndex) * m_RTVDescriptorSize;
    m_pCommandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    m_pCommandList->ClearRenderTargetView(rtv, m_ClearColor, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = pBackBuffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = state;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_pCommandList->ResourceBarrier(1, &barrier);
    state = D3D12_RESOURCE_STATE_PRESENT;

    m_pCommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { m_pCommandList.Get() };
    m_pGraphicsQueue->GetQueue()->ExecuteCommandLists(1, ppCommandLists);
}

void CDX12Device::EndFrame()
{
    if (!m_pSwapChain || !m_pGraphicsQueue)
        return;

    m_pSwapChain->Present(m_bVSync);
    m_pGraphicsQueue->Signal(m_FrameIndex);
}
```

박제 분석:
- BeginFrame 이 PRESENT → RENDER_TARGET → ClearRTV → PRESENT 전이 모두 한 command list 안에서 + Execute 까지 호출 (단일 command list submit)
- EndFrame 은 Present + Signal 만 (다음 frame 의 BeginFrame WaitForFrame 가 이전 frame fence 대기)
- ClearRenderTargetView 외 draw call 없음. M2 합격 시 화면 = dark navy (`m_ClearColor` default `{0.08, 0.08, 0.12, 1.0}`)

검증 항목 (박제 변경 X, 런타임 동작만 확인):

```txt
- BeginFrame(0.08, 0.08, 0.12, 1.0) → EndFrame() 1 프레임 호출 시 화면에 dark navy clear color 표시
- DXGI Validation Layer 0 warning (Output 창 D3D12 ERROR/WARNING 0건)
- 60 FPS (vsync) 정상 측정 (Profiler/PIX)
- 60 초 frame counter 정상 증가 (3600 frames @ 60fps)
- m_BackBufferStates[0..2] 가 매 frame 끝에 PRESENT 로 복귀
- M2-4 적용 후 fence value 가 1, 2, 3, ... 단조 증가 (monotonic)
```

### M2-4. DX12Queue fence 전역 monotonic 변경 (codex D-7, P1-C 보정)

문제 (`Engine/Private/RHI/DX12/DX12Queue.cpp:80-117` 실측 인용 — 본 박제는 변경 대상):

```cpp
void CDX12Queue::Signal(u32_t frameIndex)
{
    if (!m_pQueue || !m_pFence)
        return;

    const u32_t safeIndex = frameIndex % 3u;
    const u64_t fenceValue = ++m_FenceValues[safeIndex];   // ★ frame slot per-index
    m_pQueue->Signal(m_pFence.Get(), fenceValue);
}

void CDX12Queue::WaitForFrame(u32_t frameIndex)
{
    if (!m_pFence || !m_hFenceEvent)
        return;

    const u32_t safeIndex = frameIndex % 3u;
    const u64_t fenceValue = m_FenceValues[safeIndex];
    if (fenceValue == 0 || m_pFence->GetCompletedValue() >= fenceValue)
        return;

    m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent);
    WaitForSingleObject(m_hFenceEvent, INFINITE);
}

void CDX12Queue::Flush()
{
    if (!m_pQueue || !m_pFence || !m_hFenceEvent)
        return;

    const u64_t fenceValue = ++m_FenceValues[0];           // ★ 슬롯 0 만 사용
    m_pQueue->Signal(m_pFence.Get(), fenceValue);

    if (m_pFence->GetCompletedValue() < fenceValue)
    {
        m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent);
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }
}
```

Frame 0 → 1 → 0 시퀀스에서 fence value 가 frame slot 별로 독립 증가. frame 1 의 fence value 가 frame 0 의 다음 fence value 보다 작을 수 있다. ID3D12Fence::Signal 은 monotonic 보장 안 됨 (작은 값으로 signal 시 SetEventOnCompletion 이 즉시 발화 → 미완료 GPU 작업을 완료로 오인). 또한 Flush 가 슬롯 0 만 증가시키므로 다른 슬롯과 카운터 sync 깨짐.

#### 수정 1 — DX12Queue.h 멤버 변경

수정 전 (`Engine/Private/RHI/DX12/DX12Queue.h:29-33` 전문):

```cpp
Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pQueue;
Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;
u64_t m_FenceValues[3] = {};
HANDLE m_hFenceEvent = nullptr;
D3D12_COMMAND_LIST_TYPE m_Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
```

수정 후 (v2):

```cpp
Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pQueue;
Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;
u64_t m_NextFenceValue = 0;                     // 전역 monotonic counter (single-thread submit 가정)
u64_t m_FrameFenceValues[3] = {};               // frame 별 마지막 시그널 값 저장
HANDLE m_hFenceEvent = nullptr;
D3D12_COMMAND_LIST_TYPE m_Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
```

주: Track 3 Fiber 진입 후 multi-thread submit 가능 시 `std::atomic<u64_t> m_NextFenceValue{ 0 };` 으로 변경. 현재 W7-9 단계는 single-thread submit (CEngineApp main loop) 이라 plain `u64_t` 충분.

#### 수정 2 — Signal 변경 (`DX12Queue.cpp:80-88`)

수정 전:

```cpp
void CDX12Queue::Signal(u32_t frameIndex)
{
    if (!m_pQueue || !m_pFence)
        return;

    const u32_t safeIndex = frameIndex % 3u;
    const u64_t fenceValue = ++m_FenceValues[safeIndex];
    m_pQueue->Signal(m_pFence.Get(), fenceValue);
}
```

수정 후 (v2):

```cpp
void CDX12Queue::Signal(u32_t frameIndex)
{
    if (!m_pQueue || !m_pFence)
        return;

    const u32_t safeIndex = frameIndex % 3u;
    // 전역 monotonic counter 증가 → frame slot 에 저장
    const u64_t fenceValue = ++m_NextFenceValue;
    m_FrameFenceValues[safeIndex] = fenceValue;
    m_pQueue->Signal(m_pFence.Get(), fenceValue);
}
```

#### 수정 3 — WaitForFrame 변경 (`DX12Queue.cpp:90-102`)

수정 전:

```cpp
void CDX12Queue::WaitForFrame(u32_t frameIndex)
{
    if (!m_pFence || !m_hFenceEvent)
        return;

    const u32_t safeIndex = frameIndex % 3u;
    const u64_t fenceValue = m_FenceValues[safeIndex];
    if (fenceValue == 0 || m_pFence->GetCompletedValue() >= fenceValue)
        return;

    m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent);
    WaitForSingleObject(m_hFenceEvent, INFINITE);
}
```

수정 후 (v2):

```cpp
void CDX12Queue::WaitForFrame(u32_t frameIndex)
{
    if (!m_pFence || !m_hFenceEvent)
        return;

    const u32_t safeIndex = frameIndex % 3u;
    const u64_t fenceValue = m_FrameFenceValues[safeIndex];
    if (fenceValue == 0 || m_pFence->GetCompletedValue() >= fenceValue)
        return;

    m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent);
    WaitForSingleObject(m_hFenceEvent, INFINITE);
}
```

#### 수정 4 — Flush 변경 (`DX12Queue.cpp:104-117`)

수정 전:

```cpp
void CDX12Queue::Flush()
{
    if (!m_pQueue || !m_pFence || !m_hFenceEvent)
        return;

    const u64_t fenceValue = ++m_FenceValues[0];
    m_pQueue->Signal(m_pFence.Get(), fenceValue);

    if (m_pFence->GetCompletedValue() < fenceValue)
    {
        m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent);
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }
}
```

수정 후 (v2):

```cpp
void CDX12Queue::Flush()
{
    if (!m_pQueue || !m_pFence || !m_hFenceEvent)
        return;

    // 전역 monotonic counter 사용. Flush 후 모든 frame slot 의 last value 도 갱신
    // (Flush 가 GPU 의 모든 outstanding work 를 완료로 표시)
    const u64_t fenceValue = ++m_NextFenceValue;
    m_pQueue->Signal(m_pFence.Get(), fenceValue);

    if (m_pFence->GetCompletedValue() < fenceValue)
    {
        m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent);
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }

    // Flush 는 모든 outstanding work 완료를 의미 → frame slot 카운터를 그 값으로 sync
    for (u32_t i = 0; i < 3u; ++i)
        m_FrameFenceValues[i] = fenceValue;
}
```

#### M2-4 합격 기준

- Frame 0 → 1 → 0 → 1 → ... 순서에서 fence value 가 매 frame 1, 2, 3, 4, ... 단조 증가
- `m_NextFenceValue` 가 모든 Signal/Flush 호출 후 strict monotonic
- `m_FrameFenceValues[0..2]` 가 마지막 signal 값만 저장
- DXGI Validation 0 warning (특히 fence-related)
- 1000 프레임 stress test (60초 @ 60fps 미만 vsync) 시 GPU hang 0 hit
- WaitForFrame 호출 시 stale frame value 로 즉시 return 케이스 0 (실제 GPU 작업 미완료를 미완료로 정확 인식)

### M2-5. 렌더 루프 통합 (codex D-11 보정)

박제 위치: `Engine/Private/Framework/CEngineApp.cpp` 의 main loop 내 (현재 DX11 BeginFrame/EndFrame 호출 위치).

**v2 합격 기준 (D-11 반영, Resize 검증 제거)**:
- 60 초 동안 clear color 만 표시 (mesh/scene 없이) → frame counter 정상 증가
- DXGI Validation Layer 0 warning
- M2-4 적용 후 fence monotonic 검증

**제외 항목 (v1 박제에서 삭제)**:
- ~~"Alt+Tab → 화면 전환 시 swapchain Resize 정상"~~ — DX12SwapChain.cpp:91-96 의 Resize 가 stub 이므로 본 단계 검증 불가능. W10-13 으로 이동.

### M2-6. Resize 옵션 — W10-13 이동 (codex D-11 보정)

**v1 박제 (정정 대상)**: "M2 합격 후 진입. WaitIdle → ResizeBuffers → CreateBackBufferViews 재호출."

**v2 박제 (코드 기준)**: `CDX12SwapChain::Resize` 는 §0-10 인용 그대로 deferred stub:

```cpp
void CDX12SwapChain::Resize(u32_t width, u32_t height)
{
    (void)width;
    (void)height;
    OutputDebugStringA("[CDX12SwapChain] Resize is deferred to W10-13.\n");
}
```

본 W7-9 합격 범위에서 **완전 제외**. W10-13 의 시각 동일성 단계에서 별도 박제:
- WaitIdle 호출
- 모든 backbuffer ComPtr 해제
- `m_pSwapChain->ResizeBuffers(kBackBufferCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, flags)`
- backbuffer 재취득 + RTV 재생성
- `m_BackBufferStates[]` PRESENT 로 초기화

W7-9 합격 검증 항목 변경:
- Alt+Tab 정상 동작 확인은 화면 깨짐 허용 (Resize stub) — log 만 출력되면 합격
- 실제 fullscreen toggle 은 `MakeWindowAssociation(DXGI_MWA_NO_ALT_ENTER)` 로 자동 fullscreen 비활성 = 윈도우 모드 고정

---

## §5. M3 - 정상 종료 (WaitIdle + 소멸자)

### M3-1. Alt+F4 시퀀스

현 박제 (`DX12Device.cpp:23-42`, §0-8 인용 참조):

```cpp
CDX12Device::~CDX12Device()
{
    WaitIdle();                          // 모든 fence signal 대기
    m_pCommandList.Reset();
    for (auto& pAllocator : m_pCommandAllocators)
        pAllocator.Reset();
    m_pRTVHeap.Reset();
    m_pSwapChain.reset();
    m_pCopyQueue.reset();
    m_pComputeQueue.reset();
    m_pGraphicsQueue.reset();
    if (m_pAllocator) {
        m_pAllocator->Release();
        m_pAllocator = nullptr;
    }
}
```

합격 기준:
- Alt+F4 시 WaitIdle 호출 → 모든 GPU fence 시그널 (대기 시간 < 100ms)
- D3D12 Live Object Reporter 출력 0 leak (Debug Layer)
- DXGI Live Object 0 leak

### M3-2. Live Object 검증 (v3 코드 패치 박제)

박제 위치: `Engine/Private/RHI/DX12/DX12Device.cpp`.

현재 `DX12Device.cpp:12` 에 `<dxgidebug.h>` include 는 이미 존재하지만 `DXGIGetDebugInterface1` / `ReportLiveObjects` 호출은 없다. M3 합격 기준을 유지하려면 아래 패치를 실제 코드 변경 대상으로 포함한다.

추가 1 — include 아래 helper:

```cpp
#if defined(_DEBUG)
namespace
{
    void ReportDX12LiveObjects()
    {
        Microsoft::WRL::ComPtr<IDXGIDebug1> pDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
        {
            pDebug->ReportLiveObjects(
                DXGI_DEBUG_ALL,
                DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL);
        }
    }
}
#endif
```

추가 2 — `CDX12Device::~CDX12Device()` 의 끝에서 모든 DXGI/D3D12 소유 객체를 명시 Reset 후 report:

```cpp
CDX12Device::~CDX12Device()
{
    WaitIdle();

    m_BindGroupTable.Clear();
    m_BindGroupLayoutTable.Clear();
    m_RenderPassTable.Clear();
    m_PipelineTable.Clear();

    m_pCommandList.Reset();
    for (auto& pAllocator : m_pCommandAllocators)
        pAllocator.Reset();

    m_pRTVHeap.Reset();
    m_pSwapChain.reset();
    m_pCopyQueue.reset();
    m_pComputeQueue.reset();
    m_pGraphicsQueue.reset();

    if (m_pAllocator)
    {
        m_pAllocator->Release();
        m_pAllocator = nullptr;
    }

    m_pDevice.Reset();
    m_pAdapter.Reset();
    m_pFactory.Reset();

#if defined(_DEBUG)
    ReportDX12LiveObjects();
#endif
}
```

주의: `CRHIResourceTable` 은 소멸자에서 `Clear()` 를 호출하지만, Live Object report 를 destructor body 안에서 찍으려면 table 멤버 소멸보다 먼저 명시 Clear 해야 false positive 를 줄일 수 있다.

합격 기준:
- 출력 창에 `Live Objects: 0` 또는 debug layer 내부 기본 객체 수준의 잔여만 표시
- `RefCount > 1` 의 Winters 소유 D3D12/DXGI 객체 0 hit

### M3-3. PIX 캡처 (옵션)

PIX for Windows 로 1 프레임 캡처. command list 시퀀스 검증 (PRESENT → RT → Clear → RT → PRESENT 순).

---

## §6. 진입 순서 + 합격 단계 (v2 갱신)

```txt
0. Stabilization-0 통과
   - Engine/External/imgui imconfig.h 정리
   - Debug|x64 Engine 빌드 (DX11 호환 유지)
   - devenv 종료 + WintersGame.exe / WintersServer.exe / WintersAssetConverter.exe 종료
   - git checkout -b feature/rhi-dx12-bootstrap

1. M0 Compile-only + 배포 경로 확정 (v2 신규 항목 포함)
   - M0-1: Debug-DX12 Engine 빌드 0 errors + Debug baseline 동등 (50 warnings 이하)
   - M0-2: Debug|x64 DX11 빌드 영향 0
   - M0-3: sln alias 정상 (UpdateLib race 회피, 실행 파일 잠금 0, /m:1 빌드)
   - M0-4: UpdateLib.bat Debug-DX12 분기 추가 (옵션 A) 또는 DX12SmokeHost 신설 (옵션 B)
   - M0-5: Engine.vcxproj L241-244 ExcludedFromBuild 4 config 보정
   - M0-6: Server.vcxproj alias 정합 — sln Build.0 제거 (옵션 a) 또는 ABI 호환 명시

2. M1 Device Bootstrap
   - M1-1: CDX12Device::Create 호출 + Bootstrap 로그 출력 (M0-4 의 호스트 로 검증)
   - M1-2: Adapter Description 출력 + Feature Level 12_0 (11_0 fallback 박제 제거)
   - M1-3: D3D12MA::Allocator 생성
   - M1-4: 3 queue 생성 + fence stress test

3. M2 SwapChain + Present
   - M2-1: 3 backbuffer + tearing support
   - M2-2: CommandAllocator/List/RTV heap 생성
   - M2-3: BeginFrame Clear EndFrame 1 프레임 정상
   - M2-4: fence 전역 monotonic 변경 + 1000 프레임 stress (DX12Queue.h/cpp 4 수정)
   - M2-5: 60 초 clear color 루프 + DXGI Validation 0 warning (Resize 검증 제외)
   - M2-6: Resize 옵션 → W10-13 이동, W7-9 범위 제외

4. M3 정상 종료
   - M3-1: Alt+F4 → WaitIdle < 100ms
   - M3-2: Live Object 0 leak
   - M3-3: PIX 캡처 (옵션)
```

---

## §7. 박제 함정 자체 점검 (v2 8 게이트 통과)

`.md/process/PLAN_AUTHORING_PITFALLS.md` 의 8 단계 관문:

```txt
A. §1 사전 결정 미박제 0      OK — 사고 ID D-1~D-12 모두 명시
B. PIMPL 본체 read           OK — DX12Device.h L29-109 + DX12Device.cpp L23-141, 242-300 직접 인용
                              + DX12SwapChain.h 전수 + DX12SwapChain.cpp L1-107 전수
                              + DX12Queue.h L1-37 전수 + DX12Queue.cpp L1-127 전수
C. Render path 전수          N/A — Bootstrap 단계, 시각 동일성 X
D. 인용 의미 검증             OK — Winters.sln L17-22, L26-51 + Engine.vcxproj L8-9, L65-66, L115, L132,
                              L241-244 + Server.vcxproj L147-156 + UpdateLib.bat L42-58 모두 직접 인용 블록
E. ECS Scheduler 동시성       N/A — RHI Bootstrap 단계, ECS 변경 0
F. Owner Scope 매트릭스       OK — CDX12Device = CEngineApp 전역 (PITFALLS §P-10 매트릭스 일치),
                              Server alias 동작 = M0-6 으로 명시 분리
G. API 실재 검증              OK — 모든 호출 API 가 헤더에 실재 (DX12Queue::Signal/WaitForFrame/Flush,
                              D3D12CreateDevice, DXGIGetDebugInterface1)
H. 인용 의미 + 행동 보존       OK — 인용 의미 일치 (UpdateLib.bat 의 "Debug/Release only" =
                              v2 박제 결론 "Debug-DX12 분기 누락"). DX11 동작 변경 0 (compile-only +
                              `#if WINTERS_RHI_BACKEND_DX12` 가드).
                              v2 추가 행동 검증: M2-4 fence 변경이 single-thread submit 동작 보존
                              (현재 DX11 도 single-thread, behavior preserving).
```

8 게이트 모두 통과. 박제 본문 진입 가능.

---

## §8. 외부 영향 분석

### 8-1. 영향 받는 파일

```txt
[v1 변경 대상 - 유지]
Engine/Private/Framework/CEngineApp.cpp            RHI device factory 에 DX12 분기 (M1-1, 박제됨)
Engine/Private/RHI/DX12/DX12Device.cpp             M3-2 Live Object reporter helper + destructor Reset/report 추가
Engine/Private/RHI/DX12/DX12SwapChain.cpp          변경 없음 (Resize stub 그대로, W10-13 변경)
Engine/Private/RHI/DX12/DX12Queue.cpp + .h         M2-4 fence 전역 monotonic 변경 (4 수정)
그 외 26 파일                                       변경 없음, 가드만 확인

[v2/v3 신규 변경 대상]
Engine/Include/Engine.vcxproj                      L241-244 AssetConverter main.cpp ExcludedFromBuild
                                                    Debug-DX12 / Release-DX12 추가 (M0-5)
Winters.sln                                        Server / Client / Tools 의 Debug-DX12 / Release-DX12 Build.0 6라인 제거 (M0-6)
UpdateLib.bat                                      L42-58 다음에 Debug-DX12 / Release-DX12 분기 추가 (M0-4 옵션 A)
                                                    또는 변경 없음 (M0-4 옵션 B = DX12SmokeHost 신설)
Tools/DX12SmokeHost/main.cpp + .vcxproj            [옵션 B 만] 신설 — WintersRun 기반 Engine.dll 단독 검증 호스트
Server/Include/Server.vcxproj                      변경 없음 (M0-6 옵션 a 적용 시), 또는 명시적 config (옵션 b)
```

### 8-2. 외부 호출 호환성

- `IRHIDevice` public API 불변. DX11/DX12 모두 동일 인터페이스
- `Get_RHIDevice() -> IRHIDevice*` 그대로
- Client/Server/Tools 코드 변경 0 (sln Build.0 제거 = 컴파일 자체 미실행)

### 8-3. 데스크탑 진입 시 주의 (v2 강화)

- VS (devenv.exe) 종료 후 빌드
- WintersGame.exe / WintersServer.exe / WintersAssetConverter.exe 모두 종료 (M0-3 빌드 시 lock 회피)
- `git checkout -b feature/rhi-dx12-bootstrap`
- M0 → M1 → M2 → M3 순차 진입. 각 단계 통과 후 다음.
- M0-4 (DLL 배포 경로) 가 M1 진입 전제 — 이거 없이는 DX12 device 가 절대 init 안 됨
- M0-5 / M0-6 은 빌드 정합성 — M1 진입 전 박제
- M2-3 진입 전 BeginFrame/EndFrame 본체 1회 더 read (현재 §4 M2-3 인용 부분 외 200줄 정도 더 있을 수 있음)
- D3D12MA cpp 컴파일 누락 시 vcxproj `<ItemGroup>` 직접 박제 절대 X (wrapper 패턴 보존)

---

## §9. W10-13 진입 조건 (참고, v2 갱신)

본 계획서 합격 후 `.md/plan/engine/2026-05-10_WEEK_10_13_DETAILED_BAKE.md` 진입.

W10-13 목표: 시각 동일성 (DX11 vs DX12 동일 mesh/material 렌더 결과 비교).

전제:
- 본 계획서 GATE 1~4 모두 통과 (M0~M3)
- M2-6 Resize 본체 박제 (v2 에서 W10-13 이동 명시)
- D3D_FEATURE_LEVEL 11_0 fallback 박제 (W10-13 이동, 호환성 확장)
- Track 1 W6 caller 정식 마이그 완료 (DX11 native bridge 제거)
- 셰이더 `register(... space0)` 명시 (11개 셰이더 전수)
- Track 3 Fiber 안정화 M0~M2 통과 (병렬 command list submit 안정성 — 단, M2-4 가 atomic 으로 변경되어야 함)
- UpdateLib.bat 옵션 A 박제 + Client.vcxproj Debug-DX12 config 신설

---

## §10. 진입 직전 체크리스트 (v3 강화)

```txt
[Stabilization]
- Stabilization-0 5 항목 완료
- 박제 함정 8 관문 통과 확인 (§7)
- devenv 종료 + WintersGame/WintersServer/WintersAssetConverter 종료
- git checkout -b feature/rhi-dx12-bootstrap

[v2/v3 신규 추가 - M0 진입 전]
- DLL 배포 경로 결정: 옵션 A (UpdateLib.bat 분기 + Client Debug-DX12 config) 또는
  옵션 B (DX12SmokeHost 신설). v3 결정 = 옵션 B (Tools/DX12SmokeHost + WintersRun)
- Engine.vcxproj L241-244 의 ExcludedFromBuild 4 config 박제 (M0-5)
- Server alias 정합 — Winters.sln 의 Server/Client/Tools Debug-DX12 / Release-DX12 Build.0 6라인 제거 (M0-6 옵션 a)
- 또는 Engine Debug vs Debug-DX12 export ABI diff 검증 (M0-6 옵션 c, dumpbin /EXPORTS)

[코드 read 강제 - M1/M2 진입 전]
- Engine/Private/RHI/DX12/DX12Device.cpp 100줄 이상 1회 더 read
  (특히 BeginFrame/EndFrame 외 CreateAllocator/CreateQueues/CreateFrameResources/
   CreateBackBufferViews 본체)
- Engine/Private/RHI/DX12/DX12SwapChain.cpp Initialize 본체 (§0-9 인용 외 부분)
- Engine/Private/RHI/DX12/DX12Queue.cpp Signal/WaitForFrame/Flush 본체 (M2-4 변경 대상,
  §4 M2-4 인용 재확인)
- Engine/Private/RHI/DX12/DX12Queue.h L29-33 멤버 (M2-4 변경 대상)
- Engine/Private/RHI/DX12/DX12Device.cpp destructor + M3-2 Live Object reporter 패치 위치 재확인
- Engine/Include/Engine.vcxproj 의 <ItemGroup><ClCompile> D3D12MemAlloc.cpp 등재 여부 확인 (X 가 정답)
- UpdateLib.bat L42-58 직접 read (옵션 A 박제 시)
- Server/Include/Server.vcxproj L147-156 직접 read (M0-6 옵션 c 시)

[합격 판정]
- M0-1: 0 errors + warnings <= 50 (Debug baseline 동등)
- M0-2: Debug|x64 빌드 영향 0
- M0-3: Debug-DX12 솔루션 빌드 = Engine only + Engine/Bin/Debug-DX12 산출물 생성 + 실행 파일 잠금 0
- M0-4: Tools/DX12SmokeHost.exe 또는 Client/Bin/Debug-DX12 에서 Engine DLL 자동 로드
- M0-5: Engine DLL 에 main.obj 미포함
- M0-6: Server link line 에 의도된 lib 만 (옵션 a = link 자체 없음)
- M1-1: [CDX12Device] Bootstrap device ready. 로그 가시
- M1-2: 12_0 device 생성 (hardware 또는 WARP)
- M1-3: D3D12MA Allocator nullptr 아님
- M1-4: 3 queue + fence stress
- M2-3: dark navy clear color + DXGI Validation 0
- M2-4: fence monotonic + 1000 frame stress
- M2-5: 60 초 frame loop (Resize 검증 제외)
- M3-1: WaitIdle < 100ms
- M3-2: Live Object 0
```

---

## §11. v1 → v3 변경 요약표 (참고)

| 영역 | v1 | v2/v3 | 본질 (P# / D#) |
|---|---|---|---|
| §0-3 | 28 파일 박제 상태 | 28 파일 + L241-244 인용 (P1-B 박제) | D-9 |
| §0-4 (신규) | — | UpdateLib.bat L42-58 인용 + 부재 분석 | D-8 |
| §0-7 (신규) | M1-2 추정 박제 | DX12Device.cpp L97-141 본체 인용 (12_0 only) | D-10 |
| §0-10 (신규) | — | DX12SwapChain.cpp L91-96 본체 인용 (Resize stub) | D-11 |
| §0-12 (신규) | — | Server.vcxproj L147-156 인용 + LinkLibraryDependencies 동작 분석 | D-12 |
| §1 | D-1~D-7 7건 | D-1~D-12 12건 (D-8~D-12 신규) | codex 3차 |
| §2 M0-1 | "0 errors, 0 warnings" | "0 errors + 50 warnings 이하 (Debug 동등)" | codex |
| §2 M0-3 | alias 정상 | v2: alias 정상 + UpdateLib race + lock 회피 / v3: Engine-only DX12 solution build | codex |
| §2 M0-4 (신규) | — | UpdateLib.bat Debug-DX12 분기 박제 또는 `Tools/DX12SmokeHost` | D-8 |
| §2 M0-5 (신규) | — | Engine.vcxproj L241-244 4 config ExcludedFromBuild | D-9 |
| §2 M0-6 (신규) | — | Server/Client/Tools Debug-DX12/Release-DX12 Build.0 6라인 제거 | D-12 |
| §3 M1-2 | "12_0, 11_0 fallback, WARP 비활성" | "12_0 only, WARP 활성, 11_0 fallback W10-13" | D-10 |
| §4 M2-5 | "Resize 정상" 검증 포함 | Resize 검증 제거 | D-11 |
| §4 M2-6 | "M2 합격 후 Resize 진입" | "W10-13 이동, W7-9 범위 제외" | D-11 |
| §6 진입 순서 | M0 3 항목 | M0 6 항목 (M0-4/5/6 추가) | codex |
| §10 체크리스트 | 7 항목 | 20+ 항목 (DLL 배포 + ABI + read 강제 추가) | codex |
| §2 M0-3 (v3) | alias fallback 검증 | Engine-only DX12 solution build + 실제 `Engine/Bin/Debug-DX12` 산출물 검증 | codex 4차 |
| §2 M0-4 (v3) | SmokeHost 위치/진입점 불명확 | `Tools/DX12SmokeHost` + `WintersRun` + 최소 `IWintersApp` 로 정정 | codex 4차 |
| §2 M0-6 (v3) | placeholder GUID + `.sln` 주석 예시 | 실제 Build.0 삭제 6라인 명시, ActiveCfg 보존 6라인 명시 | codex 4차 |
| §5 M3-2 (v3) | Live Object 합격 기준만 존재 | `DX12Device.cpp` helper + destructor Reset/report 코드 패치 박제 | codex 4차 |

---

**END OF DOCUMENT**
> 2026-05-25 update: this document is now historical for the old DX12 bootstrap. `DX12SmokeHost` was deleted; current DX12 work continues as a backend in `Engine/Private/RHI/DX12` and as `Debug-DX12` / `Release-DX12` Engine + Client configurations.
