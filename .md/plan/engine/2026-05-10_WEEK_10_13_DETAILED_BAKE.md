# 2026-05-10 Week 10-13 상세 박제 — Track 2 RH-5 DX12 Visual Parity

**작성일**: 2026-05-10
**수정일**: 2026-05-02 (코드베이스 감사 반영, 전면 보정)
**상태**: 검토 대기 — **Week 7-9 전제 미충족, 본 계획서는 목표 형태(target shape) 문서**
**전제**: Week 7-9 (RH-5 DX12 compile-only — 빌드 통과 + LoL exe 정상 종료) 완료
**상위 문서**: [Twin Track 계획서 §5.4](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md), [RHI 마스터 §6.4 RH-5 합격](../rhi/00_RHI_MIGRATION_MASTER.md)

---

## 0. 한 줄

> **Week 10-13 = DX12 gameplay / visual parity on top of the corrected W7-9 bootstrap. Deterministic scene parity comes first; full LoL match parity only happens after the remaining DX11-native bridges and DX11-only UI/ImGui paths have an explicit DX12 story.**

---

## 0-A. 코드베이스 현실 감사 (2026-05-02 전면 보정)

> **CRITICAL**: 2026-05-02 시점 코드베이스 감사 결과, Week 7-9 전제 조건이 **전부 미충족**이다.
> 본 계획서의 모든 DX12 코드 예시는 **목표 형태(target shape)**이며, 현재 존재하는 코드가 아니다.

### 0-A.1 미존재 항목 (계획서 가정 vs 현실)

| 계획서 가정 | 현실 (2026-05-02) | 선행 단계 |
|---|---|---|
| `Engine/Private/RHI/DX12/` 디렉토리 + 14 파일 | **디렉토리 자체 없음** | RH-5 (W7-9) |
| `Debug-DX12` / `Release-DX12` vcxproj 구성 | **빌드 구성 없음** | RH-5 (W7) |
| `CDX12Device`, `CDX12CommandList`, `CDX12SwapChain` 등 | **모두 미존재** | RH-5 (W7-9) |
| `WINTERS_RHI_BACKEND_DX12` 전처리기 매크로 | **정의 없음** | RH-5 (W7) |
| `IRHICommandList`, `IRHISwapChain`, `IRHIQueue` | **모두 미존재** | RH-2~RH-3 (W3-6) |
| `IRHIPipelineState`, `IRHIRenderPass`, `IRHIBindGroup` | **모두 미존재** | RH-3 (W5-6) |
| `EngineConfig` RHI 백엔드 선택 필드 | **없음** (윈도우/vsync/targetFPS 만) | RH-1 또는 W7 |
| `--rhi` / `--scenario` / `--capture-frame` CLI 파싱 | **없음** | W10 신설 |
| `Tools/PixCompare.ps1` | **없음** | W13 신설 |
| `★ RH-2 TODO` 주석 (Engine 소스 9개 파일) | **Engine 소스 0개** (EngineSDK/inc 복사본 1개만) | RH-0 미완료 |

### 0-A.2 존재하는 인프라 (활용 가능)

| 항목 | 위치 | 상태 |
|---|---|---|
| `IRHIDevice` 인터페이스 (seed) | `Engine/Public/RHI/IRHIDevice.h` | `GetBackend()` + `GetNativeHandle()` 2개 메서드만 |
| `eRHIBackend` enum | `Engine/Public/RHI/RHITypes.h` | DX11/DX12/Vulkan 3개 값 정의됨 |
| `eNativeHandleType` enum | `Engine/Public/RHI/RHITypes.h` | **DX11 타입만** (DX11Device/DX11DeviceContext/DX11SwapChain) |
| `RHIHandle` 구조체 | `Engine/Public/RHI/RHIHandles.h` | index+generation, Buffer/Texture/Shader handle typedef |
| `CDX11Device : public IRHIDevice` | `Engine/Public/RHI/CDX11Device.h` | DX11 전용, `GetBackend()` 오버라이드 구현 |
| `CMaterialPBR` | `Engine/Public/Renderer/CMaterialPBR.h` | API는 `IRHIDevice*` 이나 내부 `DX11ConstantBuffer` 직접 사용 |
| `CBPerMaterial` cbuffer | `Engine/Public/Renderer/CBPerMaterial.h` | b3 슬롯 PBR 상수 |
| PBR 셰이더 | `Shaders/Mesh3D_PBR.hlsl`, `Skinned3D_PBR.hlsl` | Track 1 산출물 |
| BRDF 라이브러리 | `Shaders/BRDF/BRDF_GGX.hlsli` | Track 1 산출물 |
| SSAO 셰이더 | `Shaders/SSAO/` (4개 파일) | Track 1 산출물 |
| `CEngineApp` | `Engine/Public/Framework/CEngineApp.h` | **100% DX11 전용** — `unique_ptr<CDX11Device>`, 12개 DX11Shader/DX11Pipeline 멤버 |
| `CGameInstance` RHI 게터 | `Engine/Include/GameInstance.h` | `Get_RHIDevice()` 존재하나, `Get_MeshShader()` 등은 `DX11Shader*` 반환 |

### 0-A.3 DX11 직접 의존 사이트 (DX12 포팅 시 변경 필수)

아래 8개 런타임 파일이 `IRHIDevice*` → `GetNativeHandle()` → `ID3D11Device*`/`ID3D11DeviceContext*` 직접 캐스팅:

1. `Engine/Private/Manager/UI/UI_Manager.cpp`
2. `Engine/Private/Resource/Texture.cpp`
3. `Engine/Private/Resource/Model.cpp`
4. `Engine/Private/Resource/Mesh.cpp`
5. `Engine/Private/Renderer/CMaterialPBR.cpp`
6. `Engine/Private/Renderer/ModelRenderer.cpp`
7. `Engine/Private/Renderer/PlaneRenderer.cpp`
8. `Engine/Private/Renderer/FxStaticMeshRenderer.cpp` + `Client/Private/GameObject/FX/FxSystem.cpp`

추가 DX11-only 공개 시그니처:
- `Engine/Public/Editor/ImGuiLayer.h` — ImGui 초기화/렌더가 `ID3D11Device*`/`ID3D11DeviceContext*` 직접 사용
- `Engine/Public/Manager/UI/UI_Manager.h` — `IRHIDevice*` 받으나 내부 DX11 캐스팅
- `CEngineApp.h` — 12개 `DX11Shader*`/`DX11Pipeline*` 퍼블릭 게터

### 0-A.4 진입 차단 결론

