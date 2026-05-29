# 04. IBL Prefiltering — Winters Engine 이식 가이드

> 원전: `C:\Users\user\Desktop\.markdown\Graphics\04_IBL_Prefiltering.md`
> 대상 경로: `Engine/Public/Renderer/IBL/`, `Engine/Private/Renderer/IBL/`, `Shaders/IBL/`, `Tools/WintersIBLBaker/`
> Phase: **E Stage 2 (PBR)** 의 중후반 — Stage 1 BRDF 이후 바로 착수.

---

## 0. 이 문서의 목표 (Winters 관점)

1. **HDR 환경맵 → SH9 (diffuse) + Prefilter Mip Chain (specular) + BRDF LUT** 로 오프라인 베이크.
2. **BRDF LUT 은 공용 엔진 에셋** (`Engine/Asset/brdf_lut.dds`, 한 번만 베이크).
3. **Prefilter / SH 는 맵별 에셋** (`Resource/IBL/<map>_prefilter.dds` + `<map>_sh.bin`).
4. 런타임 `CIBLManager` 가 맵 전환 시 스트리밍. `CGameInstance::Get_IBL()` Tier 2 Getter.
5. **WintersIBLBaker.exe** 는 DLL 의존성 없이 `EngineSDK/lib` 로 링크 (vcxproj 추가).

---

## 1. 디렉토리

```
Engine/Public/Renderer/IBL/
├── IBLTypes.h              // SH9, CBIBL, IBLAsset
├── IIBL.h                  // IBLManager 인터페이스
└── IBLManager.h            // class CIBLManager

Engine/Private/Renderer/IBL/
├── IBLManager.cpp
└── IBLLoader.cpp           // DDS / SH9 .bin 로드

Shaders/IBL/
├── IBLCommon.hlsli         // CubemapUVToDirection, Hammersley, ImportanceSampleGGX
├── EquirectToCubemap.hlsl  // CS
├── ConvolveIrradiance.hlsl // CS (SH9 투영이 아닌, cubemap 버전 — 백업용)
├── SHProject.hlsl          // CS (SH9 프로젝션)
├── PrefilterEnv.hlsl       // CS (mip 별)
├── GenerateBRDFLUT.hlsl    // CS
└── IBLSample.hlsli         // 런타임 파이프 통합용 (Lighting PS 에서 #include)

Tools/WintersIBLBaker/
├── main.cpp                // CLI 진입점
├── IBLBaker.h / IBLBaker.cpp
└── WintersIBLBaker.vcxproj
```

---

## 2. 공유 타입 — `IBLTypes.h`

```cpp
// Engine/Public/Renderer/IBL/IBLTypes.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <DirectXMath.h>
#include <array>

struct SH9
{
    std::array<DirectX::XMFLOAT3, 9> coeffs{};
};

// 공개 에셋 레이아웃 — bin 파일과 1:1 매칭. 버전 bump 시 BumpVersion_Internal 수정.
struct IBLAssetHeader
{
    u32_t uMagic;          // 'WIBL' = 0x4C424957
    u32_t uVersion;        // 1
    u32_t uFlags;          // bit0: HasPrefilter, bit1: HasSH
    u32_t _pad;

    // SH section
    DirectX::XMFLOAT4 shCoeffs[9];   // XMFLOAT3 + 1 float pad (cbuffer 호환)

    // Prefilter section
    u32_t uPrefilterSize;  // face 해상도 (mip 0 기준)
    u32_t uNumMips;
    u32_t uCubemapFormat;  // DXGI_FORMAT 값 — R16G16B16A16_FLOAT 권장
    u32_t _pad2;
};
constexpr u32_t WIBL_MAGIC = 0x4C424957u;

// b6 — Lighting PS 에서 참조
struct CBIBL
{
    DirectX::XMFLOAT4 shCoeffs[9];   // SH9, cbuffer 16B alignment
    f32_t             fNumPrefilterMips;
    f32_t             fIntensity;    // 광원 튜닝
    DirectX::XMFLOAT2 _pad;
};
static_assert(sizeof(CBIBL) % 16 == 0);
```

---

## 3. 인터페이스 / 매니저

