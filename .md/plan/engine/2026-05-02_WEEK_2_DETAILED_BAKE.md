# 2026-05-02 Week 2 상세 박제 — Track 1 PBR + Track 2 RH-0 §2

**작성일**: 2026-05-02
**상태**: 검토 대기 (계획서만 작성, 코드 변경 X — 사용자가 직접 반영)
**전제**: Week 1 (BRDF_GGX.hlsli 박제 + 9 leak consumer TODO marker) 완료
**상위 문서**: [`.md/plan/engine/2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md`](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md) §4 Week 2 박제

**Week 2 정합 보정**:
- 상위 계획서 §4.1 Mesh3D_PBR.hlsl 의 cbuffer 명 `PerFrame`/`PerObject` → 본 저장소 실 코드 `CBPerFrame`/`CBPerObject` 와 정합
- 상위 계획서 누락: **Skinned3D_PBR.hlsl** (챔프 = 스키닝 메시. Mesh3D_PBR 만으로는 이렐리아 적용 불가) — 본 문서 §3 에 신설
- 상위 계획서 §4.3 CMaterialPBR.h 만 → 본 문서 §5 에 .cpp 본문 추가

---

## 0. 한 줄

> **Week 2 = Track 1 (CBPerFrame/CBPerObject 확장 + Mesh3D_PBR + Skinned3D_PBR + CBPerMaterial + CMaterialPBR + ModelRenderer 변경 + 이렐리아 적용 + ChampionTuner 슬라이더) + Track 2 (GameInstance 8 게터 _Legacy rename + caller 일괄 치환). 합격: 이렐리아 metallic/roughness 시각 변화 + Frame ≤17ms + LoL 빌드 통과 (deprecated warning 만, error 0).**

---

## 1. Week 1 결과 검증 (Week 2 진입 전)

```bash
# 1. BRDF_GGX.hlsli 존재 확인
ls Shaders/BRDF/BRDF_GGX.hlsli

# 2. TODO marker 카운트 (≥12 hit 기대)
grep -rn "★ RH-2 TODO" Engine/Public/ Client/ | wc -l

# 3. LoL 빌드 통과 확인
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
# 기대: error 0
```

3개 모두 통과 시 Week 2 진입.

---

## 2. Week 2 작업 매트릭스 (시간 순서 + 의존성)

| 순서 | 작업 | 파일 | 의존성 |
|---|---|---|---|
| 2.1 | CBPerFrame / CBPerObject 확장 | `Engine/Public/RHI/DX11/DX11ConstantBuffer.h` | Week 1 완료 |
| 2.2 | CBPerMaterial.h 신설 | `Engine/Public/Renderer/CBPerMaterial.h` | 2.1 |
| 2.3 | Mesh3D_PBR.hlsl 신설 | `Shaders/Mesh3D_PBR.hlsl` | 2.1, 2.2 |
| 2.4 | **Skinned3D_PBR.hlsl 신설** (★ 챔프용) | `Shaders/Skinned3D_PBR.hlsl` | 2.1, 2.2 |
| 2.5 | CMaterialPBR.h + .cpp 신설 | `Engine/Public/Renderer/CMaterialPBR.h` + `Engine/Private/Renderer/CMaterialPBR.cpp` | 2.2 |
| 2.6 | ModelRenderer cbuffer update 변경 | `Engine/Private/Renderer/ModelRenderer.cpp` | 2.1 |
| 2.7 | GameInstance.h 8 게터 _Legacy rename | `Engine/Include/GameInstance.h` | (Track 2, 독립) |
| 2.8 | GameInstance.cpp 8 정의 _Legacy rename | `Engine/Private/GameInstance.cpp` | 2.7 |
| 2.9 | caller 일괄 치환 (8 게터) | Engine + Client 전체 | 2.7, 2.8 |
| 2.10 | 이렐리아 PBR 머티리얼 적용 | Champion 폴더 또는 Scene_InGame | 2.4, 2.5, 2.6 |
| 2.11 | ChampionTuner metallic/roughness 슬라이더 | UI 패널 | 2.5 |
| 2.12 | 합격 검증 | — | 모두 |

**병렬 가능**:
- Track 1 (2.1~2.6, 2.10~2.11) ↔ Track 2 (2.7~2.9) 는 독립 — 같은 사람이 작업 시 sequential, 두 사람 시 parallel.
- 단 2.9 caller 치환 시점에 Track 1 의 `CMaterialPBR::Create(pDevice)` 가 이미 작성되어 있으면 함께 치환 (`Get_RHIDevice()` → `Get_RHIDevice_Legacy()`).

---

## 3. Track 1 W2.1 — cbuffer 확장

### 3.1 BEFORE (`Engine/Public/RHI/DX11/DX11ConstantBuffer.h:113-127`)

```cpp
// ── 상수 버퍼 데이터 구조체 ──────────────────────────────────────

// register(b0) — 프레임당 1회 업데이트
struct CBPerFrame
{
    DirectX::XMFLOAT4X4 viewProjection;
};
static_assert(sizeof(CBPerFrame) % 16 == 0);

// register(b1) — 오브젝트당 1회 업데이트
struct CBPerObject
{
    DirectX::XMFLOAT4X4 world;
};
static_assert(sizeof(CBPerObject) % 16 == 0);
```

### 3.2 AFTER (Week 2)

```cpp
// ── 상수 버퍼 데이터 구조체 ──────────────────────────────────────

// register(b0) — 프레임당 1회 업데이트
//   PBR 진입 (W2): camera world position + directional light (W3 Forward+ 전 임시)
struct CBPerFrame
{
    DirectX::XMFLOAT4X4 viewProjection;     // 0   : 64
    DirectX::XMFLOAT3   cameraWorld;        // 64  : 76
    f32_t               _pad0;              // 76  : 80
    DirectX::XMFLOAT3   lightDirWorld;      // 80  : 92    (정규화된 단방향 라이트)
    f32_t               _pad1;              // 92  : 96
    DirectX::XMFLOAT3   lightColor;         // 96  : 108
    f32_t               lightIntensity;     // 108 : 112
};
static_assert(sizeof(CBPerFrame) == 112, "CBPerFrame must be 112 bytes (W2 PBR)");
static_assert(sizeof(CBPerFrame) % 16 == 0);

// register(b1) — 오브젝트당 1회 업데이트
//   PBR 진입 (W2): WorldInvTranspose (normal 변환용 — non-uniform scale 안전)
struct CBPerObject
{
    DirectX::XMFLOAT4X4 world;              // 0  : 64
    DirectX::XMFLOAT4X4 worldInvTranspose;  // 64 : 128
};
static_assert(sizeof(CBPerObject) == 128, "CBPerObject must be 128 bytes (W2 PBR)");
static_assert(sizeof(CBPerObject) % 16 == 0);
```

