# 02. HLSL BRDF 라이브러리 — 전문 코드

> 본 문서는 `Shaders/BRDF/*.hlsli` + `Shaders/PBR/Mesh3D_PBR.hlsl` / `Skinned3D_PBR.hlsl` 의 **전체 코드** 를 박제.
> 컨벤션: `row_major matrix` 필수, 레지스터 슬롯은 b0=PerFrame, b1=PerObject, b2=BoneMatrices, **b3=Material 신설**, **b4=DirLight 신설**.

---

## 1. 신규 파일 목록

| 파일 | 줄 수 (목표) | 역할 |
|---|---|---|
| `Shaders/BRDF/BRDFCommon.hlsli` | ~60 | PI, sRGB 변환, 유틸 |
| `Shaders/BRDF/BRDFGGX.hlsli` | ~80 | D_GGX, V_SmithGGXCorrelated, F_Schlick, Fd_Lambert |
| `Shaders/BRDF/BRDFCookTorrance.hlsli` | ~50 | EvaluateBRDF 통합 함수 |
| `Shaders/BRDF/BRDFLighting.hlsli` | ~70 | ApplyDirLight, ApplyPointLight |
| `Shaders/PBR/Mesh3D_PBR.hlsl` | ~120 | 정적 메시 PBR 셰이더 |
| `Shaders/PBR/Skinned3D_PBR.hlsl` | ~140 | 스키닝 메시 PBR 셰이더 |
| `Shaders/PBR/DepthPrepass.hlsl` | ~50 | Forward+ 사전 depth |

---

## 2. `Shaders/BRDF/BRDFCommon.hlsli`

```hlsl
// =========================================================
//  BRDFCommon.hlsli — 상수, sRGB 변환, 유틸
//  Phase E Stage 1 — 모든 PBR 셰이더가 include 하는 공용 헤더
// =========================================================

#ifndef BRDF_COMMON_HLSLI
#define BRDF_COMMON_HLSLI

static const float PI         = 3.14159265358979323846f;
static const float TWO_PI     = 6.28318530717958647692f;
static const float INV_PI     = 0.31830988618379067154f;
static const float MIN_DOT    = 1e-4f;     // dot 결과 0 division 방지

// ─── sRGB ↔ Linear ───────────────────────────────────────
//  엄밀한 변환은 분기가 있지만 PS 에선 power 1/2.2 근사로 충분
float3 SRGBtoLinear(float3 c) { return pow(c, 2.2f); }
float3 LinearToSRGB(float3 c) { return pow(c, 1.0f / 2.2f); }

// 안전한 dot: NaN/0div 방지
float SafeDot(float3 a, float3 b)
{
    return max(dot(a, b), MIN_DOT);
}

// 휘도 (luminance) — Rec. 709 가중
float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

#endif // BRDF_COMMON_HLSLI
```

---

## 3. `Shaders/BRDF/BRDFGGX.hlsli`

