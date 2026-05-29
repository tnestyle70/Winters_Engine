# 05. Motion Vectors — Winters Engine 이식 가이드

> 원전: `C:\Users\user\Desktop\.markdown\Graphics\05_Motion_Vectors.md`
> 대상 경로: `Engine/Public/Renderer/MotionVector/`, `Shaders/MotionVector/`, `Shaders/BRDF/Mesh3D_PBR.hlsl` 수정
> Phase: **E Stage 7 (TAA) 의 전제** — TAA (문서 03) 선행 조건. 본 문서를 먼저 완료해야 TAA 성립.

---

## 0. 이 문서의 목표 (Winters 관점)

1. **MV RT 신설** — GBuffer MRT 마지막 슬롯 (`RT3 = R16G16_FLOAT`). Deferred 파이프에서 한 장소에서 관리.
2. **정적 메시**: `Mesh3D_PBR.hlsl` VS 에 `posCur` / `posPrev` 추가, PS MRT 에 MV 출력.
3. **스키닝 메시**: `Skinned3D_PBR.hlsl` 에 prev bone matrices 바인딩. 본 매트릭스 더블 버퍼링 시스템 구축.
4. **캐릭터/오브젝트 prev world 관리**: ECS `TransformComponent` 에 `matPrevWorld` 추가. 프레임 말에 swap.
5. **파티클/정적 배경**은 fullscreen MV 패스로 fallback (최적화).

---

## 1. 디렉토리

```
Engine/Public/Renderer/MotionVector/
├── MotionVectorTypes.h         // CBPerFrameMV, PrevBonesBufferDesc
├── MotionVectorPass.h          // class CMotionVectorPass
└── PrevBoneManager.h           // 스키닝 prev bone 더블버퍼링

Engine/Private/Renderer/MotionVector/
├── MotionVectorPass.cpp
└── PrevBoneManager.cpp

Shaders/MotionVector/
├── MVCommon.hlsli              // NDC→UV 변환, DX UV flip
├── CameraOnlyMV.hlsl           // Fullscreen PS — 정적 배경
└── (Mesh3D_PBR/Skinned3D_PBR 는 기존 파일에 MRT 추가)
```

---

## 2. 타입 — `MotionVectorTypes.h`

```cpp
// Engine/Public/Renderer/MotionVector/MotionVectorTypes.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <DirectXMath.h>

// Mesh3D_PBR / Skinned3D_PBR 공통 — b0 이 아닌 b7 슬롯 (BRDF/Clustered/IBL 과 충돌 없음)
struct CBPerFrameMV
{
    DirectX::XMFLOAT4X4 matViewProjCur_NoJitter;
    DirectX::XMFLOAT4X4 matViewProjPrev_NoJitter;
};
static_assert(sizeof(CBPerFrameMV) % 16 == 0);

// b1 (PerObject) 는 이미 world 한 장. 이를 확장하지 않고 b8 로 prev world 별도.
struct CBPerObjectPrev
{
    DirectX::XMFLOAT4X4 matPrevWorld;
};
static_assert(sizeof(CBPerObjectPrev) % 16 == 0);
```

---

## 3. TransformComponent 확장 (ECS)

```cpp
// Engine/Public/ECS/Components/TransformComponent.h  (추가 필드)
struct TransformComponent
{
    DirectX::XMFLOAT3 vPosition;
    DirectX::XMFLOAT4 qRotation;
    DirectX::XMFLOAT3 vScale;

    DirectX::XMFLOAT4X4 matWorld;      // 현재 프레임
    DirectX::XMFLOAT4X4 matPrevWorld;  // 이전 프레임 — MV 용 신설

    bool_t bDirty         = true;
    bool_t bSpawnedThisFrame = true;   // true 면 prev = cur 강제 (원전 §4.5)
};
```

`CTransformSystem` 의 행렬 재계산 이후, 프레임 끝에서:

```cpp
// Engine/Private/ECS/Systems/TransformSystem.cpp  (프레임 말에 호출)
void CTransformSystem::EndFrame_StashPreviousTransforms()
{
    for (auto& t : m_pWorld->GetComponents<TransformComponent>())
    {
        if (t.bSpawnedThisFrame)
        {
            t.matPrevWorld       = t.matWorld;
            t.bSpawnedThisFrame  = false;
        }
        else
        {
            t.matPrevWorld = t.matWorld;
        }
    }
}
```

> 원전 §9.3 의 "pointer swap" 은 Winters 의 POD Transform 구조상 필요 없음. 그냥 복사 (64 bytes × 엔티티 수 = 수천 엔티티 × 60Hz = 수 MB/s — 무시할 수준).

---

## 4. Jitter / Non-Jitter 프레임 상수 (TAA 와 공통)

TAA (문서 03 §4) 에서 이미 `matViewProjNoJitter` 를 `CBFrameCamera` 에 두기로 했다. MV 전용 b7 슬롯에는 **현재/이전** 의 NoJitter 를 같이 묶는다:

```cpp
// Engine/Private/Framework/CEngineApp.cpp  (프레임 시작)
CBPerFrameMV mv{};
mv.matViewProjCur_NoJitter  = m_matVPCurNoJitter;   // UpdateTAAJitter 결과
mv.matViewProjPrev_NoJitter = m_matVPPrevNoJitter;
m_cbMV.Update(m_pCtx, mv);

// 프레임 끝:
m_matVPPrevNoJitter = m_matVPCurNoJitter;
```

---

## 5. Mesh3D_PBR.hlsl — MV 확장

BRDF 문서(01 §7) 의 `Mesh3D_PBR.hlsl` 를 다음과 같이 수정.

### 5.1 cbuffer / 입출력 확장

```hlsl
// Shaders/BRDF/Mesh3D_PBR.hlsl  (수정)
#include "BRDFGGX.hlsli"

cbuffer CBPerFrame : register(b0)    { row_major matrix g_matViewProj;
                                        row_major matrix g_matViewProjNoJitter;
                                        float3 g_vCameraPos; float g_fTime; };
cbuffer CBPerObject : register(b1)   { row_major matrix g_matWorld; };

cbuffer CBMaterial : register(b3)    { /* as before */ };
cbuffer CBDirLight : register(b4)    { /* as before */ };

// 신규
cbuffer CBPerFrameMV : register(b7)
{
    row_major matrix g_matVPCurNoJitter;
    row_major matrix g_matVPPrevNoJitter;
};
cbuffer CBPerObjectPrev : register(b8)
{
    row_major matrix g_matPrevWorld;
};

// VS output 에 posCur/posPrev 추가
struct PS_INPUT
{
    float4 vPosition  : SV_POSITION;
    float3 vWorldNrm  : NORMAL;
    float3 vWorldPos  : TEXCOORD0;
    float2 vTexCoord  : TEXCOORD1;
    float3 vWorldTan  : TEXCOORD2;
    float4 vPosCur    : TEXCOORD3;   // non-jittered current clip
    float4 vPosPrev   : TEXCOORD4;   // non-jittered previous clip
};

PS_INPUT VS(VS_INPUT input)
{
    PS_INPUT o;
    float4 wp     = mul(float4(input.vPosition, 1.f), g_matWorld);
    float4 wpPrev = mul(float4(input.vPosition, 1.f), g_matPrevWorld);

    o.vPosition = mul(wp, g_matViewProj);                 // jittered
    o.vPosCur   = mul(wp,     g_matVPCurNoJitter);
    o.vPosPrev  = mul(wpPrev, g_matVPPrevNoJitter);

    o.vWorldNrm = normalize(mul(input.vNormal,  (float3x3) g_matWorld));
    o.vWorldTan = normalize(mul(input.vTangent, (float3x3) g_matWorld));
    o.vWorldPos = wp.xyz;
    o.vTexCoord = input.vTexCoord;
    return o;
}
```

