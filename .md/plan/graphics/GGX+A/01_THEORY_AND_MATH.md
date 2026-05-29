# 01. 이론 + 수학 — GGX·Smith·Schlick·Cook-Torrance

> 본 문서는 **순수 이론** 만 다룸. 코드 구현은 `02_HLSL_BRDF_LIBRARY.md` 부터.
> 출처: Real-Time Rendering 4th (Akenine-Möller et al.), PBRT 3rd (Pharr/Jakob/Humphreys),
> Walter et al. 2007 "Microfacet Models for Refraction" (GGX 원전), Heitz 2014 "Understanding the Masking-Shadowing Function".

---

## 1. 렌더링 방정식 (Kajiya 1986)

표면 점 `x` 에서 방향 `ω_o` 로 나가는 라디언스:

```
L_o(x, ω_o) = L_e(x, ω_o) + ∫_Ω f_r(x, ω_i, ω_o) · L_i(x, ω_i) · (ω_i · n) dω_i
```

| 기호 | 의미 |
|---|---|
| `L_o` | outgoing radiance (PS 가 출력해야 하는 값) |
| `L_e` | self-emission (emissive 텍스처) |
| `f_r` | **BRDF — 본 문서의 주제** |
| `L_i` | incoming radiance (광원 또는 환경광) |
| `(ω_i · n)` | Lambert cosine 항 |
| `Ω` | 표면 위 반구 |

**실시간 렌더링은 적분을 못 푸므로** → 광원 `N` 개에 대한 **이산 합** 으로 근사:

```
L_o = L_e + Σ_{j=1..N} f_r(ω_j, ω_o) · L_j · max(0, ω_j · n)
```

→ Forward+ 는 이 합의 `N` 을 **타일별로 가변** 으로 만든다 (관련 없는 광원 스킵).

---

## 2. BRDF 가 만족해야 하는 3 성질

| 성질 | 식 | 위반 시 |
|---|---|---|
| **비음수성** | `f_r ≥ 0` | 음수 색상 (검정 이하) |
| **Helmholtz 상호성** | `f_r(ω_i, ω_o) = f_r(ω_o, ω_i)` | Path Tracing 불가 |
| **에너지 보존** | `∫ f_r · (ω_i·n) dω_i ≤ 1` | 자체 발광 (1 이상 반사 = 영구기관) |

**Phong/Blinn-Phong 은 에너지 보존 위반** → 학습용 외 미사용. Cook-Torrance 는 이 3개 모두 만족.

---

## 3. Microfacet 이론 — Cook-Torrance 1982

### 3.1 핵심 가정

표면은 **무수히 많은 미세 거울(microfacet)** 의 집합. 각 microfacet 은 perfect mirror.
거시적 normal `n` 과 별개로, 각 microfacet 은 자체 normal `m` 을 가짐.

빛이 `ω_i` 로 들어와 `ω_o` 로 나가려면 **half-vector** 방향 microfacet 만 기여:

```
h = normalize(ω_i + ω_o)
```

### 3.2 Cook-Torrance Specular BRDF

```
f_specular = (D(h) · G(ω_i, ω_o, h) · F(ω_o, h)) / (4 · (n · ω_i) · (n · ω_o))
```

| 항 | 이름 | 답 |
|---|---|---|
| `D(h)` | **Normal Distribution Function (NDF)** | "얼마나 많은 microfacet 이 `h` 방향을 향하나?" |
| `G(ω_i, ω_o, h)` | **Geometry / Masking-Shadowing** | "그중 가려지지 않고 빛이 도달/탈출 가능한 비율?" |
| `F(ω_o, h)` | **Fresnel** | "그중 반사되는 빛의 비율 (vs 흡수)?" |
| `4·(n·ω_i)·(n·ω_o)` | **정규화 분모** | microfacet → macrofacet 변환 야코비안 |

→ **D · G · F** = 3개 항 곱. 각각 GGX, Smith, Schlick 으로 구현.

### 3.3 Diffuse 항 추가 (Lambert)

Cook-Torrance 는 specular 만 다룸. 비금속(dielectric) 은 diffuse 도 있음:

```
f_diffuse = albedo / π                         (Lambertian)
f_total   = (1 - F) · f_diffuse + f_specular   (energy conservation)
```

`(1 - F)` 가 핵심: Fresnel 로 반사된 빛은 표면 안으로 못 들어가니 diffuse 에서 빼야 함.

