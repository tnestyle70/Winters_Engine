# Stage 5 — DX11 Instancing Renderer + 빌보드 + 소팅 + 블렌딩

## 목표

시뮬된 파티클을 **카메라 정렬 빌보드 쿼드** 로 화면에 뿌림.
N 개 파티클 = 1 draw call (인스턴싱). 블렌딩 / 소팅 / 텍스처 아틀라스 지원.

## 왜 이게 MVP 마무리인가

Stage 1~4 가 다 되어도 화면에 **아무것도 안 뜨면** 개발자 동기가 꺾임.
Stage 5 가 끝나면 "불꽃이 폭 터지는 장면" 이 실제로 보이고, 이게 있으면 나머지 Stage (6 Editor, 7 GPU)
는 확실한 목표로 쌓을 수 있다.

## 파이프라인 개요

```
ParticlePool (SoA)
   ↓  ── 매 프레임 ──
[Collect Step]             FxRenderSystem::Collect(camera)
   ↓
[Cull + Sort]              Frustum cull + depth sort (radix)
   ↓
[Pack SoA → AoS]           Dynamic VB 에 ParticleInstance 배열 쓰기
   ↓
[Bind + Draw]              쿼드 VB + Instance VB 2-slot
   ↓
DrawIndexedInstanced(6, N)
```

## 쿼드 지오메트리 (정적)

모든 파티클이 공유하는 단일 쿼드:

```cpp
// Engine/Private/FX/Render/FxBillboardRenderer.cpp 의 일부
struct QuadVertex {
    f32_t px, py;    // 로컬 [-0.5, 0.5]
    f32_t u,  v;
};

static constexpr QuadVertex kQuad[4] = {
    { -0.5f, -0.5f, 0.f, 1.f },
    { -0.5f,  0.5f, 0.f, 0.f },
    {  0.5f,  0.5f, 1.f, 0.f },
    {  0.5f, -0.5f, 1.f, 1.f },
};
static constexpr std::uint16_t kQuadIdx[6] = { 0, 1, 2, 0, 2, 3 };
```

## 인스턴스 데이터

```cpp
// Engine/Public/FX/Render/FxBillboardRenderer.h
#pragma once
#include "WintersMath.h"

namespace Engine::FX {

struct ParticleInstance
{
    Vec3  worldPos;
    f32_t size;
    Vec4  color;
    f32_t rotation;   // 빌보드 회전 (radians)
    f32_t uvRect[3];  // padding 활용: u0, v0, uvSize (아틀라스)
};
static_assert(sizeof(ParticleInstance) == 48, "ParticleInstance layout");

} // namespace Engine::FX
```

**Phase 1 MVP**: rotation / uvRect 간략. `rotation = 0`, `uvRect = {0, 0, 1}` 고정.

## Input Layout

```cpp
// 쿼드 VB (slot 0) + Instance VB (slot 1)
D3D11_INPUT_ELEMENT_DESC elems[] = {
    // Quad
    { "POSITION",  0, DXGI_FORMAT_R32G32_FLOAT,
      0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,
      0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },

    // Instance
    { "POSITION",  1, DXGI_FORMAT_R32G32B32_FLOAT,
      1, 0,  D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    { "TEXCOORD",  1, DXGI_FORMAT_R32_FLOAT,
      1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1 },   // size
    { "COLOR",     0, DXGI_FORMAT_R32G32B32A32_FLOAT,
      1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    { "TEXCOORD",  2, DXGI_FORMAT_R32_FLOAT,
      1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },   // rotation
    { "TEXCOORD",  3, DXGI_FORMAT_R32G32B32_FLOAT,
      1, 36, D3D11_INPUT_PER_INSTANCE_DATA, 1 },   // uvRect
};
```

## HLSL

