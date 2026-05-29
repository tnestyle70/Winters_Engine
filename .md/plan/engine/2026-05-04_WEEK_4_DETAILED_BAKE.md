# 2026-05-04 Week 4 상세 박제 — Track 1 SSAO + Track 2 RH-2 시작

**작성일**: 2026-05-04
**상태**: 검토 대기 (계획서만 작성, 코드 변경은 codex 가 진행 / 작성자 후속 검토)
**전제**: Week 3 (Forward+ + RH-1 9 인터페이스) 완료
**상위 문서**: [Twin Track 계획서 §5.2](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md), [RHI 마스터 §6.3 RH-2 합격](../rhi/00_RHI_MIGRATION_MASTER.md)

---

## 0. 한 줄

> **Week 4 = T1 (GTAO depth/normal 기반 SSAO + ambient 통합) + T2 (RH-2 시작 — 9 leak consumer 마이그 to IRHIDevice/Buffer/Texture). 합격: SSAO 시각 차폐 + Frame ≤20ms + Public 헤더에서 ID3D11Device* 노출 9 → 0 (W5 에서 헤더 이동 완료).**

---

> **Dependency note (2026-05-02):** this bake assumes the corrected Week 3 baseline, not the original full compute/UAV Forward+ path. Revalidate RH-2 scope against the actual RH-1 seed before touching public DX11 consumers.

## 1. Week 3 결과 검증 (Week 4 진입 전)

```bash
# 1. Forward+ 인프라
ls Shaders/LightCull/LightCullCS.hlsl Engine/Public/Renderer/{LightData,LightCullSystem}.h

# 2. RHI 9 인터페이스
ls Engine/Public/RHI/{IRHIDevice,IRHIBuffer,IRHITexture,IRHIShader,IRHISampler,IRHISwapChain,IRHIQueue,RHITypes,RHIDescriptors,RHIHandles}.h

# 3. CDX11Device 다중 상속
grep -n "class.*CDX11Device.*public.*IRHIDevice" Engine/Public/RHI/CDX11Device.h

# 4. Get_NewRHIDevice() 동작
grep -n "Get_NewRHIDevice" Engine/Include/GameInstance.h Engine/Private/GameInstance.cpp

# 5. 빌드 + 런타임 64 라이트 ≤16ms
```

---

## 2. Week 4 작업 매트릭스

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T1.1** | SSAO 알고리즘 결정 (GTAO) | 문서만 | (없음) |
| **T1.2** | Normal RT 신설 (W4 임시 — G-Buffer 없으므로 별도 RT) | `Engine/Public/Renderer/NormalPass.h/.cpp` | (W3) |
| **T1.3** | Mesh3D_PBR / Skinned3D_PBR 가 NormalRT 도 출력 (MRT) | 셰이더 변경 | T1.2 |
| **T1.4** | SSAO compute shader (GTAO) | `Shaders/SSAO/GTAO_CS.hlsl` | T1.2 |
| **T1.5** | SSAO blur (cross bilateral) | `Shaders/SSAO/GTAO_Blur_CS.hlsl` | T1.4 |
| **T1.6** | CSSAOPass 신설 | `Engine/Public/Renderer/SSAOPass.h/.cpp` | T1.4, T1.5 |
| **T1.7** | PBR PS ambient 곱: `ambient * AO` | Mesh3D_PBR / Skinned3D_PBR | T1.6 |
| **T1.8** | ChampionTuner SSAO 강도 슬라이더 + on/off | UI | T1.6 |
| **T2.1** | ModelRenderer 마이그 (`ID3D11Device*` → `IRHIDevice*`) | `Engine/Private/Renderer/ModelRenderer.cpp` | (W3 RH-1) |
| **T2.2** | PlaneRenderer 마이그 | `Engine/Public/Renderer/PlaneRenderer.h` + `.cpp` | (W3) |
| **T2.3** | Mesh.h / Model.h / Texture.h / ResourceCache.h 마이그 | 헤더에서 d3d11.h 제거 | (W3) |
| **T2.4** | UI_Manager.h 마이그 + ImGui escape (`GetNativeHandle`) | `Engine/Public/Manager/UI/UI_Manager.h` | (W3) |
| **T2.5** | FxBillboardComponent / FxMeshComponent / FxSystem 마이그 | `Client/Public/GameObject/FX/*` | (W3) |
| **T2.6** | Engine_Defines.h / CEngineApp.h 의 `RHI/DX11/...` 직접 include 제거 | 헤더 수정 | T2.1~T2.5 |
| **T2.7** | 이번 W4 시점 잔여 노출 inventory | 문서 | T2.1~T2.6 |

