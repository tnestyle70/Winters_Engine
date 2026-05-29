# RHI Multi-Backend Migration — Master Plan

> 2026-05-25 업데이트: 실제 작업 진입은 세션 단위 문서 `.md/plan/rhi/sessions/00_RHI_SESSION_INDEX.md`를 기준으로 한다.
> `Smoke.vcxproj`, `DX12.vcxproj`, `DX12.exe`, `Vulkan.exe` 같은 standalone 테스트 프로젝트는 만들지 않는다.
> DX12/Vulkan/Console은 모두 `WintersEngine.dll` 내부 RHI backend이며, 검증은 `WintersGame.exe`의 backend 선택으로 수행한다.

**작성일**: 2026-04-30 (Codex 2차 검토 보정 2026-04-30)
**상위 문서**: `CLAUDE.md` 의 "Phase 2: RenderGraph + Deferred" 항목 + `WINTERS_ENGINE_ARCHITECTURE_FINAL.md`
**범위**: Phase RH-0 ~ RH-6 — DX11 단일 백엔드 → DX11 + DX12 + Vulkan 멀티 백엔드 RHI 전환
**전제**: Codex 1차 분석 (DX12/VK 모델 기준 설계 → DX11 emulation) + Codex 2차 (30건 보정) 모두 채택. 산업 합의 (Unreal RHI / WebGPU / The-Forge / NVRHI / Diligent) 일관.

> ★ **Codex 2차 검토 (2026-04-30) 핵심 변경**:
> - **RH-0 = "이동 X, inventory + Legacy rename + TODO marker + shim 유지"** 로 축소
> - **RH-1 의 자원 생성 API = `unique_ptr<IRHI*>` 가 아닌 handle 기반** (DLL boundary CRT 충돌 방지)
> - **`Get_RHIDevice()` 이름 재사용 금지** — RH-0 `_Legacy` rename → RH-1 신규 이름 (`Get_NewRHIDevice()` 권장, 추후 모든 caller 마이그 후 RH-2 시점에 `Get_RHIDevice()` 로 정식 rename)
> - **Public DX11 헤더 이동은 RH-2 종료 후** (Renderer/Resource/CEngineApp 마이그 완료 시점)
> - 부록 B 에 30건 매핑 명시

**한 줄**: **DX11 을 추상화하지 않고 DX12/Vulkan 의 명시적 렌더링 모델을 기준으로 RHI 를 설계한 뒤, DX11 이 그걸 immediate-mode emulation 하게 한다. CGameInstance 의 8개 DX11 leak getter 제거가 RH-0 의 핵심.**

---

## 1. Why — 정당화

### 1.1 현재 코드베이스 진단 (실제 grep 데이터)

| 문제 | 위치 | 심각도 |
|---|---|---|
| `CGameInstance` 가 `DX11Shader / DX11Pipeline / CBlendStateCache / CDX11Device` 직접 반환 | `Engine/Include/GameInstance.h:101-108` (8개 getter) | ★★★ |
| `Get_RHIDevice()` 이름은 RHI 인데 반환 타입 `CDX11Device*` | 동상 L101 | ★★★ |
| `Scene_InGame.cpp:1898` 가 `Get_RHIDevice()->GetContext()` 직접 호출 | `Client/Private/Scene/Scene_InGame.cpp:1898` | ★★★ |
| `CDX11Device::GetDevice()` / `GetContext()` 가 raw `ID3D11Device*` / `ID3D11DeviceContext*` 노출 | `Engine/Public/RHI/CDX11Device.h:49-52` | ★★ |
| `Engine/Public/RHI/CDX11Device.h` **자체가 Public 위치 + d3d11.h include** — 헤더 자체가 leak | `Engine/Public/RHI/CDX11Device.h:2-7` | ★★★ |
| `IBuffer::GetNativeHandle()` 가 `void*` 반환 — 백엔드 노출 escape hatch | `Engine/Public/RHI/IBuffer.h:32` | ★★ |
| `Engine/Public/RHI/DX11/*` 9개 헤더가 `d3d11.h` + `ComPtr` 직접 include | `DX11VertexBuffer.h:2-4` 등 | ★★ |
| **Public DX11 헤더 직접 consumer (★ Codex 2차 검증)** — 9개 파일이 `RHI/DX11/...` 또는 `CDX11Device.h` 직접 include: `Engine/Public/Engine_Defines.h`, `CEngineApp.h`, `UI_Manager.h`, `PlaneRenderer.h`, `Mesh.h`, `Client/Public/.../FxBillboardComponent.h`, `FxMeshComponent.h`, `FxSystem.h`, `Scene_InGame.cpp` (12 hit) | 9 파일 + Scene 12 hit | ★★★ |
| Resource (Mesh.h / Model.h / ResourceCache.h / Texture.h) + Renderer (PlaneRenderer.h) + Editor (ImGuiLayer.h) + UI (UI_Manager.h) + WMeshLoader.h — 16개 헤더가 `ID3D11Device*` 매개변수 노출 | Engine/Public 16곳 | ★★ |
| Client 측 `ID3D11*` 누수 — `FxSystem.h` + FX components 4건 | `Client/Public/GameObject/FX/*` | ★★ |