→ **금속(metal)** 은 `f_diffuse = 0` (전부 specular). **비금속(dielectric)** 만 diffuse 기여.

---

## 4. NDF — GGX (Trowbridge-Reitz 1975 / Walter 2007)

### 4.1 공식

```
              α²
D_GGX(h) = ─────────────────────────────
           π · ((n·h)² · (α² - 1) + 1)²

α = roughness²    (Disney perceptual roughness)
```

### 4.2 왜 GGX 인가 (vs Beckmann/Phong NDF)

| NDF | 하이라이트 모양 | 꼬리 (long tail) |
|---|---|---|
| Phong | 동그란 점 | 짧음 (비현실) |
| Beckmann | 가우시안 | 중간 |
| **GGX** | 별빛처럼 퍼지는 코어 + 긴 꼬리 | **현실 매칭** |

**GGX 의 long tail** = 거친 금속의 "끓어오르는" 광택 표현. 영화 VFX·Disney·Unreal·Unity URP 모두 GGX.

### 4.3 α (alpha) 와 roughness 의 관계

- Disney 제안: `α = roughness²` (perceptual linearity)
- `roughness = 0` → mirror (α=0, D 가 delta 함수)
- `roughness = 1` → 균등 분포 (α=1, D 가 거의 평탄)

### 4.4 검증 — D 의 적분

```
∫_Ω D(h) · (n·h) dh = 1
```

**필수**: 위반 시 에너지 보존 깨짐. GGX 는 위 식을 정확히 만족 (analytical).

---

## 5. Geometry — Smith 1967 / Heitz 2014

### 5.1 두 가지 가림

- **Masking** `G_1(ω_o)`: 카메라에서 본 microfacet 이 다른 microfacet 에 가려질 확률.
- **Shadowing** `G_1(ω_i)`: 광원에서 본 microfacet 이 가려질 확률.

### 5.2 Smith Joint G

```
G(ω_i, ω_o) = G_1(ω_i) · G_1(ω_o)         (Smith Separable, 단순)
G(ω_i, ω_o) = G_1(ω_i) · G_1(ω_o) · ...   (Smith Height-Correlated, 정확) — Heitz 2014
```

Winters 는 **Height-Correlated** 채택 (UE4/Filament 표준):

```
              0.5
G_2(l, v) = ─────────────────────────────────────────────────────
           (n·v) · sqrt((n·l)² · (1-α²) + α²) +
           (n·l) · sqrt((n·v)² · (1-α²) + α²)
```

→ 이 G_2 는 이미 Cook-Torrance 분모 `4·(n·l)·(n·v)` 와 묶여서 **Visibility V** 로 쓰임:

```
V = G_2 / (4 · (n·l) · (n·v))
```

→ 셰이더는 `V_SmithGGXCorrelated()` 한 함수로 구현.

### 5.3 G_1 (Smith mono, 단순화 버전)

UE4 가 사용하는 단순화:

```
G_1(v) = (n·v) / ((n·v)·(1-k) + k)

k = α / 2                                  (direct light)
k = α² / 2                                 (IBL)
```

→ Phase E Stage 1 은 **G_2 height-correlated 만** 구현. G_1 은 IBL prefilter 단계 (Stage 7) 에서.

---

## 6. Fresnel — Schlick 1994 근사

### 6.1 Fresnel 정확식 (sphere physics)

```
F(θ) = ½ · (((n2·cos θ_i - n1·cos θ_t) / (n2·cos θ_i + n1·cos θ_t))² +
            ((n1·cos θ_t - n2·cos θ_i) / (n1·cos θ_t + n2·cos θ_i))²)
```

→ 너무 비싸서 실시간 X.

### 6.2 Schlick 근사

```
F(h, v) = F0 + (1 - F0) · (1 - (h·v))⁵
```

| 변수 | 설명 |
|---|---|
| `F0` | normal incidence 반사율 |
| `(h·v)` | half vector 와 view 의 dot |
| `(1 - (h·v))⁵` | 5승 — Fresnel 가장자리 강조 |

### 6.3 F0 의 결정 — metallic workflow

```
F0_dielectric = 0.04     (모든 비금속 동일 — 4% 반사, 물·플라스틱·피부)
F0_metal      = albedo   (금속은 색이 곧 반사율, 황금=노랑반사, 구리=주황반사)

F0 = lerp(0.04, albedo, metallic)
```

