# Phase RH-6 Sub-plan: Vulkan Backend

**작성일**: 2026-04-30 (Codex 2차 검토 보정 2026-04-30)
**상위 문서**: `00_RHI_MIGRATION_MASTER.md` §2
**범위**: `Engine/Private/RHI/Vulkan/*` 전체 + DXC HLSL→SPIR-V 파이프라인 + Winters.sln Debug-VK / Release-VK 컨피그 + **VMA 외부 라이브러리 편입**
**합격**: **compile-only 합격 (4~6주) + visual parity 합격 (3~4주 추가)** — 동일 LoL 빌드가 VK 컨피그에서 시각 결과 동일, validation layer 0 error/warning

**한 줄**: **★ Codex 2차 보정 — compile-only / visual parity 분리 + VMA 외부 라이브러리 편입 + validation layer availability 사전 체크.**

---

## ★ Codex 2차 검토 변경 요약

| 변경 | 이전 RH-6 (1차) | 신규 RH-6 (2차) |
|---|---|---|
| 합격 단계 | 단일 | **compile-only (4~6주) + visual parity (3~4주)** (P2-21) |
| VMA | 단순 mention | **ThirdPartyLib 편입 계획 명시** (P1-20) |
| Validation layer | 단순 활성 | **`vkEnumerateInstanceLayerProperties` 사전 체크 + fallback** (P2-29) |

---

## 1. 신규 파일 목록

```
Engine/Private/RHI/Vulkan/
├── VulkanDevice.h / .cpp           (CVulkanDevice : public IRHIDevice)
├── VulkanInstance.h / .cpp         (★ VK 특유 — VkInstance 별도 wrapping)
├── VulkanSwapChain.h / .cpp        (CVulkanSwapChain : public IRHISwapChain)
├── VulkanQueue.h / .cpp            (CVulkanQueue : public IRHIQueue)
├── VulkanCommandList.h / .cpp      (CVulkanCommandList : public IRHICommandList)
├── VulkanCommandPool.h / .cpp      (★ VK 특유 — VkCommandPool per-thread)
├── VulkanBuffer.h / .cpp
├── VulkanTexture.h / .cpp          (VkImage + VkImageView)
├── VulkanShader.h / .cpp           (SPIR-V loader)
├── VulkanSampler.h / .cpp
├── VulkanPipelineState.h / .cpp    (CVulkanPipelineState : public IRHIPipelineState)
├── VulkanRenderPass.h / .cpp       (★ VK 특유 — VkRenderPass / VkFramebuffer)
├── VulkanDescriptorSet.h / .cpp    (BindGroup wrap)
├── VulkanFence.h / .cpp
├── VulkanSemaphore.h / .cpp        (★ VK 특유 — GPU↔GPU sync)
├── VulkanMemoryAllocator.h / .cpp  (VMA wrapper — Vulkan Memory Allocator)
└── VulkanShaderCompiler.h / .cpp   (★ DXC HLSL→SPIR-V)
```

총 17쌍 = ~34 파일. (DX12 보다 많음 — VK 의 명시성)

---

## 2. 핵심 디자인 — VK vs DX12 차이

| 개념 | DX12 | Vulkan | RHI 추상화 |
|---|---|---|---|
| Device | ID3D12Device | VkDevice | IRHIDevice (공통) |
| Queue | ID3D12CommandQueue | VkQueue | IRHIQueue (공통) |
| Command list | ID3D12CommandList | VkCommandBuffer | IRHICommandList (공통) |
| Command allocator | ID3D12CommandAllocator | VkCommandPool | IRHICommandPool (공통) |
| Pipeline | ID3D12PipelineState | VkPipeline | IRHIPipelineState (공통) |
| Root signature | ID3D12RootSignature | VkPipelineLayout | IRHIBindGroupLayout (공통) |
| Descriptor heap | ID3D12DescriptorHeap | VkDescriptorPool + VkDescriptorSetLayout | IRHIBindGroup (공통) |
| Resource state | D3D12_RESOURCE_STATES | VkPipelineStageFlags + VkAccessFlags | eRHIResourceState (변환 필요) |
| Render pass | (DX12 = implicit) | VkRenderPass + VkFramebuffer | IRHIRenderPass (★ VK 가 명시 — DX12 도 RH-3 부터 명시) |
| Sync | Fence (monolithic) | Semaphore (GPU↔GPU) + Fence (GPU↔CPU) | IRHIFence + IRHISemaphore (분리) |
| Memory | (manual heap) | (manual VkDeviceMemory) | IRHIMemoryAllocator (공통 — D3D12MA / VMA wrap) |
| Surface | (DXGI swap chain) | VkSurfaceKHR + VkSwapchainKHR | IRHISwapChain (공통) |