### 3.3 합격 게이트 (2.1)
- ✅ `static_assert(sizeof(CBPerFrame) == 112)` 통과
- ✅ `static_assert(sizeof(CBPerObject) == 128)` 통과
- ✅ 기존 ModelRenderer / PlaneRenderer 의 `cbPerFrame.Update()` 컴파일 통과 (struct 확장만 + 추가 필드 0 초기화)

---

## 4. Track 1 W2.2 — `Engine/Public/Renderer/CBPerMaterial.h` (신설)

**파일 경로**: `Engine/Public/Renderer/CBPerMaterial.h` (신설)

**전문**:

```cpp
#pragma once
//=============================================================
// CBPerMaterial.h
//   PBR 머티리얼 cbuffer (b3) POD struct.
//   HLSL slot: register(b3)
//   Shader 측 정의: Shaders/Mesh3D_PBR.hlsl / Shaders/Skinned3D_PBR.hlsl 의 CBPerMaterial.
//
//   ★ RH-2 TODO: DX11ConstantBuffer<CBPerMaterial> → IRHIBuffer (handle API)
//=============================================================

#include "WintersAPI.h"
#include "WintersTypes.h"
#include <DirectXMath.h>

namespace Engine
{
    // 64 bytes (16-byte aligned, cbuffer 호환)
    struct WINTERS_ENGINE CBPerMaterial
    {
        DirectX::XMFLOAT3 albedoTint;          // 0  : 12
        f32_t             metallic;            // 12 : 16
        f32_t             roughness;           // 16 : 20
        f32_t             ambientOcclusion;    // 20 : 24
        f32_t             emissiveIntensity;   // 24 : 28
        f32_t             _matPad0;            // 28 : 32
        DirectX::XMFLOAT3 emissiveTint;        // 32 : 44
        f32_t             _matPad1;            // 44 : 48
        f32_t             _matReserved[4];     // 48 : 64  (clearcoat / anisotropy 등 미래 확장)
    };
    static_assert(sizeof(CBPerMaterial) == 64, "CBPerMaterial must be 64 bytes (cbuffer alignment)");

    // 디폴트 (비금속, 거칠기 0.5, AO 1.0)
    inline CBPerMaterial MakeDefaultPBRMaterial()
    {
        CBPerMaterial cb{};
        cb.albedoTint = { 1.0f, 1.0f, 1.0f };
        cb.metallic = 0.0f;
        cb.roughness = 0.5f;
        cb.ambientOcclusion = 1.0f;
        cb.emissiveIntensity = 0.0f;
        cb.emissiveTint = { 0.0f, 0.0f, 0.0f };
        return cb;
    }
}
```

**합격 게이트**:
- ✅ `static_assert(sizeof(CBPerMaterial) == 64)` 통과
- ✅ `DX11ConstantBuffer<CBPerMaterial>` 인스턴스화 컴파일 통과 (기존 템플릿 활용)

---

## 5. Track 1 W2.3 — `Shaders/Mesh3D_PBR.hlsl` (신설, 정합 보정판)

**파일 경로**: `Shaders/Mesh3D_PBR.hlsl` (신설)

**역할**: 정적 메쉬 (맵 오브젝트, 미니언 등) PBR. 챔프는 Skinned3D_PBR 사용.

**전문**:

```hlsl
// =============================================================
// Mesh3D_PBR.hlsl
//   GGX BRDF 기반 정적 메쉬 PBR 셰이더.
//   기반: Mesh3D.hlsl (unlit) + BRDF_GGX.hlsli.
//   슬롯: b0=CBPerFrame(VP+Camera+Light), b1=CBPerObject(World+WorldInvTranspose), b3=CBPerMaterial(PBR)
//   t0=Albedo, t1=Normal(opt), t2=MetallicRoughness(opt)
//   s0=Linear-Wrap
// =============================================================

#include "BRDF/BRDF_GGX.hlsli"

// ── b0 CBPerFrame (정합: DX11ConstantBuffer.h 의 CBPerFrame struct 와 1:1) ──
cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3           g_vCameraWorld;
    float            _pad0;
    float3           g_vLightDirWorld;       // 정규화 (W2 한정 — Forward+ 는 W3)
    float            _pad1;
    float3           g_vLightColor;
    float            g_fLightIntensity;
};

// ── b1 CBPerObject (정합) ──
cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
};

// ── b3 CBPerMaterial (PBR 신설) ──
cbuffer CBPerMaterial : register(b3)
{
    float3 g_vAlbedoTint;
    float  g_fMetallic;
    float  g_fRoughness;
    float  g_fAmbientOcclusion;
    float  g_fEmissiveIntensity;
    float  _matPad0;
    float3 g_vEmissiveTint;
    float  _matPad1;
};

// ── Textures ──
Texture2D    g_AlbedoMap            : register(t0);
Texture2D    g_NormalMap            : register(t1);  // optional
Texture2D    g_MetallicRoughnessMap : register(t2);  // optional, R=Metallic, G=Roughness
SamplerState g_LinearWrap           : register(s0);

// ── Input / Output ──
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
    float3 vNormalWS : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

// ── Vertex Shader ──
PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos  = mul(float4(input.vPosition, 1.0f), g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vNormalWS = normalize(mul(float4(input.vNormal, 0.0f), g_matWorldInvTranspose).xyz);
    output.vTexCoord = input.vTexCoord;
    output.vWorldPos = worldPos.xyz;

    return output;
}

// ── Pixel Shader ──
float4 PS(PS_INPUT input) : SV_TARGET
{
    // 1. Sample albedo + alpha clip (LoL 머티리얼 보존)
    float4 albedoSample = g_AlbedoMap.Sample(g_LinearWrap, input.vTexCoord);
    clip(albedoSample.a - 0.05f);
    float3 albedo = albedoSample.rgb * g_vAlbedoTint;

    // 2. Metallic / Roughness (map 우선, 없으면 cbuffer 상수)
    float2 mr = g_MetallicRoughnessMap.Sample(g_LinearWrap, input.vTexCoord).rg;
    float  metallic  = (mr.r > 0.0f) ? mr.r : g_fMetallic;
    float  roughness = (mr.g > 0.0f) ? mr.g : g_fRoughness;
    roughness = clamp(roughness, 0.04f, 1.0f);  // floor 0.04 (NaN 방지)

    // 3. Normal (W2 단순화: vertex normal. TBN 정식화는 W6+)
    float3 N = normalize(input.vNormalWS);

    // 4. View / Light
    float3 V = normalize(g_vCameraWorld - input.vWorldPos);
    float3 L = normalize(-g_vLightDirWorld);   // light dir = "from surface to light"

    // 5. BRDF
    float3 brdf = BRDF_CookTorrance(N, V, L, albedo, metallic, roughness);
    float3 radiance = g_vLightColor * g_fLightIntensity;

    // 6. Direct lighting
    float3 Lo = brdf * radiance;

    // 7. Ambient (W2 단순화: 상수, IBL 은 W6+)
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * g_fAmbientOcclusion;

    // 8. Emissive
    float3 emissive = g_vEmissiveTint * g_fEmissiveIntensity;

    float3 color = ambient + Lo + emissive;

    // 9. Tone map (W2 단순화: Reinhard. ACES 는 W6+)
    color = color / (color + float3(1.0f, 1.0f, 1.0f));

    // 10. Gamma (W2 단순화: 2.2. sRGB Render Target 은 W6+)
    color = pow(saturate(color), 1.0f / 2.2f);

    return float4(color, albedoSample.a);
}
```

