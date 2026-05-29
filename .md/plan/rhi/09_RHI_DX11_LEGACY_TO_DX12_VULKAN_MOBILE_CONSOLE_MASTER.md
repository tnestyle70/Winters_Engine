# RHI Migration Master: DX11 Legacy -> DX12 / Vulkan / Mobile / Console

> 2026-05-25 update: `Tools/DX12SmokeHost` and its `Winters.sln` project registration were removed. DX12 is now treated only as an RHI backend path inside `WintersEngine.dll` / `WintersGame.exe`; historical SmokeHost mentions below are archive/context, not the active plan.

**작성일**: 2026-05-07
**상태**: active implementation master
**대상 코드**: `Engine/Public/RHI`, `Engine/Private/RHI`, `Engine/Public/Platform`, `Engine/Private/Platform`, `Shaders`, `Tools/*SmokeHost`
**상위 문서**: `00_RHI_MIGRATION_MASTER.md`, `DX12_BOOTSTRAP_PLAN.md`, `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md`
**한 줄 결론**: Winters RHI는 DX11을 legacy reference backend로 유지하고, DX12/Vulkan/Console의 explicit rendering model을 public contract의 기준으로 삼는다. Mobile/Console은 backend 문제가 아니라 platform surface, lifecycle, capability, asset/shader matrix 문제까지 같이 풀어야 한다.

---

## 0. 현재 코드 기준선

### 0.1 이미 존재하는 것

```txt
Engine/Public/RHI/
  IRHIDevice.h
  IRHIQueue.h
  IRHICommandList.h
  IRHISwapChain.h
  IRHIPipelineState.h
  IRHIRenderPass.h
  IRHIBindGroupLayout.h
  IRHIBindGroup.h
  RHIHandles.h
  RHITypes.h
  RHIDescriptors.h
  CRHIResourceTable.h
  ShaderCompiler.h

Engine/Private/RHI/DX11/
  현행 실행 backend. LoL scene 실제 렌더링 기준선.

Engine/Private/RHI/DX12/
  28 파일 scaffold.
  CDX12Device, CDX12Queue, CDX12SwapChain, DX12CommandList, descriptor/root/pso/buffer/texture skeleton.

Engine/ThirdPartyLib/D3D12MA/
  Inc/D3D12MemAlloc.h
  Src/D3D12MemAlloc.cpp

Tools/DX12SmokeHost/
  WintersRun + IWintersApp 기반 DX12 smoke host 프로젝트.
```

### 0.2 현재 중요한 제약

```txt
1. DX12 선택은 런타임 EngineConfig가 아니라 WINTERS_RHI_BACKEND_DX12 compile macro가 실제 결정한다.
2. EngineConfig::rhiBackend는 의도 표기 필드에 가깝다.
3. Public DX11 header leak가 남아 있다.
4. Renderer/Resource 구현부는 IRHIDevice*를 받더라도 내부에서 DX11 native handle을 추출한다.
5. DX12CommandList의 draw 관련 바인딩 함수 상당수는 stub이다.
6. Vulkan backend directory는 아직 없다.
7. Mobile/Console은 RHISurface/lifecycle/capability public contract 초안이 생겼지만, backend별 real adapter와 lifecycle event path는 아직 없다.
8. HLSL register(..., space0) 규약이 아직 전수 반영되지 않았다.
9. UpdateLib.bat은 Debug/Release만 배포하고 Debug-DX12 배포는 smoke host 우회가 현재 안전하다.
10. 2026-05-09 현재 `Winters.sln`에는 `DX12SmokeHost` 프로젝트가 포함되어 있다.
```

### 0.3 이 문서의 판정

기존 RH-0~RH-6 문서는 core RHI migration으로 유지한다. 이 문서는 그 문서들을 현재 코드 기준으로 묶고, RH-7 이후 platform/mobile/console expansion까지 한 실행 순서로 정리한다.

---

## 1. 목표 아키텍처

