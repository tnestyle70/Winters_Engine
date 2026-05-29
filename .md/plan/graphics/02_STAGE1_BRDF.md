# Stage 1 — BRDF 이론 + 구현

## 목표

**BRDF (Bidirectional Reflectance Distribution Function)** — 표면의 빛 반사 특성을 기술하는 함수.
Lambertian (1760년) 부터 Disney Principled (2012) 까지 단계적 학습 + 구현.

## 왜 BRDF 인가

- 모든 라이팅 계산의 기초
- PBR (Physically Based Rendering) 의 핵심
- Path Tracing (Stage 4) 에서 샘플링 확률 분포
- 실시간 (Stage 5 GI) 에서도 동일 BRDF 평가

## 렌더링 방정식 (Kajiya 1986)

```
L_o(x, ω_o) = L_e(x, ω_o) + ∫_Ω f_r(x, ω_i, ω_o) × L_i(x, ω_i) × (ω_i · n) dω_i
```

- `L_o`: 나가는 라디언스
- `L_e`: 자체 발광
- `f_r`: BRDF (이 Stage 의 주제)
- `L_i`: 들어오는 라디언스
- `(ω_i · n)`: 코사인 항

## BRDF 성질

유효한 BRDF 는 다음을 만족해야 함:

1. **비음수성**: `f_r ≥ 0`
2. **헬름홀츠 상호성**: `f_r(ω_i, ω_o) = f_r(ω_o, ω_i)`
3. **에너지 보존**: `∫_Ω f_r × (ω_i · n) dω_i ≤ 1`

## Stage 1.1 — Lambertian

완전 확산 (diffuse) 반사. 모든 방향으로 균일 반사.

```
f_r = albedo / π
```

`π` 나눗셈은 반구 적분 정규화. 누락하면 에너지 위반.

```hlsl
// BRDF/BRDFLambert.hlsli
float3 BRDF_Lambert(float3 albedo)
{
    return albedo / PI;
}
```

## Stage 1.2 — Phong / Blinn-Phong

반사성 + 확산 혼합. Lambert 위에 하이라이트.

### Phong

```
f_spec = k_s × max(0, (r · v))^n
       where r = reflect(-l, n)
```

### Blinn-Phong (개선, 성능 ↑)

```
f_spec = k_s × max(0, (h · n))^n
       where h = normalize(l + v)   ← Half vector
```

**문제점**: 에너지 보존 위반, 물리 기반 아님. 학습용으로만.

## Stage 1.3 — Cook-Torrance (1982)

Microfacet 이론. 표면은 미세한 거울 조각들의 집합.

```
f_r = (D × G × F) / (4 × (n·l) × (n·v))
```

- **D**: Normal Distribution Function — 미세면이 half vector 방향을 향할 확률
- **G**: Geometry function — 자기 그림자 / 마스킹
- **F**: Fresnel — 시각 각도별 반사율

## Stage 1.4 — GGX / Trowbridge-Reitz (2007) ★ 핵심

현대 PBR 의 사실상 표준 NDF.

### D (Normal Distribution)

```
D_GGX(h) = α² / (π × ((n·h)² × (α² - 1) + 1)²)
   where α = roughness²
```

```hlsl
float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}
```

### G (Geometry - Smith + Schlick GGX)

```
G(l, v, h) = G_1(l) × G_1(v)
G_1(x) = (n·x) / ((n·x) × (1 - k) + k)
   where k = α / 2  (for IBL: k = α² / 2)
```

```hlsl
float G_SchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}
```

### F (Fresnel - Schlick 근사)

```
F(h, v, F_0) = F_0 + (1 - F_0) × (1 - (v·h))^5
```

`F_0`: 수직 입사 반사율 (유전체 0.04, 금속 albedo).

```hlsl
float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
```

### 통합 GGX BRDF

```hlsl
// BRDF/BRDFGGX.hlsli
float3 BRDF_GGX(
    float3 N, float3 V, float3 L,
    float3 albedo, float metallic, float roughness)
{
    float3 H = normalize(V + L);
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float NdotH = saturate(dot(N, H));
    float HdotV = saturate(dot(H, V));

    // F0: metallic → albedo, non-metal → 0.04
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float  D = D_GGX(NdotH, roughness);
    float  G = G_Smith(NdotV, NdotL, roughness);
    float3 F = F_Schlick(HdotV, F0);

    // Specular
    float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);

    // Diffuse (metallic 은 diffuse 0)
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 diffuse = kD * albedo / PI;

    return diffuse + specular;
}
```

