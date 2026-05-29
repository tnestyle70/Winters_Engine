# 2026-05-01 GGX BRDF + DX12/Vulkan 결합 투 트랙 병합 계획서

**작성일**: 2026-05-01
**상태**: 검토 대기 (계획서만 작성, 코드 변경 X — 사용자가 직접 반영)
**원칙**: Behavioral Guidelines 4 (Think Before Coding / Simplicity First / Surgical Changes / Goal-Driven Execution)

**관련 마스터 문서**:
- 압축 검토: [.md/plan/engine/CLAUDE_COMPRESSION_REVIEW_PLAN_2026_05_01.md](CLAUDE_COMPRESSION_REVIEW_PLAN_2026_05_01.md)
- Track 1 핵심: `.md/plan/graphics/GGX+A/00_INDEX.md` + `01~05`
- Track 1 확장: `.md/plan/graphics/Graphics/00_INDEX.md` + `01~06`
- Track 2 마스터: `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md`

---

## 0. 한 줄

> **Track 1 (GGX BRDF + PBR + Forward+ + SSAO) 와 Track 2 (DX12/Vulkan 멀티 백엔드) 를 17주 동안 병합 진행. Week 1~2 는 충돌 0 (셰이더/inventory 만), Week 3 부터 RH-1 인터페이스 도입으로 점진 합치, RH-2 (W4-5) 에서 Track 1 PBR 코드가 IRHI 통과해 자동 마이그. Track 1 가시 결과 W2 부터, DX12 가시 결과 W13 부터.**

---

## 1. 개요

### 1.1 두 트랙 정의

| Track | 목표 | 마스터 문서 | 베이스라인 |
|---|---|---|---|
| **Track 1 (Graphics)** | unlit → GGX BRDF + PBR + Forward+ + SSAO | `.md/plan/graphics/GGX+A/00_INDEX.md` | 6 HLSL 모두 unlit (Mesh3D / Skinned3D / Default3D / FxMesh / FxSprite / Triangle), 라이팅 0 |
| **Track 2 (RHI)** | DX11 single → DX11 + DX12 + Vulkan | `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md` | DX11 only. CGameInstance 8개 leak getter (`Get_RHIDevice` 등). Public DX11 헤더 9개 leak consumer (Engine_Defines / CEngineApp / UI_Manager / PlaneRenderer / Mesh / FxBillboardComponent / FxMeshComponent / FxSystem / Scene_InGame). |

### 1.2 병합 의도

1. **시간 압축**: 두 트랙 sequential = 17주 + 17주 = 34주. 병합 = 17주 (50% 절감).
2. **충돌 회피**: Track 1 Week 1~2 는 셰이더 영역 (RHI 영향 0). Track 2 Week 1~2 는 inventory + Legacy rename (코드 이동 0).
3. **자동 마이그 흡수**: Track 2 RH-2 (Week 4~5) 에서 Public DX11 헤더 제거 시, Track 1 의 PBR 코드도 함께 IRHI 인터페이스 통과 → DX12/Vulkan 자동 호환.
4. **검증 단순화**: 각 Week 마다 합격 게이트 명시. 한 트랙 fail 해도 다른 트랙 전진 가능 (decoupling).

### 1.3 산업 합의

- **PBR/GGX**: Disney Principled BSDF (Burley 2012), UE4 (Karis 2013), Frostbite (Lagarde 2014). Trowbridge-Reitz GGX NDF 가 산업 표준 D항.
- **Forward+**: AMD 2012 / Olsson-Billeter-Assarsson 2012 / "Forward+ : Bringing Deferred Lighting to the Next Level" (Harada-McKee-Yang 2012).
- **SSAO**: HBAO+ (NVIDIA 2008), GTAO (Activision 2018, Jimenez et al.).
- **RHI**: WebGPU/Wgpu, Unreal RHI, The-Forge, NVRHI (NVIDIA), Diligent Engine — 모두 DX12/VK 기준 + DX11 emulation.

---

## 2. 합치 / 충돌 / 의존 그래프

### 2.1 합치점 (Synergy)

| # | 합치 | 의미 |
|---|---|---|
| S-1 | 셰이더-RHI 디커플링 | Track 1 의 BRDF.hlsli / Mesh3D_PBR.hlsl 은 RHI 영향 0 → Week 1~2 동안 무관하게 진행 |
| S-2 | DXC SPIR-V 도입 | Track 2 RH-1 의 DXC 도입이 Track 1 PBR 셰이더의 cross-compile 자연 지원 |
| S-3 | Texture/Buffer 인터페이스 통과 | Track 2 RH-1 의 IRHITexture/IRHIBuffer 가 Track 1 의 albedo/metallic/roughness 텍스처 로드 통과 |
| S-4 | Forward+ Compute | Track 1 Forward+ Light Cull 의 ComputeShader 가 Track 2 RH-3 의 IRHIComputePipeline 통과 |
| S-5 | DX12/Vulkan 자동 호환 | RH-2 후 Track 1 코드는 자동으로 DX12/Vulkan 동작 (셰이더는 DXC, 자원은 인터페이스) |

### 2.2 충돌점 (Conflict)

| # | 충돌 | 영향 | 완화 |
|---|---|---|---|
| C-1 | Track 1 W2 의 `DX11ConstantBuffer<CBPerMaterial>` 추가 | RH-2 에서 IRHIBuffer 통과로 마이그 필요 (~30분) | RH-2 마이그 매트릭스에 CBPerMaterial 명시 |
| C-2 | Track 1 W2 의 `Get_RHIDevice()` 호출 | RH-0 Legacy rename 시 `_Legacy` 접미사 자동 호환 | RH-0 §2 Legacy rename 시 Track 1 코드도 함께 치환 |
| C-3 | Track 1 의 `CTexture` 사용 | Mesh.h / Texture.h 가 RH-2 에서 IRHITexture 통과 | RH-2 마이그 매트릭스에 CTexture 명시 |
| C-4 | Track 1 W4 SSAO 의 G-Buffer (depth/normal) | Track 2 RH-3 RenderPass 도입 후 정식 G-Buffer 통과 | W4 는 임시 RT 사용 → W6 후 RenderPass 통과로 정식화 |
| C-5 | Track 1 W3 Forward+ Compute 의 StructuredBuffer | RH-3 BindGroup immutable 정책과 호환 | StructuredBuffer 는 BindGroup 안에 read-only 로 배치 |

### 2.3 의존 그래프

