# 2026-05-14 Week 14-17 상세 박제 — Track 2 RH-6 Vulkan Backend (선택)

**작성일**: 2026-05-14
**상태**: 검토 대기 (계획서만 작성, 코드 변경은 codex 가 진행 / 작성자 후속 검토)
**전제**: Week 10-13 (RH-5 DX12 visual parity 합격) 완료
**상위 문서**: [Twin Track 계획서 §5.5](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md), [RHI 마스터 §6.5 RH-6 합격](../rhi/00_RHI_MIGRATION_MASTER.md)

> **선택 단계**: Vulkan 은 cross-platform target (Steam Deck / Linux / Android) 결정 시점에 진입. LoL 모작 한정이면 skip 가능. 본 박제는 Vulkan 진입 결정 시 사용.

---

## 0. 한 줄

> **Week 14-17 = optional RH-6 Vulkan only after the corrected DX12 parity path is real. The Vulkan target shape remains `Engine/Private/RHI/Vulkan/` + VMA + SPIR-V compilation + validation fallback + Debug-VK config, but acceptance must start from deterministic bootstrap / parity first, not immediate full-LoL parity.**

> **Codebase reality correction (2026-05-02 review):**
> - The repo still has no backend selector, no DXC integration, no SPIR-V build path, and no runtime `--rhi=Vulkan` entrypoint.
> - DX11-only seams still exist in ImGui/UI/resource/renderer code, so Vulkan cannot assume gameplay parity immediately after DX12.
> - `Engine/ThirdPartyLib/Vulkan/`, `Engine/ThirdPartyLib/VMA/`, and `Tools/CompileShaders_Vulkan.ps1` do not exist in the current tree.

> **Library integration correction:**
> - Keep Vulkan loader linkage on the SDK / system side (`vulkan-1.lib` from `$(VULKAN_SDK)`), not by pretending the repo already ships a full local Vulkan SDK mirror.
> - Keep VMA as a single implementation TU owned by the Engine project (`VulkanVMA.cpp` with `#define VMA_IMPLEMENTATION`), not by referencing a nonexistent `vk_mem_alloc.cpp`.

> **Re-scoped order:**
> 1. W14: shared compiler/backend bootstrap + SDK gating + deterministic bring-up path.
> 2. W15-16: Vulkan backend files and allocator integration.
> 3. W17: deterministic parity; full gameplay parity only after UI/FX/runtime seams have a Vulkan story.
> - Treat the `--rhi=Vulkan`, `CompileShaders_Vulkan.ps1`, and `PixCompare.ps1 -Backend Vulkan` examples below as post-bootstrap target-shape items, not as tools that already exist in this repo today.

> **Codebase deep review correction (2026-05-02 2nd pass):**
> 아래 항목들은 원본 계획서에서 코드베이스에 존재하지 않는 타입/인터페이스를 참조하고 있어 수정되었다.
>
> **A. 존재하지 않는 타입 (컴파일 실패 원인)**
> - `CRHIResourceTable` — plan 문서에서만 언급. 코드 0 hit. 제거.
> - `IRHISwapChain` — 미존재. CVulkanSwapChain 을 standalone 으로 변경.
> - `IRHICommandList` — 미존재. CVulkanCommandBuffer 를 standalone 으로 변경.
> - `RHIBufferDesc`, `eRHIBufferUsage`, `RHIPipelineHandle`, `RHIBindGroupHandle`, `RHIRenderPassHandle`, `eRHIResourceState`, `eRHIFormat`, `eRHINativeType`, `RHIWindowHandle` — 미존재. 구체적 Vulkan 타입 또는 로컬 struct 로 대체.
> - `CDX12Device` — 미존재. §6.1 에서 참조 제거.
>
> **B. 컨벤션 위반 수정**
> - `WINTERS_ENGINE` dllexport 를 내부 전용 Vulkan 클래스에 적용 — §5.2 위반. 제거.
> - `namespace Engine` — CDX11Device 는 global namespace. 일관성 위해 global namespace 로 변경.
> - `CVulkanDevice(CVulkanDevice&&) = default` — raw VkDevice handle move 후 dangling. 삭제.
> - `CVulkanBufferImpl` — `Impl` 접미 비표준. `CVulkanBuffer` 로 통일.
> - `OutputDebugStringA` `#ifdef _DEBUG` 미감싸 — §security 규칙 5 위반. 감싸기.
> - `CEngineApp::OnInit()` — 실제 함수명은 `CEngineApp::Initialize()`.
> - `m_pDevice` 타입 — 현재 `unique_ptr<CDX11Device>`, `unique_ptr<IRHIDevice>` 아님.
>
> **C. 사전 의존성 (이 계획서 진입 전 필수)**
> - RH-1 ~ RH-4 (IRHISwapChain, IRHICommandList, eRHIResourceState 등 인터페이스 정의)
>   미완료 시 Vulkan 클래스들은 standalone 으로 구현하고, 추후 인터페이스 도입 시 상속 추가.
> - `eNativeHandleType` enum 에 Vulkan 엔트리 추가 필요 (`Engine/Public/RHI/RHITypes.h`).

---

## 1. Week 10-13 결과 검증 (Week 14 진입 전)

```bash
# 1. DX12 visual parity 합격 확인
# NOTE: Tools\PixCompare.ps1 은 W10-13 에서 신설. 미존재 시 수동 비교.
.\Tools\PixCompare.ps1 -Scenario Irelia_idle
# 기대: mean<1, max<5

# 2. PSO 캐시 동작
ls Cache/PSO_DX12.bin

# 3. Vulkan SDK 설치 확인 (1.3.x 이상 필수)
vulkaninfo --summary
# 기대: Instance Version 1.3+, GPU device 인식

# 4. DXC --spirv 옵션 사전 검증
dxc.exe -T ps_6_0 -spirv -E PS Shaders/Mesh3D_PBR.hlsl -Fo /tmp/test.spv
file /tmp/test.spv
# 기대: SPIR-V binary
```

---

## 2. Week 14-17 작업 매트릭스 (4주 분할)

