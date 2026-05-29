# 2026-05-03 Week 3 상세 박제 — Track 1 Forward+ Light Cull + Track 2 RH-1

**작성일**: 2026-05-03
**상태**: 검토 대기 (계획서만 작성, 코드 변경은 codex 가 진행 / 작성자 후속 검토)
**전제**: Week 1 (BRDF.hlsli + RH-0 §1 TODO marker), Week 2 (PBR + RH-0 §2 Legacy rename) 완료
**상위 문서**: [Twin Track 계획서 §5.1](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md), [RHI 마스터 §6.2 RH-1 합격](../rhi/00_RHI_MIGRATION_MASTER.md), [Forward+ 마스터](../graphics/GGX+A/04_FORWARD_PLUS_LIGHT_CULLING.md)

---

## 0. 한 줄

> **Week 3 = T1 (Forward+ 16×16 px tile compute + 다중 라이트 PBR) + T2 (RH-1 9 인터페이스 + handle API + DX11 어댑터). 합격: 64 동적 라이트 ≤16ms + IRHIDevice* 통과 컴파일 + 신규 Get_NewRHIDevice() 동작.**

---

> **Codebase reality correction (2026-05-02):** the current engine does **not** yet have Week 2 `_Legacy` rename, compute/UAV plumbing, or the full RH-1 9-interface pack. Actual Week 3 implementation is therefore narrowed to **shared PBR opt-in + multi-light cbuffer + RH-1 seed (`RHITypes.h`, `RHIHandles.h`, `IRHIDevice.h`, `Get_NewRHIDevice()`)**. Compute-based Forward+ light culling is deferred until UAV / compute binding exists in the real codebase.

## 1. Week 2 결과 검증 (Week 3 진입 전)

```bash
# 1. PBR 셰이더 컴파일 확인
ls Shaders/Mesh3D_PBR.hlsl Shaders/Skinned3D_PBR.hlsl Shaders/BRDF/BRDF_GGX.hlsli

# 2. cbuffer 확장 검증
grep -A 8 "struct CBPerFrame" Engine/Public/RHI/DX11/DX11ConstantBuffer.h
# 기대: viewProjection + cameraWorld + lightDirWorld + lightColor + lightIntensity (112B)

# 3. _Legacy rename 검증
rg "\\bGet_RHIDevice\\(\\)" Engine Client | grep -v _Legacy | grep -v "GameInstance\\.h\\|GameInstance\\.cpp" | wc -l
# 기대: 0 (모든 caller 가 _Legacy 호출)

# 4. 빌드 통과
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
# 기대: error 0 (deprecated warning 다수 OK)

# 5. 런타임: 이렐리아 ChampionTuner Metallic/Roughness 슬라이더 시각 변화
```

5개 모두 통과 시 Week 3 진입.

---

## 2. Week 3 작업 매트릭스

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T1.1** | LightData 구조 (PointLight/SpotLight) 신설 | `Engine/Public/Renderer/LightData.h` | W2 완료 |
| **T1.2** | CBLightCull cbuffer struct 신설 | `Engine/Public/RHI/DX11/DX11ConstantBuffer.h` 확장 | T1.1 |
| **T1.3** | LightCullCS.hlsl 신설 (16×16 tile compute) | `Shaders/LightCull/LightCullCS.hlsl` | T1.1, T1.2 |
| **T1.4** | CLightCullSystem 신설 | `Engine/Public/Renderer/LightCullSystem.h` + `.cpp` | T1.1~T1.3 |
| **T1.5** | Mesh3D_PBR / Skinned3D_PBR PS 변경 (다중 라이트) | `Shaders/Mesh3D_PBR.hlsl`, `Shaders/Skinned3D_PBR.hlsl` | T1.4 |
| **T1.6** | ChampionTuner 라이트 추가/제거 슬라이더 | UI 패널 | T1.4, T1.5 |
| **T2.1** | `Engine/Public/RHI/RHITypes.h` 신설 (eRHIFormat 등 enum) | 신설 | (독립) |
| **T2.2** | `Engine/Public/RHI/RHIDescriptors.h` 신설 (RHIBufferDesc 등) | 신설 | T2.1 |
| **T2.3** | `Engine/Public/RHI/RHIHandles.h` 신설 (handle 32+32 bit) | 신설 | T2.1 |
| **T2.4** | `Engine/Public/RHI/IRHIDevice.h` 신설 | 신설 | T2.1~T2.3 |
| **T2.5** | `Engine/Public/RHI/IRHIBuffer.h` / `IRHITexture.h` / `IRHIShader.h` / `IRHISampler.h` 신설 | 신설 | T2.1~T2.3 |
| **T2.6** | `IRHISwapChain.h` / `IRHIQueue.h` 신설 | 신설 | T2.4 |
| **T2.7** | CDX11Device : public IRHIDevice (다중 상속 + IRHIDevice 메서드 구현) | `Engine/Public/RHI/CDX11Device.h` + `.cpp` | T2.4~T2.6 |
| **T2.8** | CGameInstance::Get_NewRHIDevice() -> IRHIDevice* 신규 | `Engine/Include/GameInstance.h` + `.cpp` | T2.7 |