```cpp
// Engine/Public/Renderer/IBL/IIBL.h
#pragma once
#include "WintersAPI.h"
#include "IBLTypes.h"

class WINTERS_ENGINE IIBLManager
{
public:
    virtual ~IIBLManager() = default;

    // 초기화 시: brdf_lut.dds 로드 (또는 없으면 generate + save)
    virtual bool_t EnsureBRDFLUT(const wstring_t& strAssetPath) = 0;

    // IBL 에셋 (Prefilter + SH9) 로드/전환
    virtual bool_t LoadIBLAsset(const wstring_t& strPath) = 0;

    // Lighting PS 에서 바인드할 리소스
    virtual ID3D11ShaderResourceView* GetPrefilterSRV() const = 0;
    virtual ID3D11ShaderResourceView* GetBRDFLUTSRV()   const = 0;
    virtual const CBIBL&              GetCBIBL()       const = 0;

    // 튜닝
    virtual void SetIntensity(f32_t f) = 0;
    virtual f32_t GetIntensity() const = 0;
};
```

```cpp
// Engine/Public/Renderer/IBL/IBLManager.h
#pragma once
#include "IIBL.h"
#include "DX11ConstantBuffer.h"
#include <wrl/client.h>
#include <memory>

class WINTERS_ENGINE CIBLManager : public IIBLManager
{
public:
    ~CIBLManager() override;

    static std::unique_ptr<CIBLManager> Create(ID3D11Device* pDev,
                                               ID3D11DeviceContext* pCtx);

    CIBLManager(const CIBLManager&)            = delete;
    CIBLManager& operator=(const CIBLManager&) = delete;
    CIBLManager(CIBLManager&&)                 = default;
    CIBLManager& operator=(CIBLManager&&)      = default;

    bool_t EnsureBRDFLUT(const wstring_t& strAssetPath) override;
    bool_t LoadIBLAsset(const wstring_t& strPath) override;

    ID3D11ShaderResourceView* GetPrefilterSRV() const override;
    ID3D11ShaderResourceView* GetBRDFLUTSRV()   const override;
    const CBIBL&              GetCBIBL()       const override;

    void   SetIntensity(f32_t f) override;
    f32_t  GetIntensity() const override;

    // Lighting PS 바인딩 헬퍼 — t5=Prefilter, t6=BRDF LUT, b6=CBIBL
    void BindForLighting(u32_t uSlotPrefilter = 5, u32_t uSlotBRDF = 6, u32_t uCBSlot = 6);

private:
    CIBLManager() = default;
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

---

## 4. 런타임 통합 — Lighting PS 확장

Clustered Lighting PS (문서 02 §8) 에 IBL ambient 추가:

```hlsl
// Shaders/IBL/IBLSample.hlsli
#ifndef IBL_SAMPLE_HLSLI
#define IBL_SAMPLE_HLSLI
#include "../BRDF/BRDFGGX.hlsli"

cbuffer CBIBL : register(b6)
{
    float4 g_SHCoeffs[9];
    float  g_fNumPrefilterMips;
    float  g_fIBLIntensity;
    float2 _pad;
};

TextureCube g_PrefilterMap : register(t5);
Texture2D   g_BRDFLUT      : register(t6);
SamplerState g_LinearClamp : register(s2);   // mip 샘플용

// Ramamoorthi & Hanrahan 2001
float3 IrradianceFromSH(float3 N)
{
    const float c1 = 0.429043f;
    const float c2 = 0.511664f;
    const float c3 = 0.743125f;
    const float c4 = 0.886227f;
    const float c5 = 0.247708f;

    return
        c1 * g_SHCoeffs[8].rgb * (N.x * N.x - N.y * N.y) +
        c3 * g_SHCoeffs[6].rgb * (N.z * N.z) +
        c4 * g_SHCoeffs[0].rgb -
        c5 * g_SHCoeffs[6].rgb +
        2.0f * c1 * (g_SHCoeffs[4].rgb * N.x * N.y
                   + g_SHCoeffs[7].rgb * N.x * N.z
                   + g_SHCoeffs[5].rgb * N.y * N.z) +
        2.0f * c2 * (g_SHCoeffs[3].rgb * N.x
                   + g_SHCoeffs[1].rgb * N.y
                   + g_SHCoeffs[2].rgb * N.z);
}