VK 가 더 verbose 하지만 RHI 인터페이스는 같음. VK 백엔드 내부에서 더 많은 객체 관리.

---

## 3. DXC HLSL→SPIR-V 파이프라인

### 3.1 셰이더 컴파일

```cpp
// VulkanShaderCompiler.cpp
std::vector<u32_t> CVulkanShaderCompiler::CompileHLSLToSPIRV(
    const wchar_t* hlslPath,
    const char*    entryPoint,
    eRHIShaderStage stage)
{
    // DXC: HLSL → SPIR-V
    ComPtr<IDxcCompiler3> pCompiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));

    std::vector<LPCWSTR> args = {
        L"-E", entryPoint,
        L"-T", GetTargetProfile(stage),     // vs_6_0 / ps_6_0 / cs_6_0
        L"-spirv",                           // ★ SPIR-V output
        L"-fspv-target-env=vulkan1.2",
        L"-fvk-use-dx-layout",               // DX layout 호환
        L"-fvk-bind-register",               // register(b0) → set=0,binding=0
    };

    // ... compile + return SPIR-V bytecode (vector<u32_t>)
}
```

### 3.2 셰이더 모듈 생성

```cpp
// VulkanShader.cpp
std::unique_ptr<CVulkanShader> CVulkanShader::Create(
    VkDevice vkDevice, const RHIShaderDesc& desc)
{
    auto spirv = CVulkanShaderCompiler::CompileHLSLToSPIRV(
        desc.filepath, desc.entryPoint, desc.stage);

    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = spirv.size() * sizeof(u32_t);
    info.pCode    = spirv.data();

    VkShaderModule module;
    vkCreateShaderModule(vkDevice, &info, nullptr, &module);

    auto pShader = std::unique_ptr<CVulkanShader>(new CVulkanShader());
    pShader->m_Module = module;
    pShader->m_Stage  = desc.stage;
    pShader->m_SpirvBytecode = std::move(spirv);
    return pShader;
}
```

---

## 4. Validation Layer (★ Codex P2-29 — availability 사전 체크)

★ **Codex P2-29 보정**: validation layer 사용 전 사전 확인. 없으면 fallback / log.

```cpp
// VulkanInstance::Create
std::vector<const char*> layers;
if (desc.bDebug)
{
    // 1. 사용 가능한 layer 열거
    u32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    // 2. KHRONOS validation 존재 확인
    bool_t bValidationAvailable = false;
    for (const auto& layer : available)
    {
        if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
        {
            bValidationAvailable = true;
            break;
        }
    }

    // 3. 활성 또는 fallback log
    if (bValidationAvailable)
    {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        OutputDebugStringA("[CVulkanInstance] Validation layer enabled\n");
    }
    else
    {
        OutputDebugStringA("[CVulkanInstance] Validation layer unavailable. "
            "Install Vulkan SDK for debug builds. (continuing without validation)\n");
    }
}

VkInstanceCreateInfo info{};
info.enabledLayerCount   = static_cast<u32_t>(layers.size());
info.ppEnabledLayerNames = layers.data();
// ...
```

Debug callback 등록 → validation error/warning 을 OutputDebugStringA / 콘솔 로그.

---

## 5. Frame in Flight (VK 의 핵심)

```cpp
class CVulkanDevice
{
    static constexpr u32_t kMaxFramesInFlight = 2;

    struct FrameContext
    {
        VkSemaphore imageAvailableSem;
        VkSemaphore renderFinishedSem;
        VkFence     inFlightFence;
        std::unique_ptr<CVulkanCommandPool> pCmdPool;
    };

    FrameContext m_FrameContexts[kMaxFramesInFlight];
    u32_t m_FrameIndex = 0;

    void BeginFrame() override
    {
        auto& ctx = m_FrameContexts[m_FrameIndex];
        vkWaitForFences(m_Device, 1, &ctx.inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences  (m_Device, 1, &ctx.inFlightFence);

        u32_t imageIndex;
        vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX,
            ctx.imageAvailableSem, VK_NULL_HANDLE, &imageIndex);
        m_CurrentImageIndex = imageIndex;
    }

    void EndFrame() override
    {
        auto& ctx = m_FrameContexts[m_FrameIndex];

        VkPresentInfoKHR present{};
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = &ctx.renderFinishedSem;
        // ... swap chain present

        m_FrameIndex = (m_FrameIndex + 1) % kMaxFramesInFlight;
    }
};
```

---