```txt
Client / Game
  |
  v
Renderer / RenderGraph / Resource / UI Adapter
  |
  v
Public RHI Contract
  - IRHIDevice
  - IRHIQueue
  - IRHICommandList
  - IRHISwapChain
  - IRHIPipelineState
  - IRHIRenderPass
  - IRHIBindGroupLayout
  - IRHIBindGroup
  - RHIHandles
  - RHISurfaceDesc
  - RHICapabilities
  |
  +-- DX11 Legacy backend
  +-- DX12 Windows backend
  +-- Vulkan desktop backend
  +-- Vulkan Android backend
  +-- Metal iOS backend or MoltenVK decision
  +-- Xbox backend, SDK-gated
  +-- PS5 backend, SDK-gated

Public Platform Contract
  - IPlatformWindow
  - IPlatformSurface
  - PlatformTypes
  - lifecycle state
  |
  +-- Win32
  +-- Android
  +-- iOS
  +-- Xbox
  +-- PS5
```

### 1.1 Backend와 Platform은 다른 축이다

| 축 | 예시 | 책임 |
|---|---|---|
| RHI backend | DX11, DX12, Vulkan, Metal, Xbox, PS5 | device, queue, command list, resource, shader, swapchain |
| Platform backend | Win32, Android, iOS, Xbox OS, PS5 OS | window, surface, filesystem, input, lifecycle, timers, thread naming |

같은 Vulkan이라도 Windows Vulkan과 Android Vulkan은 surface/lifecycle이 다르다. 같은 DX12 계열이라도 Windows DX12와 Xbox는 SDK와 swapchain/memory/debug tooling이 다르다.

### 1.2 Public API 원칙

```txt
Engine/Public/RHI:
  Opaque handle / desc / interface만 둔다.
  d3d11.h, d3d12.h, vulkan.h, Metal, Xbox GDK, PS5 SDK 타입 금지.

Engine/Public/Platform:
  platform 종류와 opaque native handle만 둔다.
  HWND 같은 Win32 타입은 가능하면 CWin32Window 내부로 격리한다.

Engine/Private/RHI/<Backend>:
  실제 graphics API header include 가능.

Engine/Private/Platform/<Platform>:
  실제 OS/window/surface API include 가능.
```

---

## 2. Backend 정책

### 2.1 DX11 Legacy

DX11은 제거하지 않는다. 그러나 더 이상 설계 기준 backend가 아니다.

유지 목적:

```txt
1. 현재 LoL scene 안정 실행.
2. DX12/Vulkan 회귀 비교용 reference path.
3. 학원 DX11 수업 구조 흡수 검증.
4. 저사양 Windows fallback.
```

금지:

```txt
1. 새 Renderer/Client 코드가 ID3D11*를 직접 보게 하지 않는다.
2. 새 RHI 기능을 DX11 native API로 먼저 설계하지 않는다.
3. DX11에서만 성공하고 DX12/Vulkan에서 표현 불가능한 public contract를 추가하지 않는다.
```

최종 상태:

```txt
Engine/Private/RHI/DX11 only.
Public DX11 headers 0.
DX11 barrier/renderpass/commandlist는 explicit model의 no-op/emulation backend.
```

### 2.2 DX12 Windows Main

DX12는 Windows PC 메인 backend다.

구현 축:

```txt
1. Device/factory/adapter/queue/swapchain bootstrap.
2. Frame ring: allocator/list/fence per frame.
3. Descriptor heap: RTV/DSV/CBV/SRV/UAV/Sampler.
4. Root signature from RHIBindGroupLayout.
5. PipelineState from RHIPipelineDesc.
6. Resource allocation through D3D12MA.
7. Explicit resource barriers.
8. Texture/buffer upload path.
9. ImGui DX12 backend.
10. RenderGraph execution.
11. Mesh3D/Skinned3D/FX parity.
12. GPU timing/capture/validation.
```

주의:

```txt
Windows CDX12Device를 Xbox에 그대로 쓴다고 가정하지 않는다.
공유 가능한 helper는 DX12Common으로 분리하고, GDK/PIX/swapchain 차이는 Xbox backend에 둔다.
```

### 2.3 Vulkan Desktop Main