// Fdez-Aguera 2019 MS compensation
float3 EvaluateIBL(float3 N, float3 V, float roughness, float3 albedo,
                   float metallic, float ao)
{
    float  NoV = saturate(dot(N, V));
    float3 F0  = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    // Diffuse
    float3 irradiance = IrradianceFromSH(N);
    float3 kS         = F_SchlickRoughness(NoV, F0, roughness);
    float3 kD         = (1.0f - kS) * (1.0f - metallic);
    float3 diffuse    = kD * albedo * irradiance * INV_PI;

    // Specular
    float3 R        = reflect(-V, N);
    float  mipLevel = roughness * (g_fNumPrefilterMips - 1.0f);
    float3 Lr       = g_PrefilterMap.SampleLevel(g_LinearClamp, R, mipLevel).rgb;

    float2 brdf = g_BRDFLUT.Sample(g_LinearClamp, float2(NoV, roughness)).rg;

    // Multi-scatter (Fdez-Aguera)
    float3 FssEss = F0 * brdf.x + brdf.y;
    float  Ems    = 1.0f - (brdf.x + brdf.y);
    float3 Favg   = F0 + (1.0f - F0) / 21.0f;
    float3 Fms    = FssEss * Favg / (1.0f - Ems * Favg);
    float3 spec   = (FssEss + Fms * Ems) * Lr;

    return (diffuse + spec) * ao * g_fIBLIntensity;
}

#endif
```

문서 02 §8 `LightingPass.hlsl` 마지막 줄을 다음으로 교체:

```hlsl
#include "../IBL/IBLSample.hlsli"

// ... cluster loop 끝난 후:
total += EvaluateIBL(s.normal, V, s.roughness, s.albedo, s.metallic, ao);
// total += 0.03f * albedo * ao;   ← 임시 ambient 제거
```

> 법선 `s.normal` 은 world-space 이어야 IBL 이 맞음. Clustered Lighting 의 view-space 법선 사용 시 world 로 되돌리는 `matViewInverse` 곱셈 또는 GBuffer 에 world normal 저장 중 택일. Winters 는 **GBuffer 에 world normal 저장** 정책으로.

---

## 5. Shaders — 핵심 베이커 CS

### 5.1 Equirectangular → Cubemap

```hlsl
// Shaders/IBL/IBLCommon.hlsli
#ifndef IBL_COMMON_HLSLI
#define IBL_COMMON_HLSLI

static const float PI = 3.14159265359f;

float3 CubemapUVToDirection(float2 uv, uint faceIdx)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y     *= -1.0f;
    switch (faceIdx)
    {
        case 0: return normalize(float3( 1.0f,  ndc.y, -ndc.x));
        case 1: return normalize(float3(-1.0f,  ndc.y,  ndc.x));
        case 2: return normalize(float3( ndc.x,  1.0f, -ndc.y));
        case 3: return normalize(float3( ndc.x, -1.0f,  ndc.y));
        case 4: return normalize(float3( ndc.x,  ndc.y,  1.0f));
        case 5: return normalize(float3(-ndc.x,  ndc.y, -1.0f));
        default: return float3(0,0,1);
    }
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}
float2 Hammersley(uint i, uint N) { return float2(float(i)/float(N), RadicalInverse_VdC(i)); }

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi      = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a*a - 1.0f) * Xi.y));
    float sinTheta = sqrt(max(1.0f - cosTheta*cosTheta, 0.0f));

    float3 H_ts = float3(sinTheta*cos(phi), sinTheta*sin(phi), cosTheta);
    float3 up      = abs(N.z) < 0.999f ? float3(0,0,1) : float3(1,0,0);
    float3 tangent = normalize(cross(up, N));
    float3 bitan   = cross(N, tangent);

    return normalize(tangent*H_ts.x + bitan*H_ts.y + N*H_ts.z);
}

float D_GGX_Baker(float NoH, float roughness)
{
    float a = roughness*roughness, a2 = a*a;
    float d = NoH*NoH*(a2 - 1.0f) + 1.0f;
    return a2 / (PI * d * d + 1e-5f);
}

#endif
```

```hlsl
// Shaders/IBL/EquirectToCubemap.hlsl
#include "IBLCommon.hlsli"