→ **이게 metallic workflow 의 핵심**. metallic = 0/1 두 극값 외 사용 안 함 (현실 재현).

### 6.4 Roughness-dependent Fresnel (옵션, IBL 용)

거친 표면의 grazing angle 에서 Schlick 이 과하게 밝아지는 문제. Lazarov 보정:

```
F_roughness(h, v) = F0 + (max(1-roughness, F0) - F0) · (1 - (h·v))⁵
```

→ Stage 7 IBL 에서만 사용. Direct light 는 일반 Schlick.

---

## 7. Cook-Torrance 결합

### 7.1 최종 셰이딩 함수 (한 광원 기준)

```hlsl
float3 EvaluateBRDF(
    float3 albedo, float metallic, float roughness,
    float3 N, float3 V, float3 L)
{
    float3 H = normalize(V + L);
    float NdotL = saturate(dot(N, L));
    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float HdotV = saturate(dot(H, V));

    float  alpha   = roughness * roughness;
    float3 F0      = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

    float  D = D_GGX(NdotH, alpha);
    float  V_term = V_SmithGGXCorrelated(NdotL, NdotV, alpha);    // = G/(4·NdotL·NdotV)
    float3 F = F_Schlick(F0, HdotV);

    float3 specular = D * V_term * F;                              // 4·NdotL·NdotV 이미 V 에 포함

    float3 kd = (1.0 - F) * (1.0 - metallic);                      // metal 은 diffuse 0
    float3 diffuse = kd * albedo / PI;

    return (diffuse + specular) * NdotL;                           // Lambert cosine 외부 곱
}
```

### 7.2 분모 4·NdotL·NdotV 처리 주의

- 일반 식: `f_specular = D·G·F / (4·NdotL·NdotV)`
- V 함수: `V = G / (4·NdotL·NdotV)` → 한꺼번에 처리
- 따라서 `f_specular = D · V · F` (4 분모 다시 나누지 말 것)

→ 흔한 버그: `D·G·F / (4*NdotL*NdotV)` + V 함수 호출로 **두 번 나눔** = 결과 너무 어두움.

---

## 8. 수학적 검증 — White Furnace Test

### 8.1 정의

- 환경: 균등 백색 조명 (모든 방향 라디언스 = 1).
- 표면: roughness=1, metallic=0, albedo=0.18 (gray).
- 기대 출력: ≈ 0.18 (입력 그대로 반사).

### 8.2 위반 패턴

| 패턴 | 원인 |
|---|---|
| 출력 너무 밝음 (1.0+) | Energy gain — π 분모 누락, V 함수 분모 중복 |
| 출력 너무 어두움 | Energy loss — multiple scattering 무시 (Stage 7 에서 보정) |
| 일정한 어두움 (0.18 → 0.10) | Multiple scattering loss — Filament 의 `EnergyCompensation` 용어 |

### 8.3 Stage 1 검증 코드

`Shaders/Tests/FurnaceTest.hlsl` (Compute Shader) 을 만들어 직접 적분:

```hlsl
[numthreads(8, 8, 1)]
void CSMain(uint3 tid : SV_DispatchThreadID)
{
    float3 N = float3(0, 0, 1);
    float3 V = float3(0, 0, 1);

    float sum = 0;
    const int SAMPLES = 4096;
    for (int i = 0; i < SAMPLES; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLES);
        float3 H = ImportanceSampleGGX(Xi, N, alpha);
        float3 L = reflect(-V, H);
        sum += EvaluateBRDF(0.18, 0, 1, N, V, L) * saturate(dot(N, L));
    }
    sum *= 2 * PI / SAMPLES;
    g_Output[tid.xy] = sum;       // 0.18 ± 2% 면 OK
}
```

→ Stage 1 통과 = 이 결과가 **0.176 ~ 0.184 사이** (95% 정확도).

---

## 9. Forward+ 이론 — Olsson & Assarsson 2012

### 9.1 Forward 의 한계

```
PS 한 픽셀: for each light in 1..N { L_o += BRDF(light) }
```

- `N = 100` 광원, 1080p (200만 픽셀) → 2억 BRDF 평가 = GPU 폭사.
- 광원 대부분이 픽셀에 영향 없음 (반대편 맵의 광원 등).

### 9.2 Forward+ 핵심 아이디어

