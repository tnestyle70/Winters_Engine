# 그레이스케일 + HLSL = AAA 이펙트의 진실

> 엘든링/롤 이펙트를 뜯었더니 그레이스케일 텍스처와 단순 FBX밖에 없다는 관찰. 정확하다. AAA 이펙트는 **"단순한 데이터 + 극단적으로 정교한 머티리얼 셰이더"** 의 결합이다. 이 문서는 그 분업 구조를 분해한다.

---

## Session 0. 한 줄 답변

> **텍스처는 색이 아니라 데이터다. 색·움직임·라이팅은 셰이더가 만든다. PBR/GI는 거의 안 쓴다 — VFX의 라이팅 모델은 PBR과 다른 계열이다.**

이 문장이 모든 것을 설명한다. 풀어쓰면:

1. **텍스처 = "함수의 입력값"**. 노이즈 패턴, 마스크, 그라디언트. 색이 아니라 0~1 값.
2. **셰이더 = "함수 본체"**. 입력값을 받아서 디자이너가 지정한 색·움직임·디졸브로 변환.
3. **머티리얼 파라미터 = "함수의 인자"**. 디자이너가 인스턴스마다 다르게 설정. 같은 셰이더가 빨강 불 / 초록 독 / 파랑 마법으로 분화.
4. **PBR/GI**: VFX는 표면이 없거나 모호해서 PBR이 적합하지 않음. **VFX 전용 라이팅 모델**(unlit, 6-way, custom)을 쓴다.

이 분업 구조가 **재사용성, 메모리 효율, 디자이너 자유도**를 동시에 충족시킨다. 그래서 이게 표준이 되었다.

---

## Session 1. 그레이스케일은 색이 아니라 데이터다

### 잘못된 멘탈 모델

"텍스처는 그림이다. 디자이너가 빨간 불꽃을 그리면 그게 빨간 불꽃으로 렌더된다."

→ **틀렸다**. 이러면 같은 효과 색만 다른 버전을 만들 때마다 텍스처를 새로 그려야 한다. 메모리 폭발, 작업 폭발.

### 올바른 멘탈 모델

> **텍스처는 모양과 강도(intensity)만 표현한다. "이 픽셀이 얼마나 진한가" 만 0~1로 저장. 색은 셰이더가 추가한다.**

같은 그레이스케일 노이즈 한 장이:
- 머티리얼 A에서 → `BaseColor=red, EmissionColor=orange-yellow` → 화염
- 머티리얼 B에서 → `BaseColor=green-toxic, EmissionColor=acid-green` → 독
- 머티리얼 C에서 → `BaseColor=ice-blue, EmissionColor=cyan` → 얼음 마법
- 머티리얼 D에서 → `BaseColor=dark-purple, EmissionColor=magenta` → 어둠 마법

같은 텍스처 한 장으로 **수십 개 효과**가 파생된다. 이게 단일 그레이스케일 텍스처의 진짜 가치.

### 셰이더 측 코드

```hlsl
cbuffer MaterialParams {
    float3 BaseColor;          // 디자이너가 머티리얼 인스턴스에 설정
    float3 EmissionColor;
    float  EmissionIntensity;  // HDR (>1)
    float  Contrast;
};

float4 PSMain(VSOutput i) : SV_Target {
    // 텍스처는 그저 "강도" — 0~1 그레이스케일
    float intensity = NoiseTex.Sample(LinearWrap, i.UV).r;
    
    // 셰이더가 색을 입힘
    float shapedIntensity = pow(intensity, Contrast);
    float3 baseRGB = BaseColor * shapedIntensity;
    float3 emissive = EmissionColor * EmissionIntensity * shapedIntensity;
    
    return float4(baseRGB + emissive, intensity);
}
```

색이 텍스처에 박혀 있지 않다. 머티리얼 파라미터로 들어와서 곱해진다.

### BC4 압축의 보너스

단일 채널 그레이스케일은 **BC4** 압축 (1채널 8bpp). RGBA를 BC7로 압축한 것보다 화질 보존이 압도적이다.

| 포맷 | 비트율 | 화질 | 채널 |
|------|--------|------|------|
| BC4 (R only) | 4bpp | 매우 좋음 | 1 |
| BC5 (RG) | 8bpp | 좋음 | 2 |
| BC7 (RGBA) | 8bpp | 보통 | 4 |

같은 메모리에 단일 채널을 4배 많이 담을 수 있다. 노이즈, 마스크, 디테일을 잔뜩 가질 여유가 생긴다.

---

## Session 2. 채널 패킹 — 한 텍스처에 네 가지 마스크

물론 RGBA 텍스처도 쓴다. 그런데 **R/G/B/A가 각각 다른 정보**다.

### 흔한 패킹 패턴

```
Pattern A (마법 셰이더용 만능 마스크):
  R: 베이스 알파/디졸브 노이즈
  G: 가장자리 마스크 (smoothstep용)
  B: 이미션 마스크
  A: 디스토션 강도

Pattern B (메쉬 surface VFX용):
  R: 디테일 노이즈 (high freq)
  G: 큰 패턴 (low freq)
  B: 흐름 가이드
  A: 알파

Pattern C (트레일/리본용):
  R: 헤드 마스크 (앞쪽)
  G: 테일 마스크 (뒤쪽)
  B: 이미션 핫스팟
  A: 알파 페이드
```

### 셰이더 사용

```hlsl
float4 packed = MaskTex.Sample(LinearWrap, i.UV);
float dissolveMask = packed.r;
float edgeMask     = packed.g;
float emissionMask = packed.b;
float distortMask  = packed.a;

// 각자 다른 용도로
float dissolve = step(dissolveMask, threshold);
float edge = smoothstep(threshold, threshold + 0.05, dissolveMask) * edgeMask;
float3 emission = EmissionColor * emissionMask * EmissionIntensity;
float2 distortOffset = (distortMask * 2 - 1) * DistortStrength;
```

**한 sample, 네 가지 정보**. GPU 대역폭이 1/4로 줄고, 효과 한 픽셀 비용이 줄어든다.