### 2.1 Week 14 — Foundation

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T2.W14.1** | Vulkan SDK 사전 체크 + ThirdPartyLib 편입 | `Engine/ThirdPartyLib/VMA/` | (W13) |
| **T2.W14.2** | VMA 편입 (Vulkan Memory Allocator, AMD) | `Engine/ThirdPartyLib/VMA/` | T2.W14.1 |
| **T2.W14.3** | DXC SPIR-V 빌드 시스템 통합 | `Tools/CompileShaders_Vulkan.ps1` 또는 vcxproj custom build | (W13) |
| **T2.W14.4** | Engine.sln 의 Debug-VK / Release-VK 컨피그 추가 | `Engine.sln`, `*.vcxproj` | T2.W14.1~T2.W14.3 |
| **T2.W14.5** | `Engine/Private/RHI/Vulkan/` 디렉토리 + 컴파일 매크로 (`WINTERS_RHI_BACKEND_VULKAN`) | 신설 | T2.W14.4 |
| **T2.W14.6** | Validation layer 사전 체크 (`vkEnumerateInstanceLayerProperties`) | `VulkanDevice.cpp` | T2.W14.5 |
| **T2.W14.7** | `eNativeHandleType` Vulkan 엔트리 추가 | `Engine/Public/RHI/RHITypes.h` | T2.W14.5 |

### 2.2 Week 15 — Device + SwapChain + Resource

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T2.W15.1** | `CVulkanDevice.h+.cpp` (VkInstance + VkPhysicalDevice + VkDevice + Queues) | `Engine/Private/RHI/Vulkan/VulkanDevice.{h,cpp}` | (W14) |
| **T2.W15.2** | `CVulkanSwapChain.h+.cpp` (VK_KHR_swapchain) | `VulkanSwapChain.{h,cpp}` | T2.W15.1 |
| **T2.W15.3** | `CVulkanQueue.h+.cpp` (Graphics + Compute + Transfer) | `VulkanQueue.{h,cpp}` | T2.W15.1 |
| **T2.W15.4** | `CVulkanBuffer` (VMA Allocation + VkBuffer) | `VulkanBuffer.{h,cpp}` | T2.W15.1 |
| **T2.W15.5** | `CVulkanImage` (VMA + VkImage + ImageView) | `VulkanImage.{h,cpp}` | T2.W15.1 |
| **T2.W15.6** | `CVulkanShaderModule` (vkCreateShaderModule + SPIR-V 로드) | `VulkanShaderModule.{h,cpp}` | T2.W15.1 |
| **T2.W15.7** | `CVulkanSampler` (VkSampler + filter/address mode) | `VulkanSampler.{h,cpp}` | T2.W15.1 |

### 2.3 Week 16 — CommandBuffer + Descriptor + Pipeline

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T2.W16.1** | `CVulkanCommandPool + CVulkanCommandBuffer` (per-frame + per-thread) | `VulkanCommandBuffer.{h,cpp}` | (W15) |
| **T2.W16.2** | Descriptor Pool / Set Layout / Set 관리 | `VulkanDescriptor.{h,cpp}` | T2.W16.1 |
| **T2.W16.3** | `CVulkanRenderPass` (VkRenderPass + framebuffer) | `VulkanRenderPass.{h,cpp}` | T2.W16.1 |
| **T2.W16.4** | `CVulkanPipeline` (Graphics + Compute) | `VulkanPipeline.{h,cpp}` | T2.W16.2, T2.W16.3 |
| **T2.W16.5** | `CVulkanBindGroup` (DescriptorSet 채우기) | `VulkanBindGroup.{h,cpp}` | T2.W16.2 |
| **T2.W16.6** | Pipeline cache (`VkPipelineCache` 디스크) | `VulkanPipelineCache.{h,cpp}` | T2.W16.4 |
| **T2.W16.7** | Resource layout transition (image barrier) | `VulkanCommandBuffer.cpp` | T2.W16.1 |

### 2.4 Week 17 — 통합 + Visual Parity

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T2.W17.1** | CEngineApp backend 선택 (`#if defined(WINTERS_RHI_BACKEND_VULKAN)`) | `CEngineApp.{h,cpp}` | (W16) |
| **T2.W17.2** | DXC SPIR-V 셰이더 일괄 컴파일 자동화 (`.cso` 와 동등한 `.spv`) | build script | (W14) |
| **T2.W17.3** | Validation layer 0 error/warning 달성 (디버깅) | runtime | 모두 |
| **T2.W17.4** | RenderDoc capture DX11 vs Vulkan 비교 | RenderDoc | T2.W17.3 |
| **T2.W17.5** | Frame diff 측정 (mean<1, max<5) | 스크립트 | T2.W17.4 |
| **T2.W17.6** | LoL 게임 1 매치 회귀 0 | 수동 | 모두 |

---

## 3. Week 14 — Foundation

### 3.1 Vulkan SDK 사전 체크 (T2.W14.1)

**최소 요구사항**: Vulkan SDK 1.3.x. `LunarG SDK` https://vulkan.lunarg.com/

**환경 변수**:
- `VULKAN_SDK` = `C:\VulkanSDK\1.3.x` (자동 설정)
- `Path` 에 `%VULKAN_SDK%\Bin` 추가

**Vulkan 헤더/라이브러리 참조 (SDK 시스템 경로)**:

vcxproj 의 `$(VULKAN_SDK)\Include` 및 `$(VULKAN_SDK)\Lib` 로 참조한다.
`Engine/ThirdPartyLib/Vulkan/` 에 SDK 헤더를 복사하지 **않는다** (correction note 반영).
Vulkan loader (`vulkan-1.dll`) 은 시스템 설치 (LunarG SDK 또는 GPU 드라이버) 필수.

### 3.2 VMA 편입 (T2.W14.2)

**라이브러리**: Vulkan Memory Allocator (`https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator`, MIT, AMD)

**편입 위치**: `Engine/ThirdPartyLib/VMA/`

```
Engine/ThirdPartyLib/VMA/
+-- Inc/vk_mem_alloc.h              (single-header)
+-- README.md
```

**컴파일 통합** (Engine.vcxproj — Debug-VK / Release-VK 만):

```xml
<ItemGroup Condition="'$(Configuration)'=='Debug-VK' Or '$(Configuration)'=='Release-VK'">
  <ClCompile Include="..\Private\RHI\Vulkan\VulkanVMA.cpp" />
</ItemGroup>
```

`VulkanVMA.cpp`:

```cpp
// Engine/Private/RHI/Vulkan/VulkanVMA.cpp
// VMA single implementation TU (ODR: 프로젝트 전체에서 이 파일 1개에서만 VMA_IMPLEMENTATION 정의)
#include "WintersPCH.h"

#if defined(WINTERS_RHI_BACKEND_VULKAN)
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>
#endif
```

### 3.3 DXC SPIR-V 빌드 시스템 (T2.W14.3)

**셰이더 일괄 컴파일 스크립트** (`Tools/CompileShaders_Vulkan.ps1`):