**병렬**: T1 (T1.1~T1.6) ↔ T2 (T2.1~T2.8) 는 독립. 같은 작업자면 sequential, 두 작업자면 parallel.

---

## 3. Track 1 — Forward+ Light Cull

### 3.1 알고리즘 개요

```
[1] CPU: All lights (point/spot) → StructuredBuffer<PointLight>
[2] GPU CS: 16×16 px tile 마다 frustum sphere test → per-tile light index list
[3] GPU PS: 픽셀이 속한 tile → light index list 순회 → 다중 라이트 BRDF 누적
```

- Tile 크기 16×16 px (산업 표준, AMD/Olsson 2012 권장)
- Per-tile 최대 라이트 수: 256 (오버할 시 saturate)
- 화면 1920×1080 → 120×68 = 8160 tiles
- StructuredBuffer<uint> g_TileLightIndex 크기: 8160 × 257 = ~2M uint = ~8 MB

### 3.2 LightData 구조 (T1.1)

**파일**: `Engine/Public/Renderer/LightData.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <DirectXMath.h>

namespace Engine
{
    // 32 bytes (16-aligned, StructuredBuffer 호환)
    struct WINTERS_ENGINE PointLight
    {
        DirectX::XMFLOAT3 positionWorld;   // 0  : 12
        f32_t             radius;          // 12 : 16
        DirectX::XMFLOAT3 color;           // 16 : 28
        f32_t             intensity;       // 28 : 32
    };
    static_assert(sizeof(PointLight) == 32, "PointLight must be 32 bytes");

    // 48 bytes (W3 미사용 — W4+ Spot 라이트 진입 시 활용)
    struct WINTERS_ENGINE SpotLight
    {
        DirectX::XMFLOAT3 positionWorld;
        f32_t             radius;
        DirectX::XMFLOAT3 directionWorld;
        f32_t             innerCosAngle;
        DirectX::XMFLOAT3 color;
        f32_t             intensity;
    };
    static_assert(sizeof(SpotLight) == 48, "SpotLight must be 48 bytes");

    // CBLightCull (b0 — compute shader 전용)
    struct WINTERS_ENGINE CBLightCull
    {
        DirectX::XMFLOAT4X4 viewProj;          // 0   : 64
        DirectX::XMFLOAT4X4 viewProjInv;       // 64  : 128
        DirectX::XMFLOAT2   screenSize;        // 128 : 136
        u32_t               numLights;         // 136 : 140
        u32_t               tileCountX;        // 140 : 144
    };
    static_assert(sizeof(CBLightCull) == 144, "CBLightCull must be 144 bytes (16-aligned)");
}
```

### 3.3 LightCullCS.hlsl (T1.3)

**파일**: `Shaders/LightCull/LightCullCS.hlsl` (신설)

