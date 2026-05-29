# Stage 3 — 몬테카를로 적분

## 목표

**렌더링 방정식을 확률적으로 풀기**. Path Tracing (Stage 4) 과 실시간 GI (Stage 5) 의 수학 기반.

## 왜 몬테카를로 인가

렌더링 방정식의 적분은 닫힌 해가 없음 (복잡한 씬):
```
L_o = ∫_Ω f × L_i × cos dω
```

숫자 적분은 수천 차원 (각 간접 바운스마다 반구) → 불가능.
**몬테카를로**: 무작위 샘플링으로 추정.

```
E[f(X)] ≈ (1/N) × Σ f(X_i)
   where X_i ~ 확률 분포 p
```

추정치의 분산 `σ²/N`. 샘플 많을수록 정확.

## 기본 MC 추정기

```
∫ f(x) dx ≈ (1/N) × Σ f(X_i) / p(X_i)
```

`p` 가 **확률밀도함수 (PDF)**. 0 이 아닌 곳에서 샘플링 가능해야.

## 분산 감소 기법 (핵심)

### 1. Importance Sampling

`p(x)` 를 `f(x)` 와 비례하게 만들면 분산 ↓.

**이상적**: `p(x) = f(x) / ∫ f` — 이건 이미 답을 아는 셈. 현실적으로 `p` 를 비슷하게만.

BRDF Importance Sampling:
- Diffuse: `p(ω) = cos(θ)/π` (cosine-weighted hemisphere)
- GGX Specular: `p(ω_h) ∝ D(ω_h)` (GGX NDF 샘플링)

### Cosine-Weighted Hemisphere

```hlsl
float3 CosineWeightedHemisphere(float2 xi)
{
    float r     = sqrt(xi.x);
    float phi   = 2 * PI * xi.y;
    float x     = r * cos(phi);
    float y     = r * sin(phi);
    float z     = sqrt(max(0, 1 - xi.x));
    return float3(x, y, z);
}

float PDF_CosineHemisphere(float NdotL)
{
    return NdotL / PI;
}
```

### GGX Importance Sampling

```hlsl
float3 ImportanceSampleGGX(float2 xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2 * PI * xi.x;
    float cosTheta = sqrt((1 - xi.y) / (1 + (a*a - 1) * xi.y));
    float sinTheta = sqrt(1 - cosTheta * cosTheta);
    
    float3 H_local = float3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta
    );
    
    // local → world (tangent space)
    float3 up = abs(N.z) < 0.999 ? float3(0,0,1) : float3(1,0,0);
    float3 T = normalize(cross(up, N));
    float3 B = cross(N, T);
    return normalize(T * H_local.x + B * H_local.y + N * H_local.z);
}
```

### 2. Multiple Importance Sampling (MIS) — Veach 1997

두 샘플링 전략 조합 시 더 나은 쪽에 가중치.

```
E = w1(X1)·f(X1)/p1(X1) + w2(X2)·f(X2)/p2(X2)
```

**Balance heuristic**:
```
w_i = p_i(X) / Σ p_j(X)
```

**Power heuristic** (β=2, 더 공격적):
```
w_i = p_i² / Σ p_j²
```

Path Tracing (Stage 4) 에서 BRDF sampling + Light sampling 조합 시 필수.

### 3. Stratified Sampling

[0,1] 구간을 N 칸으로 나누고 각 칸 안에서 샘플.

```hlsl
float2 StratifiedSample(uint i, uint N)
{
    uint sqrtN = (uint)sqrt((float)N);
    uint x = i % sqrtN;
    uint y = i / sqrtN;
    float2 jitter = RandomFloat2();   // pseudo-random
    return float2((x + jitter.x) / sqrtN, (y + jitter.y) / sqrtN);
}
```

단순 pseudo-random 대비 빠른 수렴.

## Low-Discrepancy Sequences (LDS) ★ 핵심

"고르게 분포된" 결정론적 시퀀스. Pseudo-random 보다 수렴 빠름.

### Van der Corput Sequence

```hlsl
// Base 2 radical inverse
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;   // / 0x100000000
}
```

### Hammersley Sequence (2D)

```hlsl
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}
```

간단하고 효과적. IBL 베이커에서 이미 사용 (Stage 2).

### Halton Sequence (Base 2, 3)

```hlsl
float HaltonBase(uint i, uint base)
{
    float f = 1.0;
    float result = 0.0;
    while (i > 0) {
        f /= float(base);
        result += f * float(i % base);
        i /= base;
    }
    return result;
}

float2 Halton(uint i) {
    return float2(HaltonBase(i, 2), HaltonBase(i, 3));
}
```

TAA jitter (Stage 7) 에서 씀.

### Sobol Sequence

더 높은 차원에서도 좋은 품질. 구현 복잡 (matrix 기반). `pbrt-v3` 소스 참고.

### Owen Scrambling

Sobol 시퀀스에 랜덤 scramble 적용 → 분산 더 낮춤. 2020 년대 최신 Path Tracer 표준.

## 의사 난수 생성기 (PRNG)

### Xorshift

```hlsl
uint XorShift32(inout uint state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
```

빠르지만 통계적 품질 보통.

### PCG (Permuted Congruential Generator) ★ 권장

```hlsl
uint PCG_Next(inout uint state)
{
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}
```

매우 빠르면서 품질 우수. 최신 PBRT 채택.