```powershell
# DX11/DX12 = .cso (DXIL)
# Vulkan    = .spv (SPIR-V)

$shaders = Get-ChildItem -Recurse -Filter "*.hlsl" Shaders/
$dxc = "$env:VULKAN_SDK\Bin\dxc.exe"

if (-not (Test-Path $dxc)) {
    Write-Error "[Vulkan] dxc.exe not found at $dxc. Install LunarG Vulkan SDK."
    exit 1
}

foreach ($sh in $shaders) {
    $vsOut = $sh.FullName -replace '\.hlsl$', '_vs.spv'
    $psOut = $sh.FullName -replace '\.hlsl$', '_ps.spv'
    $csOut = $sh.FullName -replace '\.hlsl$', '_cs.spv'

    if ($sh.Name -notmatch 'CS\.hlsl$') {
        & $dxc -spirv -T vs_6_0 -E VS $sh.FullName -Fo $vsOut
        & $dxc -spirv -T ps_6_0 -E PS $sh.FullName -Fo $psOut
    } else {
        & $dxc -spirv -T cs_6_0 -E CS_Main $sh.FullName -Fo $csOut
    }
}

Write-Host "[Vulkan] Shaders compiled to .spv"
```

**vcxproj 의 Custom Build Step** (Debug-VK 진입 시 자동 실행):

```xml
<PreBuildEvent Condition="'$(Configuration)'=='Debug-VK'">
  <Command>powershell -ExecutionPolicy Bypass -File "$(SolutionDir)Tools\CompileShaders_Vulkan.ps1"</Command>
</PreBuildEvent>
```

### 3.4 Engine.sln Debug-VK 컨피그 (T2.W14.4)

**주의**: 현재 Engine.vcxproj 는 `Debug|x64` + `Release|x64` 만 존재.
아래 블록은 **추가**이며, 기존 Debug/Release 와 병존한다.

```xml
<!-- ProjectConfigurations 에 추가 -->
<ProjectConfiguration Include="Debug-VK|x64">
  <Configuration>Debug-VK</Configuration>
  <Platform>x64</Platform>
</ProjectConfiguration>

<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug-VK|x64'" Label="Configuration">
  <ConfigurationType>DynamicLibrary</ConfigurationType>
  <UseDebugLibraries>true</UseDebugLibraries>
  <PlatformToolset>v143</PlatformToolset>
  <CharacterSet>Unicode</CharacterSet>
</PropertyGroup>

<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug-VK|x64'">
  <OutDir>$(ProjectDir)..\Bin\Debug-VK\</OutDir>
  <IntDir>$(ProjectDir)..\Bin\Intermediate\Debug-VK\</IntDir>
  <TargetName>WintersEngine</TargetName>
</PropertyGroup>

<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug-VK|x64'">
  <ClCompile>
    <WarningLevel>Level3</WarningLevel>
    <SDLCheck>true</SDLCheck>
    <PreprocessorDefinitions>WINTERS_ENGINE_EXPORTS;WINTERS_RHI_BACKEND_VULKAN;VK_USE_PLATFORM_WIN32_KHR;WIN32;_DEBUG;_WINDOWS;_USRDLL;WINTERS_PROFILING;%(PreprocessorDefinitions)</PreprocessorDefinitions>
    <ConformanceMode>true</ConformanceMode>
    <LanguageStandard>stdcpp20</LanguageStandard>
    <AdditionalOptions>/utf-8 %(AdditionalOptions)</AdditionalOptions>
    <AdditionalIncludeDirectories>$(VULKAN_SDK)\Include;$(ProjectDir)..\ThirdPartyLib\VMA\Inc;$(ProjectDir)..\External\imgui;$(ProjectDir)..\External\imgui\backends;$(ProjectDir)..\Public;$(ProjectDir)..\Public\ECS;$(ProjectDir)..\Public\ECS\Components;$(ProjectDir)..\Public\ECS\Systems;$(ProjectDir)..\Public\Sound;$(ProjectDir)..\ThirdPartyLib\Assimp\Inc;$(ProjectDir)..\ThirdPartyLib\DirectXTK\Inc;$(ProjectDir)..\ThirdPartyLib\FMOD\Inc;$(ProjectDir);%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    <FloatingPointModel>Precise</FloatingPointModel>
    <Optimization>Disabled</Optimization>
    <RuntimeLibrary>MultiThreadedDebugDLL</RuntimeLibrary>
    <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    <PrecompiledHeader>Use</PrecompiledHeader>
    <PrecompiledHeaderFile>WintersPCH.h</PrecompiledHeaderFile>
    <ForcedIncludeFiles>WintersPCH.h</ForcedIncludeFiles>
  </ClCompile>
  <Link>
    <SubSystem>Windows</SubSystem>
    <GenerateDebugInformation>true</GenerateDebugInformation>
    <AdditionalLibraryDirectories>$(VULKAN_SDK)\Lib;$(ProjectDir)..\ThirdPartyLib\Assimp\Lib\Debug;$(ProjectDir)..\ThirdPartyLib\DirectXTK\Lib\Debug;$(ProjectDir)..\ThirdPartyLib\FMOD\Lib;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    <AdditionalDependencies>vulkan-1.lib;d3d11.lib;dxgi.lib;d3dcompiler.lib;dxguid.lib;assimp-vc143-mtd.lib;DirectXTK.lib;fmod_vc.lib;%(AdditionalDependencies)</AdditionalDependencies>
  </Link>
  <PostBuildEvent>
    <Command>$(MSBuildThisFileDirectory)..\..\UpdateLib.bat</Command>
    <Message>EngineSDK 배포 (Debug-VK)</Message>
  </PostBuildEvent>
</ItemDefinitionGroup>
```

**필수 포함**: `WINTERS_ENGINE_EXPORTS` — 현재 Debug/Release 에 이미 존재하며 DLL export 필수. 원본 계획서에서 누락.

### 3.5 eNativeHandleType Vulkan 엔트리 추가 (T2.W14.7)

**파일**: `Engine/Public/RHI/RHITypes.h`

현재:
```cpp
enum class eNativeHandleType : u32_t
{
    Unknown = 0,
    DX11Device,
    DX11DeviceContext,
    DX11SwapChain,
};
```

변경:
```cpp
enum class eNativeHandleType : u32_t
{
    Unknown = 0,
    DX11Device,
    DX11DeviceContext,
    DX11SwapChain,
    // ── Vulkan (W14 추가) ──
    VulkanInstance,
    VulkanPhysicalDevice,
    VulkanDevice,
    VulkanGraphicsQueue,
};
```

### 3.6 Validation layer 사전 체크 (T2.W14.6)