### LoL 스타일의 전형적 텍스처 세트

LoL 마법 한 발의 텍스처 인벤토리(추정):
- `magic_noise01.tga` — 그레이스케일 노이즈 (BC4)
- `magic_mask_pack.tga` — 4채널 마스크 (R=베이스 모양, G=가장자리, B=이미션, A=디졸브)
- `magic_flow.tga` — 2채널 flowmap (BC5, RG)
- `magic_subuv_packed.tga` — flipbook 4×4 그레이스케일 (BC4)

총 4개 텍스처, 모두 단순. 그러나 머티리얼 한 인스턴스에 BaseColor, EmissionColor, FlowSpeed, DissolveSpeed, FresnelPower, RimColor 등 **20~40개 머티리얼 파라미터**가 노출되어 디자이너가 조작.

같은 셰이더 + 같은 텍스처로 챔피언 100명 × 스킬 4개 = 400가지 마법이 파라미터만 다르게 해서 만들어진다.

---

## Session 3. 색은 어디서 오는가 — Material Parameters

### Material Instance 계층

```
Master Material: M_Magic_Generic.uasset (셰이더 정의)
   ├─ Material Instance: MI_Lux_Q (라이트 바인딩, 노란색 톤)
   ├─ Material Instance: MI_Brand_Q (불꽃, 빨강-주황)
   ├─ Material Instance: MI_Lissandra_Q (얼음, 시안-흰색)
   └─ Material Instance: MI_Veigar_Q (어둠, 보라-검정)
```

Master는 **셰이더 코드 + 노출된 파라미터 인터페이스**. Instance는 **그 인터페이스에 값을 채운 것**. 셰이더 컴파일은 Master에서 한 번, Instance는 cbuffer만 다르게.

### 노출되는 파라미터 종류

```hlsl
cbuffer MaterialParams {
    // 색감
    float3 BaseColor;
    float3 EmissionColor;
    float  EmissionIntensity;     // HDR (1~50)
    float3 RimColor;
    float  RimIntensity;
    
    // 모양 컨트롤
    float  AlphaContrast;         // pow exponent
    float  EdgeWidth;             // smoothstep width
    float  Opacity;
    
    // 움직임
    float2 ScrollSpeed;           // UV pan
    float  FlowStrength;
    float  DistortStrength;
    
    // 디졸브
    float  DissolveSpeed;
    float3 DissolveEdgeColor;
    float  DissolveEdgeWidth;
    
    // 라이프타임 곡선 (1D 텍스처)
    Texture2D ColorOverLife;
    Texture2D AlphaOverLife;
    
    // Fresnel
    float  FresnelPower;
    float3 FresnelColor;
    
    // 변주
    float  RandomVariation;        // per-particle 색 다양성
};
```

이게 Master 머티리얼에 정의된 인터페이스. 디자이너는 이 30~50개 노브를 만지면서 한 캐릭터의 한 스킬을 다른 캐릭터로 변환한다.

### Color Over Life 곡선

가장 강력한 단일 노브:

```hlsl
// 1D 텍스처에 R/G/B 곡선 baked
float3 SampleColorOverLife(float normalizedAge) {
    return ColorOverLifeTex.SampleLevel(LinearClamp,
        float2(normalizedAge, 0.5), 0).rgb;
}

// PS에서:
float3 baseRGB = SampleColorOverLife(i.NormalizedAge);
float3 finalColor = baseRGB * intensity;
```

이거 하나로:
- 화염: white → yellow → orange → red → dark
- 얼음: blue-white → cyan → blue → fade
- 마법: violet → magenta → blue → invisible

**색만 곡선으로 변경 → 효과의 정체성 변경**. 텍스처/메쉬 동일.

### Per-Particle Random Variation

```hlsl
// 각 파티클마다 0~1 랜덤값 (spawn 시 한 번 생성)
float random = i.MaterialRandom;

// 색에 살짝 변주
float3 varied = lerp(ColorA, ColorB, random);

// 또는 hue shift
float3 shifted = HueShift(BaseColor, (random - 0.5) * VariationRange);
```

100개 입자가 100가지 미세하게 다른 색. 한 종류 입자라도 균일하지 않아 보임. 자연스러움의 핵심.

---

## Session 4. VFX와 PBR/GI — 왜 거의 안 쓰는가

### PBR이 안 맞는 이유

PBR(Physically Based Rendering)은 **표면이 명확한 솔리드 오브젝트**에 맞는 모델이다.
- BaseColor + Metallic + Roughness + Normal + AO
- 디퓨즈 + 스펙큘러 분리, 광원에 정확히 반응
- IBL과 GI로 환경 반사 표현

VFX는 거의 정반대 성질이다.

| PBR 전제 | VFX 현실 |
|---------|---------|
| 표면이 명확 | 안개·연기·불은 표면 없음, 빌보드는 fake |
| 알베도 = 표면색 | VFX는 알베도 개념 모호 (불꽃의 알베도가 뭔가?) |
| 단일 광원 → 단일 반응 | VFX는 자기가 광원 (emission) |
| 메모리 여유 | VFX는 수백 입자 오버드로우, 픽셀당 비용 극단적으로 줄여야 함 |
| 사진 같은 사실감 목표 | VFX는 디자이너가 직접 색 컨트롤 원함 |

따라서 VFX 머티리얼은 PBR 파라미터(metallic/roughness)를 거의 안 노출한다. 노출해도 디자이너가 안 만진다.

### GI도 거의 안 쓰는 이유

#### GI를 받는 측 (VFX가 환경 빛 받는 측)

이론적으로 안개가 sky color를 받으면 자연스럽다. 하지만:
- **균일해진다**: GI 받으면 멀리 있든 가까이 있든 안개가 비슷한 색이 됨. 디자이너의 의도된 색감 손실.
- **비용**: 매 입자가 GI probe sample. 입자 만 개면 만 번.
- **충돌**: 디자이너가 입힌 색 + GI 색 = 색 망가짐.