```hlsl
// =========================================================
//  BRDFGGX.hlsli — Microfacet BRDF 컴포넌트 함수
//  D: GGX (Trowbridge-Reitz)
//  V: Smith Height-Correlated (Visibility = G / (4·NdotL·NdotV))
//  F: Schlick approximation
//  Fd: Lambert (선택: Disney Diffuse)
// =========================================================

#ifndef BRDF_GGX_HLSLI
#define BRDF_GGX_HLSLI

#include "BRDFCommon.hlsli"

// ─── D: Normal Distribution Function (GGX) ───────────────
//  α = roughness² (Disney perceptual)
float D_GGX(float NdotH, float alpha)
{
    float a2  = alpha * alpha;
    float NoH2 = NdotH * NdotH;
    float denom = NoH2 * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

// ─── V: Smith Height-Correlated Visibility ───────────────
//  Heitz 2014, Filament 표기. 분모의 4·NdotL·NdotV 흡수.
float V_SmithGGXCorrelated(float NdotL, float NdotV, float alpha)
{
    float a2 = alpha * alpha;
    float GGXV = NdotL * sqrt(NdotV * NdotV * (1.0f - a2) + a2);
    float GGXL = NdotV * sqrt(NdotL * NdotL * (1.0f - a2) + a2);
    return 0.5f / max(GGXV + GGXL, MIN_DOT);
}

// ─── F: Fresnel-Schlick ──────────────────────────────────
float3 F_Schlick(float3 F0, float HdotV)
{
    float f = pow(1.0f - HdotV, 5.0f);
    return F0 + (1.0f - F0) * f;
}

// Roughness-aware Schlick (IBL 용, Stage 7) — Lazarov
float3 F_SchlickRoughness(float3 F0, float NdotV, float roughness)
{
    float r1 = 1.0f - roughness;
    return F0 + (max(float3(r1, r1, r1), F0) - F0) * pow(1.0f - NdotV, 5.0f);
}

// ─── Fd: Lambert ─────────────────────────────────────────
float3 Fd_Lambert(float3 albedo)
{
    return albedo * INV_PI;
}

// ─── Fd: Disney Diffuse (옵션) ───────────────────────────
//  retro-reflective rim — 약간 비쌈, Stage 1 에선 미사용.
float3 Fd_Burley(float3 albedo, float NdotV, float NdotL, float LdotH, float roughness)
{
    float fd90 = 0.5f + 2.0f * roughness * LdotH * LdotH;
    float lightScatter = 1.0f + (fd90 - 1.0f) * pow(1.0f - NdotL, 5.0f);
    float viewScatter  = 1.0f + (fd90 - 1.0f) * pow(1.0f - NdotV, 5.0f);
    return albedo * INV_PI * lightScatter * viewScatter;
}

#endif // BRDF_GGX_HLSLI
```

---

## 4. `Shaders/BRDF/BRDFCookTorrance.hlsli`

```hlsl
// =========================================================
//  BRDFCookTorrance.hlsli — D·V·F + Diffuse 결합
//  광원 1개에 대한 라디언스 기여 평가.
// =========================================================

#ifndef BRDF_COOK_TORRANCE_HLSLI
#define BRDF_COOK_TORRANCE_HLSLI

#include "BRDFGGX.hlsli"

// 한 광원에 대한 BRDF 평가. 출력은 cosine-weighted (NdotL 곱한 결과).
// 호출측이 lightColor · attenuation 만 별도로 곱하면 됨.
float3 EvaluateBRDF(
    float3 albedo,
    float  metallic,
    float  roughness,
    float3 N,
    float3 V,
    float3 L)
{
    float3 H = normalize(V + L);

    float NdotL = saturate(dot(N, L));
    if (NdotL <= MIN_DOT) return float3(0, 0, 0);

    float NdotV = saturate(dot(N, V));
    float NdotH = saturate(dot(N, H));
    float HdotV = saturate(dot(H, V));
    float LdotH = saturate(dot(L, H));

    // alpha = roughness² (Disney)
    float alpha = max(roughness * roughness, 0.0045f);   // 너무 작으면 specular spike

    // F0: dielectric=0.04, metal=albedo
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    // Microfacet specular = D · V · F  (V 가 4·NdotL·NdotV 분모 포함)
    float  D = D_GGX(NdotH, alpha);
    float  V_t = V_SmithGGXCorrelated(NdotL, NdotV, alpha);
    float3 F = F_Schlick(F0, HdotV);
    float3 specular = D * V_t * F;

    // Diffuse — energy conservation (kd = 1-F, metal 은 0)
    float3 kd = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = kd * Fd_Lambert(albedo);

    // 최종 = (diffuse + specular) · cosθ_i
    return (diffuse + specular) * NdotL;
}

#endif // BRDF_COOK_TORRANCE_HLSLI
```

---

## 5. `Shaders/BRDF/BRDFLighting.hlsli`