```
Week 10-13 진입 불가.
선행 필수 순서:
  1. RH-0 완료 (9개 파일 ★ RH-2 TODO 주석 박제 + CGameInstance deprecated rename)
  2. RH-1 완료 (IRHIDevice 확장 + 핸들 기반 자원 생성 API)
  3. RH-2 완료 (IRHICommandList + Public DX11 헤더 제거)
  4. RH-3 완료 (IRHIPipelineState / IRHIRenderPass / IRHIBindGroup)
  5. RH-4 완료 (64-bit handle + CRHIResourceTable)
  6. W7-9 = RH-5 DX12 compile-only bootstrap
  7. → 그 후 W10-13 진입 가능
```

---

## 0-B. Re-scoped 실행 순서

> 본 계획서의 원래 Week 순서를 코드베이스 현실에 맞게 재배치한다.

**Re-scoped order:**
1. **W10**: backend selection + deterministic capture harness + explicit DX11-only exclusion matrix.
   - `EngineConfig`에 `eRHIBackend` 필드 추가 (현재 없음 → 신설)
   - `CEngineApp`에 백엔드 분기 (현재 `unique_ptr<CDX11Device>` 하드코딩 → `IRHIDevice*` 분기)
   - `--rhi=DX11|DX12` CLI 인자 파싱 (현재 없음 → 신설)
   - `--scenario`, `--capture-frame` CLI (현재 없음 → 신설)
   - DX11-only 시스템 exclusion matrix 문서화 (ImGui/UI/FxSystem 등)
2. **W11**: port or wrap the remaining DX11-native bridge sites and the ImGui/UI path.
   - 8개 런타임 파일의 `GetNativeHandle()` 캐스팅 → `IRHICommandList` 추상화
   - `CImGuiLayer` DX12 백엔드 분기 또는 exclusion
3. **W12**: resource-state, cbuffer alignment, and sRGB correctness.
   - `DX12ResourceBarrier`, `DX12CommandList` resource state 자동화
   - cbuffer 256B alignment
   - sRGB RTV/SRV 처리
4. **W13**: PSO cache + automated diff + one gameplay match.
   - `CDX12PipelineCache` (ID3D12PipelineLibrary)
   - `Tools/PixCompare.ps1` 신설
   - DX11 vs DX12 1 frame diff + LoL 1 match 회귀

> **★ 모든 코드 예시는 target shape이다.** 현재 존재하는 코드가 아님을 명시한다.

---

## 1. Week 7-9 결과 검증 (Week 10 진입 전)

> **현재 상태**: 아래 5개 검증 항목 중 **0개 통과**. Week 7-9 미진입.

```bash
# 1. DX12 디렉토리 + 14 파일
ls Engine/Private/RHI/DX12/*.{h,cpp} | wc -l   # 현재: 0 (디렉토리 없음) → 목표: ≥ 28

# 2. Debug-DX12 / Release-DX12 빌드 통과
MSBuild Winters.sln /p:Configuration=Debug-DX12 /p:Platform=x64
# 현재: 빌드 구성 자체 없음

# 3. DX12 LoL exe 실행 → 정상 종료 (시각 무관, 검은 화면도 OK)
WintersGame.exe  # 현재: DX12 bootstrap path 없음

# 4. DX11 회귀 0
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64
WintersGame.exe  # DX11 — 이렐리아 PBR + Forward+ + SSAO 시각 정상
# 현재: DX11 빌드는 정상 작동

# 5. PIX 또는 RenderDoc 설치 + 캡처 가능
# (검증 단계에서 사용)
```

---

## 2. Week 10-13 작업 매트릭스 (4주 분할)

### 2.1 Week 10 — Backend Selection + Capture Harness + DX11-only Exclusion Matrix

> **현실 보정**: 원래 "Resource state + RTV/DSV 정합"이었으나, 백엔드 선택 인프라 자체가 없으므로 W10 범위를 재정의.

| 순서 | 작업 | 파일 | 현재 상태 | 의존 |
|---|---|---|---|---|
| **T2.W10.0** | `EngineConfig`에 `eRHIBackend backend = eRHIBackend::DX11` 필드 추가 | `Engine/Include/EngineConfig.h` | **필드 없음** | (W9) |
| **T2.W10.0a** | `CEngineApp::Initialize`에서 backend 분기 — DX12일 때 `CDX12Device::Create()` | `Engine/Private/Framework/CEngineApp.cpp` | **CDX11Device 하드코딩** | T2.W10.0 |
| **T2.W10.0b** | `--rhi=DX11\|DX12` CLI 파싱 + `--scenario` + `--capture-frame` | `Engine/Private/Framework/CEngineApp.cpp` 또는 `Client/Private/main.cpp` | **CLI 파싱 없음** | T2.W10.0 |
| **T2.W10.0c** | DX11-only exclusion matrix 문서 작성 (ImGui/UI/FxSystem 등 DX12 미지원 시스템 목록) | `.md/plan/rhi/DX11_ONLY_EXCLUSION_MATRIX.md` | **없음** | — |
| **T2.W10.1** | Resource state transition 자동화 (PIX validation 통과) | `DX12CommandList.cpp`, `DX12ResourceBarrier.cpp` | **파일 없음** | T2.W10.0a |
| **T2.W10.2** | Backbuffer Present transition (RT → Present + 역방향) | `DX12CommandList.cpp` | **파일 없음** | T2.W10.1 |
| **T2.W10.3** | RTV clear color 정합 (alpha pre-multiplied 차이 검증) | `DX12CommandList::BeginRenderPass` | **파일 없음** | (W9) |
| **T2.W10.4** | DSV clear depth/stencil 정합 (1.0 vs 0.0 reverse-Z 확인) | `DX12CommandList::BeginRenderPass` | **파일 없음** | (W9) |
| **T2.W10.5** | PIX capture 첫 frame DX11 vs DX12 비교 (수동) | PIX | — | T2.W10.1~T2.W10.4 |

### 2.2 Week 11 — cbuffer + 셰이더 binding

| 순서 | 작업 | 파일 | 현재 상태 | 의존 |
|---|---|---|---|---|
| **T2.W11.1** | cbuffer 256B alignment 자동 처리 | `DX12BufferImpl::Initialize` | **파일 없음** | (W10) |
| **T2.W11.2** | Root signature binding 정합 (descriptor table vs root constant) | `DX12RootSignature.cpp` | **파일 없음** | (W10) |
| **T2.W11.3** | sRGB Render Target / Texture 처리 (DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) | `DX12TextureImpl.cpp`, `DX12SwapChain.cpp` | **파일 없음** | (W10) |
| **T2.W11.4** | Texture swizzle (BC1/BC3 압축 텍스처 같은 alpha 처리) | `DX12TextureImpl::CreateSRV` | **파일 없음** | T2.W11.3 |
| **T2.W11.5** | 셰이더 register space 정합 (DXC `--register-shift`) | DXC compile flag | **DXC 미도입** | (W6 DXC) |

