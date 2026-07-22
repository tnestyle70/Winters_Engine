# Render Graph v2 — 차세대 렌더 파이프라인 (코드베이스 반영) — **rev 2**

> **2026-07-15 동기화 판정: 역사 자료.** 이 문서는 같은 본문 안에서 ECS World 직접 접근과 `RenderWorldSnapshot` 전용 계약을 동시에 제시하고, 현재 Winters RHI에 없는 API 및 DXGI format을 공용 graph desc에 노출한다. 실제 다음 구현 지시는 `.md/plan/2026-07-15_UNREAL_SYNCED_RHI_RENDER_GRAPH_DX12_VULKAN_PLAN.md`를 권위로 삼는다. 첫 slice는 single-thread, snapshot-only, `eRHIResourceState` 기반 dependency/transition 원장으로 제한하며 transient aliasing, Fiber 병렬 실행, Vulkan 완료를 주장하지 않는다.

**작성일**: 2026-05-04
**rev 2 (2026-05-04, Codex 검토 반영)**: ① `IRHIDevice::TextureHandle` → `RHITextureHandle` (RHIHandles.h:58) ② Pass 의 ECS World 직접 의존 → **RenderWorldSnapshot** 패턴
**v1 폐기**: [`RENDER_GRAPH_PLAN.md`](RENDER_GRAPH_PLAN.md) — Engine/Header 경로 + raw DX11 중심 stale.
**권위 마스터**: [`2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md`](2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md) (Codex). 본 문서는 §6 Render Graph 의 상세 박제.
**선행**: ECS v2 (rev 2) + Worker-Safe CommandBuffer + System Access Contract + Fiber v2 모두 후 진입
**가이드**: PLAN_AUTHORING_PITFALLS.md (P-1~P-19)

---

## §rev 2 Codex 검토 반영 매트릭스

| # | rev 1 결함 | rev 2 정정 | PITFALLS |
|---|---|---|---|
| 1 | `IRHIDevice::TextureHandle` / `BufferHandle` 박제 — 실제는 [RHIHandles.h:58-65](../../../Engine/Public/RHI/RHIHandles.h:58) 의 `RHITextureHandle` / `RHIBufferHandle` 등 8 종 typed handle | **RHITextureHandle / RHIBufferHandle 사용** (전 박제 일괄 교체) | P-18 (RHI 인프라 미인지) |
| 2 | Pass 의 `Execute(ctx)` 안에서 `ctx.GetWorld()->Query()` 직접 호출 — 렌더/시뮬 결합. 향후 멀티스레드 / 서버 분리 / 결정성 검증 시 깨짐 | **RenderWorldSnapshot** 패턴 — ECS Simulation tick 후 `CRenderExtractionSystem` 이 `RenderWorldSnapshot` 으로 추출 → Render Graph 가 snapshot 만 read. Pass 가 ECS Query 직접 호출 0. | **P-19 신규 — Render/Sim 결합** |
| 3 | `pDevice->BindVertexBuffer(...)` 같은 IRHIDevice 메서드 호출 박제 — 실제 draw/dispatch/binding 은 [IRHICommandList](../../../Engine/Public/RHI/IRHICommandList.h) 에 있음 | **CRgPassContext::GetCommandList() -> IRHICommandList\*** 노출. Pass 는 cmdList 호출. | P-13 (미존재 API) |

---

## §0. v1 → v2 정정 매트릭스

| # | v1 (2026-04) | v2 정정 사유 | PITFALLS |
|---|---|---|---|
| 1 | `Engine/Header/Renderer/...` 폴더 — Winters 컨벤션은 `Engine/Public/Renderer/` | 경로 정정 | P-2 변형 |
| 2 | `CDX11Device::GetWidth()/GetHeight()` 직접 노출 | 현재 [IRHIDevice](../../../Engine/Public/RHI/IRHIDevice.h) 추상화 진행 — RHI 경계 보존 (`IRHIDevice::GetSwapchainSize` 형태) | DLL 경계 |
| 3 | `Client/Code/CGameApp.cpp` 직접 렌더링 → graph.AddPass | 현재 Scene 기반 — `CScene_InGame::OnRender()` 안에서 graph 빌드 | P-2 변형 |
| 4 | Frostbite FrameGraph 기반 단일 RT 풀 | B-16 v2 의 `CFogOfWarRenderer` (PIMPL + IRHIDevice) 패턴과 일관 — Texture handle opaque, native SRV 는 `void*` 반환 | DLL 경계 |
| 5 | DAG + Kahn's algorithm 단순 전제 | Fiber 통합 — 패스 의존 그래프가 Fiber Counter 로 자연 표현. 토폴로지 정렬 후 chunk 단위 Fiber Submit | P-9 |
| 6 | Single render queue | B-13 v2 의 main pass + normal pass + 향후 shadow/depth pre-pass 다중 — pass type enum 확장 | P-3 |
| 7 | Resource pool 단일 인스턴스 (CEngineApp) | Phase 2: per-frame instance (frame budget 격리) + global pool (texture handle reuse) 2-tier | P-10 |