```
Week 1                              Week 2                              Week 3                  Week 4-5                    Week 6+
─────                              ─────                              ─────                  ─────                       ─────
T1: BRDF_GGX.hlsli (신설)         T1: PBR + Mesh3D_PBR.hlsl          T1: Forward+ (Compute)  T1: SSAO + 안정화          T1: → IRHI 통과 마이그
   ↓ (셰이더 컴파일 통과)               ↓ (이렐리아 PBR 시각 변화)            ↓ (64 라이트 ≤16ms)        ↓ (깊이 차폐 ≤20ms)            ↓ (PBR 셰이더 IRHI 통과)
                                                                                                                          ↓
T2: RH-0 §1 inventory + 9 leak    T2: RH-0 §2 Legacy rename          T2: RH-1 IRHIDevice    T2: RH-2 시작 + 완료        T2: RH-3 PSO/RenderPass +
   consumer 파악                       (`Get_RHIDevice` → `_Legacy`)        9 인터페이스 + handle      (Public DX11 헤더 제거)        RH-4 64-bit handle
                                                                                                                          ↓
                                                                                                                       T2: RH-5 DX12 (W7-13)
                                                                                                                          ↓
                                                                                                                       T2: RH-6 Vulkan (W14-17, 선택)
```

### 2.4 마일스톤 매트릭스 (Week × Track)

| Week | Track 1 (Graphics) | Track 2 (RHI) | T1 합격 게이트 | T2 합격 게이트 |
|---|---|---|---|---|
| **W1** | GGX BRDF.hlsli (D/G/F + Cook-Torrance) | RH-0 §1: inventory + 9 leak consumer 파악 + TODO marker 박제 | hlsli 컴파일 통과 + 단위 호출 1회 | TODO marker 박제 + LoL 빌드 통과 |
| **W2** | PBR + Mesh3D_PBR.hlsl + CBPerMaterial + 이렐리아 적용 | RH-0 §2: Legacy rename + 8 게터 deprecated | 이렐리아 metallic/roughness 시각 변화 + Frame ≤17ms | deprecated warning 다수 + error 0 |
| **W3** | Forward+ Light Cull (Tile compute) | RH-1: 9 인터페이스 + handle API + DX11 어댑터 | 64 동적 라이트 Frame ≤16ms | handle API 컴파일 + IRHIDevice* 통과 |
| **W4** | SSAO (HBAO+ 또는 GTAO) | RH-2 시작: Renderer/Resource 9 leak 마이그 | 깊이 기반 차폐 시각 + Frame ≤20ms | ModelRenderer/PlaneRenderer/Mesh.h 마이그 |
| **W5** | T1 안정화 + 튜닝 (회귀 검증) | RH-2 완료: Public DX11 헤더 제거 | (T1 회귀 검증 통과) | `Engine/Public` grep d3d11.h hit 0 |
| **W6** | T1 → IRHI 통과 마이그 (CBPerMaterial → IRHIBuffer) | RH-3 PSO/RenderPass/BindGroup + RH-4 64-bit handle | PBR 셰이더가 IRHI 통과해서 동작 | PSO 1개로 끝, BindGroup immutable |
| **W7-9** | (안정화) | RH-5 DX12 compile-only | (회귀 검증) | DX12 빌드 통과 |
| **W10-13** | (안정화) | RH-5 DX12 visual parity | — | DX12 시각 결과 동일 (frame diff < 1px) + PSO 캐시 동작 |
| **W14-17** (선택) | (안정화) | RH-6 Vulkan | — | VK 빌드 + DXC SPIR-V cross-compile + validation 0 error |

---

## 3. Week 1 즉시 진입 — 코드 박제 (★ h/cpp 전문)

> 본 절은 사용자 즉시 진입용. CLAUDE.md §8 계획서 규칙 1~7 준수.

### 3.1 Track 1 W1 Step 1 — `Shaders/BRDF/` 폴더 신설 + `BRDF_GGX.hlsli`

**파일 경로**: `Shaders/BRDF/BRDF_GGX.hlsli` (신설)

**역할**: GGX BRDF 의 D / G / F 함수 + Cook-Torrance 헬퍼. RHI 영향 0 (셰이더 레벨).

**전문**:

```hlsl
// =============================================================
// BRDF_GGX.hlsli
//   GGX (Trowbridge-Reitz) BRDF + Cook-Torrance helpers.
//   - Disney Principled BSDF 의 핵심 D/G/F.
//   - Karis 2013 (UE4) / Burley 2012 (Disney) 관례.
// =============================================================

#ifndef BRDF_GGX_HLSLI
#define BRDF_GGX_HLSLI

static const float PI = 3.14159265359f;

// -------------------------------------------------------------
// D: Trowbridge-Reitz GGX NDF (Normal Distribution Function)
//   roughness: linear (perceptual). 내부에서 a = roughness^2 사용 (Disney 관례).
// -------------------------------------------------------------
float D_GGX(float NoH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NoH2 = NoH * NoH;
    float denom = NoH2 * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

// -------------------------------------------------------------
// G: Smith Schlick-GGX (Karis approx, direct lighting)
//   k = (roughness + 1)^2 / 8   (direct)
//   k = roughness^2 / 2         (IBL — Stage 2 에서 별도 사용)
// -------------------------------------------------------------
float G_SchlickGGX(float NoX, float k)
{
    return NoX / (NoX * (1.0f - k) + k);
}

float G_Smith(float NoV, float NoL, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return G_SchlickGGX(NoV, k) * G_SchlickGGX(NoL, k);
}

// -------------------------------------------------------------
// F: Schlick Fresnel
//   F0: 비금속 ~0.04, 금속 = albedo (energy conservation).
// -------------------------------------------------------------
float3 F_Schlick(float VoH, float3 F0)
{
    float Fc = pow(saturate(1.0f - VoH), 5.0f);
    return F0 + (1.0f - F0) * Fc;
}

// -------------------------------------------------------------
// Cook-Torrance BRDF (full)
//   입력: N(world normal) / V(view dir) / L(light dir, 모두 normalize)
//          albedo: linear / metallic [0,1] / roughness [0,1]
//   출력: BRDF * NdotL (광도/색은 호출자가 별도 곱)
// -------------------------------------------------------------
float3 BRDF_CookTorrance(
    float3 N, float3 V, float3 L,
    float3 albedo, float metallic, float roughness)
{
    float3 H = normalize(V + L);
    float NoV = saturate(dot(N, V)) + 1e-5f;
    float NoL = saturate(dot(N, L));
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    // F0: dielectric 0.04, metal = albedo
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float  D = D_GGX(NoH, roughness);
    float3 F = F_Schlick(VoH, F0);
    float  G = G_Smith(NoV, NoL, roughness);

    float3 specular = (D * F * G) / (4.0f * NoV * NoL + 1e-5f);

    // Lambertian diffuse (PI 정규화) + 비금속만 (kD)
    float3 kD = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * NoL;
}

#endif // BRDF_GGX_HLSLI
```

