# Stage 7 — Temporal 기법 (TAA + Reprojection)

## 목표

**프레임 간 정보 축적** 으로 고품질 안티앨리어싱 + 노이즈 제거 + 업스케일링.

## 왜 Temporal 인가

- MSAA: 기하 엣지만, 셰이더 엘리어싱 (깜빡임) 해결 못함
- SSAA: 완벽하지만 4~16× 비용 → 실시간 불가
- **TAA**: 여러 프레임에 걸쳐 jitter 된 샘플 합침 → 유사 SSAA 품질, 비용 낮음
- DLSS / FSR 도 본질은 temporal upsampling

## TAA 핵심 원리

### 1. Camera Jitter

매 프레임 카메라 projection 에 서브픽셀 오프셋 적용 (Halton sequence):

```hlsl
// C++
float2 GetJitterOffset(u32_t frameIdx)
{
    float2 h = mc::Halton(frameIdx % 16);   // Halton(2, 3)
    return (h - 0.5f) * 2.f / screenSize;   // NDC 범위
}

// 셰이더에 Projection 전송 전
Matrix jitterMat = Matrix::Translation(jitter.x, jitter.y, 0);
Matrix jitteredProj = proj * jitterMat;
```

### 2. Reprojection (Motion Vectors)

이전 프레임 UV 를 재구성:
```
prevUV = CurrentScreenPos × inverse(ViewProj_curr) × ViewProj_prev / screen
motionVector = currentUV - prevUV
```

G-Buffer 에 **Motion Vector** 버퍼 (RG16F) 추가. 동적 오브젝트는 per-vertex 속도 계산:

```hlsl
// Opaque.hlsl
struct VSOut {
    float4 clipPos : SV_Position;
    float4 currClip : TEXCOORD0;
    float4 prevClip : TEXCOORD1;
    // ...
};

VSOut VS_Opaque(VSInput input)
{
    VSOut o;
    float4 currWorld = mul(g_worldMatrix, float4(input.pos, 1));
    float4 prevWorld = mul(g_prevWorldMatrix, float4(input.pos, 1));
    o.currClip = mul(g_viewProj,     currWorld);
    o.prevClip = mul(g_prevViewProj, prevWorld);
    o.clipPos  = o.currClip;
    return o;
}

float2 PS_MotionVector(VSOut input) : SV_Target1
{
    float2 curr = input.currClip.xy / input.currClip.w;
    float2 prev = input.prevClip.xy / input.prevClip.w;
    return (curr - prev) * 0.5;   // NDC → UV
}
```

### 3. History Accumulation

```
color_final = lerp(color_history, color_new, α)   // α ≈ 0.1
```

매 프레임 새 샘플 10% 혼합 → 여러 프레임 샘플 합산.

### 4. Neighborhood Clipping (★ 가장 중요)

역사 재사용 시 **고스팅 (잔상)** 문제. 해결: 현재 프레임 3×3 이웃의 색 범위로 역사 clamp.

```hlsl
// TAA.hlsl
float3 SampleNeighborhood(int2 pixel, out float3 nMin, out float3 nMax)
{
    nMin = 1e10;
    nMax = -1e10;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            float3 c = g_sceneColor.Load(int3(pixel + int2(dx, dy), 0)).rgb;
            nMin = min(nMin, c);
            nMax = max(nMax, c);
        }
    }
    return g_sceneColor.Load(int3(pixel, 0)).rgb;
}

float3 ClipAABB(float3 point, float3 boxMin, float3 boxMax)
{
    float3 center = (boxMin + boxMax) * 0.5;
    float3 half  = (boxMax - boxMin) * 0.5;
    float3 diff  = point - center;
    float3 dirToBox = abs(diff / max(half, 1e-4));
    float maxDir = max(dirToBox.x, max(dirToBox.y, dirToBox.z));
    return maxDir > 1 ? center + diff / maxDir : point;
}
```

### 5. Variance-Based Clipping (개선)

Min/Max 는 과한 제약. 이웃의 평균 ± N×표준편차로:
```hlsl
float3 mean = 0, mom2 = 0;
// ... 이웃 9 샘플 누적
float3 variance = mom2 / 9 - mean * mean;
float3 std = sqrt(max(variance, 0));
nMin = mean - 1.5 * std;
nMax = mean + 1.5 * std;
```

NVIDIA "High Quality Temporal Supersampling" (Salvi 2016) 방식.

### 6. TAA Compute Shader

