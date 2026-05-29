# 02. Clustered Deferred — Winters Engine 이식 가이드

> 원전: `C:\Users\user\Desktop\.markdown\Graphics\02_Clustered_Deferred.md`
> 대상 경로: `Engine/Public/Renderer/Clustered/`, `Engine/Private/Renderer/Clustered/`, `Shaders/Clustered/`
> Phase: **E Stage 2 후반** — BRDF(Stage 1) 완료 후. G-Buffer 신설 + Clustered Light Culling.

---

## 0. 이 문서의 목표 (Winters 관점)

1. 현재 `Mesh3D.hlsl`/`Skinned3D.hlsl` 의 Forward 경로를 두고 **병렬로 Clustered Deferred 파이프** 신설.
2. MOBA 특성상 grid **16 × 9 × 24 = 3456 cluster**, `max lights/cluster = 32` 로 타겟 고정.
3. **모든 GPU 자원은 `DX11StructuredBuffer<T>` / `DX11ConstantBuffer<T>` 템플릿 재사용**.
4. `CGameInstance` 의 Tier 2 API 로 `IClusteredLighting* Get_ClusteredLighting()` 제공.
5. **ImGui 튜너 필수**: cluster X/Y/Z, max lights, heatmap 토글.

---

## 1. 디렉토리 신설

```
Engine/Public/Renderer/Clustered/
├── ClusteredTypes.h            // ClusterAABB / GPULight / ClusterUniforms
├── IClusteredLighting.h        // 인터페이스 (CGameInstance Tier 2 Getter 반환)
└── ClusteredLighting.h         // class CClusteredLighting : public IClusteredLighting

Engine/Private/Renderer/Clustered/
└── ClusteredLighting.cpp

Shaders/Clustered/
├── ClusteredCommon.hlsli       // 상수 매크로 (CLUSTER_X/Y/Z, MAX_LIGHTS_PER_CLUSTER)
├── BuildClusterAABB.hlsl       // CS
├── CullLights.hlsl             // CS
├── LightingPass.hlsl           // PS (fullscreen triangle)
└── ClusterDebug.hlsl           // PS (heatmap)
```

---

## 2. 공유 타입 — `ClusteredTypes.h`

```cpp
// Engine/Public/Renderer/Clustered/ClusteredTypes.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <DirectXMath.h>

constexpr u32_t CLUSTER_X = 16;
constexpr u32_t CLUSTER_Y = 9;
constexpr u32_t CLUSTER_Z = 24;
constexpr u32_t CLUSTER_COUNT           = CLUSTER_X * CLUSTER_Y * CLUSTER_Z;     // 3456
constexpr u32_t MAX_LIGHTS_PER_CLUSTER  = 32;
constexpr u32_t GLOBAL_INDEX_CAPACITY   = CLUSTER_COUNT * MAX_LIGHTS_PER_CLUSTER; // 110592
constexpr u32_t MAX_LIGHTS_TOTAL        = 1024;

struct ClusterAABB
{
    DirectX::XMFLOAT4 vMin;   // w unused
    DirectX::XMFLOAT4 vMax;
};
static_assert(sizeof(ClusterAABB) == 32);

enum class eLightType : u32_t
{
    Point       = 0,
    Spot        = 1,
    Directional = 2,
};

struct GPULight
{
    DirectX::XMFLOAT3 vPosition;     // world
    f32_t             fRadius;

    DirectX::XMFLOAT3 vColor;        // linear HDR
    f32_t             fIntensity;

    DirectX::XMFLOAT3 vDirection;    // spot/dir — normalized
    f32_t             fSpotOuter;    // cos(outer)

    u32_t             uType;
    f32_t             fSpotInner;    // cos(inner)
    f32_t             _pad0;
    f32_t             _pad1;
};
static_assert(sizeof(GPULight) % 16 == 0);
static_assert(sizeof(GPULight) == 64);

struct CBClusterUniforms
{
    DirectX::XMFLOAT4X4 matView;
    DirectX::XMFLOAT4X4 matInvProjection;
    f32_t               fZNear;
    f32_t               fZFar;
    f32_t               fSliceScale;     // CLUSTER_Z / log2(zFar/zNear)
    f32_t               fSliceBias;      // -CLUSTER_Z * log2(zNear) / log2(zFar/zNear)

    DirectX::XMUINT3    uGridDim;
    u32_t               uNumLights;

    DirectX::XMFLOAT2   vScreenSize;
    DirectX::XMFLOAT2   vTileSizePx;
};
static_assert(sizeof(CBClusterUniforms) % 16 == 0);
```

---

## 3. 인터페이스 — `IClusteredLighting.h`