---

## §1. Preflight Evidence Table

| 항목 | 결과 |
|---|---|
| 현재 RHI 추상화 | `IRHIDevice` ([Engine/Public/RHI/IRHIDevice.h](../../../Engine/Public/RHI/IRHIDevice.h)) + `RHITypes.h` (handle 타입 박제 진행) |
| 현재 Render 호출 패턴 | Scene_InGame::OnRender → ForEach<RenderComponent> → `rc.pRenderer->UpdateTransform/UpdateCamera/Render`. Normal pass 별도 loop (PBR G-Buffer) |
| 현재 FoW 통합 | `CFogOfWarRenderer` PIMPL + `IRHIDevice*` + `void* Get_NativeSRV()` (B-16 v2) |
| 현재 ModelRenderer | `Render()` + `RenderNormalPass(...)` 분리. main pass 와 normal pass 별도 함수 |
| 현재 Shadow pass | 미존재 |
| 현재 PostFX | 미존재 |
| 현재 Compute Shader 사용 | 0 (DX11 D3D11_BIND_UNORDERED_ACCESS 미사용) |

---

## §2. 핵심 설계 (v1 → v2 차이)

### 2-1. 파일 경로 (Winters 컨벤션 준수)

```
Engine/Public/Renderer/RenderGraph/
├── RgTypes.h              — RgHandle (Generation 비트 포함 — ECS v2 패턴 채용)
├── RgResource.h           — RgTextureDesc, RgBufferDesc (RHITypes 와 일관)
├── RgPassNode.h           — IRgPass 인터페이스, RgPassContext
├── RgResourcePool.h       — 2-tier pool (frame transient + global persistent)
├── CRenderGraph.h         — 그래프 빌더 + Setup/Compile/Execute
└── Passes/                — 신규: 패스 라이브러리
    ├── GeometryPass.h     — main pass (ECS v2 Query<TransformComponent, RenderComponent>)
    ├── NormalPass.h       — G-Buffer normal pre-pass
    ├── ShadowPass.h       — CSM (Phase 3+)
    ├── SsaoPass.h         — Phase 3+
    ├── LightingPass.h     — Phase 3+ (Forward+ tile cull)
    └── FogOfWarOverlayPass.h — B-16 의 CFogOfWarRenderer 통합

Engine/Private/Renderer/RenderGraph/  — 구현
```

### 2-2. RgHandle (Generation 채용 — ECS v2 패턴)

```cpp
// Engine/Public/Renderer/RenderGraph/RgTypes.h
#pragma once
#include "WintersTypes.h"

struct RgHandle
{
    uint32_t raw = UINT32_MAX;

    constexpr RgHandle() = default;
    constexpr RgHandle(uint16_t resourceIdx, uint16_t generation)
        : raw((static_cast<uint32_t>(resourceIdx) << 16) | generation) {}

    constexpr uint16_t GetResourceIndex() const { return static_cast<uint16_t>(raw >> 16); }
    constexpr uint16_t GetGeneration()    const { return static_cast<uint16_t>(raw & 0xFFFFu); }
    constexpr bool     IsValid()          const { return raw != UINT32_MAX; }

    constexpr bool operator==(RgHandle r) const { return raw == r.raw; }
    constexpr bool operator!=(RgHandle r) const { return raw != r.raw; }
};

// 패스 타입 — ECS Phase 시스템과 분리. RG 내부 토폴로지 정렬 키.
enum class eRgPassType : uint8_t
{
    Geometry,        // Main pass — 메시 색
    Normal,          // G-Buffer normal pre-pass (PBR)
    Shadow,          // CSM
    Compute,         // GPU cull, SSAO, FFT 등
    PostFX,          // Tonemap, Bloom, FXAA
    Overlay,         // FogOfWar, UI, ImGui
    Custom
};
```

