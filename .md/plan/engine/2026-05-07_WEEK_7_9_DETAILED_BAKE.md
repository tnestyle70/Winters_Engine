# 2026-05-07 Week 7-9 상세 박제 — Track 2 RH-5 DX12 Backend (compile-only)

**작성일**: 2026-05-07
**상태**: 검토 대기 (계획서만 작성, 코드 변경은 codex 가 진행 / 작성자 후속 검토)
**전제**: Week 6 (RH-3 PSO/RenderPass/BindGroup + RH-4 64-bit handle + Track 1 IRHI 마이그) 완료
**상위 문서**: [Twin Track 계획서 §5.4](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md), [RHI 마스터 §2 RH-5](../rhi/00_RHI_MIGRATION_MASTER.md)

---

## 0. 한 줄

> **Week 7-9 = RH-5 DX12 backend bootstrap on top of the current RH-1 seed, not a direct "14-file drop" from a finished W6 baseline. Target shape stays `Engine/Private/RHI/DX12/` + Debug-DX12 config + `CDX12Device : public IRHIDevice`, but the compile-only acceptance is corrected to: DX12 config build passes and a DX12 bootstrap path can initialize, clear/present, and shut down with DX11-only UI/ImGui temporarily disabled. Full LoL scene parity is deferred to W10-13.**

> **Codebase reality correction (2026-05-02 review):**
> - `IRHIDevice` is still a seed with only `GetBackend()` and `GetNativeHandle()` in [Engine/Public/RHI/IRHIDevice.h](/C:/Users/tnest/Desktop/Winters_restored/Winters/Engine/Public/RHI/IRHIDevice.h:1).
> - `Engine/Public/RHI/` does not yet contain `IRHICommandList`, `IRHISwapChain`, `IRHIQueue`, `IRHIPipelineState`, `IRHIRenderPass`, `IRHIBindGroup`, `RHIDescriptors`, or `CRHIResourceTable`.
> - [CEngineApp.h](/C:/Users/tnest/Desktop/Winters_restored/Winters/Engine/Public/Framework/CEngineApp.h:1) still owns `unique_ptr<CDX11Device>` and public `DX11Shader` / `DX11Pipeline` objects, and [EngineConfig.h](/C:/Users/tnest/Desktop/Winters_restored/Winters/Engine/Include/EngineConfig.h:1) still has no backend field.
> - Eight runtime files still unwrap native DX11 handles through `GetNativeHandle(DX11Device/DX11DeviceContext)`: `UI_Manager.cpp`, `Texture.cpp`, `Model.cpp`, `Mesh.cpp`, `CMaterialPBR.cpp`, `ModelRenderer.cpp`, `PlaneRenderer.cpp`, `FxSystem.cpp`.
> - [ImGuiLayer.h](/C:/Users/tnest/Desktop/Winters_restored/Winters/Engine/Public/Editor/ImGuiLayer.h:1) and [UI_Manager.h](/C:/Users/tnest/Desktop/Winters_restored/Winters/Engine/Public/Manager/UI/UI_Manager.h:1) are still DX11-only in their public signatures.

> **Re-scoped order before original T2.W7.1~T2.W7.6:**
> 1. Add a backend selection seam in `EngineConfig` / `CEngineApp` / `main.cpp` (compile-time only is acceptable for W7, but the ownership boundary must stop assuming `CDX11Device` everywhere).
> 2. Add the missing bootstrap contracts required for compile-only DX12 (`IRHISwapChain`, `IRHIQueue`, and the minimum command submission surface actually needed by startup / clear / present).
> 3. Introduce a documented DX11-only exclusion path for ImGui/UI/gameplay systems so Debug-DX12 can boot without pretending those paths are backend-neutral yet.
> 4. Only after that gate passes should the original DX12 device/swapchain/queue/resource work start.
> - Treat the original Week 6 gate checklist further below as a historical target-shape, not as the current repo truth.

---

## 1. Week 6 결과 검증 (Week 7 진입 전)

```bash
# 1. Public 노출 0 + _Legacy 미존재
rg "ID3D11Device|d3d11\.h|RHI/DX11" Engine/Public/ Client/Public/ -l | wc -l    # 0
rg "Get_.*_Legacy" Engine Client | wc -l                                          # 0

# 2. RH-3 신규 인터페이스 4개
ls Engine/Public/RHI/{IRHIPipelineState,IRHIRenderPass,IRHIBindGroup,IRHIBindGroupLayout,CRHIResourceTable}.h

# 3. CMaterialPBR / CTexture / LightCullSystem / SSAOPass IRHI 통과
rg "ID3D11Device" Engine/Public/Renderer/ Engine/Public/Resource/ | wc -l         # 0

# 4. DXC 컴파일 (DXIL 컨테이너)
file Shaders/Mesh3D_PBR.cso   # DXIL ✓

# 5. 빌드 + 런타임 회귀 0 (이렐리아 PBR + Forward+ + SSAO Frame ≤20ms)
```

5개 모두 통과 시 Week 7 진입.

---

## 2. Week 7-9 작업 매트릭스 (3주 분할)

### 2.1 Week 7 — Foundation + Device

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T2.W7.1** | D3D12MA 외부 라이브러리 편입 | `Engine/ThirdPartyLib/D3D12MA/` | (W6) |
| **T2.W7.2** | Engine.sln 에 `Debug-DX12` / `Release-DX12` 컨피그 추가 | `Engine.sln`, `*.vcxproj` | (W6) |
| **T2.W7.3** | `Engine/Private/RHI/DX12/` 디렉토리 신설 + 컴파일 매크로 (`WINTERS_RHI_BACKEND_DX12`) | 신설 | T2.W7.2 |
| **T2.W7.4** | `CDX12Device.h` + `.cpp` 신설 (Initialize/Shutdown + 디바이스/팩토리/큐 생성) | `Engine/Private/RHI/DX12/DX12Device.{h,cpp}` | T2.W7.1, T2.W7.3 |
| **T2.W7.5** | `CDX12SwapChain.h` + `.cpp` 신설 (3 buffer + frame index) | `Engine/Private/RHI/DX12/DX12SwapChain.{h,cpp}` | T2.W7.4 |
| **T2.W7.6** | `CDX12Queue.h` + `.cpp` 신설 (Direct + Compute + Copy) | `Engine/Private/RHI/DX12/DX12Queue.{h,cpp}` | T2.W7.4 |