화면을 **16×16 픽셀 타일** 로 분할 → 각 타일이 자신에게 영향 주는 광원만 리스트화 → PS 는 그 리스트만 순회.

```
1080p / 16 = 120 × 67 = 8040 tiles
각 타일: 영향 광원 평균 8개 (전체 100개 중)
→ PS 평가량 = 200만 × 8 = 1600만  (2억 → 8% 로 감축)
```

### 9.3 알고리즘

1. **Depth Pre-pass** — 화면 전체 depth buffer 채움 (오버드로우 방지).
2. **Light Cull Compute Shader** — 각 16×16 타일마다:
   - 타일의 min/max depth 로 frustum 생성
   - 모든 광원 순회 → frustum vs sphere 교차 → 영향 광원 인덱스 저장
3. **Shading Pass** — PS 에서 자기 타일의 광원 리스트 읽어 BRDF 평가.

### 9.4 Cluster 변형 (Forward Clustered, Avalanche 2016)

타일을 **2D + Z-slice** 로 나눠 3D 클러스터. 깊이 차이 큰 픽셀끼리 광원 공유 안 함 → 더 정확.

→ Winters Phase E Stage 5 는 **2D Tiled Forward+** 우선. Cluster 는 Stage 5b 옵션.

### 9.5 데이터 구조

```hlsl
StructuredBuffer<PointLight> g_Lights        : register(t10);  // 모든 광원 (예: 1024개)
RWStructuredBuffer<uint>     g_LightIndex    : register(u0);   // 타일별 인덱스 [tiles × maxLightsPerTile]
RWStructuredBuffer<uint2>    g_LightGrid     : register(u1);   // [offset, count] per tile
```

---

## 10. 단위 검증 표

| 단계 | 입력 | 기대 출력 | 허용 오차 |
|---|---|---|---|
| Lambert diffuse | albedo=0.5, NdotL=1 | 0.5/π ≈ 0.159 | ±0.001 |
| GGX D | NdotH=1, α=0.1 | 1/(π·α²) = 31.83 | ±0.01 |
| Smith V | NdotL=NdotV=1, α=0.1 | 0.5/(1·1) = 0.5 | ±0.001 |
| Schlick F | F0=0.04, HdotV=1 | 0.04 | ±0.0001 |
| Schlick F | F0=0.04, HdotV=0 | 1.0 | ±0.0001 |
| White furnace | albedo=0.18, metal=0, rough=1 | 0.18 | ±2% |

---

## 11. 참고 문헌

| # | 출처 | 활용 부분 |
|---|---|---|
| 1 | Walter 2007 "Microfacet Models for Refraction" | GGX 원전 |
| 2 | Heitz 2014 "Understanding the Masking-Shadowing Function" | Smith Height-Correlated G |
| 3 | Schlick 1994 "An Inexpensive BRDF Model" | Fresnel 근사 |
| 4 | Cook-Torrance 1982 "A Reflectance Model for Computer Graphics" | 전체 microfacet 프레임워크 |
| 5 | Burley 2012 "Physically-Based Shading at Disney" | metallic workflow / α=roughness² |
| 6 | Karis 2013 "Real Shading in Unreal Engine 4" | UE4 합성 함수 (G_1 형태, Split-Sum IBL) |
| 7 | Filament Rendering | `V_SmithGGXCorrelated` Visibility 결합 |
| 8 | Olsson & Assarsson 2012 "Tiled Forward Shading" | Forward+ 원전 |
| 9 | Avalanche 2016 "Practical Clustered Shading" | Cluster 변형 |

---

## 다음 문서

→ `02_HLSL_BRDF_LIBRARY.md` — 위 수식들을 HLSL 함수로 옮긴 전문 코드.

## 검증 체크리스트 (이 단계 완료 조건)

- [ ] Cook-Torrance 의 D, G, F 가 각각 무엇을 의미하는지 1줄로 설명 가능.
- [ ] White furnace test 가 왜 0.18 결과를 기대하는지 식 유도 가능.
- [ ] metallic workflow 의 F0 결정식 (`F0 = lerp(0.04, albedo, metallic)`) 외움.
- [ ] V_SmithGGXCorrelated 가 G 가 아니라 `G/(4·NdotL·NdotV)` 임을 인지 (분모 중복 방지).
- [ ] Forward+ 가 "타일별 광원 리스트" 라는 핵심 아이디어 1줄로 설명 가능.
