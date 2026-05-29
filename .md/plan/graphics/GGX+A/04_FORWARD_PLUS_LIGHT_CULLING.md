# 04. Forward+ Light Culling — Compute Shader + Tile Grid

> 본 문서는 **수백 개 동적 광원** 을 60fps 로 렌더링하기 위한 Forward+ 파이프라인 전문 구현.
> 의존: `00_INDEX.md` Stage 0~4 완료. Depth pre-pass 가 이미 동작.
> 출처: Olsson & Assarsson 2012 "Tiled Forward Shading", Filament Engine 문서, NVIDIA 샘플 "PracticalForwardPlus".

---

## 1. 파이프라인 전체 흐름 (1 프레임)

```
┌─────────────────────────────────────────────────────────────┐
│ 1. CLEAR depth + color targets                              │
│                                                             │
│ 2. DEPTH PRE-PASS                                           │
│    └─ Mesh3D 한 번 그려서 depth buffer 채움 (color write off) │
│                                                             │
│ 3. LIGHT CULLING COMPUTE SHADER                             │
│    ├─ Input:  StructuredBuffer<PointLight> g_Lights         │
│    │          Texture2D g_DepthBuffer                       │
│    ├─ For each 16x16 tile (8040 tiles for 1080p):           │
│    │   a. Compute tile min/max depth → frustum              │
│    │   b. Test frustum vs every light sphere                │
│    │   c. Write light indices to g_LightIndex (UAV)         │
│    │   d. Write [offset, count] to g_LightGrid (UAV)        │
│    │                                                        │
│ 4. SHADING PASS                                             │
│    └─ Mesh3D_PBR.hlsl PS:                                   │
│       a. Read tile coord from SV_Position.xy / 16           │
│       b. Read [offset, count] from g_LightGrid              │
│       c. for (i = 0; i < count; ++i)                        │
│             Lo += ApplyPointLight(g_Lights[g_LightIndex[offset+i]]) │
│                                                             │
│ 5. FX PASS (FxSprite/FxMesh — PBR 없음, 그대로)             │
│                                                             │
│ 6. ImGui                                                    │
│                                                             │
│ 7. Present                                                  │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. 자료구조 — Tile Grid

### 2.1 Tile 크기 결정

- **16×16 픽셀** (NVIDIA/Unity HDRP 추천).
- 1920×1080 → 120×68 = 8160 tiles (마지막 줄 padding).
- **Tile 당 최대 광원 수**: 256 (실측 평균 4~8, 최악 50).

### 2.2 GPU 자료구조

```hlsl
// 입력 (Shading Pass + Cull CS 공유)
StructuredBuffer<PointLight> g_Lights      : register(t10);   // 모든 광원 (kMaxPointLights = 1024)

// CS 출력 (Shading Pass 입력)
RWStructuredBuffer<uint>     g_LightIndex  : register(u0);    // [tileCount × maxLightsPerTile]
RWStructuredBuffer<uint2>    g_LightGrid   : register(u1);    // [tileCount] = (offset, count)

