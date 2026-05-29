# RHI Multi-Platform Expansion Plan

> 2026-05-25 update: standalone backend smoke host projects are retired. DX12/Vulkan/Console validation should go through Engine/Client configurations, runtime flags, or in-project smoke functions, not separate `*SmokeHost.vcxproj` executables.

**작성일**: 2026-05-07
**대상**: DX11 Legacy -> DX12 / Vulkan / Mobile / Console(PS5, Xbox) 통합 RHI
**상위 문서**: `00_RHI_MIGRATION_MASTER.md`, `DX12_BOOTSTRAP_PLAN.md`
**현재 결론**: 기존 활성 RHI 계획은 DX11 -> DX12 -> Vulkan까지는 반영되어 있지만, Mobile / Console은 실행 계획으로는 부족하다. 이 문서는 RH-7 이후 플랫폼 확장 로드맵을 추가한다.

---

## 0. 현재 문서 반영 여부

| 범위 | 현재 반영 상태 | 근거 | 판정 |
|---|---|---|---|
| DX11 Legacy 유지 | 반영됨 | `00_RHI_MIGRATION_MASTER.md`의 RH-0~RH-4가 DX11 leak 제거 + DX11 emulation을 전제로 함 | 진행 가능 |
| DX12 PC backend | 반영됨 | `DX12_BOOTSTRAP_PLAN.md`, `06_RHI_PHASE_5_DX12_BACKEND.md`, `Engine/Private/RHI/DX12/*` | 진행 가능 |
| Vulkan PC backend | 반영됨 | `07_RHI_PHASE_6_VULKAN_BACKEND.md` | 계획 있음 |
| Android Vulkan | 일부 반영 | Vulkan 문서에 Android surface 언급은 있으나 lifecycle/build/asset/shader/mobile budget 계획 부족 | 보강 필요 |
| iOS / Metal | 거의 미반영 | MoltenVK/macOS 언급 수준. iOS Metal/MoltenVK 결정 gate 없음 | 신규 필요 |
| Xbox | 미반영 | 활성 RHI phase에 Xbox/GDK/DX12-family backend 분리 계획 없음 | 신규 필요 |
| PS5 | 미반영 | 활성 RHI phase에 proprietary console backend 격리 계획 없음 | 신규 필요 |
| 플랫폼 추상화 | 일부 반영 | `RHIWindowHandle`은 `void* nativeWindow`만 보유. platform type/lifecycle/capability 없음 | RH-7 필요 |

현재 코드 기준도 같은 결론이다.

- `Engine/Include/EngineConfig.h`의 `eEngineRHIBackend`는 `DX11 / DX12 / Vulkan`까지만 있다.
- `Engine/Public/RHI/RHITypes.h`의 `eRHIBackend`도 `DX11 / DX12 / Vulkan`까지만 있다.
- `RHIWindowHandle`은 `void* nativeWindow` + 크기/vsync/fullscreen만 있고, Win32/Android/iOS/Console surface 종류를 구분하지 않는다.
- `eNativeHandleType`은 DX11/DX12 native handle만 열거하고 Vulkan/Mobile/Console handle type이 없다.

따라서 RH-0~RH-6은 "core RHI migration"으로 유지하고, RH-7+를 "platform expansion"으로 별도 진행한다.

---

## 1. 목표

Winters RHI의 최종 목표는 다음과 같다.

```
Renderer / Resource / ImGui / Game
        |
        v
Public RHI Interfaces
        |
        +-- DX11 Legacy Backend        Windows compatibility / 교육용 legacy
        +-- DX12 Backend               Windows PC / high-end baseline
        +-- Vulkan Backend             Windows / Linux / Android baseline
        +-- Mobile Backend Layer        Android Vulkan first, iOS Metal decision gate
        +-- Xbox Backend               DX12-family, GDK-specific code isolated
        +-- PS5 Backend                proprietary API, SDK/NDA code isolated
```

핵심 원칙:

1. Public RHI는 DX12/Vulkan/Console의 explicit model을 기준으로 설계한다.
2. DX11은 설계 기준이 아니라 legacy compatibility backend로 둔다.
3. Mobile/Console SDK 타입은 `Engine/Public`에 절대 노출하지 않는다.
4. Xbox/PS5 구현은 NDA 영역이므로 공개 문서/공개 헤더에는 추상 정책과 stub만 둔다.
5. HLSL single source를 우선 유지하되, backend별 shader product를 asset pipeline에서 명시적으로 생성한다.
6. 플랫폼 차이는 RHI backend 내부가 아니라 `Platform` layer와 `RHICapabilities`로 먼저 표현한다.

---

## 2. 비목표 / 금지

- PS5 SDK, Xbox GDK proprietary header/type/function name을 public 문서나 `Engine/Public`에 박제하지 않는다.
- RHI를 DX11 최저공통분모로 낮추지 않는다.
- Android/iOS를 "PC Vulkan/Metal이 그냥 돈다"로 취급하지 않는다. surface loss, suspend/resume, tile memory, thermal budget을 별도 합격 기준으로 둔다.
- Console backend를 Windows DX12 backend에 조건부 `#ifdef`로 뒤섞지 않는다.
- Mobile/Console config를 전체 솔루션 Build.0에 바로 넣지 않는다. Engine + platform smoke host부터 독립 검증한다.
- console 실기 최적화 정보를 공개 자료 추측으로 구현하지 않는다. SDK 확보 전에는 stub + capability contract까지만 진행한다.

---

## 3. 목표 디렉토리 구조

```text
Engine/
  Include/
    EngineConfig.h
  Public/
    RHI/
      IRHIDevice.h
      IRHISwapChain.h
      IRHICommandList.h
      RHITypes.h
      RHIDescriptors.h
      RHICapabilities.h          (RH-7 신규)
      RHISurface.h               (RH-7 신규)
    Platform/
      IPlatformWindow.h          (RH-7 신규)
      IPlatformSurface.h         (RH-7 신규)
      PlatformTypes.h            (RH-7 신규)
  Private/
    Platform/
      Win32/
      Android/
      IOS/
      Xbox/                      (SDK 보유 시 private/NDA 영역)
      PS5/                       (SDK 보유 시 private/NDA 영역)
    RHI/
      DX11/
      DX12/
      Vulkan/
      Metal/                     (iOS native Metal 선택 시)
      Xbox/                      (DX12-family wrapper, GDK 격리)
      PS5/                       (proprietary backend, SDK 격리)
Tools/
  DX12SmokeHost/
  VulkanSmokeHost/               (RH-6)
  AndroidVulkanSmokeHost/        (RH-9)
  ConsoleSmokeHost/              (RH-10, stub 우선)
```

중요한 경계:

- `Engine/Public/RHI`는 renderer가 보는 유일한 그래픽 API다.
- `Engine/Public/Platform`은 window/surface/lifecycle을 표현하되, native SDK 타입은 opaque handle로만 둔다.
- `Engine/Private/RHI/<Backend>`만 실제 DX11/DX12/Vulkan/Metal/Console API를 include한다.

---

## 4. Backend / Platform 구분

RHI backend와 platform backend는 다른 축이다.

| 축 | 예시 | 책임 |
|---|---|---|
| RHI backend | DX11, DX12, Vulkan, Metal, Xbox, PS5 | device, queue, command list, resource, shader, swapchain |
| Platform backend | Win32, Android, iOS, Xbox OS, PS5 OS | window, surface, filesystem, input, lifecycle, timer, thread naming |

같은 RHI라도 platform surface가 다르다.

| 조합 | RHI | Surface |
|---|---|---|
| Windows DX12 | DX12 | HWND / DXGI swapchain |
| Windows Vulkan | Vulkan | Win32 surface |
| Android Vulkan | Vulkan | ANativeWindow surface |
| iOS Metal | Metal | CAMetalLayer |
| Xbox | Xbox/DX12-family | GDK-specific surface/swapchain |
| PS5 | PS5 proprietary | SDK-specific display/swapchain |