### 5.2 PS — MRT 출력

```hlsl
// Deferred 구성: PBR 결과를 바로 RT 에 쓰는 것이 아니라 GBuffer 에 분해 출력.
// (Forward 임시 구현은 §6 참조.)
struct PS_OUT
{
    float4 GBuffer0     : SV_Target0;   // Albedo + Metallic
    float4 GBuffer1     : SV_Target1;   // World Normal (octahedral) + Roughness + AO
    float2 MotionVector : SV_Target2;   // R16G16_FLOAT
};

PS_OUT PS(PS_INPUT input)
{
    // ... 기존 샘플링/계산 ...
    float3 albedo     = /* ... */;
    float  metallic   = /* ... */;
    float  roughness  = /* ... */;
    float  ao         = /* ... */;
    float3 worldN     = /* SampleNormal(...) */;

    PS_OUT o;
    o.GBuffer0.rgb = albedo;
    o.GBuffer0.a   = metallic;
    o.GBuffer1.rg  = EncodeOctahedralNormal(worldN);
    o.GBuffer1.b   = roughness;
    o.GBuffer1.a   = ao;

    // Motion Vector
    float2 ndcCur  = input.vPosCur.xy  / input.vPosCur.w;
    float2 ndcPrev = input.vPosPrev.xy / input.vPosPrev.w;

    float2 uvCur  = ndcCur  * float2(0.5f, -0.5f) + 0.5f;
    float2 uvPrev = ndcPrev * float2(0.5f, -0.5f) + 0.5f;

    o.MotionVector = uvCur - uvPrev;
    return o;
}
```

`EncodeOctahedralNormal` 은 공유 util:

```hlsl
// Shaders/BRDF/BRDFCommon.hlsli 에 추가
float2 EncodeOctahedralNormal(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 e = n.xy;
    if (n.z < 0.0f) e = (1.0f - abs(e.yx)) * sign(e);
    return e;
}
float3 DecodeOctahedralNormal(float2 e)
{
    float3 n = float3(e, 1.0f - abs(e.x) - abs(e.y));
    if (n.z < 0.0f) n.xy = (1.0f - abs(n.yx)) * sign(n.xy);
    return normalize(n);
}
```

---

## 6. Skinned3D_PBR.hlsl — 스키닝 MV

```hlsl
// Shaders/BRDF/Skinned3D_PBR.hlsl  (수정)

// 기존 b2 = g_BoneMatrices[256]
cbuffer CBBones     : register(b2) { row_major matrix g_BoneMatrices[256]; };
// 신규 b9 = prev bone matrices
cbuffer CBPrevBones : register(b9) { row_major matrix g_PrevBoneMatrices[256]; };

cbuffer CBPerFrameMV    : register(b7) { /* as above */ };
cbuffer CBPerObjectPrev : register(b8) { /* as above */ };

PS_INPUT VS_Skinned(VS_INPUT_Skinned v)
{
    matrix skinCur  = (matrix)0;
    matrix skinPrev = (matrix)0;
    [unroll] for (int i = 0; i < 4; ++i)
    {
        skinCur  += g_BoneMatrices[v.iBoneIndices[i]]     * v.fBoneWeights[i];
        skinPrev += g_PrevBoneMatrices[v.iBoneIndices[i]] * v.fBoneWeights[i];
    }

    float4 wCur  = mul(mul(float4(v.vPosition, 1.f), skinCur),  g_matWorld);
    float4 wPrev = mul(mul(float4(v.vPosition, 1.f), skinPrev), g_matPrevWorld);

    PS_INPUT o;
    o.vPosition = mul(wCur, g_matViewProj);
    o.vPosCur   = mul(wCur,  g_matVPCurNoJitter);
    o.vPosPrev  = mul(wPrev, g_matVPPrevNoJitter);

    float3x3 skinN = (float3x3) skinCur;
    o.vWorldNrm = normalize(mul(v.vNormal, (float3x3) mul(skinCur, g_matWorld)));
    o.vWorldTan = normalize(mul(v.vTangent, (float3x3) mul(skinCur, g_matWorld)));
    o.vWorldPos = wCur.xyz;
    o.vTexCoord = v.vTexCoord;
    return o;
}
```