### 2.3 Week 12 — PSO 캐시

| 순서 | 작업 | 파일 | 현재 상태 | 의존 |
|---|---|---|---|---|
| **T2.W12.1** | `ID3D12PipelineLibrary` 디스크 캐시 신설 | `Engine/Private/RHI/DX12/DX12PipelineCache.{h,cpp}` | **파일 없음** | (W9) |
| **T2.W12.2** | 셰이더 hash → PSO key 매핑 (DXIL hash 또는 SHA1) | `DX12PipelineCache.cpp` | **파일 없음** | T2.W12.1 |
| **T2.W12.3** | startup 시 PSO 캐시 로드 (디스크 → ID3D12PipelineLibrary::Deserialize) | `CDX12Device::Initialize` | **파일 없음** | T2.W12.1 |
| **T2.W12.4** | shutdown 시 PSO 캐시 저장 (ID3D12PipelineLibrary::Serialize → 디스크) | `CDX12Device::~Destructor` | **파일 없음** | T2.W12.1 |
| **T2.W12.5** | PSO miss 시 stutter 측정 → ImGui 표시 | `DX12PipelineState::Initialize` | **파일 없음** | T2.W12.1 |

### 2.4 Week 13 — 시각 parity 검증 + 게임 1 매치

| 순서 | 작업 | 파일 | 현재 상태 | 의존 |
|---|---|---|---|---|
| **T2.W13.1** | PIX 자동 비교 스크립트 (DX11 vs DX12 1 frame capture 자동 diff) | `Tools/PixCompare.ps1` | **파일 없음** | (W12) |
| **T2.W13.2** | Frame diff 측정 (per-pixel RGB 차이 평균 < 1, 최대 < 5) | 스크립트 | — | T2.W13.1 |
| **T2.W13.3** | LoL 게임 1 매치 회귀 (DX11 → DX12 단일 매치 진행 가능) | 수동 게임 진행 | — | 모두 |
| **T2.W13.4** | Performance 표 (DX11 vs DX12 동일 해상도) | 문서 | — | T2.W13.3 |

---

## 3. Week 10 — Backend Selection + Resource State + RTV/DSV 정합

### 3.0 Backend Selection 인프라 신설 (T2.W10.0 ~ T2.W10.0c)

> **현실**: `EngineConfig`에 백엔드 필드가 없고, `CEngineApp`이 `CDX11Device`를 하드코딩 소유한다.

**T2.W10.0 — EngineConfig 확장** (target shape):

```cpp
// Engine/Include/EngineConfig.h — 현재 상태에서 추가할 부분
#include "RHI/RHITypes.h"   // eRHIBackend

struct EngineConfig
{
    // ── 윈도우 ───────────────────────────────────────────────
    WStr     windowTitle  = L"Winters Engine";
    uint32   windowWidth  = 1280;
    uint32   windowHeight = 720;
    bool     fullscreen   = false;
    uint32_t iNumScenes   = 16;

    // ── 렌더링 ───────────────────────────────────────────────
    bool     vsync        = true;
    uint32   targetFPS    = 60;

    // ── RHI 백엔드 (★ W10 신설) ──────────────────────────────
    eRHIBackend rhiBackend = eRHIBackend::DX11;   // CLI --rhi=DX12 로 오버라이드 가능
};
```

**T2.W10.0a — CEngineApp 백엔드 분기** (target shape):

```cpp
// Engine/Private/Framework/CEngineApp.cpp — Initialize 내 분기
bool CEngineApp::Initialize(IWintersApp* pGameApp, const EngineConfig& config)
{
    // ... 윈도우 생성 ...

    DeviceDesc desc{};
    desc.hwnd   = m_Window.GetHWND();
    desc.width  = config.windowWidth;
    desc.height = config.windowHeight;
    desc.vsync  = config.vsync;

    switch (config.rhiBackend)
    {
    case eRHIBackend::DX11:
        m_pDevice = CDX11Device::Create(desc);
        break;
#if defined(WINTERS_RHI_BACKEND_DX12)
    case eRHIBackend::DX12:
        m_pDevice = CDX12Device::Create(desc);   // W7-9 산출물
        break;
#endif
    default:
        return false;
    }
    // ...
}
```

> **주의**: `CEngineApp.h`의 `unique_ptr<CDX11Device> m_pDevice`를 `unique_ptr<IRHIDevice> m_pDevice`로 변경 필요.
> 현재 `CEngineApp.h:56`은 `unique_ptr<CDX11Device>` 하드코딩.
> 이 변경은 12개 DX11Shader/DX11Pipeline 게터의 리팩터링을 수반함 (RH-2 범위).

**T2.W10.0b — CLI 파싱** (target shape):

```cpp
// Client/Private/main.cpp 또는 CEngineApp에서
void ParseCommandLine(const char* lpCmdLine, EngineConfig& config)
{
    // --rhi=DX12
    if (strstr(lpCmdLine, "--rhi=DX12"))
        config.rhiBackend = eRHIBackend::DX12;

    // --scenario=Irelia_idle (W13 시각 parity 검증용)
    // --capture-frame=10 (W13 자동 캡처용)
}
```

**T2.W10.0c — DX11-only Exclusion Matrix** (문서):

현재 DX12 백엔드가 없으므로, DX12 부팅 시 비활성화할 시스템 목록:

| 시스템 | 파일 | DX11 직접 의존 | DX12 대응 |
|---|---|---|---|
| ImGui 렌더링 | `CImGuiLayer.h/.cpp` | `ImGui_ImplDX11_*` 직접 호출 | `ImGui_ImplDX12_*` 포팅 필요 (`Engine/External/imgui/backends/imgui_impl_dx12.{h,cpp}` 이미 존재) |
| UI Manager | `UI_Manager.h/.cpp` | `IRHIDevice` → `GetNativeHandle(DX11Device)` 캐스팅 | `IRHICommandList` 추상화 |
| ModelRenderer | `ModelRenderer.h/.cpp` | `ID3D11DeviceContext*` 직접 | `IRHICommandList` 추상화 |
| PlaneRenderer | `PlaneRenderer.h/.cpp` | `ID3D11DeviceContext*` 직접 | `IRHICommandList` 추상화 |
| FxStaticMeshRenderer | `FxStaticMeshRenderer.h/.cpp` | `ID3D11DeviceContext*` 직접 | `IRHICommandList` 추상화 |
| FxSystem | `FxSystem.h/.cpp` | `ID3D11DeviceContext*` 직접 | `IRHICommandList` 추상화 |
| Texture 로드 | `Texture.cpp` | `ID3D11Device*` + `CreateShaderResourceView` | `IRHIDevice::CreateTexture` |
| Model 로드 | `Model.cpp` | `ID3D11Device*` + buffer 생성 | `IRHIDevice::CreateBuffer` |
| Mesh 로드 | `Mesh.cpp` | `ID3D11Device*` + buffer 생성 | `IRHIDevice::CreateBuffer` |
| CMaterialPBR | `CMaterialPBR.cpp` | `DX11ConstantBuffer<CBPerMaterial>` 직접 | `IRHIBuffer` handle |
| CBlendStateCache | `BlendStateCache.h` | `ID3D11BlendState*` 직접 | `IRHIPipelineState` blend 통합 |
| DX11Shader/DX11Pipeline | `DX11Shader.h/.cpp` | DX11 전용 | `IRHIShader` + `IRHIPipelineState` |

### 3.1 Resource state 자동 transition (T2.W10.1) — TARGET SHAPE

> **전제**: `CDX12CommandList`, `CDX12Device`, `DX12ResourceBarrier` 가 W7-9 에서 생성되어야 함.
> **현재**: 이 파일들이 모두 미존재.

**`DX12CommandList::SetVertexBuffer` 예시** (target shape):

```cpp
void CDX12CommandList::SetVertexBuffer(u32_t slot, RHIBufferHandle h, u32_t stride, u32_t offset)
{
    auto* pBuf = m_pOwnerDevice->LookupBuffer(h);
    if (!pBuf) return;

    // 자동 transition: 현재 state → VERTEX_AND_CONSTANT_BUFFER
    DX12::TransitionBarrier(m_pCmdList.Get(),
                             pBuf->GetResource(),
                             pBuf->GetState(),
                             D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    pBuf->SetState(D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = pBuf->GetResource()->GetGPUVirtualAddress() + offset;
    vbv.SizeInBytes    = pBuf->GetDesc().sizeBytes - offset;
    vbv.StrideInBytes  = stride;
    m_pCmdList->IASetVertexBuffers(slot, 1, &vbv);
}
```

**`SetIndexBuffer` / `SetBindGroup` (cbuffer/SRV/UAV)** 도 동일 패턴.

### 3.2 Backbuffer Present transition (T2.W10.2) — TARGET SHAPE

```cpp
void CDX12SwapChain::Present(bool_t bVSync)
{
    auto* pBackbuffer = m_pBackBuffers[m_BackBufferIndex].Get();

    // Render target → Present transition
    DX12::TransitionBarrier(m_pOwnerDevice->GetCurrentCmdList()->GetNativeCmdList(),
                             pBackbuffer,
                             D3D12_RESOURCE_STATE_RENDER_TARGET,
                             D3D12_RESOURCE_STATE_PRESENT);

    // CommandList Close + Execute
    m_pOwnerDevice->GetGraphicsQueue()->Submit(m_pOwnerDevice->GetCurrentCmdList());

    // Present
    UINT syncInterval = bVSync ? 1 : 0;
    m_pSwapChain->Present(syncInterval, 0);
    m_BackBufferIndex = m_pSwapChain->GetCurrentBackBufferIndex();
}
```

### 3.3 RTV clear color 정합 (T2.W10.3) — TARGET SHAPE

```cpp
void CDX12CommandList::BeginRenderPass(RHIRenderPassHandle h)
{
    auto* pRP = m_pOwnerDevice->LookupRenderPass(h);
    const auto& desc = pRP->GetDesc();

    // RTV transition + clear
    for (u32_t i = 0; i < desc.colorCount; ++i)
    {
        const auto& a = desc.colorAttachments[i];
        auto* pTex = m_pOwnerDevice->LookupTexture(a.textureHandle);
        DX12::TransitionBarrier(m_pCmdList.Get(),
                                 pTex->GetResource(),
                                 pTex->GetState(),
                                 D3D12_RESOURCE_STATE_RENDER_TARGET);
        pTex->SetState(D3D12_RESOURCE_STATE_RENDER_TARGET);

        if (a.loadOp == eRHILoadOp::Clear)
        {
            m_pCmdList->ClearRenderTargetView(pTex->GetRTV(),
                                               a.clearColor, 0, nullptr);
        }
    }

    // DSV
    if (desc.hasDepth)
    {
        auto* pDSV = m_pOwnerDevice->LookupTexture(desc.depthAttachment.textureHandle);
        DX12::TransitionBarrier(m_pCmdList.Get(),
                                 pDSV->GetResource(),
                                 pDSV->GetState(),
                                 D3D12_RESOURCE_STATE_DEPTH_WRITE);
        pDSV->SetState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

        if (desc.depthAttachment.loadOp == eRHILoadOp::Clear)
        {
            m_pCmdList->ClearDepthStencilView(pDSV->GetDSV(),
                                                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                                desc.depthAttachment.clearDepth,
                                                desc.depthAttachment.clearStencil,
                                                0, nullptr);
        }
    }

    // OMSetRenderTargets
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[8];
    for (u32_t i = 0; i < desc.colorCount; ++i)
        rtvs[i] = m_pOwnerDevice->LookupTexture(desc.colorAttachments[i].textureHandle)->GetRTV();
    auto dsv = desc.hasDepth ? m_pOwnerDevice->LookupTexture(desc.depthAttachment.textureHandle)->GetDSV()
                              : D3D12_CPU_DESCRIPTOR_HANDLE{};
    m_pCmdList->OMSetRenderTargets(desc.colorCount, rtvs, FALSE, desc.hasDepth ? &dsv : nullptr);
}
```

### 3.4 PIX 첫 frame 비교 (T2.W10.5)

```bash
# 수동 절차 (W10 검증)
1. DX11 LoL 실행 → PIX capture 1 frame
2. DX12 LoL 실행 → PIX capture 1 frame (동일 view)
3. PIX 의 "Frame Comparison" 으로 RTV / DSV / draw call 비교
4. 1px 단위 차이 발견 시 cbuffer / sRGB / blend mode 디버깅
```

