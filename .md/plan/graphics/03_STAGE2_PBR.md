# Stage 2 — PBR 파이프라인 (Metallic/Roughness + IBL)

## 목표

Stage 1 BRDF 를 실제 게임 씬에 적용. **Metallic/Roughness 워크플로우** + **Image-Based Lighting (IBL)**
로 실시간 환경 조명.

## Metallic / Roughness 워크플로우

업계 표준 파라미터화. 아티스트가 "금속성 + 거칠기" 만 조정.

| 맵 | 포맷 | 설명 |
|---|---|---|
| BaseColor | sRGB 8-bit (R8G8B8) | 확산/금속 알베도 |
| Normal | R8G8 (tangent space) | 노멀 맵 |
| Metallic | R8 (linear) | 0 = 비금속, 1 = 금속 |
| Roughness | R8 (linear) | 0 = 거울, 1 = 완전 확산 |
| AO | R8 (linear) | 앰비언트 오클루전 |

최적화: Metallic+Roughness+AO 를 R8G8B8 하나로 패킹.

## Material 클래스 확장

```cpp
// Engine/Public/Renderer/PBR/Material.h
class CMaterialPBR : public CMaterial
{
public:
    static unique_ptr<CMaterialPBR> Create(const std::string& name);

    // 텍스처 슬롯
    void SetBaseColor(CTexture* tex);
    void SetNormal(CTexture* tex);
    void SetMetallicRoughness(CTexture* tex);   // MRA 패킹
    void SetEmissive(CTexture* tex);

    // 파라미터 (텍스처 없을 때 또는 multiplier)
    void SetBaseColorFactor(const Vec4& color);
    void SetMetallicFactor(f32_t m);
    void SetRoughnessFactor(f32_t r);
    void SetEmissiveFactor(const Vec3& e);

    MaterialGPU ToGPU() const override;

private:
    CMaterialPBR() = default;
};
```

## GLTF / 표준 에셋 포맷

GLTF 2.0 의 metallicRoughness 정책 채택. Assimp 로딩 시 바로 매핑.

## IBL (Image-Based Lighting)

환경 맵 (HDRI) 을 광원으로 활용. **사전 계산 (precompute)** + **런타임 샘플링**.

### 3가지 LUT/Map 생성

| 이름 | 용도 | 크기 |
|---|---|---|
| **Irradiance Map** | Diffuse 라이팅 | 32×32×6 (CubeMap) |
| **Prefiltered Environment Map** | Specular (roughness 별 mip) | 256×256×6 × 5 mip |
| **BRDF LUT** | Split-Sum 적분 정적 LUT | 256×256 2D |

### Irradiance Map (Diffuse Precompute)

```
E(n) = (1/π) × ∫_Ω L_i(ω_i) × (ω_i · n) dω_i
```

각 방향 `n` 에 대해 반구 적분. 낮은 해상도 (32×32) 로 충분.

```hlsl
// Shaders/PBR/IrradianceBaker.hlsl
[numthreads(8, 8, 1)]
void CS_Irradiance(uint3 id : SV_DispatchThreadID)
{
    float3 N = CubemapDirection(id);
    float3 irradiance = 0;
    
    const int SAMPLE_COUNT = 1024;
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        float2 xi = Hammersley(i, SAMPLE_COUNT);
        float3 L  = CosineWeightedHemisphere(xi, N);
        irradiance += g_envMap.SampleLevel(s_linear, L, 0).rgb;
    }
    irradiance /= SAMPLE_COUNT;
    
    g_irradianceOut[id] = float4(irradiance, 1);
}
```

### Prefiltered Environment Map (Specular)

Roughness 별로 환경 맵 흐림 처리. GGX 분포에 따라 importance sampling.

```hlsl
[numthreads(8, 8, 1)]
void CS_Prefilter(uint3 id : SV_DispatchThreadID)
{
    float roughness = float(g_mipLevel) / float(g_mipCount - 1);
    float3 N = CubemapDirection(id);
    float3 V = N;   // 가정: 시각 = 법선 (Split-Sum 근사)
    
    float3 prefiltered = 0;
    float totalWeight = 0;
    
    const int SAMPLES = 1024;
    for (int i = 0; i < SAMPLES; ++i) {
        float2 xi = Hammersley(i, SAMPLES);
        float3 H = ImportanceSampleGGX(xi, N, roughness);
        float3 L = reflect(-V, H);
        float NdotL = saturate(dot(N, L));
        if (NdotL > 0) {
            prefiltered += g_envMap.SampleLevel(s_linear, L, 0).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    
    prefiltered /= max(totalWeight, 1e-4);
    g_prefilteredOut[id] = float4(prefiltered, 1);
}
```

각 mip level 이 다른 roughness 에 대응. 런타임엔 `roughness * mipCount` 로 샘플.

### BRDF LUT (Split-Sum 2nd Term)

Epic Games UE4 (Karis 2013) 의 핵심 트릭. Fresnel 항을 분리해서 2D LUT 에 저장.

```
∫ BRDF × (n·l) dω ≈ (F_0 × scale + bias) × prefiltered
```

2D LUT: (NdotV, roughness) → (scale, bias).