**병렬**: T1 (T1.1~T1.8) 과 T2 (T2.1~T2.7) 는 독립.

---

## 3. Track 1 — GTAO (Activision 2018)

### 3.1 GTAO 개요

**GTAO** (Ground-Truth-Based Ambient Occlusion, Jimenez et al. 2018):
- HBAO+ 보다 ground-truth 에 가까운 결과
- Compute shader 기반 (24~32 sample / pixel)
- Multi-bounce approximation 포함
- 최종: `AO = pow(occlusion, intensity) * multiBounce`

### 3.2 NormalPass (W4 임시 G-Buffer 없음 대응) (T1.2)

**파일**: `Engine/Public/Renderer/NormalPass.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <memory>

namespace Engine
{
    class IRHIDevice;
    class CDX11Texture2D;

    // W4 임시 — W6 정식 G-Buffer 진입 후 제거 가능.
    // depth + per-pixel world-space normal 만 필요.
    class WINTERS_ENGINE CNormalPass
    {
    public:
        ~CNormalPass();
        CNormalPass(const CNormalPass&) = delete;
        CNormalPass& operator=(const CNormalPass&) = delete;
        CNormalPass(CNormalPass&&) = default;
        CNormalPass& operator=(CNormalPass&&) = default;

        static std::unique_ptr<CNormalPass> Create(IRHIDevice* pDevice, u32_t w, u32_t h);

        // 호출 시 NormalRT 를 RTV 로 바인딩 + DepthBuffer 클리어
        void Begin(/* IRHICommandList* */);
        void End(/* IRHICommandList* */);

        // SSAO 가 t0=Depth, t1=Normal SRV 로 사용
        void* GetDepthSRVNative();
        void* GetNormalSRVNative();

    private:
        CNormalPass();
        struct Impl; std::unique_ptr<Impl> m_pImpl;
    };
}
```

### 3.3 Mesh3D_PBR 의 MRT 출력 (T1.3)

**변경**: PS 가 RTV0 (color) + RTV1 (normal) 동시 출력.

```hlsl
struct PS_OUTPUT {
    float4 color  : SV_TARGET0;
    float4 normal : SV_TARGET1;   // RGB = world normal (0~1 range), A = 1
};

PS_OUTPUT PS(PS_INPUT input)
{
    PS_OUTPUT output;
    // (기존 PBR 로직 그대로)
    output.color = float4(color, albedoSample.a);

    // Normal 출력 (world normal, 0.5 + 0.5 * N 로 [0,1] 범위)
    output.normal = float4(input.vNormalWS * 0.5f + 0.5f, 1.0f);

    return output;
}
```

### 3.4 GTAO_CS.hlsl (T1.4)

**파일**: `Shaders/SSAO/GTAO_CS.hlsl` (신설)