**핵심**: Vulkan SDK 미설치 환경에서 validation layer 부재 시 자동 fallback.

```cpp
#if defined(WINTERS_RHI_BACKEND_VULKAN)

static bool_t CheckValidationLayerSupport()
{
    u32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

    const char* targetLayer = "VK_LAYER_KHRONOS_validation";
    for (const auto& l : layers)
    {
        if (strcmp(l.layerName, targetLayer) == 0)
            return true;
    }

#ifdef _DEBUG
    OutputDebugStringA("[Vulkan] WARNING: Validation layer not available, running without validation.\n");
#endif
    return false;
}

#endif // WINTERS_RHI_BACKEND_VULKAN
```

**활용**:

```cpp
const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
VkInstanceCreateInfo info{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
if (CheckValidationLayerSupport() && bEnableValidation)
{
    info.enabledLayerCount   = 1;
    info.ppEnabledLayerNames = layers;
}
else
{
    info.enabledLayerCount   = 0;
    info.ppEnabledLayerNames = nullptr;
}
```

---

## 4. Week 15 — Device + SwapChain + Resource

### 4.1 CVulkanDevice (T2.W15.1)

**파일**: `Engine/Private/RHI/Vulkan/VulkanDevice.h`

> **수정 사항 (코드베이스 대조)**:
> - `#include "RHI/CRHIResourceTable.h"` 제거 — 파일 미존재.
> - `namespace Engine` 제거 — CDX11Device 는 global namespace.
> - `WINTERS_ENGINE` dllexport 제거 — 내부 전용 클래스 (§5.2).
> - Move ctor/assign 삭제 — VkDevice 등 raw handle, move 후 dangling.
> - `GetBackend()` / `GetNativeHandle()` override 명시 — IRHIDevice 의 실제 인터페이스 (2개 메서드만).
> - "DX12Device 동일 시그니처" 주석 제거 — IRHIDevice 인터페이스가 미확장 상태.

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_VULKAN)

#include "RHI/IRHIDevice.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <memory>

class CVulkanSwapChain;
class CVulkanQueue;

class CVulkanDevice final : public IRHIDevice
{
public:
    ~CVulkanDevice();
    CVulkanDevice(const CVulkanDevice&) = delete;
    CVulkanDevice& operator=(const CVulkanDevice&) = delete;

    static std::unique_ptr<CVulkanDevice> Create(bool_t bEnableValidation = true);

    // ── IRHIDevice override (현재 인터페이스: GetBackend + GetNativeHandle 2개) ──
    eRHIBackend GetBackend() const override { return eRHIBackend::Vulkan; }
    void* GetNativeHandle(eNativeHandleType type) const override;

    // ── Vulkan native (Engine 내부 전용) ──
    VkInstance       GetInstance()       const { return m_Instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice         GetDevice()         const { return m_Device; }
    VmaAllocator     GetAllocator()      const { return m_Allocator; }

    u32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }

private:
    CVulkanDevice() = default;

    VkInstance       m_Instance       = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice         m_Device         = VK_NULL_HANDLE;
    VmaAllocator     m_Allocator      = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;

    u32_t m_GraphicsQueueFamily = UINT32_MAX;
    u32_t m_ComputeQueueFamily  = UINT32_MAX;
    u32_t m_TransferQueueFamily = UINT32_MAX;

    std::unique_ptr<CVulkanQueue> m_pGraphicsQueue;
    std::unique_ptr<CVulkanQueue> m_pComputeQueue;
    std::unique_ptr<CVulkanQueue> m_pTransferQueue;
};

#endif // WINTERS_RHI_BACKEND_VULKAN
```

`.cpp` Create 핵심:

```cpp
#include "WintersPCH.h"

#if defined(WINTERS_RHI_BACKEND_VULKAN)
#include "RHI/Vulkan/VulkanDevice.h"

// ── GetNativeHandle ──
void* CVulkanDevice::GetNativeHandle(eNativeHandleType type) const
{
    switch (type)
    {
    case eNativeHandleType::VulkanInstance:       return (void*)m_Instance;
    case eNativeHandleType::VulkanPhysicalDevice: return (void*)m_PhysicalDevice;
    case eNativeHandleType::VulkanDevice:         return (void*)m_Device;
    default: return nullptr;
    }
}

CVulkanDevice::~CVulkanDevice()
{
    if (m_Allocator != VK_NULL_HANDLE)
        vmaDestroyAllocator(m_Allocator);
    if (m_Device != VK_NULL_HANDLE)
        vkDestroyDevice(m_Device, nullptr);
    if (m_DebugMessenger != VK_NULL_HANDLE)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(m_Instance, m_DebugMessenger, nullptr);
    }
    if (m_Instance != VK_NULL_HANDLE)
        vkDestroyInstance(m_Instance, nullptr);
}