Vulkan은 cross-platform main backend다.

구현 축:

```txt
1. Instance/device/surface/swapchain.
2. Validation layer availability check.
3. Queue family selection.
4. Command pool/list/fence/semaphore.
5. VMA allocator.
6. Descriptor set layout = RHIBindGroupLayout.
7. PipelineLayout = BindGroupLayout collection.
8. Render pass or dynamic rendering policy.
9. SPIR-V shader pipeline through DXC.
10. Pipeline cache.
11. Mesh3D/Skinned3D/FX parity.
```

Vulkan은 Android와 공유하되, surface/lifecycle/memory budget은 platform path로 분리한다.

### 2.4 Android Vulkan Mobile

Android는 Mobile 1차 target이다.

추가로 필요한 것:

```txt
1. ANativeWindow surface adapter.
2. Surface lost / suspend / resume handling.
3. Swapchain recreation at lifecycle boundaries.
4. ASTC/ETC2 texture variant selection.
5. Tile-based GPU load/store policy.
6. Dynamic resolution hook.
7. Thermal/perf budget profile.
8. No desktop-only shader stage assumption.
```

Renderer policy:

```txt
deferred heavy path는 capability로 옵션화.
GTAO/SSAO는 mobile profile에서 low/disabled path 제공.
FX overdraw budget 필요.
```

### 2.5 iOS Metal 또는 MoltenVK

iOS는 결정 gate를 먼저 둔다.

선택지:

```txt
Option A: Native Metal backend.
  장점: iOS/tile GPU와 가장 자연스럽다.
  단점: MSL shader pipeline과 별도 backend 유지 비용.

Option B: MoltenVK.
  장점: Vulkan backend 재사용.
  단점: feature gap, App Store 정책, 디버깅/성능 튜닝 비용.
```

권장:

```txt
단기: iOS stub + decision record만 유지.
장기: iOS를 진짜 target으로 확정하면 native Metal이 유지보수상 더 안전하다.
```

### 2.6 Xbox

Xbox는 DX12-family conceptual model을 활용하지만 Windows DX12와 같은 backend로 취급하지 않는다.

정책:

```txt
1. Public RHI는 동일.
2. GDK-specific code는 Engine/Private/RHI/Xbox 또는 private repo.
3. Windows CDX12Device와 공통 코드는 DX12Common helper로 제한.
4. Xbox backend build는 WINTERS_PLATFORM_XBOX와 SDK 존재 여부로 gated.
5. SDK 없는 공개 repo에서는 stub backend selection까지만 컴파일.
```

### 2.7 PS5

PS5는 proprietary console backend다.

정책:

```txt
1. Public RHI에는 PS5 SDK 타입/function name을 쓰지 않는다.
2. Engine/Public에는 enum/capability/stub contract만 둔다.
3. 실제 구현은 SDK/EULA 확인 후 private branch 또는 guarded folder에서 진행한다.
4. 공개 문서에는 실기 최적화 추측을 구현 지시로 쓰지 않는다.
```

---

## 3. Shader / Asset 전략

### 3.1 Source policy

기본은 HLSL single source다.

| Target | Source | Compiler | Output |
|---|---|---|---|
| DX11 Legacy | HLSL | D3DCompile or DXC fallback | DXBC |
| DX12 Windows | HLSL | DXC | DXIL |
| Vulkan Desktop | HLSL | DXC `-spirv` | SPIR-V |
| Android Vulkan | HLSL | DXC `-spirv` | SPIR-V mobile profile |
| iOS Metal | HLSL | SPIR-V -> MSL or native path | MSL/metallib |
| Xbox | HLSL | platform toolchain | platform shader binary |
| PS5 | HLSL or platform source | platform toolchain | platform shader binary |

### 3.2 Binding convention

```txt
space0: frame/global resources
space1: material resources
space2: object resources
space3: bindless/extended resources
```

규칙:

```txt
1. 모든 신규 HLSL은 register(bN, spaceM), register(tN, spaceM), register(sN, spaceM)를 명시한다.
2. Vulkan은 space를 descriptor set으로 매핑한다.
3. DX12는 space를 root signature descriptor range로 매핑한다.
4. Mobile은 descriptor count와 dynamic indexing을 capability로 제한한다.
5. Shader reflection 결과와 RHIBindGroupLayoutDesc를 빌드 단계에서 대조한다.
```

### 3.3 Texture matrix

| Platform | Primary | Fallback |
|---|---|---|
| Windows DX11/DX12 | BCn | RGBA8 |
| Vulkan Desktop | BCn if supported | RGBA8 |
| Android | ASTC | ETC2 / RGBA8 debug |
| iOS | ASTC | RGBA8 debug |
| Xbox | BCn / platform optimized | RGBA8 debug |
| PS5 | BCn / platform optimized | RGBA8 debug |

AssetConverter는 `.wtex` 또는 texture manifest를 통해 platform별 variant를 고를 수 있어야 한다.

---

## 4. Build / Config 전략

### 4.1 현재 유지

```txt
Debug          Windows DX11 baseline
Release        Windows DX11 baseline
Debug-DX12     Engine DX12 config
Release-DX12   Engine DX12 config
```

### 4.2 확장 목표

| Config | 대상 | 기본 Build.0 |
|---|---|---|
| Debug | Windows DX11 | Engine + Client |
| Release | Windows DX11 | Engine + Client |
| Debug-DX12 | Windows DX12 | Engine + DX12SmokeHost |
| Release-DX12 | Windows DX12 | Engine + DX12SmokeHost |
| Debug-VK | Windows Vulkan | Engine + VulkanSmokeHost |
| Release-VK | Windows Vulkan | Engine + VulkanSmokeHost |
| Debug-Android-VK | Android Vulkan | Android platform lib + Android smoke |
| Release-Android-VK | Android Vulkan | Android platform lib + Android smoke |
| Debug-Xbox | Xbox | SDK-gated Engine + ConsoleSmokeHost |
| Release-Xbox | Xbox | SDK-gated Engine + ConsoleSmokeHost |
| Debug-PS5 | PS5 | SDK-gated Engine + ConsoleSmokeHost |
| Release-PS5 | PS5 | SDK-gated Engine + ConsoleSmokeHost |

규칙:

```txt
1. Backend smoke host는 해당 backend DLL을 같은 output folder에서 명시적으로 로드한다.
2. 2026-05-09 현재 DX12는 Client도 Build.0에 묶여 있다. Server/AssetConverter는 alias만 유지한다. VK/Console은 smoke host 중심으로 시작하고, DX12 Client Build.0 정책은 별도 결정한다.
3. EngineSDK/inc 복사 race를 피하기 위해 smoke host 중심으로 검증한다.
4. Config별 OutDir/IntDir를 분리한다.
5. Console config는 SDK가 없으면 stub compile까지만 수행한다.
```

---

## 5. Migration Phase

### RH-A: 현재 기준선 잠금

목표: 문서와 코드의 불일치를 더 늘리지 않는다.

작업:

```txt
1. Active plan table에 본 문서 추가.
2. Public DX11 leak inventory 갱신.
3. DX12SmokeHost 솔루션 등록은 완료. 다음 결정은 triangle/texture/depth/blend smoke 확장 범위다.
4. Debug-DX12 Engine build와 smoke host build를 별도 검증.
5. Shaders register space audit report 작성.
```

합격:

```txt
Debug DX11 build baseline 유지.
Debug-DX12 Engine compile-only 결과 기록.
RHI leak list가 문서에 최신화.
```

### RH-7: Platform Surface & Capabilities

목표: Mobile/Console을 붙이기 위한 public contract를 만든다.

작업:

```txt
1. Engine/Public/RHI/RHISurface.h 추가.
2. Engine/Public/RHI/RHICapabilities.h 추가.
3. Engine/Public/Platform/PlatformTypes.h 추가.
4. Engine/Public/Platform/IPlatformWindow.h 추가.
5. Engine/Public/Platform/IPlatformSurface.h 추가.
6. IRHIDevice에 RHISurfaceDesc 기반 CreateSwapChain shim 추가.
7. RHIWindowHandle은 deprecated compatibility shim으로 유지.
8. eRHIBackend/eEngineRHIBackend에 future backend slot 추가 여부 결정.
```

