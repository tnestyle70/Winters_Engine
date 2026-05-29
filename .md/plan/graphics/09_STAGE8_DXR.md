# Stage 8 — Hardware Ray Tracing (DXR) — 선택

## 목표

GPU 의 하드웨어 레이 트레이싱 유닛 (RT Core / Ray Accelerator) 을 활용. 소프트웨어 Path Tracer
(Stage 4) 대비 10~100배 빠름.

## 왜 선택인가

- **GPU 요구**: NVIDIA RTX 20/30/40 시리즈, AMD RX 6000/7000 시리즈, Intel Arc — 대중 GPU 아님
- **구현 복잡**: HLSL 셰이더 모델 6.3+, DXR 1.1+
- **게임 의존성**: MOBA 에선 오버스펙, 엘든링 모작 정도에 의미
- **시간 투자**: 3~4주

## DXR 아키텍처

### Acceleration Structure (AS)

```
Top-Level Acceleration Structure (TLAS)
 ├── Instance: Transform + BLAS_ptr → Irelia Champion
 ├── Instance: Transform + BLAS_ptr → Yasuo Champion
 └── Instance: Transform + BLAS_ptr → Static Terrain
    
Bottom-Level Acceleration Structure (BLAS)
 ├── 메시 A 의 BVH
 └── 메시 B 의 BVH
```

GPU 하드웨어가 직접 순회. 소프트웨어 BVH 불필요.

### Shader Stages

| 스테이지 | 역할 |
|---|---|
| **Ray Generation (RayGen)** | 픽셀별 primary ray 시작 |
| **Miss** | 레이가 아무것도 안 맞았을 때 (sky, env map) |
| **Closest Hit** | 가장 가까운 교차점 셰이딩 |
| **Any Hit** | 교차점마다 호출 (투명도, alpha test) |
| **Intersection** | 프로시저 geometry 교차 (sphere 등 — 선택) |

### Shader Binding Table (SBT)

어느 셰이더를 호출할지 매핑. 각 instance × ray type 조합별로.

## DX11 한계

DXR 은 **DX12 / Vulkan 전용**. 현재 Winters 는 DX11. 선택지:
1. **DX12 RHI 추가** (Phase E 확장)
2. **Fallback Layer 미지원** (MS 가 2019 deprecate)
3. **Vulkan RHI** (다른 API)

결론: **Phase 2 RenderGraph + DX12 RHI 추가 이후** 고려.

## BLAS / TLAS 빌드

```cpp
// Engine/Public/Renderer/DXR/AccelerationStructure.h
class CAccelerationStructure
{
public:
    // 메시 1개 → BLAS
    ComPtr<ID3D12Resource> BuildBLAS(ID3D12Device5* dev, const MeshData& mesh);
    
    // 모든 메시 인스턴스 → TLAS
    ComPtr<ID3D12Resource> BuildTLAS(ID3D12Device5* dev,
        const std::vector<InstanceDesc>& instances);

    // 동적 오브젝트용 재빌드/업데이트
    void UpdateTLAS(ID3D12GraphicsCommandList4* cmdList,
        const std::vector<InstanceDesc>& instances);

private:
    ComPtr<ID3D12Resource> m_scratchBuffer;
};
```

빌드 비용은 프레임당 0.5~2ms (수만 instance).

## Ray Generation Shader

```hlsl
// Shaders/DXR/PathTracer.hlsl
RWTexture2D<float4> g_output : register(u0);
RaytracingAccelerationStructure g_scene : register(t0);

[shader("raygeneration")]
void RayGen()
{
    uint2 pixel = DispatchRaysIndex().xy;
    uint2 screen = DispatchRaysDimensions().xy;
    
    RayDesc ray = GenerateCameraRay(pixel, screen);
    
    RayPayload payload;
    payload.radiance = 0;
    payload.throughput = 1;
    payload.rng = InitRNG(pixel, g_frameIdx);
    payload.depth = 0;
    
    TraceRay(g_scene,
        RAY_FLAG_NONE,
        0xFF,           // instance mask
        0,              // ray contribution to hit group
        0,              // multiplier
        0,              // miss shader index
        ray, payload);
    
    // Progressive accumulation
    float3 prev = g_output[pixel].rgb;
    float3 newColor = (prev * g_accumCount + payload.radiance) / (g_accumCount + 1);
    g_output[pixel] = float4(newColor, 1);
}
```

## Closest Hit Shader