### 2.2 Week 8 — Resource + CommandList

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T2.W8.1** | `CDX12Buffer` (D3D12MA Allocation + state) | `DX12Buffer.{h,cpp}` | (W7) |
| **T2.W8.2** | `CDX12Texture` (D3D12MA Allocation + RTV/DSV/SRV/UAV 4 view 생성) | `DX12Texture.{h,cpp}` | T2.W8.1 |
| **T2.W8.3** | `CDX12Shader` (DXIL bytecode 보유 + reflection) | `DX12Shader.{h,cpp}` | (W7) |
| **T2.W8.4** | `CDX12Sampler` (descriptor heap slot 보유) | `DX12Sampler.{h,cpp}` | (W7) |
| **T2.W8.5** | `CDX12CommandList` (RH-2 IRHICommandList 구현) | `DX12CommandList.{h,cpp}` | T2.W8.1~T2.W8.4 |
| **T2.W8.6** | Descriptor Heap 관리 (CBV/SRV/UAV + RTV + DSV + Sampler 4 heap) | `DX12DescriptorHeap.{h,cpp}` | T2.W8.5 |
| **T2.W8.7** | Resource state transition (D3D12_RESOURCE_BARRIER) | `DX12ResourceBarrier.cpp` | T2.W8.5 |

### 2.3 Week 9 — PSO + RenderPass + BindGroup + 통합

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T2.W9.1** | Root Signature 빌더 (RH-3 BindGroupLayout → D3D12_ROOT_SIGNATURE_DESC1) | `DX12RootSignature.{h,cpp}` | (W8) |
| **T2.W9.2** | `CDX12PipelineState` (Graphics + Compute PSO) | `DX12PipelineState.{h,cpp}` | T2.W9.1 |
| **T2.W9.3** | `CDX12RenderPass` (D3D12 Render Pass — DX12 1.4+ 또는 임시 RTV/DSV bind) | `DX12RenderPass.{h,cpp}` | T2.W9.1 |
| **T2.W9.4** | `CDX12BindGroup` (descriptor table 채우기) | `DX12BindGroup.{h,cpp}` | T2.W9.1 |
| **T2.W9.5** | CDX12Device 의 11 IRHI 메서드 구현 완료 | `DX12Device.cpp` | 모두 |
| **T2.W9.6** | CEngineApp 의 backend 선택 (`#if defined(WINTERS_RHI_BACKEND_DX12)`) | `CEngineApp.{h,cpp}` | T2.W9.5 |
| **T2.W9.7** | DX12 컨피그 빌드 통과 + LoL exe 정상 종료 검증 | 빌드 | 모두 |

### 2.4 Track 1 (병행, 가벼움)

| 순서 | 작업 | 비고 |
|---|---|---|
| **T1.W7-9** | DX11 컨피그에서 회귀 검증 (이렐리아 PBR + Forward+ + SSAO Frame ≤20ms) | 코드 변경 0, 회귀 검증만 |

---

## 3. Week 7 — Foundation + Device

### 3.1 D3D12MA 편입 (T2.W7.1)

**라이브러리**: D3D12 Memory Allocator (`https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator`, MIT, AMD)

**편입 위치**: `Engine/ThirdPartyLib/D3D12MA/`

```
Engine/ThirdPartyLib/D3D12MA/
├── Inc/D3D12MemAlloc.h              (헤더 1개)
└── README.md (라이센스 표기)
```

> **Integration correction:** do not plan around a fictional `D3D12MA.lib`. Treat D3D12MA as vendor source integration for this repo: keep the header under `ThirdPartyLib`, compile the vendor implementation TU (or a thin local wrapper TU) inside the Engine project, and link only the platform libs (`d3d12.lib`, `dxgi.lib`, `dxguid.lib`).

**vcxproj 추가** (Engine.vcxproj — Debug-DX12 / Release-DX12 만):

```xml
<ItemGroup Condition="'$(Configuration)'=='Debug-DX12'">
  <ClCompile Include="..\Private\RHI\DX12\DX12MemoryAllocator.cpp" />
</ItemGroup>
```

`DX12MemoryAllocator.cpp` example:

```cpp
#include <D3D12MemAlloc.h>
```

Link settings stay on system libs only:

```xml
<AdditionalDependencies>d3d12.lib;dxgi.lib;dxguid.lib;%(AdditionalDependencies)</AdditionalDependencies>
```

**편입 절차**: `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 참조. README + 라이센스 명시 + vcpkg 미사용 정책 유지.

### 3.2 Engine.sln 컨피그 추가 (T2.W7.2)

**Debug-DX12 / Release-DX12 컨피그 추가**:

`Engine.vcxproj` (예시 PropertyGroup):

```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug-DX12|x64'" Label="Configuration">
  <ConfigurationType>DynamicLibrary</ConfigurationType>
  <UseDebugLibraries>true</UseDebugLibraries>
  <PlatformToolset>v143</PlatformToolset>
  <CharacterSet>Unicode</CharacterSet>
</PropertyGroup>