따라서 `RHIWindowHandle`은 RH-7에서 `RHISurfaceDesc`로 승격한다.

```cpp
enum class eRHIPlatformSurfaceType : u32_t
{
    Unknown = 0,
    Win32HWND,
    AndroidNativeWindow,
    IOSMetalLayer,
    XboxNative,
    PS5Native,
};

struct RHISurfaceDesc
{
    eRHIPlatformSurfaceType type = eRHIPlatformSurfaceType::Unknown;
    void* nativeHandle = nullptr;
    u32_t width = 0;
    u32_t height = 0;
    bool_t vsync = true;
    bool_t fullscreen = false;
};
```

주의: 위 코드는 방향성이다. 실제 반영 시 `RHIWindowHandle`과 호환 shim을 두고 단계적으로 교체한다.

---

## 5. Capability System

Mobile/Console을 RHI에 붙이려면 backend 이름보다 capability가 먼저 필요하다.

### 5.1 신규 타입

`Engine/Public/RHI/RHICapabilities.h`

```cpp
enum class eRHIFeatureTier : u32_t
{
    LegacyDX11 = 0,
    ExplicitDesktop,
    MobileTiled,
    Console,
};

struct RHICapabilities
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
    bool_t prefersRenderPassLoadStore = false;
    bool_t isTileBasedGPU = false;

    u32_t maxFramesInFlight = 2;
    u32_t maxColorAttachments = 8;
    u32_t constantBufferAlignment = 256;
    u32_t textureUploadAlignment = 256;
};
```

### 5.2 Feature policy

| 기능 | DX11 | DX12 | Vulkan Desktop | Android Vulkan | iOS Metal | Xbox | PS5 |
|---|---:|---:|---:|---:|---:|---:|---:|
| Explicit command list | Emulated | Yes | Yes | Yes | Yes | Yes | Yes |
| RenderPass load/store 중요도 | Low | Medium | High | Very high | Very high | High | High |
| Bindless | No/limited | Optional | Optional | Limited | Optional | Platform-specific | Platform-specific |
| Async compute | No | Optional | Optional | Device-specific | Device-specific | Yes/optional | Yes/optional |
| Ray tracing | No | Optional | Optional | No/limited | Limited | Optional | Optional |
| Tile memory 최적화 | No | No | No | Yes | Yes | No | Platform-specific |

Renderer는 `backend == DX12` 같은 분기를 최소화하고, `supportsBindless`, `isTileBasedGPU`, `prefersRenderPassLoadStore` 같은 capability로 경로를 선택한다.

---

## 6. Shader / Asset Matrix

Winters는 HLSL single source를 기본으로 유지한다. 단, 산출물은 backend/platform별로 분리한다.

| Target | Source | Compiler | Output | 비고 |
|---|---|---|---|---|
| DX11 Legacy | HLSL | DXC or D3DCompile fallback | DXBC or DXIL-compatible path | 기존 DX11 유지 구간 |
| DX12 PC | HLSL | DXC | DXIL | root signature / register space 명시 |
| Vulkan PC | HLSL | DXC `-spirv` | SPIR-V | descriptor set/space mapping 필요 |
| Android Vulkan | HLSL | DXC `-spirv` | SPIR-V | mobile profile + precision/bandwidth 제약 |
| iOS Metal | HLSL | DXC -> SPIR-V -> MSL or native Metal path | MSL/metallib | RH-9 decision gate |
| Xbox | HLSL | platform toolchain | platform shader binary | SDK/NDA 영역 |
| PS5 | HLSL or platform shader source | platform toolchain | platform shader binary | SDK/NDA 영역 |

### 6.1 Shader binding 규약