```cpp
// Engine/Public/Renderer/Clustered/IClusteredLighting.h
#pragma once
#include "WintersAPI.h"
#include "ClusteredTypes.h"

class WINTERS_ENGINE IClusteredLighting
{
public:
    virtual ~IClusteredLighting() = default;

    // 프로젝션 변경 시 호출 (FOV 변화, 해상도 변화). 매 프레임 호출 허용.
    virtual void RebuildClusterAABBs(const DirectX::XMFLOAT4X4& matProjection,
                                     f32_t fZNear, f32_t fZFar,
                                     u32_t uScreenW, u32_t uScreenH) = 0;

    // CPU 라이트 리스트를 GPU 업로드 (upper bound MAX_LIGHTS_TOTAL)
    virtual void UploadLights(const GPULight* pLights, u32_t uCount,
                              const DirectX::XMFLOAT4X4& matView) = 0;

    // Cull CS Dispatch
    virtual void CullLights() = 0;

    // Lighting PS 에 SRV 바인드 (t8=ClusterInfo, t9=IndexList, t10=Lights)
    virtual void BindForLightingPass(u32_t uStartSlot) = 0;

    // 디버그: 현재 cluster 별 count 를 heatmap 에 바인드
    virtual void BindClusterDebugSRV(u32_t uSlot) = 0;
};
```

---

## 4. 구현 클래스 — `ClusteredLighting.h/.cpp`

```cpp
// Engine/Public/Renderer/Clustered/ClusteredLighting.h
#pragma once
#include "IClusteredLighting.h"
#include "DX11StructuredBuffer.h"
#include "DX11ConstantBuffer.h"
#include "DX11Shader.h"
#include <memory>

class WINTERS_ENGINE CClusteredLighting : public IClusteredLighting
{
public:
    ~CClusteredLighting() override;

    static std::unique_ptr<CClusteredLighting> Create(ID3D11Device* pDev,
                                                      ID3D11DeviceContext* pCtx);

    CClusteredLighting(const CClusteredLighting&)            = delete;
    CClusteredLighting& operator=(const CClusteredLighting&) = delete;
    CClusteredLighting(CClusteredLighting&&)                 = default;
    CClusteredLighting& operator=(CClusteredLighting&&)      = default;

    void RebuildClusterAABBs(const DirectX::XMFLOAT4X4& matProjection,
                             f32_t fZNear, f32_t fZFar,
                             u32_t uScreenW, u32_t uScreenH) override;
    void UploadLights(const GPULight* pLights, u32_t uCount,
                      const DirectX::XMFLOAT4X4& matView) override;
    void CullLights() override;
    void BindForLightingPass(u32_t uStartSlot) override;
    void BindClusterDebugSRV(u32_t uSlot) override;

private:
    CClusteredLighting() = default;

    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

```cpp
// Engine/Private/Renderer/Clustered/ClusteredLighting.cpp
#include "ClusteredLighting.h"
#include "ClusteredTypes.h"
#include <DirectXMath.h>
#include <cmath>

struct CClusteredLighting::Impl
{
    ID3D11Device*        m_pDev = nullptr;
    ID3D11DeviceContext* m_pCtx = nullptr;

    // GPU 버퍼들
    DX11StructuredBuffer<ClusterAABB> m_AABBs;            // RW + SRV
    DX11StructuredBuffer<GPULight>    m_Lights;           // SRV only
    DX11StructuredBuffer<u32_t>       m_GlobalIndex;      // RW + SRV (capacity GLOBAL_INDEX_CAPACITY)
    DX11StructuredBuffer<DirectX::XMUINT2> m_ClusterInfo; // RW + SRV (offset, count) per cluster
    DX11StructuredBuffer<u32_t>       m_AtomicCounter;    // size 1, RW

    DX11ConstantBuffer<CBClusterUniforms> m_cbCluster;

    // 컴파일된 CS
    DX11Shader m_CSBuildAABB;
    DX11Shader m_CSCullLights;

    u32_t m_uCurrentLightCount  = 0;
    bool_t m_bAABBNeedsRebuild   = true;
    DirectX::XMFLOAT4X4 m_CachedProj{};
};

CClusteredLighting::~CClusteredLighting() = default;

