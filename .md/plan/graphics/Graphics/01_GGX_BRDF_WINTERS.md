# 01. GGX BRDF — Winters Engine 이식 가이드

> 원전: `C:\Users\user\Desktop\.markdown\Graphics\01_GGX_BRDF_Complete.md`
> 대상 경로: `Engine/Public/Renderer/BRDF/`, `Engine/Private/Renderer/BRDF/`, `Shaders/BRDF/`
> Phase: **E Stage 1 (BRDF)** — 현재 Mesh3D/Skinned3D는 unlit. 이 문서가 첫 lit 파이프라인.

---

## 0. 이 문서의 목표 (Winters 관점)

원전의 Cook-Torrance microfacet BRDF + GGX 이론을 **Winters 엔진에 실제로 컴파일되는 형태**로:

1. **기존 unlit 파이프라인 분기** — `Mesh3D.hlsl` / `Skinned3D.hlsl` 은 건드리지 않고 `Shaders/BRDF/` 하위에 PBR 전용 셰이더 신설. ModelRenderer 는 셰이더 경로 매개변수로 선택.
2. **cbuffer slot 규약 유지** — 기존 b0=PerFrame, b1=PerObject, b2=BoneMatrices 를 확장만 함. 새 MaterialGPU 는 b3.
3. **WINTERS_ENGINE DLL 경계 준수** — 공개 C++ API 는 `CMaterial`, `CBRDFShaderCache` 만. 나머지는 `.cpp` 내 static.
4. **flat include 강제** — 공개 헤더는 `#include "BRDFTypes.h"` (서브폴더 금지, EngineSDK flat 복사 대비).
5. **타입 별칭 준수** — `f32_t`/`u32_t`/`bool_t`. raw `float`/`int` 금지.

---

## 1. 디렉토리 신설

```
Engine/Public/Renderer/BRDF/
├── BRDFTypes.h            // MaterialGPU, CBPerLight, CBMaterial (C++ ↔ HLSL 공유)
├── BRDFMaterial.h         // class CBRDFMaterial (WINTERS_ENGINE)
├── BRDFShaderCache.h      // class CBRDFShaderCache — Mesh3D_PBR.hlsl permutations
└── FurnaceTest.h          // class CFurnaceTestHarness — 디버그용

Engine/Private/Renderer/BRDF/
├── BRDFMaterial.cpp
├── BRDFShaderCache.cpp
└── FurnaceTest.cpp

Shaders/BRDF/
├── BRDFCommon.hlsli       // PI, Luminance, to/from sRGB
├── BRDFGGX.hlsli          // D_GGX / V_SmithGGXCorrelated / F_Schlick / Fd_Lambert / Fd_Burley
├── BRDFIS.hlsli           // ImportanceSampleGGX + Hammersley
├── Mesh3D_PBR.hlsl        // 정적 메시 PBR
└── Skinned3D_PBR.hlsl     // 스키닝 메시 PBR

Shaders/Tests/
└── FurnaceTest.hlsl       // White furnace test (compute)
```

> EngineSDK flat 복사 때문에 `Engine/Public/Renderer/BRDF/*.h` 는 전부 파일명만으로 include 가능해야 함. `Engine.vcxproj` `AdditionalIncludeDirectories` 에 `Engine/Public/Renderer/BRDF` 를 추가해 Engine 내부 TU 도 flat 해소.

---

## 2. C++ 공개 타입 — `BRDFTypes.h`

```cpp
// Engine/Public/Renderer/BRDF/BRDFTypes.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <DirectXMath.h>

// HLSL 과 공유되는 POD. 16바이트 정렬 필수 (DX11 cbuffer 제약).
// HLSL 쪽에서는 같은 레이아웃을 직접 적어둔다 (SharedShaderTypes 도입은 Phase 2).

struct CBMaterial
{
    DirectX::XMFLOAT3 vAlbedo;
    f32_t             fMetallic;

    DirectX::XMFLOAT3 vEmissive;
    f32_t             fRoughness;

    f32_t             fAO;
    f32_t             fReflectance;     // 비금속 F0 (기본 0.04)
    u32_t             uFlags;           // bit0: useDisneyDiffuse, bit1: useMSComp
    f32_t             _pad0;
};
static_assert(sizeof(CBMaterial) % 16 == 0, "CBMaterial must be 16-byte aligned");

// 씬에서 1개 (directional sun) + N 개 (point/spot) 확장은 Clustered 도입 후.
struct CBDirLight
{
    DirectX::XMFLOAT3 vDirection;       // normalized, toward surface (from-light)
    f32_t             fIntensity;
    DirectX::XMFLOAT3 vColor;
    f32_t             _pad0;
};
static_assert(sizeof(CBDirLight) % 16 == 0);

struct CBFrameCamera
{
    DirectX::XMFLOAT4X4 matViewProj;    // jittered (TAA 도입 후)
    DirectX::XMFLOAT4X4 matViewProjNoJitter;
    DirectX::XMFLOAT3   vCameraPos;     // world
    f32_t               fTime;
};
static_assert(sizeof(CBFrameCamera) % 16 == 0);

enum class eBRDFFlag : u32_t
{
    UseDisneyDiffuse   = 1u << 0,
    UseMSCompensation  = 1u << 1,
    UseAnisotropic     = 1u << 2,       // 장기: 머리카락/브러시드 메탈
};
```