cbuffer CBEquirect : register(b0) { uint g_uFaceIdx; uint g_uCubeSize; uint2 _pad; };
Texture2D<float4>           g_Equirect : register(t0);
RWTexture2DArray<float4>    g_Cube     : register(u0);
SamplerState                g_Linear   : register(s0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (any(dtid.xy >= g_uCubeSize)) return;
    float2 uv  = (float2(dtid.xy) + 0.5f) / float(g_uCubeSize);
    float3 dir = CubemapUVToDirection(uv, g_uFaceIdx);

    float2 eq = float2(
        atan2(dir.z, dir.x) / (2.0f * PI) + 0.5f,
        acos(clamp(dir.y, -1.0f, 1.0f)) / PI);

    float3 color = g_Equirect.SampleLevel(g_Linear, eq, 0).rgb;
    g_Cube[uint3(dtid.xy, g_uFaceIdx)] = float4(color, 1.0f);
}
```

### 5.2 Prefiltered Environment (Mip)

```hlsl
// Shaders/IBL/PrefilterEnv.hlsl
#include "IBLCommon.hlsli"

cbuffer CBPrefilter : register(b0)
{
    uint  g_uFaceIdx;
    uint  g_uMipSize;
    float g_fRoughness;
    uint  g_uEnvBaseSize;  // face 해상도 mip 0
};

TextureCube<float4>         g_Env : register(t0);
RWTexture2DArray<float4>    g_Out : register(u0);
SamplerState                g_LinearSampler : register(s0);

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (any(dtid.xy >= g_uMipSize)) return;
    float2 uv = (float2(dtid.xy) + 0.5f) / float(g_uMipSize);
    float3 N  = CubemapUVToDirection(uv, g_uFaceIdx);
    float3 R  = N;
    float3 V  = R;

    const uint N_SAMPLES = 1024;
    float3 pref = 0.0f;
    float  w    = 0.0f;

    for (uint i = 0; i < N_SAMPLES; ++i)
    {
        float2 Xi = Hammersley(i, N_SAMPLES);
        float3 H  = ImportanceSampleGGX(Xi, N, g_fRoughness);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NoL = saturate(dot(N, L));
        if (NoL > 0.0f)
        {
            float NoH = saturate(dot(N, H));
            float VoH = saturate(dot(V, H));
            float D   = D_GGX_Baker(NoH, g_fRoughness);
            float pdf = D * NoH / (4.0f * VoH) + 1e-4f;

            float saTex    = 4.0f * PI / (6.0f * float(g_uEnvBaseSize) * float(g_uEnvBaseSize));
            float saSample = 1.0f / (float(N_SAMPLES) * pdf + 1e-4f);
            float mip      = g_fRoughness == 0.0f ? 0.0f : 0.5f * log2(saSample / saTex);

            pref += g_Env.SampleLevel(g_LinearSampler, L, mip).rgb * NoL;
            w    += NoL;
        }
    }
    g_Out[uint3(dtid.xy, g_uFaceIdx)] = float4(pref / max(w, 1e-4f), 1.0f);
}
```

### 5.3 BRDF LUT

```hlsl
// Shaders/IBL/GenerateBRDFLUT.hlsl
#include "IBLCommon.hlsli"

RWTexture2D<float2> g_LUT : register(u0);

float V_SmithGGX_LUT(float NoV, float NoL, float roughness)
{
    float a  = roughness * roughness, a2 = a * a;
    float GV = NoL * sqrt(NoV*NoV*(1.0f - a2) + a2);
    float GL = NoV * sqrt(NoL*NoL*(1.0f - a2) + a2);
    return 0.5f / (GV + GL + 1e-5f);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    if (any(dtid.xy >= uint2(256, 256))) return;

    float NoV = (float(dtid.x) + 0.5f) / 256.0f;
    float r   = max((float(dtid.y) + 0.5f) / 256.0f, 0.045f);

    float3 V = float3(sqrt(1.0f - NoV*NoV), 0.0f, NoV);
    float3 N = float3(0.0f, 0.0f, 1.0f);

    float A = 0.0f, B = 0.0f;
    const uint N_SAMPLES = 1024;
    for (uint i = 0; i < N_SAMPLES; ++i)
    {
        float2 Xi = Hammersley(i, N_SAMPLES);
        float3 H  = ImportanceSampleGGX(Xi, N, r);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NoL = saturate(L.z);
        float NoH = saturate(H.z);
        float VoH = saturate(dot(V, H));

        if (NoL > 0.0f)
        {
            float Vs  = V_SmithGGX_LUT(NoV, NoL, r);
            float Gv  = Vs * VoH * NoL / max(NoH, 1e-4f);
            float Fc  = pow(1.0f - VoH, 5.0f);
            A += (1.0f - Fc) * Gv;
            B += Fc * Gv;
        }
    }
    g_LUT[dtid.xy] = float2(A / float(N_SAMPLES), B / float(N_SAMPLES));
}
```

### 5.4 SH9 프로젝션 (CPU 가 간단)

CPU 에서 Cubemap 을 읽어 9 계수 적분하는 것이 디버깅 쉽고 충분히 빠름 (베이커 32³ 페이스 = 6144 텍셀 × 9 basis).

```cpp
// Tools/WintersIBLBaker/SHProject.cpp
#include "IBLTypes.h"
#include <DirectXMath.h>