```hlsl
// =============================================================
// GTAO_CS.hlsl — Ground-Truth-Based Ambient Occlusion
//   Activision 2018, Jimenez et al.
//   Sample count: 16 horizontal + 16 vertical = 32 / pixel
// =============================================================

static const float PI = 3.14159265359f;

cbuffer CBGTAO : register(b0) {
    row_major matrix g_matViewProj;
    row_major matrix g_matViewProjInv;
    float2 g_vScreenSize;
    float  g_fRadius;            // world-space radius (default 0.5m)
    float  g_fIntensity;         // pow exponent (default 2.0)
    float  g_fThicknessHeuristic;// 0.0 = thin / 1.0 = thick
    float3 _pad;
};

Texture2D<float>     g_DepthBuffer  : register(t0);
Texture2D<float4>    g_NormalBuffer : register(t1);
RWTexture2D<float>   g_AOOutput     : register(u0);

SamplerState g_PointSampler : register(s0);

// Reconstruct world position from depth (linear)
float3 ReconstructWorldPos(float2 uv, float depth)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, g_matViewProjInv);
    return world.xyz / world.w;
}

[numthreads(8, 8, 1)]
void CS_Main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)g_vScreenSize.x || DTid.y >= (uint)g_vScreenSize.y) return;

    float2 uv = (float2(DTid.xy) + 0.5f) / g_vScreenSize;
    float  centerDepth  = g_DepthBuffer.Load(uint3(DTid.xy, 0));
    if (centerDepth >= 0.999999f) {  // sky
        g_AOOutput[DTid.xy] = 1.0f;
        return;
    }

    float3 centerPos    = ReconstructWorldPos(uv, centerDepth);
    float3 centerNormal = normalize(g_NormalBuffer.Load(uint3(DTid.xy, 0)).xyz * 2.0f - 1.0f);

    // GTAO core: integrate horizon angle in 4 directions
    const int NUM_DIRS    = 4;
    const int NUM_SAMPLES = 8;
    float occlusion = 0.0f;

    for (int dir = 0; dir < NUM_DIRS; ++dir) {
        float angle = (float)dir / NUM_DIRS * PI;
        float2 sliceDir = float2(cos(angle), sin(angle));

        // 양방향 horizon search
        float h1 = -1.0f;  // backward horizon cos
        float h2 = -1.0f;  // forward horizon cos

        for (int s = 1; s <= NUM_SAMPLES; ++s) {
            float t = (float)s / NUM_SAMPLES;
            float2 sampleUV = uv + sliceDir * t * (g_fRadius * 0.01f);

            float  sampleDepth = g_DepthBuffer.SampleLevel(g_PointSampler, sampleUV, 0);
            float3 samplePos   = ReconstructWorldPos(sampleUV, sampleDepth);
            float3 toSample    = samplePos - centerPos;
            float  dist        = length(toSample);

            if (dist < g_fRadius) {
                float3 sampleDir = toSample / dist;
                float  cosH      = dot(sampleDir, centerNormal);
                h2 = max(h2, cosH);
            }

            // backward direction
            sampleUV = uv - sliceDir * t * (g_fRadius * 0.01f);
            sampleDepth = g_DepthBuffer.SampleLevel(g_PointSampler, sampleUV, 0);
            samplePos = ReconstructWorldPos(sampleUV, sampleDepth);
            toSample  = samplePos - centerPos;
            dist      = length(toSample);

            if (dist < g_fRadius) {
                float3 sampleDir = toSample / dist;
                float  cosH      = dot(sampleDir, centerNormal);
                h1 = max(h1, cosH);
            }
        }

        // GTAO arc integral (간이)
        float arc = (acos(h1) + acos(h2)) * 0.5f;
        occlusion += arc / PI;
    }
    occlusion /= NUM_DIRS;
    occlusion = pow(saturate(occlusion), g_fIntensity);

    g_AOOutput[DTid.xy] = occlusion;
}
```

### 3.5 GTAO_Blur_CS.hlsl (T1.5)

**파일**: `Shaders/SSAO/GTAO_Blur_CS.hlsl` (신설, cross-bilateral 5×5)

