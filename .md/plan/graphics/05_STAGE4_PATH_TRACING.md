# Stage 4 — 오프라인 Path Tracing (Reference 렌더러)

## 목표

**Kajiya 1986 렌더링 방정식을 완전히 풀어내는 Reference 렌더러**. 실시간 GI (Stage 5) 결과와
비교할 Ground Truth.

## 왜 오프라인 Path Tracing 인가

- 모든 간접광 / 반사 / 굴절 / Caustic 물리적으로 정확
- 실시간 기법들의 근사 오차 측정 기준
- 엔진 아티스트가 "정답" 확인용 스크린샷 렌더
- 포트폴리오로 Cornell Box / 조명 풍경 장면 데모

## 알고리즘

### Whitted 1980 (Recursive Ray Tracing)

직사광 + 완전 거울 반사 + 굴절. 간접 확산 X. 학습용.

### Kajiya 1986 (Path Tracing)

각 픽셀에 대해:
1. 카메라 → 픽셀 → 씬 레이 쏨
2. 첫 hit 지점 찾기
3. 직사광 계산 (Light Sampling)
4. BRDF 기반 다음 바운스 방향 샘플
5. 재귀 (또는 iterative, Russian Roulette 종료)

## 핵심 파이프라인

```
GenerateCameraRay(pixel) → Ray
for bounce in 1..maxDepth:
    hit = TraceScene(ray)
    if no hit: return envMap(ray.dir)
    
    // Direct light (Next Event Estimation)
    L += NEE(hit, lights) × throughput
    
    // Indirect light (BRDF bounce)
    sampleBRDF → newDir
    throughput *= BRDF × cos / PDF
    
    // Russian Roulette
    if rand > throughput.max: break
    throughput /= throughput.max
    
    ray = { hit.pos, newDir }

return L
```

## BVH 가속 구조 (Physics Stage 3 공유)

Path Tracer 는 수백만 개 레이 → BVH 필수.

```cpp
// Engine/Public/Renderer/PathTracer/BVH.h
struct BVHNode {
    AABB     aabb;
    u32_t    leftChild, rightChild;
    u32_t    primitiveIdx, primitiveCount;   // 리프만
    bool_t   IsLeaf() const { return primitiveCount > 0; }
};

class CBVHBuilder
{
public:
    // SAH (Surface Area Heuristic) 기반 최적 트리
    std::vector<BVHNode> Build(const std::vector<Triangle>& triangles);

private:
    f32_t ComputeSAHCost(const std::vector<Triangle>& tris, i32_t start, i32_t count, i32_t axis, f32_t splitPos);
};
```

SAH 비용 함수:
```
cost = C_trav + (SA(L)/SA(P)) × n_L × C_tri + (SA(R)/SA(P)) × n_R × C_tri
```

### GPU BVH 순회 (Stackless)

```hlsl
bool TraceBVH(Ray ray, out HitInfo hit)
{
    hit.t = FLT_MAX;
    uint stack[64];
    int sp = 0;
    stack[sp++] = 0;   // root
    
    while (sp > 0) {
        uint nodeIdx = stack[--sp];
        BVHNode node = g_bvhNodes[nodeIdx];
        
        float tMin, tMax;
        if (!RayAABB(ray, node.aabb, tMin, tMax)) continue;
        if (tMin > hit.t) continue;
        
        if (node.IsLeaf()) {
            for (uint i = 0; i < node.primitiveCount; ++i) {
                Triangle tri = g_triangles[node.primitiveIdx + i];
                float t; float2 bary;
                if (RayTriangle(ray, tri, t, bary) && t < hit.t) {
                    hit.t = t;
                    hit.bary = bary;
                    hit.primitiveIdx = node.primitiveIdx + i;
                }
            }
        } else {
            stack[sp++] = node.leftChild;
            stack[sp++] = node.rightChild;
        }
    }
    
    return hit.t < FLT_MAX;
}
```

## Next Event Estimation (NEE)

각 hit 에서 **빛을 직접 샘플링** — 분산 대폭 감소.