std::unique_ptr<CClusteredLighting> CClusteredLighting::Create(ID3D11Device* pDev,
                                                               ID3D11DeviceContext* pCtx)
{
    auto p           = std::unique_ptr<CClusteredLighting>(new CClusteredLighting());
    p->m_pImpl       = std::make_unique<Impl>();
    auto& I          = *p->m_pImpl;
    I.m_pDev         = pDev;
    I.m_pCtx         = pCtx;

    I.m_AABBs.CreateRW(pDev, CLUSTER_COUNT);
    I.m_Lights.CreateSRV(pDev, MAX_LIGHTS_TOTAL);
    I.m_GlobalIndex.CreateRW(pDev, GLOBAL_INDEX_CAPACITY);
    I.m_ClusterInfo.CreateRW(pDev, CLUSTER_COUNT);
    I.m_AtomicCounter.CreateRW(pDev, 1);

    I.m_cbCluster.Create(pDev);

    I.m_CSBuildAABB.LoadCompute(pDev, L"Shaders/Clustered/BuildClusterAABB.hlsl", "CSMain");
    I.m_CSCullLights.LoadCompute(pDev, L"Shaders/Clustered/CullLights.hlsl",        "CSMain");

    return p;
}

void CClusteredLighting::RebuildClusterAABBs(const DirectX::XMFLOAT4X4& matProjection,
                                             f32_t fZNear, f32_t fZFar,
                                             u32_t uScreenW, u32_t uScreenH)
{
    auto& I = *m_pImpl;

    // Change check — FOV/resolution 변경 감지
    if (!std::memcmp(&I.m_CachedProj, &matProjection, sizeof(matProjection)) && !I.m_bAABBNeedsRebuild)
        return;
    I.m_CachedProj        = matProjection;
    I.m_bAABBNeedsRebuild = false;

    CBClusterUniforms u{};
    u.fZNear        = fZNear;
    u.fZFar         = fZFar;
    f32_t invLog    = 1.f / std::log2(fZFar / fZNear);
    u.fSliceScale   = static_cast<f32_t>(CLUSTER_Z) * invLog;
    u.fSliceBias    = -static_cast<f32_t>(CLUSTER_Z) * std::log2(fZNear) * invLog;
    u.uGridDim      = { CLUSTER_X, CLUSTER_Y, CLUSTER_Z };
    u.vScreenSize   = { (f32_t)uScreenW, (f32_t)uScreenH };
    u.vTileSizePx   = { u.vScreenSize.x / CLUSTER_X, u.vScreenSize.y / CLUSTER_Y };

    // matInvProjection
    DirectX::XMMATRIX proj    = DirectX::XMLoadFloat4x4(&matProjection);
    DirectX::XMVECTOR det;
    DirectX::XMMATRIX invProj = DirectX::XMMatrixInverse(&det, proj);
    DirectX::XMStoreFloat4x4(&u.matInvProjection, invProj);

    I.m_cbCluster.Update(I.m_pCtx, u);

    // Dispatch CSBuildAABB
    I.m_pCtx->CSSetShader(I.m_CSBuildAABB.GetComputeShader(), nullptr, 0);
    ID3D11Buffer* cbs[] = { I.m_cbCluster.GetRaw() };
    I.m_pCtx->CSSetConstantBuffers(0, 1, cbs);

    ID3D11UnorderedAccessView* uavs[] = { I.m_AABBs.GetUAV() };
    UINT initialCount[] = { 0 };
    I.m_pCtx->CSSetUnorderedAccessViews(0, 1, uavs, initialCount);

    I.m_pCtx->Dispatch(CLUSTER_X, CLUSTER_Y, CLUSTER_Z);

    ID3D11UnorderedAccessView* nullUAV[] = { nullptr };
    I.m_pCtx->CSSetUnorderedAccessViews(0, 1, nullUAV, nullptr);
}

void CClusteredLighting::UploadLights(const GPULight* pLights, u32_t uCount,
                                      const DirectX::XMFLOAT4X4& matView)
{
    auto& I = *m_pImpl;

    uCount = std::min(uCount, MAX_LIGHTS_TOTAL);
    I.m_Lights.UpdateRange(I.m_pCtx, pLights, uCount);
    I.m_uCurrentLightCount = uCount;

    // 뷰 행렬 업데이트
    CBClusterUniforms u = I.m_cbCluster.GetCached();
    u.matView    = matView;
    u.uNumLights = uCount;
    I.m_cbCluster.Update(I.m_pCtx, u);

    // Atomic counter reset to 0
    u32_t zero = 0;
    I.m_AtomicCounter.UpdateRange(I.m_pCtx, &zero, 1);
}