```hlsl
[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    if (payload.depth >= g_maxDepth) return;
    
    uint instanceID = InstanceID();
    uint primitiveIdx = PrimitiveIndex();
    float3 hitPos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    
    // 정점 데이터 복원
    Material mat = g_materials[instanceID];
    float3 N = GetInterpolatedNormal(instanceID, primitiveIdx, attr.barycentrics);
    
    // Emissive 면 radiance 누적 후 종료
    if (mat.type == EMISSIVE) {
        payload.radiance += payload.throughput * mat.emission;
        return;
    }
    
    // NEE
    payload.radiance += payload.throughput * NextEventEstimation(hitPos, N, mat, payload.rng);
    
    // Indirect ray
    float3 newDir; float pdf;
    float3 f = SampleBSDF(N, mat, payload.rng, newDir, pdf);
    if (pdf < 1e-6) return;
    
    float NdotL = saturate(dot(N, newDir));
    payload.throughput *= f * NdotL / pdf;
    
    // Russian Roulette
    float p = max(payload.throughput.r, max(payload.throughput.g, payload.throughput.b));
    if (Rand(payload.rng) > p) return;
    payload.throughput /= p;
    
    // Recursive (depth++)
    payload.depth++;
    RayDesc newRay = { hitPos + N * EPSILON, 0, newDir, FLT_MAX };
    TraceRay(g_scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, newRay, payload);
}
```

## Miss Shader

```hlsl
[shader("miss")]
void Miss(inout RayPayload payload)
{
    float3 env = g_envMap.SampleLevel(s_linear, WorldRayDirection(), 0).rgb;
    payload.radiance += payload.throughput * env;
}
```

## Hybrid Rendering

**G-Buffer 래스터 + DXR 보조** 가 최고 성능 전략:

```
1. G-Buffer Pass (rasterization)  ← 고전 파이프라인
   Output: Albedo, Normal, Depth, Motion, Material
   
2. Direct Lighting (raster)        ← 고전 shadow map 또는 
   Output: HDR (direct contribution)
   
3. DXR Shadow                      ← ★ 하드웨어 레이 (정확한 shadow)
   Input: G-Buffer, LightDir
   Output: Shadow mask
   
4. DXR Reflection                  ← ★ SSR 대체 (off-screen 가능)
   Input: G-Buffer, Roughness
   Output: Reflection color
   
5. DXR Diffuse GI                  ← ★ DDGI 대체 또는 보강
   Input: G-Buffer
   Output: Indirect diffuse
   
6. Composite + TAA + Tonemap
```

각 DXR 패스는 1~2 SPP 로 노이지 → TAA/SVGF denoising.

## Inline Ray Tracing (DXR 1.1)

Ray Generation Shader 안 쓰고 일반 Compute Shader 에서 `RayQuery` 오브젝트로 직접 순회:

```hlsl
[numthreads(8, 8, 1)]
void CS_InlineRT(uint3 id : SV_DispatchThreadID)
{
    RayDesc ray = ...;
    RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES> q;
    q.TraceRayInline(g_scene, 0, 0xFF, ray);
    q.Proceed();
    
    if (q.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        float3 hitPos = ray.Origin + ray.Direction * q.CommittedRayT();
        // ... 셰이딩
    }
}
```

간단한 쿼리 (Shadow 체크 등) 에 적합. RayGen/Hit Group 없어도 됨.

## 성능 (RTX 3080 기준)

| 패스 | 1080p |
|---|---|
| BLAS 빌드 (한 번) | — |
| TLAS 빌드 (매 프레임) | 0.5 ms |
| DXR Shadow (1 SPP) | 1.5 ms |
| DXR Reflection (1 SPP) | 2.5 ms |
| DXR GI (1 SPP) | 3.0 ms |
| Full Path Trace (1 SPP) | 8~15 ms |

SVGF denoising 추가 0.5~1ms.

## 디버그

- **Acceleration Structure 시각화**: BLAS/TLAS 바운딩 박스 오버레이
- **RT 타이밍**: GPU profiler 로 RayGen/ClosestHit/Miss 각각 시간
- **Per-pixel Bounce Count**: 각 픽셀의 실제 실행된 bounce 수 시각화

## 하위 호환

DXR 미지원 GPU 에선 Stage 4 Software Path Tracer fallback. 에디터/스크린샷용으로만.

## 구현 순서

(Phase E 최후 스테이지. DX12 RHI 필수)

1. DX12 RHI 추가 또는 DX11/DX12 dual 지원
2. Agility SDK 1.7+ 준비
3. Acceleration Structure 빌더
4. SBT 매니저
5. 간단한 RayGen (primary ray only)
6. Closest Hit + Miss (direct lighting)
7. Recursive path tracing
8. NEE
9. Hybrid: G-Buffer + DXR Shadow
10. Hybrid: DXR Reflection (SSR 대체)
11. Hybrid: DXR Diffuse GI
12. SVGF Denoiser (Compute)
13. 실시간 60FPS 목표 튜닝

## 참고 문헌

- **Microsoft DXR Specification** (GitHub DirectX-Specs)
- **NVIDIA Ray Tracing Gems 1/2** (무료 전자책)
- **Jorge Jimenez 2019** — Ray Tracing in Games (GDC)
- **Kajita & Olsson 2019** — Ray Traced Reflections in UE4
- **NVIDIA RTXGI SDK** (오픈소스 DDGI 참고)
- **Falcor Framework** (NVIDIA 실시간 렌더링 연구 플랫폼)