> `DX11ConstantBuffer<CBMaterial>` 로 바로 사용 (`DX11ConstantBuffer.h` 템플릿 재사용).

---

## 3. 공개 재질 클래스 — `CBRDFMaterial`

```cpp
// Engine/Public/Renderer/BRDF/BRDFMaterial.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "BRDFTypes.h"
#include <memory>
#include <string>

class CTexture;

class WINTERS_ENGINE CBRDFMaterial
{
public:
    ~CBRDFMaterial();

    // 팩토리 — private ctor, unique_ptr 소유권
    static std::unique_ptr<CBRDFMaterial> Create(const std::string& strName);

    // copy 금지 / move 허용 — unique_ptr 멤버 + WINTERS_ENGINE dllexport 조합 회피 (CLAUDE.md gotcha)
    CBRDFMaterial(const CBRDFMaterial&)            = delete;
    CBRDFMaterial& operator=(const CBRDFMaterial&) = delete;
    CBRDFMaterial(CBRDFMaterial&&)                 = default;
    CBRDFMaterial& operator=(CBRDFMaterial&&)      = default;

    // Setters — Dirty flag 패턴
    void SetAlbedo(const DirectX::XMFLOAT3& v);
    void SetMetallic(f32_t fMetallic);
    void SetRoughness(f32_t fRoughness);
    void SetEmissive(const DirectX::XMFLOAT3& v);
    void SetReflectance(f32_t f);      // 비금속 F0. 기본 0.04
    void SetFlag(eBRDFFlag flag, bool_t bOn);

    // Texture slots — t0 Albedo, t1 NormalMap, t2 MetallicRoughnessAO, t3 Emissive
    void SetAlbedoMap(CTexture* pTex);
    void SetNormalMap(CTexture* pTex);
    void SetMetallicRoughnessAOMap(CTexture* pTex);
    void SetEmissiveMap(CTexture* pTex);

    // GPU 전송용 POD 변환
    const CBMaterial& GetGPUData() const;

    // 셰이더 순열 키 (BRDFShaderCache 가 사용)
    u32_t GetShaderPermutationID() const;

private:
    CBRDFMaterial() = default;

    struct Impl;
    std::unique_ptr<Impl> m_pImpl;    // copy 가 필요하다고 MSVC 가 오해하므로 명시적 delete 필수
};
```