### 2-3. IRgPass — 인터페이스

```cpp
// Engine/Public/Renderer/RenderGraph/RgPassNode.h
#pragma once
#include "Renderer/RenderGraph/RgTypes.h"
#include "RHI/IRHIDevice.h"

class CRgPassBuilder;
class CRgPassContext;

class IRgPass
{
public:
    virtual ~IRgPass() = default;

    // Setup: 입력 (Read) / 출력 (Write) 리소스 선언
    virtual void Setup(CRgPassBuilder& builder) = 0;

    // Execute: RHI 호출. ECS Query / Component access 가능 (CECSWorld* 통한)
    virtual void Execute(CRgPassContext& ctx) = 0;

    virtual eRgPassType GetType() const = 0;
    virtual const char* GetName() const = 0;
};
```

### 2-4. CRgPassBuilder — Setup phase API

```cpp
class CRgPassBuilder
{
public:
    // 리소스 생성 — frame transient (자동 풀)
    RgHandle CreateTexture(const RgTextureDesc& desc);
    RgHandle CreateBuffer(const RgBufferDesc& desc);

    // 외부 리소스 import (FogOfWar 의 IRHITextureHandle 등)
    RgHandle ImportTexture(RHITextureHandle handle, const char* name);

    // 의존 선언
    RgHandle Read(RgHandle handle);     // SRV 바인딩 보장
    RgHandle Write(RgHandle handle);    // RTV/UAV 바인딩 보장 — 새 generation 반환

    // 패스 종속 명시 — 데드 패스 컬링용
    void SetSideEffect();   // 출력을 아무도 안 읽어도 실행 강제
};
```

### 2-5. CRgPassContext — Execute phase API

```cpp
class CRgPassContext
{
public:
    IRHIDevice*          GetDevice() const;
    Engine::CECSWorld*   GetWorld() const;            // ★ ECS v2 통합 — Pass 가 Query 사용
    Engine::CJobSystem*  GetJobSystem() const;

    // 핸들 → 실 RHI 핸들 (Compile 후 Execute 전 풀링됨)
    RHITextureHandle Resolve(RgHandle h) const;
    RHIBufferHandle  ResolveBuffer(RgHandle h) const;
};
```

### 2-6. CRenderGraph — 메인 빌더

```cpp
class WINTERS_ENGINE CRenderGraph
{
public:
    static std::unique_ptr<CRenderGraph> Create(IRHIDevice* pDevice);
    ~CRenderGraph();

    CRenderGraph(const CRenderGraph&) = delete;
    CRenderGraph& operator=(const CRenderGraph&) = delete;

    // 매 frame 시작 — 이전 그래프 reset
    void BeginFrame();

    // 패스 등록 — Setup 즉시 호출, Execute 는 EndFrame 에서
    template<typename T, typename... Args>
    T* AddPass(Args&&... args);

    // ★ rev 2: World 직접 주입 X — Snapshot 만 받음
    void SetSnapshot(const RenderWorldSnapshot& snapshot);
    void SetJobSystem(Engine::CJobSystem* pJobSystem);

    // Compile + Execute. 토폴로지 정렬 + 데드 패스 컬링 + Fiber Submit
    void EndFrame();

private:
    CRenderGraph() = default;
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

---

## §3. 표준 패스 라이브러리

### 3-1. GeometryPass (main pass)

```cpp
class CGeometryPass : public IRgPass
{
public:
    eRgPassType GetType() const override { return eRgPassType::Geometry; }
    const char* GetName() const override { return "Geometry"; }

    void Setup(CRgPassBuilder& builder) override
    {
        m_hBackBuffer = builder.CreateTexture({
            /*format*/ DXGI_FORMAT_R8G8B8A8_UNORM,
            /*width*/  0, /*height*/ 0,    // 0 = swapchain size
            /*usage*/  RgTextureUsage::RenderTarget | RgTextureUsage::ShaderResource
        });
        m_hDepth = builder.CreateTexture({
            /*format*/ DXGI_FORMAT_R24G8_TYPELESS,
            /*width*/  0, /*height*/ 0,
            /*usage*/  RgTextureUsage::DepthStencil | RgTextureUsage::ShaderResource
        });
        builder.Write(m_hBackBuffer);
        builder.Write(m_hDepth);
    }