std::unique_ptr<CVulkanDevice> CVulkanDevice::Create(bool_t bEnableValidation)
{
    auto p = std::unique_ptr<CVulkanDevice>(new CVulkanDevice());

    // 1. Instance
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName  = "WintersGame";
    appInfo.applicationVersion= VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName       = "WintersEngine";
    appInfo.engineVersion     = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion        = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };
    if (bEnableValidation)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    std::vector<const char*> layers;
    if (bEnableValidation && CheckValidationLayerSupport())
        layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo ici{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    ici.pApplicationInfo        = &appInfo;
    ici.enabledExtensionCount   = static_cast<u32_t>(extensions.size());
    ici.ppEnabledExtensionNames = extensions.data();
    ici.enabledLayerCount       = static_cast<u32_t>(layers.size());
    ici.ppEnabledLayerNames     = layers.data();

    if (vkCreateInstance(&ici, nullptr, &p->m_Instance) != VK_SUCCESS)
        return nullptr;

    // 2. Physical Device (DISCRETE_GPU 우선)
    u32_t devCount = 0;
    vkEnumeratePhysicalDevices(p->m_Instance, &devCount, nullptr);
    std::vector<VkPhysicalDevice> devs(devCount);
    vkEnumeratePhysicalDevices(p->m_Instance, &devCount, devs.data());

    for (auto d : devs)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            p->m_PhysicalDevice = d;
            break;
        }
    }
    if (!p->m_PhysicalDevice && !devs.empty())
        p->m_PhysicalDevice = devs[0];

    if (!p->m_PhysicalDevice)
        return nullptr;

    // 3. Queue family 탐색
    u32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(p->m_PhysicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfs(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(p->m_PhysicalDevice, &qfCount, qfs.data());

    for (u32_t i = 0; i < qfCount; ++i)
    {
        if ((qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && p->m_GraphicsQueueFamily == UINT32_MAX)
            p->m_GraphicsQueueFamily = i;
    }

    if (p->m_GraphicsQueueFamily == UINT32_MAX)
        return nullptr;

    // 4. Logical Device
    f32_t qPriority = 1.0f;
    VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    qci.queueFamilyIndex = p->m_GraphicsQueueFamily;
    qci.queueCount       = 1;
    qci.pQueuePriorities = &qPriority;

    const char* devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount    = 1;
    dci.pQueueCreateInfos       = &qci;
    dci.enabledExtensionCount   = 1;
    dci.ppEnabledExtensionNames = devExts;
    dci.pEnabledFeatures        = &features;

    if (vkCreateDevice(p->m_PhysicalDevice, &dci, nullptr, &p->m_Device) != VK_SUCCESS)
        return nullptr;

    // 5. VMA
    VmaAllocatorCreateInfo aci{};
    aci.physicalDevice   = p->m_PhysicalDevice;
    aci.device           = p->m_Device;
    aci.instance         = p->m_Instance;
    aci.vulkanApiVersion = VK_API_VERSION_1_3;
    if (vmaCreateAllocator(&aci, &p->m_Allocator) != VK_SUCCESS)
        return nullptr;

    return p;
}

#endif // WINTERS_RHI_BACKEND_VULKAN
```

### 4.2 CVulkanSwapChain (T2.W15.2)

> **수정 사항**:
> - `IRHISwapChain` 상속 제거 — 인터페이스 미존재. standalone 클래스.
> - `WINTERS_ENGINE` 제거 — 내부 전용.
> - `RHIWindowHandle` → `HWND` (Win32 native).
> - `eRHINativeType` → 불필요, 제거.
> - `RHITextureHandle GetCurrentBackBuffer()` → `VkImageView GetCurrentImageView()` (native).
> - 추후 IRHISwapChain 인터페이스 도입 (RH-2~RH-3) 시 상속 추가.

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_VULKAN)

#include <vulkan/vulkan.h>
#include "WintersTypes.h"
#include <vector>
#include <memory>

class CVulkanDevice;

class CVulkanSwapChain final
{
public:
    ~CVulkanSwapChain();
    CVulkanSwapChain(const CVulkanSwapChain&) = delete;
    CVulkanSwapChain& operator=(const CVulkanSwapChain&) = delete;

    static std::unique_ptr<CVulkanSwapChain> Create(CVulkanDevice* pDevice, HWND hwnd, u32_t w, u32_t h);

    void Present(bool_t bVSync);
    u32_t GetCurrentBackBufferIndex() const { return m_ImageIndex; }
    VkImageView GetCurrentImageView() const;
    void Resize(u32_t w, u32_t h);

private:
    CVulkanSwapChain() = default;

    CVulkanDevice*  m_pDevice        = nullptr;
    VkSurfaceKHR    m_Surface        = VK_NULL_HANDLE;
    VkSwapchainKHR  m_SwapChain      = VK_NULL_HANDLE;
    std::vector<VkImage>     m_Images;
    std::vector<VkImageView> m_ImageViews;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkSemaphore> m_RenderFinishedSemaphores;
    u32_t m_ImageIndex = 0;
    VkFormat m_Format  = VK_FORMAT_B8G8R8A8_SRGB;
};

#endif // WINTERS_RHI_BACKEND_VULKAN
```

### 4.3 CVulkanBuffer / Image (T2.W15.4~T2.W15.5)

> **수정 사항**:
> - `CVulkanBufferImpl` → `CVulkanBuffer` (Impl 접미 비표준).
> - `RHIBufferDesc` → `VulkanBufferDesc` 로컬 struct 신설 (RHIBufferDesc 미존재).
> - `eRHIBufferUsage` → `VkBufferUsageFlags` 직접 사용.
> - 추후 RH-4 (Resource Handle) 도입 시 RHIBufferDesc 으로 교체.

**Buffer**:

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_VULKAN)

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "WintersTypes.h"

struct VulkanBufferDesc
{
    u64_t              sizeBytes = 0;
    VkBufferUsageFlags usage     = 0;
    bool_t             bDynamic  = false;
};

class CVulkanBuffer
{
public:
    ~CVulkanBuffer();
    CVulkanBuffer(const CVulkanBuffer&) = delete;
    CVulkanBuffer& operator=(const CVulkanBuffer&) = delete;

    static std::unique_ptr<CVulkanBuffer> Create(VmaAllocator alloc, const VulkanBufferDesc& desc, const void* pInitData = nullptr);

    VkBuffer       GetBuffer()     const { return m_Buffer; }
    VmaAllocation  GetAllocation() const { return m_Allocation; }

private:
    CVulkanBuffer() = default;

    VkBuffer         m_Buffer     = VK_NULL_HANDLE;
    VmaAllocation    m_Allocation = VK_NULL_HANDLE;
    VmaAllocator     m_Allocator  = VK_NULL_HANDLE;
    VulkanBufferDesc m_Desc{};
};

#endif // WINTERS_RHI_BACKEND_VULKAN
```

`.cpp`:

```cpp
#include "WintersPCH.h"

#if defined(WINTERS_RHI_BACKEND_VULKAN)
#include "RHI/Vulkan/VulkanBuffer.h"

CVulkanBuffer::~CVulkanBuffer()
{
    if (m_Buffer != VK_NULL_HANDLE && m_Allocator != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_Allocator, m_Buffer, m_Allocation);
}

std::unique_ptr<CVulkanBuffer> CVulkanBuffer::Create(VmaAllocator alloc, const VulkanBufferDesc& desc, const void* pInitData)
{
    auto p = std::unique_ptr<CVulkanBuffer>(new CVulkanBuffer());
    p->m_Desc      = desc;
    p->m_Allocator = alloc;

    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size  = desc.sizeBytes;
    bci.usage = desc.usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo aci{};
    aci.usage = desc.bDynamic
        ? VMA_MEMORY_USAGE_CPU_TO_GPU
        : VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateBuffer(alloc, &bci, &aci, &p->m_Buffer, &p->m_Allocation, nullptr) != VK_SUCCESS)
        return nullptr;

    if (pInitData && desc.bDynamic)
    {
        void* pMapped = nullptr;
        vmaMapMemory(alloc, p->m_Allocation, &pMapped);
        memcpy(pMapped, pInitData, static_cast<size_t>(desc.sizeBytes));
        vmaUnmapMemory(alloc, p->m_Allocation);
    }
    // GPU-only + initData: staging buffer -> vkCmdCopyBuffer (CommandBuffer 통해 처리)

    return p;
}