**검증** (Goal-Driven, 2 단계):

1. **단위 컴파일**: 임시로 `Shaders/Mesh3D.hlsl` 끝에 `#include "BRDF/BRDF_GGX.hlsli"` 한 줄 추가 후 fxc 또는 dxc 로 컴파일 → 에러 0.
2. **호출 테스트**: 임시로 PS 내부에 self-contained 더미 벡터를 두고 호출 후 컴파일 → 에러 0:
   ```hlsl
   float3 N = normalize(float3(0, 1, 0));
   float3 V = normalize(float3(0, 1, 1));
   float3 L = normalize(float3(1, 1, 0));
   float3 dummy = BRDF_CookTorrance(N, V, L, float3(1, 1, 1), 0.0f, 0.5f);
   ```
3. 검증 후 임시 include 제거.

**합격 게이트 (T1-W1)**:
- ✅ `Shaders/BRDF/BRDF_GGX.hlsli` 파일 존재
- ✅ Mesh3D.hlsl 끝 임시 include + 호출 → fxc/dxc 컴파일 에러 0
- ✅ Frame time 회귀 0 (셰이더 추가 자체는 사용 안 하면 영향 0)

---

### 3.2 Track 2 W1 Step 1 — RH-0 §1 inventory + 9개 leak consumer TODO marker

**대상 파일 (1차 시드 목록)**:

> 아래 9개는 시작점 (`.md/plan/rhi/00_RHI_MIGRATION_MASTER.md §1.1` 표 인용). 실제 진입 전 `rg "ID3D11Device|d3d11.h|RHI/DX11" Engine/Public Client` 재실행으로 최신 inventory 를 확정한다.

| # | 파일 | leak 종류 | hit 수 |
|---|---|---|---|
| 1 | `Engine/Public/Engine_Defines.h` | `RHI/DX11/...` 직접 include | ≥1 |
| 2 | `Engine/Public/Framework/CEngineApp.h` | `RHI/DX11/...` 직접 include | ≥1 |
| 3 | `Engine/Public/Manager/UI/UI_Manager.h` | `ID3D11Device*` 매개변수 | ≥1 |
| 4 | `Engine/Public/Renderer/PlaneRenderer.h` | `ID3D11Device*` 매개변수 | ≥1 |
| 5 | `Engine/Public/Resource/Mesh.h` | `ID3D11Device*` 매개변수 | ≥1 |
| 6 | `Client/Public/GameObject/FX/FxBillboardComponent.h` | `ID3D11*` 직접 사용 | ≥1 |
| 7 | `Client/Public/GameObject/FX/FxMeshComponent.h` | `ID3D11*` 직접 사용 | ≥1 |
| 8 | `Client/Public/GameObject/FX/FxSystem.h` | `ID3D11*` 직접 사용 | ≥1 |
| 9 | `Client/Private/Scene/Scene_InGame.cpp` | `Get_RHIDevice()->GetContext()` 12 hit | 12 |

**추가 점검 후보** (Week 1 inventory 시 함께 확인):
- `Engine/Public/Editor/ImGuiLayer.h` (ImGui DX11 backend)
- `Engine/Public/Renderer/FxStaticMeshRenderer.h`
- `Engine/Public/Resource/Model.h`
- `Engine/Public/Resource/ResourceCache.h`
- `Engine/Public/Resource/Texture.h`
- 이상 5개는 위 9개 시드 목록에 빠진 추가 노출 지점.

**작업**: 각 leak 위치 위에 다음 형식의 TODO marker 박제 (한 줄 또는 두 줄 주석):

```cpp
// ★ RH-2 TODO: replace ID3D11Device* with IRHIDevice* (handle API)
// ★ RH-2 TODO: remove d3d11.h include after Public DX11 헤더 제거
```

**예시 1** — `Engine/Public/Renderer/PlaneRenderer.h` (가상 위치):

```cpp
// BEFORE
class CPlaneRenderer
{
public:
    static unique_ptr<CPlaneRenderer> Create(
        ID3D11Device* pDevice,    // ← leak
        DX11Shader* pShader,
        DX11Pipeline* pPipeline);
    // ...
};

// AFTER (TODO marker 박제)
class CPlaneRenderer
{
public:
    // ★ RH-2 TODO: replace ID3D11Device* with IRHIDevice* (handle API)
    static unique_ptr<CPlaneRenderer> Create(
        ID3D11Device* pDevice,
        DX11Shader* pShader,
        DX11Pipeline* pPipeline);
    // ...
};
```

**예시 2** — `Client/Private/Scene/Scene_InGame.cpp` 의 `Get_RHIDevice()->GetContext()` 호출부 (현재 약 L2080, 실제 진입 전 grep 으로 정확 위치 재확인):

```cpp
// BEFORE
auto* pCtx = pGI->Get_RHIDevice()->GetContext();
pCtx->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);

// AFTER (TODO marker 박제)
// ★ RH-2 TODO: replace Get_RHIDevice()->GetContext() with Get_FrameCommandList()
auto* pCtx = pGI->Get_RHIDevice()->GetContext();
pCtx->IASetVertexBuffers(0, 1, &pVB, &stride, &offset);
```

**검증** (Goal-Driven):

```bash
# 1. TODO marker 카운트
grep -rn "★ RH-2 TODO" Engine/Public/ Client/ | wc -l
# 기대: ≥ 12 hit (9 파일 × 1 + Scene_InGame.cpp × 4 추가)

# 2. LoL 빌드 통과
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
# 기대: error 0 (deprecated warning 가능, 다음 W2 의 Legacy rename 까지 미발생)
```

**합격 게이트 (T2-W1)**:
- ✅ 9개 파일 grep `★ RH-2 TODO` hit ≥ 1 each
- ✅ Scene_InGame.cpp grep ≥ 4 hit
- ✅ LoL 빌드 통과 (error 0)

---

## 4. Week 2 박제 — Track 1 Step 2 + Track 2 RH-0 §2