## Stage 1.5 — Disney Principled BSDF (2012)

Disney 스튜디오가 표준화한 산업계 표준. 11개 파라미터:

```cpp
struct DisneyParams {
    float3  baseColor;
    float   metallic;
    float   subsurface;
    float   specular;       // 0.5 default → F0 = 0.04
    float   specularTint;
    float   roughness;
    float   anisotropic;
    float   sheen;
    float   sheenTint;
    float   clearcoat;
    float   clearcoatGloss;
};
```

기본 GGX 위에 다음 추가:
- **Subsurface**: 피부/밀랍 반투명 효과 (Hanrahan-Krueger)
- **Sheen**: 패브릭 가장자리 반짝
- **Clearcoat**: 자동차 페인트 2차 광택 (별도 GGX 레이어)
- **Anisotropic**: 헤어/브러시드 메탈 방향성

PBRT / Filament 오픈소스 참고.

## Stage 1.6 — Energy Conservation 검증

### Furnace Test

완전 균일 환경 (Gray = 0.5) 에 물체 놓고 렌더 → 물체 색 = 환경색이어야.

```hlsl
// 셰이더 단위 테스트 (Compute)
[numthreads(1, 1, 1)]
void FurnaceTest()
{
    const int N = 1024;
    float3 sum = 0;
    for (int i = 0; i < N; ++i) {
        float2 rand = Halton(i);
        float3 L = CosineWeightedHemisphere(rand);
        // BRDF integrate over hemisphere
        sum += BRDF_GGX(N, V, L, 1.0, 0.0, roughness) * L.z / PDF;
    }
    sum /= N;
    g_output[0] = float4(sum, 1.0);   // 이상적이면 (1,1,1)
}
```

값이 1 을 넘으면 에너지 생성 (BUG), 훨씬 작으면 손실.

### Multiple Scattering 보정 (선택)

GGX 는 거친 재질에서 어두워지는 문제 (multiple scattering 누락).
Kulla-Conty 2017 "Revisiting Physically Based Shading" 에너지 보상 LUT.

## 구현 순서

1. `BRDFCommon.hlsli` — PI, saturate, F_Schlick 헬퍼
2. `BRDFLambert.hlsli` — 가장 간단
3. `BRDFBlinnPhong.hlsli` — 레거시 비교용
4. `BRDFGGX.hlsli` — D/G/F 분해 → 통합
5. C++ Material 파라미터 (albedo/metallic/roughness) ↔ GPU cbuffer
6. Furnace Test 셰이더 (Compute)
7. ImGui 에서 실시간 metallic/roughness 슬라이더
8. Disney BSDF (시간 허락 시)

## 디버그 시각화

ImGui 에 BRDF 파라미터 실시간 조작:

```
[Material Inspector — #42 Irelia_Body]
BaseColor      [R: 0.75] [G: 0.60] [B: 0.55]
Metallic       [────●──] 0.5
Roughness      [─●─────] 0.25
Specular       [────●──] 0.5
────────────────────────────────────
[Furnace Test Output]  (0.99, 0.99, 0.99)  ✓ Energy conserving
[BRDF Lobe 3D Plot]    [mini viewport]
```

## 성능 고려

- D/G/F 계산: ~20 ALU per pixel
- Blinn-Phong 대비 약 3~5배. 현대 GPU 에선 무시 가능
- Roughness 가 0 에 매우 가까우면 D 가 무한대 → clamp `max(roughness, 0.01)`
- Metallic 이 정확히 0 이면 `F0 = 0.04`, 1 이면 `F0 = albedo` — 중간값은 lerp

## 참고 문헌

- **Cook & Torrance 1982** — A Reflectance Model for Computer Graphics
- **Walter et al. 2007** — Microfacet Models for Refraction through Rough Surfaces (GGX)
- **Burley 2012** — Physically Based Shading at Disney
- **Karis 2013** — Real Shading in Unreal Engine 4
- **Hammon 2017** — PBR Diffuse Lighting for GGX+Smith Microsurfaces
- **Kulla & Conty 2017** — Revisiting Physically Based Shading at Imageworks