- 모든 신규 HLSL은 `register(bN, spaceM)`, `register(tN, spaceM)`, `register(sN, spaceM)`를 명시한다.
- `space0`은 frame/global, `space1`은 material, `space2`는 object, `space3`은 bindless/extended로 예약한다.
- Vulkan은 `space`를 descriptor set으로 매핑한다.
- DX12는 `space`를 root signature range로 매핑한다.
- Mobile은 dynamic indexing과 descriptor count 상한을 capability로 제한한다.

### 6.2 Texture format 정책

| Platform | 기본 압축 포맷 | fallback |
|---|---|---|
| Windows DX11/DX12 | BCn | RGBA8 |
| Vulkan Desktop | BCn if supported | RGBA8 |
| Android | ASTC 우선, ETC2 fallback | RGBA8 debug |
| iOS | ASTC 우선 | RGBA8 debug |
| Xbox | BCn / platform optimized | RGBA8 debug |
| PS5 | BCn / platform optimized | RGBA8 debug |

AssetConverter는 RH-8에서 `.wtex` 또는 texture manifest를 추가해 platform별 texture variant를 선택할 수 있어야 한다.

---

## 7. Phase Roadmap

### RH-7 Platform Surface & Lifecycle

목표: `RHIWindowHandle`을 platform-neutral surface로 확장하고, RHI가 Win32 HWND에 묶이지 않게 만든다.

작업:

- `Engine/Public/Platform/PlatformTypes.h` 추가
- `Engine/Public/Platform/IPlatformWindow.h` 추가
- `Engine/Public/Platform/IPlatformSurface.h` 추가
- `Engine/Public/RHI/RHISurface.h` 추가
- `RHIWindowHandle`은 deprecated shim으로 유지
- `IRHIDevice::CreateSwapChain(const RHIWindowHandle&)`를 `CreateSwapChain(const RHISurfaceDesc&)`로 단계 전환
- Win32 구현은 `Engine/Private/Platform/Win32/*`로 격리
- Android/iOS/Xbox/PS5 surface type은 public enum + opaque pointer까지만 둔다

합격 기준:

- `Engine/Public/RHI`에서 `Windows.h`, `HWND`, `HINSTANCE` 직접 노출 0건
- Windows DX11/DX12 smoke host가 기존과 동일하게 실행
- Android/iOS/Console surface type은 SDK 없이도 컴파일 가능한 stub 상태

### RH-8 Shader & Asset Cross-Compile Pipeline

목표: DX12/Vulkan/Mobile/Console shader 산출물을 한 asset manifest에서 관리한다.

작업:

- `ShaderCompiler`에 target triple 추가
- `eRHIShaderTarget` 추가: `DX11`, `DX12`, `VulkanDesktop`, `VulkanAndroid`, `MetalIOS`, `Xbox`, `PS5`
- shader manifest 추가: source path, entry point, profile, defines, target outputs
- HLSL register space 규약 전수 점검
- SPIR-V reflection 결과를 `RHIBindGroupLayoutDesc`와 대조
- console target은 SDK 없을 때 stub manifest만 생성

합격 기준:

- 동일 HLSL에서 DX12 DXIL + Vulkan SPIR-V 산출물 생성
- descriptor set/space reflection mismatch 0건
- Mobile target은 unsupported feature를 명시적으로 실패 처리

### RH-9 Mobile RHI

목표: Android Vulkan을 1차 mobile target으로 붙이고, iOS는 Metal native vs MoltenVK를 decision gate로 둔다.

Android Vulkan 작업:

- `Engine/Private/Platform/Android/*` stub 추가
- `CVulkanDevice`가 Android surface path를 받을 수 있게 분리
- surface loss / resize / suspend / resume 처리 계획 추가
- transient attachment / load-store op 최적화
- ASTC/ETC2 texture variant 선택
- thermal budget용 dynamic resolution hook 추가

iOS decision gate:

- Option A: Metal native backend (`Engine/Private/RHI/Metal/*`)
- Option B: MoltenVK path로 Vulkan 유지
- 결정 기준: App Store 정책, MoltenVK feature gap, shader pipeline 비용, maintenance 비용

합격 기준:

- Android Vulkan smoke host가 clear color + triangle + texture sample까지 통과
- suspend/resume 후 swapchain 재생성 통과
- mobile profile에서 geometry shader/tessellation/desktop-only path 사용 0건
- iOS는 SDK 확보 전까지 stub + decision record까지만 유지

### RH-10 Console RHI

목표: Xbox와 PS5를 public RHI에 붙일 수 있는 구조를 만들되, proprietary 구현은 private/NDA 영역에 격리한다.

Xbox 방향:

- DX12-family backend로 보되 Windows `CDX12Device`와 직접 섞지 않는다.
- 공통 가능한 코드는 `RHI/DX12Common` 또는 내부 helper로 분리한다.
- GDK-specific device/swapchain/memory/event/PIX hook은 `RHI/Xbox`에 둔다.
- public enum은 `eRHIBackend::Xbox` 또는 `eRHIBackend::DX12` + platform capability 중 하나로 결정한다.

PS5 방향:

- PS5 backend는 별도 `RHI/PS5`로 둔다.
- Public RHI에는 PS5 SDK type을 절대 노출하지 않는다.
- SDK 미보유 상태에서는 compile stub + capability contract만 작성한다.
- 실구현은 SDK/EULA 확인 후 private repo 또는 guarded folder에서 진행한다.

합격 기준:

- SDK 없이 PC repo 전체가 컴파일 가능
- Xbox/PS5 backend folder는 `WINTERS_PLATFORM_XBOX` / `WINTERS_PLATFORM_PS5`가 없으면 빌드 제외
- Console smoke host는 stub backend selection까지 컴파일
- Console-specific code가 `Engine/Public`에 노출되지 않음

### RH-11 Build Matrix & CI

목표: backend/platform config가 서로의 산출물을 덮어쓰지 않게 하고, smoke host 중심으로 검증한다.

권장 config:

| Config | 대상 | Build.0 기본 |
|---|---|---|
| Debug | Windows DX11 legacy | Engine + Client |
| Release | Windows DX11 legacy | Engine + Client |
| Debug-DX12 | Windows DX12 | Engine + Client + DX12SmokeHost (2026-05-09 현재) |
| Release-DX12 | Windows DX12 | Engine + Client + DX12SmokeHost (2026-05-09 현재) |
| Debug-VK | Windows Vulkan | Engine only + VulkanSmokeHost |
| Release-VK | Windows Vulkan | Engine only + VulkanSmokeHost |
| Debug-Android-VK | Android Vulkan | Engine platform lib + Android smoke |
| Release-Android-VK | Android Vulkan | Engine platform lib + Android smoke |
| Debug-Xbox | Xbox | SDK-gated Engine + smoke |
| Release-Xbox | Xbox | SDK-gated Engine + smoke |
| Debug-PS5 | PS5 | SDK-gated Engine + smoke |
| Release-PS5 | PS5 | SDK-gated Engine + smoke |

규칙:

- Debug-DX12는 2026-05-09 현재 Client도 Build.0에 묶여 있다. VK/Console은 smoke host 중심으로 시작하고, DX12도 Client를 계속 묶을지 다시 정책 결정한다.
- SmokeHost는 해당 backend DLL을 명시적으로 로드하고 로그에 backend 이름을 출력한다.
- 산출물은 `Engine/Bin/<Config>/`와 `Tools/<Host>/Bin/<Config>/`로 분리한다.
- root 병렬 빌드 시 vc143.pdb lock 회피를 위해 config별 중간 디렉토리를 분리한다.

### RH-12 Visual Parity & Performance Gate

목표: backend가 늘어도 "같은 장면이 같은 의미로 렌더링된다"를 자동 검증한다.

작업:

- Frame capture golden image 도입
- Backend별 clear/triangle/texture/depth/blend/skinning/FX test scene 분리
- GPU timing query 추상화
- RenderDoc/PIX/validation layer capture hook 정리
- Mobile/Console은 device-specific tolerance를 둔다

합격 기준:

- DX11 vs DX12 frame diff 기준 통과
- DX12 vs Vulkan frame diff 기준 통과
- Mobile은 resolution/tone/precision tolerance를 별도 적용
- Backend별 GPU time regression threshold 기록

---

## 8. 현재 코드베이스 기준 선행 Blocker

| 우선 | 위치 | 문제 | 필요한 조치 |
|---|---|---|---|
| P0 | `Engine/Public/RHI/RHITypes.h` | `eRHIBackend`가 DX11/DX12/Vulkan까지만 있음 | RH-7/RH-10에서 backend/platform enum 정책 결정 |
| P0 | `Engine/Public/RHI/RHITypes.h` | `RHIWindowHandle`이 surface type 없이 `void* nativeWindow`만 보유 | `RHISurfaceDesc` + deprecated shim 도입 |
| P0 | `Engine/Public/RHI/RHITypes.h` | `eNativeHandleType`이 DX11/DX12만 표현 | Vulkan/Metal/Console native type 또는 backend-private query 정책 결정 |
| P1 | `Engine/Include/EngineConfig.h` | `eEngineRHIBackend`가 Mobile/Console을 표현하지 않음 | backend와 platform을 분리하거나 enum 확장 |
| P1 | `Engine/Private/RHI/DX12/*` | Windows DX12와 Xbox DX12-family 공통/분리 경계 없음 | `DX12Common` 분리 여부를 RH-10에서 결정 |
| P1 | `Engine/Public/RHI/RHIDescriptors.h` | texture format/usage가 mobile compression과 transient attachment를 표현하기 부족 | RH-8/RH-9에서 format/usage 확장 |
| P2 | `Tools/` | DX12 smoke host만 있음 | Vulkan/Mobile/Console smoke host 단계 추가 |

---

## 9. Backend별 구현 방향

### 9.1 DX11 Legacy

- 목적: 현재 LoL target 안정성, 교육용 DX11 유지, legacy fallback.
- CommandList/RenderPass/Barrier는 no-op/emulation으로 둔다.
- 신규 기능은 DX11 기준으로 제한하지 않는다.
- 최종 상태는 `Engine/Private/RHI/DX11`에 격리한다.

### 9.2 DX12 PC

- 목적: Windows high-end baseline.
- Descriptor heap, root signature, PSO cache, resource barrier를 표준 explicit path로 둔다.
- Xbox와 공유 가능한 코드는 helper화하되, GDK type은 섞지 않는다.

### 9.3 Vulkan Desktop

- 목적: Windows/Linux/Steam Deck 가능성.
- RenderPass/load-store, descriptor set, pipeline cache를 명시한다.
- DXC SPIR-V와 validation layer를 기본 gate로 둔다.

### 9.4 Android Vulkan

- 목적: Mobile 1차 target.
- Vulkan backend를 재사용하되 surface/lifecycle/memory/texture profile은 별도 처리한다.
- tile-based GPU에서 render pass load/store와 transient attachment가 성능 기준이다.

### 9.5 iOS Metal

- 목적: Mobile 2차 target.
- Metal native와 MoltenVK 중 하나를 RH-9 decision gate에서 선택한다.
- iOS를 장기 target으로 확정하면 `RHI/Metal`을 별도 backend로 두는 쪽이 유지보수상 안전하다.

### 9.6 Xbox

- 목적: Console target 1.
- DX12-family conceptual model을 활용한다.
- Windows `CDX12Device`를 그대로 재사용한다고 가정하지 않는다.
- GDK integration은 SDK/NDA guard 아래 private 구현으로 둔다.

### 9.7 PS5

- 목적: Console target 2.
- public RHI contract만 먼저 맞춘다.
- proprietary graphics API 구현은 SDK 확보 후 별도 gated backend에서 진행한다.
- 공개 문서에는 SDK 함수명/세부 behavior를 적지 않는다.

---

## 10. 결정이 필요한 지점

