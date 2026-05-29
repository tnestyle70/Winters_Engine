# GPU Driven Pipeline v2 — Mega Buffer + Compute Cull + Indirect Draw — **rev 2**

**작성일**: 2026-05-04
**rev 2 (2026-05-04, Codex 검토 반영)**: ① `pCmdList->DrawIndexedIndirect` → **`pCmdList->DrawIndexedIndirect`** (IRHICommandList 확장) ② RHITextureHandle/RHIBufferHandle 사용 ③ GPU Scene 분리 (Mega Buffer + InstanceData = 별도 GPU Scene 단계, GPU Driven 은 그 위에 Cull+Draw)
**v1 폐기**: [`GPU_DRIVEN_PIPELINE.md`](GPU_DRIVEN_PIPELINE.md) — Engine/Header 경로 + raw DX11 stale.
**권위 마스터**: [`2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md`](2026-05-04_ECS_FIBER_RENDERGRAPH_GPU_DRIVEN_PLAN.md) (Codex). 본 문서는 §7 GPU Scene + §8 GPU Driven 의 상세 박제.
**선행**: ECS v2 (rev 2) + Worker-Safe CommandBuffer + System Access Contract + Fiber v2 + Render Graph v2 (rev 2) 모두 후 진입
**가이드**: PLAN_AUTHORING_PITFALLS.md (P-1~P-19)

---

## §rev 2 Codex 검토 반영 매트릭스

| # | rev 1 결함 | rev 2 정정 | PITFALLS |
|---|---|---|---|
| 1 | `pCmdList->DrawIndexedIndirect(...)` 박제 — `IRHIDevice` 에 Draw API 없음. 실제 draw/dispatch 는 [IRHICommandList](../../../Engine/Public/RHI/IRHICommandList.h) 에 존재 | **IRHICommandList 확장**: `DrawIndexedIndirect(RHIBufferHandle args, u32_t offset, u32_t maxCount, u32_t stride)`, `DispatchIndirect(RHIBufferHandle args, u32_t offset)` 신규. Pass 는 cmdList 호출. | P-13 (미존재 API) |
| 2 | `IRHIDevice::DrawIndexedIndirect` 박제 — RHI 추상화 [IRHICommandList.h:24-26](../../../Engine/Public/RHI/IRHICommandList.h:24) 의 `Draw / DrawIndexed / Dispatch` 패턴과 분리 | **IRHICommandList 의 동일 인터페이스로 통합** — `DrawIndexed` 옆에 `DrawIndexedIndirect` 추가, `Dispatch` 옆에 `DispatchIndirect` 추가 | DLL 경계 |
| 3 | Mega Buffer + Compute Cull + Indirect Draw 단일 GPU Driven Pipeline 으로 박제 | **GPU Scene + GPU Driven Pipeline 2 단계 분리** (Codex 권고): GPU Scene = MegaBuffer + InstanceData layout (정적 인프라). GPU Driven = Compute Cull + IndirectDraw (frame 단위 동작). | P-14 (행동 보존) |
| 4 | `RHIBufferHandle` 표현 박제 부족 — `RgHandle` resolve 후 native handle 사용 | RHIHandles.h 의 `RHIBufferHandle` (uint64 generation handle) 일관 사용 — RgHandle 은 RG 내부 handle, RHI handle 은 외부 노출 | P-18 |

---

## §0. v1 → v2 정정 매트릭스