<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug-DX12|x64'">
  <ClCompile>
    <PreprocessorDefinitions>WINTERS_RHI_BACKEND_DX12;_DEBUG;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <FloatingPointModel>Precise</FloatingPointModel>
    <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
  </ClCompile>
  <Link>
    <AdditionalDependencies>d3d12.lib;dxgi.lib;dxguid.lib;%(AdditionalDependencies)</AdditionalDependencies>
  </Link>
</ItemDefinitionGroup>
```

**Engine.sln** 에 다음 추가:

```
GlobalSection(SolutionConfigurationPlatforms) = preSolution
    Debug|x64       = Debug|x64
    Release|x64     = Release|x64
    Debug-DX12|x64  = Debug-DX12|x64
    Release-DX12|x64 = Release-DX12|x64
EndGlobalSection
```

**Client / Server / Tools 도 동일 컨피그 추가** (Engine 만 DX12 빌드, Client 는 Engine.dll 참조 — symbol 호환).

### 3.3 디렉토리 + 컴파일 매크로 (T2.W7.3)

`Engine/Private/RHI/DX12/` 디렉토리 신설:

```
Engine/Private/RHI/DX12/
├── DX12Device.h, .cpp
├── DX12SwapChain.h, .cpp
├── DX12Queue.h, .cpp
├── DX12Buffer.h, .cpp                  (W8)
├── DX12Texture.h, .cpp                 (W8)
├── DX12Shader.h, .cpp                  (W8)
├── DX12Sampler.h, .cpp                 (W8)
├── DX12CommandList.h, .cpp             (W8)
├── DX12DescriptorHeap.h, .cpp          (W8)
├── DX12ResourceBarrier.cpp             (W8)
├── DX12RootSignature.h, .cpp           (W9)
├── DX12PipelineState.h, .cpp           (W9)
├── DX12RenderPass.h, .cpp              (W9)
└── DX12BindGroup.h, .cpp               (W9)
```

**모든 `.cpp` 가드**:

```cpp
#if defined(WINTERS_RHI_BACKEND_DX12)
// ...구현...
#endif
```

→ DX11 빌드 시 컴파일 0, DX12 빌드 시만 활성.

### 3.4 CDX12Device 신설 (T2.W7.4)

**파일**: `Engine/Private/RHI/DX12/DX12Device.h`

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_DX12)

#include "RHI/IRHIDevice.h"
#include "RHI/RHIHandles.h"
#include "RHI/CRHIResourceTable.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3D12MemAlloc.h>
#include <memory>

namespace Engine
{
    class CDX12SwapChain;
    class CDX12Queue;
    class CDX12CommandList;

    class WINTERS_ENGINE CDX12Device final : public IRHIDevice
    {
    public:
        ~CDX12Device();
        CDX12Device(const CDX12Device&) = delete;
        CDX12Device& operator=(const CDX12Device&) = delete;
        CDX12Device(CDX12Device&&) = default;
        CDX12Device& operator=(CDX12Device&&) = default;

        static std::unique_ptr<CDX12Device> Create();

        // ── IRHIDevice 구현 (W6 RH-3 인터페이스) ──
        RHIBufferHandle  CreateBuffer(const RHIBufferDesc& desc, const void* initData) override;
        RHITextureHandle CreateTexture(const RHITextureDesc& desc, const void* initData) override;
        RHIShaderHandle  CreateShader(const RHIShaderDesc& desc) override;
        RHISamplerHandle CreateSampler(const RHISamplerDesc& desc) override;

        void DestroyBuffer(RHIBufferHandle h) override;
        void DestroyTexture(RHITextureHandle h) override;
        void DestroyShader(RHIShaderHandle h) override;
        void DestroySampler(RHISamplerHandle h) override;

        void UpdateBuffer(RHIBufferHandle h, const void* data, size_t sizeBytes) override;

        IRHISwapChain* CreateSwapChain(const RHIWindowHandle& window) override;
        IRHIQueue*     GetGraphicsQueue() override;

        RHIPipelineHandle    CreatePipeline(const RHIPipelineDesc& desc) override;
        void                 DestroyPipeline(RHIPipelineHandle h) override;
        RHIRenderPassHandle  CreateRenderPass(const RHIRenderPassDesc& desc) override;
        void                 DestroyRenderPass(RHIRenderPassHandle h) override;
        RHIBindGroupHandle   CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc) override;
        RHIBindGroupHandle   CreateBindGroup(const RHIBindGroupDesc& desc) override;
        void                 DestroyBindGroup(RHIBindGroupHandle h) override;
        void UpdateBindGroup(RHIBindGroupHandle h,
                             const RHIBindGroupResource* resources,
                             u32_t resourceCount) override;

        void* GetNativeHandle(eRHINativeType type) override;
        void  BeginFrame() override;
        void  EndFrame() override;

        // ── DX12 native (Engine 내부 전용) ──
        ID3D12Device*  GetD3D12Device()  const { return m_pDevice.Get(); }
        IDXGIFactory6* GetDXGIFactory()  const { return m_pFactory.Get(); }
        D3D12MA::Allocator* GetAllocator() const { return m_pAllocator; }

    private:
        CDX12Device();

        // DXGI / Device
        Microsoft::WRL::ComPtr<IDXGIFactory6> m_pFactory;
        Microsoft::WRL::ComPtr<IDXGIAdapter4> m_pAdapter;
        Microsoft::WRL::ComPtr<ID3D12Device>  m_pDevice;

        // Memory Allocator
        D3D12MA::Allocator* m_pAllocator = nullptr;

        // Queue (Direct/Compute/Copy)
        std::unique_ptr<CDX12Queue> m_pGraphicsQueue;
        std::unique_ptr<CDX12Queue> m_pComputeQueue;
        std::unique_ptr<CDX12Queue> m_pCopyQueue;

        // Frame management
        u32_t m_FrameIndex = 0;
        static constexpr u32_t kFramesInFlight = 3;

        // Resource tables
        CRHIResourceTable<class CDX12BufferImpl,        BufferTag>     m_BufferTable;
        CRHIResourceTable<class CDX12TextureImpl,       TextureTag>    m_TextureTable;
        CRHIResourceTable<class CDX12ShaderImpl,        ShaderTag>     m_ShaderTable;
        CRHIResourceTable<class CDX12SamplerImpl,       SamplerTag>    m_SamplerTable;
        CRHIResourceTable<class CDX12PipelineStateImpl, PipelineTag>   m_PipelineTable;
        CRHIResourceTable<class CDX12RenderPassImpl,    RenderPassTag> m_RenderPassTable;
        CRHIResourceTable<class CDX12BindGroupImpl,     BindGroupTag>  m_BindGroupTable;
    };
}

#endif // WINTERS_RHI_BACKEND_DX12
```

