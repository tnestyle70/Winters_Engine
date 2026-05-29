# Ch2. RenderGraph / Lighting / GI / VT / Nanite-tier

> Winters 현재: `Engine/Public/Renderer/RenderGraph.h` 스켈레톤, `GPUDrivenPipeline.h` 스켈레톤, deferred path는 fixed (DX11에 hardcoded).
> 목표: UE5 RDG 등가 → Lumen-tier GI / Virtual Texture / Nanite-tier cluster culling.
> 레퍼런스: `UnrealEngine/Engine/Source/Runtime/RenderCore/Public/RenderGraph*.h`, `Runtime/Renderer/Private/{Lumen,Nanite,VT}/`.

---

## 1. 기초 원리 — 왜 RenderGraph인가

옛날 엔진은 렌더 파이프라인을 **고정 함수 호출 시퀀스**로 짰다:

```cpp
// 옛날
RenderShadow();
RenderGBuffer();
RenderSSAO();
RenderLighting();
RenderTranslucent();
RenderPostProcess();
```

문제:
- pass 간 **resource lifetime**을 손으로 잡아야 한다 (텍스처를 언제 free?)
- **barrier**를 손으로 잡아야 한다 (RTV→SRV 전이)
- **temp memory**가 frame당 수백 MB. 매번 alloc 비용
- **async compute**는 거의 불가능 (의존성 사람이 계산)
- pass 순서 바꾸려면 위 4개를 손으로 재배선

해결책: **렌더 패스를 데이터로 선언하고, 엔진이 DAG로 컴파일.** 이게 RenderGraph(RDG).

```cpp
// RDG 스타일
FRDGBuilder GraphBuilder(RHICmdList);
FRDGTexture* GBuffer = GraphBuilder.CreateTexture(GBufferDesc, TEXT("GBuffer"));
FRDGTexture* Shadow  = GraphBuilder.CreateTexture(ShadowDesc, TEXT("Shadow"));
AddGBufferPass(GraphBuilder, GBuffer);
AddShadowPass (GraphBuilder, Shadow);
AddLightingPass(GraphBuilder, GBuffer, Shadow, FinalColor);
GraphBuilder.Execute();
// → 빌더가 의존성 계산, barrier 자동, transient pool에서 alloc, async compute 자동 배치
```

핵심 4가지 자동화:
1. **Lifetime**: 마지막 쓰는 pass 직후 자동 free → transient pool로 메모리 재사용
2. **Barrier**: pass parameter의 access flag로 자동 transition
3. **Culling**: 결과를 안 쓰는 pass는 제거
4. **Async compute**: graphics queue와 compute queue 동시 실행 자동

---

## 2. 핵심 — UE5 RDG의 실코드

### 2.1 FRDGBuilder

`Source/Runtime/RenderCore/Public/RenderGraphBuilder.h:47~80`:

```cpp
/** Use the render graph builder to build up a graph of passes and then call Execute() to process them.
 *  Resource barriers and lifetimes are derived from _RDG_ parameters in the pass parameter struct provided
 *  to each AddPass call. The resulting graph is compiled, culled, and executed in Execute().
 */
class FRDGBuilder : public FRDGScopeState
{
public:
    RENDERCORE_API FRDGBuilder(FRHICommandListImmediate& RHICmdList,
                               FRDGEventName Name = {},
                               ERDGBuilderFlags Flags = ERDGBuilderFlags::None,
                               EShaderPlatform ShaderPlatform = GMaxRHIShaderPlatform);
    RENDERCORE_API ~FRDGBuilder();

    FRDGTexture* FindExternalTexture(FRHITexture* Texture) const;
    FRDGTextureRef RegisterExternalTexture(...);
};
```

### 2.2 AddPass — 핵심 진입점

같은 헤더 `:217~230`:

```cpp
/**
 *  Declare the type of GPU workload (i.e. Copy, Compute / AsyncCompute, Graphics) to the pass via the
 *  Flags argument. This is used to determine async compute regions, render pass setup / merging,
 *  RHI transition accesses, etc.
 */
template <typename ParameterStructType, typename ExecuteLambdaType>
FRDGPassRef AddPass(FRDGEventName&& Name,
                    const ParameterStructType* ParameterStruct,
                    ERDGPassFlags Flags,
                    ExecuteLambdaType&& ExecuteLambda);
```

`ParameterStructType`은 **매크로로 정의된 reflectable struct**다:

```cpp
BEGIN_SHADER_PARAMETER_STRUCT(FMyPassParameters, )
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SceneColor)
    SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, OutputUAV)
    SHADER_PARAMETER(FVector4f, Tint)
END_SHADER_PARAMETER_STRUCT()

// 사용
FMyPassParameters* P = GraphBuilder.AllocParameters<FMyPassParameters>();
P->SceneColor = GraphBuilder.CreateSRV(SceneColorTexture);
P->OutputUAV  = GraphBuilder.CreateUAV(OutputTexture);
P->Tint = FVector4f(1, 0, 0, 1);

GraphBuilder.AddPass(
    RDG_EVENT_NAME("MyComputePass"),
    P,
    ERDGPassFlags::Compute,
    [P](FRHICommandList& RHICmdList)
    {
        TShaderMapRef<FMyComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *P, FIntVector(8, 8, 1));
    });
```