### 3.5 합격 게이트 (Week 10)
- [ ] `EngineConfig`에 `eRHIBackend` 필드 존재
- [ ] `--rhi=DX12` CLI 파싱 동작
- [ ] DX11-only exclusion matrix 문서 완성
- [ ] DX12 PIX validation error 0 (Resource state)
- [ ] Backbuffer present transition 정합
- [ ] RTV clear color = DX11 과 동일 시각

---

## 4. Week 11 — cbuffer + 셰이더 binding

### 4.1 cbuffer 256B alignment (T2.W11.1) — TARGET SHAPE

**DX12 요구사항**: cbuffer 의 GPU virtual address 가 256B aligned 필수.

```cpp
bool_t CDX12BufferImpl::Initialize(D3D12MA::Allocator* pAlloc, const RHIBufferDesc& desc, const void* initData)
{
    u32_t alignedSize = desc.sizeBytes;

    // cbuffer 면 256B align (D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
    if ((u32_t)desc.usage & (u32_t)eRHIBufferUsage::Constant)
    {
        alignedSize = (desc.sizeBytes + 255) & ~255u;
    }

    D3D12_RESOURCE_DESC rdesc{};
    rdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rdesc.Width  = alignedSize;
    rdesc.Height = 1;
    rdesc.DepthOrArraySize = 1;
    rdesc.MipLevels = 1;
    rdesc.SampleDesc.Count = 1;
    rdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    /* ... */
}
```

> **현재 상태**: `eRHIBufferUsage` enum 미존재. RH-1에서 신설 예정.
> 현재 cbuffer는 `DX11ConstantBuffer<T>` 템플릿으로 직접 생성 (Engine/Public/RHI/DX11/DX11ConstantBuffer.h).

### 4.2 Root signature binding 정합 (T2.W11.2) — TARGET SHAPE

DX11 의 cbuffer slot (b0/b1/b2/b3) ↔ DX12 root signature root parameter 매핑:

| DX11 slot | 현재 사용처 | DX12 root parameter | 비고 |
|---|---|---|---|
| b0 (PerFrame) | VP 행렬 (`Mesh3D.hlsl` 등 전체) | root descriptor table 0 (CBV) | shared (모든 패스) |
| b1 (PerObject) | World 행렬 | root descriptor 1 (CBV inline) | per-draw |
| b2 (CBBones) | 스키닝 본 행렬 (`Skinned3D.hlsl`) | root descriptor table 2 (CBV) | per-skinned |
| b3 (CBPerMaterial) | PBR 상수 (`Mesh3D_PBR.hlsl`) | root descriptor 3 (CBV inline) | per-draw |
| t0~t2 (Textures) | albedo/normal/metallicRoughness | root descriptor table 4 (SRV range) | per-material |
| s0 (Sampler) | 선형 필터링 | static sampler | RootSignature 에 inline |

### 4.3 sRGB 처리 (T2.W11.3) — TARGET SHAPE

**Backbuffer**: DXGI swap chain 은 `R8G8B8A8_UNORM` 으로 만들고 RTV 만 `R8G8B8A8_UNORM_SRGB` 로:

```cpp
void CDX12SwapChain::CreateRTVs()
{
    for (u32_t i = 0; i < 3; ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;   // sRGB only on RTV
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_pDevice->CreateRenderTargetView(m_pBackBuffers[i].Get(), &rtvDesc, GetRTV(i));
    }
}
```

**Albedo Texture**: WIC 로드 시 sRGB 명시 (DX11 의 `DDS_LOADER_FORCE_SRGB` 와 동일):

```cpp
desc.format = sRGB ? eRHIFormat::R8G8B8A8_UNorm_sRGB : eRHIFormat::R8G8B8A8_UNorm;
```

> **현재 상태**: `eRHIFormat` enum 미존재. RH-1에서 `RHITypes.h`에 추가 예정.
> 현재 텍스처 로드는 `CTexture::Initialize()` → `CreateWICTextureFromFile()` (DirectXTK) 직접 호출.

PBR 파이프라인:
- Albedo SRV = sRGB (자동 linearize on sample)
- Normal SRV = UNORM (선형 데이터)
- MetallicRoughness SRV = UNORM
- RTV (color output) = sRGB (자동 gamma correct on write)
- Mesh3D_PBR.hlsl 의 `pow(saturate(color), 1.0f/2.2f)` 제거 가능 (sRGB RTV 가 처리)

### 4.4 합격 게이트 (Week 11)
- [ ] cbuffer 256B align — D3D12 validation error 0
- [ ] Root signature 와 셰이더 register binding 정합 (RenderDoc 으로 확인)
- [ ] Albedo 색상 = DX11 과 동일 (sRGB 처리)

---

## 5. Week 12 — PSO 캐시

### 5.1 ID3D12PipelineLibrary (T2.W12.1) — TARGET SHAPE

**파일**: `Engine/Private/RHI/DX12/DX12PipelineCache.h`

```cpp
#pragma once
#if defined(WINTERS_RHI_BACKEND_DX12)

#include "WintersAPI.h"
#include <wrl/client.h>
#include <d3d12.h>
#include <unordered_map>
#include <string>
#include <memory>

namespace Engine
{
    class WINTERS_ENGINE CDX12PipelineCache
    {
    public:
        ~CDX12PipelineCache();
        static std::unique_ptr<CDX12PipelineCache> Create(ID3D12Device* pDevice,
                                                          const wstring_t& cachePath);

        // 캐시에 PSO 검색 (key = 셰이더 hash + state hash)
        ID3D12PipelineState* TryLoad(const wstring_t& key);

        // 새로 컴파일한 PSO 를 캐시에 저장 (메모리 + 디스크)
        bool_t Store(const wstring_t& key, ID3D12PipelineState* pPSO,
                     const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

        // shutdown 시 디스크에 일괄 저장
        bool_t Flush();

    private:
        CDX12PipelineCache();
        Microsoft::WRL::ComPtr<ID3D12Device1>          m_pDevice1;
        Microsoft::WRL::ComPtr<ID3D12PipelineLibrary>  m_pLibrary;
        wstring_t                                       m_CachePath;
        std::unordered_map<wstring_t, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_MemCache;
    };
}

#endif
```

`.cpp` Create 핵심 (target shape):

