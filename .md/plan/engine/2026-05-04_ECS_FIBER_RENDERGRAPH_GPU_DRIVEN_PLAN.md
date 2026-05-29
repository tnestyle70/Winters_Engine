# Winters Engine — ECS v2 + Fiber + Render Graph + GPU Driven Framework Plan

> 작성일: 2026-05-04  
> 목적: 현재 Winters ECS/RHI 코드베이스 기준으로, `Fiber Job System -> Render Graph -> GPU Driven Pipeline` 순서를 감당할 수 있는 엔진 프레임워크 설계 나침반을 만든다.  
> 핵심 결론: Fiber를 먼저 깊게 붙이기 전에 ECS의 entity generation, structural change 규칙, system access contract를 먼저 세워야 한다.

---

## 0. 현재 코드베이스 진단

### 0.1 ECS는 "동작하는 sparse-set"이지만 아직 production ECS는 아니다

현재 구조:

- `Engine/Public/ECS/Entity.h`
  - `using EntityID = uint32_t`
  - `CEntityManager`는 `m_vecAlive`, `m_vecFreeList`, `m_iNextID`만 보유
  - ID 재사용 시 generation 검증 없음
- `Engine/Public/ECS/ComponentStore.h`
  - sparse/dense 배열 기반
  - component 자체는 연속 메모리에 저장됨
  - `Remove()`는 swap-remove라 기존 reference/index가 쉽게 무효화됨
- `Engine/Public/ECS/World.h`
  - `std::type_index -> IComponentStoreBase` map
  - `ForEach`는 1~3 component 템플릿만 존재
  - read/write access contract 없음
- `Engine/Public/ECS/SystemScheduler.h`
  - phase 단위 순차, 같은 phase 안 병렬 실행
  - system이 어떤 component를 읽고 쓰는지 scheduler가 모름
- `Engine/Public/ECS/CCommandBuffer.h`
  - 단일 vector 기반
  - worker thread에서 push하면 race

따라서 현재 ECS는 "컴포넌트 저장소와 시스템 실행 틀"은 있지만, 다음이 없다.

- generational entity handle
- stale handle 감지
- component access declaration
- structural change barrier
- query/chunk 단위 병렬화
- worker-safe command buffer
- entity lifecycle event
- system dependency DAG

이 상태에서 Fiber `WaitForCounter` yield를 깊게 붙이면, 버그가 JobSystem 문제가 아니라 ECS lifetime/race 문제로 폭발한다.

### 0.2 RHI 쪽은 오히려 좋은 선례가 이미 있다

`Engine/Public/RHI/RHIHandles.h`는 이미 64-bit generation handle 구조를 사용한다.

```cpp
value = (generation << 32) | index
```

`Engine/Public/RHI/CRHIResourceTable.h`도 index reuse 시 generation을 증가시킨다.  
ECS v2의 entity handle은 이 패턴을 거의 그대로 가져오면 된다.

### 0.3 Render Graph / GPU Driven 기존 문서의 stale 지점

기존 문서:

- `.md/plan/engine/RENDER_GRAPH_PLAN.md`
- `.md/plan/engine/GPU_DRIVEN_PIPELINE.md`

수정 필요:

- `Engine/Header`, `Engine/Code` 경로가 남아 있음 -> 현재는 `Engine/Public`, `Engine/Private`
- raw `ID3D11*` 중심 설계 -> 현재 RHI 추상화(`IRHIDevice`, `IRHICommandList`, RHI handles)를 기준으로 재작성 필요
- Render Graph가 DX11 helper처럼 설계되어 있음 -> DX11/DX12/Vulkan 공통 frame graph로 승격 필요
- GPU Driven 계획이 DX11 클래스 직접 확장 중심 -> RHI에 compute/indirect/UAV capability를 먼저 추가해야 함

---

## 1. 큰 순서

최종 순서는 다음으로 고정한다.