```cpp
// Engine/Private/Renderer/BRDF/BRDFMaterial.cpp
#include "BRDFMaterial.h"
#include "BRDFTypes.h"

// Texture 전방선언 아닌 실제 사용은 internal — raw pointer 만 보유
class CTexture;

struct CBRDFMaterial::Impl
{
    std::string     m_strName;
    CBMaterial      m_GPU{};
    CTexture*       m_pAlbedo             = nullptr;
    CTexture*       m_pNormal             = nullptr;
    CTexture*       m_pMetallicRoughnessAO = nullptr;
    CTexture*       m_pEmissive           = nullptr;
    u32_t           m_uPermutationID      = 0;
    bool_t          m_bDirty              = true;
};

CBRDFMaterial::~CBRDFMaterial() = default;

std::unique_ptr<CBRDFMaterial> CBRDFMaterial::Create(const std::string& strName)
{
    auto p = std::unique_ptr<CBRDFMaterial>(new CBRDFMaterial());
    p->m_pImpl            = std::make_unique<Impl>();
    p->m_pImpl->m_strName = strName;

    // 안전한 기본값 (White furnace test 기본 상태)
    p->m_pImpl->m_GPU.vAlbedo     = { 1.f, 1.f, 1.f };
    p->m_pImpl->m_GPU.vEmissive   = { 0.f, 0.f, 0.f };
    p->m_pImpl->m_GPU.fMetallic   = 0.f;
    p->m_pImpl->m_GPU.fRoughness  = 0.5f;
    p->m_pImpl->m_GPU.fAO          = 1.f;
    p->m_pImpl->m_GPU.fReflectance = 0.04f;
    p->m_pImpl->m_GPU.uFlags       = 0;
    return p;
}

void CBRDFMaterial::SetAlbedo(const DirectX::XMFLOAT3& v)
{
    m_pImpl->m_GPU.vAlbedo = v;
    m_pImpl->m_bDirty      = true;
}

void CBRDFMaterial::SetMetallic(f32_t fMetallic)
{
    m_pImpl->m_GPU.fMetallic = std::clamp(fMetallic, 0.f, 1.f);
    m_pImpl->m_bDirty        = true;
}

void CBRDFMaterial::SetRoughness(f32_t fRoughness)
{
    // MIN_ROUGH = 0.045. shader 에서 max(r, MIN_ROUGH) 다시 보호.
    m_pImpl->m_GPU.fRoughness = std::clamp(fRoughness, 0.045f, 1.f);
    m_pImpl->m_bDirty         = true;
}

void CBRDFMaterial::SetEmissive(const DirectX::XMFLOAT3& v)
{
    m_pImpl->m_GPU.vEmissive = v;
    m_pImpl->m_bDirty        = true;
}

void CBRDFMaterial::SetReflectance(f32_t f)
{
    m_pImpl->m_GPU.fReflectance = std::clamp(f, 0.f, 0.1f);
    m_pImpl->m_bDirty           = true;
}

void CBRDFMaterial::SetFlag(eBRDFFlag flag, bool_t bOn)
{
    if (bOn) m_pImpl->m_GPU.uFlags |=  static_cast<u32_t>(flag);
    else     m_pImpl->m_GPU.uFlags &= ~static_cast<u32_t>(flag);
    m_pImpl->m_bDirty = true;
    // permutation 에도 영향 → 재계산
    m_pImpl->m_uPermutationID = m_pImpl->m_GPU.uFlags;
}

const CBMaterial& CBRDFMaterial::GetGPUData() const      { return m_pImpl->m_GPU; }
u32_t             CBRDFMaterial::GetShaderPermutationID() const { return m_pImpl->m_uPermutationID; }
```

---

## 4. HLSL — `Shaders/BRDF/BRDFCommon.hlsli`

```hlsl
// Shaders/BRDF/BRDFCommon.hlsli
#ifndef BRDF_COMMON_HLSLI
#define BRDF_COMMON_HLSLI

static const float PI         = 3.14159265359f;
static const float INV_PI     = 0.31830988618f;
static const float MIN_ROUGH  = 0.045f;
static const float TWO_PI     = 6.28318530718f;

float Luminance(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 LinearToSRGB(float3 c)
{
    // 정확한 sRGB는 분기 포함. 근사 2.2 감마.
    return pow(max(c, 0.0f), 1.0f / 2.2f);
}

float3 SRGBToLinear(float3 c)
{
    return pow(max(c, 0.0f), 2.2f);
}

#endif
```

---

## 5. HLSL — `Shaders/BRDF/BRDFGGX.hlsli`