```cpp
std::unique_ptr<CDX12PipelineCache> CDX12PipelineCache::Create(ID3D12Device* pDevice, const wstring_t& cachePath)
{
    auto p = std::unique_ptr<CDX12PipelineCache>(new CDX12PipelineCache());
    p->m_CachePath = cachePath;

    // ID3D12Device1 캐스팅 (CreatePipelineLibrary 지원)
    if (FAILED(pDevice->QueryInterface(IID_PPV_ARGS(&p->m_pDevice1))))
        return nullptr;

    // 디스크 파일 읽어 Deserialize
    std::vector<u8_t> blob = ReadFile(cachePath);  // 없으면 empty
    HRESULT hr = p->m_pDevice1->CreatePipelineLibrary(
        blob.empty() ? nullptr : blob.data(),
        blob.size(),
        IID_PPV_ARGS(&p->m_pLibrary));

    if (hr == E_INVALIDARG || hr == D3D12_ERROR_DRIVER_VERSION_MISMATCH)
    {
        // 캐시 invalidate (드라이버 업그레이드 등) → 빈 라이브러리로 재생성
        p->m_pDevice1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(&p->m_pLibrary));
        // 디스크 파일 삭제
        DeleteFileW(cachePath.c_str());
    }
    else if (FAILED(hr))
    {
        return nullptr;
    }

    return p;
}

ID3D12PipelineState* CDX12PipelineCache::TryLoad(const wstring_t& key)
{
    auto it = m_MemCache.find(key);
    if (it != m_MemCache.end())
        return it->second.Get();

    // ID3D12PipelineLibrary 에서 LoadGraphicsPipeline
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pPSO;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC dummyDesc{};   // shape only
    HRESULT hr = m_pLibrary->LoadGraphicsPipeline(key.c_str(), &dummyDesc, IID_PPV_ARGS(&pPSO));
    if (SUCCEEDED(hr))
    {
        m_MemCache[key] = pPSO;
        return pPSO.Get();
    }
    return nullptr;   // miss
}

bool_t CDX12PipelineCache::Flush()
{
    // ID3D12PipelineLibrary::Serialize 으로 binary 생성
    SIZE_T size = m_pLibrary->GetSerializedSize();
    std::vector<u8_t> blob(size);
    HRESULT hr = m_pLibrary->Serialize(blob.data(), size);
    if (FAILED(hr)) return false;

    // 디스크 파일 저장
    return WriteFile(m_CachePath, blob);
}
```

### 5.2 셰이더 hash 키 (T2.W12.2) — TARGET SHAPE

```cpp
wstring_t MakePSOKey(const RHIPipelineDesc& desc, ID3D12RootSignature* pRS)
{
    // 셰이더 bytecode hash + state hash
    SHA1 sha;
    auto* pVS = LookupShader(desc.vsHandle);
    auto* pPS = LookupShader(desc.psHandle);
    sha.Update(pVS->bytecode.data(), pVS->bytecode.size());
    sha.Update(pPS->bytecode.data(), pPS->bytecode.size());
    sha.Update(&desc.blendMode, sizeof(eRHIBlendMode));
    sha.Update(&desc.depthOp, sizeof(eRHIDepthOp));
    sha.Update(&desc.cullMode, sizeof(eRHICullMode));
    sha.Update(desc.rtvFormats, sizeof(desc.rtvFormats));
    sha.Update(&desc.dsvFormat, sizeof(eRHIFormat));
    return ToHex(sha.Final());   // 40 char hex
}
```

> **현재 상태**: `RHIPipelineDesc`, `eRHIBlendMode`, `eRHIDepthOp`, `eRHICullMode` 모두 미존재. RH-3에서 신설 예정.

### 5.3 PSO Initialize 통합 (T2.W12.5) — TARGET SHAPE

```cpp
RHIPipelineHandle CDX12Device::CreatePipeline(const RHIPipelineDesc& desc)
{
    wstring_t key = MakePSOKey(desc, GetRootSignature(desc));

    auto* pCachedPSO = m_pPipelineCache->TryLoad(key);
    if (pCachedPSO)
    {
        // cache hit
        return RegisterPSO(pCachedPSO);
    }

    // miss — 컴파일 (수십~수백 ms stutter 가능)
    auto t0 = std::chrono::high_resolution_clock::now();
    auto pPSO = CompileGraphicsPSO(desc);
    auto dt = std::chrono::high_resolution_clock::now() - t0;

    // 캐시 저장
    m_pPipelineCache->Store(key, pPSO.Get(), psoDesc);

    // ImGui 로깅
    if (std::chrono::duration_cast<std::chrono::milliseconds>(dt).count() > 10)
    {
        OutputDebugStringA(("[PSOCache] miss + compile took: "
            + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(dt).count())
            + "ms\n").c_str());
    }

    return RegisterPSO(pPSO.Get());
}
```

### 5.4 합격 게이트 (Week 12)
- [ ] shutdown 시 `Cache/PSO_DX12.bin` 디스크 저장
- [ ] 다음 startup 시 cache hit → 컴파일 시간 0
- [ ] 드라이버 업그레이드 시 cache invalidate + 자동 재생성

---

## 6. Week 13 — 시각 parity 검증 + 게임 1 매치

### 6.1 PIX 자동 비교 스크립트 (T2.W13.1) — TARGET SHAPE

**Tools/PixCompare.ps1** (신설):

```powershell
# DX11 / DX12 1 frame capture 자동 비교

param([string]$Scenario = "Irelia_idle")

$pixPath = "C:\Program Files\Microsoft PIX\WinPixEventRuntime"

# 1. DX11 capture
Start-Process WintersGame.exe -ArgumentList "--rhi=DX11", "--scenario=$Scenario", "--capture-frame=10"
Start-Sleep -Seconds 5
Stop-Process -Name WintersGame
Move-Item "captures/dx11_$Scenario.wpix" "captures/dx11_$Scenario_baseline.wpix"

# 2. DX12 capture
Start-Process WintersGame.exe -ArgumentList "--rhi=DX12", "--scenario=$Scenario", "--capture-frame=10"
Start-Sleep -Seconds 5
Stop-Process -Name WintersGame
Move-Item "captures/dx12_$Scenario.wpix" "captures/dx12_$Scenario_test.wpix"

# 3. PIX command-line diff (CLI)
& "$pixPath\PixWinCli.exe" diff `
    --baseline "captures/dx11_$Scenario_baseline.wpix" `
    --test     "captures/dx12_$Scenario_test.wpix" `
    --output   "captures/diff_$Scenario.html"