```hlsl
// Shaders/FxBillboard.hlsl

cbuffer CBPerFrame : register(b0)
{
    row_major matrix g_viewProj;
    float3  g_camRight;
    float   _pad0;
    float3  g_camUp;
    float   _pad1;
    float3  g_camPos;
    float   g_time;
};

Texture2D    g_Atlas  : register(t0);
SamplerState g_Sampler : register(s0);

struct VSIn
{
    // Quad (slot 0)
    float2 vQuadPos  : POSITION0;
    float2 vQuadUV   : TEXCOORD0;
    // Instance (slot 1)
    float3 iWorldPos : POSITION1;
    float  iSize     : TEXCOORD1;
    float4 iColor    : COLOR0;
    float  iRotation : TEXCOORD2;
    float3 iUvRect   : TEXCOORD3;   // u0, v0, uvSize
};

struct VSOut
{
    float4 pos   : SV_Position;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

VSOut VSMain(VSIn v)
{
    VSOut o;

    // 로컬 쿼드 회전 (quad 평면 내)
    float cR = cos(v.iRotation);
    float sR = sin(v.iRotation);
    float2 local = float2(
        v.vQuadPos.x * cR - v.vQuadPos.y * sR,
        v.vQuadPos.x * sR + v.vQuadPos.y * cR);

    // 카메라 정렬 빌보드 (spherical)
    float3 world = v.iWorldPos
                 + g_camRight * local.x * v.iSize
                 + g_camUp    * local.y * v.iSize;

    o.pos = mul(float4(world, 1.0), g_viewProj);

    // 아틀라스 UV
    o.uv = v.vQuadUV * v.iUvRect.z + v.iUvRect.xy;
    o.color = v.iColor;
    return o;
}

float4 PSMain(VSOut i) : SV_Target
{
    float4 tex = g_Atlas.Sample(g_Sampler, i.uv);
    return tex * i.color;
}
```

## 블렌드 모드

Phase 1 은 3 종:

```cpp
enum class eFxBlendMode : std::uint8_t
{
    Additive,          // Src=One,      Dst=One              → 불꽃/광선
    AlphaBlend,        // Src=SrcAlpha, Dst=InvSrcAlpha       → 일반 투명
    Premultiplied      // Src=One,      Dst=InvSrcAlpha       → 아틀라스 프리멀티플
};
```

모드별 `ID3D11BlendState` 미리 3 개 만들어 캐싱:

```cpp
struct FxBlendStates
{
    ComPtr<ID3D11BlendState> additive;
    ComPtr<ID3D11BlendState> alphaBlend;
    ComPtr<ID3D11BlendState> premultiplied;
};
```

## FxBillboardRenderer 전체

```cpp
// Engine/Public/FX/Render/FxBillboardRenderer.h
#pragma once
#include "WintersMath.h"
#include "ParticlePool.h"
#include <wrl/client.h>
#include <d3d11.h>

namespace Engine::FX {

class CFxBillboardRenderer
{
public:
    static std::unique_ptr<CFxBillboardRenderer> Create(ID3D11Device* device);
    ~CFxBillboardRenderer() = default;

    // 카메라 정렬 + 인스턴스 뿌리기
    // sortedIndices: 카메라 뒤→앞 순으로 이미 정렬된 파티클 인덱스
    void Render(ID3D11DeviceContext* ctx,
                const CParticlePool&  pool,
                const std::vector<std::uint32_t>& sortedIndices,
                const Mat4& viewProj,
                const Vec3& camRight,
                const Vec3& camUp,
                const Vec3& camPos,
                ID3D11ShaderResourceView* atlasSRV,
                eFxBlendMode blendMode);

    // Instance VB 확장 (동적 재할당)
    void EnsureInstanceCapacity(ID3D11Device* device, std::uint32_t count);

private:
    CFxBillboardRenderer() = default;

    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_quadVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_quadIB;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_instanceVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_cbFrame;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_layout;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vs;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_ps;

    FxBlendStates                              m_blendStates;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthNoWrite;   // 투명 전용
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_rasterNoCull;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_sampler;

    std::uint32_t m_instanceCapacity = 0;
};

} // namespace Engine::FX
```

## 구현 핵심