void EvaluateSH9Basis(const DirectX::XMFLOAT3& d, f32_t basis[9])
{
    f32_t x = d.x, y = d.y, z = d.z;
    basis[0] = 0.282094792f;                         // Y00
    basis[1] = 0.488602512f * y;                     // Y1-1
    basis[2] = 0.488602512f * z;                     // Y10
    basis[3] = 0.488602512f * x;                     // Y11
    basis[4] = 1.092548431f * x * y;                 // Y2-2
    basis[5] = 1.092548431f * y * z;                 // Y2-1
    basis[6] = 0.315391565f * (3.0f * z * z - 1.0f); // Y20
    basis[7] = 1.092548431f * x * z;                 // Y21
    basis[8] = 0.546274215f * (x * x - y * y);       // Y22
}

f32_t TexelSolidAngle(i32_t x, i32_t y, i32_t size)
{
    f32_t u = (2.0f * (x + 0.5f) / size) - 1.0f;
    f32_t v = (2.0f * (y + 0.5f) / size) - 1.0f;
    f32_t d = 1.0f + u * u + v * v;
    return 4.0f / (size * size * std::sqrt(d * d * d));
}

SH9 ProjectCubemapToSH(const float* pFaceData[6], i32_t size)
{
    SH9 sh{};
    for (i32_t face = 0; face < 6; ++face)
    {
        for (i32_t y = 0; y < size; ++y)
        for (i32_t x = 0; x < size; ++x)
        {
            f32_t u = (2.0f * (x + 0.5f) / size) - 1.0f;
            f32_t v = (2.0f * (y + 0.5f) / size) - 1.0f;
            DirectX::XMFLOAT3 dir = FaceUVToDirectionCPU(face, u, v);

            const float* p = pFaceData[face] + (y * size + x) * 4;
            DirectX::XMFLOAT3 L{p[0], p[1], p[2]};
            f32_t sa = TexelSolidAngle(x, y, size);

            f32_t basis[9];
            EvaluateSH9Basis(dir, basis);
            for (i32_t i = 0; i < 9; ++i)
            {
                sh.coeffs[i].x += L.x * basis[i] * sa;
                sh.coeffs[i].y += L.y * basis[i] * sa;
                sh.coeffs[i].z += L.z * basis[i] * sa;
            }
        }
    }
    return sh;
}
```

---

## 6. 오프라인 베이커 — `Tools/WintersIBLBaker/main.cpp`

```cpp
// Tools/WintersIBLBaker/main.cpp
// 사용: WintersIBLBaker.exe --input sky.hdr --output Resource/IBL/sky.ibl --size 512 --mips 5
// brdf_lut 생성: WintersIBLBaker.exe --brdfLUT Engine/Asset/brdf_lut.dds

#include "IBLBaker.h"
#include <string>
#include <iostream>

int wmain(int argc, wchar_t** argv)
{
    std::wstring input, output;
    u32_t uSize = 512, uMips = 5;
    bool_t bBRDF = false;
    std::wstring brdfOut;

    for (int i = 1; i < argc; ++i)
    {
        std::wstring a = argv[i];
        if      (a == L"--input"    && i + 1 < argc) input   = argv[++i];
        else if (a == L"--output"   && i + 1 < argc) output  = argv[++i];
        else if (a == L"--size"     && i + 1 < argc) uSize   = _wtoi(argv[++i]);
        else if (a == L"--mips"     && i + 1 < argc) uMips   = _wtoi(argv[++i]);
        else if (a == L"--brdfLUT"  && i + 1 < argc) { bBRDF = true; brdfOut = argv[++i]; }
    }

    IBLBaker baker;
    if (!baker.InitDevice()) return 1;

    if (bBRDF)
    {
        return baker.BakeBRDFLUT(brdfOut) ? 0 : 2;
    }

    if (input.empty() || output.empty()) { std::wcerr << L"Missing --input/--output\n"; return 3; }
    return baker.BakeIBLAsset(input, output, uSize, uMips) ? 0 : 4;
}
```

```cpp
// Tools/WintersIBLBaker/IBLBaker.h
#pragma once
#include "IBLTypes.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <string>