## 6. Cross-platform 확장

VK 백엔드의 진짜 가치는 cross-platform:

| 플랫폼 | Surface 확장 |
|---|---|
| Windows | VK_KHR_win32_surface |
| Linux X11 | VK_KHR_xlib_surface |
| Linux Wayland | VK_KHR_wayland_surface |
| macOS | MoltenVK + VK_EXT_metal_surface |
| Android | VK_KHR_android_surface |

→ **Steam Deck / Linux 출시 시 VK 백엔드 필수**. macOS 는 MoltenVK 통해서.

---

## 6.5 VMA 외부 라이브러리 편입 (★ Codex P1-20)

### 6.5.1 Vulkan Memory Allocator 정보
- **GitHub**: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
- **License**: MIT
- **버전**: 3.1.0 (2024 기준 latest)
- **언어**: C++17
- **단일 헤더**: `vk_mem_alloc.h` (header-only, define `VMA_IMPLEMENTATION` 1곳에서)

### 6.5.2 ThirdPartyLib 편입 절차

```
Engine/ThirdPartyLib/
└── VMA/                           (★ 신규)
    ├── README.md                  (license + version note)
    └── Inc/
        └── vk_mem_alloc.h         (header-only)
```

**Engine.vcxproj 추가 ItemGroup**:
```xml
<ItemGroup Label="ThirdParty: VMA">
  <ClInclude Include="..\ThirdPartyLib\VMA\Inc\vk_mem_alloc.h" />
</ItemGroup>

<ItemDefinitionGroup>
  <ClCompile>
    <AdditionalIncludeDirectories>
      $(ProjectDir)..\ThirdPartyLib\VMA\Inc;
      %(AdditionalIncludeDirectories)
    </AdditionalIncludeDirectories>
  </ClCompile>
</ItemDefinitionGroup>
```

**`VulkanMemoryAllocator.cpp` 에서 implementation 활성화**:
```cpp
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
```

(★ define 은 단 1곳 에서만. 다른 곳은 일반 include.)

### 6.5.3 Vulkan SDK 의존
- VMA 자체는 header-only 지만 Vulkan headers 필요
- LunarG Vulkan SDK (https://vulkan.lunarg.com/) 설치 필수
- `VULKAN_SDK` 환경 변수 사용 — Engine.vcxproj 의 `$(VULKAN_SDK)\Include`, `$(VULKAN_SDK)\Lib`

### 6.5.4 합격
- ✅ ThirdPartyLib/VMA 폴더 + 1 헤더 배치
- ✅ Engine.vcxproj 편입 + WINTERS_RHI_VULKAN 컨피그에서만 빌드
- ✅ License 명시 (MIT)
- ✅ Vulkan SDK 설치 가이드 문서화

---

## 7. 합격 (★ Codex 2차 보정 — compile-only / visual parity 분리)

### 7.1 Compile-only 합격 (4~6주)
- ✅ 17쌍 VK 백엔드 파일 박제
- ✅ Winters.sln Debug-VK / Release-VK 컨피그
- ✅ VMA + Vulkan SDK 편입 완료
- ✅ DXC HLSL→SPIR-V 컴파일 파이프라인 동작
- ✅ VK 빌드 통과 + 첫 frame 도달

### 7.2 Visual parity 합격 (3~4주 추가)
- ✅ VK 빌드 → LoL 시각 결과 동일
- ✅ Validation layer 0 error / 0 warning (★ Codex P2-29 — availability 사전 체크)
- ✅ Frame in flight 2 buffer 동작 (vkAcquireNextImageKHR / vkQueuePresentKHR loop)
- ✅ Pipeline barrier 전수 검증 (validation 의 sync hazard 통과)
- ✅ MoltenVK / macOS 동작 확인 (선택)

---

## 8. 위험

| 위험 | 완화 |
|---|---|
| VK 의 verbose 함이 개발 속도 저하 | RH-1~5 의 인터페이스가 잘 잡혀있으면 mechanical translation. 단 sync primitive 는 추가 학습 필요 |
| SPIR-V cross-compile 시 register binding 충돌 | `-fvk-bind-register` + `register(b0, space0)` 명시 |
| MoltenVK 의 일부 feature 부재 (geometry shader 등) | Vulkan 1.2 baseline 으로 결정. macOS 는 fallback path |
| Validation layer 가 false positive 자주 발생 | VK_LAYER_KHRONOS_validation 의 known issues 문서 참조 |

---

## 9. 추후 박제

RH-5 합격 + cross-platform target 확정 시점 (Steam Deck / Linux 결정) 진입. 현재는 outline.