```hlsl
// Shaders/BRDF/BRDFGGX.hlsli
#ifndef BRDF_GGX_HLSLI
#define BRDF_GGX_HLSLI
#include "BRDFCommon.hlsli"

//── D: Normal Distribution Function (GGX / Trowbridge-Reitz) ─────────
float D_GGX(float NoH, float roughness)
{
    float a  = roughness * roughness;  // α = roughness² (Disney)
    float a2 = a * a;
    float d  = NoH * NoH * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d);
}

//── V: Visibility = G / (4·NoV·NoL) — Height-Correlated Smith GGX ────
float V_SmithGGXCorrelated(float NoV, float NoL, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float GV = NoL * sqrt(NoV * NoV * (1.0f - a2) + a2);
    float GL = NoV * sqrt(NoL * NoL * (1.0f - a2) + a2);
    return 0.5f / (GV + GL + 1e-5f);
}

// Hammon 2017 근사 — 모바일/저성능
float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness)
{
    float a  = roughness;
    float GV = NoL * (NoV * (1.0f - a) + a);
    float GL = NoV * (NoL * (1.0f - a) + a);
    return 0.5f / (GV + GL + 1e-5f);
}

//── F: Schlick Fresnel ───────────────────────────────────────────────
float3 F_Schlick(float VoH, float3 F0)
{
    float f = pow(1.0f - VoH, 5.0f);
    return F0 + (1.0f - F0) * f;
}

// IBL grazing 보정용
float3 F_SchlickRoughness(float NoV, float3 F0, float roughness)
{
    float f = pow(1.0f - NoV, 5.0f);
    return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * f;
}

//── Diffuse ──────────────────────────────────────────────────────────
float3 Fd_Lambert(float3 albedo)
{
    return albedo * INV_PI;
}

// Disney Diffuse (Burley 2012) — 옵션
float3 Fd_Burley(float NoV, float NoL, float LoH, float roughness, float3 albedo)
{
    float F90 = 0.5f + 2.0f * roughness * LoH * LoH;
    float Ls  = 1.0f + (F90 - 1.0f) * pow(1.0f - NoL, 5.0f);
    float Vs  = 1.0f + (F90 - 1.0f) * pow(1.0f - NoV, 5.0f);
    return albedo * INV_PI * Ls * Vs;
}

//── Master BRDF ──────────────────────────────────────────────────────
struct SurfaceData
{
    float3 albedo;
    float3 normal;    // world, normalized
    float  metallic;
    float  roughness;
    float3 position;  // world
};

float3 EvaluateBRDF(SurfaceData s, float3 V, float3 L, float3 lightColor,
                    float reflectance, bool useDisney)
{
    float roughness = max(s.roughness, MIN_ROUGH);
    float3 H = normalize(V + L);

    float NoV = abs(dot(s.normal, V)) + 1e-5f;
    float NoL = saturate(dot(s.normal, L));
    float NoH = saturate(dot(s.normal, H));
    float VoH = saturate(dot(V, H));
    float LoH = saturate(dot(L, H));

    if (NoL <= 0.0f)
        return float3(0.0f, 0.0f, 0.0f);

    float3 F0_base = float3(reflectance, reflectance, reflectance);
    float3 F0 = lerp(F0_base, s.albedo, s.metallic);

    float  D  = D_GGX(NoH, roughness);
    float  Vs = V_SmithGGXCorrelated(NoV, NoL, roughness);
    float3 F  = F_Schlick(VoH, F0);
    float3 Fr = D * Vs * F;

    float3 kd = (1.0f - F) * (1.0f - s.metallic);
    float3 Fd = useDisney
              ? kd * Fd_Burley(NoV, NoL, LoH, roughness, s.albedo)
              : kd * Fd_Lambert(s.albedo);

    return (Fd + Fr) * lightColor * NoL;
}

#endif
```

---

## 6. HLSL — `Shaders/BRDF/BRDFIS.hlsli` (Importance Sampling)

```hlsl
// Shaders/BRDF/BRDFIS.hlsli
#ifndef BRDF_IS_HLSLI
#define BRDF_IS_HLSLI
#include "BRDFCommon.hlsli"

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi      = TWO_PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));

    float3 H_t = float3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    float3 up      = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangent = normalize(cross(up, N));
    float3 bitan   = cross(N, tangent);

    return normalize(tangent * H_t.x + bitan * H_t.y + N * H_t.z);
}

#endif
```

---

## 7. 실전 셰이더 — `Shaders/BRDF/Mesh3D_PBR.hlsl`