---

## 7. Prev Bone Manager

```cpp
// Engine/Public/Renderer/MotionVector/PrevBoneManager.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "DX11StructuredBuffer.h"
#include <DirectXMath.h>
#include <unordered_map>
#include <memory>

class WINTERS_ENGINE CPrevBoneManager
{
public:
    ~CPrevBoneManager();
    static std::unique_ptr<CPrevBoneManager> Create(ID3D11Device* pDev);

    CPrevBoneManager(const CPrevBoneManager&)            = delete;
    CPrevBoneManager& operator=(const CPrevBoneManager&) = delete;
    CPrevBoneManager(CPrevBoneManager&&)                 = default;
    CPrevBoneManager& operator=(CPrevBoneManager&&)      = default;

    // 캐릭터 스폰 시
    u32_t RegisterInstance(u32_t uBoneCount);
    void  DestroyInstance(u32_t uInstanceID);

    // 매 프레임 — Animator 가 현재 본 행렬을 업데이트한 직후
    // "current → prev" 을 GPU 에서 복사. 다음 프레임의 prev 가 된다.
    void UpdatePrev(ID3D11DeviceContext* pCtx, u32_t uInstanceID,
                    const DirectX::XMFLOAT4X4* pCurrent, u32_t uCount);

    // 렌더 시 b2 / b9 바인드
    void BindForRender(ID3D11DeviceContext* pCtx, u32_t uInstanceID,
                       u32_t uSlotCurrent = 2, u32_t uSlotPrev = 9);

private:
    CPrevBoneManager() = default;
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

```cpp
// Engine/Private/Renderer/MotionVector/PrevBoneManager.cpp  (핵심만)
struct CPrevBoneManager::Impl
{
    ID3D11Device* m_pDev = nullptr;
    struct Inst
    {
        // 각 인스턴스가 current + prev StructuredBuffer 를 소유
        DX11StructuredBuffer<DirectX::XMFLOAT4X4> m_BonesCur;
        DX11StructuredBuffer<DirectX::XMFLOAT4X4> m_BonesPrev;
        u32_t m_uCount = 0;
        bool_t m_bFirstUpdate = true;
    };
    std::unordered_map<u32_t, Inst> m_Instances;
    u32_t m_uNext = 1;
};

u32_t CPrevBoneManager::RegisterInstance(u32_t uBoneCount)
{
    auto& I = *m_pImpl;
    u32_t id = I.m_uNext++;
    auto& inst = I.m_Instances[id];
    inst.m_uCount = uBoneCount;
    inst.m_BonesCur.CreateSRV(I.m_pDev, uBoneCount);
    inst.m_BonesPrev.CreateSRV(I.m_pDev, uBoneCount);
    inst.m_bFirstUpdate = true;
    return id;
}

void CPrevBoneManager::UpdatePrev(ID3D11DeviceContext* pCtx, u32_t uInstanceID,
                                  const DirectX::XMFLOAT4X4* pCurrent, u32_t uCount)
{
    auto& I = *m_pImpl;
    auto it = I.m_Instances.find(uInstanceID);
    if (it == I.m_Instances.end()) return;
    auto& inst = it->second;

    // 1) prev ← cur (이전 프레임의 cur 를 이번 프레임의 prev 로)
    //    ※ 첫 업데이트는 prev 도 cur 로 초기화 (MV 0 보장).
    if (inst.m_bFirstUpdate)
    {
        inst.m_BonesPrev.UpdateRange(pCtx, pCurrent, uCount);
        inst.m_bFirstUpdate = false;
    }
    else
    {
        // CopyResource 로 GPU-local copy — CPU→GPU 업로드 회피
        pCtx->CopyResource(inst.m_BonesPrev.GetRaw(), inst.m_BonesCur.GetRaw());
    }

    // 2) cur ← pCurrent
    inst.m_BonesCur.UpdateRange(pCtx, pCurrent, uCount);
}