```text
ECS v2 Foundation
  -> Worker-Safe Command Buffer
  -> System Access Contract + Scheduler DAG
  -> Fiber Job System v2
  -> Render Graph over RHI
  -> GPU Scene
  -> GPU Driven Pipeline
```

Fiber 문서가 이미 있으므로, 이번 계획서는 Fiber 앞뒤의 프레임워크를 정의한다.

---

## 2. ECS v2 목표

목표는 LoL식 대량 유닛/스킬/투사체와 Elden Ring식 오픈월드 액터/AI/물리/애니메이션을 동시에 감당하는 ECS다.

### 2.1 Entity Handle

현재:

```cpp
using EntityID = uint32_t;
```

목표:

```cpp
struct EntityHandle
{
    u64_t value = 0;

    u32_t Index() const;
    u32_t Generation() const;
    bool_t IsValid() const;

    static EntityHandle Make(u32_t index, u32_t generation);
};
```

전환 전략:

- Stage 1: `EntityID` 이름은 유지하되 내부 표현을 64-bit로 전환
- Stage 2: 신규 코드는 `EntityHandle` 사용
- Stage 3: legacy API `EntityID` 제거 또는 alias only

권장:

```cpp
using EntityID = u64_t; // migration bridge
```

단, `NULL_ENTITY = 0` 규칙은 유지한다.

### 2.2 Entity Record

`CEntityManager`는 다음 record table을 가져야 한다.

```cpp
struct EntityRecord
{
    u32_t generation = 1;
    bool_t bAlive = false;

    // Stage 2 archetype 준비 필드
    u32_t archetypeId = INVALID_U32;
    u32_t row = INVALID_U32;
};
```

파괴 규칙:

- Destroy 시 `bAlive=false`
- generation 증가
- free list에 index 반환
- 이전 handle은 `IsAlive(handle)==false`

이게 없으면 "죽은 미니언 ID를 타겟으로 들고 있던 스킬/투사체가 새로 생성된 챔피언을 때리는" 류의 버그를 막을 수 없다.

### 2.3 Component Store v1.5

바로 archetype으로 갈 필요는 없다. 먼저 현재 sparse-set을 generation-aware로 만든다.

필수 변경:

- sparse index는 `handle.Index()` 기준
- `Has/Get/Remove`는 generation validation을 통과해야 함
- dense entity 배열에는 full handle 저장
- `Remove` 후 이전 handle 접근은 assert 또는 false

추가 API:

```cpp
template<typename T>
T* TryGetComponent(EntityHandle e);

template<typename T>
const T* TryGetComponent(EntityHandle e) const;
```

worker job에서는 assert 기반 `GetComponent`보다 `TryGetComponent`가 안전하다.

### 2.4 Component Type Registry

`std::type_index` map은 작은 코드베이스에서는 편하지만 큰 엔진에서는 나침반이 약하다.

목표:

```cpp
using ComponentTypeID = u16_t;

struct ComponentTypeInfo
{
    ComponentTypeID id;
    const char* name;
    u32_t size;
    u32_t alignment;
    void (*construct)(void*);
    void (*destroy)(void*);
    void (*moveConstruct)(void* dst, void* src);
};
```

효과:

- query cache 가능
- system access contract 가능
- archetype/chunk 전환 가능
- serializer/network replication 기반이 됨

### 2.5 Archetype/Chunk는 Stage 2

처음부터 full archetype ECS로 갈 필요는 없다. Winters는 이미 sparse-set 기반 코드가 많이 있다.

권장 구조:

- ECS v1.5: generation-aware sparse-set
- ECS v2.0: hot path component만 chunk/archetype storage
- ECS v2.5: gameplay query 대부분을 chunk iterator로 전환

Hot path 후보:

- `TransformComponent`
- `VelocityComponent`
- `NavAgentComponent`
- `MinionStateComponent`
- `HealthComponent`
- `RenderComponent`
- `AnimationComponent`
- `PhysicsBodyComponent`

Sidecar sparse-set 유지 후보:

- debug/editor metadata
- rare state
- champion-specific temporary state
- script component

---

## 3. Worker-Safe Structural Change

### 3.1 원칙

worker job은 world 구조를 직접 바꾸면 안 된다.

금지:

- `CreateEntity`
- `DestroyEntity`
- `AddComponent`
- `RemoveComponent`
- component vector resize를 유발하는 작업

허용:

- 자기 chunk/range의 write component 수정
- declared read component 읽기
- command buffer에 structural command 기록

### 3.2 CommandBuffer v2

현재 `CCommandBuffer`는 단일 vector다. Fiber/worker 환경에서는 per-worker buffer로 바꾼다.

목표:

```cpp
class CCommandBuffer
{
public:
    void Resize_Workers(u32_t workerCount);

    void DeferCreate(CreateFn fn);
    void DeferDestroy(EntityHandle e);
    void DeferAddComponent(EntityHandle e, ComponentTypeID type, const void* data);
    void DeferRemoveComponent(EntityHandle e, ComponentTypeID type);

    void Playback(CWorld& world);

private:
    std::vector<CommandList> m_vecPerWorker;
};
```

slot 규칙:

- main slot = 0
- worker slot = `CJobSystem::Get_WorkerSlot()`
- Fiber yield 가능한 함수에서는 slot 캐시 금지

### 3.3 Playback Barrier

각 scheduler phase 끝에서 command buffer를 playback한다.

```text
Phase N systems execute
  -> WaitForCounter
  -> CommandBuffer.Playback()
  -> Entity event flush
  -> Phase N+1
```

Render snapshot은 모든 structural change가 끝난 뒤 생성한다.

---

## 4. System Access Contract

### 4.1 왜 필요한가

현재 scheduler는 같은 phase에 system이 여러 개 있으면 병렬 실행한다. 하지만 어떤 system이 어떤 component를 쓰는지 모른다.

Fiber era에는 이 방식이 위험하다. scheduler가 다음을 알아야 한다.

- read components
- write components
- structural write 여부
- external resource read/write
- render/physics/audio thread affinity

### 4.2 목표 API

```cpp
struct SystemAccessDesc
{
    ComponentAccess* components;
    u32_t componentCount;
    ResourceAccess* resources;
    u32_t resourceCount;
    bool_t bStructuralWrite;
};

class ISystem
{
public:
    virtual u32_t GetPhase() const = 0;
    virtual SystemAccessDesc GetAccess() const = 0;
    virtual void Execute(CWorld& world, f32_t dt) = 0;
};
```

단계적 도입:

- Stage 1: 기존 `ISystem` 유지, 선택적 `GetAccess()` 추가
- Stage 2: scheduler가 conflict graph를 만들고 safe groups 병렬화
- Stage 3: query chunk 단위 job split

### 4.3 Scheduler DAG

현재:

```text
phase -> vector<ISystem>
```

목표:

```text
phase -> access graph -> parallel batches -> chunk jobs
```

예시:

```text
Phase 2
  Batch A: Vision(read Transform, write Vision)
  Batch B: MinionAI(read Transform/Health, write MinionDecisionBuffer)
  Barrier: CommandBuffer playback
```

conflict 규칙:

- Read/Read 병렬 가능
- Read/Write 충돌
- Write/Write 충돌
- StructuralWrite는 phase barrier 필요
- external resource write는 명시 owner 필요

---

## 5. Fiber Job System과 ECS의 접점

`FIBER_JOB_SYSTEM_v2.md`는 계속 유지하되 다음 전제를 추가해야 한다.

### 5.1 FiberFull 전 ECS 필수 조건

M3 `WaitForCounter` yield 전에 완료되어야 하는 것:

- generational entity handle
- command buffer per-worker
- system access contract 최소 버전
- no component reference across yield 규칙
- `TryGetComponent` 기반 stale handle 안전 경로
- worker job 안 structural change 금지

### 5.2 Chunk Job 규칙