```hlsl
Texture2D<float>     g_AOInput      : register(t0);
Texture2D<float>     g_DepthBuffer  : register(t1);
RWTexture2D<float>   g_AOOutput     : register(u0);

cbuffer CBBlur : register(b0) {
    float2 g_vScreenSize;
    float2 _pad;
};

[numthreads(8, 8, 1)]
void CS_Main(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= (uint)g_vScreenSize.x || DTid.y >= (uint)g_vScreenSize.y) return;

    float centerDepth = g_DepthBuffer.Load(uint3(DTid.xy, 0));
    float sumAO   = 0.0f;
    float sumWeight = 0.0f;

    [unroll] for (int dy = -2; dy <= 2; ++dy) {
        [unroll] for (int dx = -2; dx <= 2; ++dx) {
            int2 sampleCoord = int2(DTid.xy) + int2(dx, dy);
            sampleCoord = clamp(sampleCoord, int2(0, 0), int2(g_vScreenSize) - 1);

            float sampleDepth = g_DepthBuffer.Load(uint3(sampleCoord, 0));
            float depthDiff   = abs(sampleDepth - centerDepth);
            float weight      = exp(-depthDiff * 100.0f);

            float sampleAO = g_AOInput.Load(uint3(sampleCoord, 0));
            sumAO     += sampleAO * weight;
            sumWeight += weight;
        }
    }

    g_AOOutput[DTid.xy] = (sumWeight > 0.0f) ? (sumAO / sumWeight) : 1.0f;
}
```

### 3.6 CSSAOPass (T1.6)

**파일**: `Engine/Public/Renderer/SSAOPass.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <memory>

namespace Engine
{
    class IRHIDevice;

    class WINTERS_ENGINE CSSAOPass
    {
    public:
        ~CSSAOPass();
        CSSAOPass(const CSSAOPass&) = delete;
        CSSAOPass& operator=(const CSSAOPass&) = delete;

        static std::unique_ptr<CSSAOPass> Create(IRHIDevice* pDevice, u32_t w, u32_t h);

        void SetRadius(f32_t r)            { m_Radius = r;            m_bDirty = true; }
        void SetIntensity(f32_t i)         { m_Intensity = i;         m_bDirty = true; }
        f32_t GetRadius() const            { return m_Radius; }
        f32_t GetIntensity() const         { return m_Intensity; }

        // ★ RH-2 진행 중: IRHICommandList* 통과 (W6 정식)
        void Dispatch(/* IRHICommandList* */ void* pNativeCtx,
                      void* pDepthSRV, void* pNormalSRV,
                      const DirectX::XMFLOAT4X4& matViewProj);

        void* GetAOSRVNative();   // PS 측 t5 슬롯 등에 바인딩

    private:
        CSSAOPass();
        bool_t m_bDirty = true;
        f32_t  m_Radius = 0.5f;       // world-space (m)
        f32_t  m_Intensity = 2.0f;    // pow exponent
        u32_t  m_Width = 0;
        u32_t  m_Height = 0;
        struct Impl; std::unique_ptr<Impl> m_pImpl;
    };
}
```

### 3.7 PBR PS ambient × AO (T1.7)

```hlsl
// Mesh3D_PBR.hlsl + Skinned3D_PBR.hlsl PS

Texture2D<float> g_AOMap : register(t5);  // SSAO output

// PS_Main 끝부분 변경:
float ssao = g_AOMap.Load(uint3(input.vPosition.xy, 0));
float3 ambient = float3(0.03f, 0.03f, 0.03f) * albedo * g_fAmbientOcclusion * ssao;
//              ↑ cbuffer AO (머티리얼)              ↑ SSAO (스크린 스페이스)
```

### 3.8 ChampionTuner (T1.8)

```cpp
if (ImGui::CollapsingHeader("SSAO (GTAO)"))
{
    static bool bEnable = true;
    ImGui::Checkbox("Enable", &bEnable);
    f32_t radius = m_pSSAO->GetRadius();
    f32_t intensity = m_pSSAO->GetIntensity();
    if (ImGui::SliderFloat("Radius (m)", &radius, 0.1f, 5.0f)) m_pSSAO->SetRadius(radius);
    if (ImGui::SliderFloat("Intensity",  &intensity, 0.5f, 5.0f)) m_pSSAO->SetIntensity(intensity);
}
```