```hlsl
// =========================================================
//  BRDFLighting.hlsli — 광원 종류별 호출 래퍼
//  Stage 4 단일 directional → Stage 5 Forward+ point/spot
// =========================================================

#ifndef BRDF_LIGHTING_HLSLI
#define BRDF_LIGHTING_HLSLI

#include "BRDFCookTorrance.hlsli"

// ─── 광원 POD (HLSL ↔ C++ 공유) ──────────────────────────
struct DirectionalLight
{
    float3 vDirection;       // toward surface (light → surface)
    float  fIntensity;
    float3 vColor;
    float  _pad;
};

struct PointLight
{
    float3 vPosition;
    float  fRadius;          // 영향 반경 (attenuation 0 지점)
    float3 vColor;
    float  fIntensity;
};

// ─── Directional Light ───────────────────────────────────
float3 ApplyDirLight(
    DirectionalLight light,
    float3 albedo, float metallic, float roughness,
    float3 N, float3 V, float3 worldPos)
{
    float3 L = -normalize(light.vDirection);             // surface → light
    float3 brdf = EvaluateBRDF(albedo, metallic, roughness, N, V, L);
    return brdf * light.vColor * light.fIntensity;
}

// ─── Point Light (Forward+) ──────────────────────────────
//  Inverse-square + smooth radius cutoff (Frostbite, Filament 패턴)
float SmoothDistanceAttenuation(float distSq, float invRadiusSq)
{
    float factor = distSq * invRadiusSq;
    float smoothFactor = saturate(1.0f - factor * factor);
    return smoothFactor * smoothFactor;
}

float3 ApplyPointLight(
    PointLight light,
    float3 albedo, float metallic, float roughness,
    float3 N, float3 V, float3 worldPos)
{
    float3 toLight = light.vPosition - worldPos;
    float  distSq  = dot(toLight, toLight);
    float  invRadSq = 1.0f / max(light.fRadius * light.fRadius, MIN_DOT);

    float att = SmoothDistanceAttenuation(distSq, invRadSq) / max(distSq, 0.01f * 0.01f);
    if (att <= 0) return float3(0, 0, 0);

    float3 L = toLight * rsqrt(distSq);
    float3 brdf = EvaluateBRDF(albedo, metallic, roughness, N, V, L);
    return brdf * light.vColor * light.fIntensity * att;
}

#endif // BRDF_LIGHTING_HLSLI
```

---

## 6. `Shaders/PBR/Mesh3D_PBR.hlsl`