```hlsl
float3 NextEventEstimation(HitInfo hit, Material mat)
{
    // 1. 랜덤 라이트 선택 (uniform 또는 power-weighted)
    uint lightIdx = RandLightIdx(rng);
    Light light = g_lights[lightIdx];
    float lightPdf = 1.0 / g_numLights;
    
    // 2. 빛 표면 샘플
    float3 lightSamplePos;
    float samplePdf;
    SampleLight(light, rng, lightSamplePos, samplePdf);
    
    // 3. 가시성 체크
    float3 toLight = lightSamplePos - hit.pos;
    float  dist = length(toLight);
    float3 L = toLight / dist;
    
    Ray shadowRay = { hit.pos + hit.normal * EPSILON, L, dist - EPSILON };
    if (TraceBVH(shadowRay, _)) return 0;   // 가림
    
    // 4. BRDF × L × cos / (lightPdf * samplePdf)
    float3 f = BRDF_GGX(hit.normal, hit.V, L, mat);
    float  NdotL = saturate(dot(hit.normal, L));
    float  G = NdotL / (dist * dist);   // Geometry term
    
    return f * light.emission * G / (lightPdf * samplePdf);
}
```

### MIS NEE + BRDF Sampling

빛 샘플만으론 거울 면 처리 어려움. BRDF 샘플과 MIS 결합:

```
L = NEE (with MIS weight) + BRDF_sample (with MIS weight)
```

Veach thesis Chapter 9 참고.

## Russian Roulette (종료)

무한 재귀 방지. 매 바운스 후 확률적 종료.

```hlsl
float p = min(throughput.max(), 0.95);
if (Rand(rng) > p) break;
throughput /= p;
```

Unbiased — 평균적으로 정확한 결과.

## Material Branching

hit 지점의 재질에 따라 다른 샘플링:

```hlsl
enum MaterialType { DIFFUSE, METAL, GLASS, EMISSIVE };

float3 SampleBSDF(HitInfo hit, Material mat, inout RNGState rng, out float3 newDir, out float pdf)
{
    switch (mat.type) {
        case DIFFUSE:
            newDir = CosineWeightedHemisphere(RandFloat2(rng));
            newDir = LocalToWorld(newDir, hit.normal);
            pdf = PDF_CosineHemisphere(saturate(dot(hit.normal, newDir)));
            return mat.albedo / PI;
            
        case METAL: {
            float3 H = ImportanceSampleGGX(RandFloat2(rng), hit.normal, mat.roughness);
            newDir = reflect(-hit.V, H);
            pdf = PDF_GGX(hit.normal, H, mat.roughness) / (4 * dot(hit.V, H));
            return BRDF_GGX_Metal(hit.normal, hit.V, newDir, mat);
        }
        
        case GLASS:
            // Fresnel 에 따라 반사/굴절 확률 결정
            // ... Snell's law
            break;
            
        case EMISSIVE:
            return mat.emission;   // 광원에 도달
    }
}
```

## GPU Compute Shader 구현

```hlsl
// Shaders/PathTracer/PathTracer.hlsl
[numthreads(8, 8, 1)]
void CS_PathTrace(uint3 id : SV_DispatchThreadID)
{
    if (any(id.xy >= g_screenSize)) return;
    
    RNGState rng; InitRNG(id.x + id.y * g_screenSize.x, g_frameIdx, rng);
    
    float3 accumColor = 0;
    
    for (int s = 0; s < g_samplesPerFrame; ++s) {
        float2 jitter = Hammersley(s + g_frameIdx * g_samplesPerFrame, 1024);
        Ray ray = GenerateCameraRay(id.xy, jitter);
        
        float3 radiance = 0;
        float3 throughput = 1;
        
        for (int bounce = 0; bounce < g_maxBounces; ++bounce) {
            HitInfo hit;
            if (!TraceBVH(ray, hit)) {
                radiance += throughput * SampleEnvironment(ray.dir);
                break;
            }
            
            Material mat = g_materials[hit.materialID];
            hit.V = -ray.dir;
            
            if (mat.type == EMISSIVE) {
                radiance += throughput * mat.emission;
                break;
            }
            
            // NEE
            radiance += throughput * NextEventEstimation(hit, mat, rng);
            
            // Indirect
            float3 newDir; float pdf;
            float3 f = SampleBSDF(hit, mat, rng, newDir, pdf);
            if (pdf < 1e-6) break;
            
            float NdotL = saturate(dot(hit.normal, newDir));
            throughput *= f * NdotL / pdf;
            
            // Russian Roulette
            float p = saturate(max(throughput.r, max(throughput.g, throughput.b)));
            if (Rand(rng) > p) break;
            throughput /= p;
            
            ray = MakeRay(hit.pos + hit.normal * EPSILON, newDir);
        }
        
        accumColor += radiance;
    }
    
    accumColor /= g_samplesPerFrame;
    
    // Progressive accumulation
    float3 prev = g_accumBuffer[id.xy].rgb;
    float3 newColor = (prev * g_accumCount + accumColor) / (g_accumCount + 1);
    g_accumBuffer[id.xy] = float4(newColor, 1);
}
```