**합격 게이트 (2.3)**:
- ✅ `Shaders/Mesh3D_PBR.hlsl` 컴파일 통과 (fxc/dxc)
- ✅ cbuffer 명이 본 저장소 `DX11ConstantBuffer.h` 의 `CBPerFrame/CBPerObject` 와 정합
- ✅ 정적 메쉬 (예: 맵 오브젝트) 에 적용 시 시각 변화

---

## 6. Track 1 W2.4 — `Shaders/Skinned3D_PBR.hlsl` (★ 신설, 챔프용)

**파일 경로**: `Shaders/Skinned3D_PBR.hlsl` (신설)

**역할**: 챔프 (이렐리아 등) 스키닝 메시 PBR. Skinned3D.hlsl + BRDF_GGX.hlsli + b3 PerMaterial.

**전문**:

```hlsl
// =============================================================
// Skinned3D_PBR.hlsl
//   GGX BRDF 기반 스키닝 메쉬 PBR 셰이더 (챔프용).
//   기반: Skinned3D.hlsl (unlit) + BRDF_GGX.hlsli.
//   슬롯: b0=CBPerFrame, b1=CBPerObject, b2=CBBones, b3=CBPerMaterial
// =============================================================

#include "BRDF/BRDF_GGX.hlsli"

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3           g_vCameraWorld;
    float            _pad0;
    float3           g_vLightDirWorld;
    float            _pad1;
    float3           g_vLightColor;
    float            g_fLightIntensity;
};

cbuffer CBPerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
};

cbuffer CBBones : register(b2)
{
    row_major matrix g_BoneMatrices[256];
};

cbuffer CBPerMaterial : register(b3)
{
    float3 g_vAlbedoTint;
    float  g_fMetallic;
    float  g_fRoughness;
    float  g_fAmbientOcclusion;
    float  g_fEmissiveIntensity;
    float  _matPad0;
    float3 g_vEmissiveTint;
    float  _matPad1;
};

Texture2D    g_AlbedoMap            : register(t0);
Texture2D    g_NormalMap            : register(t1);
Texture2D    g_MetallicRoughnessMap : register(t2);
SamplerState g_LinearWrap           : register(s0);

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
    float4 vPosition : SV_Position;
    float3 vNormalWS : NORMAL;
    float2 vTexCoord : TEXCOORD0;
    float3 vWorldPos : TEXCOORD1;
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT output;

    // 1. Skinning
    matrix skinMatrix =
        g_BoneMatrices[input.iBoneIndices.x] * input.fBoneWeights.x +
        g_BoneMatrices[input.iBoneIndices.y] * input.fBoneWeights.y +
        g_BoneMatrices[input.iBoneIndices.z] * input.fBoneWeights.z +
        g_BoneMatrices[input.iBoneIndices.w] * input.fBoneWeights.w;

    float4 skinned   = mul(float4(input.vPosition, 1.0f), skinMatrix);
    float4 worldPos  = mul(skinned, g_matWorld);
    output.vPosition = mul(worldPos, g_matViewProj);
    output.vWorldPos = worldPos.xyz;
    output.vTexCoord = input.vTexCoord;

    // 2. Normal: skin → world (skinMatrix 의 3x3 사용 + g_matWorldInvTranspose)
    //   주의: skinMatrix 자체에 non-uniform scale 가능. WorldInvTranspose 적용 후 정규화.
    float3 skinnedNormal = mul(input.vNormal, (float3x3)skinMatrix);
    output.vNormalWS     = normalize(mul(float4(skinnedNormal, 0.0f), g_matWorldInvTranspose).xyz);

    return output;
}

float4 PS(PS_INPUT input) : SV_TARGET
{
    // 동일 PBR 픽셀 셰이더 (Mesh3D_PBR.hlsl 와 동일 로직)
    float4 albedoSample = g_AlbedoMap.Sample(g_LinearWrap, input.vTexCoord);
    clip(albedoSample.a - 0.05f);
    float3 albedo = albedoSample.rgb * g_vAlbedoTint;

    float2 mr = g_MetallicRoughnessMap.Sample(g_LinearWrap, input.vTexCoord).rg;
    float  metallic  = (mr.r > 0.0f) ? mr.r : g_fMetallic;
    float  roughness = (mr.g > 0.0f) ? mr.g : g_fRoughness;
    roughness = clamp(roughness, 0.04f, 1.0f);

    float3 N = normalize(input.vNormalWS);
    float3 V = normalize(g_vCameraWorld - input.vWorldPos);
    float3 L = normalize(-g_vLightDirWorld);

    float3 brdf = BRDF_CookTorrance(N, V, L, albedo, metallic, roughness);
    float3 radiance = g_vLightColor * g_fLightIntensity;
    float3 Lo = brdf * radiance;

    float3 ambient  = float3(0.03f, 0.03f, 0.03f) * albedo * g_fAmbientOcclusion;
    float3 emissive = g_vEmissiveTint * g_fEmissiveIntensity;

    float3 color = ambient + Lo + emissive;
    color = color / (color + float3(1.0f, 1.0f, 1.0f));      // Reinhard
    color = pow(saturate(color), 1.0f / 2.2f);                // Gamma 2.2

    return float4(color, albedoSample.a);
}
```

**합격 게이트 (2.4)**:
- ✅ Skinned3D_PBR.hlsl 컴파일 통과
- ✅ 이렐리아 적용 시 시각 변화 (metallic 슬라이더 0~1 변화 확인)

---

## 7. Track 1 W2.5 — `CMaterialPBR.h` + `CMaterialPBR.cpp` 신설

### 7.1 `Engine/Public/Renderer/CMaterialPBR.h` (상위 계획서 §4.3 + 보정)