합격:

```txt
Public RHI가 Win32 HWND 없이 surface kind를 표현할 수 있다.
기존 DX11/DX12 path가 RHIWindowHandle로 그대로 컴파일된다.
Android/iOS/Xbox/PS5 surface type을 SDK 없이 표현할 수 있다.
```

### RH-8: Shader / Asset Matrix

목표: backend별 shader product와 texture product를 asset pipeline에서 관리한다.

작업:

```txt
1. eRHIShaderTarget 추가.
2. ShaderCompiler target triple 추가.
3. Shader manifest schema 작성.
4. DX12 DXIL / Vulkan SPIR-V output path 추가.
5. register space convention 전수 반영.
6. Reflection -> RHIBindGroupLayoutDesc mismatch checker 작성.
7. Texture variant manifest 설계.
```

합격:

```txt
동일 HLSL에서 DX12 DXIL + Vulkan SPIR-V 생성.
space/binding mismatch 0.
Mobile unsupported feature가 명시적으로 실패한다.
```

### RH-9: DX11 Public Leak 제거

목표: Public/Client가 DX11 native type을 보지 않게 한다.

작업:

```txt
1. Engine/Public/RHI/DX11/* 이동 준비.
2. CEngineApp.h에서 DX11Shader/DX11Pipeline/BlendStateCache 직접 include 제거.
3. UI_Manager.h의 ID3D11ShaderResourceView 노출 제거.
4. Renderer/Resource public headers에서 DX11 타입 제거.
5. Client direct include 목록 제거.
6. CGameInstance legacy getter를 backend-neutral getter로 단계 전환.
```

합격:

```txt
Engine/Public + Engine/Include + Client/Public grep:
  d3d11.h / ID3D11 / RHI/DX11 / CDX11Device.h = 0 hit
```

### RH-10: CommandList / RenderPass / BindGroup 실제화

목표: DX11과 DX12가 같은 explicit command model을 통과하게 한다.

작업:

```txt
1. IRHICommandList에 실제 draw binding path 확정.
2. DX11 immediate emulation 구현.
3. DX12 command list SetPipeline/SetBindGroup/SetVertexBuffer/SetIndexBuffer 구현.
4. Resource transition API를 buffer/texture 모두 handle 기반으로 정리.
5. RenderPass load/store semantics를 DX11 no-op, DX12/Vulkan explicit로 매핑.
```

합격:

```txt
Renderer가 ID3D11DeviceContext를 직접 보지 않고 draw 제출.
DX11 scene visual unchanged.
DX12 smoke triangle draw 가능.
```

### RH-11: DX12 Visual Parity

목표: Windows DX12가 LoL scene 렌더링을 따라잡는다.

작업:

```txt
1. Mesh3D / Skinned3D / FxSprite / FxMesh PSO 구현.
2. Constant buffer upload ring.
3. Texture upload + SRV descriptor path.
4. Depth buffer + RTV/DSV management.
5. ImGui DX12 backend.
6. NormalPass / SSAO path option 또는 defer.
7. Frame capture diff gate.
```

합격:

```txt
DX11 vs DX12 golden scene diff threshold 통과.
Irelia + map + FX minimum scene render.
DX12 debug layer major error 0.
```

### RH-12: Vulkan Desktop Compile & Triangle

목표: Vulkan backend가 Windows에서 clear/triangle까지 간다.

작업:

```txt
1. Engine/Private/RHI/Vulkan scaffold.
2. ThirdPartyLib/Vulkan or SDK detection policy.
3. VMA integration.
4. VulkanSmokeHost.
5. Win32 Vulkan surface.
6. SPIR-V shader path.
7. Validation layer check.
```

합격:

```txt
Debug-VK Engine + VulkanSmokeHost build.
Clear + triangle present.
Validation error 0.
```