```hlsl
// =============================================================
// LightCullCS.hlsl — Forward+ tile light cull
//   Tile size: 16×16 px
//   Output: g_TileLightIndex[tile][0]=count, [1..]=indices
// =============================================================

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256

struct PointLight {
    float3 positionWorld;
    float  radius;
    float3 color;
    float  intensity;
};

cbuffer CBLightCull : register(b0) {
    row_major matrix g_matViewProj;
    row_major matrix g_matViewProjInv;
    float2  g_vScreenSize;
    uint    g_uNumLights;
    uint    g_uTileCountX;
};

StructuredBuffer<PointLight>  g_AllLights      : register(t0);
Texture2D<float>              g_DepthBuffer    : register(t1);
RWStructuredBuffer<uint>      g_TileLightIndex : register(u0);

groupshared uint gs_MinDepth;
groupshared uint gs_MaxDepth;
groupshared uint gs_TileLightCount;
groupshared uint gs_TileLightIndices[MAX_LIGHTS_PER_TILE];

// 화면 좌표 → World ray (ndc.z=0 near plane, ndc.z=1 far plane)
float3 ScreenToWorld(float2 ndc, float depth)
{
    float4 clip = float4(ndc, depth, 1.0f);
    float4 world = mul(clip, g_matViewProjInv);
    return world.xyz / world.w;
}

// Sphere - frustum 6 plane test (간이)
bool SphereInsideFrustum(float3 center, float radius, float3 frustumCenter, float3 frustumExtent)
{
    // AABB 근사 (실제로는 6 plane test 권장)
    float3 d = abs(center - frustumCenter) - frustumExtent;
    float dist = length(max(d, 0));
    return dist <= radius;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void CS_Main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint threadIdx   = GTid.x + GTid.y * TILE_SIZE;
    uint threadCount = TILE_SIZE * TILE_SIZE;

    // 1. 그룹 초기화
    if (threadIdx == 0) {
        gs_MinDepth = 0xFFFFFFFF;
        gs_MaxDepth = 0;
        gs_TileLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // 2. tile depth 범위
    uint2 pixelCoord = DTid.xy;
    if (pixelCoord.x < (uint)g_vScreenSize.x && pixelCoord.y < (uint)g_vScreenSize.y) {
        float depth = g_DepthBuffer.Load(uint3(pixelCoord, 0));
        uint  depthInt = asuint(depth);
        InterlockedMin(gs_MinDepth, depthInt);
        InterlockedMax(gs_MaxDepth, depthInt);
    }
    GroupMemoryBarrierWithGroupSync();

    float minDepth = asfloat(gs_MinDepth);
    float maxDepth = asfloat(gs_MaxDepth);

    // 3. tile frustum AABB (NDC → World)
    float2 tileMinNDC = float2(
        ((float)Gid.x        * TILE_SIZE) / g_vScreenSize.x * 2.0f - 1.0f,
        1.0f - ((float)(Gid.y + 1) * TILE_SIZE) / g_vScreenSize.y * 2.0f);
    float2 tileMaxNDC = float2(
        ((float)(Gid.x + 1) * TILE_SIZE) / g_vScreenSize.x * 2.0f - 1.0f,
        1.0f - ((float)Gid.y * TILE_SIZE) / g_vScreenSize.y * 2.0f);

    float3 corners[8];
    corners[0] = ScreenToWorld(tileMinNDC, minDepth);
    corners[1] = ScreenToWorld(float2(tileMaxNDC.x, tileMinNDC.y), minDepth);
    corners[2] = ScreenToWorld(float2(tileMinNDC.x, tileMaxNDC.y), minDepth);
    corners[3] = ScreenToWorld(tileMaxNDC, minDepth);
    corners[4] = ScreenToWorld(tileMinNDC, maxDepth);
    corners[5] = ScreenToWorld(float2(tileMaxNDC.x, tileMinNDC.y), maxDepth);
    corners[6] = ScreenToWorld(float2(tileMinNDC.x, tileMaxNDC.y), maxDepth);
    corners[7] = ScreenToWorld(tileMaxNDC, maxDepth);

    float3 minBox = corners[0], maxBox = corners[0];
    [unroll] for (uint c = 1; c < 8; ++c) {
        minBox = min(minBox, corners[c]);
        maxBox = max(maxBox, corners[c]);
    }
    float3 frustumCenter = (minBox + maxBox) * 0.5f;
    float3 frustumExtent = (maxBox - minBox) * 0.5f;

    // 4. Light cull (그룹 thread 가 라이트 분담)
    for (uint i = threadIdx; i < g_uNumLights; i += threadCount) {
        PointLight light = g_AllLights[i];

        if (SphereInsideFrustum(light.positionWorld, light.radius, frustumCenter, frustumExtent)) {
            uint slot;
            InterlockedAdd(gs_TileLightCount, 1, slot);
            if (slot < MAX_LIGHTS_PER_TILE) {
                gs_TileLightIndices[slot] = i;
            }
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // 5. 결과 RWStructuredBuffer 에 기록 (그룹 첫 thread 만)
    if (threadIdx == 0) {
        uint tileIdx    = Gid.x + Gid.y * g_uTileCountX;
        uint baseOffset = tileIdx * (MAX_LIGHTS_PER_TILE + 1);
        uint count      = min(gs_TileLightCount, (uint)MAX_LIGHTS_PER_TILE);
        g_TileLightIndex[baseOffset] = count;
        for (uint k = 0; k < count; ++k) {
            g_TileLightIndex[baseOffset + 1 + k] = gs_TileLightIndices[k];
        }
    }
}
```

### 3.4 CLightCullSystem (T1.4)

**파일**: `Engine/Public/Renderer/LightCullSystem.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "LightData.h"
#include <memory>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

namespace Engine
{
    template<typename T> class DX11ConstantBuffer;
    template<typename T> class CDX11StructuredBuffer;

    class WINTERS_ENGINE CLightCullSystem
    {
    public:
        ~CLightCullSystem();
        CLightCullSystem(const CLightCullSystem&) = delete;
        CLightCullSystem& operator=(const CLightCullSystem&) = delete;
        CLightCullSystem(CLightCullSystem&&) = default;
        CLightCullSystem& operator=(CLightCullSystem&&) = default;

        // ★ RH-2 TODO: ID3D11Device* → IRHIDevice*
        static std::unique_ptr<CLightCullSystem> Create(ID3D11Device* pDevice,
                                                        u32_t screenW, u32_t screenH);

        void AddLight(const PointLight& l) { m_Lights.push_back(l); m_bDirty = true; }
        void ClearLights()                  { m_Lights.clear();      m_bDirty = true; }
        const std::vector<PointLight>& GetLights() const { return m_Lights; }

        // ★ RH-2 TODO: ID3D11DeviceContext* → IRHICommandList*
        void Dispatch(ID3D11DeviceContext* pCtx,
                      const DirectX::XMFLOAT4X4& matViewProj,
                      ID3D11ShaderResourceView* pDepthSRV);

        // PS 측 SRV (g_TileLightIndex + g_AllLights)
        ID3D11ShaderResourceView* GetTileLightIndexSRV() const;
        ID3D11ShaderResourceView* GetAllLightsSRV() const;

    private:
        CLightCullSystem();
        bool_t  m_bDirty = true;
        u32_t   m_ScreenW = 0;
        u32_t   m_ScreenH = 0;
        u32_t   m_TileCountX = 0;
        u32_t   m_TileCountY = 0;

        std::vector<PointLight> m_Lights;
        std::unique_ptr<CDX11StructuredBuffer<PointLight>> m_pAllLightsSB;
        std::unique_ptr<CDX11StructuredBuffer<u32_t>>      m_pTileLightIndexSB;
        std::unique_ptr<DX11ConstantBuffer<CBLightCull>>   m_pCBuffer;
        // 컴퓨트 셰이더 핸들 (LightCullCS.hlsl)
        struct Impl; std::unique_ptr<Impl> m_pImpl;
    };
}
```