```cpp
#pragma once
//=============================================================
// CMaterialPBR.h
//   PBR 머티리얼 클래스. Albedo / Normal / MetallicRoughness 텍스처 +
//   CBPerMaterial 상수 버퍼 1개. Mesh3D_PBR / Skinned3D_PBR 셰이더와 1:1.
//
//   ★ RH-2 TODO: ID3D11Device* → IRHIDevice* / DX11ConstantBuffer → IRHIBuffer
//=============================================================

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "CBPerMaterial.h"
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace Engine
{
    class CTexture;
    template<typename T> class DX11ConstantBuffer;

    class WINTERS_ENGINE CMaterialPBR
    {
    public:
        ~CMaterialPBR();
        CMaterialPBR(const CMaterialPBR&) = delete;
        CMaterialPBR& operator=(const CMaterialPBR&) = delete;
        CMaterialPBR(CMaterialPBR&&) = default;
        CMaterialPBR& operator=(CMaterialPBR&&) = default;

        // ★ RH-2 TODO: replace ID3D11Device* with IRHIDevice*
        static std::unique_ptr<CMaterialPBR> Create(ID3D11Device* pDevice);

        void SetAlbedoMap(std::shared_ptr<CTexture> pTex)            { m_pAlbedo = std::move(pTex); }
        void SetNormalMap(std::shared_ptr<CTexture> pTex)            { m_pNormal = std::move(pTex); }
        void SetMetallicRoughnessMap(std::shared_ptr<CTexture> pTex) { m_pMR = std::move(pTex); }

        void SetMetallic(f32_t v)            { m_CB.metallic = v;          m_bDirty = true; }
        void SetRoughness(f32_t v)           { m_CB.roughness = v;         m_bDirty = true; }
        void SetAmbientOcclusion(f32_t v)    { m_CB.ambientOcclusion = v;  m_bDirty = true; }
        void SetEmissiveIntensity(f32_t v)   { m_CB.emissiveIntensity = v; m_bDirty = true; }
        void SetAlbedoTint(f32_t r, f32_t g, f32_t b)
        { m_CB.albedoTint = { r, g, b }; m_bDirty = true; }
        void SetEmissiveTint(f32_t r, f32_t g, f32_t b)
        { m_CB.emissiveTint = { r, g, b }; m_bDirty = true; }

        // 슬라이더용 getter (ChampionTuner)
        f32_t GetMetallic() const            { return m_CB.metallic; }
        f32_t GetRoughness() const           { return m_CB.roughness; }
        f32_t GetAmbientOcclusion() const    { return m_CB.ambientOcclusion; }
        f32_t GetEmissiveIntensity() const   { return m_CB.emissiveIntensity; }

        // ★ RH-2 TODO: replace ID3D11DeviceContext* with IRHICommandList*
        // 호출 시 b3 슬롯에 cbuffer + t0/t1/t2 슬롯에 텍스처 SRV 바인딩 (PS 측).
        void Bind(ID3D11DeviceContext* pCtx);

        const CBPerMaterial& GetCB() const { return m_CB; }

    private:
        CMaterialPBR();

        bool_t m_bDirty = true;
        CBPerMaterial m_CB{};
        std::shared_ptr<CTexture> m_pAlbedo;
        std::shared_ptr<CTexture> m_pNormal;
        std::shared_ptr<CTexture> m_pMR;
        std::unique_ptr<DX11ConstantBuffer<CBPerMaterial>> m_pCBuffer;
    };
}
```

### 7.2 `Engine/Private/Renderer/CMaterialPBR.cpp` (★ 신규 박제)

```cpp
//=============================================================
// CMaterialPBR.cpp
//   Week 2 Track 1 신설. Mesh3D_PBR / Skinned3D_PBR 셰이더와 1:1 매칭.
//=============================================================

#include "CMaterialPBR.h"
#include "RHI/DX11/DX11ConstantBuffer.h"
#include "Resource/Texture.h"

#include <d3d11.h>

namespace Engine
{
    CMaterialPBR::CMaterialPBR()
    {
        m_CB = MakeDefaultPBRMaterial();
    }

    CMaterialPBR::~CMaterialPBR() = default;

    // ★ RH-2 TODO: replace ID3D11Device* with IRHIDevice*
    std::unique_ptr<CMaterialPBR> CMaterialPBR::Create(ID3D11Device* pDevice)
    {
        if (pDevice == nullptr) return nullptr;

        auto pInstance = std::unique_ptr<CMaterialPBR>(new CMaterialPBR());
        pInstance->m_pCBuffer = std::make_unique<DX11ConstantBuffer<CBPerMaterial>>();
        if (FAILED(pInstance->m_pCBuffer->Initialize(pDevice)))
        {
            return nullptr;
        }
        return pInstance;
    }

    // ★ RH-2 TODO: replace ID3D11DeviceContext* with IRHICommandList*
    void CMaterialPBR::Bind(ID3D11DeviceContext* pCtx)
    {
        if (pCtx == nullptr || m_pCBuffer == nullptr) return;

        // 1. cbuffer update (Dirty 시에만 Map/Unmap)
        if (m_bDirty)
        {
            m_pCBuffer->Update(pCtx, m_CB);
            m_bDirty = false;
        }

        // 2. b3 슬롯 (PS) 바인딩
        ID3D11Buffer* pBuffer = m_pCBuffer->GetBuffer();
        pCtx->PSSetConstantBuffers(3, 1, &pBuffer);

        // 3. 텍스처 SRV 바인딩 (t0=Albedo, t1=Normal, t2=MR)
        ID3D11ShaderResourceView* pSRVs[3] = { nullptr, nullptr, nullptr };

        if (m_pAlbedo) pSRVs[0] = m_pAlbedo->GetShaderResourceView_NonOwning();
        if (m_pNormal) pSRVs[1] = m_pNormal->GetShaderResourceView_NonOwning();
        if (m_pMR)     pSRVs[2] = m_pMR->GetShaderResourceView_NonOwning();

        pCtx->PSSetShaderResources(0, 3, pSRVs);
    }
}
```

> **주의 (구현 시 확인 필요)**: 본 .cpp 의 `m_pCBuffer->Initialize(pDevice)` / `GetBuffer()` / `m_pAlbedo->GetShaderResourceView_NonOwning()` 의 정확한 메서드 이름은 `DX11ConstantBuffer.h` 와 `Texture.h` 의 실 API 와 정합 확인 필요. 만약 API 가 다르면 (`Create` factory pattern + `m_pBuffer` 직접 접근 등) 본 본문 수정.