Fiber job의 기본 단위는 "entity 하나"가 아니라 "chunk/range"여야 한다.

```text
Query<Read<Transform>, Write<Velocity>>
  -> chunks
  -> chunk range jobs
  -> counter wait
```

장점:

- Submit 수 감소
- cache locality 증가
- false sharing 감소
- Render/Animation/Physics 같은 hot loop에 맞음

### 5.3 Yield 금지 구간

다음 구간에서는 Fiber yield를 금지한다.

- raw component pointer/reference를 들고 있는 동안
- per-worker slot을 캐시한 뒤 push 전
- command buffer playback 중
- RHI resource table 접근 중
- render thread only resource 접근 중

규칙:

```text
handle은 yield를 건너도 된다.
component reference는 yield를 건너면 안 된다.
```

---

## 6. Render Graph 계획 보정

### 6.1 기존 계획서 수정 방향

`RENDER_GRAPH_PLAN.md`는 다음 방향으로 개정한다.

수정:

- `Engine/Header` -> `Engine/Public`
- `Engine/Code` -> `Engine/Private`
- raw `ID3D11*` public API 제거
- `CDX11Device*` 의존 -> `IRHIDevice*`, `IRHICommandList*`
- resource handle은 `RHITextureHandle`, `RHIBufferHandle` 기반
- DX11 hazard 처리와 DX12 barrier 처리를 backend policy로 분리

### 6.2 파일 배치

```text
Engine/Public/Renderer/RenderGraph/
  RenderGraphTypes.h
  RenderGraphResource.h
  RenderGraphPass.h
  RenderGraphBuilder.h
  RenderGraph.h

Engine/Private/Renderer/RenderGraph/
  RenderGraph.cpp
  RenderGraphCompiler.cpp
  RenderGraphResourcePool.cpp
  RenderGraphBarrierPlanner.cpp
```

클래스명:

- `CRenderGraph`
- `CRenderGraphBuilder`
- `CRenderGraphCompiler`
- `CRenderGraphResourcePool`

파일명은 C 접두사 없이 둔다.

### 6.3 핵심 API

```cpp
class CRenderGraph
{
public:
    void Initialize(IRHIDevice* pDevice);
    void BeginFrame(const RenderGraphFrameDesc& desc);

    template<typename SetupFn, typename ExecuteFn>
    void AddPass(const char* name, eRenderGraphPassType type,
                 SetupFn&& setup, ExecuteFn&& execute);

    void Compile();
    void Execute(IRHICommandList* pCmd);
    void EndFrame();
};
```

Pass execute는 native DX11 context가 아니라 RHI command list를 받는다.

```cpp
using RenderGraphExecuteFn =
    std::function<void(RenderGraphContext&, IRHICommandList&)>;
```

### 6.4 Compile 단계

Compile은 다음 순서를 가진다.

```text
1. pass resource read/write 수집
2. producer/consumer dependency 생성
3. dead pass culling
4. topological sort
5. resource lifetime 계산
6. transient resource aliasing
7. barrier/hazard plan 생성
8. async compute 후보 표시
```

DX11:

- explicit barrier는 없음
- SRV/RTV/UAV conflict unbind 필요
- UAV barrier는 제한적이며 compute/graphics hazard를 보수적으로 처리

DX12:

- `eRHIResourceState` transition plan 생성
- queue ownership transfer는 추후 async compute 단계에서 추가

### 6.5 ECS와 Render Graph의 경계

Render Graph는 `CWorld`를 직접 순회하지 않는다.

올바른 흐름:

```text
ECS Simulation
  -> RenderExtractionSystem
  -> RenderWorldSnapshot
  -> RenderGraph pass registration
  -> RenderGraph execute
```

`RenderWorldSnapshot`은 frame immutable이어야 한다.  
이 구조가 있어야 Fiber simulation과 render thread가 서로 물고 늘어지지 않는다.

---

## 7. GPU Driven Pipeline 계획 보정

### 7.1 기존 계획서 수정 방향