```hlsl
// Shaders/BRDF/Mesh3D_PBR.hlsl
#include "BRDFGGX.hlsli"

// Winters cbuffer 규약: b0=PerFrame, b1=PerObject, b2=(예약, 스키닝), b3=Material, b4=DirLight
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    row_major matrix g_matViewProjNoJitter;     // TAA 도입 시 사용
    float3           g_vCameraPos;
    float            g_fTime;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

cbuffer CBMaterial : register(b3)
{
    float3 g_vAlbedo;      float g_fMetallic;
    float3 g_vEmissive;    float g_fRoughness;
    float  g_fAO;          float g_fReflectance;
    uint   g_uFlags;       float g_fPadMat;
};

cbuffer CBDirLight : register(b4)
{
    float3 g_vLightDir;    float g_fLightIntensity;
    float3 g_vLightColor;  float g_fPadLight;
};

Texture2D    g_AlbedoMap              : register(t0);
Texture2D    g_NormalMap              : register(t1);
Texture2D    g_MRA                    : register(t2);  // R: Metallic, G: Roughness, B: AO
Texture2D    g_EmissiveMap            : register(t3);
SamplerState g_Sampler                : register(s0);

static const uint FLAG_DISNEY     = 1u;
static const uint FLAG_MS_COMP    = 2u;

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
    float3 vWorldNrm  : NORMAL;
    float3 vWorldPos  : TEXCOORD0;
    float2 vTexCoord  : TEXCOORD1;
    float3 vWorldTan  : TEXCOORD2;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT o;
    float4 wp  = mul(float4(input.vPosition, 1.f), g_matWorld);
    o.vPosition = mul(wp, g_matViewProj);
    o.vWorldNrm = normalize(mul(input.vNormal,  (float3x3) g_matWorld));
    o.vWorldTan = normalize(mul(input.vTangent, (float3x3) g_matWorld));
    o.vWorldPos = wp.xyz;
    o.vTexCoord = input.vTexCoord;
    return o;
}

float3 SampleNormal(float3 N, float3 T, float2 uv)
{
    float3 nTS = g_NormalMap.Sample(g_Sampler, uv).xyz * 2.f - 1.f;
    float3 B   = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(nTS, TBN));
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float3 baseAlbedo = SRGBToLinear(g_AlbedoMap.Sample(g_Sampler, input.vTexCoord).rgb) * g_vAlbedo;
    float3 mra        = g_MRA.Sample(g_Sampler, input.vTexCoord).rgb;
    float  metallic   = saturate(mra.r * g_fMetallic);
    float  roughness  = max(mra.g * g_fRoughness, MIN_ROUGH);
    float  ao         = mra.b * g_fAO;

    SurfaceData s;
    s.albedo    = baseAlbedo;
    s.metallic  = metallic;
    s.roughness = roughness;
    s.position  = input.vWorldPos;
    s.normal    = SampleNormal(normalize(input.vWorldNrm),
                               normalize(input.vWorldTan),
                               input.vTexCoord);

    float3 V = normalize(g_vCameraPos - input.vWorldPos);
    float3 L = normalize(-g_vLightDir);  // surface → light

    float3 lightColor = g_vLightColor * g_fLightIntensity;

    bool useDisney = (g_uFlags & FLAG_DISNEY) != 0;
    float3 direct  = EvaluateBRDF(s, V, L, lightColor, g_fReflectance, useDisney);

    // 임시 ambient (IBL 도입 전). Stage 2 에서 irradiance 로 대체.
    float3 ambient = 0.03f * s.albedo * ao;

    float3 color = direct + ambient + SRGBToLinear(g_EmissiveMap.Sample(g_Sampler, input.vTexCoord).rgb) * g_vEmissive;

    // 최종은 linear HDR. Tonemap/감마는 PostFX 패스가 담당 (Stage 7 TAA 이후).
    return float4(color, 1.f);
}
```

---

## 8. 실전 셰이더 — `Shaders/BRDF/Skinned3D_PBR.hlsl`