대신: **간단한 ambient term + 직접 광원 반응** 정도만. 또는 6-way lit smoke 같은 전용 모델 (Session 5).

#### GI에 영향 주는 측 (VFX가 환경에 빛 비추는 측)

이쪽은 의미 있다. 폭발이 주변을 비춰야 자연스러움. 하지만 진짜 GI 시스템(Lumen 등)에 emissive 매터리얼로 자동 잡히는 것보다는 **명시적 dynamic light**가 흔하다.

```cpp
// 흔한 패턴: 폭발에 dynamic point light 부착
SpawnPointLight(explosionLocation, color, intensity, radius, duration);
```

이게 GI 시스템 의존하는 것보다 빠르고 컨트롤 잘 됨.

### 예외 케이스

PBR/GI가 약간이라도 들어가는 VFX:
- **Volumetric mesh**: 큰 안개 기둥은 ambient + sun 반응 살짝 받음. 그래도 PBR 풀스택은 아님.
- **메쉬 파티클의 단단한 부분**: 검 트레일 메쉬가 환경 반사 살짝. Cube map sample 정도.
- **안개/먼지의 6-way lighting**: 사실상 PBR 대체 (Session 5).

엘든링이 LoL보다 사실적인 톤이라 이런 케이스가 좀 더 많지만, 여전히 **VFX의 95%는 PBR과 별개의 라이팅 시스템**이다.

---

## Session 5. VFX가 실제 쓰는 라이팅 모델

PBR이 아니면 무엇? 다섯 가지 정도가 있다.

### 1. Unlit (Additive)

```hlsl
float4 PSMain(VSOutput i) : SV_Target {
    float intensity = NoiseTex.Sample(LinearWrap, i.UV).r;
    float3 emissive = EmissionColor * intensity * EmissionIntensity;
    return float4(emissive, intensity);
}

// Blend: Src=ONE, Dst=ONE (additive)
```

라이팅 무시. 자기가 광원. 화염, 마법, 빛, 임팩트 코어.

**VFX의 70~80%가 이거다**. 가장 단순, 가장 빠름.

### 2. Simple Lit (Diffuse + Ambient)

연기, 먼지처럼 자기가 빛 안 내는 효과.

```hlsl
float4 PSMain(VSOutput i) : SV_Target {
    float density = NoiseTex.Sample(LinearWrap, i.UV).r;
    
    // 단순 lambertian + ambient
    float3 normal = i.WorldNormal;  // 또는 fake sphere normal
    float NdotL = saturate(dot(normal, -SunDir));
    float3 lit = SunColor * NdotL + AmbientColor;
    
    float3 baseRGB = BaseColor * lit;
    return float4(baseRGB * density, density);
}

// Blend: Src=SrcAlpha, Dst=InvSrcAlpha (alpha blend)
```

PBR보다 훨씬 단순. 환경 빛 반응 정도만.

### 3. 6-Way Lit Smoke

이전 문서에서 다뤘던 그것. 연기에 directional light가 닿는 느낌.

```hlsl
// 6 마스크: ±X, ±Y, ±Z 방향 광원에 대한 반응
float4 m0 = LightingTex0.Sample(LinearWrap, i.UV);   // +X, -X, +Y, -Y
float2 m1 = LightingTex1.Sample(LinearWrap, i.UV).rg; // +Z, -Z

float3 lightDir = -SunDir;  // 광원에서 픽셀로
float wPx = saturate(lightDir.x), wNx = saturate(-lightDir.x);
float wPy = saturate(lightDir.y), wNy = saturate(-lightDir.y);
float wPz = saturate(lightDir.z), wNz = saturate(-lightDir.z);

float litMask = m0.r*wPx + m0.g*wNx + m0.b*wPy + m0.a*wNy
              + m1.r*wPz + m1.g*wNz;

float3 baseRGB = BaseColor * (SunColor * litMask + AmbientColor);
```

PBR과 라이팅 결과가 비슷해 보이지만, **본질적으로 다른 시스템**. 표면이 아니라 6방향 라이트 응답을 미리 baked.

### 4. Volumetric Custom

ray march 기반. 안에서 노이즈 적분.

```hlsl
float density = 0;
float3 emission = 0;
for (int s = 0; s < NumSteps; s++) {
    float3 p = ...;
    float n = fbm3D(p);
    density += n * step;
    emission += FireColor * n * (해당 step에서의 가시성);
}
```

Unlit이지만 적분 통해 두께감/연기감 살림. PBR과 무관.

### 5. Custom Toon / Stylized

LoL 같은 스타일라이즈드 셰이딩.

```hlsl
// 이산화된 라이팅 (cell shading)
float NdotL = dot(normal, -lightDir);
float cellLit = NdotL > 0.5 ? 1.0 : (NdotL > 0.0 ? 0.5 : 0.0);

// 강한 fresnel rim
float rim = pow(1 - saturate(dot(normal, viewDir)), 4);

float3 final = BaseColor * cellLit + RimColor * rim * RimIntensity;
```

또는 라이팅 자체 없이 그라디언트 기반:

```hlsl
// view space normal로 그라디언트 sample
float gradY = i.ViewNormal.y * 0.5 + 0.5;
float3 lit = TopColor * gradY + BottomColor * (1 - gradY);
```

태양 방향 무관, "디자이너가 정한 톱→바텀 그라디언트"가 항상. LoL의 비-PBR 스타일라이즈드 셰이딩이 이 계열.

### 표

| 모델 | 사용처 | 비용 | PBR과의 관계 |
|------|--------|------|--------------|
| Unlit | 화염, 마법, 빛 | 매우 낮음 | 무관 |
| Simple Lit | 연기, 먼지 (저비용) | 낮음 | PBR의 단순화 |
| 6-Way Lit | 안개, 폭발 연기 (고품질) | 중간 | PBR과 별 시스템 |
| Volumetric | 불기둥, 안개기둥 | 높음 | 무관 |
| Stylized | LoL/만화 게임 | 낮음 | PBR 대체 |