```hlsl
[numthreads(8, 8, 1)]
void CS_TAA(uint3 id : SV_DispatchThreadID)
{
    int2 pixel = id.xy;
    if (any(pixel >= g_screenSize)) return;
    
    // 현재 프레임 + 이웃
    float3 nMin, nMax;
    float3 curr = SampleNeighborhoodClipping(pixel, nMin, nMax);
    
    // 이전 프레임 UV 재투영
    float2 motion = g_motionTex.Load(int3(pixel, 0)).xy;
    float2 uvPrev = ((pixel + 0.5) / g_screenSize) - motion;
    
    // 이전 프레임 색상 (boundary check)
    float3 history;
    if (all(uvPrev > 0) && all(uvPrev < 1)) {
        history = g_historyTex.SampleLevel(s_linear, uvPrev, 0).rgb;
        history = ClipAABB(history, nMin, nMax);   // ghost 방지
    } else {
        history = curr;   // 화면 벗어나면 현재값만
    }
    
    // Blend
    float alpha = 0.1;
    
    // Depth/Disocclusion 체크 (선택)
    float prevDepth = g_depthHistory.SampleLevel(s_point, uvPrev, 0).r;
    float currDepth = g_depthTex.Load(int3(pixel, 0)).r;
    if (abs(prevDepth - currDepth) > 0.05) alpha = 1;   // 역사 버림
    
    float3 final = lerp(history, curr, alpha);
    
    g_historyOut[pixel] = float4(final, 1);
    g_sceneOut[pixel] = float4(final, 1);
}
```

### 7. Sharpening (선택)

TAA 로 약간 blur 됨. RCAS (AMD) 또는 FidelityFX CAS 로 선명도 복원.

## Temporal 재사용 (다른 기법)

### SSR Temporal Accumulation

노이지 SSR 결과를 여러 프레임 blend. Motion vector 로 재투영.

### Volumetric Lighting Temporal

3D froxel 볼륨 역시 이전 프레임 재사용 → 체커보드 샘플링 → 누적.

### Temporal Super Sampling (DLSS/FSR 축소판)

저해상도 렌더 (720p) + 여러 프레임 누적 → 고해상도 (1440p) 복원. 자체 구현은 도전 과제:

```
render 1280×720 with jitter sequence
each pixel maps to 2×2 sub-pixels
over 4 frames collect 4 distinct sub-pixels per pixel
reconstruct 2560×1440 output
```

## Frustum Jitter Cancel

Projection 에 jitter 먹인 만큼 TAA resolve 시 빼야 픽셀 정렬:
```
sampleOffset = jitter / screen
finalUV = pixelUV - sampleOffset   // 역보정
```

## History Buffer Management

```cpp
// TAASystem.h
class CTAASystem
{
public:
    void Initialize(CDX11Device* dev, u32_t w, u32_t h);
    
    // Ping-pong: 이번 프레임의 output 이 다음 프레임의 history
    void Execute(CommandList& cmd);

private:
    ComPtr<ID3D11Texture2D>  m_historyBuffers[2];
    ComPtr<ID3D11Texture2D>  m_depthHistoryBuffers[2];
    u32_t                    m_currentIdx = 0;
    
    void SwapBuffers() { m_currentIdx ^= 1; }
};
```

## Motion Vector 품질

- **Static geometry**: ProjPrev vs ProjCurr 자동
- **Dynamic**: per-object prev world matrix 필요 (Skeleton + Transform 이전 프레임 캐시)
- **Transparent**: Motion Vector 없음 → TAA 비활성 또는 별도 처리

## 일반적 문제

### Ghosting (잔상)

- Clipping 제대로 안 됨
- Occlusion 감지 실패
- History boundary 벗어남

### Flickering

- 매우 얇은 디테일 (선, 머리카락) → variance clipping 으로 샘플 버림
- Specular highlight → clamp 가 가려버림

### Blur

- α 너무 작음
- 해결: sharpening, 적응형 α (움직임 크면 높임)

## 디버그 모드

- **History Debug**: 현재 픽셀의 최근 N 프레임 accumulate 만 표시
- **Motion Vector Visualize**: RG 를 red/green 으로 표시
- **Disocclusion Mask**: clipping 된 픽셀 강조
- **Jitter 패턴 시각화**: 카메라 offset history

## 성능

- TAA: ~1 ms (1080p Compute)
- History 버퍼: RGB16F × 2 (ping-pong) = ~16 MB
- Motion Vector: RG16F = ~8 MB

## 구현 순서

1. Camera Jitter (Halton Matrix 수정)
2. Motion Vector G-Buffer 추가
3. Dynamic objects prev world matrix 추적
4. History buffer 관리 (ping-pong)
5. 기본 TAA Resolve (단순 alpha blend)
6. Neighborhood AABB Clipping
7. Variance 기반 clipping
8. Disocclusion 감지 (depth 차이)
9. Sharpening (RCAS 또는 FidelityFX CAS)
10. (선택) Temporal Upsampling
11. ImGui 토글 (on/off, α 슬라이더, 모드 전환)

## 참고 문헌

- **Karis 2014** — High Quality Temporal Supersampling (UE4)
- **Salvi 2016** — Anti-Aliasing Methods in CryENGINE
- **NVIDIA 2018** — Deep Learning Super Sampling (DLSS 개념)
- **AMD 2021** — FidelityFX Super Resolution
- **Marrs et al. 2018** — Adaptive Temporal Antialiasing
- **Yang et al. 2020** — A Survey of Temporal Antialiasing Techniques
