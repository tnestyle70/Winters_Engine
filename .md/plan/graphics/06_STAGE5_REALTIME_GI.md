# Stage 5 — 실시간 Global Illumination

## 목표

Path Tracer (Stage 4) 의 **간접광을 실시간 근사**. 60FPS 유지하면서 픽셀당 효과 부여.

## 왜 각 기법이 필요한가

| 기법 | 해결 문제 | 근사 정도 |
|---|---|---|
| **SSAO/HBAO+** | 구석 음영 | 경험적, 저렴 |
| **SSR** | 거울 반사 (스크린 공간) | 정확, 범위 제한 |
| **VXGI** | 2차 바운스 확산 | 부드럽고 동적 |
| **DDGI** | 전역 확산 조명 | 정확도 ↑, 프로브 기반 |
| **LPV** | 방향성 GI | 상대적 오래됨, 비교 학습용 |

## SSAO (Screen Space Ambient Occlusion)

화면 공간 깊이 기반 AO. 각 픽셀 주변 반구 샘플.

```hlsl
[numthreads(8, 8, 1)]
void CS_SSAO(uint3 id : SV_DispatchThreadID)
{
    float3 viewPos = ReconstructViewPos(id.xy, g_depthTex.Load(int3(id.xy, 0)));
    float3 normal = g_normalTex.Load(int3(id.xy, 0)).xyz * 2 - 1;
    
    const int SAMPLES = 16;
    float ao = 0;
    for (int i = 0; i < SAMPLES; ++i) {
        // 반구 내 랜덤 오프셋 샘플
        float3 sampleDir = g_ssaoKernel[i];
        sampleDir = reflect(sampleDir, g_ssaoNoise[id.xy % 4]);   // rotation noise
        sampleDir = AlignToNormal(sampleDir, normal);
        
        float3 samplePos = viewPos + sampleDir * g_radius;
        float4 projected = mul(g_proj, float4(samplePos, 1));
        projected.xy /= projected.w;
        float2 sampleUV = projected.xy * 0.5 + 0.5;
        
        float sampleDepth = ReconstructViewPos(sampleUV, 
            g_depthTex.SampleLevel(s_point, sampleUV, 0)).z;
        
        float rangeCheck = smoothstep(0, 1, g_radius / abs(viewPos.z - sampleDepth));
        ao += (sampleDepth >= samplePos.z ? 1 : 0) * rangeCheck;
    }
    
    ao = 1 - (ao / SAMPLES);
    g_ssaoOut[id.xy] = pow(ao, g_power);
}
```

### HBAO+ (Horizon-Based AO)

정밀 업그레이드. 반구 샘플 대신 수평선 각도 측정. 디테일 풍부.

## SSR (Screen Space Reflections)

화면 공간 레이 마칭으로 거울 반사.

### Hi-Z 가속

Depth buffer 의 mip 체인 (min-depth 피라미드) → 점프 가능한 레이 순회.

```hlsl
// 1. Hi-Z 빌드 (매 프레임)
[numthreads(16, 16, 1)]
void CS_HiZBuild(uint3 id : SV_DispatchThreadID)
{
    float d0 = g_depthPrev.Load(int3(id.xy * 2, 0)).r;
    float d1 = g_depthPrev.Load(int3(id.xy * 2 + int2(1, 0), 0)).r;
    float d2 = g_depthPrev.Load(int3(id.xy * 2 + int2(0, 1), 0)).r;
    float d3 = g_depthPrev.Load(int3(id.xy * 2 + int2(1, 1), 0)).r;
    g_hiZOut[id.xy] = min(min(d0, d1), min(d2, d3));
}
```

### SSR Ray Marching

```hlsl
bool TraceSSR(float3 origin, float3 direction, out float2 hitUV)
{
    float3 rayPos = origin;
    for (int step = 0; step < g_maxSteps; ++step) {
        rayPos += direction * g_stepSize;
        float4 projected = mul(g_proj, float4(rayPos, 1));
        projected.xy /= projected.w;
        hitUV = projected.xy * 0.5 + 0.5;
        
        if (any(hitUV < 0 || hitUV > 1)) return false;
        
        float sceneDepth = g_hiZ.SampleLevel(s_point, hitUV, g_mip).r;
        if (rayPos.z > sceneDepth) return true;   // 히트
    }
    return false;
}
```