// 또는 atomic counter 방식
RWStructuredBuffer<uint>     g_LightCounter : register(u2);   // 글로벌 인덱스 카운터
```

| Buffer | 크기 (1920×1080, 256 lights/tile) | 용도 |
|---|---|---|
| `g_Lights` | 1024 × 32B = 32KB | 모든 점광원 |
| `g_LightIndex` | 8160 × 256 × 4B = 8.4MB | 타일별 광원 인덱스 |
| `g_LightGrid`  | 8160 × 8B = 65KB | 타일별 (offset, count) |

→ 8.4MB 가 가장 무거움. **maxLightsPerTile=128 로 줄이면 4.2MB** (대부분 충분).

---

## 3. Depth Pre-pass — Stage 0

### 3.1 신규 셰이더

`Shaders/PBR/DepthPrepass.hlsl` (이미 `02_HLSL_BRDF_LIBRARY.md` 에 작성).

### 3.2 RTV 바인딩 — color write off

```cpp
// Engine/Private/Renderer/PBR/DepthPrepass.cpp 신규 또는 ModelRenderer 확장
void RenderDepthPrepass(ID3D11DeviceContext* pCtx,
                        const std::vector<ModelRenderer*>& opaqueModels)
{
    // 1. RTV = nullptr, DSV 만 바인딩
    ID3D11RenderTargetView* nullRTV = nullptr;
    pCtx->OMSetRenderTargets(0, &nullRTV, m_pDSV.Get());

    // 2. BlendState = NoColorWrite (모든 RenderTargetWriteMask = 0)
    pCtx->OMSetBlendState(m_pNoColorWriteBS.Get(), nullptr, 0xFFFFFFFF);

    // 3. DepthState = depth write ON, depth test LESS
    pCtx->OMSetDepthStencilState(m_pDepthWriteState.Get(), 0);

    // 4. DepthPrepass shader 바인딩
    m_DepthPrepassShader.Bind(pCtx);

    // 5. opaque 모델 전부 그리기
    for (auto* pModel : opaqueModels) {
        pModel->RenderDepthOnly(pCtx);
    }
}
```

### 3.3 BlendState — NoColorWrite

```cpp
D3D11_BLEND_DESC bd{};
bd.RenderTarget[0].BlendEnable    = FALSE;
bd.RenderTarget[0].RenderTargetWriteMask = 0;       // R/G/B/A 전부 OFF
pDevice->CreateBlendState(&bd, m_pNoColorWriteBS.GetAddressOf());
```

### 3.4 DSV 텍스처 (CS 입력 용)

기본 DSV 는 CS 에서 읽기 어려움. **별도 SRV** 필요:

```cpp
D3D11_TEXTURE2D_DESC td{};
td.Width      = screenW;
td.Height     = screenH;
td.MipLevels  = 1;
td.ArraySize  = 1;
td.Format     = DXGI_FORMAT_R24G8_TYPELESS;          // ★ TYPELESS 필수
td.SampleDesc.Count = 1;
td.BindFlags  = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

pDevice->CreateTexture2D(&td, nullptr, m_pDepthTex.GetAddressOf());

// DSV view
D3D11_DEPTH_STENCIL_VIEW_DESC dsvD{};
dsvD.Format        = DXGI_FORMAT_D24_UNORM_S8_UINT;
dsvD.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
pDevice->CreateDepthStencilView(m_pDepthTex.Get(), &dsvD, m_pDSV.GetAddressOf());

// SRV view (CS 가 사용)
D3D11_SHADER_RESOURCE_VIEW_DESC srvD{};
srvD.Format        = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
srvD.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
srvD.Texture2D.MipLevels = 1;
pDevice->CreateShaderResourceView(m_pDepthTex.Get(), &srvD, m_pDepthSRV.GetAddressOf());
```

---

## 4. Light Culling Compute Shader

### 4.1 `Shaders/PBR/ForwardPlus_LightCull.hlsl` 전문

```hlsl
// =========================================================
//  ForwardPlus_LightCull.hlsl — Compute Shader
//  스레드 그룹 = 1 tile (16×16 픽셀)
//  각 그룹의 64 스레드가 협동하여:
//   1. 타일 min/max depth 계산
//   2. 타일 frustum 6 평면 생성
//   3. 모든 광원 분산 테스트
//   4. 테스트 통과한 광원 인덱스를 g_LightIndex 에 기록
// =========================================================

#include "../BRDF/BRDFLighting.hlsli"

#define TILE_SIZE 16
#define TILE_THREAD_COUNT (TILE_SIZE * TILE_SIZE)
#define MAX_LIGHTS_PER_TILE 128

cbuffer CBCullParams : register(b0)
{
    row_major float4x4 g_matInvProj;        // proj^-1 (view space frustum 복원)
    row_major float4x4 g_matView;
    uint2  g_uScreenSize;                   // (W, H) 픽셀
    uint   g_uLightCount;                   // 활성 광원 수
    uint   _pad0;
};

StructuredBuffer<PointLight>   g_Lights     : register(t0);
Texture2D<float>               g_Depth      : register(t1);

RWStructuredBuffer<uint>       g_LightIndex : register(u0);
RWStructuredBuffer<uint2>      g_LightGrid  : register(u1);

// 그룹 공유 메모리 (64KB 한도)
groupshared uint  s_uMinDepth;
groupshared uint  s_uMaxDepth;
groupshared uint  s_uTileLightCount;
groupshared uint  s_uTileLightIndices[MAX_LIGHTS_PER_TILE];
groupshared float4 s_FrustumPlanes[6];

// view-space depth 복원
float LinearizeDepth(float zNDC)
{
    float4 clip = float4(0, 0, zNDC, 1);
    float4 view = mul(clip, g_matInvProj);
    return view.z / view.w;
}