VFX 셰이더 90%가 unlit / simple lit / stylized 중 하나다.

---

## Session 6. 셰이더의 "극한 깎기"가 실제 무엇인가

질문의 핵심: **HLSL을 어떻게 그렇게 정교하게 만지길래 그레이스케일 한 장이 마법이 되는가?**

답은: **여러 단순한 트릭을 조합**한다. 각 트릭은 5~20줄 HLSL. 합치면 200~500줄.

### 흔히 쓰이는 단위 트릭들

```hlsl
// 1. UV Pan (가장 기본)
float2 panUV = i.UV + ScrollSpeed * Time;

// 2. UV Distortion (다른 노이즈로 UV 휘기)
float2 noiseUV = i.UV * DistortScale + Time * DistortFlow;
float2 distortion = (NoiseTex2.Sample(samp, noiseUV).rg * 2 - 1) * DistortStrength;
float2 distortedUV = panUV + distortion;

// 3. Domain Warping (UV 자체를 노이즈로 휨)
float warp1 = NoiseTex.Sample(samp, panUV * 0.5).r;
float warp2 = NoiseTex.Sample(samp, panUV * 0.5 + 5.2).r;
float2 warpedUV = panUV + float2(warp1, warp2) * WarpStrength;

// 4. 두 노이즈 곱 (디테일 풍부함)
float n1 = NoiseTex.Sample(samp, warpedUV * 1.0).r;
float n2 = NoiseTex.Sample(samp, warpedUV * 2.7 + 0.3).r;
float combined = n1 * n2;

// 5. Threshold + Smoothstep edge (가장자리 강조)
float threshold = i.NormalizedAge;
float dissolved = combined - threshold;
clip(dissolved);  // 사라지는 부분 폐기
float edge = smoothstep(0, EdgeWidth, dissolved);
float emission = (1 - edge) * EdgeIntensity;

// 6. Polar coordinate (마법진 형태)
float2 centered = i.UV - 0.5;
float r = length(centered);
float a = atan2(centered.y, centered.x) / (2*PI);
float2 polarUV = float2(a + Time * RotSpeed, r);
float ring = NoiseTex.Sample(samp, polarUV).r;

// 7. Fresnel rim (가장자리 빛남)
float fresnel = pow(1 - saturate(dot(i.Normal, ViewDir)), FresnelPow);
float3 rim = RimColor * fresnel;

// 8. Per-particle 변주
float variation = i.MaterialRandom;
float3 variedColor = lerp(ColorA, ColorB, variation);
float3 hueShift = HueShift(BaseColor, (variation - 0.5) * 0.3);

// 9. Sub-UV 보간 flipbook
float frameFloat = i.NormalizedAge * (NumFrames - 1);
int frame0 = floor(frameFloat); int frame1 = frame0 + 1;
float blend = frac(frameFloat);
float c0 = SampleSubUV(panUV, frame0).r;
float c1 = SampleSubUV(panUV, frame1).r;
float frame = lerp(c0, c1, blend);

// 10. Soft particle
float sceneZ = LinearizeDepth(SceneDepthTex.Sample(point, screenUV));
float soft = saturate((sceneZ - i.ViewZ) / SoftDistance);

// 11. Color over life
float3 colorAtAge = ColorOverLife.Sample(samp, float2(i.NormalizedAge, 0.5)).rgb;

// 12. Center mask (중심 강조)
float2 fromCenter = i.UV - 0.5;
float centerMask = 1 - saturate(length(fromCenter) * 2);
centerMask = pow(centerMask, CenterPow);

// 13. Vertex animation (정점에서 미리 노이즈 적용)
// VS 측: i.Pos += i.Normal * NoiseTex.SampleLevel(samp, vUV, 0).r * BulgeStrength;
```

### 합치면

이 13개 정도를 골라서 합치면 한 마법 셰이더가 된다.

```hlsl
float4 PSMain(VSOutput i) : SV_Target {
    // 1. Time + per-particle pan
    float2 panUV = i.UV + ScrollSpeed * Time + i.MaterialRandom * 0.1;
    
    // 2. Distort UV
    float2 distNoise = (NoiseTex2.Sample(samp, panUV * 0.7 + Time * 0.1).rg * 2 - 1);
    float2 distortedUV = panUV + distNoise * DistortStrength;
    
    // 3. Two-octave noise
    float n1 = NoiseTex.Sample(samp, distortedUV).r;
    float n2 = NoiseTex.Sample(samp, distortedUV * 2.3 + 1.7).r;
    float n = n1 * 0.7 + n2 * 0.3;
    
    // 4. Center mask
    float centerMask = 1 - saturate(length(i.UV - 0.5) * 2);
    centerMask = pow(centerMask, 2);
    
    // 5. Combined alpha
    float alpha = n * centerMask;
    
    // 6. Dissolve over life
    float threshold = i.NormalizedAge * 1.2 - 0.1;
    alpha = saturate(alpha - threshold) / (1 - threshold + 0.01);
    
    // 7. Edge emission
    float edge = smoothstep(0, EdgeWidth, alpha) * (1 - smoothstep(EdgeWidth, EdgeWidth*2, alpha));
    
    // 8. Color over life
    float3 lifeColor = ColorOverLife.Sample(samp, float2(i.NormalizedAge, 0.5)).rgb;
    
    // 9. Per-particle hue
    float3 baseColor = lerp(BaseColor * 0.9, BaseColor * 1.1, i.MaterialRandom);
    
    // 10. Final color
    float3 finalColor = baseColor * lifeColor * alpha
                      + EmissionColor * edge * EmissionIntensity
                      + EmissionColor * centerMask * CoreIntensity;
    
    // 11. Soft particle
    float sceneZ = LinearizeDepth(SceneDepthTex.Sample(point, i.ScreenUV));
    float soft = saturate((sceneZ - i.ViewZ) / SoftDistance);
    alpha *= soft;
    
    // 12. Per-particle alpha curve
    float alphaCurve = AlphaOverLife.Sample(samp, float2(i.NormalizedAge, 0.5)).r;
    alpha *= alphaCurve;
    
    return float4(finalColor, alpha);
}
```