```hlsl
// Shaders/BRDF/Skinned3D_PBR.hlsl
// Mesh3D_PBR 과 동일한 b0/b1/b3/b4 + b2(BoneMatrices) 추가
#include "BRDFGGX.hlsli"

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    row_major matrix g_matViewProjNoJitter;
    float3           g_vCameraPos;
    float            g_fTime;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
};

cbuffer CBBones : register(b2)
{
    row_major matrix g_BoneMatrices[256];
};

cbuffer CBMaterial : register(b3)
{
    float3 g_vAlbedo;      float g_fMetallic;
    float3 g_vEmissive;    float g_fRoughness;
    float  g_fAO;          float g_fReflectance;
    uint   g_uFlags;       float g_fPadMat;
};

cbuffer CBDirLight : register(b4)
{
    float3 g_vLightDir;    float g_fLightIntensity;
    float3 g_vLightColor;  float g_fPadLight;
};

Texture2D    g_AlbedoMap   : register(t0);
Texture2D    g_NormalMap   : register(t1);
Texture2D    g_MRA         : register(t2);
Texture2D    g_EmissiveMap : register(t3);
SamplerState g_Sampler     : register(s0);

struct VS_INPUT
{
    float3 vPosition    : POSITION;
    float3 vNormal      : NORMAL;
    float2 vTexCoord    : TEXCOORD0;
    float3 vTangent     : TANGENT;
    uint4  iBoneIndices : BLENDINDICES;
    float4 fBoneWeights : BLENDWEIGHT;
};

struct PS_INPUT
{
    float4 vPosition  : SV_POSITION;
    float3 vWorldNrm  : NORMAL;
    float3 vWorldPos  : TEXCOORD0;
    float2 vTexCoord  : TEXCOORD1;
    float3 vWorldTan  : TEXCOORD2;
};

PS_INPUT VS(VS_INPUT input)
{
    matrix skin =
        g_BoneMatrices[input.iBoneIndices.x] * input.fBoneWeights.x +
        g_BoneMatrices[input.iBoneIndices.y] * input.fBoneWeights.y +
        g_BoneMatrices[input.iBoneIndices.z] * input.fBoneWeights.z +
        g_BoneMatrices[input.iBoneIndices.w] * input.fBoneWeights.w;

    float4 skinned = mul(float4(input.vPosition, 1.f), skin);
    float4 wp      = mul(skinned, g_matWorld);

    PS_INPUT o;
    o.vPosition = mul(wp, g_matViewProj);
    o.vWorldNrm = normalize(mul(input.vNormal,  (float3x3) mul(skin, g_matWorld)));
    o.vWorldTan = normalize(mul(input.vTangent, (float3x3) mul(skin, g_matWorld)));
    o.vWorldPos = wp.xyz;
    o.vTexCoord = input.vTexCoord;
    return o;
}

float3 SampleNormal(float3 N, float3 T, float2 uv)
{
    float3 nTS = g_NormalMap.Sample(g_Sampler, uv).xyz * 2.f - 1.f;
    float3 B   = normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(nTS, TBN));
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    float3 baseAlbedo = SRGBToLinear(g_AlbedoMap.Sample(g_Sampler, input.vTexCoord).rgb) * g_vAlbedo;
    float3 mra        = g_MRA.Sample(g_Sampler, input.vTexCoord).rgb;
    float  metallic   = saturate(mra.r * g_fMetallic);
    float  roughness  = max(mra.g * g_fRoughness, MIN_ROUGH);
    float  ao         = mra.b * g_fAO;

    SurfaceData s;
    s.albedo    = baseAlbedo;
    s.metallic  = metallic;
    s.roughness = roughness;
    s.position  = input.vWorldPos;
    s.normal    = SampleNormal(normalize(input.vWorldNrm),
                               normalize(input.vWorldTan),
                               input.vTexCoord);

    float3 V = normalize(g_vCameraPos - input.vWorldPos);
    float3 L = normalize(-g_vLightDir);
    float3 lightColor = g_vLightColor * g_fLightIntensity;
    bool   useDisney  = (g_uFlags & 1u) != 0;
    float3 direct     = EvaluateBRDF(s, V, L, lightColor, g_fReflectance, useDisney);

    float3 ambient  = 0.03f * s.albedo * ao;
    float3 emissive = SRGBToLinear(g_EmissiveMap.Sample(g_Sampler, input.vTexCoord).rgb) * g_vEmissive;

    return float4(direct + ambient + emissive, 1.f);
}
```

---

## 9. ModelRenderer 통합 — 기존 API 보존 전략

`ModelRenderer::Init(path, pHlslPath)` 이 이미 셰이더 경로를 받는다. **기존 호출부는 그대로**, 신규 호출만 PBR 경로로:

```cpp
// 기존 (unlit) — 그대로 동작
pIrelia->Init("Resource/Irelia.fbx", L"Shaders/Skinned3D.hlsl");

// 신규 PBR — 셰이더 경로 교체 + Material 주입 (Phase B-7a 에서 Material slot 추가 예정)
pIrelia->Init("Resource/Irelia.fbx", L"Shaders/BRDF/Skinned3D_PBR.hlsl");
pIrelia->SetMaterial(std::move(CBRDFMaterial::Create("irelia_body")));
```

`ModelRenderer::Impl` 내부에 `DX11ConstantBuffer<CBMaterial> m_cbMat;` / `DX11ConstantBuffer<CBDirLight> m_cbLight;` 추가. 바인딩 슬롯은 b3/b4 고정.

> 현 시점 `ModelRenderer` 는 분해 예정(B-7a). 임시로 `Impl` 에 Material 포인터만 걸어두고, B-7a 에서 `MaterialComponent` 로 이관.

---

## 10. Furnace Test — `Shaders/Tests/FurnaceTest.hlsl`

