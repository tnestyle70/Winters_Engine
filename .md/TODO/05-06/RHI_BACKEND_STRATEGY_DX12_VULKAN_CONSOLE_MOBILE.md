# RHI Backend 전략: DX11 Legacy, DX12 + Vulkan Main

작성일: 2026-05-06

## 1. 결정

```txt
DX11    = Legacy backend / 학습·비교·fallback 용도
DX12    = Windows PC main backend
Vulkan  = Cross-platform main backend
Mobile  = Vulkan-first, 필요 시 Metal 경유 별도 검토
Xbox    = DX12 계열 console backend 후보
PS5     = console RHI backend 후보, public code path와 분리
```

7G Client/Server/AssetConverter filters 정리는 현재 champion/scene 구조 정리 트랙에서 제외한다. Engine filters는 이미 1차 검증 완료 상태로 두고, 다음 구조 목표는 Solution Explorer 정리가 아니라 runtime/backend ownership 정리다.

---

## 2. Backend 계층

```txt
Game / Client
  ↓
Renderer / RenderGraph
  ↓
RHI Interface
  ├─ DX11      legacy
  ├─ DX12      Windows main
  ├─ Vulkan    PC + Android main
  ├─ Metal     iOS/macOS 후보, MoltenVK 또는 native Metal 중 추후 결정
  ├─ Xbox      DX12-family console backend
  └─ PS5       console-private backend
```

공통 RHI가 가져야 하는 핵심 추상화:

```txt
IRHIDevice
IRHIQueue
IRHICommandList
IRHISwapChain
IRHIPipelineState
IRHIRenderPass
IRHIBindGroupLayout
IRHIBindGroup
RHIResourceHandle
RHIDescriptorHandle
RHIShaderBlob
RHIBarrier
```

---

## 3. DX11 Legacy 정책

DX11은 제거하지 않는다. 단, 새 기능의 기준 backend로 삼지 않는다.

```txt
유지:
  - 현재 LoL 모작 실행 안정성
  - 학원 DX11 수업 구조 흡수 검증
  - DX12/Vulkan 회귀 비교용 reference path

금지:
  - DX11 native 객체를 새 Renderer/Client 코드에 직접 노출
  - 신규 기능을 DX11 전용 API에 먼저 박제
  - DX11 path만 통과하는 렌더링 기능 추가
```

DX11은 “작동하는 기준선”이고, DX12/Vulkan은 “앞으로의 주력 경로”다.

---

## 4. DX12 Main 목표

DX12는 Windows PC 메인 백엔드로 둔다.

```txt
M0. compile-only + DLL load
M1. device / queue / swapchain / clear-present
M2. descriptor heap / root signature / bind group
M3. command allocator/list/fence frame ring
M4. pipeline state + render pass
M5. texture/buffer upload + resource barrier
M6. Mesh3D / Skinned3D draw parity
M7. ImGui DX12 backend
M8. RenderGraph 전환
M9. GPU-driven / compute / async queue
```

주의:

```txt
DX12는 DX11 shader/texture/model caller를 우회하지 않는다.
Renderer는 RHI interface만 보고, backend 선택은 GameContext 또는 launch config가 담당한다.
```

---

## 5. Vulkan Main 목표

Vulkan은 Windows/Linux/Android 확장 가능성을 고려한 cross-platform main backend다.

```txt
V0. Vulkan instance/device/surface/swapchain
V1. SPIR-V shader pipeline
V2. descriptor set layout = RHIBindGroupLayout 매핑
V3. command buffer / fence / semaphore
V4. render pass 또는 dynamic rendering
V5. memory allocator(VMA 후보)
V6. Mesh3D / Skinned3D draw parity
V7. Android surface / mobile swapchain
V8. pipeline cache / PSO cache
```

Vulkan 도입 시 shader 전략:

```txt
Authoring: HLSL 우선 유지
DX12: DXC -> DXIL
Vulkan: DXC -> SPIR-V
Metal 후보: SPIRV-Cross 또는 별도 MSL path 검토
```

---

## 6. Mobile 편입 방향

모바일은 Vulkan-first로 둔다.

```txt
Android:
  - Vulkan backend 직접 사용
  - swapchain/surface만 platform adapter로 분리
  - texture compression: ASTC/ETC2 후보
  - input/touch는 Platform layer에서 별도 추상화

iOS:
  - native Metal 또는 MoltenVK 후보
  - 현재 우선순위는 Android보다 낮음
```

모바일 RHI 제약:

```txt
deferred + heavy SSAO/GTAO는 옵션화
shader permutation 줄이기
bindless는 capability check 후 사용
tile-based GPU 고려: render pass/load-store 명확화
```

---

## 7. Xbox / PS5 편입 방향

콘솔은 public repo에서 직접 SDK 의존을 노출하지 않는다.

```txt
Xbox:
  - DX12-family backend로 설계 가능
  - public RHI interface는 동일
  - Xbox 전용 구현은 Console/Xbox private adapter로 격리

PS5:
  - public code에는 추상 backend slot만 둔다
  - 실제 SDK/API 코드는 별도 private branch 또는 excluded project
```

콘솔 대응을 위해 지금부터 지켜야 할 규칙:

```txt
Win32 HWND를 Renderer/RHI public API에 직접 박제하지 않기
PlatformWindowHandle 같은 opaque handle 사용
파일 경로 / shader compile / input / audio device는 platform adapter로 격리
```

---

## 8. 다음 실행 순서

```txt
RHI-A. DX11 native caller 잔존 목록 갱신
RHI-B. DX12 compile-only / clear-present 검증
RHI-C. Shader register(..., space0) 전수 명시
RHI-D. RHIResourceHandle / BindGroupLayout 기준 확정
RHI-E. Vulkan planning doc를 현재 DX12 scaffold와 같은 깊이로 작성
RHI-F. PlatformWindowHandle 도입 계획 작성
```

현재 champion/scene 정리 트랙과 병행할 때의 원칙:

```txt
Scene_InGame 분해가 먼저다.
Renderer/RHI backend 교체는 Scene이 직접 DX 객체를 덜 볼수록 쉬워진다.
따라서 RHI 본격 구현 전 7E RenderBridge 추출이 선행되면 좋다.
```