이게 **그레이스케일 노이즈 두 장 + 1D 라이프 곡선 두 장**으로 만든 매직 셰이더의 전형. 디자이너가 BaseColor/EmissionColor/Curves만 바꾸면 다른 마법이 됨.

"극한 깎기" = **이 13개 트릭을 디자이너 노브로 노출하고, 머티리얼 인스턴스마다 다르게 조합**.

---

## Session 7. LoL의 손그림 미학 — 텍스처는 "데이터"가 아니라 "스트로크"

LoL은 또 다르다. 일반 AAA보다 **더 적극적으로 텍스처를 단순화**.

### Riot의 워크플로우 (공개된 GDC 토크 기반)

1. **컨셉 → 손그림 brush stroke 텍스처**: 페인팅 소프트웨어에서 brush로 그린 형태감 텍스처. 그레이스케일.
2. **메쉬는 단순 FBX**: 검광 메쉬, 디스크, 콘 같은 기본 도형. 본 애니메이션 거의 없음.
3. **머티리얼이 모든 것**: 색, 라이팅, 디졸브, 그라디언트, fresnel 모두 HLSL.
4. **인스턴싱**: 같은 메쉬 + 같은 셰이더, 각 챔피언이 다른 머티리얼 인스턴스로 분기.

### 왜 단순한 메쉬와 텍스처인가

LoL은 **탑다운 카메라**다. 화면당 입자/메쉬가 매우 많다. 챔피언 10명 동시 5스킬 = 50개 동시 효과.

조건:
- 모바일/저사양 PC도 돌아가야 함
- 60FPS 유지
- 가독성 (어느 챔피언의 어느 스킬인지 한눈에)

→ **메쉬와 텍스처는 단순하게, 셰이더로 가독성/스타일라이제이션 보강**.

### 가독성 우선의 디자인 원칙

LoL VFX는 사실성을 추구하지 않는다. **읽기 쉬워야 함**이 최우선.
- 색이 명확해야 함 (어느 챔피언이 누구의 스킬인지)
- 형태가 명확해야 함 (히트박스가 어디인지)
- 타이밍이 명확해야 함 (피하는 데 필요한 정보)

이 모든 게 **셰이더의 강한 색감, 강한 fresnel, 강한 컨트라스트**로 달성. PBR로는 절대 못 만들 룩.

### 전형적인 LoL 마법 셰이더 골격

```hlsl
// 단순화된 LoL-style 셰이더
float4 PSMain(VSOutput i) : SV_Target {
    // 1. 손그림 텍스처 sample (brush 형태감)
    float brush = BrushTex.Sample(LinearWrap, i.UV + ScrollSpeed * Time).r;
    
    // 2. 두 색의 강한 그라디언트 (라이프 곡선 또는 마스크 기반)
    float3 colorA = HotCoreColor;       // 노랑-하양 코어
    float3 colorB = OutlineColor;       // 파랑-보라 외곽
    float gradient = pow(brush, 2);      // 컨트라스트 강조
    float3 mainColor = lerp(colorB, colorA, gradient);
    
    // 3. 강한 fresnel rim (스타일라이제이션 핵심)
    float fresnel = pow(1 - saturate(dot(i.Normal, ViewDir)), 3);
    float3 rim = RimColor * fresnel * 2;  // HDR
    
    // 4. 시간 기반 디졸브 (들어오고 나가는 페이드)
    float fadeIn  = smoothstep(0.0, 0.1, i.NormalizedAge);
    float fadeOut = 1 - smoothstep(0.7, 1.0, i.NormalizedAge);
    float alpha = brush * fadeIn * fadeOut;
    
    // 5. HDR 이미션으로 블룸
    float3 emission = mainColor * EmissionIntensity;  // 5~20
    
    return float4(emission + rim, alpha);
}
```

40줄 셰이더 + 단일 그레이스케일 텍스처 + 단일 메쉬 = LoL 한 스킬.

핵심: **HDR 색상 (1~20 범위), 강한 fresnel, 강한 컨트라스트 그라디언트**. 이게 LoL의 시그니처 룩.

### 챔피언 간 차이는 머티리얼 인스턴스로

같은 셰이더를 쓰는 챔피언들 사이의 차이:

| 챔피언 | HotCoreColor | OutlineColor | RimColor | 시각적 정체성 |
|--------|--------------|--------------|----------|---------------|
| Lux | 노랑(2,2,1) | 파랑(0.3,0.5,1) | 흰색(2,2,2) | 빛 마법사 |
| Brand | 흰색(3,3,2) | 빨강-주황(2,0.5,0.1) | 노랑(3,2,0.5) | 화염 |
| Veigar | 보라(1,0.3,2) | 검정(0.1,0,0.2) | 마젠타(2,0.5,3) | 어둠 |
| Lissandra | 시안(1,2,2) | 진청(0.2,0.4,0.8) | 흰청(2,3,3) | 얼음 |

같은 셰이더, 같은 메쉬, 같은 텍스처. 색상 인스턴스 변경만으로 4가지 마법사. 이게 채널 분리의 진짜 가치.

---

## Session 8. 한 노이즈 텍스처가 마법이 되는 풀 예시

가장 구체적인 답을 위해. 입력 자산:

- `noise01.png`: 256×256 그레이스케일 perlin noise (BC4)
- `M_Magic_Surface.usf`: 셰이더
- `MI_FireMagic.uasset`: 머티리얼 인스턴스

이게 전부. 아래 셰이더가 변환을 담당.