`.cpp` (요약, codex 가 박제 시 BlendStateCache.cpp / DX11StructuredBuffer 패턴 참조):

```cpp
// CLightCullSystem.cpp 요약
// - Create: AllLights SB (max 1024) + TileLightIndex RWSB (tileCount * 257 uint) + CBuffer + ComputeShader
// - Dispatch:
//     1. Dirty 시 AllLights SB 업데이트
//     2. CBLightCull update (viewProj, viewProjInv, screenSize, numLights, tileCountX)
//     3. CSSetShader + CSSetShaderResources(t0=AllLights, t1=Depth) + CSSetUnorderedAccessViews(u0=TileLightIndex)
//     4. Dispatch(tileCountX, tileCountY, 1)
//     5. UAV barrier: 다음 PS 가 SRV 로 읽도록 unbind
```

### 3.5 Mesh3D_PBR / Skinned3D_PBR PS 다중 라이트 (T1.5)

**변경**: PS 의 단방향 라이트 (W2) → tile-based 다중 라이트 (W3).

```hlsl
// (Mesh3D_PBR.hlsl + Skinned3D_PBR.hlsl 공통 변경 — PS_Main)

#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256

struct PointLight {
    float3 positionWorld;
    float  radius;
    float3 color;
    float  intensity;
};

StructuredBuffer<PointLight> g_AllLights      : register(t3);
StructuredBuffer<uint>       g_TileLightIndex : register(t4);

cbuffer CBLightCullPS : register(b4) {
    uint g_uTileCountX;
    uint g_uPad0;
    uint g_uPad1;
    uint g_uPad2;
};

// Point light attenuation (Karis 2013)
float Attenuate(float distance, float radius)
{
    float ratio = distance / radius;
    float numerator = saturate(1.0f - pow(ratio, 4.0f));
    return (numerator * numerator) / (distance * distance + 1.0f);
}

// PS_Main 본문 변경 부분
float4 PS(PS_INPUT input) : SV_TARGET
{
    // (1~3 albedo / metallic / roughness / normal 동일)
    float3 N = normalize(input.vNormalWS);
    float3 V = normalize(g_vCameraWorld - input.vWorldPos);

    // 다중 라이트 누적
    uint2 tileCoord = uint2(input.vPosition.xy) / TILE_SIZE;
    uint  tileIdx   = tileCoord.x + tileCoord.y * g_uTileCountX;
    uint  baseOffset = tileIdx * (MAX_LIGHTS_PER_TILE + 1);
    uint  lightCount = g_TileLightIndex[baseOffset];

    float3 Lo = float3(0, 0, 0);
    for (uint i = 0; i < lightCount; ++i) {
        uint lightIdx = g_TileLightIndex[baseOffset + 1 + i];
        PointLight light = g_AllLights[lightIdx];

        float3 lightVec = light.positionWorld - input.vWorldPos;
        float  distance = length(lightVec);
        if (distance > light.radius) continue;

        float3 L = lightVec / distance;
        float3 brdf = BRDF_CookTorrance(N, V, L, albedo, metallic, roughness);
        float  atten = Attenuate(distance, light.radius);
        float3 radiance = light.color * light.intensity * atten;

        Lo += brdf * radiance;
    }

    // (ambient + emissive + tone map + gamma 동일)
    return float4(color, albedoSample.a);
}
```

### 3.6 ChampionTuner 라이트 슬라이더 (T1.6)

```cpp
// ChampionTuner::OnImGui
if (ImGui::CollapsingHeader("Forward+ Lights", ImGuiTreeNodeFlags_DefaultOpen))
{
    static int sNumPointLights = 16;
    ImGui::SliderInt("Num Point Lights", &sNumPointLights, 1, 64);

    if (ImGui::Button("Spawn Lights")) {
        m_pLightCull->ClearLights();
        for (int i = 0; i < sNumPointLights; ++i) {
            PointLight l{};
            l.positionWorld = { /* random in map AABB */ };
            l.radius        = 5.0f + (rand() % 10);
            l.color         = { rand01(), rand01(), rand01() };
            l.intensity     = 5.0f;
            m_pLightCull->AddLight(l);
        }
    }

    ImGui::Text("Active lights: %u", (u32_t)m_pLightCull->GetLights().size());
}
```