**`.cpp` 핵심 (Initialize)**:

```cpp
#if defined(WINTERS_RHI_BACKEND_DX12)

#include "DX12Device.h"
#include "DX12SwapChain.h"
#include "DX12Queue.h"

namespace Engine
{
    std::unique_ptr<CDX12Device> CDX12Device::Create()
    {
        auto p = std::unique_ptr<CDX12Device>(new CDX12Device());

        // 1. DXGI Factory
        UINT factoryFlags = 0;
    #if defined(_DEBUG)
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        Microsoft::WRL::ComPtr<ID3D12Debug> pDebug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
            pDebug->EnableDebugLayer();
    #endif
        if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&p->m_pFactory))))
            return nullptr;

        // 2. Adapter (DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE)
        if (FAILED(p->m_pFactory->EnumAdapterByGpuPreference(
            0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&p->m_pAdapter))))
            return nullptr;

        // 3. Device (FL 12_0 minimum)
        if (FAILED(D3D12CreateDevice(p->m_pAdapter.Get(),
                                     D3D_FEATURE_LEVEL_12_0,
                                     IID_PPV_ARGS(&p->m_pDevice))))
            return nullptr;

        // 4. D3D12MA Allocator
        D3D12MA::ALLOCATOR_DESC desc{};
        desc.pDevice = p->m_pDevice.Get();
        desc.pAdapter = p->m_pAdapter.Get();
        if (FAILED(D3D12MA::CreateAllocator(&desc, &p->m_pAllocator)))
            return nullptr;

        // 5. Queue 3종
        p->m_pGraphicsQueue = CDX12Queue::Create(p->m_pDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
        p->m_pComputeQueue  = CDX12Queue::Create(p->m_pDevice.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE);
        p->m_pCopyQueue     = CDX12Queue::Create(p->m_pDevice.Get(), D3D12_COMMAND_LIST_TYPE_COPY);

        return p;
    }

    CDX12Device::~CDX12Device()
    {
        if (m_pAllocator)
        {
            m_pAllocator->Release();
            m_pAllocator = nullptr;
        }
    }

    void* CDX12Device::GetNativeHandle(eRHINativeType type)
    {
        switch (type)
        {
        case eRHINativeType::DX12Device:       return m_pDevice.Get();
        case eRHINativeType::DX12CommandQueue: return m_pGraphicsQueue ? m_pGraphicsQueue->GetQueue() : nullptr;
        default: return nullptr;
        }
    }

    void CDX12Device::BeginFrame()
    {
        m_FrameIndex = (m_FrameIndex + 1) % kFramesInFlight;
        // CPU/GPU 동기화 (per-frame fence)
        m_pGraphicsQueue->WaitForFrame(m_FrameIndex);
    }

    void CDX12Device::EndFrame()
    {
        m_pGraphicsQueue->Signal(m_FrameIndex);
    }
}

#endif // WINTERS_RHI_BACKEND_DX12
```

### 3.5 CDX12SwapChain (T2.W7.5)

**파일**: `Engine/Private/RHI/DX12/DX12SwapChain.h`

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_DX12)

#include "RHI/IRHISwapChain.h"
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>

namespace Engine
{
    class WINTERS_ENGINE CDX12SwapChain final : public IRHISwapChain
    {
    public:
        ~CDX12SwapChain();
        static std::unique_ptr<CDX12SwapChain> Create(class CDX12Device* pDevice,
                                                      const RHIWindowHandle& window);

        void Present(bool_t bVSync) override;
        u32_t GetCurrentBackBufferIndex() const override { return m_BackBufferIndex; }
        RHITextureHandle GetCurrentBackBuffer() override;
        void Resize(u32_t w, u32_t h) override;
        void* GetNativeHandle(eRHINativeType type) override;

    private:
        CDX12SwapChain();
        Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;
        Microsoft::WRL::ComPtr<ID3D12Resource>  m_pBackBuffers[3];
        RHITextureHandle m_hBackBuffers[3]{};
        u32_t m_BackBufferIndex = 0;
        u32_t m_Width = 0, m_Height = 0;
        bool_t m_bTearingSupported = false;
    };
}

#endif
```

`.cpp` Create 핵심:

```cpp
std::unique_ptr<CDX12SwapChain> CDX12SwapChain::Create(CDX12Device* pDevice, const RHIWindowHandle& window)
{
    auto p = std::unique_ptr<CDX12SwapChain>(new CDX12SwapChain());
    p->m_Width  = window.width;
    p->m_Height = window.height;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width  = window.width;
    desc.Height = window.height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // sRGB 는 RTV 측에서 따로
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 3;
    desc.SampleDesc.Count = 1;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwap1;
    if (FAILED(pDevice->GetDXGIFactory()->CreateSwapChainForHwnd(
        pDevice->GetGraphicsQueue()->GetQueue(),
        (HWND)window.nativeWindow,
        &desc, nullptr, nullptr, &pSwap1)))
        return nullptr;

    pSwap1.As(&p->m_pSwapChain);

    // 3 backbuffer 획득 + RHITextureHandle 등록
    for (u32_t i = 0; i < 3; ++i)
    {
        p->m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&p->m_pBackBuffers[i]));
        // Texture table 에 등록 (RH-4 handle)
        // p->m_hBackBuffers[i] = pDevice->RegisterBackBuffer(p->m_pBackBuffers[i].Get());
    }

    return p;
}