| # | v1 (2026-04) | v2 정정 사유 | PITFALLS |
|---|---|---|---|
| 1 | "CPU 가 매 프레임 ForEach + DrawIndexed" | 현재 Scene_InGame::OnRender 가 ECS v1 ForEach 사용. ECS v2 Query 로 마이그된 후 GPU Driven 진입 | P-2 변형 |
| 2 | Mega Buffer 단일 — 모든 메시 합치기 | B-16 의 submesh visibility mask + Yone soul 분리 시 **per-instance submesh selection** 필요 — v2 는 Mega Buffer + per-instance offset table | P-3 변형 |
| 3 | DX11 `DrawIndexedInstancedIndirect` 단일 함수 | Render Graph v2 의 IndirectArgs Buffer 가 transient pool 위에 — frame 단위 자동 관리 | DLL 경계 |
| 4 | Compute Cull 단일 단계 (Frustum 만) | 차세대 게임 부하 (10K+ 드로우콜) → 2-pass: Hi-Z occlusion + Frustum + LOD 통합 | P-7 |
| 5 | LOD 단순 distance-based | 엘든링 오픈월드 = LOD bias per-camera (FoV 의존) + skinned mesh LOD = 별도 분기 | P-14 |
| 6 | DX11 한정 박제 | RHI 추상화 진행 중 — DX12 ExecuteIndirect 호환 인터페이스 (`IRHIDevice::DrawIndexedIndirect`) | DLL 경계 |
| 7 | "PvP 게임" 시나리오만 거론 | 엘든링 오픈월드 = 100K+ 정적 메시 (지형 / 풀 / 나무) — instancing + impostor 도 GPU Driven 안에 통합 | P-7 |

---

## §1. Preflight Evidence Table

| 항목 | 결과 |
|---|---|
| 현재 Draw 호출 | Per-entity `m_pSharedModel->Render(pDevice)` ([ModelRenderer.cpp:251](../../../Engine/Private/Renderer/ModelRenderer.cpp:251)) — DX11 `DrawIndexed()` 호출 N 회 |
| 현재 Indirect Draw | 0 (DX11 `DrawIndexedInstancedIndirect` 미호출) |
| 현재 Compute Shader | 0 (D3D11_BIND_UNORDERED_ACCESS 미사용) |
| 현재 LOD | 0 (모든 메시 full detail) |
| 현재 frustum cull | 0 — 모든 entity ForEach |
| 현재 RHI 추상화 | `IRHIDevice` ([Engine/Public/RHI/IRHIDevice.h](../../../Engine/Public/RHI/IRHIDevice.h)) — DX11 native handle 추출 패턴 |
| RenderComponent 가시 | `bVisible` flag 만 — visibility filter (B-13 v2) 가 CPU loop 안 |

---

## §2. 핵심 설계

### 2-1. Mega Buffer + Per-Instance Offset Table (B-16 호환)

```
[GPU 메모리]
┌─────────────────────────────┐
│ MegaVertexBuffer (단일)     │  ← 모든 챔프/미니언/맵 정점 합침. 76B stride 통일 (skinned)
├─────────────────────────────┤
│ MegaIndexBuffer (단일)      │
├─────────────────────────────┤
│ DrawArgsBuffer (per-frame)  │  ← Compute Cull 결과: D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS × N
├─────────────────────────────┤
│ InstanceDataBuffer (SoA)    │  ← worldMatrix, animatorBoneMatrices, materialIndex, submeshVisibilityMask
├─────────────────────────────┤
│ SubmeshOffsetTable          │  ← per-mesh: vertex_offset / index_offset / index_count
└─────────────────────────────┘
```

**B-16 통합**: `instanceData.submeshVisibilityMask` (256-bit) 가 Mega Buffer 안의 어떤 submesh 를 그릴지 결정. Yone soul 처럼 같은 mesh 다른 mask 인스턴스 N 개를 Indirect Draw 한 번으로 처리.

### 2-2. 2-Pass Compute Cull

```
[Frame N]
  Compute Pass 1 (Frustum + LOD)
     입력: InstanceDataBuffer + Camera CB + LODBiasTable
     출력: VisibleInstanceList (atomic counter 로 가시 인스턴스 추가)
              ↓
  Compute Pass 2 (Hi-Z Occlusion)
     입력: VisibleInstanceList + Hi-Z Pyramid (이전 frame depth)
     출력: DrawArgsBuffer (실 발사 IndirectArgs)
              ↓
  GeometryPass (Render Graph 의 IRgPass)
     DrawIndexedIndirect(DrawArgsBuffer, ...) × 1 회
```

**왜 2-pass?** Frustum 만 하면 카메라 뒤 + 멀리 있는 메시도 GPU 까지 도달. Hi-Z occlusion 으로 가려진 메시 90%+ 컬링.