### 3.7 합격 게이트 (Track 1 W3)

- ✅ LightCullCS.hlsl 컴파일 통과 (`fxc /T cs_5_0`)
- ✅ CLightCullSystem::Create + Dispatch 동작 (PIX 캡처로 UAV write 확인)
- ✅ 64 동적 라이트 spawn 시 Frame ≤16ms (1920×1080, RTX 3060급 기준)
- ✅ 라이트 색이 시각 변화 (랜덤 RGB 라이트로 검증)

---

## 4. Track 2 — RH-1 인터페이스

### 4.1 RHITypes.h (T2.1)

**파일**: `Engine/Public/RHI/RHITypes.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

namespace Engine
{
    // DXGI_FORMAT / VK_FORMAT 의 백엔드 중립 enum
    enum class eRHIFormat : u32_t
    {
        Unknown = 0,
        // 8-bit
        R8_UNorm,
        R8G8_UNorm,
        R8G8B8A8_UNorm,
        R8G8B8A8_UNorm_sRGB,
        // 16-bit
        R16_UNorm, R16_Float,
        R16G16B16A16_Float,
        // 32-bit
        R32_Float, R32_UInt,
        R32G32_Float,
        R32G32B32_Float,
        R32G32B32A32_Float,
        // Depth
        D24_UNorm_S8_UInt,
        D32_Float,
    };

    enum class eRHIBufferUsage : u32_t
    {
        Vertex          = 1 << 0,
        Index           = 1 << 1,
        Constant        = 1 << 2,
        Structured      = 1 << 3,
        UAV             = 1 << 4,
        Indirect        = 1 << 5,
    };
    inline eRHIBufferUsage operator|(eRHIBufferUsage a, eRHIBufferUsage b)
    { return (eRHIBufferUsage)((u32_t)a | (u32_t)b); }
    inline u32_t operator&(eRHIBufferUsage a, eRHIBufferUsage b)
    { return (u32_t)a & (u32_t)b; }

    enum class eRHITextureUsage : u32_t
    {
        ShaderResource  = 1 << 0,
        RenderTarget    = 1 << 1,
        DepthStencil    = 1 << 2,
        UAV             = 1 << 3,
    };

    enum class eRHIResourceState : u32_t
    {
        Common,
        VertexConstant,
        IndexBuffer,
        RenderTarget,
        DepthRead,
        DepthWrite,
        ShaderResource,
        UAV,
        CopyDest,
        CopySource,
        Present,
    };

    enum class eRHIShaderStage : u32_t
    {
        Vertex,
        Pixel,
        Compute,
        Geometry,
        Hull,
        Domain,
    };

    enum class eRHINativeType : u32_t
    {
        DX11Device,
        DX11DeviceContext,
        DX11Buffer,
        DX11Texture,
        DX11SRV,
        DX11RTV,
        DX11DSV,
        DX11UAV,
        DX11Sampler,
        // RH-5+ DX12
        DX12Device,
        DX12CommandQueue,
        DX12Resource,
        // RH-6+ Vulkan
        VulkanDevice,
        VulkanQueue,
        VulkanBuffer,
    };
}
```

### 4.2 RHIDescriptors.h (T2.2)

**파일**: `Engine/Public/RHI/RHIDescriptors.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHITypes.h"

namespace Engine
{
    struct WINTERS_ENGINE RHIBufferDesc
    {
        u32_t           sizeBytes = 0;
        u32_t           strideBytes = 0;       // structured 일 때만
        eRHIBufferUsage usage = eRHIBufferUsage::Vertex;
        bool_t          dynamic = false;       // CPU write per frame
        const char*     debugName = nullptr;
    };

    struct WINTERS_ENGINE RHITextureDesc
    {
        u32_t            width = 0;
        u32_t            height = 0;
        u32_t            depthOrArraySize = 1;
        u32_t            mipLevels = 1;
        eRHIFormat       format = eRHIFormat::R8G8B8A8_UNorm;
        eRHITextureUsage usage = eRHITextureUsage::ShaderResource;
        bool_t           cubeMap = false;
        const char*      debugName = nullptr;
    };

    struct WINTERS_ENGINE RHIShaderDesc
    {
        eRHIShaderStage stage = eRHIShaderStage::Vertex;
        const void*     bytecode = nullptr;     // borrowed, shader 객체와 동일 lifetime
        size_t          bytecodeSize = 0;
        const char*     entryPoint = "main";
        const char*     debugName = nullptr;
    };

    enum class eRHISamplerFilter : u32_t { Point, Linear, Anisotropic };
    enum class eRHISamplerAddress : u32_t { Wrap, Clamp, Mirror, Border };

    struct WINTERS_ENGINE RHISamplerDesc
    {
        eRHISamplerFilter   filter = eRHISamplerFilter::Linear;
        eRHISamplerAddress  addressU = eRHISamplerAddress::Wrap;
        eRHISamplerAddress  addressV = eRHISamplerAddress::Wrap;
        eRHISamplerAddress  addressW = eRHISamplerAddress::Wrap;
        u32_t               maxAnisotropy = 1;
    };

    // 백엔드 중립 윈도우 핸들 (HWND / xcb / wayland 추상화)
    struct WINTERS_ENGINE RHIWindowHandle
    {
        void*  nativeWindow = nullptr;          // HWND or xcb_window_t
        u32_t  width = 0;
        u32_t  height = 0;
    };
}
```