**중요 (Codex P0-1)**: Public DX11 헤더 직접 consumer 9개를 먼저 마이그하지 않고 `Engine/Public/RHI/DX11/*` 를 Private 로 이동하면 즉시 빌드 실패. **RH-0 의 file move 작업은 RH-2 종료 후 (Renderer/Resource 마이그 완료 시점) 에만 가능**.

### 1.2 LoL → 엘든링 호환 ROI

| 게임 | DX11 충분? | DX12 필요? | VK 필요? |
|---|---|---|---|
| **LoL 모작** | ✅ 5v5 = 50K~200K triangle. DX11 driver 친절함이 도움 | ❌ | ❌ |
| **엘든링 모작** | ⚠️ 1M~10M triangle 오픈월드. DX11 single-thread command 병목 명확 | ✅ multi-thread command recording 필수 | △ Steam Deck / Linux 시 |

**ROI 정당화**: ER 진입 시 DX12 백엔드 추가가 거의 필연. **LoL 단계에서 추상화 미리 안 해두면 ER 진입 시 렌더 코드 30~40 곳 다 헤집어야**. RH-0~RH-4 는 LoL Phase 2 (RenderGraph) 와 병행, RH-5 (DX12) 는 ER 진입 직전.

### 1.3 산업 합의

| RHI | BindGroup | RenderPass | PSO | CommandList | 추상화 기준 |
|---|---|---|---|---|---|
| WebGPU/Wgpu | ✅ | ✅ | ✅ | ✅ | DX12/VK/Metal |
| Unreal RHI | ✅ | ✅ | ✅ | ✅ | DX12/VK/console |
| The-Forge | ✅ | ✅ | ✅ | ✅ | DX12/VK/Metal |
| NVIDIA NVRHI | ✅ | ✅ | ✅ | ✅ | DX12/VK |
| Diligent Engine | ✅ | ✅ | ✅ | ✅ | DX12/VK |
| Sokol-gfx | ⚠️ slot | ⚠️ | ✅ | ✅ | DX11 기준 (limit) |
| bgfx | ⚠️ slot | ⚠️ | ⚠️ | ⚠️ | DX9/11 기준 (legacy) |

→ **DX12/VK 기준 설계가 산업 표준**. DX11 기준 RHI (sokol/bgfx) 는 modern feature 추가에서 한계 봉착.

---

## 2. Phase Overview