// 평면 vs 구 거리 (in front of plane = positive)
float SignedDistance(float4 plane, float3 p)
{
    return dot(plane.xyz, p) + plane.w;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CSMain(
    uint3 dtid : SV_DispatchThreadID,
    uint3 gid  : SV_GroupID,
    uint3 gtid : SV_GroupThreadID,
    uint  gi   : SV_GroupIndex)
{
    // ===== 1. 그룹 공유 변수 초기화 (스레드 0번만) =====
    if (gi == 0) {
        s_uMinDepth = 0x7F7FFFFF;        // FLT_MAX as uint
        s_uMaxDepth = 0;
        s_uTileLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // ===== 2. 자기 픽셀 depth 읽기 =====
    float depth = g_Depth.Load(int3(dtid.xy, 0));
    uint  depthInt = asuint(depth);

    // 원자적 min/max
    InterlockedMin(s_uMinDepth, depthInt);
    InterlockedMax(s_uMaxDepth, depthInt);
    GroupMemoryBarrierWithGroupSync();

    // ===== 3. Tile frustum 6 평면 생성 (스레드 0번만) =====
    if (gi == 0) {
        float minZ = LinearizeDepth(asfloat(s_uMinDepth));
        float maxZ = LinearizeDepth(asfloat(s_uMaxDepth));

        // 타일의 NDC 좌표
        float2 tileMin = (float2(gid.xy * TILE_SIZE) / g_uScreenSize) * 2 - 1;
        float2 tileMax = (float2((gid.xy + 1) * TILE_SIZE) / g_uScreenSize) * 2 - 1;
        tileMin.y = -tileMin.y;          // DX y-down → up
        tileMax.y = -tileMax.y;

        // 4 모서리 view-space 위치 복원
        float3 corners[4];
        corners[0] = mul(float4(tileMin.x, tileMin.y, 1, 1), g_matInvProj).xyz;
        corners[1] = mul(float4(tileMax.x, tileMin.y, 1, 1), g_matInvProj).xyz;
        corners[2] = mul(float4(tileMin.x, tileMax.y, 1, 1), g_matInvProj).xyz;
        corners[3] = mul(float4(tileMax.x, tileMax.y, 1, 1), g_matInvProj).xyz;

        // 4 측면 평면 (origin 통과, normal = corner X corner_next)
        s_FrustumPlanes[0] = float4(normalize(cross(corners[2], corners[0])), 0); // left
        s_FrustumPlanes[1] = float4(normalize(cross(corners[1], corners[3])), 0); // right
        s_FrustumPlanes[2] = float4(normalize(cross(corners[0], corners[1])), 0); // top
        s_FrustumPlanes[3] = float4(normalize(cross(corners[3], corners[2])), 0); // bottom

        // near/far 평면 (z 축 정렬)
        s_FrustumPlanes[4] = float4(0, 0, -1,  minZ);    // near
        s_FrustumPlanes[5] = float4(0, 0,  1, -maxZ);    // far
    }
    GroupMemoryBarrierWithGroupSync();

    // ===== 4. 광원 분산 테스트 (각 스레드가 1개씩 N 라운드) =====
    for (uint lightId = gi; lightId < g_uLightCount; lightId += TILE_THREAD_COUNT) {
        PointLight L = g_Lights[lightId];
        // light position 을 view space 로
        float3 viewPos = mul(float4(L.vPosition, 1), g_matView).xyz;

        bool inside = true;
        [unroll]
        for (uint i = 0; i < 6; ++i) {
            if (SignedDistance(s_FrustumPlanes[i], viewPos) < -L.fRadius) {
                inside = false;
                break;
            }
        }

        if (inside) {
            uint slot;
            InterlockedAdd(s_uTileLightCount, 1, slot);
            if (slot < MAX_LIGHTS_PER_TILE) {
                s_uTileLightIndices[slot] = lightId;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // ===== 5. 결과 글로벌 버퍼에 기록 (스레드 0번만) =====
    if (gi == 0) {
        uint count = min(s_uTileLightCount, MAX_LIGHTS_PER_TILE);
        uint tileIndex = gid.y * (g_uScreenSize.x / TILE_SIZE) + gid.x;
        uint offset = tileIndex * MAX_LIGHTS_PER_TILE;

        g_LightGrid[tileIndex] = uint2(offset, count);

        for (uint i = 0; i < count; ++i) {
            g_LightIndex[offset + i] = s_uTileLightIndices[i];
        }
    }
}
```

### 4.2 Dispatch 호출 (C++)

```cpp
// Engine/Private/Renderer/PBR/ForwardPlusPipeline.cpp
void CForwardPlusPipeline::DispatchLightCull(
    ID3D11DeviceContext* pCtx,
    const DirectX::XMFLOAT4X4& matView,
    const DirectX::XMFLOAT4X4& matProj,
    u32_t screenW, u32_t screenH,
    u32_t lightCount)
{
    // 1. Cull 셰이더 바인딩
    pCtx->CSSetShader(m_pCullCS.Get(), nullptr, 0);

    // 2. cbuffer 업로드
    CBCullParams cb{};
    DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4(&matProj);
    DirectX::XMStoreFloat4x4(&cb.matInvProj,
        DirectX::XMMatrixInverse(nullptr, P));
    cb.matView    = matView;
    cb.uScreenSize = {screenW, screenH};
    cb.uLightCount = lightCount;

    D3D11_MAPPED_SUBRESOURCE map{};
    pCtx->Map(m_pCullCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &cb, sizeof(CBCullParams));
    pCtx->Unmap(m_pCullCB.Get(), 0);

    ID3D11Buffer* cbs[] = { m_pCullCB.Get() };
    pCtx->CSSetConstantBuffers(0, 1, cbs);

    // 3. SRV 바인딩 — 광원 + depth
    ID3D11ShaderResourceView* srvs[] = {
        m_pLightSRV,        // t0 = lights (CLightManager 에서 받음)
        m_pDepthSRV.Get(),  // t1 = depth
    };
    pCtx->CSSetShaderResources(0, 2, srvs);

    // 4. UAV 바인딩 — 인덱스 + 그리드
    ID3D11UnorderedAccessView* uavs[] = {
        m_pLightIndexUAV.Get(),
        m_pLightGridUAV.Get(),
    };
    UINT initialCounts[] = { 0, 0 };
    pCtx->CSSetUnorderedAccessViews(0, 2, uavs, initialCounts);

    // 5. Dispatch — 화면 / TILE_SIZE
    u32_t tileX = (screenW + 15) / 16;
    u32_t tileY = (screenH + 15) / 16;
    pCtx->Dispatch(tileX, tileY, 1);

    // 6. UAV 언바인딩 (다음 단계가 SRV 로 읽으므로)
    ID3D11UnorderedAccessView* nullUAVs[2] = {nullptr, nullptr};
    pCtx->CSSetUnorderedAccessViews(0, 2, nullUAVs, nullptr);
}
```

---

## 5. Shading Pass — PS 가 LightGrid 읽기

### 5.1 `Mesh3D_PBR.hlsl` 의 PS 확장

#### BEFORE (Stage 4 — 단일 dir light 만)

```hlsl
float3 Lo = ApplyDirLight(g_DirLight, albedo, metallic, roughness, N, V, input.vWorldPos);
```

#### AFTER (Stage 5 — Forward+ point lights 추가)

```hlsl
// 추가 슬롯
StructuredBuffer<PointLight> g_Lights      : register(t10);
StructuredBuffer<uint>       g_LightIndex  : register(t11);
StructuredBuffer<uint2>      g_LightGrid   : register(t12);

cbuffer CBScreenInfo : register(b5)
{
    uint2 g_uScreenSize;
    uint  g_uTileSizeX;
    uint  g_uMaxLightsPerTile;
};

// PS 본문
float3 Lo = ApplyDirLight(g_DirLight, albedo, metallic, roughness, N, V, input.vWorldPos);

// 자기 타일 좌표
uint2 tileXY = uint2(input.vPosition.xy) / 16;
uint  tileIndex = tileXY.y * (g_uScreenSize.x / 16) + tileXY.x;
uint2 grid = g_LightGrid[tileIndex];
uint  offset = grid.x;
uint  count  = grid.y;

[loop]
for (uint i = 0; i < count; ++i)
{
    uint lightId = g_LightIndex[offset + i];
    PointLight L = g_Lights[lightId];
    Lo += ApplyPointLight(L, albedo, metallic, roughness, N, V, input.vWorldPos);
}
```

→ 화면당 광원 100개 + 타일 평균 8개 = **PS 당 8회 BRDF 평가** (직접 100개 순회 대비 **12.5x 빠름**).

---

## 6. `Engine/Public/Renderer/PBR/ForwardPlusPipeline.h`

```cpp
// =========================================================
//  ForwardPlusPipeline.h — Light Culling CS + UAV/SRV 관리
//  CGameInstance::Get_ForwardPlusPipeline() 로 노출.
// =========================================================

#pragma once
#include "PBRTypes.h"
#include "WintersAPI.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>

class WINTERS_ENGINE CForwardPlusPipeline
{
public:
    static constexpr u32_t kTileSize           = 16;
    static constexpr u32_t kMaxLightsPerTile   = 128;

    ~CForwardPlusPipeline() = default;
    CForwardPlusPipeline(const CForwardPlusPipeline&)            = delete;
    CForwardPlusPipeline& operator=(const CForwardPlusPipeline&) = delete;

    static std::unique_ptr<CForwardPlusPipeline> Create(
        ID3D11Device* pDevice, u32_t screenW, u32_t screenH);

    // 화면 크기 변경 (윈도우 리사이즈)
    bool Resize(ID3D11Device* pDevice, u32_t screenW, u32_t screenH);

    // Depth Pre-pass DSV/SRV 접근
    ID3D11DepthStencilView*    GetDepthDSV() const { return m_pDepthDSV.Get(); }
    ID3D11ShaderResourceView*  GetDepthSRV() const { return m_pDepthSRV.Get(); }

    // 매 프레임 호출 — Compute 디스패치
    void DispatchLightCull(
        ID3D11DeviceContext* pCtx,
        const DirectX::XMFLOAT4X4& matView,
        const DirectX::XMFLOAT4X4& matProj,
        ID3D11ShaderResourceView*  pLightSRV,
        u32_t lightCount);

    // Shading Pass 시작 전 LightGrid SRV 바인딩 (t11, t12)
    void BindShadingResources(ID3D11DeviceContext* pCtx);

private:
    CForwardPlusPipeline() = default;

    bool LoadCullShader(ID3D11Device* pDevice);
    bool CreateDepthBuffer(ID3D11Device* pDevice, u32_t W, u32_t H);
    bool CreateLightBuffers(ID3D11Device* pDevice, u32_t tileCount);

    Microsoft::WRL::ComPtr<ID3D11ComputeShader>      m_pCullCS;
    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_pCullCB;          // CBCullParams

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_pDepthTex;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_pDepthDSV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pDepthSRV;

    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_pLightIndexBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pLightIndexUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pLightIndexSRV;

    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_pLightGridBuffer;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> m_pLightGridUAV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pLightGridSRV;

    u32_t m_uScreenW = 0;
    u32_t m_uScreenH = 0;
};
```

---

## 7. CEngineApp 프레임 루프 통합

### BEFORE (`Engine/Private/Framework/CEngineApp.cpp` Render() 대략적 형태)

```cpp
void CEngineApp::Render()
{
    // 1. clear targets
    ClearRenderTargets();
    // 2. scene render (모델 그리기)
    m_pSceneManager->Render();
    // 3. ImGui
    m_pImGui->Render();
    // 4. present
    m_pSwapChain->Present(...);
}
```

### AFTER

```cpp
void CEngineApp::Render()
{
    auto* pCtx = m_pDevice->GetContext();

    // 1. clear targets
    ClearRenderTargets();

    // 2. [Stage 0] Depth Pre-pass
    m_pForwardPlus->BeginDepthPrepass(pCtx);
    m_pSceneManager->RenderDepthOnly();
    m_pForwardPlus->EndDepthPrepass(pCtx);

    // 3. [Stage 5] Light Culling Compute
    auto* pLights = m_pLightManager.get();
    m_pForwardPlus->DispatchLightCull(
        pCtx,
        m_pCamera->GetView(),
        m_pCamera->GetProj(),
        pLights->GetPointLightSRV(),
        pLights->GetPointLightCount());

    // 4. Shading Pass
    BindBackBuffer();
    m_pForwardPlus->BindShadingResources(pCtx);     // t11/t12
    m_pSceneManager->Render();                       // PBR 셰이더 사용

    // 5. FX Pass (그대로)
    m_pSceneManager->RenderFX();

    // 6. ImGui + Present
    m_pImGui->Render();
    m_pSwapChain->Present(...);
}
```

---

## 8. 디버그 — Tile Light Count 히트맵

성능 핫스팟 발견에 필수. 별도 디버그 PS:

```hlsl
// Shaders/Debug/TileLightHeatmap.hlsl
StructuredBuffer<uint2> g_LightGrid : register(t0);
cbuffer CB : register(b0) { uint2 g_uScreenSize; }

float4 PS(float4 pos : SV_POSITION) : SV_TARGET
{
    uint2 tile = uint2(pos.xy) / 16;
    uint  idx  = tile.y * (g_uScreenSize.x / 16) + tile.x;
    uint  count = g_LightGrid[idx].y;

    // 0~16: 녹색 / 16~64: 노랑 / 64~128: 빨강
    float t = saturate(count / 64.0f);
    return float4(t, 1.0f - t * 0.5f, 0, 0.5f);
}
```

→ ImGui 패널 토글로 켜고/끄기. 빨간 영역 = 광원 과밀 → 게임플레이/스킬 FX 광원 정리 필요.

---

## 9. 광원 수 제한 + LRU 정책

수백 개 스킬 FX 가 동시 발동하면 1024 한도 초과 위험.

```cpp
// CLightManager 확장
class CLightManager
{
public:
    struct LightHandle {
        u32_t  uIndex;
        f32_t  fLifetime;       // 0 = 영구
        f32_t  fSpawnTime;
    };

    u32_t AddPointLight(const PointLightGPU& light, f32_t fLifetime = 0.0f);
    void  TickLifetime(f32_t fDeltaTime);    // 만료 광원 자동 제거

private:
    std::vector<LightHandle> m_Lifetimes;
};
```

→ 스킬 FX 광원은 `fLifetime = 1.0` 등 설정 시 자동 GC.

---

## 10. Forward+ 통합 후 MOBA 활용 시나리오

| 광원 종류 | 색상/강도 | 수명 | 트리거 |
|---|---|---|---|
| 태양광 (directional) | 1.0 / 3.0 | 영구 | Scene_InGame OnEnter |
| 포탑 아우라 | 푸른빛 / 0.8 | 영구 | 포탑 스폰 시 |
| 챔프 림라이팅 | 팀 색 / 0.5 | 영구 | 챔프 스폰 시, 팀별 색 |
| 이렐리아 R 펄스 | (0.5, 0.8, 1.6) / 5.0 | 0.6초 | UltWaveSystem hit |
| Q dash 잔광 | 흰색 / 2.0 | 0.3초 | Q 발동 시 |
| 스턴 마크 | 노랑 / 1.5 | 1초 | E 스턴 적용 시 |
| 미니언 사망 폭발 | 주황 / 3.0 | 0.4초 | HP 0 도달 시 |

→ 모든 광원이 Forward+ 로 처리 = **CPU 사이드는 등록만** 하면 GPU 가 알아서 컬링.

---

## 11. 성능 측정 — 목표

| 시나리오 | 광원 수 | 목표 ms (1080p, RTX 3060 기준) |
|---|---|---|
| 평상 (포탑 + 챔프 림 + 태양) | ~25 | < 0.5 ms |
| 한타 (스킬 FX 동시 폭발) | ~200 | < 1.5 ms |
| 최악 (1024 광원) | 1024 | < 3 ms |

→ 측정은 `Winters_Profile_Push("Forward+ Cull")` 매크로로 박제. CPU 프레임 < 16.6ms 유지.

---

## 다음 문서

→ `05_INTEGRATION_PHASES.md` — 7단계 롤아웃 일정, 각 단계 위험 요소, 롤백 전략.

## 검증 체크리스트 (이 단계 완료 조건)

- [ ] Depth Pre-pass 후 RenderDoc 에서 depth buffer 가 채워져 있음 확인.
- [ ] Light Cull CS 가 1080p 8160 tiles 디스패치 (`Dispatch(120, 68, 1)`).
- [ ] g_LightGrid 의 한 타일 (offset, count) 이 ImGui 디버그로 노출.
- [ ] 100개 점광원 산포 시 PS 가 평균 8개만 평가 (Tile Heatmap 으로 확인).
- [ ] 광원 1024개 한도 도달 시 `AddPointLight` 가 UINT32_MAX 반환 + 신규 등록 거절.
- [ ] CLightManager::TickLifetime 호출 시 만료 광원 자동 제거.
- [ ] Forward+ Cull CS 의 Dispatch ms 가 1080p / 100 lights 기준 < 1ms.