`GPU_DRIVEN_PIPELINE.md`는 다음을 반영해 개정한다.

- raw DX11 helper 중심 -> RHI capability 중심
- `DX11StructuredBuffer` 단독 설계 -> `RHIBufferDesc` flags 확장
- indirect draw는 `IRHICommandList` API로 승격
- `RenderComponent`는 generation-aware entity와 GPU instance generation을 가져야 함
- GPUScene은 ECS world를 직접 잡지 않고 `RenderWorldSnapshot`에서 갱신

### 7.2 RHI 필수 확장

현재 `IRHICommandList`에는 `Draw`, `DrawIndexed`, `Dispatch`는 있지만 indirect draw가 없다.

추가 필요:

```cpp
virtual void DrawIndexedIndirect(RHIBufferHandle argsBuffer,
                                 u32_t argsOffsetBytes) = 0;

virtual void CopyBuffer(RHIBufferHandle dst,
                        RHIBufferHandle src,
                        u32_t sizeBytes) = 0;

virtual void ClearUAV(RHIBufferHandle buffer,
                      u32_t value) = 0;
```

`RHIBufferDesc` 확장:

```cpp
enum class eRHIBufferUsage : u32_t
{
    Vertex,
    Index,
    Constant,
    Structured,
    Raw,
    IndirectArgs,
};

struct RHIBufferDesc
{
    u32_t sizeBytes;
    u32_t strideBytes;
    eRHIBufferUsage usage;
    u32_t bindFlags; // SRV/UAV/Indirect
};
```

### 7.3 GPU Scene

목표:

```text
RenderWorldSnapshot
  -> CGPUScene::Update()
  -> GPUInstanceBuffer
  -> GPUMeshDescriptorBuffer
  -> GPUMaterialBuffer
```

`RenderComponent`에는 다음이 필요하다.

```cpp
struct RenderComponent
{
    MeshHandle mesh;
    MaterialHandle material;
    u32_t gpuInstanceIndex = INVALID_U32;
    u32_t gpuInstanceGeneration = 0;
    bool_t bVisible = true;
    bool_t bCastShadow = true;
};
```

ECS entity generation과 GPU instance generation을 둘 다 둔다.

### 7.4 Pass 순서

Render Graph 위에서 다음 pass를 등록한다.

```text
GPUSceneUpload
  -> DepthPrepass or HiZSource
  -> HiZBuild
  -> FrustumCullCS
  -> OcclusionCullCS
  -> LODSelectCS
  -> CompactVisibleInstancesCS
  -> BuildIndirectArgsCS
  -> IndirectDepthPrepass
  -> IndirectGBuffer or ForwardOpaque
```

DX11 제한:

- mesh/material group별 `DrawIndexedInstancedIndirect` 호출
- multi-draw는 DX12 `ExecuteIndirect` 단계에서 확장

DX12 목표:

- bindless descriptor table
- ExecuteIndirect
- mesh shader path
- async compute culling

---

## 8. AI Codebase Compass 적용

큰 코드베이스에서는 AI에게 모든 파일을 읽게 하면 안 된다. 모듈별 나침반을 제공해야 한다.

### 8.1 모듈별 Compass 문서 형식

각 큰 모듈은 다음 문서를 가진다.

```text
.ai-readiness/modules/<module>/COMPASS.md
```

필수 항목:

- 책임 범위
- 절대 건드리면 안 되는 invariants
- public entry points
- 핵심 파일 5~15개
- dependency 방향
- thread/fiber safety 규칙
- resource lifetime 규칙
- debug/profiler 진입점
- 관련 tests
- 흔한 gotchas

### 8.2 Winters 우선 모듈

최초 12개 Compass:

1. `ecs-core`
2. `ecs-scheduler`
3. `job-system`
4. `render-rhi`
5. `render-graph`
6. `gpu-driven`
7. `resource-model`
8. `animation`
9. `navigation`
10. `ai-gameplay`
11. `client-scene-ingame`
12. `build-vcxproj`