class IBLBaker
{
public:
    bool_t InitDevice();
    bool_t BakeBRDFLUT(const std::wstring& strOut);
    bool_t BakeIBLAsset(const std::wstring& strHDRIn, const std::wstring& strOut,
                        u32_t uSize, u32_t uMips);
private:
    Microsoft::WRL::ComPtr<ID3D11Device>        m_pDev;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pCtx;

    bool_t LoadHDR(const std::wstring& strIn,
                   Microsoft::WRL::ComPtr<ID3D11Texture2D>& pEquirect);
    bool_t EquirectToCubemap(ID3D11Texture2D* pEquirect,
                             Microsoft::WRL::ComPtr<ID3D11Texture2D>& pCube,
                             u32_t uSize);
    bool_t GeneratePrefilter(ID3D11Texture2D* pCube,
                             Microsoft::WRL::ComPtr<ID3D11Texture2D>& pPrefilter,
                             u32_t uSize, u32_t uMips);
    bool_t ProjectSH(ID3D11Texture2D* pCube, u32_t uSize, SH9& outSH);

    bool_t SaveIBLAsset(const std::wstring& strOut,
                        ID3D11Texture2D* pPrefilter, u32_t uMips,
                        const SH9& sh);
};
```

```cpp
// Tools/WintersIBLBaker/IBLBaker.cpp  — 핵심 경로만 발췌
bool_t IBLBaker::InitDevice()
{
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   flags, nullptr, 0, D3D11_SDK_VERSION,
                                   m_pDev.GetAddressOf(), &fl, m_pCtx.GetAddressOf());
    return SUCCEEDED(hr);
}

bool_t IBLBaker::BakeBRDFLUT(const std::wstring& strOut)
{
    // 1) 256x256 R16G16_FLOAT UAV 생성
    // 2) GenerateBRDFLUT.hlsl CS 로드 & Dispatch(32, 32, 1)
    // 3) DirectXTK::SaveDDSTextureToFile 로 저장
    // (상세는 원전 §3.7 + DirectXTK 예제 참조)
    return true;
}

bool_t IBLBaker::BakeIBLAsset(const std::wstring& strHDRIn, const std::wstring& strOut,
                              u32_t uSize, u32_t uMips)
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pEq, pCube, pPre;
    if (!LoadHDR(strHDRIn, pEq))                                return false;
    if (!EquirectToCubemap(pEq.Get(), pCube, uSize))            return false;

    SH9 sh;
    if (!ProjectSH(pCube.Get(), uSize, sh))                     return false;

    if (!GeneratePrefilter(pCube.Get(), pPre, uSize, uMips))    return false;

    return SaveIBLAsset(strOut, pPre.Get(), uMips, sh);
}

// SaveIBLAsset 구현: IBLAssetHeader + DDS 이어붙이기
bool_t IBLBaker::SaveIBLAsset(const std::wstring& strOut,
                              ID3D11Texture2D* pPrefilter, u32_t uMips,
                              const SH9& sh)
{
    IBLAssetHeader h{};
    h.uMagic          = WIBL_MAGIC;
    h.uVersion        = 1;
    h.uFlags          = 1u | 2u;     // both
    h.uPrefilterSize  = 0;           // 채우기: D3D11_TEXTURE2D_DESC 조회
    h.uNumMips        = uMips;
    h.uCubemapFormat  = DXGI_FORMAT_R16G16B16A16_FLOAT;

    D3D11_TEXTURE2D_DESC td; pPrefilter->GetDesc(&td);
    h.uPrefilterSize = td.Width;

    // SH 복사 (float3 → float4 pad)
    for (i32_t i = 0; i < 9; ++i)
        h.shCoeffs[i] = DirectX::XMFLOAT4{ sh.coeffs[i].x, sh.coeffs[i].y, sh.coeffs[i].z, 0.f };

    // Prefilter 텍스처를 DDS blob 으로
    // DirectXTK::SaveDDSTextureToFile 의 Memory 버전 (SaveDDSTextureToBlob) 사용
    Blob prefilterBlob;
    if (FAILED(DirectX::SaveDDSTextureToMemory(m_pCtx.Get(), pPrefilter, prefilterBlob)))
        return false;

    // 파일: [header][ddsBlob]
    FILE* fp = _wfopen(strOut.c_str(), L"wb");
    if (!fp) return false;
    fwrite(&h, sizeof(h), 1, fp);
    fwrite(prefilterBlob.Data(), 1, prefilterBlob.Size(), fp);
    fclose(fp);
    return true;
}
```

> `SaveDDSTextureToMemory` 는 DirectXTK 의 확장. 원래 `SaveDDSTextureToFile` 만 제공되면 임시 파일 경유. 현재 Winters ThirdPartyLib DirectXTK 에 포함된 함수 확인 후 경로 결정.

---

## 7. 런타임 로드 — `IBLLoader.cpp`

```cpp
// Engine/Private/Renderer/IBL/IBLLoader.cpp
#include "IBLManager.h"
#include "IBLTypes.h"
#include <fstream>
#include <DirectXTK/DDSTextureLoader.h>