void CDX12SwapChain::Present(bool_t bVSync)
{
    UINT syncInterval = bVSync ? 1 : 0;
    UINT flags = (m_bTearingSupported && !bVSync) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    m_pSwapChain->Present(syncInterval, flags);
    m_BackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}
```

### 3.6 CDX12Queue (T2.W7.6)

**파일**: `Engine/Private/RHI/DX12/DX12Queue.h`

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_DX12)

#include "RHI/IRHIQueue.h"
#include <wrl/client.h>
#include <d3d12.h>

namespace Engine
{
    class WINTERS_ENGINE CDX12Queue final : public IRHIQueue
    {
    public:
        ~CDX12Queue();
        static std::unique_ptr<CDX12Queue> Create(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type);

        ID3D12CommandQueue* GetQueue() const { return m_pQueue.Get(); }

        // Frame fence
        void Signal(u32_t frameIndex);
        void WaitForFrame(u32_t frameIndex);
        void Flush();

        void* GetNativeHandle(eRHINativeType type) override;

    private:
        CDX12Queue();
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pQueue;
        Microsoft::WRL::ComPtr<ID3D12Fence>        m_pFence;
        u64_t  m_FenceValues[3] = { 0, 0, 0 };
        HANDLE m_hFenceEvent = nullptr;
        D3D12_COMMAND_LIST_TYPE m_Type;
    };
}

#endif
```

---

## 4. Week 8 — Resource + CommandList

### 4.1 CDX12Buffer (T2.W8.1)

**파일**: `Engine/Private/RHI/DX12/DX12Buffer.h`

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_DX12)

#include "RHI/IRHIBuffer.h"
#include <D3D12MemAlloc.h>
#include <wrl/client.h>

namespace Engine
{
    // Note: IRHIBuffer 는 인터페이스. CDX12BufferImpl 은 ResourceTable 의 실 객체 (handle 별 매핑).
    class CDX12BufferImpl
    {
    public:
        CDX12BufferImpl() = default;
        ~CDX12BufferImpl();

        bool_t Initialize(D3D12MA::Allocator* pAlloc, const RHIBufferDesc& desc, const void* initData);

        ID3D12Resource* GetResource() const { return m_pResource.Get(); }
        D3D12_RESOURCE_STATES GetState() const { return m_State; }
        void SetState(D3D12_RESOURCE_STATES s) { m_State = s; }

        const RHIBufferDesc& GetDesc() const { return m_Desc; }

    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
        D3D12MA::Allocation*                    m_pAllocation = nullptr;
        D3D12_RESOURCE_STATES                   m_State = D3D12_RESOURCE_STATE_COMMON;
        RHIBufferDesc                           m_Desc{};
    };
}

#endif
```

`.cpp` Initialize 핵심:

```cpp
bool_t CDX12BufferImpl::Initialize(D3D12MA::Allocator* pAlloc, const RHIBufferDesc& desc, const void* initData)
{
    m_Desc = desc;

    D3D12_RESOURCE_DESC rdesc{};
    rdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rdesc.Width  = desc.sizeBytes;
    rdesc.Height = 1;
    rdesc.DepthOrArraySize = 1;
    rdesc.MipLevels = 1;
    rdesc.SampleDesc.Count = 1;
    rdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12MA::ALLOCATION_DESC adesc{};
    adesc.HeapType = desc.dynamic ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_STATES initState = desc.dynamic
        ? D3D12_RESOURCE_STATE_GENERIC_READ
        : D3D12_RESOURCE_STATE_COPY_DEST;

    if (FAILED(pAlloc->CreateResource(&adesc, &rdesc, initState, nullptr,
                                       &m_pAllocation, IID_PPV_ARGS(&m_pResource))))
        return false;

    m_State = initState;

    // upload heap 이면 즉시 Map + memcpy (dynamic)
    if (desc.dynamic && initData)
    {
        void* pMapped = nullptr;
        D3D12_RANGE empty{ 0, 0 };
        m_pResource->Map(0, &empty, &pMapped);
        memcpy(pMapped, initData, desc.sizeBytes);
        m_pResource->Unmap(0, nullptr);
    }

    // default heap 이면 staging upload buffer 통해 복사 (별도 CopyQueue)
    // (T2.W8.5 CDX12CommandList 에서 처리)

    return true;
}
```

### 4.2 CDX12Texture / Shader / Sampler (T2.W8.2~T2.W8.4)

**CDX12TextureImpl**: 동일 패턴 + 4 view (RTV/DSV/SRV/UAV) descriptor heap slot 보유.
**CDX12ShaderImpl**: DXIL bytecode 보유 (`std::vector<u8_t> m_Bytecode`).
**CDX12SamplerImpl**: descriptor heap slot 1개 (Sampler heap).

각 Impl 은 별도 `.h/.cpp` 박제 (W8 진입 시 codex 가 동일 패턴으로 신설).

### 4.3 CDX12CommandList (T2.W8.5)

**파일**: `Engine/Private/RHI/DX12/DX12CommandList.h`

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_DX12)

#include "RHI/IRHICommandList.h"      // W6 RH-3 신설
#include <wrl/client.h>
#include <d3d12.h>

namespace Engine
{
    class CDX12Device;

    class WINTERS_ENGINE CDX12CommandList final : public IRHICommandList
    {
    public:
        ~CDX12CommandList();
        static std::unique_ptr<CDX12CommandList> Create(CDX12Device* pDevice, D3D12_COMMAND_LIST_TYPE type);

        // ── IRHICommandList 구현 ──
        void Begin() override;
        void End()   override;

        void BeginRenderPass(RHIRenderPassHandle h) override;
        void EndRenderPass() override;

        void SetPipeline(RHIPipelineHandle h) override;
        void SetBindGroup(u32_t slot, RHIBindGroupHandle h) override;

        void SetVertexBuffer(u32_t slot, RHIBufferHandle h, u32_t stride, u32_t offset) override;
        void SetIndexBuffer(RHIBufferHandle h, u32_t offset, eRHIFormat indexFormat) override;

        void Draw(u32_t vertexCount, u32_t instanceCount, u32_t firstVertex, u32_t firstInstance) override;
        void DrawIndexed(u32_t indexCount, u32_t instanceCount, u32_t firstIndex, i32_t baseVertex, u32_t firstInstance) override;

        void Dispatch(u32_t x, u32_t y, u32_t z) override;

        void UpdateBuffer(RHIBufferHandle h, const void* data, size_t sizeBytes) override;

        void TransitionResource(RHIBufferHandle h, eRHIResourceState newState) override;
        void TransitionResource(RHITextureHandle h, eRHIResourceState newState) override;

        ID3D12GraphicsCommandList* GetNativeCmdList() const { return m_pCmdList.Get(); }

    private:
        CDX12CommandList();
        CDX12Device* m_pOwnerDevice = nullptr;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_pAllocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCmdList;
        D3D12_COMMAND_LIST_TYPE m_Type;
    };
}

#endif
```