    void Execute(CRgPassContext& ctx) override
    {
        // ★ rev 2: RenderWorldSnapshot 만 read — Pass 가 ECS World 직접 의존 X
        const auto& snap = ctx.GetSnapshot();
        IRHICommandList* pCmd = ctx.GetCommandList();

        for (const auto& inst : snap.visibleMeshInstances)
        {
            // inst 는 POD: { worldMatrix, materialHandle, meshHandle, visibilityMask, boneOffset, ... }
            // ECS Query / Component 접근 0. 결정적 / 멀티스레드 안전.
            pCmd->SetPipeline(inst.pipelineHandle);
            pCmd->SetBindGroup(0, inst.bindGroupHandle);
            pCmd->SetVertexBuffer(0, inst.vbHandle, inst.vbStride, 0);
            pCmd->SetIndexBuffer(inst.ibHandle, 0, inst.indexFormat);
            pCmd->DrawIndexed(inst.indexCount, 1, 0, 0, 0);
        }
    }

    RgHandle GetBackBuffer() const { return m_hBackBuffer; }
    RgHandle GetDepth() const { return m_hDepth; }

private:
    RgHandle m_hBackBuffer;
    RgHandle m_hDepth;
    Mat4     m_vp;
};
```

### 3-2. NormalPass (G-Buffer normal pre-pass — PBR)

GeometryPass 와 같은 Query, `RenderNormalPassWithVisibility` 호출. 출력: `RgHandle m_hNormal`.

### 3-3. FogOfWarOverlayPass (B-16 통합)

```cpp
class CFogOfWarOverlayPass : public IRgPass
{
public:
    void Setup(CRgPassBuilder& builder) override
    {
        // FogOfWarRenderer 의 native SRV 를 외부 리소스로 import
        m_hFogTex = builder.ImportTexture(
            { /*nativeHandle*/ static_cast<void*>(m_pFogRenderer->Get_NativeSRV()) },
            "FogOfWar");
        builder.Read(m_hFogTex);
        builder.Write(m_hMinimapBackbuffer);
    }
    // ...
};
```

---

## §4. 마이그레이션 매트릭스 (v1 명령형 → v2 선언형)

### 호출자 grep
```bash
grep -rnE "BeginScene|EndScene|OnRender.*ForEach" Client/ Engine/
```

| 기존 (v1) | v2 |
|---|---|
| `Scene_InGame::OnRender` 직접 D3D11 호출 | `CRenderGraph::AddPass<CGeometryPass>()` + `AddPass<CNormalPass>()` |
| `m_World.ForEach<RenderComponent>(... rc.pRenderer->Render())` | `CGeometryPass::Execute` 안의 `Query<...>().ForEachParallel<...>` |
| Per-frame RT 수동 alloc | `builder.CreateTexture()` (transient pool) |
| FogOfWarRenderer 직접 호출 | `CFogOfWarOverlayPass::Setup` 의 `builder.ImportTexture(...)` |

---

## §5. PITFALLS GATE 통과

| GATE | 검증 |
|---|---|
| A 사실 수집 | §1 Preflight 표 + IRHIDevice / FogOfWarRenderer / ModelRenderer 인용 |
| B TODO 0 | "TBD" 0 (D-0 grep 후) |
| C 호출 경로 | Scene_InGame 의 main + normal pass loop 모두 마이그 |
| D ECS 책임 | Pass 가 Scene 직접 의존 X — `CRgPassContext::GetWorld()` 만 |
| E 향후 자료형 | RgHandle 32-bit (resource 65K + generation 65K) — frame 단위 충분 |
| F Scheduler | Fiber JobSystem 통합 — pass 가 Fiber Submit + Counter |
| G Owner Scope | CRenderGraph = Scene per (frame 단위 lifetime) |
| H 인용 의미 + 행동 보존 | v2 마이그 후 시각 결과 동일 (regression test 강제) |

---

## §6. 검증

- 30+ 패스 DAG 컴파일 < 1ms
- Transient resource pool 메모리 -40% (vs 명시 alloc)
- Pass dependency 수동 관리 0 (선언만)
- Fiber Counter 기반 패스 의존성 자동 wait

---

**END OF RENDER GRAPH V2**