```cpp
// Engine/Private/FX/Render/FxBillboardRenderer.cpp
#include "FxBillboardRenderer.h"
#include "FxAttributeRegistry.h"
// ... 셰이더 컴파일 포함

namespace Engine::FX {

using Microsoft::WRL::ComPtr;

std::unique_ptr<CFxBillboardRenderer>
CFxBillboardRenderer::Create(ID3D11Device* device)
{
    auto r = std::unique_ptr<CFxBillboardRenderer>(new CFxBillboardRenderer());

    // 1) Quad VB / IB
    D3D11_BUFFER_DESC bd{};
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = sizeof(kQuad);
    D3D11_SUBRESOURCE_DATA srd{ kQuad, 0, 0 };
    device->CreateBuffer(&bd, &srd, &r->m_quadVB);

    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = sizeof(kQuadIdx);
    srd.pSysMem  = kQuadIdx;
    device->CreateBuffer(&bd, &srd, &r->m_quadIB);

    // 2) Instance VB (dynamic)
    r->EnsureInstanceCapacity(device, 4096);

    // 3) CBPerFrame
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.ByteWidth      = 16 * 5;   // VP + camR + camU + camP + time (pad 포함)
    device->CreateBuffer(&cbd, nullptr, &r->m_cbFrame);

    // 4) Shader / Layout (코드 생략 — CompileFromFile / CreateInputLayout)

    // 5) Blend states
    // Additive
    {
        D3D11_BLEND_DESC bd2{};
        bd2.RenderTarget[0].BlendEnable    = TRUE;
        bd2.RenderTarget[0].SrcBlend       = D3D11_BLEND_ONE;
        bd2.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
        bd2.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        bd2.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        bd2.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        bd2.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        bd2.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&bd2, &r->m_blendStates.additive);
    }
    // AlphaBlend
    {
        D3D11_BLEND_DESC bd2{};
        bd2.RenderTarget[0].BlendEnable    = TRUE;
        bd2.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
        bd2.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
        bd2.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        bd2.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        bd2.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd2.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        bd2.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&bd2, &r->m_blendStates.alphaBlend);
    }
    // Premultiplied
    {
        D3D11_BLEND_DESC bd2{};
        bd2.RenderTarget[0].BlendEnable    = TRUE;
        bd2.RenderTarget[0].SrcBlend       = D3D11_BLEND_ONE;
        bd2.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
        bd2.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
        bd2.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
        bd2.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bd2.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        bd2.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&bd2, &r->m_blendStates.premultiplied);
    }

    // 6) DepthStencil (Read-only, no write)
    {
        D3D11_DEPTH_STENCIL_DESC dd{};
        dd.DepthEnable    = TRUE;
        dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
        device->CreateDepthStencilState(&dd, &r->m_depthNoWrite);
    }

    // 7) Rasterizer (빌보드는 양면)
    {
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        device->CreateRasterizerState(&rd, &r->m_rasterNoCull);
    }

    // 8) Sampler
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter     = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU   = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD     = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sd, &r->m_sampler);
    }

    return r;
}

void CFxBillboardRenderer::EnsureInstanceCapacity(ID3D11Device* device, std::uint32_t count)
{
    if (count <= m_instanceCapacity) return;
    std::uint32_t newCap = std::max(count, m_instanceCapacity * 2);

    D3D11_BUFFER_DESC bd{};
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.ByteWidth      = newCap * sizeof(ParticleInstance);
    ComPtr<ID3D11Buffer> newVB;
    device->CreateBuffer(&bd, nullptr, &newVB);
    m_instanceVB = newVB;
    m_instanceCapacity = newCap;
}

void CFxBillboardRenderer::Render(
    ID3D11DeviceContext* ctx,
    const CParticlePool&  pool,
    const std::vector<std::uint32_t>& sortedIndices,
    const Mat4& viewProj,
    const Vec3& camRight,
    const Vec3& camUp,
    const Vec3& camPos,
    ID3D11ShaderResourceView* atlasSRV,
    eFxBlendMode blendMode)
{
    const std::uint32_t N = static_cast<std::uint32_t>(sortedIndices.size());
    if (N == 0) return;

    // 1) CBPerFrame 업데이트
    D3D11_MAPPED_SUBRESOURCE m{};
    ctx->Map(m_cbFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    struct CB { Mat4 vp; Vec3 cR; f32_t _0; Vec3 cU; f32_t _1; Vec3 cP; f32_t time; };
    CB* cb = reinterpret_cast<CB*>(m.pData);
    cb->vp = viewProj; cb->cR = camRight; cb->cU = camUp; cb->cP = camPos; cb->time = 0.f;
    ctx->Unmap(m_cbFrame.Get(), 0);

    // 2) Instance VB 패킹 (SoA → AoS)
    const auto* pPos  = pool.Data<Vec3>(Attr::Position);
    const auto* pCol  = pool.Data<Vec4>(Attr::Color);
    const auto* pSize = pool.Data<f32_t>(Attr::Size);

    ctx->Map(m_instanceVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
    auto* dst = reinterpret_cast<ParticleInstance*>(m.pData);
    for (std::uint32_t k = 0; k < N; ++k) {
        const std::uint32_t i = sortedIndices[k];
        dst[k].worldPos   = pPos[i];
        dst[k].size       = pSize[i];
        dst[k].color      = pCol[i];
        dst[k].rotation   = 0.f;
        dst[k].uvRect[0]  = 0.f;
        dst[k].uvRect[1]  = 0.f;
        dst[k].uvRect[2]  = 1.f;
    }
    ctx->Unmap(m_instanceVB.Get(), 0);

    // 3) IA / VS / PS bind
    ID3D11Buffer* buffers[2] = { m_quadVB.Get(), m_instanceVB.Get() };
    UINT strides[2] = { sizeof(QuadVertex), sizeof(ParticleInstance) };
    UINT offsets[2] = { 0, 0 };
    ctx->IASetVertexBuffers(0, 2, buffers, strides, offsets);
    ctx->IASetIndexBuffer(m_quadIB.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(m_layout.Get());

    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, m_cbFrame.GetAddressOf());
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &atlasSRV);
    ctx->PSSetSamplers(0, 1, m_sampler.GetAddressOf());

    // 4) OM state
    ID3D11BlendState* bs = nullptr;
    switch (blendMode) {
        case eFxBlendMode::Additive:      bs = m_blendStates.additive.Get(); break;
        case eFxBlendMode::AlphaBlend:    bs = m_blendStates.alphaBlend.Get(); break;
        case eFxBlendMode::Premultiplied: bs = m_blendStates.premultiplied.Get(); break;
    }
    const float bf[4] = { 0.f, 0.f, 0.f, 0.f };
    ctx->OMSetBlendState(bs, bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_depthNoWrite.Get(), 0);
    ctx->RSSetState(m_rasterNoCull.Get());

    // 5) Draw
    ctx->DrawIndexedInstanced(6, N, 0, 0, 0);
}

} // namespace Engine::FX
```