### 4.4 Descriptor Heap 관리 (T2.W8.6)

DX12 의 4 heap 종류 (`CBV_SRV_UAV` / `RTV` / `DSV` / `Sampler`) 관리. CBV_SRV_UAV heap 은 shader-visible (descriptor table 바인딩 시), RTV/DSV 는 non-shader-visible.

**파일**: `Engine/Private/RHI/DX12/DX12DescriptorHeap.h`

```cpp
class WINTERS_ENGINE CDX12DescriptorHeap
{
public:
    static std::unique_ptr<CDX12DescriptorHeap> Create(ID3D12Device* pDevice,
                                                       D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                       u32_t numDescriptors,
                                                       bool_t shaderVisible);

    u32_t Allocate();   // free slot 반환
    void  Free(u32_t slot);

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(u32_t slot) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(u32_t slot) const;
    ID3D12DescriptorHeap*       GetNative() const { return m_pHeap.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pHeap;
    u32_t m_DescriptorSize = 0;
    u32_t m_NumDescriptors = 0;
    std::vector<u32_t> m_FreeList;
    bool_t m_bShaderVisible = false;
};
```

### 4.5 Resource Barrier (T2.W8.7)

DX11 (implicit) → DX12 (explicit) 변환 매핑.

```cpp
namespace Engine::DX12
{
    inline D3D12_RESOURCE_STATES ToD3D12State(eRHIResourceState s)
    {
        switch (s)
        {
        case eRHIResourceState::Common:        return D3D12_RESOURCE_STATE_COMMON;
        case eRHIResourceState::VertexConstant:return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        case eRHIResourceState::IndexBuffer:   return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case eRHIResourceState::RenderTarget:  return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case eRHIResourceState::DepthRead:     return D3D12_RESOURCE_STATE_DEPTH_READ;
        case eRHIResourceState::DepthWrite:    return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case eRHIResourceState::ShaderResource:return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                                                    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        case eRHIResourceState::UAV:           return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        case eRHIResourceState::CopyDest:      return D3D12_RESOURCE_STATE_COPY_DEST;
        case eRHIResourceState::CopySource:    return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case eRHIResourceState::Present:       return D3D12_RESOURCE_STATE_PRESENT;
        }
        return D3D12_RESOURCE_STATE_COMMON;
    }

    inline void TransitionBarrier(ID3D12GraphicsCommandList* pCmd,
                                  ID3D12Resource* pResource,
                                  D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after)
    {
        if (before == after) return;

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = pResource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter  = after;

        pCmd->ResourceBarrier(1, &barrier);
    }
}
```

---

## 5. Week 9 — PSO + RenderPass + BindGroup + 통합

### 5.1 Root Signature (T2.W9.1)

DX12 의 root signature 는 Vulkan descriptor set 의 사촌. RH-3 BindGroupLayout 에서 자동 변환:

```cpp
class CDX12RootSignatureBuilder
{
public:
    static Microsoft::WRL::ComPtr<ID3D12RootSignature>
        Build(ID3D12Device* pDevice, const RHIBindGroupLayoutDesc& desc);
};
```

`.cpp` 핵심:

```cpp
Microsoft::WRL::ComPtr<ID3D12RootSignature>
CDX12RootSignatureBuilder::Build(ID3D12Device* pDevice, const RHIBindGroupLayoutDesc& desc)
{
    std::vector<D3D12_ROOT_PARAMETER1>   params;
    std::vector<D3D12_DESCRIPTOR_RANGE1> ranges;
    ranges.reserve(desc.slotCount);

    for (u32_t i = 0; i < desc.slotCount; ++i)
    {
        const auto& slot = desc.slots[i];
        D3D12_DESCRIPTOR_RANGE1 range{};
        switch (slot.type)
        {
        case eRHIBindingType::ConstantBuffer:  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
        case eRHIBindingType::ShaderResource:  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
        case eRHIBindingType::UnorderedAccess: range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
        case eRHIBindingType::Sampler:         range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
        }
        range.NumDescriptors = 1;
        range.BaseShaderRegister = slot.slot;
        range.RegisterSpace = 0;
        range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
        ranges.push_back(range);

        D3D12_ROOT_PARAMETER1 rp{};
        rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rp.DescriptorTable.NumDescriptorRanges = 1;
        rp.DescriptorTable.pDescriptorRanges = &ranges.back();
        rp.ShaderVisibility = (D3D12_SHADER_VISIBILITY)slot.visibility;
        params.push_back(rp);
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC vd{};
    vd.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    vd.Desc_1_1.NumParameters = (UINT)params.size();
    vd.Desc_1_1.pParameters   = params.data();
    vd.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> pBlob, pError;
    if (FAILED(D3D12SerializeVersionedRootSignature(&vd, &pBlob, &pError)))
        return nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> pRS;
    pDevice->CreateRootSignature(0, pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
                                  IID_PPV_ARGS(&pRS));
    return pRS;
}
```