#endif // WINTERS_RHI_BACKEND_VULKAN
```

**Image** (Texture): 동일 패턴 + `VkImageView` 추가.

---

## 5. Week 16 — CommandBuffer + Pipeline

### 5.1 CVulkanCommandBuffer (T2.W16.1)

> **수정 사항**:
> - `IRHICommandList` 상속 제거 — 인터페이스 미존재. standalone.
> - `WINTERS_ENGINE` 제거 — 내부 전용.
> - `RHIPipelineHandle`, `RHIBindGroupHandle`, `RHIRenderPassHandle` → Vulkan native 타입.
> - `eRHIFormat`, `eRHIResourceState` → Vulkan native enum.
> - 추후 IRHICommandList 인터페이스 도입 (RH-2) 시 상속 추가.

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_VULKAN)

#include <vulkan/vulkan.h>
#include "WintersTypes.h"
#include <memory>

class CVulkanDevice;

class CVulkanCommandBuffer final
{
public:
    ~CVulkanCommandBuffer();
    CVulkanCommandBuffer(const CVulkanCommandBuffer&) = delete;
    CVulkanCommandBuffer& operator=(const CVulkanCommandBuffer&) = delete;

    static std::unique_ptr<CVulkanCommandBuffer> Create(CVulkanDevice* pDevice);

    void Begin();
    void End();

    void BeginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer,
                         VkExtent2D extent, const VkClearValue* pClearValues, u32_t clearCount);
    void EndRenderPass();

    void BindPipeline(VkPipeline pipeline, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
    void BindDescriptorSets(VkPipelineLayout layout, u32_t firstSet,
                            u32_t setCount, const VkDescriptorSet* pSets);

    void SetVertexBuffer(u32_t binding, VkBuffer buffer, VkDeviceSize offset = 0);
    void SetIndexBuffer(VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType);

    void Draw(u32_t vertexCount, u32_t instanceCount, u32_t firstVertex, u32_t firstInstance);
    void DrawIndexed(u32_t indexCount, u32_t instanceCount, u32_t firstIndex,
                     i32_t vertexOffset, u32_t firstInstance);
    void Dispatch(u32_t x, u32_t y, u32_t z);

    VkCommandBuffer GetNative() const { return m_CmdBuffer; }

private:
    CVulkanCommandBuffer() = default;

    VkCommandPool   m_Pool       = VK_NULL_HANDLE;
    VkCommandBuffer m_CmdBuffer  = VK_NULL_HANDLE;
    CVulkanDevice*  m_pOwner     = nullptr;
};

#endif // WINTERS_RHI_BACKEND_VULKAN
```

### 5.2 Image layout transition (T2.W16.7)

DX12 의 ResourceBarrier 와 동일 개념:

```cpp
#if defined(WINTERS_RHI_BACKEND_VULKAN)

static void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                   VkImageLayout oldLayout, VkImageLayout newLayout)
{
    if (oldLayout == newLayout) return;

    VkImageMemoryBarrier b{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.baseMipLevel   = 0;
    b.subresourceRange.levelCount     = 1;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

#endif // WINTERS_RHI_BACKEND_VULKAN
```

### 5.3 VkPipelineCache (T2.W16.6)

> **수정 사항**:
> - `ReadFile` / `WriteFile` 는 엔진에 존재하지 않는 전역 함수. 파일 I/O 로직을 .cpp 에 인라인.

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_VULKAN)

#include <vulkan/vulkan.h>
#include "WintersTypes.h"
#include <memory>
#include <vector>

class CVulkanPipelineCache
{
public:
    ~CVulkanPipelineCache();
    CVulkanPipelineCache(const CVulkanPipelineCache&) = delete;
    CVulkanPipelineCache& operator=(const CVulkanPipelineCache&) = delete;

    static std::unique_ptr<CVulkanPipelineCache> Create(VkDevice device, const wstring_t& cachePath);

    VkPipelineCache GetCache() const { return m_Cache; }
    bool_t Flush();

private:
    CVulkanPipelineCache() = default;

    VkPipelineCache m_Cache  = VK_NULL_HANDLE;
    VkDevice        m_Device = VK_NULL_HANDLE;
    wstring_t       m_CachePath;
};

#endif // WINTERS_RHI_BACKEND_VULKAN
```

`.cpp`:

```cpp
#include "WintersPCH.h"

#if defined(WINTERS_RHI_BACKEND_VULKAN)
#include "RHI/Vulkan/VulkanPipelineCache.h"
#include <fstream>

CVulkanPipelineCache::~CVulkanPipelineCache()
{
    Flush();
    if (m_Cache != VK_NULL_HANDLE)
        vkDestroyPipelineCache(m_Device, m_Cache, nullptr);
}

std::unique_ptr<CVulkanPipelineCache> CVulkanPipelineCache::Create(VkDevice device, const wstring_t& cachePath)
{
    auto p = std::unique_ptr<CVulkanPipelineCache>(new CVulkanPipelineCache());
    p->m_Device    = device;
    p->m_CachePath = cachePath;

    // 기존 캐시 파일 로드
    std::vector<u8_t> blob;
    {
        std::ifstream f(cachePath, std::ios::binary | std::ios::ate);
        if (f.is_open())
        {
            auto sz = f.tellg();
            if (sz > 0)
            {
                blob.resize(static_cast<size_t>(sz));
                f.seekg(0);
                f.read(reinterpret_cast<char*>(blob.data()), sz);
            }
        }
    }

    VkPipelineCacheCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    info.initialDataSize = blob.size();
    info.pInitialData    = blob.empty() ? nullptr : blob.data();

    if (vkCreatePipelineCache(device, &info, nullptr, &p->m_Cache) != VK_SUCCESS)
    {
        // Cache invalidate (driver change) -> empty cache
        info.initialDataSize = 0;
        info.pInitialData    = nullptr;
        vkCreatePipelineCache(device, &info, nullptr, &p->m_Cache);
    }
    return p;
}

bool_t CVulkanPipelineCache::Flush()
{
    if (m_Cache == VK_NULL_HANDLE || m_Device == VK_NULL_HANDLE)
        return false;

    size_t size = 0;
    vkGetPipelineCacheData(m_Device, m_Cache, &size, nullptr);
    if (size == 0) return false;

    std::vector<u8_t> blob(size);
    vkGetPipelineCacheData(m_Device, m_Cache, &size, blob.data());

    std::ofstream f(m_CachePath, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(blob.data()), static_cast<std::streamsize>(size));
    return f.good();
}