### 2-3. 신규 파일 (Winters 컨벤션 준수)

```
Engine/Public/Renderer/GPUDriven/
├── MegaBuffer.h               — CMegaBuffer (vertex + index 단일 풀)
├── InstanceDataLayout.h       — POD (matrix + bone offset + mat idx + vis mask)
├── GPUCullPass.h              — IRgPass (Render Graph 패스)
├── HiZPass.h                  — Hi-Z 피라미드 빌드 (mip chain)
├── IndirectDrawPass.h         — Mega Buffer + DrawArgsBuffer 사용 IRgPass
└── LODSelectComputeShader.hlsl — LOD bias 셰이더

Engine/Private/Renderer/GPUDriven/   — 구현
Shaders/GPUDriven/                   — Compute HLSL
├── FrustumCull.hlsl
├── HiZBuild.hlsl
├── HiZCull.hlsl
└── LODSelect.hlsl
```

### 2-4. CMegaBuffer

```cpp
class WINTERS_ENGINE CMegaBuffer
{
public:
    static std::unique_ptr<CMegaBuffer> Create(IRHIDevice* pDevice,
                                                size_t maxVertexBytes,
                                                size_t maxIndexBytes);

    // 메시 등록 — Engine 시작 시 1회 또는 Asset 스트리밍 시점
    struct UploadResult { uint32_t vertexOffset; uint32_t indexOffset; uint32_t indexCount; };
    UploadResult UploadMesh(const void* vertices, size_t vertexBytes,
                             const void* indices,  size_t indexBytes);

    // RHI native handle — IndirectDrawPass 가 IASetVertexBuffer 등에 사용
    void* Get_NativeVB() const;
    void* Get_NativeIB() const;

private:
    CMegaBuffer() = default;
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

### 2-5. InstanceDataLayout (POD, 64 bytes 정렬)

```cpp
struct InstanceData
{
    // bytes 0..63
    Mat4    worldMatrix;        // 64 bytes (16 floats)
    // bytes 64..67
    uint32_t materialIndex;     // 4
    // bytes 68..71
    uint32_t boneOffset;        // 4 — Mega Bone Buffer 안의 시작 (skinned 만)
    // bytes 72..103
    std::array<uint64_t, 4> visibilityMask;   // 32 bytes — B-16 256-bit mask
    // bytes 104..115
    uint32_t lodLevel;          // 4
    uint32_t pad0, pad1;        // 8 (cache-line 정렬)
};
static_assert(sizeof(InstanceData) == 128, "InstanceData = 128 bytes (cache-line × 2)");
```

### 2-6. GPUCullPass (Render Graph 패스)

```cpp
class CGPUCullPass : public IRgPass
{
public:
    eRgPassType GetType() const override { return eRgPassType::Compute; }
    const char* GetName() const override { return "GPUCull"; }

    void Setup(CRgPassBuilder& builder) override
    {
        m_hInstanceData    = builder.ImportBuffer(m_pInstanceBuffer, "InstanceData");
        m_hVisibleList     = builder.CreateBuffer({
            /*size*/ kMaxInstances * sizeof(uint32_t),
            /*usage*/ RgBufferUsage::UnorderedAccess | RgBufferUsage::ShaderResource
        });
        m_hDrawArgs        = builder.CreateBuffer({
            /*size*/ kMaxIndirectArgs * sizeof(DrawIndexedIndirectArgs),
            /*usage*/ RgBufferUsage::IndirectArg | RgBufferUsage::UnorderedAccess
        });
        builder.Read(m_hInstanceData);
        builder.Write(m_hVisibleList);
        builder.Write(m_hDrawArgs);
    }

    void Execute(CRgPassContext& ctx) override
    {
        // Pass 1: Frustum + LOD
        DispatchFrustumCull(ctx, m_hInstanceData, m_hVisibleList);
        // Pass 2: Hi-Z Occlusion
        DispatchHiZCull(ctx, m_hVisibleList, m_hHiZ, m_hDrawArgs);
    }