### 5.2 CDX12PipelineState (T2.W9.2)

```cpp
class CDX12PipelineStateImpl
{
public:
    bool_t Initialize(ID3D12Device* pDevice, const RHIPipelineDesc& desc,
                      ID3D12RootSignature* pRS);

    ID3D12PipelineState* GetPSO() const { return m_pPSO.Get(); }
    ID3D12RootSignature* GetRS()  const { return m_pRS;       }

private:
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPSO;
    ID3D12RootSignature* m_pRS = nullptr;
};
```

`.cpp`:

```cpp
bool_t CDX12PipelineStateImpl::Initialize(ID3D12Device* pDevice, const RHIPipelineDesc& desc,
                                           ID3D12RootSignature* pRS)
{
    m_pRS = pRS;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = pRS;

    // VS / PS bytecode
    auto* pVS = /* Lookup(desc.vsHandle) */;
    auto* pPS = /* Lookup(desc.psHandle) */;
    psoDesc.VS = { pVS->bytecode.data(), pVS->bytecode.size() };
    psoDesc.PS = { pPS->bytecode.data(), pPS->bytecode.size() };

    // Input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> ies;
    for (u32_t i = 0; i < desc.inputElementCount; ++i)
    {
        const auto& src = desc.inputElements[i];
        D3D12_INPUT_ELEMENT_DESC d{};
        d.SemanticName     = src.semanticName;
        d.SemanticIndex    = src.semanticIndex;
        d.Format           = ToDXGIFormat(src.format);
        d.AlignedByteOffset= src.alignedByteOffset;
        d.InputSlot        = src.inputSlot;
        d.InputSlotClass   = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        ies.push_back(d);
    }
    psoDesc.InputLayout = { ies.data(), (UINT)ies.size() };

    // Rasterizer
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = ToD3D12CullMode(desc.cullMode);

    // Depth
    psoDesc.DepthStencilState.DepthEnable = (desc.depthOp != eRHIDepthOp::Always);
    psoDesc.DepthStencilState.DepthWriteMask = desc.depthWrite
        ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = ToD3D12DepthOp(desc.depthOp);

    // Blend
    auto& bs = psoDesc.BlendState.RenderTarget[0];
    switch (desc.blendMode)
    {
    case eRHIBlendMode::AlphaBlend:
        bs.BlendEnable = TRUE;
        bs.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        bs.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        bs.BlendOp  = D3D12_BLEND_OP_ADD;
        break;
    // ... 등
    }

    // RTV / DSV format
    for (u32_t i = 0; i < desc.rtvCount; ++i)
        psoDesc.RTVFormats[i] = ToDXGIFormat(desc.rtvFormats[i]);
    psoDesc.NumRenderTargets = desc.rtvCount;
    psoDesc.DSVFormat = ToDXGIFormat(desc.dsvFormat);
    psoDesc.SampleDesc.Count = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    return SUCCEEDED(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPSO)));
}
```

### 5.3 CEngineApp backend 선택 (T2.W9.6)

```cpp
// CEngineApp.cpp

bool CEngineApp::OnInit()
{
#if defined(WINTERS_RHI_BACKEND_DX12)
    m_pDevice = CDX12Device::Create();
#else
    m_pDevice = CDX11Device::Create(...);
#endif

    if (!m_pDevice) return false;
    // ... 이하 동일 (IRHIDevice 통과로 backend-agnostic)
}
```

### 5.4 합격 게이트 (Track 2 W7-9)

- ✅ `Engine/Private/RHI/DX12/` 14 파일 존재
- ✅ Engine.sln 의 `Debug-DX12 / Release-DX12` 컨피그 빌드 통과
- ✅ Client / Server / Tools 도 DX12 컨피그 컴파일 (Engine.dll 참조 호환)
- ✅ DX12 컨피그 LoL exe 실행 → CDX12Device::Create 성공 + main loop 진입 + 정상 종료
- ✅ 시각 검증 X (W10-13 visual parity 단계)
- ✅ DX11 컨피그 회귀 0

---

## 6. Track 1 — 안정화 (W7-9 병행)

| 검증 | 명령 |
|---|---|
| DX11 회귀 (이렐리아 PBR + Forward+ + SSAO Frame ≤20ms) | DX11 컨피그 LoL 실행 |
| 챔프 7명 PBR 모두 동작 | InGame 진입 |
| 코드 변경 0 | T1 작업 없음 |

---

## 7. 위험 시나리오

### 7.1 R-W7-1: D3D12MA vendor integration path 불명확
- 시나리오: Debug-DX12 컨피그에서 `D3D12MA.lib` 전제를 그대로 따르다가 link 단계에서 구조가 맞지 않음
- 완화: ① D3D12MA 는 repo 기준 vendor source integration 으로 고정 ② Engine project 에 implementation TU 1개만 추가 ③ link 는 system libs (`d3d12.lib`, `dxgi.lib`, `dxguid.lib`) 로 제한