```hlsl
// =========================================================
//  Mesh3D_PBR.hlsl — 정적 메시용 PBR 셰이더
//  기존 Mesh3D.hlsl 과 동일 InputLayout (PosNormTexTangent).
//  cbuffer 슬롯:
//    b0 = PerFrame   (matViewProj, cameraPos)
//    b1 = PerObject  (matWorld)
//    b3 = Material   (CBMaterial)
//    b4 = DirLight   (CBDirLight)  — Stage 4
//
//  Stage 5 진입 시 t10 = StructuredBuffer<PointLight>, t11 = LightGrid 추가.
// =========================================================

#include "../BRDF/BRDFLighting.hlsli"

// ── Constant Buffers ────────────────────────────────────
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3           g_vCameraPos;
    float            g_fTime;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvT;       // normal 변환용
};

cbuffer CBMaterial : register(b3)
{
    float3 g_vAlbedo;
    float  g_fMetallic;
    float3 g_vEmissive;
    float  g_fRoughness;
    float  g_fAO;
    float  g_fReflectance;
    uint   g_uMaterialFlags;
    float  _pad0;
};

cbuffer CBDirLight : register(b4)
{
    DirectionalLight g_DirLight;
};

// ── Textures ────────────────────────────────────────────
Texture2D    g_AlbedoMap    : register(t0);
Texture2D    g_NormalMap    : register(t1);
Texture2D    g_MetalRoughMap: register(t2);     // R=Metallic, G=Roughness (glTF 표준)
Texture2D    g_AOMap        : register(t3);
Texture2D    g_EmissiveMap  : register(t4);
SamplerState g_Sampler      : register(s0);

// ── Vertex I/O ──────────────────────────────────────────
struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal   : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent  : TANGENT;
};

struct PS_INPUT
{
    float4 vPosition  : SV_POSITION;
    float3 vWorldPos  : TEXCOORD0;
    float3 vNormal    : NORMAL;
    float3 vTangent   : TANGENT;
    float3 vBitangent : BINORMAL;
    float2 vTexCoord  : TEXCOORD1;
};

// ── Vertex Shader ───────────────────────────────────────
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT o;
    float4 worldPos = mul(float4(input.vPosition, 1.f), g_matWorld);
    o.vPosition  = mul(worldPos, g_matViewProj);
    o.vWorldPos  = worldPos.xyz;
    o.vNormal    = normalize(mul(input.vNormal,  (float3x3)g_matWorldInvT));
    o.vTangent   = normalize(mul(input.vTangent, (float3x3)g_matWorld));
    o.vBitangent = normalize(cross(o.vNormal, o.vTangent));
    o.vTexCoord  = input.vTexCoord;
    return o;
}

// ── Pixel Shader ────────────────────────────────────────
float4 PS(PS_INPUT input) : SV_TARGET
{
    // 1. 텍스처 샘플
    float4 albedoTex = g_AlbedoMap.Sample(g_Sampler, input.vTexCoord);
    clip(albedoTex.a - 0.05f);                                     // 기존 cutout 유지

    float3 albedo    = SRGBtoLinear(albedoTex.rgb) * g_vAlbedo;
    float2 mrTex     = g_MetalRoughMap.Sample(g_Sampler, input.vTexCoord).rg;
    float  metallic  = mrTex.r * g_fMetallic;
    float  roughness = max(mrTex.g * g_fRoughness, 0.04f);
    float  ao        = g_AOMap.Sample(g_Sampler, input.vTexCoord).r * g_fAO;
    float3 emissive  = SRGBtoLinear(g_EmissiveMap.Sample(g_Sampler, input.vTexCoord).rgb) * g_vEmissive;

    // 2. Normal mapping (tangent space → world)
    float3 N_tex = g_NormalMap.Sample(g_Sampler, input.vTexCoord).rgb * 2.0f - 1.0f;
    float3x3 TBN = float3x3(input.vTangent, input.vBitangent, input.vNormal);
    float3 N = normalize(mul(N_tex, TBN));

    float3 V = normalize(g_vCameraPos - input.vWorldPos);

    // 3. Direct lighting (Stage 4 단일 directional)
    float3 Lo = ApplyDirLight(g_DirLight, albedo, metallic, roughness, N, V, input.vWorldPos);

    // 4. Stage 5 진입 시 Forward+ point light loop 추가 (다음 문서)
    //    [unroll] for (uint i = 0; i < tileLightCount; ++i) { Lo += ApplyPointLight(...); }

    // 5. Ambient (IBL 도입 전 임시)
    float3 ambient = albedo * 0.03f * ao;

    float3 color = ambient + Lo + emissive;
    return float4(LinearToSRGB(color), albedoTex.a);
}
```

---

## 7. `Shaders/PBR/Skinned3D_PBR.hlsl`