void CClusteredLighting::CullLights()
{
    auto& I = *m_pImpl;

    I.m_pCtx->CSSetShader(I.m_CSCullLights.GetComputeShader(), nullptr, 0);

    ID3D11Buffer* cbs[] = { I.m_cbCluster.GetRaw() };
    I.m_pCtx->CSSetConstantBuffers(0, 1, cbs);

    ID3D11ShaderResourceView* srvs[] = { I.m_AABBs.GetSRV(), I.m_Lights.GetSRV() };
    I.m_pCtx->CSSetShaderResources(0, 2, srvs);

    ID3D11UnorderedAccessView* uavs[] = {
        I.m_GlobalIndex.GetUAV(),
        I.m_ClusterInfo.GetUAV(),
        I.m_AtomicCounter.GetUAV()
    };
    UINT counts[] = { 0, 0, 0 };
    I.m_pCtx->CSSetUnorderedAccessViews(0, 3, uavs, counts);

    I.m_pCtx->Dispatch(CLUSTER_COUNT, 1, 1);

    ID3D11UnorderedAccessView* nullUAV[] = { nullptr, nullptr, nullptr };
    I.m_pCtx->CSSetUnorderedAccessViews(0, 3, nullUAV, nullptr);
    ID3D11ShaderResourceView* nullSRV[] = { nullptr, nullptr };
    I.m_pCtx->CSSetShaderResources(0, 2, nullSRV);
}

void CClusteredLighting::BindForLightingPass(u32_t uStartSlot)
{
    auto& I = *m_pImpl;

    ID3D11ShaderResourceView* srvs[] = {
        I.m_ClusterInfo.GetSRV(),   // t[start+0]
        I.m_GlobalIndex.GetSRV(),   // t[start+1]
        I.m_Lights.GetSRV()         // t[start+2]
    };
    I.m_pCtx->PSSetShaderResources(uStartSlot, 3, srvs);

    ID3D11Buffer* cbs[] = { I.m_cbCluster.GetRaw() };
    I.m_pCtx->PSSetConstantBuffers(5, 1, cbs);   // b5 = ClusterUniforms (PBR 과 겹치지 않음)
}

void CClusteredLighting::BindClusterDebugSRV(u32_t uSlot)
{
    auto* srv = m_pImpl->m_ClusterInfo.GetSRV();
    m_pImpl->m_pCtx->PSSetShaderResources(uSlot, 1, &srv);
}
```

---

## 5. HLSL — `Shaders/Clustered/ClusteredCommon.hlsli`

```hlsl
// Shaders/Clustered/ClusteredCommon.hlsli
#ifndef CLUSTERED_COMMON_HLSLI
#define CLUSTERED_COMMON_HLSLI

#define CLUSTER_X               16u
#define CLUSTER_Y               9u
#define CLUSTER_Z               24u
#define CLUSTER_COUNT           (CLUSTER_X * CLUSTER_Y * CLUSTER_Z)
#define MAX_LIGHTS_PER_CLUSTER  32u
#define MAX_LIGHTS_TOTAL        1024u

#define LIGHT_POINT        0u
#define LIGHT_SPOT         1u
#define LIGHT_DIRECTIONAL  2u

struct ClusterAABB
{
    float4 vMin;
    float4 vMax;
};

struct GPULight
{
    float3 vPosition;  float fRadius;
    float3 vColor;     float fIntensity;
    float3 vDirection; float fSpotOuter;
    uint   uType;      float fSpotInner;  float _p0; float _p1;
};

cbuffer CBClusterUniforms : register(b0)
{
    row_major matrix matView;
    row_major matrix matInvProjection;
    float  fZNear;
    float  fZFar;
    float  fSliceScale;
    float  fSliceBias;

    uint3  uGridDim;
    uint   uNumLights;

    float2 vScreenSize;
    float2 vTileSizePx;
};

uint GetSlice(float viewZ)
{
    return uint(max(log2(viewZ) * fSliceScale + fSliceBias, 0.0f));
}

uint GetClusterIndex(float2 screenPx, float viewZ)
{
    uint zSlice = min(GetSlice(viewZ), CLUSTER_Z - 1u);
    uint2 tile  = uint2(screenPx / vTileSizePx);
    tile.x = min(tile.x, CLUSTER_X - 1u);
    tile.y = min(tile.y, CLUSTER_Y - 1u);
    return tile.x + tile.y * CLUSTER_X + zSlice * CLUSTER_X * CLUSTER_Y;
}

#endif
```

---

## 6. HLSL — `Shaders/Clustered/BuildClusterAABB.hlsl`

```hlsl
// Shaders/Clustered/BuildClusterAABB.hlsl
#include "ClusteredCommon.hlsli"

RWStructuredBuffer<ClusterAABB> g_AABBs : register(u0);