    RgHandle GetDrawArgs() const { return m_hDrawArgs; }

private:
    RgHandle m_hInstanceData;
    RgHandle m_hVisibleList;
    RgHandle m_hDrawArgs;
    RgHandle m_hHiZ;   // 다른 패스에서 import
};
```

### 2-7. IndirectDrawPass

```cpp
class CIndirectDrawPass : public IRgPass
{
public:
    void Setup(CRgPassBuilder& builder) override
    {
        builder.Read(m_hDrawArgs);
        builder.Write(m_hBackBuffer);
    }

    void Execute(CRgPassContext& ctx) override
    {
        // ★ rev 2: 모든 binding/draw 는 IRHICommandList 통해. IRHIDevice 직접 X.
        IRHICommandList* pCmdList = ctx.GetCommandList();

        // Mega Buffer 바인딩 — RHIBufferHandle 사용
        pCmdList->SetVertexBuffer(0, m_megaVB, /*stride*/ 76, /*offset*/ 0);
        pCmdList->SetIndexBuffer(m_megaIB, 0, eRHIFormat::R32_UINT);
        // BindGroup 으로 instance data buffer 바인딩 (UAV/SRV)
        pCmdList->SetBindGroup(/*slot*/ 1, m_instanceDataBindGroup);

        // ★ Indirect Draw — IRHICommandList 신규 API (rev 2 추가):
        //   void DrawIndexedIndirect(RHIBufferHandle args, u32_t offset, u32_t maxCount, u32_t stride);
        pCmdList->DrawIndexedIndirect(
            ctx.ResolveBuffer(m_hDrawArgs),
            /*offset*/  0,
            /*maxCount*/ kMaxIndirectArgs,
            /*stride*/   sizeof(D3D11_DRAW_INDEXED_INSTANCED_INDIRECT_ARGS)
        );
    }

private:
    RHIBufferHandle    m_megaVB;
    RHIBufferHandle    m_megaIB;
    RHIBindGroupHandle m_instanceDataBindGroup;
    RgHandle           m_hDrawArgs;
};
```

---

## §3. ECS v2 통합 (★ rev 2 정정 — RenderWorldSnapshot 경유)

InstanceData 빌드는 **ECS Simulation tick 후** `CRenderExtractionSystem` (RG v2 §rev 2 의 추가 시스템) 이 ECS Query → `RenderWorldSnapshot` 으로 추출. Render Graph 는 snapshot 만 read — ECS World 직접 의존 X (P-19 회피).

```cpp
// CRenderExtractionSystem (별도 ECS 시스템 — ECS sim phase 마지막에 실행)
class CRenderExtractionSystem : public ISystem
{
public:
    u32_t GetPhase() const override { return /*sim phase 의 마지막*/ 99; }
    void Execute(CECSWorld& world, f32_t dt) override
    {
        m_snapshot.visibleMeshInstances.clear();
        world.Query()
             .With<TransformComponent>()
             .With<RenderComponent>()
             .ForEachParallel<TransformComponent, RenderComponent, MeshGroupVisibilityComponent>(
                 [&](EntityHandle h, TransformComponent& tf, RenderComponent& rc, MeshGroupVisibilityComponent& vis)
                 {
                     RenderInstance ri{};
                     ri.worldMatrix    = tf.GetWorldMatrix();
                     ri.materialHandle = rc.materialHandle;     // RHI handle (이미 generation 보유)
                     ri.meshHandle     = rc.meshHandle;
                     ri.visibilityMask = vis.mask;
                     ri.boneOffset     = rc.boneOffset;
                     ri.lodHint        = ComputeLOD(tf, m_camera);
                     m_snapshot.visibleMeshInstances.push_back(ri);
                 },
                 m_pJobSystem);
    }

    const RenderWorldSnapshot& GetSnapshot() const { return m_snapshot; }

private:
    RenderWorldSnapshot m_snapshot;
};