### 7.3 합격 게이트 (2.5)
- ✅ `CMaterialPBR::Create(pDevice)` 호출 시 64B cbuffer 정상 생성 (HRESULT OK)
- ✅ `Bind(pCtx)` 호출 시 b3 cbuffer + t0/t1/t2 SRV 바인딩
- ✅ Dirty flag 동작 (Set 시에만 Map/Unmap)

---

## 8. Track 1 W2.6 — `ModelRenderer.cpp` cbuffer update 변경

### 8.1 BEFORE (`Engine/Private/Renderer/ModelRenderer.cpp:122-143`)

```cpp
void ModelRenderer::UpdateTransform(const Mat4& matWorld)
{
    if (!m_pImpl->bReady) return;

    auto* pContext = CEngineApp::Get().GetDevice().GetContext();

    CBPerObject data;
    data.world = matWorld.m;
    m_pImpl->cbPerObject.Update(pContext, data);
}

void ModelRenderer::UpdateCamera(const Mat4& matViewProj)
{
    if (!m_pImpl->bReady) return;

    auto* pContext = CEngineApp::Get().GetDevice().GetContext();

    CBPerFrame data;
    data.viewProjection = matViewProj.m;
    m_pImpl->cbPerFrame.Update(pContext, data);
}
```

### 8.2 AFTER (Week 2)

```cpp
void ModelRenderer::UpdateTransform(const Mat4& matWorld)
{
    if (!m_pImpl->bReady) return;

    auto* pContext = CEngineApp::Get().GetDevice().GetContext();

    CBPerObject data{};
    data.world = matWorld.m;

    // ★ W2 PBR: WorldInvTranspose 계산 (normal 변환용)
    //   비균등 스케일 안전. inverse(transpose(world)) = transpose(inverse(world)).
    DirectX::XMMATRIX xmWorld = DirectX::XMLoadFloat4x4(&matWorld.m);
    DirectX::XMVECTOR det;
    DirectX::XMMATRIX xmInv = DirectX::XMMatrixInverse(&det, xmWorld);
    DirectX::XMMATRIX xmInvT = DirectX::XMMatrixTranspose(xmInv);
    DirectX::XMStoreFloat4x4(&data.worldInvTranspose, xmInvT);

    m_pImpl->cbPerObject.Update(pContext, data);
}

void ModelRenderer::UpdateCamera(const Mat4& matViewProj,
                                 const Vec3& vCameraWorld,
                                 const Vec3& vLightDirWorld,
                                 const Vec3& vLightColor,
                                 f32_t fLightIntensity)
{
    if (!m_pImpl->bReady) return;

    auto* pContext = CEngineApp::Get().GetDevice().GetContext();

    CBPerFrame data{};
    data.viewProjection = matViewProj.m;

    // ★ W2 PBR: camera + light
    data.cameraWorld    = { vCameraWorld.x, vCameraWorld.y, vCameraWorld.z };
    data.lightDirWorld  = { vLightDirWorld.x, vLightDirWorld.y, vLightDirWorld.z };
    data.lightColor     = { vLightColor.x, vLightColor.y, vLightColor.z };
    data.lightIntensity = fLightIntensity;

    m_pImpl->cbPerFrame.Update(pContext, data);
}
```

### 8.3 ModelRenderer.h 시그니처 변경

```cpp
// BEFORE
void UpdateCamera(const Mat4& matViewProj);

// AFTER (W2)
//   ★ 기존 caller 호환: 디폴트 인자로 W3 Forward+ 진입 전 단방향 라이트 1개 받기.
void UpdateCamera(const Mat4& matViewProj,
                  const Vec3& vCameraWorld,
                  const Vec3& vLightDirWorld = { 0.0f, -1.0f, 0.3f },
                  const Vec3& vLightColor    = { 1.0f, 0.95f, 0.85f },
                  f32_t       fLightIntensity = 3.0f);
```

### 8.4 caller 영향 (Scene_InGame 등)

`UpdateCamera` 호출 위치 (Scene_InGame.cpp 등):

```cpp
// BEFORE
m_pPlayerRenderer->UpdateCamera(matViewProj);

// AFTER (W2 — 카메라 위치 인자 추가)
//   m_pCamera 가 World 위치 보유한다고 가정. 정확한 메서드는 CDynamicCamera 확인.
Vec3 vCamPos = m_pCamera->GetWorldPosition();
m_pPlayerRenderer->UpdateCamera(matViewProj, vCamPos);
//   라이트 인자는 디폴트 (W3 Forward+ 전 임시).
```

### 8.5 합격 게이트 (2.6)
- ✅ `CBPerObject` / `CBPerFrame` 확장 필드가 ModelRenderer 에서 매 프레임 정상 업데이트
- ✅ Scene_InGame 의 `UpdateCamera` 호출 부분이 카메라 위치 인자와 함께 호환
- ✅ Frame time 회귀 0 (cbuffer 확장 자체는 1~2 µs 추가 only)

---

## 9. Track 1 W2.7 — 이렐리아 PBR 머티리얼 적용 (가이드)

### 9.1 적용 위치 (사용자가 직접 grep 으로 확인)

이렐리아 머티리얼 적용 위치 후보:
- `Client/Private/GameObject/Champion/Irelia/IreliaSkills.cpp` 또는 IreliaFxPresets.cpp 시작부
- `Client/Private/Scene/Scene_InGame.cpp` 의 OnEnter 의 이렐리아 ModelRenderer 생성 직후

```bash
# 이렐리아 머티리얼 / Mesh 텍스처 설정 위치 grep
rg -n "Irelia.*Texture|m_pIrelia.*Set" Client/Private/
```

### 9.2 적용 패턴

```cpp
// Scene_InGame::OnEnter (이렐리아 ModelRenderer 생성 후)
auto pIreliaPBR = CMaterialPBR::Create(CGameInstance::Get()->Get_RHIDevice_Legacy());
//   ↑ Track 2 W2.7 _Legacy rename 후 호출

// 텍스처 로드
auto pAlbedo = pCache->Load<CTexture>(L"Champion/Irelia/irelia_base_body_TX_CM.png");
auto pMR     = pCache->Load<CTexture>(L"Champion/Irelia/irelia_mr.png");  // 없으면 생략

pIreliaPBR->SetAlbedoMap(pAlbedo);
if (pMR) pIreliaPBR->SetMetallicRoughnessMap(pMR);

// 디폴트 머티리얼 값 (이렐리아 = 검 = 메탈, 몸 = 비메탈 + 천)
//   첫 진입은 단일 머티리얼로 전체 적용 → 슬라이더로 시각 확인 후 메시별 분리는 W3+
pIreliaPBR->SetMetallic(0.0f);    // 비메탈 시작
pIreliaPBR->SetRoughness(0.5f);

m_pIreliaMaterial = std::move(pIreliaPBR);
```