### 7.2 R-W7-2: DX12 디바이스 생성 실패 (DX12 미지원 GPU)
- 시나리오: D3D12CreateDevice FL 12_0 실패 → CDX12Device::Create nullptr 반환 → CEngineApp::OnInit fail
- 완화: ① FL 11_0 fallback 시도 ② 또는 GUI 메시지 박스 "DX12 미지원" + 정상 종료 ③ Adapter enum 시 software adapter (WARP) fallback 옵션

### 7.3 R-W7-3: PSO compile 100ms+ stutter
- 시나리오: 첫 frame 에 100개 PSO 컴파일 = 10초 stutter
- 완화: ① 디스크 PSO 캐시 (W10-13 T2.5 PSO library) ② 또는 startup 시 사전 PSO 컴파일 (loading screen)

### 7.4 R-W7-4: Backbuffer state 초기 transition 누락
- 시나리오: SwapChain backbuffer 초기 상태 = COMMON → RenderTarget 으로 transition 안 하면 PIX validation error
- 완화: BeginRenderPass 안에서 자동 transition (W9 T2.W9.3 RenderPass 책임)

### 7.5 R-W7-5: Descriptor heap shader-visible vs non-visible 혼동
- 시나리오: RTV heap 을 shader-visible 로 만들면 D3D12 validation error
- 완화: DX12DescriptorHeap::Create 의 `bool_t shaderVisible` 인자 + `D3D12_DESCRIPTOR_HEAP_TYPE_RTV/DSV` 는 자동 false

### 7.6 R-W7-6: DXGI Factory tearing flag 미지원 모니터
- 시나리오: VRR 미지원 모니터에서 ALLOW_TEARING flag → Present 실패
- 완화: `IDXGIFactory5::CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING)` 사전 체크

---

## 8. Week 7-9 통합 합격 검증

```bash
# 1. DX12 디렉토리 + 14 파일
ls Engine/Private/RHI/DX12/{DX12Device,DX12SwapChain,DX12Queue,DX12Buffer,DX12Texture,DX12Shader,DX12Sampler,DX12CommandList,DX12DescriptorHeap,DX12RootSignature,DX12PipelineState,DX12RenderPass,DX12BindGroup}.{h,cpp}

# 2. Engine.sln 컨피그 + 빌드 통과
MSBuild Engine.vcxproj /p:Configuration=Debug-DX12 /p:Platform=x64
MSBuild Engine.vcxproj /p:Configuration=Release-DX12 /p:Platform=x64

# 3. Client / Server / Tools DX12 빌드 (Engine.dll 호환 검증)
MSBuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64

# 4. DX11 컨피그 회귀
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64
# 기대: 회귀 0

# 5. DX12 LoL exe 실행 검증 (W7-9 합격은 정상 종료까지)
WintersGame.exe   # DX12 컨피그
# 기대:
#   - DX12Device::Create 성공 로그
#   - Main loop 진입 (1 frame 이상)
#   - ESC 또는 X 클릭으로 정상 종료
#   - 시각 결과 차이는 W10-13 에서 처리
```

---

## 9. 부록 A — Week 7-9 진입 체크리스트

```
[ ] Week 6 결과 검증 (Public 0 + _Legacy 0 + RH-3 인터페이스 4개 + DXC)
[ ] Visual Studio 종료 + git: feature/2026-05-07-week7-rh5 branch
[ ] DX12 SDK 확인 (Windows SDK 10.0.20348+ 필요)

Week 7 — Foundation:
[ ] §3.1 D3D12MA 편입 (ThirdPartyLib/D3D12MA/)
[ ] §3.2 Engine.sln Debug-DX12 / Release-DX12 컨피그 추가
[ ] §3.3 Engine/Private/RHI/DX12/ 디렉토리 + WINTERS_RHI_BACKEND_DX12 매크로
[ ] §3.4 CDX12Device.h+.cpp 신설 (Initialize/Adapter/Device/Allocator)
[ ] §3.5 CDX12SwapChain.h+.cpp 신설 (3 buffer + frame index)
[ ] §3.6 CDX12Queue.h+.cpp 신설 (Direct/Compute/Copy + fence)

Week 8 — Resource + CommandList:
[ ] §4.1 CDX12BufferImpl (D3D12MA Allocation)
[ ] §4.2 CDX12TextureImpl + 4 view
[ ] CDX12ShaderImpl (DXIL bytecode)
[ ] CDX12SamplerImpl
[ ] §4.3 CDX12CommandList (IRHICommandList 구현)
[ ] §4.4 DX12DescriptorHeap (CBV_SRV_UAV / RTV / DSV / Sampler 4 heap)
[ ] §4.5 ResourceBarrier 헬퍼 (eRHIResourceState → D3D12_RESOURCE_STATES)

Week 9 — PSO + RenderPass + BindGroup + 통합:
[ ] §5.1 RootSignatureBuilder (BindGroupLayout → root signature)
[ ] §5.2 CDX12PipelineStateImpl (Graphics + Compute PSO)
[ ] CDX12RenderPassImpl
[ ] CDX12BindGroupImpl (descriptor table 채우기)
[ ] §5.3 CEngineApp backend 선택 매크로 (#if WINTERS_RHI_BACKEND_DX12)
[ ] CDX12Device 의 11 IRHI 메서드 구현 완료

검증:
[ ] §8 빌드 통과 (DX11/DX12 양쪽)
[ ] DX12 LoL exe 정상 종료
[ ] 시각 검증은 W10-13 으로 이연
```

---

## 10. 한 줄

> **Week 7-9 = backend-selection seam + DX11-only exclusion gate + D3D12MA vendor integration + `Engine/Private/RHI/DX12/` bootstrap. Original 14-file DX12 target shape remains valid, but the acceptance is corrected to DX12 build + bootstrap init/clear/present/shutdown + DX11 regression 0. Full LoL scene parity and UI/ImGui parity move to W10-13.**

---

## 끝.