[numthreads(1, 1, 1)]
void CSMain(uint3 dtid : SV_DispatchThreadID)
{
    uint idx = dtid.x + dtid.y * CLUSTER_X + dtid.z * CLUSTER_X * CLUSTER_Y;

    // NDC tile [-1, +1] 범위
    float2 ndcMin = (float2(dtid.xy)        / float2(CLUSTER_X, CLUSTER_Y)) * 2.0f - 1.0f;
    float2 ndcMax = (float2(dtid.xy + 1.0f) / float2(CLUSTER_X, CLUSTER_Y)) * 2.0f - 1.0f;

    float zN = fZNear * pow(fZFar / fZNear, float(dtid.z)       / float(CLUSTER_Z));
    float zF = fZNear * pow(fZFar / fZNear, float(dtid.z + 1u)  / float(CLUSTER_Z));

    float2 corners[4] = {
        float2(ndcMin.x, ndcMin.y),
        float2(ndcMax.x, ndcMin.y),
        float2(ndcMin.x, ndcMax.y),
        float2(ndcMax.x, ndcMax.y)
    };

    float3 aMin = float3( 1e30,  1e30,  1e30);
    float3 aMax = float3(-1e30, -1e30, -1e30);

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        // NDC(z=0) 를 뷰 공간으로 역투영 — 카메라 원점에서의 방향 구성
        float4 pNear4 = mul(float4(corners[i], 0.0f, 1.0f), matInvProjection);
        float3 dir    = pNear4.xyz / pNear4.w;

        // Winters DX11 관례: LH +Z forward. 플레인 z = zN/zF.
        // dir.z > 0 보장 (near plane 넘어간 점이므로).
        float tNear = zN / max(dir.z, 1e-4f);
        float tFar  = zF / max(dir.z, 1e-4f);

        float3 pN = dir * tNear;
        float3 pF = dir * tFar;

        aMin = min(aMin, min(pN, pF));
        aMax = max(aMax, max(pN, pF));
    }

    g_AABBs[idx].vMin = float4(aMin, 0.0f);
    g_AABBs[idx].vMax = float4(aMax, 0.0f);
}
```

---

## 7. HLSL — `Shaders/Clustered/CullLights.hlsl`

```hlsl
// Shaders/Clustered/CullLights.hlsl
#include "ClusteredCommon.hlsli"

#define THREADS_PER_CLUSTER 64u

StructuredBuffer<ClusterAABB>     g_AABBs  : register(t0);
StructuredBuffer<GPULight>        g_Lights : register(t1);

RWStructuredBuffer<uint>          g_IndexList   : register(u0);
RWStructuredBuffer<uint2>         g_ClusterInfo : register(u1);
RWStructuredBuffer<uint>          g_GlobalCount : register(u2);

groupshared uint gs_VisibleIdx[MAX_LIGHTS_PER_CLUSTER];
groupshared uint gs_VisibleCount;

bool SphereAABB(float3 c, float r, float3 aMin, float3 aMax)
{
    float3 q = clamp(c, aMin, aMax);
    float3 d = c - q;
    return dot(d, d) <= r * r;
}

bool ConeAABB(float3 apex, float3 dir, float cosO, float range,
              float3 aMin, float3 aMax)
{
    // 1) sphere 박스 먼저
    if (!SphereAABB(apex, range, aMin, aMax)) return false;

    // 2) Charles Bloom cone-sphere (원전 §3.4)
    float3 center = 0.5f * (aMin + aMax);
    float3 extent = 0.5f * (aMax - aMin);
    float  bboxR  = length(extent);

    float3 v      = center - apex;
    float  vLenSq = dot(v, v);
    float  v1     = dot(v, dir);

    float  sinO       = sqrt(max(1.0f - cosO * cosO, 0.0f));
    float  distClose  = cosO * sqrt(max(vLenSq - v1 * v1, 0.0f)) - v1 * sinO;

    bool angleCull = distClose > bboxR;
    bool frontCull = v1 > bboxR + range;
    bool backCull  = v1 < -bboxR;

    return !(angleCull || frontCull || backCull);
}