### Cone Tracing with Roughness

거친 재질은 여러 반사 샘플 cone 적분.

## VXGI (Voxel Cone Tracing)

Crassin 2011. 씬을 3D 텍스처 voxel 로 변환 → GI 계산.

### 1. Voxelization

메시 래스터화 시 각 삼각형을 voxel 로 투영:

```hlsl
// 기하 셰이더로 voxel 에 기록
[maxvertexcount(3)]
void GS_Voxelize(triangle VSOut input[3], inout TriangleStream<GSOut> stream)
{
    // 최대 면적 축 선택 (XY, YZ, ZX)
    float3 n = normalize(cross(input[1].worldPos - input[0].worldPos,
                                input[2].worldPos - input[0].worldPos));
    float3 an = abs(n);
    int axis = (an.x >= an.y && an.x >= an.z) ? 0 : (an.y >= an.z ? 1 : 2);
    
    // 해당 축에 정사영 투영 → voxel grid
    for (int i = 0; i < 3; ++i) {
        GSOut o = ProjectToAxis(input[i], axis);
        stream.Append(o);
    }
}

// PS 에서 voxel 에 라디언스 기록
void PS_Voxelize(PSInput input)
{
    uint3 voxelID = WorldToVoxel(input.worldPos);
    float3 radiance = ComputeDirectLighting(input);
    InterlockedAdd(g_voxelTex[voxelID].r, asuint(radiance.r));
    // ... g, b
}
```

### 2. Mip Chain (Anisotropic)

각 voxel 면별로 방향성 aggregate → 6면 mip.

### 3. Cone Tracing

GI 계산 시 각 방향 cone 을 voxel 에 레이 마칭:
```hlsl
float4 ConeTrace(float3 origin, float3 direction, float coneAngle)
{
    float4 accum = 0;
    float dist = g_startDist;
    while (dist < g_maxDist && accum.a < 0.95) {
        float diameter = 2 * dist * tan(coneAngle / 2);
        float mip = log2(diameter / g_voxelSize);
        
        float4 sample = g_voxelTex.SampleLevel(s_linear, WorldToUVW(origin + direction * dist), mip);
        accum += (1 - accum.a) * sample;
        dist += diameter;   // 자연 step
    }
    return accum;
}
```

픽셀별 diffuse: 반구 여러 cone / specular: 1 cone (좁은 각도).

## DDGI (Dynamic Diffuse Global Illumination)

NVIDIA 2019. **Probe 그리드** 에 환경 라디언스 저장 → 샘플링 시 3선형 보간.

### Probe Layout

3D 격자에 probe 배치 (예: 16×16×8 = 2048개). 각 probe 에는:
- **Irradiance Map**: 8×8 octahedral (약 60 방향 저장)
- **Visibility (depth + depth²)**: 16×16 octahedral (거리 체크용)

### Probe Update (매 프레임)

```
각 probe 에서:
  레이 N 개 쏨 (예: 256)
  hit 지점 radiance 계산 → irradiance map 에 누적
  hit 거리 → visibility map 에 누적
  Temporal blending (이전 프레임 80% + 새 20%)
```

DXR / Compute Shader 로 레이 트레이스.

### Probe Sampling (셰이딩 시)

```hlsl
float3 SampleDDGI(float3 worldPos, float3 normal)
{
    // 주변 8 probe 식별 (3선형 보간)
    int3 baseProbe = GetBaseProbe(worldPos);
    float3 alpha = GetProbeAlpha(worldPos);
    
    float3 sum = 0;
    float totalWeight = 0;
    
    for (int i = 0; i < 8; ++i) {
        int3 probeID = baseProbe + int3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        float3 probePos = ProbePosition(probeID);
        
        // Visibility 체크 (Chebyshev filter)
        float3 toProbe = normalize(probePos - worldPos);
        float  dist = distance(probePos, worldPos);
        float2 depthStats = SampleProbeVisibility(probeID, toProbe);
        float  visibility = ChebyshevTest(depthStats, dist);
        
        // Irradiance 샘플
        float3 irradiance = SampleProbeIrradiance(probeID, normal);
        
        float weight = TrilinearWeight(alpha, i) * visibility * max(dot(normal, toProbe), 0);
        sum += irradiance * weight;
        totalWeight += weight;
    }
    
    return sum / max(totalWeight, 1e-4);
}
```