### 3.9 합격 게이트 (Track 1 W4)

- ✅ GTAO_CS.hlsl + GTAO_Blur_CS.hlsl 컴파일 통과
- ✅ NormalPass + SSAOPass 가 매 프레임 디스패치 (PIX 캡처로 UAV write 확인)
- ✅ 시각: 캐릭터/벽/구석 영역에 깊이 차폐 명확히 보임
- ✅ Frame ≤20ms (현 W3 기준 +4~5ms 허용)

---

## 4. Track 2 — RH-2 시작 (9 leak consumer 마이그)

### 4.1 ModelRenderer 마이그 (T2.1)

**파일**: `Engine/Private/Renderer/ModelRenderer.cpp` (수정)

```cpp
// BEFORE (W2)
auto* pContext = CEngineApp::Get().GetDevice().GetContext();
//               ↑ raw ID3D11DeviceContext*

// AFTER (W4 RH-2 진행 중)
//   ★ W4 임시: IRHICommandList* 미구현 (W6 RH-3) → escape API 로 native 호출
auto* pDevice = CGameInstance::Get()->Get_NewRHIDevice();
auto* pContext = (ID3D11DeviceContext*)pDevice->GetNativeHandle(eRHINativeType::DX11DeviceContext);
//   ↑ borrowed pointer, 즉시 사용만
```

### 4.2 PlaneRenderer 마이그 (T2.2)

**BEFORE** (`Engine/Public/Renderer/PlaneRenderer.h`):

```cpp
// ★ RH-2 TODO: replace ID3D11Device* with IRHIDevice* (handle API)
static unique_ptr<CPlaneRenderer> Create(
    ID3D11Device* pDevice,
    DX11Shader*   pShader,
    DX11Pipeline* pPipeline);
```

**AFTER**:

```cpp
//   RH-2 (W4): IRHIDevice* 인터페이스 통과.
//   기존 caller (Scene_InGame OnEnter 등) 도 함께 변경.
static unique_ptr<CPlaneRenderer> Create(
    IRHIDevice*   pDevice,
    DX11Shader*   pShader,         // ★ W6 RH-3 에서 IRHIShader 로 변경
    DX11Pipeline* pPipeline);      // ★ W6 RH-3 에서 IRHIPipelineState 로 변경
```

`PlaneRenderer.cpp` 의 `Create` 본문 변경:

```cpp
// BEFORE
auto* pNativeDevice = pDevice;   // raw

// AFTER
auto* pNativeDevice = (ID3D11Device*)pDevice->GetNativeHandle(eRHINativeType::DX11Device);
//   ↑ escape API, .cpp 안에서만 사용 (헤더에서 d3d11.h 제거)
```

### 4.3 Mesh.h / Model.h / Texture.h / ResourceCache.h 마이그 (T2.3)

**Mesh.h 패턴** (다른 3개도 동일):

```cpp
// BEFORE
#include <d3d11.h>
class CMesh
{
public:
    static unique_ptr<CMesh> Create(ID3D11Device* pDevice, /*...*/);
    // ...
};

// AFTER
//   ★ RH-2 (W4): d3d11.h include 제거. IRHIDevice 통과.
#include "RHI/IRHIDevice.h"
class CMesh
{
public:
    static unique_ptr<CMesh> Create(IRHIDevice* pDevice, /*...*/);
    // ...
};
```

`.cpp` 에서 escape:

```cpp
auto* pNative = (ID3D11Device*)pDevice->GetNativeHandle(eRHINativeType::DX11Device);
```

### 4.4 UI_Manager 마이그 + ImGui escape (T2.4)

**UI_Manager.h**:

```cpp
// BEFORE
class CUI_Manager
{
public:
    HRESULT Load_TextureSRV(const wstring_t& path, ID3D11ShaderResourceView** ppSRV);
    // ↑ raw ID3D11* leak
};

// AFTER (W4 RH-2)
class CUI_Manager
{
public:
    HRESULT Load_TextureSRV(const wstring_t& path, ID3D11ShaderResourceView** ppSRV);
    //   ↑ ImGui DX11 backend 호환 위해 raw 유지 (W6 RH-3 후 IRHITexture 로 변경)
    //   .cpp 에서 IRHIDevice::GetNativeHandle 로 device 획득
};
```

`UI_Manager.cpp`:

```cpp
HRESULT CUI_Manager::Load_TextureSRV(const wstring_t& path, ID3D11ShaderResourceView** ppSRV)
{
    auto* pDevice = CGameInstance::Get()->Get_NewRHIDevice();
    auto* pNativeDevice = (ID3D11Device*)pDevice->GetNativeHandle(eRHINativeType::DX11Device);
    // 기존 DirectXTK::CreateWICTextureFromFile 호출 (pNativeDevice + pContext)
}
```

### 4.5 FX 컴포넌트 3개 마이그 (T2.5)

**FxBillboardComponent / FxMeshComponent / FxSystem** (`Client/Public/GameObject/FX/*`):

```cpp
// BEFORE
// 헤더에서 ID3D11Device*, ID3D11DeviceContext*, ID3D11Buffer* 등 직접 노출

// AFTER
// 1. 헤더에서 d3d11.h include 제거
// 2. raw 멤버 → IRHIBuffer / IRHITexture handle 보유 (W4 임시 — handle 만 보유)
// 3. .cpp 에서 device->GetNativeHandle 로 backend 접근
```

### 4.6 Engine_Defines.h / CEngineApp.h 의 RHI/DX11/... include 제거 (T2.6)

```cpp
// BEFORE (Engine/Public/Engine_Defines.h)
#include "RHI/DX11/DX11VertexBuffer.h"
#include "RHI/DX11/DX11IndexBuffer.h"
// ...

// AFTER (W4 RH-2)
//   ★ Public 에서 DX11 헤더 직접 include 제거. Private cpp 에서만 include.
//   대신 public 인터페이스 사용:
#include "RHI/IRHIDevice.h"
#include "RHI/RHIHandles.h"
```

### 4.7 W4 시점 잔여 노출 inventory (T2.7)

```bash
# Public 헤더에서 ID3D11Device 노출 검색
rg "ID3D11Device|d3d11\\.h" Engine/Public/ Client/Public/ -l

# W4 끝 시점 기대: 9 → 0~2 (W5 에서 헤더 이동 + 0)
```

남는 항목 분류 → W5 작업 매트릭스에 박제.

### 4.8 합격 게이트 (Track 2 W4)

- ✅ 9 leak consumer 모두 IRHIDevice* 통과 (헤더에서 raw `ID3D11Device*` 제거 또는 escape 명시)
- ✅ LoL 빌드 통과 (deprecated warning 다수 OK)
- ✅ 게임 회귀 0 (이렐리아 PBR + Forward+ + SSAO 동작)

---

## 5. 위험 시나리오

### 5.1 R-W4-1: GTAO horizon search radius 부적절
- 시나리오: 0.5m radius 가 LoL 맵 스케일 (≈10m × 10m 챔프) 에서 너무 작음 / 큼
- 완화: ChampionTuner Radius 슬라이더로 0.1~5.0 m 범위 실시간 조정

### 5.2 R-W4-2: NormalRT 가 unlit 기존 셰이더에서 미출력
- 시나리오: PBR 셰이더만 SV_TARGET1 출력 → 기존 unlit (Mesh3D, Skinned3D, Default3D, FxMesh, FxSprite, Triangle) 가 NormalRT 에 garbage 쓰기
- 완화: ① RTV 바인딩 시 PBR 패스만 NormalRT bind ② unlit 패스는 RTV0 만 bind ③ NormalRT 는 PBR 직전 clear