```hlsl
[numthreads(16, 16, 1)]
void CS_BRDFLUT(uint3 id : SV_DispatchThreadID)
{
    float NdotV = (id.x + 0.5) / 256.0;
    float roughness = (id.y + 0.5) / 256.0;
    
    float3 V = float3(sqrt(1 - NdotV*NdotV), 0, NdotV);
    float3 N = float3(0, 0, 1);
    
    float A = 0, B = 0;
    const int SAMPLES = 1024;
    for (int i = 0; i < SAMPLES; ++i) {
        float2 xi = Hammersley(i, SAMPLES);
        float3 H = ImportanceSampleGGX(xi, N, roughness);
        float3 L = reflect(-V, H);
        
        float NdotL = saturate(L.z);
        float NdotH = saturate(H.z);
        float VdotH = saturate(dot(V, H));
        
        if (NdotL > 0) {
            float G = G_Smith(NdotV, NdotL, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1 - VdotH, 5);
            A += (1 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    
    g_brdfLUT[id.xy] = float2(A, B) / SAMPLES;
}
```

BRDF LUT 은 씬과 무관 → 한 번만 베이크 후 영구 저장 (`BRDFLUT.dds`).

## 런타임 IBL 셰이딩

```hlsl
// Lighting/Opaque.hlsl
float3 EvaluateIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness, float ao)
{
    float NdotV = saturate(dot(N, V));
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = F_SchlickRoughness(NdotV, F0, roughness);
    
    // 1. Diffuse IBL
    float3 kD = (1 - F) * (1 - metallic);
    float3 irradiance = g_irradianceMap.SampleLevel(s_linear, N, 0).rgb;
    float3 diffuse = kD * irradiance * albedo;
    
    // 2. Specular IBL (Split-Sum)
    float3 R = reflect(-V, N);
    float mipLevel = roughness * (MAX_MIP_LEVEL - 1);
    float3 prefiltered = g_prefilteredMap.SampleLevel(s_linear, R, mipLevel).rgb;
    float2 brdf = g_brdfLUT.SampleLevel(s_linear, float2(NdotV, roughness), 0).rg;
    float3 specular = prefiltered * (F * brdf.x + brdf.y);
    
    return (diffuse + specular) * ao;
}
```

## 직접 조명 + IBL 결합

```hlsl
float3 finalColor = 0;

// Direct lights (점광/스팟/방향광)
[loop] for (int i = 0; i < g_numLights; ++i) {
    finalColor += BRDF_GGX(N, V, lights[i].dir, albedo, metallic, roughness) 
                * lights[i].color 
                * saturate(dot(N, lights[i].dir));
}

// IBL (간접 조명)
finalColor += EvaluateIBL(N, V, albedo, metallic, roughness, ao);

// 발광
finalColor += emissive;

return float4(finalColor, 1);
```

## HDR 환경 맵 로딩

```cpp
// Resource/HDR/CHDRILoader.h
unique_ptr<CTexture> LoadHDRI(const std::string& path);
// .hdr, .exr 포맷 지원 (stb_image_hdr 또는 tinyexr)

// Equirectangular → Cubemap 변환 셰이더 패스
void ConvertEquirectToCubemap(CTexture* equirect, CTextureCube* cube);
```

## IBL 베이킹 파이프라인

```
HDRI (.hdr)
   ↓ LoadHDRI
Equirectangular Texture (2048×1024)
   ↓ ConvertEquirectToCubemap
Environment Cubemap (1024×1024×6)
   ↓ CS_Irradiance         → Irradiance Map (32×32×6)
   ↓ CS_Prefilter × 5 mip  → Prefiltered Map (256×256×6 × 5)
```

베이킹 결과는 DDS 로 저장 → 다음 실행 시 로드만.

## Material Editor (ImGui)

```
[Material Inspector — Irelia_Body]
BaseColor     [texture slot] [color tint ●]
Normal        [texture slot] [strength 1.0]
MR_AO         [texture slot] [M×1.0 R×1.0 AO×1.0]
Emissive      [texture slot] [intensity 0.0]
----
[Environment]
HDRI          [environment/forest.hdr ▼]
Intensity     [──●─] 1.0
Rotation      [─●──] 0°
----
[Preview]     [Sphere ▼] [Sphere / Cube / Shader Ball]
[rendered mini-viewport 256×256]
```

실시간으로 재질 확인. "Shader Ball" 은 Mitsuba 스타일 시각화 메시.

## 성능

- IBL 베이킹: 게임 시작 시 1~3 초 (GPU 병렬)
- 런타임 샘플링: diffuse 1, specular 1, BRDF LUT 1 = 3 sampler 호출
- 메모리: Irradiance 32²×6×f16 = 12KB, Prefilter 256²×6×5×f16 = ~4MB, BRDF LUT 256²×f16 = 128KB

## 검증 방법

- **Reference**: Path Tracer (Stage 4) 에 같은 HDRI + 재질 → MSE 비교
- **Chrome Ball**: metallic=1, roughness=0 → HDRI 완전 반사 확인
- **Diffuse Sphere**: metallic=0, roughness=1 → Irradiance Map 색만 (spec 없음)
- **Mirror vs Matte**: roughness 슬라이더 변화로 반사 번짐 연속적 확인

## 구현 순서

1. `CMaterialPBR` + Assimp GLTF 로딩 PBR 재질 인식
2. HDRI 로더 (stb_image_hdr)
3. Equirect → Cubemap 변환
4. Irradiance Map 베이커
5. Prefiltered Environment Map 베이커 (mip 별 roughness)
6. BRDF LUT 베이커 (한 번만)
7. 런타임 셰이더 통합 (Split-Sum)
8. Material Editor ImGui
9. Path Tracer 와 품질 비교 (Stage 4 이후)

## 참고 문헌

- **Karis 2013** — Real Shading in Unreal Engine 4 (Split-Sum)
- **Lagarde 2014** — Moving Frostbite to PBR
- **Hammersley 1960** — Hammersley sequence (LDS)
- **Colbert & Krivánek 2007** — GPU-Based Importance Sampling