| Phase | 기간 | 산출물 | 합격 게이트 |
|---|---|---|---|
| **RH-0 Foundation (★ 2차 보정)** | 1주 | **Inventory + GameInstance Legacy rename + 9개 leak consumer 전수 TODO marker (★ 이동 X)** | LoL 빌드 통과, 모든 leak 위치에 `// ★ RH-2 TODO` 주석, deprecated warning 만 |
| **RH-1 Interface Extraction (★ 2차 보정 — handle API)** | 2주 | `IRHIDevice / IRHISwapChain / IRHIQueue / IRHIBuffer / IRHITexture / IRHIShader / IRHISampler` + RHITypes + RHIDescriptors + DX11 어댑터. **자원 생성 = handle 반환, Engine-owned (DLL CRT 충돌 방지)**. **신규 getter 이름 `Get_NewRHIDevice()`** (RH-0 의 `_Legacy` 와 양립) | 모든 자원 생성이 `device->CreateBuffer(desc) -> RHIBufferHandle` 통과, 컴파일 OK |
| **RH-2 CommandList + Public DX11 헤더 제거 (★ 2차 보정)** | 2주 (1주 → 2주) | `IRHICommandList / IRHICommandPool / IRHIFence / IRHISemaphore` + DX11 immediate emulation + **Renderer/Resource/CEngineApp/FX 9개 leak consumer 마이그 + 마이그 완료 후 `Engine/Public/RHI/DX11` → `Engine/Private/RHI/DX11` 이동 + `CDX11Device.h` 도 Private 로 이동** + `Get_NewRHIDevice()` → `Get_RHIDevice()` 정식 rename | Engine/Public 어디에서도 `ID3D11Device*` / `d3d11.h` / `RHI/DX11` include 0 hit |
| **RH-3 PSO + RenderPass + BindGroup** | 1주 | `IRHIPipelineState / IRHIRenderPass / IRHIBindGroup / IRHIBindGroupLayout` + Desc 시리즈 (★ BindGroup immutable + UpdateBindGroup 별도 API) | 새 셰이더 추가 시 PSO 1개로 끝 (state 흩어짐 X) |
| **RH-4 Resource Handle 강화** | 4일 | **64-bit handle (32 index + 32 generation)** + `CRHIResourceTable` thread-safety policy + `Destroy*` API 일관 | use-after-free generation check 동작, render thread only 강제 |
| **RH-5 DX12 Backend** | 3~4주 (compile-only) **+ 2~3주 visual parity** | `Engine/Private/RHI/DX12/*` 전체 + D3D12MA 외부 라이브러리 편입 + PSO 캐시 | compile-only 합격: DX12 빌드 통과 / visual parity 합격: LoL 시각 결과 동일 (frame diff < 1px) |
| **RH-6 Vulkan Backend** | 4~6주 (compile-only) **+ 3~4주 visual parity** | `Engine/Private/RHI/Vulkan/*` 전체 + VMA 외부 라이브러리 편입 + DXC SPIR-V 파이프라인 + validation layer availability check | compile-only 합격: VK 빌드 통과 / visual parity 합격: LoL 시각 결과 동일 (선택) |

**핵심 분기점**:
- RH-0~RH-4 (1.5~2개월) 가 인프라 작업. RH-2 가 가장 무거움 (Renderer 마이그 + 파일 이동 + naming 정식화).
- RH-5/6 은 **compile-only 와 visual parity 를 분리** (Codex P2-21). compile-only 는 mechanical, visual parity 는 sync/barrier 디버깅 포함.

---

## 3. 코딩 컨벤션 — 본 plan 전체에 적용

### 3.1 네이밍 (CLAUDE.md §컨벤션)
- **인터페이스: `I` 접두사 필수** — `IRHIDevice`, `IRHICommandList`
- **클래스 (구체 구현): `C` 접두사 필수** — `CDX11Device`, `CDX12Device`, `CVulkanDevice`
- **struct (POD Desc): C 접두사 금지** — `RHIBufferDesc`, `RHIPipelineDesc`
- **파일명: C/I 접두사 없음** — `Device.h` (인터페이스 `IRHIDevice`), `DX11Device.h` (클래스 `CDX11Device`)
- **enum class: `eRHI` 접두사** — `eRHIResourceState`, `eRHIFormat`

### 3.2 타입 별칭 (Engine/Include/WintersTypes.h)
- **신규 코드 강제**: `f32_t / f64_t / i32_t / u32_t / u8_t / u16_t / u64_t / i8_t / i64_t / bool_t / wstring_t / tchar_t`
- **금지**: raw `float / int / unsigned int`, legacy `float32 / int32 / uint32`
- **예외**: Win32 (HWND/DWORD/UINT), DirectXMath (XMVECTOR/XMFLOAT3), 서드파티 (DX11/DX12/VK 타입은 어댑터 구현부 한정)