[numthreads(THREADS_PER_CLUSTER, 1, 1)]
void CSMain(uint3 gid : SV_GroupID, uint gtid : SV_GroupIndex)
{
    uint cidx = gid.x;
    if (cidx >= CLUSTER_COUNT) return;

    ClusterAABB aabb = g_AABBs[cidx];
    float3 aMin = aabb.vMin.xyz;
    float3 aMax = aabb.vMax.xyz;

    if (gtid == 0) gs_VisibleCount = 0;
    GroupMemoryBarrierWithGroupSync();

    for (uint i = gtid; i < uNumLights; i += THREADS_PER_CLUSTER)
    {
        GPULight L = g_Lights[i];

        // world → view 변환
        float3 lposV = mul(float4(L.vPosition, 1.0f), matView).xyz;
        float3 ldirV = mul(float4(L.vDirection, 0.0f), matView).xyz;

        bool vis = false;
        if (L.uType == LIGHT_POINT)
            vis = SphereAABB(lposV, L.fRadius, aMin, aMax);
        else if (L.uType == LIGHT_SPOT)
            vis = ConeAABB(lposV, ldirV, L.fSpotOuter, L.fRadius, aMin, aMax);
        else
            vis = true;   // directional always visible

        if (vis)
        {
            uint slot;
            InterlockedAdd(gs_VisibleCount, 1u, slot);
            if (slot < MAX_LIGHTS_PER_CLUSTER)
                gs_VisibleIdx[slot] = i;
        }
    }

    GroupMemoryBarrierWithGroupSync();

    if (gtid == 0)
    {
        uint count  = min(gs_VisibleCount, MAX_LIGHTS_PER_CLUSTER);
        uint offset;
        InterlockedAdd(g_GlobalCount[0], count, offset);
        g_ClusterInfo[cidx] = uint2(offset, count);
    }
    GroupMemoryBarrierWithGroupSync();

    uint2 info = g_ClusterInfo[cidx];
    for (uint j = gtid; j < info.y; j += THREADS_PER_CLUSTER)
        g_IndexList[info.x + j] = gs_VisibleIdx[j];
}
```

---

## 8. HLSL — `Shaders/Clustered/LightingPass.hlsl`

GBuffer 전제 (Stage 2 에서 신설되는 MRT):

```
RT0 = Albedo.rgb + Metallic.a          (R8G8B8A8)
RT1 = Normal_RG (Octahedral) + Roughness.b + AO.a   (R10G10B10A2 or R8G8B8A8)
RT2 = Motion Vector                    (R16G16_FLOAT)
Depth = D32_FLOAT
```

```hlsl
// Shaders/Clustered/LightingPass.hlsl
#include "ClusteredCommon.hlsli"
#include "../BRDF/BRDFGGX.hlsli"

// b5 = CBClusterUniforms (ClusteredLighting 바인드)
cbuffer CBClusterRemap : register(b5)
{
    row_major matrix View_Remap;
    row_major matrix InvProjection_Remap;
    float  zNear_R; float zFar_R; float sliceScale_R; float sliceBias_R;
    uint3  gridDim_R; uint numLights_R;
    float2 screenSize_R; float2 tilePx_R;
};

Texture2D                   g_GBuffer0  : register(t0);
Texture2D                   g_GBuffer1  : register(t1);
Texture2D                   g_Depth     : register(t2);
StructuredBuffer<uint2>     g_ClusterInfo : register(t8);
StructuredBuffer<uint>      g_IndexList   : register(t9);
StructuredBuffer<GPULight>  g_Lights      : register(t10);

SamplerState g_Point : register(s0);