### RH-13: Vulkan Visual Parity

목표: Vulkan이 DX12와 같은 RHI contract로 LoL scene을 렌더링한다.

작업:

```txt
1. Descriptor set allocation.
2. Pipeline cache.
3. Texture/buffer upload.
4. RenderPass/dynamic rendering decision.
5. Mesh3D / Skinned3D / FX parity.
6. RenderDoc capture hook.
```

합격:

```txt
DX12 vs Vulkan golden scene diff threshold 통과.
Validation warning/error 0.
```

### RH-14: Android Vulkan Mobile

목표: Android Vulkan smoke를 붙인다.

작업:

```txt
1. Android platform stub -> real ANativeWindow path.
2. AndroidVulkanSmokeHost.
3. Surface lost/suspend/resume.
4. Swapchain recreation.
5. ASTC/ETC2 texture variant.
6. Mobile feature profile.
7. Dynamic resolution hook.
```

합격:

```txt
Android device/emulator clear + triangle + texture.
Suspend/resume 후 정상 present.
Mobile profile unsupported feature 0.
```

### RH-15: iOS Decision Gate

목표: Metal native vs MoltenVK를 결정한다.

작업:

```txt
1. iOS public stub surface.
2. MoltenVK feasibility note.
3. Native Metal feasibility note.
4. Shader pipeline cost 비교.
5. App Store / tooling / maintenance 비교.
```

합격:

```txt
Decision record 작성.
선택된 path의 first smoke phase가 RH-16으로 분리된다.
```

### RH-16: Console Stub

목표: Xbox/PS5 SDK 없이 public repo가 console backend slot을 표현하고 컴파일한다.

작업:

```txt
1. eRHIBackend policy 확정.
2. ConsoleSmokeHost stub.
3. WINTERS_PLATFORM_XBOX / WINTERS_PLATFORM_PS5 gated folders.
4. SDK 미보유 시 compile stub.
5. Capability contract 작성.
```

합격:

```txt
SDK 없이 PC repo compile.
Console-specific code가 Engine/Public에 노출되지 않음.
Console smoke host가 backend selection stub까지 compile.
```

### RH-17: Xbox Backend

목표: SDK 확보 후 Xbox backend를 실구현한다.

작업:

```txt
1. SDK/EULA 확인.
2. Private implementation location 확정.
3. DX12Common helper 분리.
4. Xbox device/swapchain/memory/debug adapter 구현.
5. Console shader product path.
6. SmokeHost clear/triangle.
7. Visual parity subset.
```

합격:

```txt
Xbox hardware/devkit clear + triangle.
Mesh3D subset render.
Public repo/API 오염 0.
```

### RH-18: PS5 Backend

목표: SDK 확보 후 PS5 backend를 실구현한다.

작업:

```txt
1. SDK/EULA 확인.
2. Private implementation location 확정.
3. PS5 display/swapchain/device/memory adapter 구현.
4. Shader product path.
5. SmokeHost clear/triangle.
6. Visual parity subset.
```

합격:

```txt
PS5 hardware/devkit clear + triangle.
Mesh3D subset render.
Public repo/API 오염 0.
```

### RH-19: Unified Regression Gate

목표: backend가 늘어도 렌더링 의미가 깨지지 않게 한다.

작업:

```txt
1. Golden scene set.
2. Backend별 screenshot capture.
3. GPU timing query abstraction.
4. RenderDoc/PIX/validation hook.
5. Mobile precision tolerance.
6. Console tolerance.
7. CI smoke matrix.
```

합격:

```txt
DX11/DX12/Vulkan golden scene threshold 통과.
Mobile/Console은 device-specific tolerance로 별도 관리.
Perf regression threshold 기록.
```

---

## 6. 현재 우선순위

### 6.1 즉시 구현 순서

```txt
1. RH-7 Platform Surface & Capabilities (완료: public surface/capability contract + Win32 bridge)
2. DX12SmokeHost 솔루션 등록 및 Debug-DX12 build 명령 고정 완료
3. Debug-DX12 Engine compile-only 재검증
4. Public DX11 leak inventory 갱신
5. Shader register space 전수 수정
6. RH-9 Public DX11 leak 제거
7. RH-10 CommandList 실제화
8. RH-11 DX12 visual parity
```