## 소팅 (Depth Sort)

투명 파티클은 **뒤→앞** 순으로 그려야. 10,000 파티클에 `std::sort` 는 ~2 ms (Os log N) — 아슬아슬.
Radix sort on float depth 가 훨씬 빠름:

```cpp
// Engine/Public/FX/Render/FxSortBackend.h
#pragma once
#include "ParticlePool.h"
#include "WintersMath.h"

namespace Engine::FX {

// camera forward 투영한 depth 기준 뒤→앞 소팅. 결과는 인덱스 배열.
void DepthSortBackToFront(const CParticlePool& pool,
                           const Vec3& camPos,
                           const Vec3& camForward,
                           std::vector<std::uint32_t>& outIndices);

// 간이 컬링: camForward 와 dot < 0 (카메라 뒤) 제외
void FrustumCullSimple(const CParticlePool& pool,
                        const Vec3& camPos,
                        const Vec3& camForward,
                        f32_t minDepth, f32_t maxDepth,
                        std::vector<std::uint32_t>& outAlive);

} // namespace Engine::FX
```

```cpp
// Engine/Private/FX/Render/FxSortBackend.cpp
#include "FxSortBackend.h"
#include "FxAttributeRegistry.h"
#include <algorithm>

namespace Engine::FX {

// 간단: std::sort (Phase 1 MVP). 후일 radix 로 교체.
void DepthSortBackToFront(const CParticlePool& pool,
                           const Vec3& camPos,
                           const Vec3& camForward,
                           std::vector<std::uint32_t>& outIndices)
{
    const std::uint32_t N = pool.AliveCount();
    const auto* pos = pool.Data<Vec3>(Attr::Position);

    outIndices.clear();
    outIndices.reserve(N);

    std::vector<std::pair<f32_t, std::uint32_t>> keys;
    keys.reserve(N);

    for (std::uint32_t i = 0; i < N; ++i) {
        const Vec3 rel = { pos[i].x - camPos.x, pos[i].y - camPos.y, pos[i].z - camPos.z };
        const f32_t d  = rel.x * camForward.x + rel.y * camForward.y + rel.z * camForward.z;
        if (d < 0.f) continue;   // 카메라 뒤는 컬
        keys.push_back({ d, i });
    }

    // 뒤→앞 = depth 큰 것부터
    std::sort(keys.begin(), keys.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    for (const auto& k : keys) outIndices.push_back(k.second);
}

} // namespace Engine::FX
```