bool_t CIBLManager::LoadIBLAsset(const wstring_t& strPath)
{
    std::ifstream ifs(strPath.c_str(), std::ios::binary);
    if (!ifs) return false;

    IBLAssetHeader h{};
    ifs.read(reinterpret_cast<char*>(&h), sizeof(h));
    if (h.uMagic != WIBL_MAGIC || h.uVersion != 1) return false;

    // SH9 복사 → CBIBL
    CBIBL& cb = m_pImpl->m_cbIBLData;
    for (i32_t i = 0; i < 9; ++i) cb.shCoeffs[i] = h.shCoeffs[i];
    cb.fNumPrefilterMips = static_cast<f32_t>(h.uNumMips);
    cb.fIntensity        = m_pImpl->m_fIntensity;

    m_pImpl->m_cbIBL.Update(m_pImpl->m_pCtx, cb);

    // Prefilter DDS 잔여 바이트 읽기
    ifs.seekg(0, std::ios::end);
    size_t total = ifs.tellg();
    size_t ddsSz = total - sizeof(h);
    std::vector<uint8_t> dds(ddsSz);
    ifs.seekg(sizeof(h), std::ios::beg);
    ifs.read(reinterpret_cast<char*>(dds.data()), ddsSz);

    Microsoft::WRL::ComPtr<ID3D11Resource> pRes;
    HRESULT hr = DirectX::CreateDDSTextureFromMemory(m_pImpl->m_pDev,
        dds.data(), ddsSz, pRes.GetAddressOf(),
        m_pImpl->m_PrefilterSRV.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

bool_t CIBLManager::EnsureBRDFLUT(const wstring_t& strAssetPath)
{
    // 파일 존재하면 로드. 없으면 CS dispatch 후 DDS 저장.
    Microsoft::WRL::ComPtr<ID3D11Resource> pRes;
    HRESULT hr = DirectX::CreateDDSTextureFromFile(m_pImpl->m_pDev, strAssetPath.c_str(),
        pRes.GetAddressOf(), m_pImpl->m_BRDFLUTSRV.ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr)) return true;

    // 없으면 엔진 초기화 시 CS 로 생성 후 DDS 저장 (릴리즈 빌드에서도 1회 cost)
    return GenerateBRDFLUT_Internal(strAssetPath);
}
```

---

## 8. 프레임 바인딩 패턴

```cpp
// Engine/Private/Framework/CEngineApp.cpp  Render()
auto* pIBL = m_pGameInstance->Get_IBL();
pIBL->BindForLighting(5, 6, 6);   // t5=Prefilter, t6=BRDFLUT, b6=CBIBL

// 이후 Lighting PS 가 IBLSample.hlsli 의 EvaluateIBL() 호출
```

---

## 9. 샘플러 준비

Prefilter 는 mip chain 순회 필수. Trilinear + Clamp:

```cpp
// SamplerStateCache.h  — 이미 LinearClamp 존재. 확인만.
```

---

## 10. 파일/아키 메모리

| 항목 | 크기 (512² cubemap, 5 mip) | 비고 |
|------|---------------------------|-----|
| IBLAssetHeader | ≈ 200 bytes | SH 108 + 레이아웃 overhead |
| Prefilter DDS | ≈ 17 MB | R16G16B16A16_FLOAT × 6 × mip chain |
| BRDF LUT DDS | 256 KB | R16G16_FLOAT 256×256 |
| CBIBL runtime | 192 bytes | 9 × 16 + 16 |

VRAM 은 Prefilter 가 대부분. 여러 맵 (카이리드/리에니에/시오프라 등) 동시 로드 금지 — 씬 전환 시 교체.

---

## 11. ImGui 튜너

```cpp
ImGui::Begin("IBL");

f32_t intensity = pIBL->GetIntensity();
if (ImGui::SliderFloat("IBL Intensity", &intensity, 0.f, 5.f))
    pIBL->SetIntensity(intensity);

static char path[256] = "Resource/IBL/sky_default.ibl";
ImGui::InputText("IBL Asset", path, 256);
if (ImGui::Button("Load"))
{
    std::wstring w(path, path + strlen(path));
    pIBL->LoadIBLAsset(w);
}

ImGui::Text("Prefilter Mips: %.0f", pIBL->GetCBIBL().fNumPrefilterMips);
ImGui::End();
```

---

## 12. 디버깅 체크리스트

| 증상 | 원인 | 해결 |
|------|------|------|
| Rough 금속 너무 어두움 | MS compensation 빠짐 | `EvaluateIBL` 의 Fdez-Aguera 블록 유지 |
| Cubemap seam | Sampler wrap 잘못 | `Address` 는 Clamp 여야 TextureCube 에서 자동 seam 해결 (D3D11 기본) |
| BRDF LUT 경계 아티팩트 | Clamp 샘플러 누락 | t6 의 샘플러 확인 |
| 반사가 flat | prefilter mip 단 하나만 사용 | `roughness * (N-1)` 계산 확인, `SampleLevel` 의 mip arg |
| 갱신 안 됨 | CBIBL `SetIntensity` 후 `Update` 누락 | `CIBLManager::SetIntensity` 가 내부 cbuffer 갱신 호출 |
| BRDF LUT 재생성 에러 | UAV format 불일치 | `DXGI_FORMAT_R16G16_FLOAT` 로 UAV 생성 |
| Client 빌드 실패 `IIBLManager not found` | EngineSDK 동기화 | UpdateLib.bat |
| 베이커 exe 링크 실패 | EngineSDK/lib 경로 | vcxproj `AdditionalLibraryDirectories` 에 `$(SolutionDir)EngineSDK\lib` |

---

## 13. Furnace Test (Stage 1 에서 실패했던 항목 재검증)

Stage 1 Furnace Test 가 rough=1 에서 RGB ≈ 0.4~0.6 이던 것이 MS compensation 으로 ≥ 0.95 가 되어야 한다. IBL 패스 삽입 후 Furnace 를 **IBL 경로로 다시 돌려** 확인:

```hlsl
// Shaders/Tests/FurnaceIBL.hlsl  (CS)
// 완전 흰색 환경맵 + albedo=1, metallic=1, roughness=r 로 EvaluateIBL 호출 → ~1
```

---

## 14. Stage 2 완료 기준 (IBL)

- [ ] `WintersIBLBaker.exe` 빌드 + `Engine/Asset/brdf_lut.dds` 생성
- [ ] `Resource/IBL/sky_default.ibl` 1개 이상 베이크 (이미 Resource/HDR/ 에 HDRI 가 있다면 사용)
- [ ] `CIBLManager::EnsureBRDFLUT` + `LoadIBLAsset` 런타임 성공
- [ ] Clustered Lighting PS 가 `EvaluateIBL` 호출 (임시 ambient 제거 확인)
- [ ] Furnace (rough=1, white metal, white env) ≥ 0.95
- [ ] 이렐리아 칼날 (metallic=1, rough=0.2) 에 환경 반사 명확히 보임
- [ ] ImGui 로 Intensity 슬라이더 실시간 반영

---

## 15. 참고

- Karis 2013 — UE4 Shading (Split-Sum)
- Lagarde 2014 — Moving Frostbite to PBR
- Ramamoorthi & Hanrahan 2001 — Irradiance SH
- Křivánek & Colbert 2008 — Filtered Importance Sampling
- Fdez-Aguera 2019 — Multi-Scattering IBL
- Filament Documentation, IBL chapter
- `.md\plan\graphics\03_STAGE2_PBR.md` — 상위 Stage 2 계획

---

*문서 끝. IBL 이 들어오면 그때부터 "진짜 PBR" 이라 부를 수 있다.*