### 4.1 Track 1 W2 — `Shaders/Mesh3D_PBR.hlsl` (신설)

**파일 경로**: `Shaders/Mesh3D_PBR.hlsl` (신설)

**전문**:

```hlsl
// =============================================================
// Mesh3D_PBR.hlsl
//   GGX BRDF 기반 정적 메쉬 PBR 셰이더.
//   기반: Mesh3D.hlsl (unlit) + BRDF_GGX.hlsli.
//   슬롯: b0=PerFrame(VP), b1=PerObject(World), b3=PerMaterial(PBR)
//   t0=Albedo, t1=Normal(opt), t2=MetallicRoughness(opt)
//   s0=Linear-Wrap
// =============================================================

#include "BRDF/BRDF_GGX.hlsli"

cbuffer PerFrame : register(b0)
{
    row_major matrix g_matViewProj;
    float3           g_vCameraWorld;
    float            _pad0;
    float3           g_vLightDirWorld;   // 정규화된 단방향 라이트 (W2 한정 — Forward+ 는 W3)
    float            _pad1;
    float3           g_vLightColor;
    float            g_fLightIntensity;
};

cbuffer PerObject : register(b1)
{
    row_major matrix g_matWorld;
    row_major matrix g_matWorldInvTranspose;
};

cbuffer PerMaterial : register(b3)
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
Texture2D    g_NormalMap            : register(t1);  // optional
Texture2D    g_MetallicRoughnessMap : register(t2);  // optional, R=Metallic, G=Roughness
SamplerState g_LinearWrap           : register(s0);

// -------------------------------------------------------------
// VS
// -------------------------------------------------------------
struct VSInput
{
    float3 pos      : POSITION;
    float3 nrm      : NORMAL;
    float2 uv       : TEXCOORD;
    float3 tan      : TANGENT;
};

struct VSOutput
{
    float4 posCS    : SV_POSITION;
    float3 posWS    : TEXCOORD0;
    float3 nrmWS    : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float3 tanWS    : TEXCOORD3;
};

VSOutput VS_Main(VSInput IN)
{
    VSOutput OUT;
    float4 posWS4 = mul(float4(IN.pos, 1.0f), g_matWorld);
    OUT.posWS = posWS4.xyz;
    OUT.posCS = mul(posWS4, g_matViewProj);
    OUT.nrmWS = normalize(mul(float4(IN.nrm, 0.0f), g_matWorldInvTranspose).xyz);
    OUT.tanWS = normalize(mul(float4(IN.tan, 0.0f), g_matWorld).xyz);
    OUT.uv    = IN.uv;
    return OUT;
}

// -------------------------------------------------------------
// PS
// -------------------------------------------------------------
float4 PS_Main(VSOutput IN) : SV_TARGET
{
    // 1. Sample maps
    float4 albedoSample = g_AlbedoMap.Sample(g_LinearWrap, IN.uv);
    clip(albedoSample.a - 0.05f);   // alpha cut (LoL 머티리얼 보존)

    float3 albedo = albedoSample.rgb * g_vAlbedoTint;

    // metallic / roughness: map 있으면 R/G 채널, 없으면 cbuffer 상수
    float2 mr = g_MetallicRoughnessMap.Sample(g_LinearWrap, IN.uv).rg;
    float  metallic  = (mr.r > 0.0f) ? mr.r : g_fMetallic;
    float  roughness = (mr.g > 0.0f) ? mr.g : g_fRoughness;
    roughness = clamp(roughness, 0.04f, 1.0f);  // floor 0.04 (NaN 방지)

    // 2. Normal (vertex normal 기본, normal map 있으면 TBN 변환)
    float3 N = normalize(IN.nrmWS);
    // (W2 단순화: normal map 사용 안 함. W6 후 TBN 정식화)

    // 3. View / Light
    float3 V = normalize(g_vCameraWorld - IN.posWS);
    float3 L = normalize(-g_vLightDirWorld);   // light dir = "from surface to light"

    // 4. BRDF
    float3 brdf = BRDF_CookTorrance(N, V, L, albedo, metallic, roughness);
    float3 radiance = g_vLightColor * g_fLightIntensity;

    // 5. Direct lighting
    float3 Lo = brdf * radiance;

    // 6. Ambient (W2 단순화: 상수, IBL 은 Stage 2 = W6+)
    float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * g_fAmbientOcclusion;

    // 7. Emissive
    float3 emissive = g_vEmissiveTint * g_fEmissiveIntensity;

    float3 color = ambient + Lo + emissive;

    // 8. Tone map (W2 단순화: Reinhard. ACES 는 W6+)
    color = color / (color + float3(1.0f, 1.0f, 1.0f));

    // 9. Gamma (W2 단순화: 2.2. sRGB Render Target 은 W6+)
    color = pow(saturate(color), 1.0f / 2.2f);

    return float4(color, albedoSample.a);
}
```

**합격 게이트 (T1-W2)**:
- ✅ Mesh3D_PBR.hlsl 컴파일 통과 (fxc/dxc)
- ✅ 이렐리아 1체에 PBR 머티리얼 적용 + ChampionTuner 슬라이더 metallic/roughness 0~1 실시간 변화
- ✅ Frame time ≤17ms (현재 ~9ms 기준 +8ms 허용)
- ✅ albedo alpha clip 동작 (LoL 머티리얼 invisible 영역 보존)

---

### 4.2 Track 1 W2 — `Engine/Public/Renderer/CBPerMaterial.h` (신설)

**파일 경로**: `Engine/Public/Renderer/CBPerMaterial.h` (신설)

**전문**:

```cpp
#pragma once
//=============================================================
// CBPerMaterial.h
//   PBR 머티리얼 cbuffer (b3) POD struct.
//   HLSL slot: register(b3)
//   Shader 측 정의: Shaders/Mesh3D_PBR.hlsl 의 PerMaterial cbuffer.
//=============================================================

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

namespace Engine
{
    // 64 bytes (16-byte aligned, cbuffer 호환)
    struct WINTERS_ENGINE CBPerMaterial
    {
        f32_t vAlbedoTint[3];        // 0  : 12
        f32_t fMetallic;             // 12 : 16
        f32_t fRoughness;            // 16 : 20
        f32_t fAmbientOcclusion;     // 20 : 24
        f32_t fEmissiveIntensity;    // 24 : 28
        f32_t _matPad0;              // 28 : 32
        f32_t vEmissiveTint[3];      // 32 : 44
        f32_t _matPad1;              // 44 : 48
        f32_t _matReserved[4];       // 48 : 64  (PBR 확장 — clearcoat / anisotropy 등)
    };
    static_assert(sizeof(CBPerMaterial) == 64, "CBPerMaterial must be 64 bytes (cbuffer alignment)");

    // 디폴트 (비금속, 거칠기 0.5, AO 1.0)
    inline CBPerMaterial MakeDefaultPBRMaterial()
    {
        CBPerMaterial cb{};
        cb.vAlbedoTint[0] = 1.0f; cb.vAlbedoTint[1] = 1.0f; cb.vAlbedoTint[2] = 1.0f;
        cb.fMetallic = 0.0f;
        cb.fRoughness = 0.5f;
        cb.fAmbientOcclusion = 1.0f;
        cb.fEmissiveIntensity = 0.0f;
        cb.vEmissiveTint[0] = 0.0f; cb.vEmissiveTint[1] = 0.0f; cb.vEmissiveTint[2] = 0.0f;
        return cb;
    }
}
```

**합격 게이트**:
- ✅ `static_assert(sizeof(CBPerMaterial) == 64)` 통과
- ✅ `DX11ConstantBuffer<CBPerMaterial>::Create(pDevice)` 컴파일 통과 (기존 템플릿 활용)

---

### 4.3 Track 1 W2 — `Engine/Public/Renderer/CMaterialPBR.h` (신설, 스켈레톤)

**파일 경로**: `Engine/Public/Renderer/CMaterialPBR.h` (신설)

**전문**:

```cpp
#pragma once
//=============================================================
// CMaterialPBR.h
//   PBR 머티리얼 클래스. Albedo / Normal / MetallicRoughness 텍스처 보유 +
//   CBPerMaterial 상수 버퍼 1개. Mesh3D_PBR 셰이더와 1:1 매칭.
//
//   ★ RH-2 TODO: ID3D11Device* → IRHIDevice* / DX11ConstantBuffer → IRHIBuffer
//=============================================================

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "CBPerMaterial.h"
#include <wrl/client.h>
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

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

        void SetMetallic(f32_t v)            { m_CB.fMetallic = v;          m_bDirty = true; }
        void SetRoughness(f32_t v)           { m_CB.fRoughness = v;         m_bDirty = true; }
        void SetAmbientOcclusion(f32_t v)    { m_CB.fAmbientOcclusion = v;  m_bDirty = true; }
        void SetEmissiveIntensity(f32_t v)   { m_CB.fEmissiveIntensity = v; m_bDirty = true; }
        void SetAlbedoTint(f32_t r, f32_t g, f32_t b)
        {
            m_CB.vAlbedoTint[0] = r; m_CB.vAlbedoTint[1] = g; m_CB.vAlbedoTint[2] = b;
            m_bDirty = true;
        }
        void SetEmissiveTint(f32_t r, f32_t g, f32_t b)
        {
            m_CB.vEmissiveTint[0] = r; m_CB.vEmissiveTint[1] = g; m_CB.vEmissiveTint[2] = b;
            m_bDirty = true;
        }

        // ★ RH-2 TODO: replace ID3D11DeviceContext* with IRHICommandList*
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

**합격 게이트**:
- ✅ `CMaterialPBR::Create(pDevice)` 호출 시 CBPerMaterial 64바이트 cbuffer 정상 생성
- ✅ `Bind(pCtx)` 호출 시 b3 슬롯에 cbuffer 바인딩 + t0/t1/t2 슬롯에 텍스처 바인딩
- ✅ Dirty flag 동작 (Set 호출 시에만 cbuffer Map/Unmap)

---

### 4.4 Track 2 W2 — RH-0 §2: `Engine/Include/GameInstance.h` Legacy rename diff

**파일 경로**: `Engine/Include/GameInstance.h` (수정)

**대상 라인**: L101-108 (현재 8개 RHI 게터)

**수정 BEFORE → AFTER (diff)**:

```cpp
// =====================
// BEFORE (L101-108)
// =====================
class WINTERS_ENGINE CGameInstance
{
public:
    CDX11Device*       Get_RHIDevice();
    DX11Shader*        Get_MeshShader();
    DX11Pipeline*      Get_MeshPipeline();
    CBlendStateCache*  Get_BlendStateCache();
    DX11Shader*        Get_FxSpriteShader();
    DX11Pipeline*      Get_FxSpritePipeline();
    DX11Shader*        Get_FxMeshShader();
    DX11Pipeline*      Get_FxMeshPipeline();
    // ... 외 메서드
};

// =====================
// AFTER (RH-0 §2)
// =====================
class WINTERS_ENGINE CGameInstance
{
public:
    // ★ RH-0 §2: 기존 8개 게터를 _Legacy 접미사로 rename.
    //   Track 1 (PBR) 코드는 _Legacy 호출로 자동 호환됨.
    //   RH-1 후 신규 Get_NewRHIDevice() -> IRHIDevice* 추가.
    //   RH-2 종료 후 _Legacy 8개 제거 + Get_RHIDevice() 정식 rename.
    [[deprecated("RH-2 후 IRHIDevice* 사용. .md/plan/rhi/00_RHI_MIGRATION_MASTER.md 참조")]]
    CDX11Device*       Get_RHIDevice_Legacy();
    [[deprecated("RH-2 후 IRHIShader* 사용")]]
    DX11Shader*        Get_MeshShader_Legacy();
    [[deprecated("RH-2 후 IRHIPipelineState* 사용")]]
    DX11Pipeline*      Get_MeshPipeline_Legacy();
    [[deprecated("RH-2 후 IRHIBlendState* 사용")]]
    CBlendStateCache*  Get_BlendStateCache_Legacy();
    [[deprecated("RH-2 후 IRHIShader* 사용")]]
    DX11Shader*        Get_FxSpriteShader_Legacy();
    [[deprecated("RH-2 후 IRHIPipelineState* 사용")]]
    DX11Pipeline*      Get_FxSpritePipeline_Legacy();
    [[deprecated("RH-2 후 IRHIShader* 사용")]]
    DX11Shader*        Get_FxMeshShader_Legacy();
    [[deprecated("RH-2 후 IRHIPipelineState* 사용")]]
    DX11Pipeline*      Get_FxMeshPipeline_Legacy();
    // ... 외 메서드
};
```

**대응 .cpp (`Engine/Private/GameInstance.cpp`)**: 8개 정의를 `_Legacy` 접미사로 rename. 본문은 동일.

**Week 2 동반 체크리스트 (엔진 측 필수)**:
- `CBPerFrame` 에 `g_vCameraWorld`, `g_vLightDirWorld`, `g_vLightColor`, `g_fLightIntensity` 대응 필드 추가
- `CBPerObject` 에 `g_matWorldInvTranspose` 대응 필드 추가
- `ModelRenderer` 의 cbuffer update 경로에서 위 필드를 실제로 채움
- `Mesh3D_PBR.hlsl` 도입은 셰이더 교체만이 아니라 C++ cbuffer 계약 변경까지 포함함
- 이렐리아 PBR 적용 시 ChampionTuner 와 ModelRenderer 의 머티리얼 인스턴싱 경로도 동시 수정 필요

**caller 마이그**: 9개 leak consumer + Track 1 의 신규 코드도 모두 `Get_RHIDevice()` → `Get_RHIDevice_Legacy()` 로 일괄 sed/replace.

```bash
# Windows PowerShell:
Get-ChildItem -Path Engine,Client -Recurse -Include *.cpp,*.h |
  ForEach-Object {
    (Get-Content $_.FullName) -replace 'Get_RHIDevice\(\)', 'Get_RHIDevice_Legacy()' |
    Set-Content $_.FullName
  }