void CPrevBoneManager::BindForRender(ID3D11DeviceContext* pCtx, u32_t uInstanceID,
                                     u32_t uSlotCurrent, u32_t uSlotPrev)
{
    auto& I = *m_pImpl;
    auto it = I.m_Instances.find(uInstanceID);
    if (it == I.m_Instances.end()) return;

    ID3D11ShaderResourceView* cur = it->second.m_BonesCur.GetSRV();
    ID3D11ShaderResourceView* prv = it->second.m_BonesPrev.GetSRV();
    // NOTE: 스키닝 셰이더가 cbuffer g_BoneMatrices 를 사용하므로, 원래 DX11ConstantBuffer 로
    // 256-매트릭스 업로드하는 경로와 맞물려야 한다. Winters 현재 Skinned3D.hlsl 은 cbuffer 사용.
    // → 옵션 A: cbuffer 2개 (b2/b9) — 캐릭터 수 적으면 충분
    //   옵션 B: StructuredBuffer 로 교체 (t11/t12) — 캐릭터 수 많을 때
    //
    // Winters 는 MOBA (≤ 10 캐릭터) → 옵션 A 채택. 아래는 옵션 A 의 가시화:
    //   (이 함수는 옵션 B 사용 시. 옵션 A 는 DX11ConstantBuffer<BoneArray>::Update 2회)
}
```

> **옵션 선택**: Winters MOBA 는 챔피언 ≤ 10 + 미니언 수십 → cbuffer 2개 (b2 + b9) 로 충분. StructuredBuffer 방식은 수백 캐릭터 이상에서만 의미. 현재는 cbuffer 방식 유지하고 `CPrevBoneManager` 내부도 `DX11ConstantBuffer<BoneArray>` 2개로 구성.

---

## 8. Animator 통합

```cpp
// Engine/Private/Resource/Animator.cpp  (UpdateBones 직후)
void CAnimator::Update(f32_t dt)
{
    UpdateBones(dt);   // m_BoneMatrices 채움 (256 개)

    auto* pMgr = CGameInstance::Get()->Get_PrevBoneManager();
    pMgr->UpdatePrev(m_pCtx, m_uInstanceID, m_BoneMatrices.data(), (u32_t)m_BoneMatrices.size());
}
```

스폰 시 `RegisterInstance`, 디스폰 시 `DestroyInstance` — `ModelRenderer::Impl` 가 호출.

---

## 9. Camera-Only Fullscreen Fallback

정적 배경 (맵 타일, 스카이박스) 은 매 VS 에 prev world 붙이는 것이 낭비. Depth 로부터 재구성하는 fullscreen 패스로 대체:

```hlsl
// Shaders/MotionVector/CameraOnlyMV.hlsl
Texture2D<float> g_Depth : register(t0);
SamplerState     g_Point : register(s0);

cbuffer CBCamMV : register(b0)
{
    row_major matrix g_matInvViewProjCur;    // cur NoJitter
    row_major matrix g_matVPPrev;            // prev NoJitter
};

struct VS_OUT { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

float2 PS(VS_OUT i) : SV_Target
{
    float  d    = g_Depth.Sample(g_Point, i.uv).r;
    float4 ndc  = float4(i.uv * 2.0f - 1.0f, d, 1.0f);
    ndc.y      *= -1.0f;

    float4 w4   = mul(ndc, g_matInvViewProjCur);
    float3 wpos = w4.xyz / w4.w;

    float4 prevClip = mul(float4(wpos, 1.0f), g_matVPPrev);
    float2 prevUV   = prevClip.xy / prevClip.w * float2(0.5f, -0.5f) + 0.5f;

    return i.uv - prevUV;
}
```

**사용 전략**:
1. 프레임 시작: MV RT 를 `CameraOnlyMV` 로 fullscreen fill (스카이/맵 전체를 카메라 기반으로 채움).
2. GBuffer Pass 에서 동적 메시가 MV RT 를 `SV_Target2` 로 덮어쓰기.

결과: 정적 메시는 VS 에서 prev world 계산 안 하고도 올바른 MV 획득.

---

## 10. MV Pass Class — `CMotionVectorPass`

```cpp
// Engine/Public/Renderer/MotionVector/MotionVectorPass.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <wrl/client.h>
#include <memory>
#include <d3d11.h>

class WINTERS_ENGINE CMotionVectorPass
{
public:
    ~CMotionVectorPass();