| ID | 결정 | 선택지 | 권장 |
|---|---|---|---|
| D-1 | Backend enum에 Xbox/PS5를 넣을지 | A: `eRHIBackend::Xbox/PS5` 추가, B: DX12/Vulkan/Metal + platform capability로 표현 | A 권장. 로그/telemetry/smoke host가 명확함 |
| D-2 | iOS 경로 | A: Metal native, B: MoltenVK | 장기적으로 A, 단기 조사 gate 필요 |
| D-3 | Shader source | A: HLSL single source, B: backend별 shader source | A 권장. AssetConverter에서 산출물 분기 |
| D-4 | Console 코드 위치 | A: same repo guarded folder, B: private repo/submodule | SDK/EULA 확인 전 B 또는 guarded stub 권장 |
| D-5 | RHI native handle query | A: public enum 확장, B: backend-private debug bridge만 허용 | B 권장. native escape hatch 최소화 |
| D-6 | Mobile renderer feature set | A: desktop feature fallback, B: mobile-specific render path | B 권장. tile/bandwidth 때문에 별도 path 필요 |

---

## 11. 권장 진입 순서

1. RH-0~RH-4를 완료해 Public DX11 leak를 제거한다.
2. DX12 bootstrap을 안정화한다. 현재 `DX12_BOOTSTRAP_PLAN.md`의 M0~M3가 여기에 해당한다.
3. RH-7을 먼저 진행해 `RHISurfaceDesc`와 platform lifecycle을 만든다.
4. RH-8로 shader/asset matrix를 만든다.
5. RH-6 Vulkan PC를 compile-only -> visual parity까지 끌어올린다.
6. RH-9 Android Vulkan smoke를 붙인다.
7. iOS Metal/MoltenVK decision gate를 닫는다.
8. RH-10 Console stub을 추가한다.
9. Xbox/PS5 SDK 확보 후 guarded backend를 실구현한다.
10. RH-12 visual parity/perf gate로 backend별 regression을 관리한다.

---

## 12. 최소 산출물 체크리스트

RH-7:

- [ ] `RHISurface.h`
- [ ] `RHICapabilities.h`
- [ ] `PlatformTypes.h`
- [ ] `IPlatformWindow.h`
- [ ] `IPlatformSurface.h`
- [ ] `RHIWindowHandle` deprecated shim
- [ ] Win32 smoke 유지

RH-8:

- [ ] shader target enum
- [ ] shader manifest
- [ ] DXIL + SPIR-V build path
- [ ] register space convention audit
- [ ] texture variant manifest

RH-9:

- [ ] Android Vulkan platform stub
- [ ] Android surface/swapchain path
- [ ] suspend/resume/surface loss test
- [ ] ASTC/ETC2 selection
- [ ] iOS decision record

RH-10:

- [ ] Xbox backend stub
- [ ] PS5 backend stub
- [ ] SDK guard macros
- [ ] no SDK type in `Engine/Public`
- [ ] console smoke host compile gate

RH-11:

- [ ] config matrix
- [ ] Build.0 policy
- [ ] per-config output/intermediate directory
- [ ] smoke host scripts

RH-12:

- [ ] frame diff harness
- [ ] clear/triangle/texture/depth/blend/skinning/FX scenes
- [ ] GPU timing query abstraction
- [ ] validation/capture hooks

---

## 13. 요약

현재 활성 RHI 계획은 DX11 Legacy -> DX12 -> Vulkan까지를 다룬다. Mobile/Console은 비전에는 있었지만 active implementation plan으로는 부족했다. 앞으로는 RH-7 Platform Surface, RH-8 Shader/Asset Matrix, RH-9 Mobile, RH-10 Console, RH-11 Build Matrix, RH-12 Parity Gate를 추가해 확장한다.

가장 중요한 첫 수정은 `RHIWindowHandle`을 `RHISurfaceDesc`로 승격하는 것이다. 이 지점이 정리되어야 Android/iOS/Xbox/PS5가 public RHI를 오염시키지 않고 들어올 수 있다.