```hlsl
// =========================================================
//  Skinned3D_PBR.hlsl — 스키닝 메시용 PBR
//  Mesh3D_PBR 와 동일하나 VS 에 본 변환 추가.
//  cbuffer b2 = BoneMatrices (기존 슬롯 그대로).
// =========================================================

#include "../BRDF/BRDFLighting.hlsli"

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3           g_vCameraPos;
    float            g_fTime;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvT;
};

cbuffer CBBones : register(b2)
{
    row_major matrix g_BoneMatrices[256];
};

cbuffer CBMaterial : register(b3)
{
    float3 g_vAlbedo;
    float  g_fMetallic;
    float3 g_vEmissive;
    float  g_fRoughness;
    float  g_fAO;
    float  g_fReflectance;
    uint   g_uMaterialFlags;
    float  _pad0;
};

cbuffer CBDirLight : register(b4)
{
    DirectionalLight g_DirLight;
};

Texture2D    g_AlbedoMap    : register(t0);
Texture2D    g_NormalMap    : register(t1);
Texture2D    g_MetalRoughMap: register(t2);
Texture2D    g_AOMap        : register(t3);
Texture2D    g_EmissiveMap  : register(t4);
SamplerState g_Sampler      : register(s0);

struct VS_INPUT
{
    float3 vPosition  : POSITION;
    float3 vNormal    : NORMAL;
    float2 vTexCoord  : TEXCOORD0;
    float3 vTangent   : TANGENT;
    uint4  iBoneIdx   : BLENDINDICES;
    float4 fBoneWgt   : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 vPosition  : SV_POSITION;
    float3 vWorldPos  : TEXCOORD0;
    float3 vNormal    : NORMAL;
    float3 vTangent   : TANGENT;
    float3 vBitangent : BINORMAL;
    float2 vTexCoord  : TEXCOORD1;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT o;

    // 1. 본 가중치 행렬 합산
    matrix skin =
        g_BoneMatrices[input.iBoneIdx.x] * input.fBoneWgt.x +
        g_BoneMatrices[input.iBoneIdx.y] * input.fBoneWgt.y +
        g_BoneMatrices[input.iBoneIdx.z] * input.fBoneWgt.z +
        g_BoneMatrices[input.iBoneIdx.w] * input.fBoneWgt.w;

    // 2. 위치
    float4 skinned  = mul(float4(input.vPosition, 1.f), skin);
    float4 worldPos = mul(skinned, g_matWorld);
    o.vPosition = mul(worldPos, g_matViewProj);
    o.vWorldPos = worldPos.xyz;

    // 3. Normal/Tangent (skin 의 회전 부분만 적용 — 비균등 스케일 가정 X)
    float3 nLocal = mul(input.vNormal,  (float3x3)skin);
    float3 tLocal = mul(input.vTangent, (float3x3)skin);
    o.vNormal    = normalize(mul(nLocal, (float3x3)g_matWorldInvT));
    o.vTangent   = normalize(mul(tLocal, (float3x3)g_matWorld));
    o.vBitangent = normalize(cross(o.vNormal, o.vTangent));
    o.vTexCoord  = input.vTexCoord;
    return o;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float4 albedoTex = g_AlbedoMap.Sample(g_Sampler, input.vTexCoord);
    clip(albedoTex.a - 0.05f);

    float3 albedo    = SRGBtoLinear(albedoTex.rgb) * g_vAlbedo;
    float2 mrTex     = g_MetalRoughMap.Sample(g_Sampler, input.vTexCoord).rg;
    float  metallic  = mrTex.r * g_fMetallic;
    float  roughness = max(mrTex.g * g_fRoughness, 0.04f);
    float  ao        = g_AOMap.Sample(g_Sampler, input.vTexCoord).r * g_fAO;
    float3 emissive  = SRGBtoLinear(g_EmissiveMap.Sample(g_Sampler, input.vTexCoord).rgb) * g_vEmissive;

    float3 N_tex = g_NormalMap.Sample(g_Sampler, input.vTexCoord).rgb * 2.0f - 1.0f;
    float3x3 TBN = float3x3(input.vTangent, input.vBitangent, input.vNormal);
    float3 N = normalize(mul(N_tex, TBN));

    float3 V = normalize(g_vCameraPos - input.vWorldPos);

    float3 Lo = ApplyDirLight(g_DirLight, albedo, metallic, roughness, N, V, input.vWorldPos);

    // Stage 5 진입 시 Forward+ 추가
    float3 ambient = albedo * 0.03f * ao;

    float3 color = ambient + Lo + emissive;
    return float4(LinearToSRGB(color), albedoTex.a);
}
```

---

## 8. `Shaders/PBR/DepthPrepass.hlsl` (Forward+ 의 전제)