### Philox

병렬 환경에서 counter-based. GPU compute 에 최적.

## 셰이더 통합

```hlsl
// MonteCarlo/Random.hlsli
struct RNGState {
    uint seed;
};

void InitRNG(uint pixelIdx, uint frameIdx, inout RNGState rng)
{
    rng.seed = pixelIdx * 719393u + frameIdx * 83492791u;
}

float RandFloat(inout RNGState rng) { return PCG_Next(rng.seed) * 2.3283064365386963e-10; }
float2 RandFloat2(inout RNGState rng) { return float2(RandFloat(rng), RandFloat(rng)); }
```

## PDF 와 역전환

샘플링 전략 `p(ω)` 가 있으면 역변환으로 샘플 생성 가능:
```
CDF(ω) = ∫_{0}^{ω} p(x) dx
ω = CDF⁻¹(u)   where u ~ Uniform[0,1]
```

예: 구 균일 샘플링:
```hlsl
float3 UniformSphere(float2 xi)
{
    float z = 1 - 2 * xi.x;
    float r = sqrt(max(0, 1 - z*z));
    float phi = 2 * PI * xi.y;
    return float3(r*cos(phi), r*sin(phi), z);
}
float PDF_UniformSphere() { return 1 / (4 * PI); }
```

## 응용 예시 — Direct Lighting

면광 (Area Light) 의 radiance 를 몬테카를로로 추정:

```hlsl
float3 EstimateDirectLight(float3 P, float3 N, Material mat, RectangleLight light, int samples)
{
    float3 sum = 0;
    for (int i = 0; i < samples; ++i) {
        float2 xi = Hammersley(i, samples);
        float3 lightPos = SampleRectLight(light, xi);
        float3 L = normalize(lightPos - P);
        float pdf = PDF_SampleRectLight(light, P, lightPos);
        
        // Shadow ray
        if (IsOccluded(P, lightPos)) continue;
        
        float3 f = BRDF_GGX(N, V, L, mat);
        float NdotL = saturate(dot(N, L));
        sum += f * light.emission * NdotL / pdf;
    }
    return sum / samples;
}
```

## 수렴 검증

샘플 수를 늘리면 정확도가 **1/√N** 비율로 향상:
- 4배 샘플 → 분산 1/4 → 표준편차 1/2

Cornell Box 렌더링으로 검증:
- 100 SPP: 노이지
- 1000 SPP: 수용 가능
- 10000 SPP: 거의 수렴
- 100000 SPP: Ground truth

## CPU Reference 구현

```cpp
// Engine/Public/Renderer/MonteCarlo/Samplers.h
namespace mc
{
    float RadicalInverse_VdC(u32_t bits);
    Vec2  Hammersley(u32_t i, u32_t n);
    Vec2  Halton(u32_t i);
    Vec2  Sobol2D(u32_t i);

    Vec3 CosineWeightedHemisphere(const Vec2& xi);
    Vec3 UniformSphere(const Vec2& xi);
    Vec3 ImportanceSampleGGX(const Vec2& xi, const Vec3& N, f32_t roughness);

    // PDFs
    f32_t PDF_CosineHemisphere(f32_t NdotL);
    f32_t PDF_GGX(const Vec3& N, const Vec3& H, f32_t roughness);

    // MIS heuristics
    f32_t BalanceHeuristic(f32_t pdfA, f32_t pdfB);
    f32_t PowerHeuristic(f32_t pdfA, f32_t pdfB, i32_t beta = 2);
}
```

단위 테스트로 모든 PDF 정규화 검증:
```cpp
TEST(MonteCarlo, CosineHemisphere_PDF_Sums_To_One)
{
    f32_t sum = 0;
    const int N = 100000;
    for (int i = 0; i < N; ++i) {
        Vec2 xi = mc::Hammersley(i, N);
        Vec3 dir = mc::CosineWeightedHemisphere(xi);
        sum += 1.f / mc::PDF_CosineHemisphere(dir.z);
    }
    sum /= N;
    EXPECT_NEAR(sum, 2 * M_PI, 0.01);   // 반구 면적
}
```

## 디버그 시각화

- 샘플 포인트 분포 (Hammersley vs Random vs Sobol 비교 그리드)
- 수렴 그래프 (샘플 수 vs MSE)
- PDF 히트맵 (2D 분포 시각화)

## 구현 순서

1. `Random.hlsli` — PCG + uniform float
2. `Sampler_Halton.hlsli` + `Hammersley` — 가장 먼저 필요
3. CPU 단위 테스트 (PDF 정규화 확인)
4. Cosine-Weighted Hemisphere + GGX Sample
5. Sobol / Owen (필요 시)
6. MIS 헬퍼 (balance / power heuristic)
7. Stratified Sampler
8. Direct Lighting 샘플러 (Stage 4 준비)
9. ImGui 샘플 분포 시각화

## 참고 문헌

- **Pharr, Jakob, Humphreys — PBRT 3rd Ed.** Chapter 13 "Monte Carlo Integration"
- **Veach 1997** — Robust Monte Carlo Methods for Light Transport (MIS)
- **Kollig & Keller 2002** — Efficient Multidimensional Sampling
- **Owen 1995** — Randomly Permuted (t,m,s)-Nets
- **Burley 2020** — Practical Hash-Based Owen Scrambling