```

> **주의**: `--rhi`, `--scenario`, `--capture-frame` CLI 인자는 T2.W10.0b에서 신설 필요. 현재 없음.

**WintersGame.exe CLI 인자 추가** (CGameApp::ParseCommandLine) — TARGET SHAPE:

```cpp
void CGameApp::ParseCommandLine(LPSTR lpCmdLine)
{
    // --capture-frame=10 : 10번째 frame 자동 capture + 종료
    // --scenario=Irelia_idle : 시나리오 자동 진행 (BanPick -> InGame -> 이렐리아 idle)
    // --rhi=DX12 : 명시적 backend 선택
}
```

### 6.2 Frame diff 측정 (T2.W13.2)

**합격 기준**:
- per-pixel RGB 차이 평균 (mean) < 1.0 (0~255 범위 기준)
- per-pixel RGB 차이 최대 (max) < 5.0
- alpha 채널 무시 (DX12 sRGB blending 미세 차이 허용)

**불합격 시 디버깅 순서**:
1. cbuffer 값 PIX 비교 (g_matViewProj 등)
2. RTV / DSV format 매칭
3. blend mode 매칭
4. DXIL bytecode = DX11 D3DCompile 결과와 동일 (DXC 사용 시 미세 차이 가능 → tolerance 적용)
5. Texture mip chain 매칭

### 6.3 LoL 게임 1 매치 회귀 (T2.W13.3)

**시나리오**:
1. DX11 컨피그 LoL 실행 → BanPick 이렐리아 → 5v5 봇전 1 매치 진행 (10분)
2. DX12 컨피그 LoL 실행 → 동일 시나리오 1 매치 진행
3. 차이 검증:
   - 프레임 회귀 0 (Frame ≤20ms 동일)
   - 시각 회귀 0 (이렐리아 PBR + Forward+ + SSAO 동일)
   - 게임 로직 회귀 0 (스킬 / 데미지 / 미니언 / 정글 동일)
   - 종료 정상 (Crash 0)

### 6.4 Performance 표 (T2.W13.4)

| 시나리오 | DX11 (ms) | DX12 (ms) | 차이 | 비고 |
|---|---|---|---|---|
| 단일 챔프 idle | TBD | TBD | (W13 측정) | |
| 5v5 클래시 | TBD | TBD | | DX12 multi-thread 이점 |
| PSO miss stutter | N/A | <50ms | | cache hit 시 0 |

목표:
- DX12 평균 Frame ≤ DX11 (DX12 multi-thread cmd recording 이점)
- PSO miss 시 stutter ≤ 50ms (cache 후 0)

---

## 7. 위험 시나리오

### 7.0 R-PREREQ: RH-0~RH-4 + W7-9 미완료 (★ 신규, 최대 위험)
- **시나리오**: 현재 코드베이스에 DX12 백엔드가 전혀 없음. IRHIDevice seed만 존재. W10-13 진입 불가.
- **완화**: ① RH-0 → RH-4 순차 진행 (선행 6개 phase) ② W7-9 DX12 compile-only bootstrap 완료 후 진입 ③ 각 RH phase 합격 게이트 엄격 적용
- **현재 gap 크기**: IRHIDevice 2 메서드 → 최소 6개 인터페이스 + 12개 DX12 소스 파일 + vcxproj 구성 = **RH-0~W9 전체 9+ 주**

### 7.1 R-W10-1: PIX validation barrier error 폭발
- 시나리오: 첫 frame 에 100+ barrier validation error
- 완화: ① 한 자원씩 디버깅 (PIX 의 "Resource State Watch") ② 모든 자원 초기 state = COMMON 으로 통일 후 차차 specialize

### 7.2 R-W11-1: cbuffer 256B align 누락 → 데이터 깨짐
- 시나리오: 64B cbuffer 가 align 안 되어 GPU 가 부정확 데이터 읽음
- 완화: ① CDX12BufferImpl::Initialize 에서 무조건 256B align ② DX12 validation error "non-aligned constant buffer" 즉시 검출

### 7.3 R-W11-2: sRGB 처리 차이로 색감 변화
- 시나리오: 이렐리아 색이 DX11 보다 어둡거나 채도 다름
- 완화: ① RTV format = `R8G8B8A8_UNORM_SRGB` 고정 ② Albedo SRV format = sRGB ③ 셰이더의 `pow(...,1/2.2)` 제거 (sRGB RTV 가 처리)

### 7.4 R-W12-1: PSO 캐시 디스크 파일 corrupted
- 시나리오: 비정상 종료 시 캐시 binary 일부 깨짐 → 다음 startup CreatePipelineLibrary 실패
- 완화: ① CRC32 prefix 추가 ② 실패 시 빈 라이브러리로 재생성 + 파일 삭제 ③ cache invalidate 로그 명시

### 7.5 R-W12-2: 드라이버 업그레이드 시 cache 무효화 불명
- 시나리오: NVIDIA 드라이버 업그레이드 후 cache 가 미작동
- 완화: ① `D3D12_ERROR_DRIVER_VERSION_MISMATCH` HRESULT 검출 시 자동 재생성 ② 드라이버 버전 체크 → cache key prefix

### 7.6 R-W13-1: PIX CLI diff 미지원 또는 라이센스 이슈
- 시나리오: PIX 의 CLI diff 가 production tier 만 지원
- 완화: ① 자체 RTV pixel 추출 + per-pixel diff (자체 CLI 도구) ② RenderDoc 의 capture compare API (Python script)

---

## 8. Week 10-13 통합 합격 검증

```bash
# 1. DX12 PIX validation error 0
WintersGame.exe --rhi=DX12 --validation=on
# 기대: D3D12 ERROR 0
# 현재: --rhi 파싱 없음, DX12 백엔드 없음

# 2. cbuffer 256B align (RenderDoc 캡처로 확인)
# 모든 cbuffer view 의 size 가 256B 배수

# 3. sRGB 색 정합 (이렐리아 albedo)
# RenderDoc 의 RTV pixel sample → DX11 == DX12 (1 LSB 이내)

# 4. PSO 캐시 동작
ls Cache/PSO_DX12.bin    # shutdown 후 존재
# 두 번째 startup: PSO compile time = 0 (모두 cache hit)

# 5. PIX diff 자동 비교
.\Tools\PixCompare.ps1 -Scenario Irelia_idle
# 기대: mean diff < 1.0, max diff < 5.0