```hlsl
// =========================================================
//  DepthPrepass.hlsl — depth 만 출력, color write off.
//  Forward+ Light Cull CS 가 사용할 depth buffer 채움.
//  Mesh3D_PBR 과 동일 VS, PS 는 빈 함수.
// =========================================================

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3           g_vCameraPos;
    float            g_fTime;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvT;
};

struct VS_INPUT
{
    float3 vPosition : POSITION;
    float3 vNormal   : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vTangent  : TANGENT;
};

struct PS_INPUT
{
    float4 vPosition : SV_POSITION;
    float2 vTexCoord : TEXCOORD0;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT o;
    float4 wp = mul(float4(input.vPosition, 1), g_matWorld);
    o.vPosition = mul(wp, g_matViewProj);
    o.vTexCoord = input.vTexCoord;
    return o;
}

// alpha cutout 만 처리, 색은 안 씀
Texture2D    g_AlbedoMap : register(t0);
SamplerState g_Sampler   : register(s0);

void PS(PS_INPUT input)
{
    float a = g_AlbedoMap.Sample(g_Sampler, input.vTexCoord).a;
    clip(a - 0.05f);
    // SV_TARGET 출력 없음 → DSV 만 갱신
}
```

---

## 9. 컴파일 검증 — 단독으로 빌드되는지

각 `.hlsl` 파일을 fxc 로 컴파일해 syntax 우선 검증:

```bat
:: tools/check_pbr_shaders.bat
fxc /T vs_5_0 /E VS Shaders/PBR/Mesh3D_PBR.hlsl /Fo nul
fxc /T ps_5_0 /E PS Shaders/PBR/Mesh3D_PBR.hlsl /Fo nul
fxc /T vs_5_0 /E VS Shaders/PBR/Skinned3D_PBR.hlsl /Fo nul
fxc /T ps_5_0 /E PS Shaders/PBR/Skinned3D_PBR.hlsl /Fo nul
fxc /T vs_5_0 /E VS Shaders/PBR/DepthPrepass.hlsl /Fo nul
fxc /T ps_5_0 /E PS Shaders/PBR/DepthPrepass.hlsl /Fo nul
```

전부 통과 = Stage 1+2 셰이더 코드 OK.

---

## 10. 셰이더 파일 OutDir 동기화 주의 (CLAUDE.md 박제 함정)

> **셰이더 파일은 MSBuild incremental 이 변경 감지 못 해 PostBuild xcopy 가 skip.** 신규 .hlsl/.hlsli 파일 추가 후 반드시:
>
> ```bat
> xcopy /Y /S Shaders\BRDF\*.hlsli Client\Bin\Debug\Shaders\BRDF\
> xcopy /Y /S Shaders\PBR\*.hlsl   Client\Bin\Debug\Shaders\PBR\
> ```
>
> 또는 `Client.vcxproj` PostBuild xcopy 의 `/D` 를 `/Y` 로 강제 갱신. CLAUDE.md "OutDir 강제 동기화" 함정 참조.

---

## 다음 문서

→ `03_CPP_API_AND_CBUFFERS.md` — 위 셰이더가 받을 cbuffer 데이터를 채우는 C++ 측 (`CPBRMaterial`, `CDirectionalLight`, `ModelRenderer` 확장).

## 검증 체크리스트 (이 단계 완료 조건)

- [ ] 7개 신규 셰이더 파일 모두 fxc 통과.
- [ ] `Mesh3D_PBR.hlsl` 의 cbuffer slot 이 b0/b1/b3/b4 (b2 는 스키닝 전용으로 비워둠).
- [ ] `BRDFCookTorrance.hlsli::EvaluateBRDF` 가 NdotL 외부 곱을 포함 (호출측에서 다시 곱하지 않음).
- [ ] `V_SmithGGXCorrelated` 가 G 가 아니라 Visibility 임을 인지 (분모 4·NdotL·NdotV 흡수).
- [ ] `clip(albedoTex.a - 0.05f)` 유지 (기존 cutout 호환).
- [ ] sRGB ↔ Linear 변환이 PS 입력/출력 양쪽에 박혀있음.