### 4.3 RHIHandles.h (T2.3)

**파일**: `Engine/Public/RHI/RHIHandles.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"

namespace Engine
{
    // 64-bit handle: 32 index + 32 generation (RH-4 정식 — RH-1 은 임시 64bit)
    template<typename Tag>
    struct WINTERS_ENGINE RHIHandle
    {
        u64_t value = 0;

        bool_t IsValid() const { return value != 0; }
        u32_t  Index()    const { return (u32_t)(value & 0xFFFFFFFF); }
        u32_t  Generation() const { return (u32_t)((value >> 32) & 0xFFFFFFFF); }

        static RHIHandle Make(u32_t idx, u32_t gen)
        {
            RHIHandle h;
            h.value = ((u64_t)gen << 32) | (u64_t)idx;
            return h;
        }

        bool_t operator==(const RHIHandle& o) const { return value == o.value; }
        bool_t operator!=(const RHIHandle& o) const { return value != o.value; }
    };

    struct BufferTag {};
    struct TextureTag {};
    struct ShaderTag {};
    struct SamplerTag {};
    struct PipelineTag {};
    struct BindGroupTag {};
    struct RenderPassTag {};

    using RHIBufferHandle      = RHIHandle<BufferTag>;
    using RHITextureHandle     = RHIHandle<TextureTag>;
    using RHIShaderHandle      = RHIHandle<ShaderTag>;
    using RHISamplerHandle     = RHIHandle<SamplerTag>;
    using RHIPipelineHandle    = RHIHandle<PipelineTag>;
    using RHIBindGroupHandle   = RHIHandle<BindGroupTag>;
    using RHIRenderPassHandle  = RHIHandle<RenderPassTag>;
}
```

### 4.4 IRHIDevice.h (T2.4)

**파일**: `Engine/Public/RHI/IRHIDevice.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHITypes.h"
#include "RHIDescriptors.h"
#include "RHIHandles.h"

namespace Engine
{
    class IRHISwapChain;
    class IRHIQueue;

    class WINTERS_ENGINE IRHIDevice
    {
    public:
        virtual ~IRHIDevice() = default;

        // ── 자원 생성 (handle 반환, Engine-owned) ──
        virtual RHIBufferHandle  CreateBuffer(const RHIBufferDesc& desc, const void* initData = nullptr) = 0;
        virtual RHITextureHandle CreateTexture(const RHITextureDesc& desc, const void* initData = nullptr) = 0;
        virtual RHIShaderHandle  CreateShader(const RHIShaderDesc& desc) = 0;
        virtual RHISamplerHandle CreateSampler(const RHISamplerDesc& desc) = 0;

        // ── 자원 해제 ──
        virtual void DestroyBuffer(RHIBufferHandle h) = 0;
        virtual void DestroyTexture(RHITextureHandle h) = 0;
        virtual void DestroyShader(RHIShaderHandle h) = 0;
        virtual void DestroySampler(RHISamplerHandle h) = 0;

        // ── 자원 업데이트 (dynamic 만) ──
        virtual void UpdateBuffer(RHIBufferHandle h, const void* data, size_t sizeBytes) = 0;

        // ── SwapChain / Queue ──
        virtual IRHISwapChain* CreateSwapChain(const RHIWindowHandle& window) = 0;
        virtual IRHIQueue*     GetGraphicsQueue() = 0;

        // ── Native 백엔드 escape (ImGui DX11 backend 등 외부 라이브러리 전용) ──
        //   borrowed pointer, AddRef X, 즉시 사용만.
        virtual void* GetNativeHandle(eRHINativeType type) = 0;

        // ── Frame ──
        virtual void BeginFrame() = 0;
        virtual void EndFrame()   = 0;
    };
}
```

### 4.5 IRHIBuffer / IRHITexture / IRHIShader / IRHISampler (T2.5)

**파일**: `Engine/Public/RHI/IRHIBuffer.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "RHIDescriptors.h"

namespace Engine
{
    class WINTERS_ENGINE IRHIBuffer
    {
    public:
        virtual ~IRHIBuffer() = default;
        virtual const RHIBufferDesc& GetDesc() const = 0;
        virtual u32_t GetSize() const = 0;
        // ★ 백엔드 native (DX11Buffer / DX12Resource / VkBuffer) 노출 — escape only
        virtual void* GetNativeHandle(eRHINativeType type) = 0;
    };

    // Shim: 기존 IBuffer.h 코드 호환 (RH-2 종료 시 제거)
    using IBuffer [[deprecated("RH-2 후 IRHIBuffer 사용")]] = IRHIBuffer;
}
```