```hlsl
// =================================================================
// M_Magic_Surface.usf — 단일 노이즈 텍스처를 마법 표면으로 변환
// =================================================================

cbuffer MaterialParams : register(b1) {
    // 색
    float3 BaseColor;             // RGB
    float3 EmissionColor;         // HDR
    float  EmissionIntensity;
    float3 EdgeColor;             // 가장자리 색
    
    // 모양
    float  ContrastPower;         // 1~5
    float  AlphaThreshold;        // 0~1
    float  EdgeWidth;             // 0.01~0.2
    
    // 움직임
    float2 ScrollSpeedA;          // primary scroll
    float2 ScrollSpeedB;          // secondary scroll (다른 속도, 디테일)
    float  DistortStrength;       // UV 휨
    
    // 디졸브
    float  DissolveSpeed;
    
    // Fresnel
    float  FresnelPow;
    float3 FresnelColor;
    
    // 변주
    float  RandomVariation;
};

Texture2D NoiseTex : register(t0);
SamplerState LinearWrap;
SamplerState LinearClamp;
Texture2D SceneDepthTex : register(t1);

float LinearizeDepth(float rawZ) {
    return DepthLinearizeParams.y / (rawZ - DepthLinearizeParams.x);
}

struct VSOutput {
    float4 PosCS            : SV_Position;
    float2 UV               : TEXCOORD0;
    float4 Color            : COLOR0;
    float  ViewZ            : TEXCOORD1;
    float3 WorldNormal      : TEXCOORD2;
    float3 WorldPos         : TEXCOORD3;
    float  MaterialRandom   : TEXCOORD4;
    float  NormalizedAge    : TEXCOORD5;
    float2 ScreenUV         : TEXCOORD6;
};

float4 PSMain(VSOutput i) : SV_Target {
    // ========================================================
    // STEP 1: 이중 UV 스크롤 (다른 속도 — 디테일 분리)
    // ========================================================
    float perParticleOffset = i.MaterialRandom * 6.28;
    float2 uvA = i.UV + ScrollSpeedA * Time + perParticleOffset;
    float2 uvB = i.UV * 1.7 + ScrollSpeedB * Time + perParticleOffset * 1.3;
    
    // ========================================================
    // STEP 2: UV 디스토션 (B 노이즈가 A의 UV를 휨)
    // ========================================================
    float distortNoise = NoiseTex.Sample(LinearWrap, uvB * 0.5).r;
    float2 distortVec  = float2(
        NoiseTex.Sample(LinearWrap, uvB * 0.7 + 1.3).r * 2 - 1,
        NoiseTex.Sample(LinearWrap, uvB * 0.7 + 5.7).r * 2 - 1
    );
    uvA += distortVec * DistortStrength;
    
    // ========================================================
    // STEP 3: 두 옥타브 노이즈 합성
    // ========================================================
    float n_low  = NoiseTex.Sample(LinearWrap, uvA).r;
    float n_high = NoiseTex.Sample(LinearWrap, uvA * 2.7 + 0.5).r;
    float n = n_low * 0.7 + n_high * 0.3;
    
    // ========================================================
    // STEP 4: 컨트라스트 셰이핑
    // ========================================================
    n = pow(n, ContrastPower);
    
    // ========================================================
    // STEP 5: 중심 마스크 (모양에 핵심 부여)
    // ========================================================
    float2 fromCenter = i.UV - 0.5;
    float r = length(fromCenter) * 2;
    float centerMask = saturate(1 - r);
    centerMask = pow(centerMask, 2);
    
    // ========================================================
    // STEP 6: 디졸브 over life
    // ========================================================
    float dissolveThreshold = i.NormalizedAge * DissolveSpeed;
    float dissolved = n - dissolveThreshold;
    clip(dissolved + EdgeWidth);  // 가장자리 살짝 남기고 폐기
    
    // ========================================================
    // STEP 7: 가장자리 마스크
    // ========================================================
    float edgeMask = 1 - smoothstep(0, EdgeWidth, dissolved);
    
    // ========================================================
    // STEP 8: 코어 마스크 (가장자리 아닌 안쪽)
    // ========================================================
    float coreMask = saturate(dissolved / EdgeWidth);
    
    // ========================================================
    // STEP 9: Fresnel
    // ========================================================
    float3 viewDir = normalize(CameraPos - i.WorldPos);
    float fresnel = pow(1 - saturate(dot(i.WorldNormal, viewDir)), FresnelPow);
    
    // ========================================================
    // STEP 10: 색상 합성
    // ========================================================
    float3 baseRGB    = BaseColor * coreMask * centerMask;
    float3 emissionRGB = EmissionColor * EmissionIntensity * coreMask * centerMask;
    float3 edgeRGB    = EdgeColor * edgeMask * EmissionIntensity * 2;
    float3 fresnelRGB = FresnelColor * fresnel * EmissionIntensity;
    
    // 색에 per-particle 변주
    float3 hue = lerp(float3(0.9, 0.9, 1.1), float3(1.1, 1.0, 0.9),
                     i.MaterialRandom);
    
    float3 finalRGB = (baseRGB + emissionRGB + edgeRGB + fresnelRGB) * hue;
    
    // ========================================================
    // STEP 11: Alpha 합성
    // ========================================================
    float alpha = saturate(coreMask + edgeMask) * centerMask;
    
    // ========================================================
    // STEP 12: Soft particle
    // ========================================================
    float sceneZ = LinearizeDepth(SceneDepthTex.SampleLevel(LinearClamp, i.ScreenUV, 0));
    float softFade = saturate((sceneZ - i.ViewZ) / 50.0);
    alpha *= softFade;
    
    // ========================================================
    // STEP 13: 라이프타임 fade
    // ========================================================
    float fadeIn = smoothstep(0.0, 0.1, i.NormalizedAge);
    float fadeOut = 1 - smoothstep(0.8, 1.0, i.NormalizedAge);
    alpha *= fadeIn * fadeOut;
    
    return float4(finalRGB, alpha);
}
```

### 입력 vs 출력 비교

**입력**:
- 256×256 그레이스케일 노이즈 한 장 (~32KB)
- 메쉬: 4정점 quad (또는 sphere mesh)
- 머티리얼 파라미터: 20개 float