```hlsl
// Shaders/Tests/FurnaceTest.hlsl
// White furnace: 1024 샘플 평균이 (1,1,1) 에 가까워야 에너지 보존 OK
#include "../BRDF/BRDFGGX.hlsli"
#include "../BRDF/BRDFIS.hlsli"

cbuffer CBFurnace : register(b0)
{
    float g_fRoughness;
    float3 _pad;
};

RWStructuredBuffer<float4> g_Result : register(u0);   // 1 element

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    float3 N = float3(0.f, 0.f, 1.f);
    float3 V = float3(0.f, 0.f, 1.f);
    float3 F0 = float3(1.f, 1.f, 1.f);       // White metal
    float  r  = max(g_fRoughness, MIN_ROUGH);

    float3 sum = 0.0f;
    const uint N_SAMPLES = 1024;
    for (uint i = 0; i < N_SAMPLES; ++i)
    {
        float2 Xi = Hammersley(i, N_SAMPLES);
        float3 H  = ImportanceSampleGGX(Xi, N, r);
        float3 L  = reflect(-V, H);

        float NoL = saturate(dot(N, L));
        float NoV = saturate(dot(N, V));
        float VoH = saturate(dot(V, H));
        float NoH = saturate(dot(N, H));

        if (NoL > 0.f)
        {
            // importance sampled → pdf weighted estimator
            float Vs = V_SmithGGXCorrelated(NoV, NoL, r);
            float3 F = F_Schlick(VoH, F0);
            sum += F * Vs * 4.f * NoL * VoH / max(NoH, 1e-4f);
        }
    }
    g_Result[0] = float4(sum / float(N_SAMPLES), 1.f);
}
```

C++ 구동부:

```cpp
// Engine/Private/Renderer/BRDF/FurnaceTest.cpp
// 디버그 UI (B-8 에서 CombatDebugPanel 옆 배치) 에서 호출.
// roughness 0 ~ 1 슬라이더 → 결과 RGB 출력. 0.9 이상이면 에너지 보존 통과.

#include "FurnaceTest.h"
#include "DX11StructuredBuffer.h"
#include "DX11Shader.h"
#include "CDX11Device.h"

bool CFurnaceTestHarness::Run(f32_t fRoughness, DirectX::XMFLOAT3& vOutRGB)
{
    auto* pDev = CDX11Device::Get()->GetDevice();
    auto* pCtx = CDX11Device::Get()->GetContext();

    DX11StructuredBuffer<DirectX::XMFLOAT4> result;
    result.Create(pDev, 1, /*bUAV*/true);

    struct CBParam { f32_t r; f32_t p[3]; };
    DX11ConstantBuffer<CBParam> cb;
    cb.Create(pDev);
    cb.Update(pCtx, { fRoughness, {0,0,0} });

    static DX11Shader s_CS;
    if (!s_CS.IsLoaded())
        s_CS.LoadCompute(pDev, L"Shaders/Tests/FurnaceTest.hlsl", "CSMain");

    cb.BindCS(pCtx, 0);
    auto uav = result.GetUAV();
    pCtx->CSSetShader(s_CS.GetComputeShader(), nullptr, 0);
    pCtx->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
    pCtx->Dispatch(1, 1, 1);

    DirectX::XMFLOAT4 out;
    result.ReadbackOne(pCtx, 0, out);
    vOutRGB = { out.x, out.y, out.z };

    return (out.x > 0.9f && out.y > 0.9f && out.z > 0.9f);
}
```

---

## 11. ImGui 튜너 통합

CLAUDE.md "신규 시스템 작성 시 튜닝 파라미터는 ImGui 슬라이더 노출 의무". BRDF 에 대응되는 것:

```cpp
// 예: Scene_InGame::OnImGui 에서 한 머터리얼 튜닝
ImGui::Begin("BRDF - Irelia Body");

auto& mat = *m_pIreliaMat;
DirectX::XMFLOAT3 albedo = mat.GetGPUData().vAlbedo;
if (ImGui::ColorEdit3("Albedo", &albedo.x))              mat.SetAlbedo(albedo);

f32_t metallic  = mat.GetGPUData().fMetallic;
if (ImGui::SliderFloat("Metallic",  &metallic,  0.f, 1.f)) mat.SetMetallic(metallic);

f32_t roughness = mat.GetGPUData().fRoughness;
if (ImGui::SliderFloat("Roughness", &roughness, 0.045f, 1.f)) mat.SetRoughness(roughness);

f32_t reflectance = mat.GetGPUData().fReflectance;
if (ImGui::SliderFloat("Reflectance (F0 dielectric)", &reflectance, 0.f, 0.08f)) mat.SetReflectance(reflectance);

bool_t disney = (mat.GetGPUData().uFlags & 1u) != 0;
if (ImGui::Checkbox("Disney Diffuse", (bool*)&disney)) mat.SetFlag(eBRDFFlag::UseDisneyDiffuse, disney);

if (ImGui::Button("Furnace Test"))
{
    DirectX::XMFLOAT3 rgb;
    bool_t pass = CFurnaceTestHarness::Run(roughness, rgb);
    m_furnaceResult = rgb; m_furnacePass = pass;
}
ImGui::TextColored(m_furnacePass ? ImVec4(0,1,0,1) : ImVec4(1,0,0,1),
                   "Result %.3f %.3f %.3f", m_furnaceResult.x, m_furnaceResult.y, m_furnaceResult.z);
ImGui::End();
```