**관전 포인트**:
- `RDG_TEXTURE_SRV` 매크로가 access flag를 자동 부여 (`SRVCompute`).
- builder가 이 메타데이터를 읽어서 **자동 barrier**를 발행.
- 람다는 **나중에** 실행됨. 람다 안에서만 RHI 호출.
- pass 등록 순서와 실행 순서가 다를 수 있다 (async compute / queue interleaving).

### 2.3 Pass DAG Compile/Execute

`FRDGBuilder::Execute()` 내부 흐름:

```text
1. 모든 등록된 pass를 dependency graph로 정렬 (parameter의 producer→consumer)
2. Cull: 결과를 누구도 안 쓰는 pass 제거
3. Schedule: graphics queue, async compute queue에 분산 배치
4. Allocate: transient texture pool에서 자원 할당 (lifetime overlap 없는 자원은 같은 메모리 재사용)
5. Insert barriers: RTV→SRV, SRV→UAV 등 ERHIAccess 전이를 RHITransitionInfo로 발행
6. Execute lambdas in order
7. Defer release: GPU가 끝낸 뒤 자원 free
```

### 2.4 Blackboard

여러 pass가 공유하는 데이터(예: 카메라 뷰포트, GBuffer 핸들)는 `FRDGBlackboard`에 박는다.

```cpp
auto& SceneTextures = GraphBuilder.Blackboard.Create<FSceneTextures>();
SceneTextures.GBufferA = GraphBuilder.CreateTexture(...);
// 다른 pass에서
const auto& ST = GraphBuilder.Blackboard.Get<FSceneTextures>();
```

---

## 3. 심화 — Deferred → GI → VT → Nanite

### 3.1 Deferred Shading

GBuffer 레이아웃 (`UE5 GBufferInfo.h`):

```text
GBufferA  : RGBA8   World Normal + per-pixel data
GBufferB  : RGBA8   Metallic, Specular, Roughness, ShadingModel
GBufferC  : RGBA8   BaseColor + AO
GBufferD  : RGBA8   Custom Data (subsurface, hair, etc.)
GBufferE  : RGBA8   precomputed shadow
SceneDepth: D32
```

Lighting pass가 위를 SRV로 읽어 각 픽셀의 BRDF 평가.

### 3.2 Lumen (Realtime GI)

`Source/Runtime/Renderer/Private/Lumen/Lumen.h`, 형제 파일들:

```text
Lumen.cpp                         최상위 엔트리
LumenMeshCards.cpp                메시별 card 표면 정보
LumenScene.cpp                    scene 카드 + voxelization
LumenRadianceCache.cpp            계층적 radiance cache
LumenSurfaceCache.cpp             material 정보 surface cache
LumenScreenProbeGather.cpp        screen-space probe
LumenHardwareRayTracingCommon.cpp HW RT 백엔드
LumenSoftwareRayTracing.cpp       SDF 트레이싱 백엔드 (RT 없는 GPU)
LumenReflections.cpp              반사
LumenDiffuseIndirect.cpp          간접광
```

원리 요약:
1. **Surface cache**: 메시별로 6개 방향 card에 material 정보 베이크 (BaseColor/Normal/Emissive)
2. **Voxelize**: card들을 sparse voxel로 모음
3. **Trace**: 픽셀당 ray (HW RT or SW SDF)
4. **Gather**: 화면 probe들로 hemisphere 적분
5. **Combine**: deferred에서 indirect lighting으로 적용

Winters에서 1차로 노릴 것은 **probe-based GI** (DDGI 등). Lumen 등가는 5년차 작업.

### 3.3 Virtual Texturing

GTA6/오픈월드 텍스처 GB 단위 → on-demand streaming.

`Source/Runtime/Renderer/Private/VT/`:

```text
VirtualTextureSystem.cpp     중앙 매니저
VirtualTexturePhysicalSpace  GPU 위 실제 page 풀
VirtualTextureSpace          virtual page table
TexturePagePool              page LRU
VT/RuntimeVirtualTexture     러닝타임 생성 (terrain blending 등)
```

원리:
1. 텍스처를 page(예: 128x128)로 쪼개고 어드레스만 가짐
2. shader에서 `Texture2DSampleVT()` 호출
3. miss 시 feedback buffer에 page 요청 기록
4. CPU가 feedback 읽어 디스크에서 page upload
5. 다음 프레임에 hit

Winters 1차 도입은 **terrain split + 캐릭터 texture array** 정도면 충분.