### Chebyshev Filter (Visibility)

빛이 통과하지 못하는 벽 너머 probe 오염 방지:
```
visibility = max(0, (depthMean² - depth²·variance) / (depth² + variance))
```

## LPV (Light Propagation Volumes) - 비교 학습용

Crytek 2009. 3D 그리드 + 각 셀에 Spherical Harmonics (SH) 계수.
현재는 VXGI/DDGI 에 밀렸지만 개념 이해에 유용.

## Hybrid Approach (권장)

| 용도 | 기법 |
|---|---|
| 근접 AO | SSAO/HBAO+ |
| 거울 반사 | SSR (+ VXGI fallback for off-screen) |
| 간접 확산 (동적) | DDGI |
| 간접 확산 (정적) | Baked Lightmap (전통) |
| 투명 표면 | Path Tracing (고품질 레퍼런스) |

## RenderGraph 통합

```cpp
rg.AddPass("SSAO")
  .Input("GBuffer_Normal").Input("GBuffer_Depth")
  .Output("Occlusion")
  .Execute(...);

rg.AddPass("SSR_HiZ_Build")
  .Input("GBuffer_Depth")
  .Output("HiZChain")   // mip 체인
  .Execute(...);

rg.AddPass("SSR_Trace")
  .Input("GBuffer_Normal").Input("HiZChain").Input("SceneColor_Prev")
  .Output("Reflections")
  .Execute(...);

rg.AddPass("DDGI_ProbeUpdate")
  .ReadWrite("ProbeIrradiance").ReadWrite("ProbeVisibility")
  .Read("BVHNodes").Read("Triangles")
  .Execute(...);

rg.AddPass("DDGI_Sample")
  .Input("GBuffer_*").Input("ProbeIrradiance").Input("ProbeVisibility")
  .Output("DiffuseGI")
  .Execute(...);

rg.AddPass("Lighting_Combine")
  .Input("DirectLight").Input("Occlusion").Input("Reflections").Input("DiffuseGI")
  .Output("HDR")
  .Execute(...);
```

## 디버그 시각화

- **Occlusion Only** 모드 (AO 만 렌더)
- **Reflections Only** 모드
- **Voxel Overlay** — voxelized 씬 와이어프레임
- **Probe Grid** 렌더 — 각 probe 를 작은 구로 표시, irradiance 색으로 컬러
- **Probe Visibility** octahedral map 텍스처 확인

## 성능

- SSAO: 0.5~1 ms (1080p)
- SSR Hi-Z: 1~2 ms
- VXGI: 3~5 ms (voxelization + tracing)
- DDGI: 2~4 ms (probe update + sampling)

## 참고 문헌

- **Kajalin 2009** — Screen Space Ambient Occlusion
- **Bavoil & Sainz 2008** — HBAO+
- **Mittring 2011** — Screen Space Reflections in UE3
- **Crassin et al. 2011** — VXGI
- **Majercik et al. 2019** — DDGI (NVIDIA)
- **Kaplanyan & Dachsbacher 2010** — LPV
- **McGuire et al. 2017** — Phong Reflection Models in Screen Space
- **Heitz 2018** — Combining Analytic Direct Illumination and SSR

## 구현 순서

1. G-Buffer 구성 (RenderGraph 의존 — Phase 2 동반)
2. SSAO 간단 버전
3. HBAO+ 업그레이드
4. Hi-Z depth mip chain
5. SSR 기본 ray march
6. SSR Cone tracing (roughness 적응)
7. Voxelization (기하 셰이더)
8. VXGI Cone Tracing
9. DDGI probe 시스템
10. Probe update (Compute)
11. Probe visibility filter
12. Path Tracer 와 품질 비교 (Stage 4 연동)