### 3.3 클래스 설계 (CLAUDE.md §클래스 설계 원칙)
- **생성자 private** — 외부 직접 `new` 금지
- **소멸자 public virtual** — 인터페이스 다형성
- **멤버 변수 전부 private** — getter/setter 또는 동일 클래스 내부 메서드만 직접 조작
- **Create 팩토리 패턴**:
  ```cpp
  static unique_ptr<CDX11Device> Create(const RHIDeviceDesc& desc)
  {
      auto pInstance = unique_ptr<CDX11Device>(new CDX11Device());
      // 초기화 ...
      return pInstance;
  }
  ```
- **`make_unique` 사용 범위 (★ Codex P2-22 보정)** — "private ctor 가 있는 Create 팩토리 안에서만" `unique_ptr<T>(new T())` 직접 사용. 그 외 일반 코드에서는 `make_unique<T>(...)` 사용 OK.
- **`unique_ptr` 멤버 보유 시 copy ctor / assign 명시 delete + 이동 허용** (gotchas — WINTERS_ENGINE dllexport 호환)

### 3.4 멤버 변수 접두사
- `m_p` pointer (`m_pDevice`)
- `m_b` bool (`m_bVSync`)
- `m_f` float (`m_fWidth`)
- `m_v` Vec3 (`m_vClearColor`)
- `m_` 기타 (`m_DescTable`)

### 3.5 DLL Export
- 신규 export 는 **`CGameInstance` 만** (Tier 1 포워딩)
- Tier 2 는 인터페이스 Getter — `IRHIDevice* CGameInstance::Get_RHIDevice()` (포인터 캐시 후 직접 호출)
- 내부 구현 (`CDX11Device` / `CDX12Device` 등) 은 `WINTERS_ENGINE` 미사용

### 3.6 Include 컨벤션
- Engine 헤더: `WintersAPI.h` + `WintersTypes.h` + `WintersMath.h` (Engine_Defines.h 미포함)
- DX11/12/VK 헤더는 **구현 cpp 또는 `Engine/Private/RHI/<Backend>/`** 에서만 include — Public 절대 금지
- ImGui dx11 backend 같은 외부는 `Get_NativeHandle(NativeType)` 으로 명시 escape

### 3.7 HLSL
- `row_major matrix` / `row_major float4x4` 필수 (DirectXMath row-major 관례)
- mul 순서: `mul(vector, matrix)` (행 벡터 × 행렬)
- 레지스터 슬롯 명시: `register(b0)`, `register(t0)`, `register(s0)` — DXC SPIR-V cross-compile 호환
- DXC 사용 (D3DCompile X) — DX11/12/VK 모두 동일 toolchain

---

## 4. 위험 / 트레이드오프

### 4.1 DX11 의 "친절함" 상실
- 현재: DX11 driver 가 자동 barrier/sync/메모리 관리
- DX12/VK: 명시적 관리 필수
- → 마이그 후 hang/crash 가능성

**완화**: RH-2 (CommandList) 단계에서 **DX11 도 명시적 barrier 호출** 강제. DX11 백엔드는 barrier = no-op 이지만 **호출 자체는 통과** → DX12/VK 마이그 시 race 미리 발견.

### 4.2 Bindless 한계
- DX11 = 슬롯 기반 (16 textures, 14 cbuffers)
- DX12/VK = bindless 가능 (descriptor heap 1M textures)

**완화**: 인터페이스는 슬롯 모델 따름 (`SetTexture(slot, tex)`). DX12/VK 백엔드는 내부적 bindless 사용. 미래에 bindless 가 핵심이면 인터페이스 v2 추가.

### 4.3 셰이더 cross-compile 비용
- HLSL → DX11 bytecode (D3DCompile / DXC)
- HLSL → DX12 bytecode (DXC)
- HLSL → SPIR-V → Vulkan (DXC + spirv-cross)
- 같은 HLSL 이 백엔드별 다르게 컴파일 → register binding 충돌