# 또는 bash + rg + sed:
rg -l "Get_RHIDevice\(\)" Engine Client | xargs sed -i 's/Get_RHIDevice()/Get_RHIDevice_Legacy()/g'
```

(나머지 7개 게터도 동일 패턴)

**합격 게이트 (T2-W2)**:
- ✅ LoL 빌드 통과 (deprecated warning 다수, error 0)
- ✅ `grep "Get_RHIDevice()" Engine Client` hit 0 (모두 `_Legacy` 로 치환)
- ✅ Track 1 의 PBR 코드 (`CMaterialPBR::Create`) 도 `Get_RHIDevice_Legacy()` 호출로 빌드 통과

---

## 5. Week 3~17 후속 박제 (요약 + 외화 링크)

### 5.1 Week 3 — Track 1 Forward+ Light Cull + Track 2 RH-1

#### Track 1 W3
- 진입 문서: `.md/plan/graphics/GGX+A/04_FORWARD_PLUS_LIGHT_CULLING.md`
- 신설: `Shaders/LightCull/` 폴더, `LightCullCS.hlsl` (16×16 px tile compute)
- StructuredBuffer: `LightList` (전 라이트), `TileLightIndex` (per-tile 라이트 인덱스)
- 합격: 64 동적 라이트 Frame ≤16ms

#### Track 2 W3
- 진입 문서: `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md §6.2 RH-1 합격`
- 신설: `Engine/Public/RHI/IRHIDevice.h` 등 9 인터페이스 (`IRHIDevice / IRHISwapChain / IRHIQueue / IRHIBuffer / IRHITexture / IRHIShader / IRHISampler` + `RHITypes.h` + `RHIDescriptors.h`)
- DX11 어댑터: `CDX11Device : public IRHIDevice` 다중 상속 (단일 cpp 에서 구현)
- handle API: `device->CreateBuffer(desc) -> RHIBufferHandle`
- 새 게터: `CGameInstance::Get_NewRHIDevice() -> IRHIDevice*`

### 5.2 Week 4~5 — Track 1 SSAO + Track 2 RH-2

#### Track 1 W4
- 진입 문서: `.md/plan/graphics/Graphics/03_TAA_WINTERS.md` 일부 (depth/normal 파이프라인 공유) + GTAO 별도 .md (필요 시 신설)
- 알고리즘: HBAO+ (NVIDIA 2008) 또는 GTAO (Activision 2018)
- 입력: depth buffer + per-pixel normal
- W4 임시: 별도 normal RT (G-Buffer 미존재) → W6 후 정식 RenderPass
- 합격: 깊이 차폐 시각 + Frame ≤20ms

#### Track 2 W4-5 (RH-2)
- 진입 문서: `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md §6.3 RH-2 합격`
- 9 leak consumer 마이그: `ID3D11Device*` → `IRHIDevice*`, `ID3D11DeviceContext*` → `IRHICommandList*`
- 파일 이동: `Engine/Public/RHI/DX11/` → `Engine/Private/RHI/DX11/`, `Engine/Public/RHI/CDX11Device.h` → `Engine/Private/RHI/DX11/DX11Device.h`
- `Get_NewRHIDevice()` → `Get_RHIDevice()` 정식 rename + `_Legacy` 8개 제거
- 합격: `grep "ID3D11Device\|d3d11.h\|RHI/DX11" Engine/Public` hit 0

### 5.3 Week 6 — Track 1 IRHI 마이그 + Track 2 RH-3/RH-4

#### Track 1 W6
- CBPerMaterial → IRHIBuffer (RH-1 handle API 통과)
- CTexture → IRHITexture
- Mesh3D_PBR.hlsl → DXC 컴파일 (DX11/12/VK 모두 호환)
- 합격: PBR 셰이더가 IRHI 통과해서 동작 (DX11 단계, DX12 는 W7+)

#### Track 2 W6 (RH-3 + RH-4)
- 진입 문서: `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md §2 Phase Overview`
- RH-3: PSO + RenderPass + BindGroup 인터페이스 (`IRHIPipelineState / IRHIRenderPass / IRHIBindGroup / IRHIBindGroupLayout`)
- RH-4: 64-bit handle (32 index + 32 generation) + `CRHIResourceTable` thread-safety
- 합격: 새 셰이더 추가 시 PSO 1개로 끝, BindGroup immutable

### 5.4 Week 7~13 — Track 2 RH-5 DX12

- 진입 문서: `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md §2 (RH-5)`
- 신설: `Engine/Private/RHI/DX12/*` 전체 (`DX12Device.h/.cpp` 등)
- 외부 라이브러리 편입: D3D12MA (Memory Allocator)
- PSO 캐시: `ID3D12PipelineLibrary` 디스크 저장/로드
- 합격 (compile-only, W7-9): DX12 빌드 통과
- 합격 (visual parity, W10-13): LoL 시각 결과 동일 (frame diff < 1px)

### 5.5 Week 14~17 (선택) — Track 2 RH-6 Vulkan

- 진입 문서: `.md/plan/rhi/00_RHI_MIGRATION_MASTER.md §2 (RH-6)`
- 신설: `Engine/Private/RHI/Vulkan/*` 전체
- 외부 라이브러리: VMA (Vulkan Memory Allocator), DXC SPIR-V cross-compile
- Validation layer: `vkEnumerateInstanceLayerProperties` 사전 체크 + fallback
- 합격: VK 빌드 + 시각 결과 동일 + validation 0 error/warning

---

## 6. 위험 시나리오 + 완화책

### 6.1 R-1: Track 1 PBR 시각 미달성 (W2)

- **시나리오**: 이렐리아 PBR 적용했는데 unlit 보다 어둡거나 metallic 슬라이더 변화 미미
- **원인**: NoL clamp 누락 / F0 잘못 / gamma 2번 적용 / SDR clip
- **완화**:
  1. 단위 테스트 — roughness=0.1 metallic=1.0 albedo=(1,0.86,0.57) (gold) 하드코딩 → spec 강한 골드 반사 시각 확인
  2. `clip(albedoSample.a - 0.05f)` 디버그 비활성 → 알파 0 영역 검증
  3. RenderDoc 으로 cbuffer b3 값 검사 (CBPerMaterial 64바이트 정상 매핑)
  4. Tone map 일시 제거 → linear color 직출력해서 BRDF 결과 확인

### 6.2 R-2: Track 2 RH-0 Legacy rename 후 빌드 깨짐 (W2)

- **시나리오**: 8 게터 rename + caller 일괄 치환했는데 컴파일 에러
- **원인**: PowerShell sed 가 partial match (`Get_RHIDevice2()` 같은 가상 메서드 잘못 치환) / `[[deprecated]]` 가 일부 컴파일러에서 error 처리
- **완화**:
  1. sed/replace 전 `grep "Get_RHIDevice"` 로 정확한 출현 갯수 카운트 후 dry-run
  2. `[[deprecated]]` 대신 주석만 사용 (warning 제거 후 RH-2 시점에 정식 추가)
  3. 한 번에 하나씩 게터 마이그 (일괄 X) — 8 단계로 분할

### 6.3 R-3: Track 1 W3 Forward+ Compute 가 RH-1 인터페이스와 race (W3)

- **시나리오**: T1 의 ComputeShader 가 T2 RH-1 의 IRHIComputePipeline 미존재 시 컴파일 fail
- **원인**: T1 W3 진입 시 T2 RH-1 미완료 가능
- **완화**:
  1. T1 W3 는 DX11 ComputeShader API 직접 사용 (RH-1 없이 진행)
  2. T2 RH-1 완료 후 W6 에서 IRHIComputePipeline 통과로 마이그
  3. 또는 T1 W3 를 1주 미루고 T2 RH-1 완료 대기 (병합 17주 → 18주)

### 6.4 R-4: RH-2 Public DX11 헤더 제거 후 ImGui DX11 backend 깨짐 (W5)

- **시나리오**: ImGui_ImplDX11_Init 가 raw `ID3D11Device*` 요구 → Public 에서 d3d11.h 제거 시 깨짐
- **원인**: ImGui DX11 backend 는 외부 라이브러리, RHI 인터페이스 미통과
- **완화**:
  1. `IRHIDevice::GetNativeHandle(NativeType::DX11Device)` 명시 escape API 추가 (`borrowed pointer` 정책)
  2. ImGui backend 만 escape 사용, 그 외 코드는 인터페이스 통과
  3. RH-1 §3 RHIDescriptors.h 에 `enum class eNativeType { DX11Device, DX11Context, DX12Device, DX12CommandQueue, VulkanDevice, ... }` 정식화

### 6.5 R-5: DX12 Visual Parity 미달성 (W10-13)

- **시나리오**: DX12 빌드 통과하지만 LoL 시각 결과가 DX11 과 다름 (frame diff > 1px)
- **원인**: barrier 누락 / RTV 클리어 색 다름 / cbuffer alignment / sRGB 처리 다름
- **완화**:
  1. PIX 또는 RenderDoc 으로 DX11 vs DX12 frame capture 비교 (1 draw 단위)
  2. PIX 의 `D3D12_VALIDATION` 활성 — barrier 누락 즉시 검출
  3. RH-2 §4.1 DX11 의 명시적 barrier 호출 (no-op 이지만 통과 자체는 강제) — 미리 race 검출

### 6.6 R-6: Vulkan validation layer 부재 (W14-17)

- **시나리오**: Vulkan SDK 미설치 시 validation layer 미존재 → 디버깅 불가
- **원인**: Windows 에서 Vulkan SDK 별도 설치 필요
- **완화**:
  1. `vkEnumerateInstanceLayerProperties` 로 사전 체크 + fallback (validation 없으면 그냥 진행)
  2. CI 에서 Vulkan SDK 설치 자동화
  3. RH-6 진입 전 환경 매트릭스 (Vulkan SDK 1.3.x 필수)

---

## 7. 검증 게이트 (전체)

### 7.1 Track 1 게이트

| Stage | 게이트 |
|---|---|
| W1 GGX BRDF | hlsli 컴파일 통과 + 임시 호출 통과 |
| W2 PBR | 이렐리아 시각 변화 + Frame ≤17ms + alpha clip 동작 |
| W3 Forward+ | 64 동적 라이트 ≤16ms |
| W4 SSAO | 깊이 차폐 시각 + Frame ≤20ms |
| W5 안정화 | T1 회귀 0 |
| W6 IRHI 마이그 | PBR 셰이더가 IRHI 통과 |

### 7.2 Track 2 게이트

| Stage | 게이트 |
|---|---|
| W1 RH-0 §1 | 9 파일 TODO marker + LoL 빌드 통과 |
| W2 RH-0 §2 | 8 게터 _Legacy + caller hit 0 |
| W3 RH-1 | 9 인터페이스 + handle API + IRHIDevice* 통과 |
| W4-5 RH-2 | Public d3d11.h hit 0 + 9 leak consumer 마이그 |
| W6 RH-3+RH-4 | PSO 1개 + 64-bit handle |
| W7-9 RH-5 compile-only | DX12 빌드 통과 |
| W10-13 RH-5 visual parity | DX12 frame diff < 1px + PSO 캐시 |
| W14-17 RH-6 (선택) | VK 빌드 + DXC SPIR-V + validation 0 |

### 7.3 통합 게이트

- W2 후: 이렐리아 PBR DX11 동작 + Track 2 deprecated warning 만 (error 0)
- W6 후: 이렐리아 PBR + Forward+ + SSAO 가 IRHI 통과 (DX11 동작, DX12 는 W7+)
- W13 후: DX11 + DX12 양쪽에서 동일 시각 결과
- W17 후 (선택): DX11 + DX12 + Vulkan 3 백엔드 동일 시각 결과

---

## 8. 외부 자료 (참고 + 라이센스)

### 8.1 PBR / GGX

- **Real-Time Rendering 4th Ed.** (Akenine-Möller et al., 2018) — Ch.9 Physically Based Shading 핵심.
- **Disney Principled BSDF** (Burley 2012) — `https://blog.selfshadow.com/publications/s2012-shading-course/burley/s2012_pbs_disney_brdf_notes_v3.pdf`
- **UE4 Karis 2013** — `https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf`
- **Frostbite Lagarde 2014** — Moving Frostbite to PBR.
- **GPU Gems 3 Ch.20 GPU-Based Importance Sampling** (NVIDIA, 무료 공개) — IBL prefilter 참고.

### 8.2 Forward+

- **Harada-McKee-Yang 2012** — "Forward+ : Bringing Deferred Lighting to the Next Level".
- **Olsson-Billeter-Assarsson 2012** — "Tiled and Clustered Forward Shading".
- **Doom 2016 (idTech 6)** — Tiled Forward + Cluster Shading 산업 사례.

### 8.3 SSAO

- **HBAO+** (NVIDIA 2008) — `https://developer.nvidia.com/sites/default/files/akamai/gameworks/samples/DeinterleavedTexturing.pdf`
- **GTAO** (Jimenez et al. 2018, Activision) — `https://www.activision.com/cdn/research/PracticalRealtimeStrategiesTRTermFiltering_compressed.pdf`

### 8.4 RHI 멀티 백엔드 (오픈소스 참조)

- **NVIDIA NVRHI** — `https://github.com/NVIDIAGameWorks/nvrhi` (DX12/VK, 산업 표준 RHI 참조용. MIT)
- **The-Forge** — `https://github.com/ConfettiFX/The-Forge` (DX12/VK/Metal 멀티, Apache 2.0)
- **Diligent Engine** — `https://github.com/DiligentGraphics/DiligentEngine` (DX11/12/VK/Metal/WebGPU, Apache 2.0)
- **Sokol-gfx** — `https://github.com/floooh/sokol` (DX11 기준, slot 기반 한계 — 반면교사)
- **WebGPU/Wgpu** — `https://gpuweb.github.io/gpuweb/` (사양 자체. RHI 인터페이스 설계 참고)

### 8.5 GPU Zen / 언리얼 오픈소스

- **GPU Zen 1 (2017) / GPU Zen 2 (2019)** — Wolfgang Engel 편집. PBR/Compute/Light Cull 실전 챕터.
- **Unreal Engine 5 소스** (가입 필요) — `Source/Runtime/RHI/`, `Source/Runtime/D3D12RHI/`, `Source/Runtime/VulkanRHI/`. RHI 인터페이스 설계 + barrier 처리 + PSO 캐시 산업 레벨 구현.
- **Doom 3 BFG GPL 소스** — `https://github.com/id-Software/DOOM-3-BFG` (구식이지만 idTech4 RHI 패턴 참고).

### 8.6 DX12 / Vulkan 라이브러리

- **D3D12 Memory Allocator (D3D12MA)** — `https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator` (MIT, AMD)
- **Vulkan Memory Allocator (VMA)** — `https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator` (MIT, AMD)
- **DXC** — `https://github.com/microsoft/DirectXShaderCompiler` (MIT, Microsoft. HLSL → DXIL/SPIR-V cross-compile)
- **spirv-cross** — `https://github.com/KhronosGroup/SPIRV-Cross` (Apache 2.0, Khronos. SPIR-V → HLSL/MSL/GLSL)

---

## 9. ThirdPartyLib 편입 (RH-5 이전 준비)

| 라이브러리 | 라이센스 | 편입 위치 | 필요 시점 |
|---|---|---|---|
| D3D12MA | MIT | `Engine/ThirdPartyLib/D3D12MA/` | RH-5 W7 |
| VMA | MIT | `Engine/ThirdPartyLib/VMA/` | RH-6 W14 |
| DXC | MIT | `Engine/ThirdPartyLib/DXC/` | RH-1 W3 (DXC 도입) |
| spirv-cross | Apache 2.0 | `Engine/ThirdPartyLib/spirv-cross/` | RH-6 W14 |

편입 절차: `.md/build/THIRDPARTY_INTEGRATION_GUIDE.md` 참조.

---

## 10. 한 줄

> **17주 병합 = Track 1 (GGX BRDF + PBR + Forward+ + SSAO, W1-6) + Track 2 (RH-0~RH-6 멀티 백엔드, W1-17). W1-2 충돌 0 (셰이더/inventory 만), W3-5 점진 합치, W6 Track 1 IRHI 통과, W7-13 DX12 visual parity, W14-17 Vulkan (선택). 사용자 즉시 진입: Week 1 Track 1 BRDF_GGX.hlsli 박제 + Track 2 RH-0 §1 9 leak consumer TODO marker 박제 — 두 작업 모두 충돌 0, 빌드 영향 0.**

---

## 부록 A — 즉시 진입 체크리스트 (Week 1)

```
[ ] Visual Studio (devenv.exe) 종료 (vc143.pdb lock 회피)
[ ] git: feature/2026-05-01-twin-track branch 생성
[ ] Engine 단독 빌드 1회 → EngineSDK/inc 동기화

Track 1 W1:
[ ] mkdir Shaders/BRDF/
[ ] Write Shaders/BRDF/BRDF_GGX.hlsli (본 §3.1 전문)
[ ] Mesh3D.hlsl 끝에 임시 #include + 호출 → 컴파일 통과 확인
[ ] 임시 코드 제거

Track 2 W1:
[ ] grep -rn "ID3D11Device\|d3d11.h\|RHI/DX11" Engine/Public/ Client/ → 9 파일 + Scene_InGame 12 hit 확인
[ ] 9 파일에 // ★ RH-2 TODO 주석 박제 (본 §3.2 패턴)
[ ] grep -rn "★ RH-2 TODO" Engine Client | wc -l → ≥12 hit 확인
[ ] LoL 빌드 통과 확인 (error 0)

검증 (W1 통합):
[ ] Frame time 회귀 0 (BRDF.hlsli 미사용 시 영향 0)
[ ] LoL 게임 동작 확인 (이렐리아 unlit 그대로)
```

---

## 끝.