`IRHITexture.h`:
```cpp
#pragma once
#include "WintersAPI.h"
#include "RHIDescriptors.h"

namespace Engine
{
    class WINTERS_ENGINE IRHITexture
    {
    public:
        virtual ~IRHITexture() = default;
        virtual const RHITextureDesc& GetDesc() const = 0;
        virtual u32_t GetWidth() const = 0;
        virtual u32_t GetHeight() const = 0;
        virtual void* GetNativeHandle(eRHINativeType type) = 0;
    };
}
```

`IRHIShader.h` / `IRHISampler.h`: 동일 패턴 (GetDesc + GetNativeHandle).

### 4.6 IRHISwapChain / IRHIQueue (T2.6)

```cpp
// IRHISwapChain.h
class WINTERS_ENGINE IRHISwapChain
{
public:
    virtual ~IRHISwapChain() = default;
    virtual void Present(bool_t bVSync) = 0;
    virtual u32_t GetCurrentBackBufferIndex() const = 0;
    virtual RHITextureHandle GetCurrentBackBuffer() = 0;
    virtual void Resize(u32_t w, u32_t h) = 0;
    virtual void* GetNativeHandle(eRHINativeType type) = 0;
};

// IRHIQueue.h
class WINTERS_ENGINE IRHIQueue
{
public:
    virtual ~IRHIQueue() = default;
    // RH-2 에서 Submit(IRHICommandList*) 추가
    virtual void* GetNativeHandle(eRHINativeType type) = 0;
};
```

### 4.7 CDX11Device : public IRHIDevice (T2.7)

**파일**: `Engine/Public/RHI/CDX11Device.h` (수정)

```cpp
// BEFORE (현재)
class WINTERS_ENGINE CDX11Device
{
public:
    static std::unique_ptr<CDX11Device> Create(/* ... */);
    ID3D11Device*        GetDevice() const;
    ID3D11DeviceContext* GetContext() const;
    // ...
};

// AFTER (RH-1)
//   ★ RH-2 종료 후 Engine/Private/RHI/DX11/DX11Device.h 로 이동.
class WINTERS_ENGINE CDX11Device final : public IRHIDevice
{
public:
    static std::unique_ptr<CDX11Device> Create(/* ... */);

    // ── IRHIDevice 구현 ──
    RHIBufferHandle  CreateBuffer(const RHIBufferDesc& desc, const void* initData) override;
    RHITextureHandle CreateTexture(const RHITextureDesc& desc, const void* initData) override;
    RHIShaderHandle  CreateShader(const RHIShaderDesc& desc) override;
    RHISamplerHandle CreateSampler(const RHISamplerDesc& desc) override;

    void DestroyBuffer(RHIBufferHandle h) override;
    void DestroyTexture(RHITextureHandle h) override;
    void DestroyShader(RHIShaderHandle h) override;
    void DestroySampler(RHISamplerHandle h) override;

    void UpdateBuffer(RHIBufferHandle h, const void* data, size_t sizeBytes) override;

    IRHISwapChain* CreateSwapChain(const RHIWindowHandle& window) override;
    IRHIQueue*     GetGraphicsQueue() override;

    void* GetNativeHandle(eRHINativeType type) override;
    void  BeginFrame() override;
    void  EndFrame()   override;

    // ── 기존 DX11 native 게터 (RH-2 종료 시 Public 에서 제거 후 Private 로 이동) ──
    //   ★ RH-2 TODO: Public 에서 제거.
    ID3D11Device*        GetDevice() const;
    ID3D11DeviceContext* GetContext() const;

private:
    CDX11Device();
    struct Impl; std::unique_ptr<Impl> m_pImpl;
    // resource table (handle → native ID3D11Buffer*/Texture* 매핑)
};
```

### 4.8 CGameInstance::Get_NewRHIDevice() (T2.8)

**파일**: `Engine/Include/GameInstance.h` (추가)

```cpp
// 기존 8개 _Legacy 게터 그대로 + 신규 1개 추가:

public: //RHI — RH-1 (2026-05-03): 신규 IRHIDevice* 인터페이스 게터.
    //   RH-2 종료 후 _Legacy 8개 제거 + Get_RHIDevice() 정식 rename.
    IRHIDevice* Get_NewRHIDevice();
```

`GameInstance.cpp`:
```cpp
IRHIDevice* CGameInstance::Get_NewRHIDevice()
{
    return &CEngineApp::Get().GetDevice();   // CDX11Device : public IRHIDevice
}
```

### 4.9 합격 게이트 (Track 2 W3)

- ✅ 9 인터페이스 헤더 컴파일 통과 (`IRHIDevice/Buffer/Texture/Shader/Sampler/SwapChain/Queue` + Types + Descriptors)
- ✅ `CDX11Device : public IRHIDevice` 다중 상속 빌드 통과
- ✅ handle API 1개 호출 검증: `auto h = pDevice->CreateBuffer(desc); pDevice->DestroyBuffer(h);` 컴파일
- ✅ `Get_NewRHIDevice()` 호출 후 `IRHIDevice*` 반환 + 다음 호출 시 동일 포인터
- ✅ LoL 빌드 통과 (기존 _Legacy 호출 그대로 유지, error 0)