**출력**:
- 디스토션, 두 옥타브 노이즈, 디졸브, 가장자리 emission, 중심 강조, fresnel, soft particle, fade in/out, per-particle 변주
- 시각적으로는 **출렁이며 가장자리가 빛나는 마법 표면**, 디졸브로 들어왔다 사라짐
- HDR이라 bloom으로 빛나는 코어
- 머티리얼 인스턴스 변경만으로 화염/얼음/독/어둠 분화

### 머티리얼 인스턴스로 만드는 4가지 마법

```
[MI_FireMagic]
  BaseColor:        (0.5, 0.1, 0.0)   어두운 빨강
  EmissionColor:    (4.0, 1.5, 0.2)   HDR 주황
  EdgeColor:        (5.0, 4.0, 1.0)   HDR 노랑
  FresnelColor:     (3.0, 2.0, 0.5)   따뜻한 톤
  ContrastPower:    2.5               선명함
  ScrollSpeedA:     (0, 0.5)          위로 흐름
  
[MI_IceMagic]
  BaseColor:        (0.1, 0.3, 0.5)   파랑
  EmissionColor:    (1.0, 2.5, 4.0)   HDR 시안
  EdgeColor:        (3.0, 4.0, 5.0)   HDR 흰-청
  FresnelColor:     (2.0, 3.0, 4.0)
  ContrastPower:    4.0               더 선명 (얼음)
  ScrollSpeedA:     (0.1, 0.05)       느림
  
[MI_PoisonMagic]
  BaseColor:        (0.1, 0.4, 0.1)   초록
  EmissionColor:    (0.5, 3.0, 0.2)   HDR 산성 초록
  EdgeColor:        (2.0, 4.0, 0.5)   더 밝은 초록
  FresnelColor:     (1.0, 2.5, 0.5)
  ContrastPower:    1.5               부드러움 (액체 느낌)
  ScrollSpeedA:     (0.2, 0.3)
  
[MI_VoidMagic]
  BaseColor:        (0.2, 0.0, 0.3)   어두운 보라
  EmissionColor:    (3.0, 0.5, 4.0)   HDR 마젠타
  EdgeColor:        (4.0, 1.0, 5.0)
  FresnelColor:     (2.5, 0.5, 3.5)
  ContrastPower:    3.0
  ScrollSpeedA:     (0.0, -0.3)       아래로 흐름 (불안)
```

같은 셰이더, 같은 텍스처, 같은 메쉬. 색감 차이만으로 4가지 정체성. 이게 진짜 답이다.

---

## Session 9. 메쉬가 형태감을 담당한다

질문에 "롤은 fbx만 있고"라고 한 부분의 의미.

LoL에서 메쉬가 하는 역할:

### 단순 도형 메쉬

| 메쉬 형태 | 용도 |
|----------|------|
| Quad (빌보드) | 입자 |
| Sphere | 폭발 코어, 마법 오브 |
| Disc/Ring | 마법진, 충격파 |
| Cone | 차징 빔, 스폿 효과 |
| Cylinder | 빔, 기둥 |
| Curved Plane | 검 트레일, 슬래시 |
| Custom Curved Mesh | 캐릭터 고유 형태 (Lux의 빛 길) |

거의 다 100~500 정점. 본 애니메이션 없음.

### 메쉬가 하는 것: 형태감과 시야각

빌보드는 정면만 좋다. 사방에서 봐도 그럴듯하려면 메쉬다.

- 마법진을 quad로 만들면 옆에서 보면 종이. Disc로 만들면 두께감.
- 검 트레일을 ribbon으로만 만들면 단면적. Curved plane mesh면 입체.
- 폭발을 빌보드로 만들면 평면. Sphere mesh + ray march면 부피감.

### 셰이더가 그 위에서 모든 일을 함

메쉬는 형태만 제공. 색/움직임/디졸브는 셰이더.

```hlsl
// Disc 메쉬에서 polar coordinate로 마법진 그리기
float2 fromCenter = i.LocalPos.xz;  // disc 표면 좌표
float r = length(fromCenter) / DiscRadius;  // 0~1
float a = atan2(fromCenter.y, fromCenter.x) / (2*PI);

// 룬 패턴 (각도 기준 분할)
float runeMask = step(0.95, frac(a * 12 + Time));  // 12개 룬
float ringMask = smoothstep(0.85, 0.9, r) * (1 - smoothstep(0.95, 1.0, r));
float innerMask = step(0.7, r) * (1 - step(0.85, r));

float pattern = runeMask * ringMask + innerMask;
return float4(EmissionColor * pattern * EmissionIntensity, pattern);
```

같은 disc 메쉬 + 다른 셰이더 = 다른 마법진 (소환진/폭발진/축복진).

> **결론**: 메쉬 = 형태, 셰이더 = 외관. 메쉬는 단순할수록 좋다 (재사용성). 셰이더는 정교할수록 좋다 (스타일 표현력).

---

## Session 10. 엘든링과 LoL의 차이

같은 원리지만 톤이 다르다.

### LoL (스타일라이즈드, painterly)

- HDR 강함 (이미션 5~30)
- Fresnel 강함 (rim 두드러짐)
- 색상 채도 높음
- 검은 외곽 (cell shading 같은 느낌)
- 손그림 brush 텍스처
- Unlit / stylized 셰이더 위주

### 엘든링 (반-사실적, naturalistic)

- HDR 적당함 (1~5)
- Fresnel 미묘함 (자연스러움)
- 색상 채도 절제 (지저분한 톤)
- 그라디언트 부드러움
- 사진/스캔 베이스 노이즈
- 6-way lit / volumetric 셰이더 적극

### 셰이더 차이의 실제 모습

LoL 화염:
```hlsl
// 강한 컨트라스트, 선명한 색
float3 color = lerp(DarkOrange, BrightYellow, pow(noise, 0.5));
color *= 5;  // HDR 강조
return float4(color * fresnel * 3, alpha);  // fresnel rim 강함
```

엘든링 화염:
```hlsl
// 부드러운 그라디언트, 자연스러운 톤
float3 color = SampleColorOverLife(age);  // 미세한 톤 변화
float3 lit = color * 2.5;  // 절제된 HDR
// Sky color 살짝 받기 (전체 균형감)
lit += SkyColor * 0.1 * smokeMask;
return float4(lit, alpha);
```