### 6.2 지금 당장 하지 말 것

```txt
1. Vulkan directory부터 만드는 것.
   이유: RHISurface/capability/shader matrix 없이 만들면 Windows Vulkan-only scaffold가 되어 Android/Console에 다시 걸린다.

2. Console SDK 추측 구현.
   이유: public API 오염과 EULA 위험.

3. Client를 Debug-DX12 solution Build.0에 바로 묶기.
   이유: UpdateLib/EngineSDK race와 DX11-only UI path가 아직 남아 있다.

4. DX11 Public header 이동부터 하기.
   이유: CEngineApp/UI/Renderer/Resource/Client consumer 제거 전 이동하면 빌드가 깨진다.
```

---

## 7. Acceptance Gates

### Gate A: Public API Cleanliness

```txt
Engine/Public/RHI:
  d3d11.h / d3d12.h / vulkan.h / HWND / ANativeWindow / CAMetalLayer / GDK / PS5 SDK = 0

Engine/Public/Platform:
  platform enum + opaque pointer only
```

### Gate B: Backend Smoke

```txt
DX11: WintersGame Debug minimum scene.
DX12: DX12SmokeHost clear/present.
Vulkan: VulkanSmokeHost clear/triangle.
Android: AndroidVulkanSmokeHost clear/triangle/texture.
Console: stub host compile without SDK; real host gated with SDK.
```

### Gate C: Shader Contract

```txt
All shaders:
  explicit register slot
  explicit space
  reflection matches bind group layout
```

### Gate D: Visual Parity

```txt
Golden scenes:
  clear
  triangle
  textured quad
  depth
  blend
  static mesh
  skinned mesh
  FX sprite/mesh
  UI overlay
```

### Gate E: Runtime Stability

```txt
Resize/surface loss:
  Windows resize
  Alt+Tab
  Android suspend/resume
  iOS background/foreground
  Console suspend/resume if platform supports it
```

---

## 8. Decision Log

| ID | 결정 | 현재 선택 |
|---|---|---|
| D-1 | DX11 유지 여부 | 유지, legacy/reference |
| D-2 | Public RHI 기준 | DX12/Vulkan/Console explicit model |
| D-3 | Shader source | HLSL single source 우선 |
| D-4 | Android | Vulkan first |
| D-5 | iOS | Metal native vs MoltenVK decision gate |
| D-6 | Xbox | DX12-family but separate backend |
| D-7 | PS5 | proprietary private backend |
| D-8 | Native handle escape | backend-private debug bridge로 축소 예정 |
| D-9 | First implementation phase | RH-7 Surface & Capabilities |

---

## 9. RH-7 최소 구현 체크리스트

```txt
[x] Engine/Public/RHI/RHISurface.h
[x] Engine/Public/RHI/RHICapabilities.h
[x] Engine/Public/Platform/PlatformTypes.h
[x] Engine/Public/Platform/IPlatformWindow.h
[x] Engine/Public/Platform/IPlatformSurface.h
[x] IRHIDevice RHISurfaceDesc overload
[x] CWin32Window IPlatformWindow/IPlatformSurface bridge
[x] eRHIBackend / eEngineRHIBackend future backend slots
[x] vcxproj include 등록
[x] filters 등록
[x] 기존 DX11/DX12 compile compatibility 확인
```

---

## 10. 요약

DX11을 지우는 작업이 아니다. DX11은 작동하는 reference로 남긴다. 그러나 public RHI의 설계 기준은 DX11 immediate context가 아니라 DX12/Vulkan/Console의 explicit command model이다. Mobile/Console까지 고려하면 지금 필요한 첫 구현은 새로운 draw path가 아니라 surface/lifecycle/capability 계약이다. 이 계약이 있어야 Vulkan Android, iOS Metal/MoltenVK, Xbox, PS5가 public API를 오염시키지 않고 들어올 수 있다.