**완화**: 셰이더 내 `register(bN/tN/sN)` 명시 + 컴파일 타임 reflection (PSO 만들 때 binding 자동 추출).

### 4.4 PSO 캐싱
- DX12/VK PSO 컴파일 = 수백 ms (셰이더 컴파일 포함)
- 첫 frame 에 100개 PSO 컴파일 = 10초 stutter

**완화**: 디스크 PSO 캐시 (DX12: `ID3D12PipelineLibrary`, VK: `VkPipelineCache`). 셰이더 hash 기반 lookup.

### 4.5 Frame in flight (double/triple buffering)
- DX11: driver 가 알아서
- DX12/VK: `frameIndex % MAX_FRAMES_IN_FLIGHT` 명시 관리. Per-frame UniformBuffer / DescriptorSet 풀링 필요.

**완화**: RH-1 단계에서 `IRHIDevice` 가 `BeginFrame() / EndFrame()` 명시 — frameIndex 내부 관리. DX11 백엔드는 무시, DX12/VK 는 활용.

### 4.6 YAGNI — DX12/VK 정말 필요한가?
- LoL 만 보면 DX11 충분
- ER 진입 시 multi-thread command recording 위해 DX12 거의 필연
- VK 는 Steam Deck / Linux 결정 시점에

**완화**: **RH-1~4 (1.5개월) 까지만 LoL 단계에 박제**. RH-5 (DX12) 는 ER 진입 직전. RH-6 (VK) 는 cross-platform target 결정 시점.

---

## 5. Sub-plan 목록

본 master plan 의 각 phase 는 별도 sub-plan 파일에 h/cpp 전문 박제:

| Sub-plan | 파일 | 분량 |
|---|---|---|
| **Phase RH-0** Foundation | `01_RHI_PHASE_0_FOUNDATION.md` | 3개 작업 + 합격 게이트 |
| **Phase RH-1** Interface Extraction | `02_RHI_PHASE_1_INTERFACES.md` | 9개 인터페이스 h 전문 + DX11 어댑터 cpp |
| **Phase RH-2** CommandList | `03_RHI_PHASE_2_COMMANDLIST.md` (★ 추후 박제) | IRHICommandList/Pool/Fence/Semaphore + DX11 emulation |
| **Phase RH-3** PSO + RenderPass + BindGroup | `04_RHI_PHASE_3_PSO_RENDERPASS_BINDGROUP.md` (★ 추후) | 4개 인터페이스 + Desc 시리즈 |
| **Phase RH-4** Resource Handle | `05_RHI_PHASE_4_RESOURCE_HANDLE.md` (★ 추후) | RHIHandles.h + lookup table |
| **Phase RH-5** DX12 Backend | `06_RHI_PHASE_5_DX12_BACKEND.md` (★ 추후) | Engine/Private/RHI/DX12/* |
| **Phase RH-6** Vulkan Backend | `07_RHI_PHASE_6_VULKAN_BACKEND.md` (★ 추후) | Engine/Private/RHI/Vulkan/* + DXC SPIR-V |

---

## 6. 합격 게이트 (전체)

### 6.1 RH-0 합격 (★ 2차 보정 — file move 제외)
- ✅ **9개 leak consumer 전수 inventory 문서화** (Engine_Defines.h / CEngineApp.h / UI_Manager.h / PlaneRenderer.h / Mesh.h / FxBillboardComponent.h / FxMeshComponent.h / FxSystem.h / Scene_InGame.cpp)
- ✅ `CGameInstance::Get_DX11Device_Legacy()` / `Get_MeshShader_Legacy()` 등 8개 deprecated marker
- ✅ 9개 leak consumer + Scene_InGame 12 hit 모두 `// ★ RH-2 TODO` 주석 + `_Legacy` 호출 변경
- ✅ **`Engine/Public/RHI/DX11/` 이동 X — RH-2 종료 시점에 일괄 이동**
- ✅ LoL 빌드 통과 (deprecated warning 다수, error 0건) + Scene_InGame 동작 무회귀

### 6.2 RH-1 합격 (★ 2차 보정 — handle API + 신규 이름)
- ✅ 9개 인터페이스 헤더 박제 (`IRHIDevice / IRHISwapChain / IRHIQueue / IRHIBuffer / IRHITexture / IRHIShader / IRHISampler` + Types + Descriptors)
- ✅ `CDX11Device : public IRHIDevice` 다중 상속 (CDX11Device.cpp 단일 파일에서 구현 — DX11DeviceAdapter.cpp 별도 파일 X)
- ✅ **`CGameInstance::Get_NewRHIDevice() -> IRHIDevice*` 신규 (★ RH-0 의 `Get_DX11Device_Legacy()` 와 양립, 이름 충돌 X)**
- ✅ **자원 생성 = handle API**: `device->CreateBuffer(desc) -> RHIBufferHandle` (Engine-owned, DLL CRT 충돌 방지)
- ✅ `IBuffer.h` shim 유지 (`IBuffer = IRHIBuffer` deprecated alias)
- ✅ DX11Buffer / Texture / Shader / Sampler 4종 신규 박제 (기존 4종 클래스와 **병행** — alias X, Codex P0-7)
- ✅ `RHIWindowHandle` wrapper (HWND → 백엔드 중립, Codex P1-9)
- ✅ 모든 신규 backend 클래스 `final` 키워드 (Codex P1-11)

### 6.3 RH-2 합격 (★ 2차 보정 — Public DX11 헤더 제거 포함)
- ✅ Renderer 9개 leak consumer 전수 마이그 (`Render(pCtx, ...)` → `Render(IRHICommandList*, ...)`)
- ✅ Scene_InGame.cpp:1898 의 `GetContext()` 제거 — `Get_FrameCommandList()` 로 대체
- ✅ **`Engine/Public/RHI/DX11/` → `Engine/Private/RHI/DX11/` 이동** (이 시점 안전)
- ✅ **`Engine/Public/RHI/CDX11Device.h` → `Engine/Private/RHI/DX11/DX11Device.h` 이동** (★ Codex P0-2)
- ✅ Engine.vcxproj + .filters XML 명시 갱신
- ✅ `Get_NewRHIDevice()` → `Get_RHIDevice()` 정식 rename (`_Legacy` 8개 deprecated 시작)
- ✅ `Engine/Public/` 전수 grep `ID3D11Device|d3d11.h|RHI/DX11` → 0 hit

### 6.4 RH-5 합격
- ✅ Winters.sln 에 `Debug-DX12 / Release-DX12` 컨피그 추가
- ✅ DX12 컨피그 빌드 → LoL 동일 시각 결과 (frame diff < 1px)
- ✅ DX12 PSO 캐시 디스크 저장/로드 동작

### 6.5 RH-6 합격 (선택)
- ✅ Vulkan 컨피그 빌드 → LoL 동일 시각 결과
- ✅ DXC SPIR-V cross-compile 파이프라인 동작
- ✅ Validation layer 0 error / 0 warning

---

## 7. 한 줄

**RHI 멀티 백엔드 = "DX12/VK 기준 설계 + DX11 emulation". RH-0 inventory + Legacy rename → RH-1 9개 인터페이스 + handle API → RH-2 CommandList + Public DX11 헤더 제거 → RH-3 PSO/RenderPass/BindGroup → RH-4 64-bit handle 강화 → RH-5 DX12 (compile-only / visual parity 분리) → RH-6 Vulkan (선택). 약 2개월 (RH-0~4) 후 DX12/VK 추가는 mechanical translation. LoL → 엘든링 호환의 핵심 인프라.**

---

## 부록 B — Codex 2차 검토 (30건) 반영 매핑

### P0 — 빌드 깨지는 사항 (8건)

| # | Findings | 위치 | 적용 patch |
|---|---|---|---|
| P0-1 | RH-0 file move 즉시 실행 시 9개 consumer leak 으로 빌드 실패 | 본 master §1.1, §6.1 + Phase 0 §1 | RH-0 의 file move 작업 완전 제거. RH-2 종료 후 이동으로 변경 |
| P0-2 | `CDX11Device.h` 자체가 Public 위치 — 이동 전략 별도 필요 | 본 master §1.1, §6.3 + Phase 0 §1 | RH-2 종료 시점에 `Engine/Private/RHI/DX11/DX11Device.h` 로 함께 이동 |
| P0-3 | Scene_InGame 12 hit 만 치환은 부족 — CEngineApp / Mesh / PlaneRenderer / FxSystem 도 leak | Phase 0 §3 (확장) | 9개 consumer + Scene 12 hit 전수 TODO marker |
| P0-4 | RH-1 의 `Get_RHIDevice() -> IRHIDevice*` 이름 재사용 시 RH-0 caller 충돌 | Phase 1 §14 + master §6.2 | RH-1 = `Get_NewRHIDevice()` 신규 이름. RH-2 종료 시점에 `Get_RHIDevice()` 정식 rename |
| P0-5 | `unique_ptr<IRHIBuffer>` 반환 = DLL boundary CRT 충돌 위험 | Phase 1 §10, §12 + master §6.2 | Handle API (`RHIBufferHandle = device->CreateBuffer(desc)`) — Engine-owned + 명시 Destroy |
| P0-6 | `DX11DeviceAdapter.cpp` 가 기존 `CDX11Device.cpp` 와 중복 정의 위험 | Phase 1 §12 (제거) | 신규 cpp 파일 생성 X — 기존 `CDX11Device.cpp` 에 IRHIDevice impl 합침 |
| P0-7 | `CDX11VertexBuffer = CDX11Buffer` alias 시 기존 API 차이로 깨짐 | Phase 1 §13 (수정) | RH-1 = 기존 4종 유지 + 신규 `CDX11Buffer` 병행. RH-2 caller 마이그 후 alias |
| P0-8 | code block 줄바꿈 깨진 곳 다수 — 복붙 시 컴파일 실패 | Phase 1 전체 | 전체 code block 재검수 (본 보정 patch 에 포함) |

### P1 — 설계 보정 (12건)

| # | Findings | 위치 | 적용 patch |
|---|---|---|---|
| P1-9 | `RHIDeviceDesc` 의 `HWND` 직접 노출 — VK/Linux 시 막힘 | Phase 1 §3 RHIDescriptors.h | `RHIWindowHandle` wrapper (platform-neutral) |
| P1-10 | 신규 코드의 raw `bool` — 컨벤션 위배 | Phase 1 전체 | `bool` → `bool_t` 전수 치환 |
| P1-11 | concrete backend 클래스에 `final` 미적용 | Phase 1 §11 + DX12/VK | 모든 `CDX11Device / CDX11Buffer / CDX12Device / CVulkanDevice` `final` 추가 |
| P1-12 | `GetNativeHandle()` ownership 불명확 | Phase 1 §4 IRHIBuffer + 모든 인터페이스 | "borrowed pointer, AddRef X, 즉시 사용만" 정책 명시 + debug assert |
| P1-13 | RH-2 DX11 CommandList Begin/End no-op 만으로는 RTV/DSV/viewport 책임 누락 | Phase 2 §2 | `BeginFrameCommandList` 가 RTV/DSV/viewport 바인딩 책임 명시 |
| P1-14 | "Renderer 30곳" 추상적 — 실제 pre-scan 목록 필요 | Phase 2 §3 | 4-folder pre-scan 결과 (`Resource::{Mesh,Model,Texture,ResourceCache} / Framework::CEngineApp / Renderer::{Cube,Plane,Model,FxStaticMesh,Triangle}`) 박제 |
| P1-15 | BindGroup mutable `SetBuffer/Texture/Sampler` = DX12/VK descriptor lifetime 불명확 | Phase 3 §1.3 | 생성 시 immutable desc 또는 별도 `UpdateBindGroup()` API |
| P1-16 | 24-bit index + 8-bit generation = 256 cycle wrap 너무 빠름 | Phase 4 §1 | 64-bit handle (32 index + 32 generation) |
| P1-17 | `CRHIResourceTable` thread-safety 정책 부재 | Phase 4 §3 | "main render thread only" 명시 + debug assert |
| P1-18 | DX12 생성 코드 HRESULT 체크 누락 | Phase 5 §2.1 | 모든 D3D12CreateDevice / CreateDXGIFactory2 / queue / swapchain HRESULT 체크 + log |
| P1-19 | DX12 snippet 이 RH-4 이후인데 `IRHIBuffer*` barrier 사용 | Phase 5 §2.2 | RH-4 handle API 와 일관 (`RHIBufferHandle` 기반) |
| P1-20 | D3D12MA / VMA 외부 라이브러리 미편입 | Phase 5 §1, Phase 6 §1 | ThirdPartyLib 편입 계획 (license / include / lib / vcxproj) 별도 작업 추가 |

### P2 — 문서/검증 보강 (10건)

| # | Findings | 위치 | 적용 patch |
|---|---|---|---|
| P2-21 | "RH-5/6 mechanical translation" 과감 표현 | 본 master §6 | compile-only 합격 vs visual parity 합격 분리 |
| P2-22 | `make_unique 금지` 너무 광범위 | 본 master §3.3 | "private ctor factory 안에서만" 으로 좁힘 |
| P2-23 | `Remove-Item -Recurse -Force` 위험 명령 | Phase 0 §1 | `git mv` + 빌드 통과 + SDK clean 순서로만 명시 |
| P2-24 | bash 명령 (sed / grep) 위주 — Windows 환경 충돌 | Phase 0 전반 | PowerShell 또는 `rg` 명령 병기 |
| P2-25 | `Engine.vcxproj.filters` GUID 자동 처리 — 계획서 규칙 위배 | Phase 0 §1 | filters 변경 XML 명시 |
| P2-26 | `IBuffer.h` 교체 시 기존 include shim 필요 | Phase 1 §4 | `IBuffer.h` 유지 + `IRHIBuffer.h` include + alias deprecated 보존 |
| P2-27 | `RHIShaderDesc` 의 bytecode/reflection 소유권 불명확 | Phase 1 §6 | `IRHIShader::GetBytecode/GetBytecodeSize` ownership = "borrowed, shader 객체와 동일 lifetime" 명시 |
| P2-28 | `RHIInputElementDesc::semanticName` lifetime 불명확 | Phase 3 §2 | "static const char* 만 허용" 정책 |
| P2-29 | Vulkan validation layer availability 사전 체크 누락 | Phase 6 §4 | `vkEnumerateInstanceLayerProperties` 로 사전 확인 + fallback |
| P2-30 | RH-2~RH-6 outline 만 — h/cpp 전문 규칙 미충족 | 전체 | 각 phase 본격 박제 진입 시 sub-plan 확장 (현재는 1차 outline) |

---

## 부록 C — RH-7+ Multi-Platform Expansion

DX11 Legacy -> DX12 -> Vulkan까지의 core RHI migration은 본 문서 RH-0~RH-6에서 관리한다. Mobile / Console(PS5, Xbox) 확장은 `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md`와 통합 실행 계획서 `09_RHI_DX11_LEGACY_TO_DX12_VULKAN_MOBILE_CONSOLE_MASTER.md`에서 관리한다.

추가 phase:

| Phase | 문서 | 목적 |
|---|---|---|
| RH-7 | `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md` | Platform Surface / Lifecycle 추상화 (`RHIWindowHandle` -> `RHISurfaceDesc`) |
| RH-8 | `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md` | Shader / Asset cross-compile matrix |
| RH-9 | `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md` | Mobile RHI (Android Vulkan first, iOS Metal/MoltenVK decision gate) |
| RH-10 | `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md` | Console RHI (Xbox DX12-family, PS5 proprietary backend stub/NDA guard) |
| RH-11 | `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md` | Build matrix / smoke host / per-config output policy |
| RH-12 | `08_RHI_MULTI_PLATFORM_EXPANSION_PLAN.md` | Visual parity / performance gate |

원칙: console SDK/GDK/PS5 proprietary 타입은 `Engine/Public`에 노출하지 않고, SDK 확보 전에는 stub + capability contract까지만 유지한다.