핵심은 **머티리얼 파라미터의 톤 차이**다. 같은 셰이더 골격에 색감을 다르게 칠하면 두 룩 모두 가능. 엘든링이 LoL 셰이더로 마법 만들면 LoL 같음, 거꾸로도.

> **즉, 셰이더 기법보다 디자이너의 색채 감각이 룩의 차이를 만든다**.

---

## Session 11. 그레이스케일 텍스처에 무엇을 담는가 — 정리

지금까지 본 모든 패턴을 한 번에:

| 텍스처 종류 | 채널 | 셰이더에서 무엇이 됨 |
|-----------|------|------|
| 노이즈 (FBM, Perlin) | R | 모양 ・ 디스토션 ・ 디졸브 ・ surface 디테일 |
| 마스크 (gradient) | R/G/B/A | 가장자리 ・ 중심 강조 ・ 페이드 |
| Flowmap | RG | 흐름 방향 (각 픽셀 자기 흐름) |
| Normal map | RG (BC5) 또는 RGB | 디스토션 벡터 ・ fake normal |
| Sub-UV flipbook | R (BC4) | 시간 기반 형태 변화 (8×8 = 64 프레임) |
| Color over life | RGB (1D 텍스처) | 시간에 따른 색 곡선 |
| 6-way lighting | 6채널 (두 RGBA에 packing) | 디렉셔널 라이트 응답 |
| VAT | RGB (positions) | 정점 시간별 위치 |
| Vector field | RGB (3D 텍스처) | 위치별 force vector |

각 텍스처는 **함수의 입력**. 셰이더가 함수의 본체. 머티리얼 파라미터가 함수의 인자.

이게 AAA VFX의 **데이터-함수 분리**의 본질이다.

---

## Session 12. FxGraph 시사점 — 우선순위 머티리얼 설계

이걸 알면 FxGraph 머티리얼 시스템 설계가 단순해진다.

### 1. Master Material 한두 개로 충분

거대한 셰이더 노드 그래프 시스템 만들 필요 없다. **잘 만든 master material 2~3개**가 99%를 커버한다:

- `M_VFX_Particle_Generic.usf` — 빌보드/메쉬 입자 (위 풀 예시 셰이더)
- `M_VFX_Trail.usf` — ribbon/trail
- `M_VFX_Volumetric.usf` — 메쉬 안 ray march
- (선택) `M_VFX_Decal.usf` — 데칼

각 master에 30~50개 파라미터 노출. 디자이너는 이 파라미터만 만짐.

### 2. 노드 그래프 머티리얼 에디터 — 늦게

Unreal/Unity의 노드 머티리얼 에디터를 흉내내는 건 거대한 작업이다. **그러나 필요 없다** (장기적으로는 만들겠지만 초기엔 불필요).

대신:
- Master material을 코드로 작성
- Material instance asset에서 파라미터만 노출
- 디자이너는 인스턴스를 복제하고 파라미터 조정

이게 LoL/대부분 인디 게임이 실제로 쓰는 워크플로우. 충분히 강력.

### 3. 머티리얼 인스턴스 = 데이터 애셋

```cpp
class UFxMaterialInstance : public UFxAsset {
public:
    UFxMasterMaterial* Master;  // 셰이더 코드
    
    // Master에서 노출한 파라미터에 값 채우기
    TMap<FName, float>      ScalarParams;
    TMap<FName, FLinearColor> VectorParams;
    TMap<FName, UTexture*>  TextureParams;
};
```

런타임에 셰이더 컴파일은 Master에서 한 번. Instance는 cbuffer만.

### 4. 그레이스케일 텍스처 라이브러리

처음에 100개 정도의 단순 그레이스케일 노이즈/마스크/flowmap 텍스처만 잘 만들면 그게 콘텐츠의 70%다. **콘텐츠 = 텍스처 200장 + 머티리얼 인스턴스 5000개** (1 텍스처 × 25 인스턴스 변주).

엘든링도 LoL도 텍스처 자체는 그렇게 많지 않다. 인스턴스 조합이 많은 것.

### 5. 우선 구현할 셰이더 패턴

위 풀 예시 셰이더의 13단계 중에서 가장 본전 뽑는 4개:

1. **UV Pan + 디스토션** — 모든 흐르는 효과의 기본
2. **컨트라스트/디졸브** — 시간 기반 등장/소멸
3. **Color over life + Per-particle 변주** — 색감의 풍부함
4. **HDR 이미션 + Fresnel** — 빛나는 느낌의 80%

이 4개 패턴만 잘 노출해도 단일 master material로 마법/화염/연기/얼음/폭발 다 커버.

나머지(6-way lit, volumetric, motion vector flipbook)는 Phase 2에서 추가.

---

# 마무리 — 한 문장으로

> **AAA 이펙트는 "단순한 그레이스케일 데이터" 위에 "복잡한 머티리얼 셰이더"를 얹어서 만든다. PBR/GI는 거의 안 쓰며, VFX 전용 라이팅 모델(unlit, simple lit, 6-way, custom)을 쓴다. 색·움직임·라이팅의 모든 결정은 셰이더가 머티리얼 파라미터를 받아서 만든다. 그래서 같은 텍스처 + 같은 메쉬 + 같은 셰이더가 머티리얼 인스턴스 한 줄 차이로 4가지 마법이 된다.**

이 구조를 받아들이는 순간:
- 텍스처 작업량이 1/10로 줄어든다
- 콘텐츠 변주가 무한해진다
- VRAM이 절약된다
- 디자이너가 코드 없이 새 효과를 만든다

엘든링과 LoL이 그렇게 다른 룩인데도 **같은 분업 구조**를 쓰는 이유다. 룩의 차이는 머티리얼 파라미터의 톤 차이일 뿐, 시스템 차이가 아니다.

FxGraph도 같은 길로 가면 된다.