### 3.4 Nanite (Virtualized Geometry)

`Source/Runtime/Renderer/Private/Nanite/`:

```text
NaniteCullRaster.cpp     GPU-driven culling + software raster
NaniteDrawList.cpp       persistent draw list
NaniteMaterials.cpp      material assignment per cluster
NaniteFeedback.cpp       material id read-back
NaniteShared.cpp         page table, cluster pool
NaniteVertexFactory.cpp  mesh shader path
```

원리:
1. 메시를 **128 triangle cluster**들로 쪼개고 LOD 트리 구성
2. GPU compute가 viewport별로 클러스터 선택 (frustum/occlusion/screen size)
3. small cluster는 **software rasterizer** (compute로 픽셀 칠하기)
4. large cluster는 **HW rasterizer** (mesh shader / vertex shader)
5. shadow도 같은 시스템 재사용

Winters 1차 도입은 **GPU-driven cluster culling**까지. Software raster는 2~3년차.

`GPUDrivenPipeline.h`가 이미 스켈레톤으로 있다 — 이걸 RDG와 묶어 확장.

### 3.5 Clustered Shading

수천 개 라이트를 모두 평가하면 ALU 폭주. 해결책: **카메라 frustum을 3D cell로 쪼개고 각 cell에 영향 미치는 라이트 인덱스만 저장.**

UE5: `Source/Runtime/Renderer/Private/LightGridInjection.cpp`.

`for each light → 영향 받는 cell에 인덱스 추가 → shader는 cell 인덱스로 라이트 리스트 읽기.`

LoL/로아처럼 라이트 적은 게임은 forward+ 정도면 OK. GTA6 오픈월드는 clustered 필수.

---

## 4. Winters 매핑

### 4.1 현재 골격

`Engine/Public/Renderer/RenderGraph.h`가 이미 있다. 내부 contract을 UE5 RDG에 맞춰 확장:

```cpp
// Winters Ch2 신규/확장 (제안)
class WINTERS_ENGINE CRDGBuilder
{
public:
    explicit CRDGBuilder(IRHICommandList& cmd);
    ~CRDGBuilder();

    RDGTextureRef CreateTexture(const RDGTextureDesc& desc, const char* debugName);
    RDGBufferRef  CreateBuffer (const RDGBufferDesc&  desc, const char* debugName);

    template <typename ParamT, typename ExecLambdaT>
    RDGPassRef AddPass(const char* name, const ParamT* params, eRDGPassFlags flags, ExecLambdaT&& exec);

    void Execute();
private:
    // DAG, transient allocator, barrier tracker
};
```

매크로(UE5의 `BEGIN_SHADER_PARAMETER_STRUCT`)는 Ch13 Reflection이 들어와야 깨끗이 된다. 그 전에는 매뉴얼 access flag 등록으로 시작.

### 4.2 단계별 진입

```text
Ch2-Stage1  RDGBuilder + Texture/Buffer + AddPass 기본 (lifetime + barrier)
Ch2-Stage2  Transient texture pool (메모리 재사용)
Ch2-Stage3  Pass culling + dependency DAG compile
Ch2-Stage4  GBuffer/Shadow/Lighting을 RDG로 포팅 (현재 DX11 fixed → RDG)
Ch2-Stage5  Compute shader path + AsyncCompute flag
Ch2-Stage6  Probe-based GI (DDGI 등) — Lumen 약식
Ch2-Stage7  Cluster culling (GPU-driven, Nanite 약식)
Ch2-Stage8  Streaming VT (terrain + texture array)
Ch2-Stage9  Clustered lighting (라이트 수 증가 시)
```

### 4.3 LoL/로아/GTA6 적용 차이

| 게임 | 필요 RDG 깊이 |
|------|--------------|
| LoL (현재) | Stage1~4면 충분 (라이트 적음, GI 약함, 캐릭 적음) |
| 로스트아크 급 | Stage5 + 일부 GI + clustered lighting |
| GTA6 / 엘든링 | Stage1~9 전부 + Nanite-tier + VT |

---

## 5. 검증 명령

```powershell
# RDG 도입 후 visual smoke
.\Client\Bin\Debug-DX12\WintersGame.exe --rhi=dx12 --rdg-trace
# 기대 로그
# [RDG] frame 0: 24 passes registered, 5 culled, 19 executed
# [RDG]   pass GBuffer: 4 transitions (RTV→SRV x4)
# [RDG]   transient: alloc=128MB peak, reuse=64MB
```

UE5에는 `r.RDG.Validation=1`과 `r.RDG.Debug=1`이 있다. Winters에도 같은 토글이 Stage1부터 들어가야 한다.

---

## 6. 다음 챕터로

Ch2 Stage4 완료 후 **Ch3 World Partition**으로 갈 수 있다. 큰 월드는 RDG 위에서 visibility/streaming pass를 등록해야 효율이 나온다.