### 9.3 Render 호출 흐름 (ModelRenderer 변경 필요 가능)

```cpp
// Scene_InGame::OnRender (이렐리아 Render 직전)
m_pIreliaRenderer->UpdateCamera(matViewProj, vCamPos);
m_pIreliaRenderer->UpdateTransform(matIreliaWorld);

// ★ NEW (W2): PBR 머티리얼 바인딩
auto* pCtx = CGameInstance::Get()->Get_RHIDevice_Legacy()->GetContext();
m_pIreliaMaterial->Bind(pCtx);

// ★ ModelRenderer::Render 안에서 Skinned3D 대신 Skinned3D_PBR 셰이더 사용 (선택 분기 필요)
m_pIreliaRenderer->Render();
```

### 9.4 합격 게이트 (2.7)
- ✅ 이렐리아 게임 내 렌더링 시 unlit 보다 어둡지 않음 (라이트 적용 확인)
- ✅ metallic = 0.0 → 1.0 변화 시 specular 반사 시각 변화
- ✅ roughness = 0.0 → 1.0 변화 시 specular 흐릿해짐
- ✅ Frame time ≤17ms (현 ~9ms 기준 +8ms)

---

## 10. Track 1 W2.8 — ChampionTuner metallic/roughness 슬라이더 (가이드)

### 10.1 ChampionTuner 위치 (사용자 grep)

```bash
rg -ln "ChampionTuner|Champion Tuner" Client/Private/ | head -5
```

### 10.2 슬라이더 패턴 (ImGui)

```cpp
// ChampionTuner::OnImGui
if (m_pIreliaMaterial)
{
    if (ImGui::CollapsingHeader("Irelia PBR", ImGuiTreeNodeFlags_DefaultOpen))
    {
        f32_t metallic  = m_pIreliaMaterial->GetMetallic();
        f32_t roughness = m_pIreliaMaterial->GetRoughness();
        f32_t ao        = m_pIreliaMaterial->GetAmbientOcclusion();

        if (ImGui::SliderFloat("Metallic",  &metallic,  0.0f, 1.0f))
            m_pIreliaMaterial->SetMetallic(metallic);
        if (ImGui::SliderFloat("Roughness", &roughness, 0.04f, 1.0f))
            m_pIreliaMaterial->SetRoughness(roughness);
        if (ImGui::SliderFloat("AO",        &ao,        0.0f, 1.0f))
            m_pIreliaMaterial->SetAmbientOcclusion(ao);
    }
}
```

### 10.3 합격 게이트 (2.8)
- ✅ 슬라이더 드래그 시 즉시 시각 변화 (Dirty flag 동작)
- ✅ Frame 회귀 0

---

## 11. Track 2 W2.7~W2.9 — RH-0 §2 Legacy rename (실 코드 기반 diff)

### 11.1 `Engine/Include/GameInstance.h:101-108` BEFORE → AFTER

**BEFORE** (실 코드):

```cpp
public: //RHI — 공유 디바이스/셰이더/파이프라인/BlendCache 게터 (Tier 2: 포인터 캐시 후 직접 호출).
    //   CEngineApp 는 엔진 내부 전용이라 Client 가 직접 보지 못한다.
    //   Scene 등 Client 코드가 RHI 리소스를 잡을 때는 여기를 거친다.
    CDX11Device*      Get_RHIDevice();
    DX11Shader*       Get_MeshShader();
    DX11Pipeline*     Get_MeshPipeline();
    CBlendStateCache* Get_BlendStateCache();
    DX11Shader* Get_FxSpriteShader();
    DX11Pipeline* Get_FxSpritePipeline();
    DX11Shader* Get_FxMeshShader();
    DX11Pipeline* Get_FxMeshPipeline();
```

**AFTER** (RH-0 §2):

```cpp
public: //RHI — 공유 디바이스/셰이더/파이프라인/BlendCache 게터 (Tier 2: 포인터 캐시 후 직접 호출).
    //   CEngineApp 는 엔진 내부 전용이라 Client 가 직접 보지 못한다.
    //   Scene 등 Client 코드가 RHI 리소스를 잡을 때는 여기를 거친다.
    //
    //   ★ RH-0 §2 (2026-05-02): 8개 게터를 _Legacy 접미사로 rename.
    //     Track 1 (PBR) 코드는 _Legacy 호출로 자동 호환됨.
    //     RH-1 후 신규 Get_RHIDevice() -> IRHIDevice* 추가 (이름 충돌 회피 위해 RH-1 시점에는 Get_NewRHIDevice 사용).
    //     RH-2 종료 후 _Legacy 8개 제거 + Get_RHIDevice() 정식 rename.
    //   상세: .md/plan/rhi/00_RHI_MIGRATION_MASTER.md §6.1 RH-0 합격 게이트.
    [[deprecated("RH-2 후 IRHIDevice* 사용. .md/plan/rhi/00_RHI_MIGRATION_MASTER.md 참조")]]
    CDX11Device*      Get_RHIDevice_Legacy();
    [[deprecated("RH-2 후 IRHIShader* 사용")]]
    DX11Shader*       Get_MeshShader_Legacy();
    [[deprecated("RH-2 후 IRHIPipelineState* 사용")]]
    DX11Pipeline*     Get_MeshPipeline_Legacy();
    [[deprecated("RH-2 후 IRHIBlendState* 사용")]]
    CBlendStateCache* Get_BlendStateCache_Legacy();
    [[deprecated("RH-2 후 IRHIShader* 사용")]]
    DX11Shader*       Get_FxSpriteShader_Legacy();
    [[deprecated("RH-2 후 IRHIPipelineState* 사용")]]
    DX11Pipeline*     Get_FxSpritePipeline_Legacy();
    [[deprecated("RH-2 후 IRHIShader* 사용")]]
    DX11Shader*       Get_FxMeshShader_Legacy();
    [[deprecated("RH-2 후 IRHIPipelineState* 사용")]]
    DX11Pipeline*     Get_FxMeshPipeline_Legacy();
```

### 11.2 `Engine/Private/GameInstance.cpp:35-70` BEFORE → AFTER

**BEFORE** (실 코드):