# 6. 게임 1 매치 회귀 0
# DX11 매치 + DX12 매치 동일 시나리오 → 결과 일치
```

---

## 9. 부록 A — Week 10-13 진입 체크리스트

```
=== 선행 조건 (현재 미충족) ===
[ ] RH-0 완료: 9개 파일 RH-2 TODO 주석 박제 (현재 Engine 소스 0개)
[ ] RH-1 완료: IRHIDevice 확장 + eRHIFormat/eRHIBufferUsage 등 enum 신설
[ ] RH-2 완료: IRHICommandList + Public DX11 헤더 제거
[ ] RH-3 완료: IRHIPipelineState / IRHIRenderPass / IRHIBindGroup
[ ] RH-4 완료: 64-bit handle + CRHIResourceTable
[ ] Week 7-9 결과 검증 (DX12 빌드 + LoL exe 정상 종료)
[ ] PIX 또는 RenderDoc 설치

=== Week 10 진입 후 ===
[ ] git: feature/2026-05-10-week10-rh5parity branch

Week 10 — Backend Selection + Resource State + RTV/DSV:
[ ] §3.0   EngineConfig에 eRHIBackend 필드 추가
[ ] §3.0a  CEngineApp 백엔드 분기 (CDX11Device → IRHIDevice 분기)
[ ] §3.0b  --rhi / --scenario / --capture-frame CLI 파싱
[ ] §3.0c  DX11-only exclusion matrix 문서
[ ] §3.1   자동 transition (SetVertexBuffer / IndexBuffer / BindGroup)
[ ] §3.2   Backbuffer Present transition
[ ] §3.3   RTV clear color 정합
[ ] §3.4   DSV clear depth/stencil 정합
[ ] §3.5   PIX 첫 frame 비교 (수동, 디버깅)

Week 11 — cbuffer + 셰이더:
[ ] §4.1 cbuffer 256B align (DX12BufferImpl::Initialize)
[ ] §4.2 Root signature 정합 (descriptor table 매핑)
[ ] §4.3 sRGB Backbuffer (DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
[ ] §4.4 Albedo Texture sRGB SRV
[ ] §4.5 셰이더 register space 정합 (DXC --register-shift)

Week 12 — PSO 캐시:
[ ] §5.1 CDX12PipelineCache 신설 (ID3D12PipelineLibrary)
[ ] §5.2 SHA1 셰이더 hash 키
[ ] §5.3 startup 시 디스크 캐시 로드
[ ] §5.4 shutdown 시 디스크 캐시 저장
[ ] §5.5 PSO miss stutter ImGui 로깅

Week 13 — Visual parity 검증:
[ ] §6.1 PIX 자동 비교 스크립트 (Tools/PixCompare.ps1 신설)
[ ] §6.2 Frame diff 측정 (mean<1, max<5)
[ ] §6.3 LoL 게임 1 매치 회귀 0
[ ] §6.4 Performance 표 기록

검증:
[ ] §8.1 D3D12 validation error 0
[ ] §8.4 PSO 캐시 디스크 저장/로드 동작
[ ] §8.5 PIX diff 합격 기준 충족
[ ] §8.6 게임 1 매치 시각/로직 회귀 0
```

---

## 10. 부록 B — 현재 코드베이스 RHI 인프라 스냅샷 (2026-05-02)

> 이 절은 계획서 작성 시점의 코드베이스 상태를 기록하여, 실제 진입 시점의 delta를 측정하기 위함.

### 10.1 존재하는 RHI 헤더 (Engine/Public/RHI/)

```
IRHIDevice.h       — GetBackend() + GetNativeHandle() (2 메서드)
RHITypes.h         — eRHIBackend{DX11,DX12,Vulkan} + eNativeHandleType{DX11 3종}
RHIHandles.h       — RHIHandle{index,generation} + Buffer/Texture/Shader typedef
CDX11Device.h      — CDX11Device : public IRHIDevice (DX11 전용 구현)
DX11/BlendStateCache.h — CBlendStateCache (eBlendPreset 기반)
DX11/DX11ConstantBuffer.h — DX11ConstantBuffer<T> 템플릿
DX11/DX11Shader.h  — DX11Shader (VS+PS 컴파일)
```

### 10.2 미존재 RHI 인터페이스 (W10 진입 시 필요)

```
IRHICommandList.h    — Draw/Dispatch/SetVertexBuffer/SetIndexBuffer/...
IRHISwapChain.h      — Present/GetCurrentBackBuffer/...
IRHIQueue.h          — Submit(CommandList)/WaitIdle/...
IRHIPipelineState.h  — Graphics/Compute PSO
IRHIRenderPass.h     — Color/Depth attachment + load/store op
IRHIBindGroup.h      — CBV/SRV/UAV binding
IRHIBindGroupLayout.h
CRHIResourceTable.h  — Handle → resource 매핑
```

### 10.3 CEngineApp DX11 하드코딩 목록

```
Line 56: unique_ptr<CDX11Device> m_pDevice           → IRHIDevice로 변경 필요
Line 31-43: 12개 DX11Shader*/DX11Pipeline* 게터      → IRHIShader/IRHIPipeline 추상화 필요
Line 75: unique_ptr<CBlendStateCache>                → IRHIPipelineState 통합 필요
Line 6: #include "RHI/DX11/BlendStateCache.h"        → DX12 빌드 시 제거 필요
```

### 10.4 셰이더 현황 (Track 1 산출물)

```
Shaders/
  Mesh3D.hlsl           — unlit (b0 PerFrame, b1 PerObject)
  Skinned3D.hlsl        — unlit + skinning (b0, b1, b2 Bones)
  Mesh3D_PBR.hlsl       — PBR (b0, b1, b3 PerMaterial, BRDF_GGX.hlsli include)
  Skinned3D_PBR.hlsl    — PBR + skinning
  Default3D.hlsl        — 기본
  FxMesh.hlsl           — FX 메시
  FxSprite.hlsl         — FX 빌보드
  Triangle.hlsl         — 테스트
  BRDF/BRDF_GGX.hlsli  — D_GGX + G_Smith + F_Schlick + BRDF_CookTorrance
  SSAO/NormalOnly.hlsl  — 노멀 패스
  SSAO/SkinnedNormalOnly.hlsl
  SSAO/GTAO_CS.hlsl     — GTAO compute
  SSAO/GTAO_Blur_CS.hlsl
```

---

## 11. 한 줄

> **Week 10-13 = deterministic capture harness + DX11-only seam cleanup + DX12 correctness tuning (barrier / cbuffer / sRGB / PSO cache). 단, 2026-05-02 시점 DX12 백엔드 코드 0줄. RH-0~W9 선행 완료 후 진입 가능. 합격 기준도 두 단계로 나눈다: 먼저 fixed scenario parity, 그다음 full LoL match parity.**

---

## 끝.