**radix sort 업그레이드 경로**: float → uint 매핑 (IEEE 754 trick) 후 4-pass 256-bucket radix.
1M 요소 기준 std::sort 대비 3~5 배 빠름. Phase 1 MVP 이후.

## FxRenderSystem

```cpp
// Engine/Public/FX/Systems/FxRenderSystem.h
#pragma once
#include "ISystem.h"
#include "FxBillboardRenderer.h"
#include "FxSortBackend.h"

namespace Engine::FX {

class CFxRenderSystem : public Engine::ISystem
{
public:
    void Execute(Engine::CWorld& world, f32_t dt) override;

    void SetRenderer(CFxBillboardRenderer* r) { m_pRenderer = r; }
    void SetDeviceContext(ID3D11DeviceContext* ctx) { m_pCtx = ctx; }
    void SetAtlas(ID3D11ShaderResourceView* srv) { m_pAtlas = srv; }

    // 카메라 정보는 Scene 이 설정
    void SetCamera(const Mat4& viewProj, const Vec3& right, const Vec3& up,
                   const Vec3& pos, const Vec3& forward);

private:
    CFxBillboardRenderer* m_pRenderer = nullptr;
    ID3D11DeviceContext*  m_pCtx      = nullptr;
    ID3D11ShaderResourceView* m_pAtlas = nullptr;

    Mat4  m_viewProj = Mat4::Identity();
    Vec3  m_camRight, m_camUp, m_camPos, m_camForward;

    std::vector<std::uint32_t> m_sortedIndices;   // 재사용 버퍼
};

} // namespace Engine::FX
```

```cpp
// Engine/Private/FX/Systems/FxRenderSystem.cpp
void CFxRenderSystem::Execute(Engine::CWorld& world, f32_t dt)
{
    if (!m_pRenderer || !m_pCtx) return;

    // FxSystem 가 전체 FxInstance 리스트를 쥐고 있음
    auto* fx = CGameInstance::Get()->Get_FxSystem();
    if (!fx) return;

    fx->ForEachInstance([&](CFxInstance& inst) {
        for (auto& em : inst.Emitters()) {
            const CParticlePool& pool = em->Pool();
            if (pool.AliveCount() == 0) return;

            m_sortedIndices.clear();
            DepthSortBackToFront(pool, m_camPos, m_camForward, m_sortedIndices);

            // 이미터의 블렌드 모드는 에셋 메타에 저장 — Phase 1 은 Additive 고정
            m_pRenderer->Render(m_pCtx, pool, m_sortedIndices,
                                m_viewProj, m_camRight, m_camUp, m_camPos,
                                m_pAtlas, eFxBlendMode::Additive);
        }
    });
}
```

## 카메라 Right / Up 추출

```cpp
// DynamicCamera 에서
XMVECTOR camPos  = /* position */;
XMVECTOR at      = /* lookAt */;
XMVECTOR up      = /* world up */;

XMMATRIX view = XMMatrixLookAtLH(camPos, at, up);
// view 행렬의 첫 3 행 = right, up, forward (row vector)
XMFLOAT4X4 m; XMStoreFloat4x4(&m, view);
Vec3 right = { m._11, m._21, m._31 };   // 열 벡터 추출 (view 는 전치)
Vec3 camUp = { m._12, m._22, m._32 };
Vec3 fwd   = { m._13, m._23, m._33 };
```

**Gotcha**: row-major / column-major 혼동 주의. Winters 는 row-major 관례 → view 의 3번째 열이 forward.

## 텍스처 아틀라스

불꽃 / 연기 / 빛망울 등을 하나의 큰 텍스처 (`T_FxAtlas_01.dds`) 에 배치해 draw call 병합.
`uvRect = {u0, v0, uvSize}` 로 서브 영역 지정.