struct VS_Fullscreen
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float3 DecodeOctNormal(float2 e)
{
    float3 n = float3(e, 1.0f - abs(e.x) - abs(e.y));
    if (n.z < 0.0f)
        n.xy = (1.0f - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}

float4 PS(VS_Fullscreen input) : SV_Target
{
    float2 uv = input.uv;

    // GBuffer fetch
    float4 g0 = g_GBuffer0.Sample(g_Point, uv);
    float4 g1 = g_GBuffer1.Sample(g_Point, uv);
    float  d  = g_Depth.Sample(g_Point, uv).r;

    float3 albedo     = g0.rgb;
    float  metallic   = g0.a;
    float3 normalV    = DecodeOctNormal(g1.rg);   // 이미 view-space 로 저장했다고 가정
    float  roughness  = max(g1.b, MIN_ROUGH);
    float  ao         = g1.a;

    // View space position 재구성
    float4 ndc  = float4(uv * 2.0f - 1.0f, d, 1.0f);
    ndc.y      *= -1.0f;
    float4 v4   = mul(ndc, InvProjection_Remap);
    float3 posV = v4.xyz / v4.w;

    // Cluster 조회
    uint cidx   = GetClusterIndex(uv * screenSize_R, posV.z);
    uint2 info  = g_ClusterInfo[cidx];
    uint  off   = info.x;
    uint  cnt   = min(info.y, MAX_LIGHTS_PER_CLUSTER);

    SurfaceData s;
    s.albedo    = albedo;
    s.metallic  = metallic;
    s.roughness = roughness;
    s.position  = posV;
    s.normal    = normalV;

    float3 V = normalize(-posV);       // view space 카메라 원점
    float3 total = 0.0f;

    for (uint i = 0; i < cnt; ++i)
    {
        uint lidx = g_IndexList[off + i];
        GPULight L = g_Lights[lidx];

        float3 lposV = mul(float4(L.vPosition, 1.0f), View_Remap).xyz;
        float3 to    = lposV - posV;
        float  dist  = length(to);
        float3 Ldir  = to / max(dist, 1e-4f);

        float atten = saturate(1.0f - dist / L.fRadius);
        atten *= atten;

        if (L.uType == LIGHT_SPOT)
        {
            float3 ldirV   = mul(float4(L.vDirection, 0.0f), View_Remap).xyz;
            float  spotCos = dot(-Ldir, ldirV);
            float  spotFade = smoothstep(L.fSpotOuter, L.fSpotInner, spotCos);
            atten *= spotFade;
        }

        float3 radiance = L.vColor * L.fIntensity * atten;
        total += EvaluateBRDF(s, V, Ldir, radiance, 0.04f, false);
    }

    // Dir light 는 cluster 밖에서 추가 (Winters 에서 sun 1개)
    // ... (CBDirLight 바인딩 후 EvaluateBRDF 호출)

    total += 0.03f * albedo * ao;   // 임시 ambient (IBL 미도입)
    return float4(total, 1.0f);
}
```

---

## 9. HLSL — `Shaders/Clustered/ClusterDebug.hlsl`

```hlsl
// Shaders/Clustered/ClusterDebug.hlsl
#include "ClusteredCommon.hlsli"

Texture2D                 g_Depth       : register(t0);
StructuredBuffer<uint2>   g_ClusterInfo : register(t1);
SamplerState              g_Point       : register(s0);

cbuffer CBClusterRemap2 : register(b5)
{
    row_major matrix View_D;
    row_major matrix InvProj_D;
    float zN_D; float zF_D; float ss_D; float sb_D;
    uint3 gd_D; uint nl_D;
    float2 screen_D; float2 tile_D;
};

struct VS_Fullscreen { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

float4 PS(VS_Fullscreen i) : SV_Target
{
    float d = g_Depth.Sample(g_Point, i.uv).r;
    float4 ndc = float4(i.uv*2-1, d, 1);
    ndc.y *= -1;
    float4 v4 = mul(ndc, InvProj_D);
    float  vz = v4.z / v4.w;

    uint cidx   = (i.uv.x < 0 || i.uv.x > 1) ? 0u : 0u;    // dummy
    cidx = 0u;
    {
        uint zS = min(uint(max(log2(vz) * ss_D + sb_D, 0.0f)), CLUSTER_Z - 1u);
        uint2 tile = uint2(i.uv * screen_D / tile_D);
        tile.x = min(tile.x, CLUSTER_X - 1u);
        tile.y = min(tile.y, CLUSTER_Y - 1u);
        cidx = tile.x + tile.y * CLUSTER_X + zS * CLUSTER_X * CLUSTER_Y;
    }

    uint cnt = g_ClusterInfo[cidx].y;
    float t  = saturate(float(cnt) / 32.0f);
    float3 col = lerp(float3(0,1,0), float3(1,0,0), t);
    if (cnt == 0) col = 0;
    return float4(col, 0.5f);
}
```

---

## 10. `CGameInstance` Tier 2 Getter

```cpp
// Engine/Include/GameInstance.h  (부분 추가)
class IClusteredLighting;

class WINTERS_ENGINE CGameInstance
{
public:
    // ... 기존 API 유지 ...

    // Tier 2 hot path — Client 가 포인터 캐시 후 직접 호출
    IClusteredLighting* Get_ClusteredLighting() const;

private:
    std::unique_ptr<class CClusteredLighting> m_pClusteredLighting;
};
```

```cpp
// Engine/Private/GameInstance.cpp (부분 추가, CEngineApp::Initialize 에서 호출)
void CGameInstance::InitClusteredLighting_Internal(ID3D11Device* pDev, ID3D11DeviceContext* pCtx)
{
    m_pClusteredLighting = CClusteredLighting::Create(pDev, pCtx);
}

IClusteredLighting* CGameInstance::Get_ClusteredLighting() const
{
    return m_pClusteredLighting.get();
}
```

> **CLAUDE.md 원칙 준수**: `WINTERS_ENGINE` 마크는 `CGameInstance` 와 인터페이스 `IClusteredLighting` 만. `CClusteredLighting` 구체 클래스는 Engine DLL 내부 전용이지만 편의상 헤더는 공개. Client TU 는 인터페이스만 참조.

---

## 11. 프레임 통합 순서

```cpp
// Engine/Private/Framework/CEngineApp.cpp  Render()
void CEngineApp::Render()
{
    auto* pCL = m_pGameInstance->Get_ClusteredLighting();

    // (0) 프로젝션 변경 시만 Rebuild
    pCL->RebuildClusterAABBs(m_camProj, m_fNear, m_fFar, m_uW, m_uH);

    // (1) Light Collection (CPU) — Scene 이 GPULight 리스트 제출
    std::vector<GPULight> lights;
    m_pScene->CollectLights(lights);
    pCL->UploadLights(lights.data(), (u32_t)lights.size(), m_camView);

    // (2) Cull
    pCL->CullLights();

    // (3) GBuffer Pass (Stage 2 에 정의됨)
    m_pGBufferPass->Draw(m_pScene);

    // (4) Lighting Pass — fullscreen triangle
    BindGBufferSRVs(0, 3);
    pCL->BindForLightingPass(8);   // t8,t9,t10
    m_pFullscreenTri->Draw(L"Shaders/Clustered/LightingPass.hlsl");

    // (5) TAA / Bloom / Tonemap (Stage 7)

    // (6) UI / ImGui
    m_pImGuiLayer->OnRender();
}
```

---

## 12. 메모리 예산

| 버퍼 | 크기 | 산식 |
|------|------|------|
| `m_AABBs` | 110 KB | 32 B × 3456 |
| `m_Lights` | 64 KB | 64 B × 1024 |
| `m_GlobalIndex` | 432 KB | 4 B × 110592 |
| `m_ClusterInfo` | 27 KB | 8 B × 3456 |
| `m_AtomicCounter` | 4 B | — |
| cbuffer | 256 B | — |
| **합계** | **≈ 633 KB** | 1 MB 미만 |

MOBA 1080p 기준 충분.

---

## 13. ImGui 튜너

```cpp
// Client/Private/Scene/Scene_InGame.cpp  OnImGui()
ImGui::Begin("Clustered Lighting");

ImGui::Text("Grid: %ux%ux%u = %u clusters", CLUSTER_X, CLUSTER_Y, CLUSTER_Z, CLUSTER_COUNT);
ImGui::Text("Lights uploaded: %u / %u", m_uLastLightCount, MAX_LIGHTS_TOTAL);
ImGui::Text("Max lights/cluster: %u", MAX_LIGHTS_PER_CLUSTER);

ImGui::Checkbox("Heatmap overlay",    &m_bShowClusterHeatmap);
ImGui::Checkbox("Force Rebuild AABB", &m_bForceRebuild);

if (ImGui::Button("Add 10 random point lights"))
    AddRandomLights(10);

if (ImGui::Button("Clear non-sun lights"))
    m_Lights.resize(1);    // keep sun at index 0

ImGui::End();
```

---

## 14. 디버깅 체크리스트 (Winters 특화)

| 증상 | Winters 원인 | 해결 |
|------|-------------|------|
| 라이트 전체 사라짐 | `UploadLights` 가 매 프레임 view 행렬 업데이트 안 함 | `CBClusterUniforms.matView` 갱신 확인 |
| 멀리 있는 라이트만 누락 | `log2` vs `log` 상수 불일치 | C++ `fSliceScale/fSliceBias` 도 log2 기준인지 확인 |
| 특정 cluster 만 과도 | AABB 계산 near/far 뒤바뀜 | `dir.z > 0` 가정 확인. LH +Z forward 검증 |
| Atomic overflow | `MAX_LIGHTS_PER_CLUSTER` 작음 | 32 → 64 상향 (`ClusteredTypes.h` + hlsli 동시 변경) |
| Dispatch 후 크래시 | UAV 미해제 | 모든 CS 종료 후 `CSSetUnorderedAccessViews(nullptr)` 로 분리 |
| Client 빌드 실패 `IClusteredLighting not found` | EngineSDK 미동기화 | UpdateLib.bat 재실행 |
| Engine 빌드 실패 construct_at | unique_ptr + dllexport copy | `delete` 특수 멤버 확인 (본 문서 §4) |
| Furnace Test 망가짐 | Lighting pass 가 ambient 만 남 | `EvaluateBRDF` 호출 자리 확인, cluster cnt=0 이 기본임 |

---

## 15. Stage 2 완료 기준

- [ ] `Engine/Public/Renderer/Clustered/*.h` 3 파일 + `EngineSDK` 동기화
- [ ] `Shaders/Clustered/*.hlsl` 5 파일 + Bin 동기화
- [ ] Dispatch 순서 Rebuild → Upload → Cull → GBuffer → Lighting PS 동작
- [ ] 100 point light 무작위 배치 + FPS ≥ 60 @ 1080p
- [ ] Heatmap 토글로 cluster count 시각 확인
- [ ] Furnace 테스트 (IBL 포함) 는 Stage 2.3 단계에서 정합

---

## 16. 참고

- Olsson, Billeter, Assarsson 2012 — Clustered Deferred and Forward Shading
- Persson 2013 — Practical Clustered Shading (SIGGRAPH course)
- id Tech 6/7 DOOM SIGGRAPH 발표
- Filament Documentation
- `.md\plan\graphics\01_ARCHITECTURE.md` (RenderGraph 통합은 Phase 2)

---

*문서 끝. Clustered 구조는 Stage 5 (SSR/SSAO)·Stage 6 (FFT Bloom) 을 얹을 기반.*