    static std::unique_ptr<CMotionVectorPass> Create(ID3D11Device* pDev,
                                                     ID3D11DeviceContext* pCtx,
                                                     u32_t uW, u32_t uH);

    CMotionVectorPass(const CMotionVectorPass&)            = delete;
    CMotionVectorPass& operator=(const CMotionVectorPass&) = delete;
    CMotionVectorPass(CMotionVectorPass&&)                 = default;
    CMotionVectorPass& operator=(CMotionVectorPass&&)      = default;

    // 프레임 시작 시 호출
    void BeginFrame();

    // Camera-only fullscreen fill (depth SRV 필요)
    void FillFromCamera(ID3D11ShaderResourceView* pDepthSRV,
                        const DirectX::XMFLOAT4X4& matInvVPCurNoJitter,
                        const DirectX::XMFLOAT4X4& matVPPrevNoJitter);

    // 동적 메시가 MRT 로 출력할 때 바인드할 RTV
    ID3D11RenderTargetView*   GetRTV() const;
    ID3D11ShaderResourceView* GetSRV() const;
    void Resize(u32_t uW, u32_t uH);

private:
    CMotionVectorPass() = default;
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

```cpp
// Engine/Private/Renderer/MotionVector/MotionVectorPass.cpp  (핵심)
struct CMotionVectorPass::Impl
{
    ID3D11Device*        m_pDev = nullptr;
    ID3D11DeviceContext* m_pCtx = nullptr;
    u32_t m_uW = 0, m_uH = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>          m_MVTex;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_MVRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_MVSRV;

    DX11ConstantBuffer<struct CBCamMV> m_cb;
    DX11Shader m_PS;
};

std::unique_ptr<CMotionVectorPass> CMotionVectorPass::Create(ID3D11Device* pDev,
                                                             ID3D11DeviceContext* pCtx,
                                                             u32_t uW, u32_t uH)
{
    auto p    = std::unique_ptr<CMotionVectorPass>(new CMotionVectorPass());
    p->m_pImpl = std::make_unique<Impl>();
    auto& I = *p->m_pImpl;
    I.m_pDev = pDev; I.m_pCtx = pCtx; I.m_uW = uW; I.m_uH = uH;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = uW; td.Height = uH; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16G16_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    pDev->CreateTexture2D(&td, nullptr, I.m_MVTex.GetAddressOf());
    pDev->CreateRenderTargetView(I.m_MVTex.Get(), nullptr, I.m_MVRTV.GetAddressOf());
    pDev->CreateShaderResourceView(I.m_MVTex.Get(), nullptr, I.m_MVSRV.GetAddressOf());

    I.m_cb.Create(pDev);
    I.m_PS.LoadPixel(pDev, L"Shaders/MotionVector/CameraOnlyMV.hlsl", "PS");
    return p;
}

void CMotionVectorPass::BeginFrame()
{
    f32_t zero[4] = { 0.f, 0.f, 0.f, 0.f };
    m_pImpl->m_pCtx->ClearRenderTargetView(m_pImpl->m_MVRTV.Get(), zero);
}

// FillFromCamera — Camera-only fullscreen MV
// (구현: cb Update, RTV bind, PS bind, DrawFullscreenTri)

ID3D11RenderTargetView*   CMotionVectorPass::GetRTV() const { return m_pImpl->m_MVRTV.Get(); }
ID3D11ShaderResourceView* CMotionVectorPass::GetSRV() const { return m_pImpl->m_MVSRV.Get(); }
```

---

## 11. 프레임 통합 (GBuffer Pass 내부)

```cpp
// Engine/Private/Framework/CEngineApp.cpp  Render()
m_pMV->BeginFrame();                                // MV RT clear to 0

// (1) Camera-only fullscreen MV — 정적 배경 fill
m_pMV->FillFromCamera(m_pDepth_SRV.Get(),
                      m_matInvVPCurNoJitter,
                      m_matVPPrevNoJitter);

// (2) GBuffer Pass — 동적 메시가 MRT 로 MV 덮어쓰기
ID3D11RenderTargetView* rtvs[] = {
    m_pGB0_RTV.Get(), m_pGB1_RTV.Get(), m_pMV->GetRTV()
};
m_pCtx->OMSetRenderTargets(3, rtvs, m_pDSV.Get());

// Bind global cbuffers
m_cbFrame.Bind(m_pCtx, 0);
m_cbMV.Bind(m_pCtx, 7);   // 신규 b7

// Draw opaque
m_pScene->DrawOpaque();   // 각 메시가 b1/b8 세팅 (CBPerObject / CBPerObjectPrev)

// (3) 이제 TAA (문서 03) 의 Resolve 가 m_pMV->GetSRV() 를 사용할 수 있다.
```

---

## 12. 파티클 / 빌보드

Winters 는 현재 파티클 시스템 미완. Phase G (Effect Tool) 도입 시 아래 규약 준수:

- GPU 시뮬 파티클: prev position 버퍼 유지 후 VS 에서 차이 계산.
- Billboard: particle.velocity * dt 로 prev position 근사.
- CPU 파티클: VS 매개변수에 `vPrevPosition` 추가.

```hlsl
// 예: Particle VS (CPU-simulated) — 원전 §8.1
float3 curPos  = v.position;
float3 prevPos = v.position - v.velocity * DeltaTime;

float4 wCur  = mul(float4(curPos,  1.f), g_matWorld);
float4 wPrev = mul(float4(prevPos, 1.f), g_matPrevWorld);
// ... MV 동일 계산
```

---

## 13. 스폰 / 씬 전환 처리

```cpp
// ModelRenderer::Init 또는 ECS 생성 시
transformComp.bSpawnedThisFrame = true;   // prev = cur 강제
boneMgr->RegisterInstance(uBoneCount);    // prev bone 초기화 (first update 도 prev=cur)

// Scene::Change_Scene 이후
// TransformSystem 이 전체 m_bSpawnedThisFrame = true 로 초기화 + MV RT clear
```

---

## 14. 디버그 시각화

```hlsl
// Shaders/MotionVector/Debug.hlsl (PS, fullscreen)
float3 HSVtoRGB(float3 hsv) { /* ... */ }

float4 PS(VS_OUT i) : SV_Target
{
    float2 mv = g_MV.Sample(g_Point, i.uv).rg;
    float  mag = saturate(length(mv) * 50.0f);
    float  ang = atan2(mv.y, mv.x);
    float  h   = (ang + PI) / (2.0f * PI);
    return float4(HSVtoRGB(float3(h, 1.0f, mag)), 1.0f);
}
```

ImGui 버튼 `Show MV Debug` 로 화면에 오버레이.

---

## 15. 디버깅 체크리스트

| 증상 | Winters 원인 | 해결 |
|------|-------------|------|
| TAA 전체 떨림 | MV 가 jitter 포함됨 | Mesh3D_PBR VS 의 posCur/posPrev 가 NoJitter projection 쓰는지 재확인 |
| 캐릭터 고스트 | prev bone 미업데이트 | `CPrevBoneManager::UpdatePrev` 가 Animator::Update 뒤에 호출되는지 확인 |
| Y 축 반대 | UV flip 부호 | `ndc * float2(0.5, -0.5) + 0.5` 재확인 |
| 스폰 첫 프레임 점프 | `bSpawnedThisFrame` 관리 누락 | TransformSystem 이 프레임 끝에 리셋 |
| 원거리 배경 안 맞음 | Camera-only MV 패스 실행 안 됨 | `m_pMV->FillFromCamera` 호출 순서 확인 |
| 스카이박스 MV 거대 | camera translation 반영 | 스카이는 rotation-only matView 사용 (원전 §8.2) |
| `b9` 레지스터 충돌 | 다른 셰이더도 b9 사용 | Winters 전역 레지스터 표 참조 (본 문서 §17) |
| Debug 빌드 MV RT 읽기 에러 | UAV/SRV 동시 바인딩 | RTV 바인드 해제 후 SRV 바인드 |

---

## 16. 단위 테스트

원전 §11 규약 그대로 Winters ImGui 테스트 하네스에서:

1. **정지 테스트**: `bForceCameraStatic = true`, 모든 오브젝트 velocity=0 → MV = 0 (화면 전체 검정).
2. **카메라 1px 오른쪽**: 수동 translation → 모든 정적 픽셀 MV ≈ `(1/W, 0)`.
3. **이렐리아 Q 전진**: body 영역만 non-zero MV. 배경은 카메라 MV.

---

## 17. Winters 엔진 레지스터 전역 표 (권장)

본 시리즈 (01~04) 와 통합 결과:

| slot | 용도 | 정의 문서 |
|------|------|----------|
| b0 | CBPerFrame (matViewProj, camera) | 01 BRDF |
| b1 | CBPerObject (matWorld) | 기존 |
| b2 | CBBones (스키닝 현재) | 기존 |
| b3 | CBMaterial | 01 BRDF |
| b4 | CBDirLight | 01 BRDF |
| b5 | CBClusterUniforms | 02 Clustered |
| b6 | CBIBL | 04 IBL |
| **b7** | **CBPerFrameMV** | **본 문서** |
| **b8** | **CBPerObjectPrev** | **본 문서** |
| **b9** | **CBPrevBones** | **본 문서** |
| t0~t3 | Material 텍스처 (Albedo/Normal/MRA/Emissive) | 01 BRDF |
| t5 | IBL Prefilter | 04 IBL |
| t6 | IBL BRDF LUT | 04 IBL |
| t8~t10 | Clustered (ClusterInfo / IndexList / Lights) | 02 Clustered |
| s0 | PointClamp | — |
| s1 | LinearClamp | 03 TAA |
| s2 | LinearClamp (IBL 전용 mip 샘플러) | 04 IBL |

앞으로 GI/Bloom 등이 들어올 때 이 표를 업데이트.

---

## 18. Stage 7 전제 완료 기준 (MV)

- [ ] `TransformComponent` 에 `matPrevWorld` + `bSpawnedThisFrame` 필드 추가
- [ ] `CTransformSystem::EndFrame_StashPreviousTransforms` 동작 (ECS tick 순서 문서화)
- [ ] `CMotionVectorPass` + `CPrevBoneManager` 빌드 통과 + EngineSDK 동기화
- [ ] GBuffer MRT[2] 에 `R16G16_FLOAT` MV RT 연결
- [ ] `Mesh3D_PBR.hlsl` / `Skinned3D_PBR.hlsl` MRT 출력 확장
- [ ] 단위 테스트 3종 (정지/카메라/캐릭터) 통과
- [ ] Debug 시각화 모드 ImGui 토글
- [ ] Camera-only fullscreen MV 패스로 정적 배경 처리

이후 문서 03 TAA 가 본 MV 를 소비한다.

---

## 19. 참고

- Andersson 2009 — Parallel Graphics in Frostbite
- Karis 2014 — TAA / MV requirements
- NVIDIA DLSS Programming Guide
- AMD FSR 2 Developer Guide

---

*문서 끝. MV 가 맞으면 TAA 부터 DLSS 까지 도미노처럼 연결된다.*