Phase 1 MVP 는 단일 이미지 (uvRect = {0,0,1}) 로 시작.

## 성능 목표

| 파티클 수 | Expected CPU | Expected GPU |
|---|---|---|
| 1,000 | < 0.1 ms (sort + pack) | 빠름 |
| 10,000 | < 0.5 ms | < 0.3 ms fill rate 풍부 시 |
| 100,000 | 2~3 ms (sort 병목) | blending overdraw 로 GPU 한계 |

10,000 이상은 Stage 7 GPU Compute 권장.

## Gotchas

- **투명 + ZWrite**: ZWrite 켜면 뒤 파티클이 앞을 가림. 반드시 `DepthWriteMask = ZERO`
- **소팅 누락 시 Additive OK**: Additive 는 순서 무관. AlphaBlend 만 소팅 필수. 만약 Emitter 가 Additive 면 소팅 스킵 가능 → 상당한 CPU 절감
- **Dynamic VB 재할당**: `EnsureInstanceCapacity` 가 매 프레임 호출되면 런타임 메모리 증가 누출 가능성 (초기 용량을 충분히 크게)
- **카메라 Right/Up 좌표계**: XMMatrixLookAtLH vs RH 에 따라 부호 반전. Winters 는 LH 로 통일되어 있는지 확인 (DynamicCamera 확인 필요)
- **인스턴스 0 개 호출**: `DrawIndexedInstanced(6, 0, ...)` 은 warning 없이 지나가지만 바인딩 state 만 낭비. 호출 전 `N > 0` 가드
- **vertex buffer 2 슬롯 stride 배열**: `strides[2]` 와 `buffers[2]` 요소 순서 맞춰야. 실수하면 데이터가 잘못 읽힘
- **ParticleInstance 구조체 정렬**: 48 바이트 = 3 + 4 + 5 (12/4/16/4/12 = 48) 자연 정렬. pad 없음. 변경 시 `static_assert`
- **PostBuild 로 `Shaders/FxBillboard.hlsl` 복사**: Client/Bin/Shaders 에 배포 여부 확인. 기존 `Mesh3D.hlsl` 와 동일 파이프로 복사
- **블렌드 스테이트 바꾼 뒤 복원**: FX 렌더 후 Opaque 렌더가 이어지면 bs reset 필요 — FxRenderSystem 이 보통 씬 렌더 끝에 실행되므로 문제 없지만 RenderGraph (Phase 2) 진입 시 주의

## 단위 테스트 / 스모크 테스트

- 1 이미터 + SpawnBurst(30) + 불꽃 파라미터 → 30 파티클 화면 확인
- 같은 이미터 10 개 동시 → draw call 10 개 (GPU 타이머 확인)
- AlphaBlend 모드에서 겹치는 파티클 순서 시각 확인 — 앞 파티클이 뒤를 정상적으로 가림
- 파티클 0 개일 때 no-draw, Alive 가 Capacity 초과 시 clamp 동작

## 구현 순서

1. HLSL 셰이더 파일 2 개 (`Shaders/FxBillboard.hlsl` VS / PS)
2. `Shaders/FxBillboard.hlsl` PostBuild 복사 설정 (기존 `Mesh3D.hlsl` 항목 참고)
3. `ParticleInstance` struct + layout 배열
4. `FxBillboardRenderer::Create` — 정적 버퍼 / 셰이더 / 블렌드 / DSV / RS / 샘플러
5. `FxBillboardRenderer::Render` — Map / Unmap + Draw
6. `FxSortBackend::DepthSortBackToFront` (std::sort MVP)
7. `FxRenderSystem::Execute` — 이미터 순회 + sort + render
8. `Scene_InGame::OnRender` 에 시스템 호출 추가
9. Stage 3 데모 실행 — **첫 불꽃이 스크린에 뜬다!**
10. ImGui 에 "Draw calls" / "Alive total" 표시
11. 기본 아틀라스 텍스처 (`T_FxAtlas_01.dds`) 1 장 준비 → Resource 에 배치

## 다음 Stage

Stage 6 — ImGui 기반 노드 에디터. 지금까진 JSON 직접 편집 (또는 코드 생성). 이제 **아티스트** 가 사용할 수 있는 UI.