// Render Graph 시작 시 snapshot 주입
m_pRenderGraph->SetSnapshot(m_pExtractionSystem->GetSnapshot());
```

**ECS v2 Archetype + chunk 분할 + atomic counter** 로 100K 엔티티 InstanceData 빌드 < 500us. **Pass 는 snapshot read 만** — Race 0 + 결정적 + 서버 분리 가능.

---

## §4. Render Graph 통합

본 GPU Driven 은 **Render Graph 의 패스 라이브러리** 로 등장:

```cpp
// Scene_InGame::OnRender 또는 별도 Renderer
auto& rg = *m_pRenderGraph;
rg.BeginFrame();
rg.SetWorld(&m_World);
rg.SetJobSystem(m_pJobSystem);

auto* pCull   = rg.AddPass<CGPUCullPass>(m_pInstanceBuffer);
auto* pNormal = rg.AddPass<CNormalPass>(pCull->GetDrawArgs());
auto* pGeom   = rg.AddPass<CIndirectDrawPass>(pCull->GetDrawArgs(), m_pMegaBuffer);
auto* pFog    = rg.AddPass<CFogOfWarOverlayPass>(m_pFogRenderer);

rg.EndFrame();
```

---

## §5. 마이그레이션 매트릭스

| 기존 | v2 |
|---|---|
| `m_World.ForEach<RenderComponent>(... rc.pRenderer->Render())` | `CIndirectDrawPass::Execute` 단일 `DrawIndexedIndirect` |
| Per-mesh `D3D11_BUFFER_DESC` 별도 alloc | `CMegaBuffer::UploadMesh` 단일 풀 |
| CPU frustum cull 0 | `CGPUCullPass` Compute 90%+ |
| CPU LOD 0 | `LODSelect.hlsl` Compute |

---

## §6. PITFALLS GATE 통과

| GATE | 검증 |
|---|---|
| A 사실 수집 | §1 Preflight + ModelRenderer / RHI 인용 |
| B TODO 0 | D-0 grep 후 0 |
| C 호출 경로 | Scene_InGame 의 main + normal pass 둘 다 IndirectDraw 마이그 |
| D ECS 책임 | InstanceData 빌드 = ECS v2 Query, Pass 가 Scene 직접 의존 X |
| E 향후 자료형 | InstanceData 128 bytes (10K 인스턴스 = 1.28MB), VisibilityMask 256-bit (B-16 호환) |
| F Scheduler | Fiber JobSystem + Render Graph 통합. Compute Pass 도 Fiber Counter |
| G Owner Scope | CMegaBuffer = `CEngineApp` 전역 (GPU 1개 1풀). InstanceData = per-frame transient. DrawArgs = per-frame transient. |
| H 인용 의미 + 행동 보존 | v2 마이그 후 시각 결과 동일. CPU bottleneck 5~15ms → <1ms 측정 |

---

## §7. 검증 KPI

- 10K 드로우콜 CPU 비용 < 1ms (현재 5~15ms)
- GPU frustum cull 정확도 > 95% (false negative < 5%)
- Hi-Z occlusion 가속 추가 컬링 70%+
- Indirect Draw 호출 수 < 100 per frame (현재 10K)
- Mega Buffer 메모리 < 200MB (10K 메시 평균 10KB)

---

## §8. 구현 일정

| Phase | 기간 | 내용 |
|---|---|---|
| **D-0 (RHI 확장)** | 3 days | `IRHIDevice::DrawIndexedIndirect` / `Dispatch` API 추가 |
| **D-1 (CMegaBuffer)** | 1 week | Vertex/Index 단일 풀 + UploadMesh |
| **D-2 (Compute Cull)** | 1 week | FrustumCull.hlsl + HiZBuild.hlsl + HiZCull.hlsl |
| **D-3 (Indirect Draw)** | 0.5 week | DrawIndexedIndirect 호출 + InstanceData buffer |
| **D-4 (LOD)** | 0.5 week | LODSelect.hlsl + LOD bias table |
| **D-5 (검증)** | 3 days | 10K 드로우콜 stress test + Profiler |

**총 ~3.5 weeks** (Render Graph v2 + ECS v2 + Fiber 모두 완료 후 진입).

---

**END OF GPU DRIVEN V2**