```cpp
// ───────────────── RHI 포워딩 게터 ─────────────────
// CEngineApp::Get() 은 엔진 내부에서만 유효. Client 는 CGameInstance 경유로 접근.
CDX11Device* CGameInstance::Get_RHIDevice()
{
    return &CEngineApp::Get().GetDevice();
}
DX11Shader* CGameInstance::Get_MeshShader()
{
    return CEngineApp::Get().GetMeshShader();
}
DX11Pipeline* CGameInstance::Get_MeshPipeline()
{
    return CEngineApp::Get().GetMeshPipeline();
}
CBlendStateCache* CGameInstance::Get_BlendStateCache()
{
    return CEngineApp::Get().GetBlendStateCache();
}

DX11Shader* CGameInstance::Get_FxSpriteShader()
{
	return CEngineApp::Get().GetFxSpriteShader();
}

DX11Pipeline* CGameInstance::Get_FxSpritePipeline()
{
	return CEngineApp::Get().GetFxSpritePipeline();
}

DX11Shader* CGameInstance::Get_FxMeshShader()
{
	return CEngineApp::Get().GetFxMeshShader();
}

DX11Pipeline* CGameInstance::Get_FxMeshPipeline()
{
	return CEngineApp::Get().GetFxMeshPipeline();
}
```

**AFTER** (RH-0 §2):

```cpp
// ───────────────── RHI 포워딩 게터 (RH-0 §2 _Legacy rename, 2026-05-02) ─────────────────
// CEngineApp::Get() 은 엔진 내부에서만 유효. Client 는 CGameInstance 경유로 접근.
//   ★ RH-2 종료 후 _Legacy 8개 제거 + Get_RHIDevice() 정식 rename.
CDX11Device* CGameInstance::Get_RHIDevice_Legacy()
{
    return &CEngineApp::Get().GetDevice();
}
DX11Shader* CGameInstance::Get_MeshShader_Legacy()
{
    return CEngineApp::Get().GetMeshShader();
}
DX11Pipeline* CGameInstance::Get_MeshPipeline_Legacy()
{
    return CEngineApp::Get().GetMeshPipeline();
}
CBlendStateCache* CGameInstance::Get_BlendStateCache_Legacy()
{
    return CEngineApp::Get().GetBlendStateCache();
}
DX11Shader* CGameInstance::Get_FxSpriteShader_Legacy()
{
    return CEngineApp::Get().GetFxSpriteShader();
}
DX11Pipeline* CGameInstance::Get_FxSpritePipeline_Legacy()
{
    return CEngineApp::Get().GetFxSpritePipeline();
}
DX11Shader* CGameInstance::Get_FxMeshShader_Legacy()
{
    return CEngineApp::Get().GetFxMeshShader();
}
DX11Pipeline* CGameInstance::Get_FxMeshPipeline_Legacy()
{
    return CEngineApp::Get().GetFxMeshPipeline();
}
```

### 11.3 caller 일괄 치환 (8 게터)

**dry-run 먼저** (정확한 출현 갯수 확인):

```bash
# 본 저장소 루트에서 (워크트리 내부면 워크트리 루트)
for getter in Get_RHIDevice Get_MeshShader Get_MeshPipeline Get_BlendStateCache \
              Get_FxSpriteShader Get_FxSpritePipeline Get_FxMeshShader Get_FxMeshPipeline; do
  count=$(rg -c "${getter}\(\)" Engine Client 2>/dev/null | awk -F: '{sum+=$2} END {print sum+0}')
  echo "$getter: $count hit"
done
```

**일괄 치환 (Windows PowerShell)**:

```powershell
$getters = @(
  'Get_RHIDevice', 'Get_MeshShader', 'Get_MeshPipeline', 'Get_BlendStateCache',
  'Get_FxSpriteShader', 'Get_FxSpritePipeline', 'Get_FxMeshShader', 'Get_FxMeshPipeline'
)

Get-ChildItem -Path Engine, Client -Recurse -Include *.cpp,*.h -File |
  ForEach-Object {
    $path = $_.FullName
    $content = Get-Content $path -Raw
    $orig = $content
    foreach ($g in $getters) {
      # 정의 (선언 + 구현) 자체는 이미 _Legacy 로 변경됨. caller 만 치환.
      # word boundary `\b` + `(` 로 partial match 방지.
      $content = $content -replace "\b${g}\(\)", "${g}_Legacy()"
    }
    if ($content -ne $orig) {
      Set-Content -Path $path -Value $content -NoNewline
      Write-Host "patched: $path"
    }
  }
```

**bash + rg + sed 대안**:

```bash
for g in Get_RHIDevice Get_MeshShader Get_MeshPipeline Get_BlendStateCache \
         Get_FxSpriteShader Get_FxSpritePipeline Get_FxMeshShader Get_FxMeshPipeline; do
  rg -l "\\b${g}\\(\\)" Engine Client 2>/dev/null | while read f; do
    sed -i "s/\\b${g}()/${g}_Legacy()/g" "$f"
  done
done
```

> **주의**: 위 sed 치환은 GameInstance.h / GameInstance.cpp 의 **선언/정의** 도 잘못 치환할 수 있다 (`Get_RHIDevice_Legacy` → `Get_RHIDevice_Legacy_Legacy`). 두 단계로 분리:
> 1. 먼저 GameInstance.h + GameInstance.cpp 만 수동 변경 (§11.1, §11.2 그대로 적용).
> 2. 그 다음 caller 치환 시 GameInstance.h/.cpp 는 제외.
>
> ```bash
> for g in Get_RHIDevice Get_MeshShader Get_MeshPipeline Get_BlendStateCache \
>          Get_FxSpriteShader Get_FxSpritePipeline Get_FxMeshShader Get_FxMeshPipeline; do
>   rg -l "\\b${g}\\(\\)" Engine Client 2>/dev/null \
>     | grep -v "GameInstance\\.\\(h\\|cpp\\)" \
>     | while read f; do
>       sed -i "s/\\b${g}()/${g}_Legacy()/g" "$f"
>     done
> done
> ```

### 11.4 합격 게이트 (Track 2 W2)
- ✅ LoL 빌드 통과 (deprecated warning 다수, error 0)
- ✅ `rg "\\bGet_RHIDevice\\(\\)" Engine Client | grep -v _Legacy | wc -l` = 0 (다른 7 게터도 동일)
- ✅ Track 1 의 `CMaterialPBR::Create` 도 `Get_RHIDevice_Legacy()` 호출로 빌드 통과
- ✅ 게임 동작 무회귀 (포인터 본문은 동일, 이름만 변경)

---

## 12. Week 2 통합 합격 검증 (2.12)

### 12.1 빌드 검증

```bash
# 1. Engine 단독 빌드 (EngineSDK/inc 자동 동기화)
MSBuild Engine/Engine.vcxproj /p:Configuration=Debug /p:Platform=x64 /m
# 기대: error 0, deprecated warning 약 50~100 (caller 마이그 후 _Legacy 호출이 deprecated 발화)

# 2. 전체 솔루션 빌드
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
# 기대: error 0
```

### 12.2 런타임 검증 (이렐리아 PBR)

1. LoL 실행
2. 이렐리아 BanPick 후 InGame 진입
3. ChampionTuner 윈도우 열기
4. **Irelia PBR** 섹션:
   - Metallic 슬라이더 0 → 1: 이렐리아 **specular 반사** 강해짐 (검 부분 반짝임)
   - Roughness 슬라이더 0.04 → 1.0: specular 흐릿해짐
   - AO 슬라이더 0 → 1: 그림자 강해짐
5. F3 (Profiler 오버레이): Frame time ≤17ms 확인

### 12.3 Track 2 정합 검증

```bash
# 1. _Legacy 미치환 caller 확인 (hit 0 기대)
for g in Get_RHIDevice Get_MeshShader Get_MeshPipeline Get_BlendStateCache \
         Get_FxSpriteShader Get_FxSpritePipeline Get_FxMeshShader Get_FxMeshPipeline; do
  count=$(rg -c "\\b${g}\\(\\)" Engine Client 2>/dev/null \
    | grep -v "GameInstance\\.h\\|GameInstance\\.cpp" \
    | awk -F: '{sum+=$2} END {print sum+0}')
  echo "${g}: ${count} unmigrated caller (expected 0)"
done

# 2. _Legacy 치환된 caller 확인 (Track 1 신규 코드 포함)
rg "Get_RHIDevice_Legacy\(\)" Engine Client | wc -l
# 기대: ≥ 9 (RH-0 §1 inventory 의 9개 leak consumer 만큼)
```

---

## 13. 위험 시나리오

### 13.1 R-W2-1: cbuffer 확장 후 기존 호출 사이트 silent fail
- 시나리오: ModelRenderer 외 PlaneRenderer / FxStaticMeshRenderer 도 `CBPerFrame`/`CBPerObject` 사용. 이들은 W2 에서 추가 필드를 0으로 두고 컴파일만 통과하지만, 셰이더가 g_vCameraWorld 를 (0,0,0) 으로 받음 → 이 셰이더가 새 PBR 셰이더로 교체되면 라이팅이 카메라 (0,0,0) 기준 → 비정상.
- 완화:
  1. PlaneRenderer / FxStaticMeshRenderer 는 unlit 유지 (PBR 셰이더로 교체하지 X). 즉 기존 셰이더는 b0 의 첫 64바이트만 읽음 → 호환.
  2. PBR 셰이더 (Mesh3D_PBR / Skinned3D_PBR) 만 새 cbuffer 풀 사용.

### 13.2 R-W2-2: 이렐리아 PBR 시각이 어두움
- 원인: 라이트 강도 부족 / NoL clamp / gamma 2번 적용 / SDR clip
- 완화:
  1. ModelRenderer::UpdateCamera 의 디폴트 lightIntensity 3.0 → 5.0 임시 증가
  2. ChampionTuner 에 lightIntensity 슬라이더 추가
  3. RenderDoc 으로 PS 출력 색 캡처 → BRDF 결과 확인

### 13.3 R-W2-3: PowerShell 일괄 치환 후 빌드 깨짐
- 원인: word boundary `\b` 미적용 시 `Get_RHIDevice2()` 같은 가상 메서드 잘못 치환
- 완화:
  1. dry-run 으로 정확한 hit 갯수 사전 확인 (§11.3)
  2. GameInstance.h / GameInstance.cpp 는 일괄 치환 대상에서 **제외** (이미 §11.1, §11.2 로 수동 변경)
  3. 한 번에 하나씩 게터 마이그 (8 단계 분할) — 빌드 통과 확인하면서 진행

### 13.4 R-W2-4: Skinned3D_PBR 셰이더 도입 후 챔프 외 다른 스킨드 메시 영향
- 시나리오: 챔프 외 다른 스킨드 메시 (정글몹 등) 가 Skinned3D 사용 시 — 본 W2 는 셰이더 교체가 아닌 신설 (Skinned3D_PBR 별도)이므로 영향 0
- 완화:
  - ModelRenderer 안에서 PBR 사용 분기 (`bUsePBR` 플래그) 명시 → 디폴트 false (기존 Skinned3D 사용)
  - `m_pIreliaMaterial` 를 가진 ModelRenderer 만 PBR 셰이더 사용

---

## 14. 부록 A — Week 2 진입 체크리스트

```
[ ] Week 1 결과 검증 (§1) 통과
[ ] Visual Studio (devenv.exe) 종료 (vc143.pdb lock 회피)
[ ] git: feature/2026-05-02-week2 branch 생성 (또는 W1 branch 연속)
[ ] Engine 단독 빌드 1회 → EngineSDK/inc 동기화

Track 1 W2:
[ ] §3 CBPerFrame / CBPerObject 확장 (DX11ConstantBuffer.h)
[ ] §4 CBPerMaterial.h 신설
[ ] §5 Mesh3D_PBR.hlsl 신설
[ ] §6 Skinned3D_PBR.hlsl 신설 (★ 챔프용)
[ ] §7 CMaterialPBR.h + .cpp 신설
[ ] §8 ModelRenderer.cpp UpdateTransform / UpdateCamera 변경 + 헤더 시그니처 변경
[ ] Scene_InGame 등 UpdateCamera 호출 부분에 카메라 위치 인자 추가
[ ] §9 이렐리아 PBR 머티리얼 적용 (Scene_InGame OnEnter)
[ ] §10 ChampionTuner 슬라이더 추가

Track 2 W2:
[ ] §11.1 GameInstance.h 8 게터 _Legacy rename + [[deprecated]]
[ ] §11.2 GameInstance.cpp 8 정의 _Legacy rename
[ ] §11.3 caller 일괄 치환 (8 게터, GameInstance.h/cpp 제외)

검증:
[ ] §12.1 빌드 통과 (error 0, deprecated warning 만)
[ ] §12.2 런타임 — 이렐리아 PBR 슬라이더 시각 변화 + Frame ≤17ms
[ ] §12.3 _Legacy 미치환 caller hit 0 + 치환 caller hit ≥9
[ ] 게임 회귀 0 (이렐리아 외 챔프 / 맵 / 미니언 등 unlit 그대로)
```

---

## 15. 한 줄

> **Week 2 = cbuffer 확장 + Mesh3D_PBR + Skinned3D_PBR + CBPerMaterial + CMaterialPBR + ModelRenderer 변경 + 이렐리아 적용 + ChampionTuner 슬라이더 + GameInstance 8 게터 _Legacy rename + caller 일괄 치환. 합격: 이렐리아 metallic/roughness 시각 변화 + Frame ≤17ms + LoL 빌드 deprecated warning 만 (error 0). 본문 §3~§11 의 BEFORE/AFTER diff + h/cpp 전문 박제 그대로 적용.**

---

## 끝.