#endif // WINTERS_RHI_BACKEND_VULKAN
```

---

## 6. Week 17 — 통합 + Visual Parity

### 6.1 CEngineApp backend 선택 (T2.W17.1)

> **수정 사항**:
> - `CEngineApp::OnInit()` → `CEngineApp::Initialize()` (실제 함수명).
> - `CDX12Device::Create()` 참조 제거 — 미존재.
> - 현재 `m_pDevice` 타입은 `unique_ptr<CDX11Device>`. Vulkan 백엔드 지원 시 `unique_ptr<IRHIDevice>` 로 변경 필요.
> - `CEngineApp::Initialize()` 는 `IWintersApp*` + `EngineConfig&` 를 받는 시그니처.

```cpp
// CEngineApp.h 변경 (W17 진입 시)
//   기존: unique_ptr<CDX11Device> m_pDevice;
//   변경: unique_ptr<IRHIDevice>  m_pDevice;
//   + GetDevice() 반환 타입도 IRHIDevice& 로 유지 (이미 선언됨)

// CEngineApp.cpp Initialize() 내 디바이스 생성 분기:
bool CEngineApp::Initialize(IWintersApp* pGameApp, const EngineConfig& config)
{
    // ... (기존 Window 초기화 등)

#if defined(WINTERS_RHI_BACKEND_VULKAN)
    m_pDevice = CVulkanDevice::Create(/* enableValidation */ true);
#else
    DeviceDesc desc{};
    desc.hwnd   = m_Window.GetHWND();
    desc.width  = config.width;
    desc.height = config.height;
    desc.vsync  = config.vsync;
    m_pDevice = CDX11Device::Create(desc);
#endif

    if (!m_pDevice)
    {
        MessageBoxW(nullptr, L"RHI device creation failed", L"Error", MB_OK);
        return false;
    }

    // ... (이하 동일)
    return true;
}
```

### 6.2 셰이더 컴파일 자동화 검증 (T2.W17.2)

```bash
# Debug-VK 빌드 시 PreBuildEvent 가 자동 실행
# NOTE: CompileShaders_Vulkan.ps1 은 W14 에서 신설
.\Tools\CompileShaders_Vulkan.ps1
ls Shaders/Mesh3D_PBR_vs.spv Shaders/Mesh3D_PBR_ps.spv
file Shaders/Mesh3D_PBR_vs.spv
# 기대: SPIR-V binary
```

### 6.3 Validation 0 error/warning (T2.W17.3)

```bash
# Vulkan validation 메시지 OutputDebugString 으로 받기
# NOTE: --rhi=Vulkan CLI 파서는 W17 에서 신설 (현재 미존재).
#       초기 테스트는 Debug-VK 빌드 자체가 Vulkan 경로를 타므로 CLI 불필요.
WintersGame.exe
# 기대: vkCreateInstance/Device/SwapChain/... 모두 ERROR/WARN 0
```

흔한 validation issue:
- Image layout transition 누락 (TransitionImageLayout 호출 누락)
- Descriptor set update 시점 (vkCmdBindDescriptorSets 전에 vkUpdateDescriptorSets)
- Synchronization (semaphore / fence) 부족

### 6.4 Frame diff 측정 (T2.W17.5)

```powershell
# DX11 baseline + Vulkan test 비교
# NOTE: PixCompare.ps1 은 W10-13 에서 신설. 미존재 시 RenderDoc 수동 비교.
.\Tools\PixCompare.ps1 -Backend Vulkan -Scenario Irelia_idle
# 기대: mean<1, max<5 (DX12 와 동일 기준)
```

### 6.5 LoL 게임 1 매치 회귀 (T2.W17.6)

DX11 / Vulkan 2 백엔드 동일 시나리오 진행 (DX12 는 RH-5 합격 후 추가):
- BanPick 이렐리아
- 5v5 봇전 1 매치 (10분)
- 시각 회귀 0
- Frame <=20ms 동일
- Crash 0
- Validation 0 error

---

## 7. 위험 시나리오

### 7.1 R-W14-1: Vulkan SDK 미설치 환경 빌드 실패
- 시나리오: codex 가 작업하는 환경에 Vulkan SDK 없으면 빌드 자체 실패
- 완화: (1) Debug-VK 컨피그는 SDK 필수 명시 (2) Debug/Release 컨피그는 SDK 무관 (3) CI/CD 에 LunarG SDK 설치 자동화

### 7.2 R-W14-2: VMA implementation 한 번만 정의
- 시나리오: VMA 는 single-header (header-only). `VMA_IMPLEMENTATION` 정의된 .cpp 가 1개만 있어야 ODR 위반 없음.
- 완화: (1) `Engine/Private/RHI/Vulkan/VulkanVMA.cpp` 한 파일에서만 `#define VMA_IMPLEMENTATION` (2) 다른 곳은 단순 include

### 7.3 R-W15-1: Image layout transition 누락 -> black/garbage screen
- 시나리오: SwapChain image 가 PRESENT_SRC 로 transition 안 되어 검은 화면
- 완화: (1) BeginRenderPass / EndRenderPass 에서 자동 transition (2) RenderDoc 의 "Image layout" 탭 으로 모든 image 의 layout 확인

### 7.4 R-W16-1: Descriptor pool 고갈
- 시나리오: 매 프레임 vkAllocateDescriptorSets 호출 -> 풀 고갈 후 VK_ERROR_OUT_OF_POOL_MEMORY
- 완화: (1) per-frame descriptor pool reset (vkResetDescriptorPool) (2) 또는 영구 BindGroup -> CreateBindGroup 시점에 1번만 allocate

### 7.5 R-W16-2: SPIR-V binding mismatch (DXC --register-shift)
- 시나리오: HLSL register(b0) -> SPIR-V binding 0 일까 1일까?
- 완화: (1) DXC 의 `--register-shift` 옵션으로 shift 명시 (2) root signature 와 SPIR-V binding 의 매핑 표 박제 (3) Reflection 으로 자동 검증

### 7.6 R-W17-1: Vulkan SDK 1.3.x 없으면 vkCreateInstance 실패
- 시나리오: VK_API_VERSION_1_3 명시인데 GPU/드라이버가 1.2 만 지원
- 완화: (1) vkEnumerateInstanceVersion 으로 사전 체크 (2) fallback to 1.2

### 7.7 R-W17-2: Steam Deck / Linux 빌드 시점 분리
- 시나리오: Vulkan 백엔드 + Linux toolchain 동시 도입은 너무 큰 작업
- 완화: (1) W14-17 은 Windows + Vulkan 만 (Linux 별도 Phase G) (2) cross-platform target 결정 시점에 별도 박제