---

## 12. 디버깅 체크리스트 (Winters 특화)

| 증상 | Winters 원인 | 해결 |
|------|-------------|------|
| 컴파일 에러 `'새 cbuffer' 중복 레지스터` | Mesh3D.hlsl 과 슬롯 충돌 | b3/b4 는 PBR 전용, 기존 셰이더는 b0/b1/b2 만 사용 — 슬롯 겹치지 않음. Device::PSSetConstantBuffers 호출 패턴 확인 |
| `HRESULT E_INVALIDARG` 셰이더 컴파일 | `/utf-8` 누락 | Engine.vcxproj AdditionalOptions 에 `/utf-8` — 그리고 `.hlsl` 은 FXC 가 자동 UTF-8. 소스 BOM 제거 여부 재확인 |
| White furnace 0.4 수준 | multi-scatter 미보상 | Stage 2 IBL 도입 시 Fdez-Aguera 추가 (본 문서 §7 원전) |
| Rough 금속이 검정 | `(1-F)*(1-metallic)` 누락 | `EvaluateBRDF` 의 `kd` 재확인 |
| 법선맵 흐름 이상 | TBN 방향 | Assimp import 시 tangent 유효한지 `aiProcess_CalcTangentSpace` 플래그 (ResourceCache/Model.cpp) |
| EngineSDK Client 빌드 실패 `BRDFMaterial.h` 못 찾음 | flat include 위반 | 공개 헤더끼리 서브경로 include 금지. UpdateLib.bat 재실행 |
| Engine 빌드 통과 / Client 빌드 실패 `construct_at` | dllexport + unique_ptr copy | `CBRDFMaterial` 의 `delete` 특수 멤버 확인 (본 문서 §3) |

---

## 13. 성능 기준선 (Winters 타겟)

| 항목 | 목표 |
|------|------|
| PBR PS 비용 (1 dir light) | ≤ 50 cycles / pixel |
| 1080p × 60fps 마진 | ≥ 3ms GPU |
| DrawCall (챔피언 1체) | 4 (body + blade + 2 accessory) |
| 재질 교체 시 Bind 수 | 4 texture + 1 cbuffer = 5 SetShaderResources |

원전 §11 기준 36 cycles/pixel + 노멀/MRA/Emissive 샘플 오버헤드 추가.

---

## 14. Stage 1 완료 기준 (Definition of Done)

- [ ] `Engine/Public/Renderer/BRDF/*.h` 6 파일 커밋 + `EngineSDK/inc` 동기화 (UpdateLib.bat)
- [ ] `Shaders/BRDF/*.hlsl,hlsli` 5 파일 커밋 + `Client/Bin/Resource/` xcopy 확인
- [ ] `Scene_InGame` 에서 `ModelRenderer::Init(..., L"Shaders/BRDF/Skinned3D_PBR.hlsl")` 로 이렐리아 렌더 성공
- [ ] ImGui 튜너로 Albedo/Metallic/Roughness 실시간 변경 반영
- [ ] Furnace Test 버튼: roughness 0.0, 0.25, 0.5, 0.75, 1.0 전부 RGB ≥ 0.9 (multi-scatter 보상 전이므로 1.0 에서 0.4~0.6 나오면 Stage 2 로 넘김)
- [ ] 기존 unlit `Mesh3D.hlsl` 경로 회귀 X (맵/야스오 등 unlit 대상은 그대로)
- [ ] Debug 빌드 `_CRTDBG_MAP_ALLOC` 누수 0

---

## 15. 참고 (원전 §13 재지시)

- Walter et al. 2007 — Microfacet Models
- Heitz 2014 — Masking-Shadowing Function
- Karis 2013 — UE4 Shading
- Burley 2012 — Disney Principled BSDF
- Filament Documentation
- `.md\plan\graphics\02_STAGE1_BRDF.md` — 본 엔진 Stage 1 상위 계획 (요약본, 본 문서가 실전 이식본)

---

*문서 끝. Stage 2 IBL 에서 이 BRDF 공식을 그대로 재활용해 환경광 통합.*