UE5급으로 커지면 50~60개 compass가 된다.  
AI는 grep으로 전수 스캔하기 전에 해당 module compass를 먼저 읽고, 거기서 지정한 entry point만 따라간다.

---

## 9. 구현 마일스톤

### E0. ECS Generation Safety

작업:

- `EntityHandle` 도입
- `CEntityManager` generation table
- `CWorld::IsAlive(handle)` generation 검증
- `CComponentStore<T>` generation-aware sparse lookup
- `TryGetComponent` 추가

검증:

- entity destroy 후 same index 재사용
- old handle로 `IsAlive == false`
- old handle로 component lookup 실패
- projectile target stale handle 테스트

### E1. CommandBuffer v2

작업:

- per-worker command lists
- structural command enum
- playback barrier
- generation validation on playback

검증:

- worker 16개가 create/destroy/add/remove defer
- playback 후 entity/component count 정확
- stale destroy command 무시 또는 debug assert

### E2. System Access Contract

작업:

- optional `GetAccess()` 추가
- scheduler conflict graph
- safe parallel batch
- debug view: phase/batch/system access 출력

검증:

- read/read 병렬
- read/write 분리
- write/write 분리
- structural write barrier

### E3. Fiber ECS Integration

작업:

- chunk/range job helper
- no component ref across yield rule 문서화
- scheduler가 chunk jobs submit
- Fiber v2 M1/M2/M3 재검증

검증:

- nested wait
- cross-worker resume
- command buffer slot race 0
- component ref lint/checklist

### R0. Render Graph RHI-first Rewrite

작업:

- 기존 `RENDER_GRAPH_PLAN.md` 개정
- `Engine/Public/Renderer/RenderGraph` 구조 확정
- RHI handle 기반 resource/pass API
- DX11 hazard policy + DX12 barrier policy 분리

검증:

- backbuffer clear pass
- forward opaque pass
- normal/SSAO pass graph 등록
- dead pass culling
- transient texture reuse

### G0. GPU Driven Foundation

작업:

- `GPU_DRIVEN_PIPELINE.md` 개정
- RHI indirect/structured/UAV capability 확장
- `RenderComponent` + GPU instance generation
- `CGPUScene` from RenderWorldSnapshot

검증:

- CPU draw path와 GPU driven path 동일 화면
- 10K instance frustum culling
- indirect args readback count
- DX11 per mesh-group indirect draw

---

## 10. 계획서 수정 목록

수정 대상:

1. `.md/plan/engine/FIBER_JOB_SYSTEM_v2.md`
   - M3 wait/yield race 수정
   - ECS v2 prerequisite 추가
   - no component ref across yield 규칙 추가

2. `.md/plan/engine/RENDER_GRAPH_PLAN.md`
   - 경로 최신화
   - RHI-first 설계로 개정
   - raw DX11 public API 제거
   - RenderWorldSnapshot 경계 추가

3. `.md/plan/engine/GPU_DRIVEN_PIPELINE.md`
   - RHI capability 기준으로 개정
   - ECS generation/GPU instance generation prerequisite 추가
   - Render Graph pass chain 기준으로 재구성

신규 문서:

1. `.md/plan/engine/ECS_V2_GENERATIONAL_FRAMEWORK.md`
2. `.md/plan/engine/RENDER_GRAPH_RHI_FIRST_v2.md`
3. `.md/plan/engine/GPU_DRIVEN_PIPELINE_v2.md`
4. `.ai-readiness/modules/*/COMPASS.md`

---

## 11. 한 줄 요약

Winters가 LoL 규모의 대량 시뮬레이션과 Elden Ring 규모의 월드/AI/렌더링을 동시에 감당하려면, Fiber보다 먼저 ECS를 generation-aware + access-contract 기반으로 세우고, 그 위에 Fiber scheduler, RHI-first Render Graph, GPU Scene/Indirect Draw를 순서대로 얹어야 한다.