### 7.8 R-W15-2: Phantom 인터페이스 의존 (신규)
- 시나리오: IRHISwapChain, IRHICommandList, eRHIResourceState, RHIBufferDesc 등이 RH-1~RH-4 에서 정의 예정이나 아직 미존재. Vulkan 클래스가 이 인터페이스에 의존하면 컴파일 실패.
- 완화: (1) 본 계획서에서 Vulkan 클래스를 standalone 으로 구현 (인터페이스 상속 없음) (2) RH-1~RH-4 완료 후 `public IRHISwapChain` 상속 추가하는 별도 작업 (3) IRHIDevice 만 현재 존재 (GetBackend + GetNativeHandle 2개) — CVulkanDevice 는 이것만 override.

### 7.9 R-W17-3: m_pDevice 타입 변경 (신규)
- 시나리오: 현재 CEngineApp::m_pDevice 는 `unique_ptr<CDX11Device>`. Vulkan 분기를 추가하면 `unique_ptr<IRHIDevice>` 로 변경 필요. 이 변경은 CEngineApp 내부에서 CDX11Device 고유 메서드 (GetDevice(), GetContext(), GetBackRTV(), GetDSV(), BeginFrame(), EndFrame()) 를 직접 호출하는 모든 곳에 영향.
- 완화: (1) W17 진입 전 CEngineApp 내 CDX11Device 직접 호출 지점 식별 (grep `m_pDevice->Get`) (2) DX11 전용 호출은 `#if !defined(WINTERS_RHI_BACKEND_VULKAN)` 로 가드 또는 IRHIDevice 인터페이스 확장 후 virtual dispatch

---

## 8. Week 14-17 통합 합격 검증

```bash
# 1. Vulkan SDK 확인
vulkaninfo --summary    # Instance Version 1.3+

# 2. Vulkan 디렉토리 + 14 파일
ls Engine/Private/RHI/Vulkan/{VulkanDevice,VulkanSwapChain,VulkanQueue,VulkanBuffer,VulkanImage,VulkanShaderModule,VulkanSampler,VulkanCommandBuffer,VulkanDescriptor,VulkanRenderPass,VulkanPipeline,VulkanBindGroup,VulkanPipelineCache,VulkanVMA}.{h,cpp}

# 3. DXC SPIR-V 컴파일 산출물
ls Shaders/*.spv | wc -l  # 모든 셰이더 .spv 존재

# 4. 빌드 통과
MSBuild Winters.sln /p:Configuration=Debug-VK /p:Platform=x64

# 5. Validation 0
# NOTE: --rhi=Vulkan 파서는 W17 신설. Debug-VK 빌드가 Vulkan 경로를 타므로 직접 실행.
WintersGame.exe
# 기대: ERROR 0, WARN 0

# 6. Frame diff (DX11 vs Vulkan)
# NOTE: PixCompare.ps1 미존재 시 RenderDoc 수동 비교
.\Tools\PixCompare.ps1 -Backend Vulkan
# 기대: mean<1, max<5

# 7. LoL 게임 1 매치 (DX11 / Vulkan 2 backend)
# 기대: 매치 정상 진행 + 회귀 0
```

---

## 9. 부록 A — Week 14-17 진입 체크리스트

```
[ ] Week 10-13 결과 검증 (DX12 visual parity 합격)
[ ] Vulkan SDK 1.3.x 설치 확인 (LunarG)
[ ] git: feature/2026-05-14-week14-rh6-vulkan branch

Week 14 — Foundation:
[ ] §3.1 Vulkan SDK 환경 변수 + $(VULKAN_SDK) 경로 참조
[ ] §3.2 VMA single-header 편입 (ThirdPartyLib/VMA/)
[ ] §3.3 DXC SPIR-V 빌드 스크립트 (CompileShaders_Vulkan.ps1)
[ ] §3.4 Engine.sln Debug-VK / Release-VK 컨피그 (WINTERS_ENGINE_EXPORTS 포함)
[ ] §3.5 eNativeHandleType Vulkan 엔트리 추가
[ ] §3.6 Validation layer 사전 체크 + fallback (#ifdef _DEBUG)

Week 15 — Device + SwapChain + Resource:
[ ] §4.1 CVulkanDevice (standalone, IRHIDevice 2메서드 override)
[ ] §4.2 CVulkanSwapChain (standalone, HWND 직접)
[ ] CVulkanQueue (Graphics + Compute + Transfer)
[ ] §4.3 CVulkanBuffer (VulkanBufferDesc 로컬 struct)
[ ] CVulkanImage (VMA + VkImage + ImageView)
[ ] CVulkanShaderModule + CVulkanSampler

Week 16 — CommandBuffer + Pipeline:
[ ] §5.1 CVulkanCommandBuffer (standalone, Vulkan native 타입)
[ ] DescriptorPool / SetLayout / Set 관리
[ ] CVulkanRenderPass (VkRenderPass + Framebuffer)
[ ] CVulkanPipeline (Graphics + Compute)
[ ] CVulkanBindGroup
[ ] §5.2 Image layout transition 자동 (COLOR_ATTACHMENT -> PRESENT_SRC 포함)
[ ] §5.3 CVulkanPipelineCache (파일 I/O 인라인)

Week 17 — 통합:
[ ] §6.1 CEngineApp::Initialize() backend 선택
       - m_pDevice 타입 unique_ptr<CDX11Device> -> unique_ptr<IRHIDevice> 변경
       - CDX11Device 직접 호출 지점 가드
[ ] §6.2 셰이더 .spv 컴파일 자동화
[ ] §6.3 Validation 0 error/warning
[ ] §6.4 Frame diff 측정
[ ] §6.5 LoL 게임 1 매치 회귀 0

검증:
[ ] §8.1 vulkaninfo OK
[ ] §8.2 14 파일 신설
[ ] §8.4 Debug-VK 빌드 통과
[ ] §8.5 Validation 0
[ ] §8.6 Frame diff mean<1
[ ] §8.7 게임 1 매치 회귀 0
```

---

## 10. 한 줄

> **Week 14-17 (선택) = backend-agnostic bootstrap + Vulkan SDK gate + VMA implementation TU + `Engine/Private/RHI/Vulkan/` backend files. 합격도 두 단계로 나눈다: 먼저 VK build + validation + deterministic parity, 그다음 gameplay parity. Vulkan 클래스는 현재 IRHIDevice(2메서드) 만 상속하고 나머지는 standalone — RH-1~RH-4 인터페이스 완료 후 상속 추가.**

---

## 끝.