### 5.3 R-W4-3: GetNativeHandle escape 남용
- 시나리오: 마이그 시 모든 코드가 escape 사용 → 인터페이스 의미 상실
- 완화: ① escape 호출은 cpp 안에서만 ② 헤더에서 노출 금지 ③ W6 RH-3 시점에 escape 사용 사이트 카운트 → 절반 이상 정상 인터페이스로 마이그 보장

### 5.4 R-W4-4: 9 leak consumer 마이그 후 빌드 폭발
- 시나리오: ImGui DX11 backend 외에도 외부 라이브러리 (DirectXTK 등) 가 raw 요구
- 완화: ① escape API 충분 활용 ② Public 헤더에서만 raw 제거, Private cpp 에서는 raw 유지 OK ③ 한 번에 1 파일씩 마이그 + 빌드 검증

---

## 6. Week 4 통합 합격 검증

```bash
# 1. 셰이더 컴파일
ls Shaders/SSAO/{GTAO_CS,GTAO_Blur_CS}.hlsl

# 2. SSAO/Normal 인프라
ls Engine/Public/Renderer/{NormalPass,SSAOPass}.h

# 3. Public 헤더에서 ID3D11Device 노출 검사 (감소 기대)
W3_count=9   # W3 끝 시점
W4_count=$(rg "ID3D11Device|d3d11\\.h" Engine/Public/ Client/Public/ -l | wc -l)
echo "W3: 9 → W4: $W4_count (감소 기대, 0~2 권장)"

# 4. 빌드 + 런타임
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m

# 5. 시각 검증 (이렐리아 + 맵 진입)
#    - SSAO 토글 on/off 시 그림자 변화 확인
#    - Frame ≤20ms (F3 Profiler)
```

---

## 7. 부록 A — Week 4 진입 체크리스트

```
[ ] Week 3 결과 검증 통과
[ ] devenv.exe 종료 + git: feature/2026-05-04-week4 branch
[ ] Engine 단독 빌드 → EngineSDK/inc 동기화

Track 1 (SSAO):
[ ] §3.2 NormalPass.h/.cpp 신설
[ ] §3.3 Mesh3D_PBR + Skinned3D_PBR 의 PS MRT 출력 (RTV1=Normal)
[ ] §3.4 GTAO_CS.hlsl 신설
[ ] §3.5 GTAO_Blur_CS.hlsl 신설 (cross-bilateral 5×5)
[ ] §3.6 CSSAOPass.h/.cpp 신설
[ ] §3.7 PBR PS 의 ambient * AO 곱
[ ] §3.8 ChampionTuner SSAO 슬라이더 + on/off

Track 2 (RH-2 시작):
[ ] §4.1 ModelRenderer.cpp 마이그
[ ] §4.2 PlaneRenderer.h/.cpp 마이그
[ ] §4.3 Mesh.h / Model.h / Texture.h / ResourceCache.h 마이그
[ ] §4.4 UI_Manager 마이그 (ImGui escape 명시)
[ ] §4.5 FxBillboardComponent / FxMeshComponent / FxSystem 마이그
[ ] §4.6 Engine_Defines.h / CEngineApp.h 의 RHI/DX11 직접 include 제거
[ ] §4.7 W4 끝 시점 잔여 노출 inventory 작성 → W5 입력

검증:
[ ] §6.4 빌드 error 0
[ ] §6.5 SSAO 시각 확인 + Frame ≤20ms
[ ] Public 헤더 ID3D11* 노출 9 → 2 이하
```

---

## 8. 한 줄

> **Week 4 = T1 (NormalPass + GTAO + Blur + SSAOPass + PBR ambient*AO + Tuner) + T2 (RH-2 시작 — 9 leak consumer 마이그 to IRHIDevice + escape API 활용). 합격: SSAO 시각 + Frame ≤20ms + Public ID3D11Device 노출 9→0~2.**

---

## 끝.