---

## 5. 위험 시나리오

### 5.1 R-W3-1: Forward+ tile depth range 부정확
- 시나리오: 64 라이트 spawn 했는데 실제 시각 라이트 수가 적음 (cull 너무 강함)
- 원인: tile depth min/max 가 unset (depth buffer 가 cleared)
- 완화: ① opaque pass 먼저 실행 → depth 채움 ② LightCullCS 디스패치 ③ PBR PS

### 5.2 R-W3-2: StructuredBuffer UAV write contention
- 시나리오: InterlockedAdd 가 256 라이트 × 8160 tile 에서 stall
- 완화: ① MAX_LIGHTS_PER_TILE 256 → 128 줄임 (메모리 절반) ② SubGroupOp 사용 (DX12+)

### 5.3 R-W3-3: IRHIDevice 다중 상속 후 vtable 깨짐
- 시나리오: CDX11Device 가 IRHIDevice 메서드 일부 미구현 → pure virtual call crash
- 완화: ① 모든 IRHIDevice 메서드를 CDX11Device 가 override 키워드 명시 ② 빌드 시 abstract class 체크 (instanceof CDX11Device 시도)

### 5.4 R-W3-4: handle API 의 generation overflow
- 시나리오: 32-bit generation 이 4B 사용 후 wrap
- 완화: ① generation 0 reserved (invalid) ② RH-4 시점에 정식 ResourceTable + 안전 wrap

### 5.5 R-W3-5: ImGui DX11 backend 가 IRHIDevice 인터페이스 미통과
- 시나리오: ImGui_ImplDX11_Init 가 raw `ID3D11Device*` 요구
- 완화: `pDevice->GetNativeHandle(eRHINativeType::DX11Device)` escape API 사용 (borrowed pointer)

---

## 6. Week 3 통합 합격 검증

```bash
# 1. 셰이더 컴파일
ls Shaders/LightCull/LightCullCS.hlsl

# 2. RHI 인터페이스 헤더 9개
ls Engine/Public/RHI/{IRHIDevice,IRHIBuffer,IRHITexture,IRHIShader,IRHISampler,IRHISwapChain,IRHIQueue,RHITypes,RHIDescriptors,RHIHandles}.h

# 3. 빌드 통과
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
# 기대: error 0, deprecated warning 다수 OK

# 4. 런타임 (LoL 실행 + 이렐리아 진입)
#    ChampionTuner Forward+ Lights 섹션:
#      - Spawn 64 lights
#      - Frame ≤16ms 확인 (F3 Profiler)
#      - 각 라이트 색이 시각 변화 (랜덤 RGB)

# 5. RH-1 동작 검증
#    auto* p = pGI->Get_NewRHIDevice();
#    if (p) printf("RHI Device OK: %p\n", p);
```

---

## 7. 부록 A — Week 3 진입 체크리스트

```
[ ] Week 2 결과 검증 통과
[ ] devenv.exe 종료 + git: feature/2026-05-03-week3 branch
[ ] Engine 단독 빌드 1회 → EngineSDK/inc 동기화

Track 1 (Forward+):
[ ] §3.2 LightData.h 신설 (PointLight + CBLightCull POD)
[ ] §3.3 LightCullCS.hlsl 신설 (16×16 tile compute)
[ ] §3.4 CLightCullSystem.h + .cpp 신설
[ ] §3.5 Mesh3D_PBR.hlsl + Skinned3D_PBR.hlsl PS 다중 라이트 변경
[ ] §3.6 ChampionTuner Forward+ Lights 슬라이더 + Spawn 버튼

Track 2 (RH-1):
[ ] §4.1 RHITypes.h (eRHIFormat / eRHIBufferUsage 등)
[ ] §4.2 RHIDescriptors.h (RHIBufferDesc / TextureDesc 등)
[ ] §4.3 RHIHandles.h (64-bit handle template)
[ ] §4.4 IRHIDevice.h (자원 생성/해제 + native escape)
[ ] §4.5 IRHIBuffer / Texture / Shader / Sampler 4개
[ ] §4.6 IRHISwapChain / IRHIQueue 2개
[ ] §4.7 CDX11Device : public IRHIDevice 다중 상속
[ ] §4.8 GameInstance::Get_NewRHIDevice() 신규

검증:
[ ] §6.3 빌드 error 0
[ ] §6.4 런타임 64 라이트 ≤16ms
[ ] §6.5 IRHIDevice* 통과 컴파일
```

---

## 8. 한 줄

> **Week 3 = T1 (LightData + LightCullCS + CLightCullSystem + PBR PS 다중 라이트 + Tuner) + T2 (Types/Descriptors/Handles + IRHIDevice/Buffer/Texture/Shader/Sampler/SwapChain/Queue 인터페이스 + CDX11Device : public IRHIDevice + Get_NewRHIDevice). 합격: 64 라이트 ≤16ms + 9 인터페이스 컴파일 + LoL 빌드 통과.**

---

## 끝.