Progressive rendering: 매 프레임 1 SPP 씩 누적 → 카메라 정지 시 품질 점진 향상.

## 씬 데이터 구조 (GPU 업로드)

```cpp
struct GPUSceneData
{
    std::vector<Triangle>        triangles;        // 위치 + 법선 + UV + materialID
    std::vector<MaterialGPU>     materials;
    std::vector<LightGPU>        lights;
    std::vector<BVHNode>         bvhNodes;
    u32_t                        envMapHandle;     // Cubemap SRV
};
```

StructuredBuffer 로 GPU 에 업로드.

## Denoising (선택, 선진 토픽)

적은 SPP 에서도 깨끗한 결과를 위해:
- **Bilateral Filter**: 간이
- **Non-Local Means**: 품질 좋음
- **OIDN** (Intel Open Image Denoise): AI 기반 오픈소스
- **SVGF** (Spatiotemporal Variance-Guided Filter): 실시간용

## CPU Reference (디버그용)

```cpp
// Engine/Public/Renderer/PathTracer/PathTracerCPU.h
class CPathTracerCPU
{
public:
    // 단일 픽셀에 대한 L_o 계산 (디버깅, 단위 테스트)
    Vec3 TraceRay(const Ray& ray, i32_t maxDepth);

    void RenderImage(const std::string& outputPath, i32_t width, i32_t height, i32_t spp);
};
```

GPU 쉐이더와 수치 일치 확인 → 버그 추적 핵심.

## 검증 — Cornell Box

표준 벤치마크. 특정 SPP 에서 이미 알려진 값과 비교.

```
Cornell Box 해석적 기댓값 (픽셀 (512, 512)):
  5000 SPP → expected (0.4, 0.7, 0.25) ± 0.01

Winters PT 결과 측정:
  → MSE < 0.001 이면 PASS
```

## 성능

- 카메라 정지 시 목표: 10 초에 1000 SPP (RTX 30시리즈 급)
- GPU Compute: 매 픽셀 병렬 → 1080p × 1 SPP = ~50ms
- BVH 메모리: triangle 수 × 32 바이트 + 내부 노드
- Cornell Box 120k 삼각형: BVH ~10MB, 프레임 ~80ms (1 SPP)

## 디버그 기능

- **"Buffer Visualize" 모드**: 바운스 수 / 첫 hit 법선 / 재질 ID / BVH 깊이 시각화
- **샘플 수 오버레이**: 픽셀 별 현재 누적 SPP
- **Ray Debug**: ImGui 에서 클릭한 픽셀의 레이 경로 3D 시각화
- **Variance 맵**: 분산이 큰 픽셀 강조 → denoising 우선순위

## RenderGraph 통합

```cpp
rg.AddPass("PathTrace_Accumulate")
  .ReadWrite("AccumBuffer")
  .Read("BVHNodes").Read("Triangles").Read("Materials").Read("Lights")
  .Execute([](CommandList& cmd) {
      cmd.Dispatch(PathTraceCS, (w+7)/8, (h+7)/8, 1);
  });

rg.AddPass("PathTrace_Resolve")
  .Read("AccumBuffer")
  .Write("Backbuffer")
  .Execute([](CommandList& cmd) { cmd.Draw(ResolvePS); });
```

## 구현 순서

1. `Ray` / `HitInfo` / `Triangle` 구조 정의
2. CPU BVH 빌더 (SAH 분할)
3. CPU reference tracer (1 bounce, 직사광만)
4. GPU BVH 업로드 + Ray-AABB + Ray-Triangle
5. GPU Compute path tracer (Recursive → iterative)
6. Material branching (Diffuse / Metal / Glass / Emissive)
7. NEE (Light sampling)
8. Russian Roulette
9. Progressive Accumulation
10. MIS (BRDF + NEE)
11. Denoising (간이 bilateral)
12. Cornell Box 검증
13. HDRI 환경맵 샘플링

## 참고 문헌

- **Kajiya 1986** — The Rendering Equation
- **Veach 1997** — Robust Monte Carlo Methods for Light Transport (Ph.D. Thesis)
- **PBRT 3rd Ed.** Chapter 14~16 (Light Transport)
- **Wald, Havran 2006** — On Building Fast kd-Trees
- **Aila & Laine 2009** — Understanding the Efficiency of Ray Traversal on GPUs
- **MacDonald 1990** — Heuristics for Ray Tracing Using Space Subdivision (SAH)
