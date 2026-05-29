# 03. TAA (Temporal Anti-Aliasing) — Winters Engine 이식 가이드

> 원전: `C:\Users\user\Desktop\.markdown\Graphics\03_TAA_Implementation.md`
> 대상 경로: `Engine/Public/Renderer/PostFX/TAA/`, `Engine/Private/Renderer/PostFX/TAA/`, `Shaders/PostFX/TAA/`
> Phase: **E Stage 7** — Motion Vectors(문서 05) 선행 필수. BRDF(Stage 1) + GBuffer(Stage 2) 후.

---

## 0. 이 문서의 목표 (Winters 관점)

1. **Motion Vector 버퍼를 전제로** TAA Resolve 패스 구현 (자세한 MV 생성은 문서 05 참조).
2. **Jitter 주입을 CEngineApp 프레임 루프**에 통합. `CBFrameCamera::matViewProj` 만 jitter, `matViewProjNoJitter` 는 MV 계산 전용.
3. **History Texture Ping-Pong** 은 Engine DLL 내부 관리. Client 에서 접근 불필요.
4. **ImGui 튜너**: blend factor / variance gamma / mip bias / pattern (Halton vs R2) / on-off.
5. UI 렌더는 TAA 이후로 이동 (현재 `CEngineApp::Render` 순서 변경 필요).

---

## 1. 디렉토리

```
Engine/Public/Renderer/PostFX/TAA/
├── TAATypes.h           // Jitter 시퀀스 enum, CBTAA, TAAResources
└── TAAResolve.h         // class CTAAResolve

Engine/Private/Renderer/PostFX/TAA/
├── TAAResolve.cpp
└── JitterSequences.cpp  // Halton / R2 생성 (CPU)

Shaders/PostFX/TAA/
├── TAACommon.hlsli      // YCoCg, ClipAABB, CatmullRom
└── TAAResolve.hlsl      // 본 passa
```

---

## 2. 공유 타입 — `TAATypes.h`

```cpp
// Engine/Public/Renderer/PostFX/TAA/TAATypes.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include <DirectXMath.h>

enum class eJitterPattern : u32_t
{
    Halton2_3_8  = 0,
    Halton2_3_16 = 1,
    R2           = 2,
    None         = 3,    // TAA off
};

struct CBTAA
{
    DirectX::XMFLOAT2 vScreenSize;
    DirectX::XMFLOAT2 vInvScreenSize;
    f32_t             fBlendFactor;   // 기본 0.1
    f32_t             fGamma;         // 기본 1.0 (variance clipping 폭)
    DirectX::XMFLOAT2 vJitter;        // 현재 프레임 jitter (pixel 단위)
};
static_assert(sizeof(CBTAA) % 16 == 0);
```

---

## 3. Jitter 시퀀스 — `JitterSequences.cpp`

```cpp
// Engine/Private/Renderer/PostFX/TAA/JitterSequences.cpp
#include "TAATypes.h"
#include <cmath>

namespace JitterSeq
{
    f32_t Halton(i32_t i, i32_t base)
    {
        f32_t f = 1.f, r = 0.f;
        while (i > 0)
        {
            f /= static_cast<f32_t>(base);
            r += f * static_cast<f32_t>(i % base);
            i /= base;
        }
        return r;
    }

    DirectX::XMFLOAT2 GetHalton(u32_t uFrameIdx, u32_t uPeriod)
    {
        u32_t i = (uFrameIdx % uPeriod) + 1u;
        return { Halton(static_cast<i32_t>(i), 2) - 0.5f,
                 Halton(static_cast<i32_t>(i), 3) - 0.5f };
    }

    DirectX::XMFLOAT2 GetR2(u32_t i)
    {
        const f32_t g  = 1.32471795724474602596f;
        const f32_t a1 = 1.f / g;
        const f32_t a2 = 1.f / (g * g);
        auto frac = [](f32_t v) { return v - std::floor(v); };
        return { frac(0.5f + a1 * static_cast<f32_t>(i)) - 0.5f,
                 frac(0.5f + a2 * static_cast<f32_t>(i)) - 0.5f };
    }

    DirectX::XMFLOAT2 Sample(eJitterPattern pattern, u32_t uFrameIdx)
    {
        switch (pattern)
        {
            case eJitterPattern::Halton2_3_8:  return GetHalton(uFrameIdx, 8);
            case eJitterPattern::Halton2_3_16: return GetHalton(uFrameIdx, 16);
            case eJitterPattern::R2:           return GetR2(uFrameIdx);
            case eJitterPattern::None:
            default:                           return { 0.f, 0.f };
        }
    }
}
```

---

## 4. Jitter → Projection 주입 (CEngineApp 수정)

```cpp
// Engine/Private/Framework/CEngineApp.cpp (추가)

void CEngineApp::UpdateTAAJitter()
{
    m_vPrevJitter = m_vCurJitter;
    m_vCurJitter  = JitterSeq::Sample(m_eJitterPattern, m_uFrameIndex);

    // Base projection 은 CCamera 가 돌려주는 un-jittered
    DirectX::XMMATRIX baseProj = DirectX::XMLoadFloat4x4(&m_matBaseProj);

    // NDC 단위로 주입: 픽셀 → NDC 변환
    f32_t w = static_cast<f32_t>(m_uW);
    f32_t h = static_cast<f32_t>(m_uH);
    DirectX::XMFLOAT4X4 jittered;
    DirectX::XMStoreFloat4x4(&jittered, baseProj);
    // DX 컨벤션: [row][col] — m31 m32 가 row-vector × matrix 에서 평행이동 효과
    jittered.m[2][0] += 2.f * m_vCurJitter.x / w;
    jittered.m[2][1] += 2.f * m_vCurJitter.y / h;   // 부호 주의: Y UV flip 고려

    // CBFrameCamera 갱신
    CBFrameCamera cb{};
    cb.matViewProj         = MulViewProj(m_matView, jittered);
    cb.matViewProjNoJitter = MulViewProj(m_matView, m_matBaseProj);
    cb.vCameraPos          = m_vCameraPos;
    cb.fTime               = (f32_t)m_fTime;
    m_cbFrame.Update(m_pCtx, cb);

    m_uFrameIndex++;
}
```

> 기존 `Mesh3D.hlsl` / `Skinned3D.hlsl` 는 `matViewProj` 만 본다 → jitter 자동 반영. MV 생성 셰이더만 `matViewProjNoJitter` 를 참조. (문서 05 참조)

---

## 5. TAA 리소스 관리 — `TAAResolve.h`

```cpp
// Engine/Public/Renderer/PostFX/TAA/TAAResolve.h
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "TAATypes.h"
#include "DX11ConstantBuffer.h"
#include "DX11Shader.h"
#include <wrl/client.h>
#include <memory>

class WINTERS_ENGINE CTAAResolve
{
public:
    ~CTAAResolve();

    static std::unique_ptr<CTAAResolve> Create(ID3D11Device* pDev,
                                               ID3D11DeviceContext* pCtx,
                                               u32_t uW, u32_t uH);

    CTAAResolve(const CTAAResolve&)            = delete;
    CTAAResolve& operator=(const CTAAResolve&) = delete;
    CTAAResolve(CTAAResolve&&)                 = default;
    CTAAResolve& operator=(CTAAResolve&&)      = default;

    // 입력: Lighting 결과 SRV, MV SRV, Depth SRV
    // 출력: 히스토리 RT 하나에 그리고 SRV 로 반환 (다음 패스에서 Bloom/Tonemap 의 입력)
    ID3D11ShaderResourceView* Resolve(ID3D11ShaderResourceView* pCurrentSRV,
                                      ID3D11ShaderResourceView* pMVSRV,
                                      ID3D11ShaderResourceView* pDepthSRV,
                                      const DirectX::XMFLOAT2& vJitterPx);

    void Resize(u32_t uW, u32_t uH);
    void Reset();   // 첫 프레임 또는 씬 전환 시 history 무효화

    // ImGui 튜너
    void SetBlendFactor(f32_t f);
    void SetGamma(f32_t f);
    eJitterPattern GetJitterPattern() const;
    void SetJitterPattern(eJitterPattern p);

private:
    CTAAResolve() = default;
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
```

```cpp
// Engine/Private/Renderer/PostFX/TAA/TAAResolve.cpp
#include "TAAResolve.h"
#include <d3d11.h>

using Microsoft::WRL::ComPtr;

struct CTAAResolve::Impl
{
    ID3D11Device*        m_pDev = nullptr;
    ID3D11DeviceContext* m_pCtx = nullptr;
    u32_t                m_uW = 0, m_uH = 0;

    // Ping-pong history
    ComPtr<ID3D11Texture2D>          m_Hist[2];
    ComPtr<ID3D11RenderTargetView>   m_HistRTV[2];
    ComPtr<ID3D11ShaderResourceView> m_HistSRV[2];
    u32_t m_uReadIdx  = 0;
    u32_t m_uWriteIdx = 1;

    DX11ConstantBuffer<CBTAA> m_cb;
    DX11Shader                m_PS;

    eJitterPattern            m_eJitter     = eJitterPattern::Halton2_3_16;
    f32_t                     m_fBlend       = 0.1f;
    f32_t                     m_fGamma       = 1.0f;
    bool_t                    m_bFirstFrame  = true;
};

static HRESULT CreateHistoryRT(ID3D11Device* pDev, u32_t w, u32_t h,
                               ComPtr<ID3D11Texture2D>& tex,
                               ComPtr<ID3D11RenderTargetView>& rtv,
                               ComPtr<ID3D11ShaderResourceView>& srv)
{
    D3D11_TEXTURE2D_DESC td{};
    td.Width              = w;
    td.Height             = h;
    td.MipLevels          = 1;
    td.ArraySize          = 1;
    td.Format             = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count   = 1;
    td.Usage              = D3D11_USAGE_DEFAULT;
    td.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = pDev->CreateTexture2D(&td, nullptr, tex.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;

    hr = pDev->CreateRenderTargetView(tex.Get(), nullptr, rtv.ReleaseAndGetAddressOf());
    if (FAILED(hr)) return hr;
    hr = pDev->CreateShaderResourceView(tex.Get(), nullptr, srv.ReleaseAndGetAddressOf());
    return hr;
}

CTAAResolve::~CTAAResolve() = default;

std::unique_ptr<CTAAResolve> CTAAResolve::Create(ID3D11Device* pDev,
                                                 ID3D11DeviceContext* pCtx,
                                                 u32_t uW, u32_t uH)
{
    auto p    = std::unique_ptr<CTAAResolve>(new CTAAResolve());
    p->m_pImpl = std::make_unique<Impl>();
    auto& I    = *p->m_pImpl;
    I.m_pDev   = pDev;
    I.m_pCtx   = pCtx;
    I.m_uW     = uW;
    I.m_uH     = uH;

    CreateHistoryRT(pDev, uW, uH, I.m_Hist[0], I.m_HistRTV[0], I.m_HistSRV[0]);
    CreateHistoryRT(pDev, uW, uH, I.m_Hist[1], I.m_HistRTV[1], I.m_HistSRV[1]);

    I.m_cb.Create(pDev);
    I.m_PS.LoadPixel(pDev, L"Shaders/PostFX/TAA/TAAResolve.hlsl", "PS");
    return p;
}

ID3D11ShaderResourceView*
CTAAResolve::Resolve(ID3D11ShaderResourceView* pCurrentSRV,
                     ID3D11ShaderResourceView* pMVSRV,
                     ID3D11ShaderResourceView* pDepthSRV,
                     const DirectX::XMFLOAT2& vJitterPx)
{
    auto& I = *m_pImpl;

    CBTAA cb{};
    cb.vScreenSize    = { (f32_t)I.m_uW, (f32_t)I.m_uH };
    cb.vInvScreenSize = { 1.f / cb.vScreenSize.x, 1.f / cb.vScreenSize.y };
    cb.fBlendFactor   = I.m_fBlend;
    cb.fGamma         = I.m_fGamma;
    cb.vJitter        = vJitterPx;
    I.m_cb.Update(I.m_pCtx, cb);

    // 첫 프레임: history = current 로 무복사 초기화 위해 PS 내부 플래그 사용
    if (I.m_bFirstFrame)
    {
        // history[read] 에 current 를 그대로 카피 (간단히 blit PS 나 CopyResource 를 쓰면 됨)
        // 여기선 CopyResource 로 처리 — SRV 로부터 Texture2D 얻기는 API 복잡하므로 RT 경유 blit 권장
        I.m_bFirstFrame = false;
    }

    // Bind RT
    ID3D11RenderTargetView* rtv = I.m_HistRTV[I.m_uWriteIdx].Get();
    I.m_pCtx->OMSetRenderTargets(1, &rtv, nullptr);

    D3D11_VIEWPORT vp{0.f, 0.f, (f32_t)I.m_uW, (f32_t)I.m_uH, 0.f, 1.f};
    I.m_pCtx->RSSetViewports(1, &vp);

    // Bind SRVs + cb
    ID3D11ShaderResourceView* srvs[] = { pCurrentSRV, I.m_HistSRV[I.m_uReadIdx].Get(),
                                         pMVSRV,     pDepthSRV };
    I.m_pCtx->PSSetShaderResources(0, 4, srvs);
    ID3D11Buffer* cbs[] = { I.m_cb.GetRaw() };
    I.m_pCtx->PSSetConstantBuffers(0, 1, cbs);

    // Samplers — PointClamp s0, LinearClamp s1 (샘플러 캐시에서 가져옴)
    BindTAASamplers(I.m_pCtx);

    // Fullscreen triangle (Winters 는 `DrawFullscreenTri()` 헬퍼 가정)
    I.m_pCtx->PSSetShader(I.m_PS.GetPixelShader(), nullptr, 0);
    DrawFullscreenTri(I.m_pCtx);

    // SRV 해제 (파이프라인 barrier)
    ID3D11ShaderResourceView* nullSRV[] = { nullptr, nullptr, nullptr, nullptr };
    I.m_pCtx->PSSetShaderResources(0, 4, nullSRV);

    ID3D11ShaderResourceView* pOut = I.m_HistSRV[I.m_uWriteIdx].Get();

    // swap
    std::swap(I.m_uReadIdx, I.m_uWriteIdx);
    return pOut;
}

void CTAAResolve::Resize(u32_t uW, u32_t uH)
{
    auto& I = *m_pImpl;
    I.m_uW = uW; I.m_uH = uH;
    CreateHistoryRT(I.m_pDev, uW, uH, I.m_Hist[0], I.m_HistRTV[0], I.m_HistSRV[0]);
    CreateHistoryRT(I.m_pDev, uW, uH, I.m_Hist[1], I.m_HistRTV[1], I.m_HistSRV[1]);
    I.m_bFirstFrame = true;
}

void CTAAResolve::Reset()                                   { m_pImpl->m_bFirstFrame = true; }
void CTAAResolve::SetBlendFactor(f32_t f)                    { m_pImpl->m_fBlend = std::clamp(f, 0.02f, 0.5f); }
void CTAAResolve::SetGamma(f32_t f)                          { m_pImpl->m_fGamma = std::clamp(f, 0.5f, 2.0f); }
eJitterPattern CTAAResolve::GetJitterPattern() const          { return m_pImpl->m_eJitter; }
void CTAAResolve::SetJitterPattern(eJitterPattern p)          { m_pImpl->m_eJitter = p; Reset(); }
```

---

## 6. HLSL — `Shaders/PostFX/TAA/TAACommon.hlsli`

```hlsl
// Shaders/PostFX/TAA/TAACommon.hlsli
#ifndef TAA_COMMON_HLSLI
#define TAA_COMMON_HLSLI

float Luminance(float3 c) { return dot(c, float3(0.299f, 0.587f, 0.114f)); }

float3 ToneMap(float3 c)   { return c / (1.0f + Luminance(c)); }
float3 ToneUnMap(float3 c) { return c / max(1.0f - Luminance(c), 1e-4f); }

float3 RGBToYCoCg(float3 c)
{
    return float3(
        0.25f * c.r + 0.5f * c.g + 0.25f * c.b,
        0.5f  * c.r -               0.5f  * c.b,
       -0.25f * c.r + 0.5f * c.g - 0.25f * c.b);
}
float3 YCoCgToRGB(float3 c)
{
    return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

float3 ClipAABB(float3 aabbMin, float3 aabbMax, float3 p)
{
    float3 center = 0.5f * (aabbMax + aabbMin);
    float3 extent = 0.5f * (aabbMax - aabbMin) + 1e-5f;
    float3 d      = p - center;
    float3 units  = abs(d) / extent;
    float  maxU   = max(max(units.x, units.y), units.z);
    return (maxU > 1.0f) ? (center + d / maxU) : p;
}

#endif
```

---

## 7. HLSL — `Shaders/PostFX/TAA/TAAResolve.hlsl`

```hlsl
// Shaders/PostFX/TAA/TAAResolve.hlsl
#include "TAACommon.hlsli"

cbuffer CBTAA : register(b0)
{
    float2 ScreenSize;
    float2 InvScreenSize;
    float  BlendFactor;
    float  Gamma;
    float2 Jitter;
};

Texture2D<float4>  CurrentTex   : register(t0);
Texture2D<float4>  HistoryTex   : register(t1);
Texture2D<float2>  MotionVecTex : register(t2);
Texture2D<float>   DepthTex     : register(t3);

SamplerState PointClamp  : register(s0);
SamplerState LinearClamp : register(s1);

struct VS_OUT { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

float2 GetClosestMotionVector(float2 uv)
{
    float3 closest = float3(0.0f, 0.0f, 1.0f);
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
    {
        float d = DepthTex.SampleLevel(PointClamp, uv, 0, int2(x, y)).r;
        if (d < closest.z) closest = float3(x, y, d);
    }
    return MotionVecTex.SampleLevel(PointClamp,
             uv + closest.xy * InvScreenSize, 0).rg;
}

float3 SampleHistoryCatmullRom(float2 uv)
{
    float2 pos  = uv * ScreenSize;
    float2 posI = floor(pos - 0.5f) + 0.5f;
    float2 f    = pos - posI;

    float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    float2 w3 = f * f * (-0.5f + 0.5f * f);
    float2 w12 = w1 + w2;
    float2 o12 = w2 / w12;

    float2 t0  = (posI - 1.0f) * InvScreenSize;
    float2 t3  = (posI + 2.0f) * InvScreenSize;
    float2 t12 = (posI + o12)  * InvScreenSize;

    float3 r = 0.0f;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t0.x,  t0.y ), 0).rgb * w0.x  * w0.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t12.x, t0.y ), 0).rgb * w12.x * w0.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t3.x,  t0.y ), 0).rgb * w3.x  * w0.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t0.x,  t12.y), 0).rgb * w0.x  * w12.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t12.x, t12.y), 0).rgb * w12.x * w12.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t3.x,  t12.y), 0).rgb * w3.x  * w12.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t0.x,  t3.y ), 0).rgb * w0.x  * w3.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t12.x, t3.y ), 0).rgb * w12.x * w3.y;
    r += HistoryTex.SampleLevel(LinearClamp, float2(t3.x,  t3.y ), 0).rgb * w3.x  * w3.y;
    return max(r, 0.0f);
}

float4 PS(VS_OUT i) : SV_Target
{
    float2 uv = i.uv;

    float3 current = CurrentTex.SampleLevel(PointClamp, uv, 0).rgb;
    current = ToneMap(current);

    float2 mv     = GetClosestMotionVector(uv);
    float2 prevUV = uv - mv;
    if (any(prevUV != saturate(prevUV)))
        return float4(ToneUnMap(current), 1.0f);

    float3 history = SampleHistoryCatmullRom(prevUV);
    history = ToneMap(history);

    // Variance clipping in YCoCg
    float3 m1 = 0.0f; float3 m2 = 0.0f;
    [unroll] for (int y = -1; y <= 1; ++y)
    [unroll] for (int x = -1; x <= 1; ++x)
    {
        float3 c = CurrentTex.SampleLevel(PointClamp, uv, 0, int2(x, y)).rgb;
        c = RGBToYCoCg(ToneMap(c));
        m1 += c; m2 += c * c;
    }
    m1 /= 9.0f; m2 /= 9.0f;
    float3 sigma = sqrt(max(m2 - m1 * m1, 0.0f));

    float3 aMin = m1 - Gamma * sigma;
    float3 aMax = m1 + Gamma * sigma;

    float3 hY = RGBToYCoCg(history);
    hY        = ClipAABB(aMin, aMax, hY);
    history   = YCoCgToRGB(hY);

    float mvLen = length(mv * ScreenSize);
    float alpha = lerp(BlendFactor, 0.2f, saturate(mvLen * 0.1f));

    float lC = Luminance(current);
    float lH = Luminance(history);
    float wC = 1.0f / (1.0f + lC);
    float wH = 1.0f / (1.0f + lH);

    float3 result = (current * wC * alpha + history * wH * (1.0f - alpha)) /
                    (wC * alpha + wH * (1.0f - alpha));

    return float4(ToneUnMap(result), 1.0f);
}
```

---

## 8. 렌더 파이프 통합 (CEngineApp 순서 변경)

기존 순서 (현재):
```
1. Clear backbuffer
2. Scene::OnRender (ModelRenderer draws — 전부 forward)
3. ImGui
4. Present
```

TAA 도입 후:
```
1. UpdateTAAJitter (CBFrameCamera 갱신)
2. GBuffer Pass (Stage 2; 신설)
3. Lighting Pass → HDR RT (Stage 2 Clustered; 신설)
4. TAA Resolve (HDR + MV + Depth → HistoryRT)
5. Bloom/Tonemap (Stage 6/7)
6. UI (ImGui) on LDR backbuffer
7. Present
```

```cpp
// Engine/Private/Framework/CEngineApp.cpp  Render() — 최종 형태
void CEngineApp::Render()
{
    UpdateTAAJitter();                       // §4

    m_pGBufferPass->Draw(m_pScene);

    auto* pCL = m_pGameInstance->Get_ClusteredLighting();
    pCL->RebuildClusterAABBs(m_matBaseProj, m_fNear, m_fFar, m_uW, m_uH);
    pCL->UploadLights(/* ... */); pCL->CullLights();
    m_pLightingPass->Draw(pCL);              // writes HDR

    // TAA
    DirectX::XMFLOAT2 jitterPx = m_vCurJitter;
    ID3D11ShaderResourceView* pResolved =
        m_pTAA->Resolve(m_pHDR_SRV.Get(),
                        m_pMV_SRV.Get(),
                        m_pDepth_SRV.Get(),
                        jitterPx);

    // Bloom/Tonemap
    m_pPostFX->Run(pResolved);               // writes LDR backbuffer

    // UI
    m_pImGuiLayer->OnRender();
}
```

---

## 9. Texture Mip Bias 설정

TAA 켜면 텍스처가 흐려지는 것을 막기 위해 Sampler mip bias `-1.0` 적용. `SamplerStateCache` 에 Prefab 추가:

```cpp
// Engine/Public/RHI/DX11/SamplerStateCache.h  (enum 추가)
enum class eSamplerPreset
{
    LinearWrap,
    LinearClamp,
    PointWrap,
    PointClamp,
    LinearWrap_TAABias,   // 신규: MipLODBias = -1.0
    // ...
};
```

```cpp
// Engine/Private/RHI/DX11/SamplerStateCache.cpp  (LinearWrap_TAABias 추가)
case eSamplerPreset::LinearWrap_TAABias:
    desc.Filter        = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU      = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV      = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW      = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MipLODBias    = -1.0f;
    desc.MaxAnisotropy = 1;
    break;
```

ModelRenderer 가 TAA on 상태면 `LinearWrap_TAABias` 바인드. off 면 `LinearWrap` 유지.

---

## 10. 첫 프레임/씬 전환 처리

```cpp
// 씬 전환 시 (Scene_Manager::Change_Scene 이후)
m_pTAA->Reset();

// 해상도 변경 시
m_pTAA->Resize(newW, newH);
```

Reset 이 없으면 과거 씬 화면이 한두 프레임 섞여 보임.

---

## 11. ImGui 튜너

```cpp
ImGui::Begin("TAA");

const char* patterns[] = { "Halton 2,3 period 8", "Halton 2,3 period 16", "R2 (quasi-random)", "Off (no jitter)" };
i32_t idx = static_cast<i32_t>(m_pTAA->GetJitterPattern());
if (ImGui::Combo("Pattern", &idx, patterns, 4))
    m_pTAA->SetJitterPattern(static_cast<eJitterPattern>(idx));

f32_t blend = m_pTAA->GetBlendFactor();
if (ImGui::SliderFloat("Blend Factor", &blend, 0.02f, 0.5f))
    m_pTAA->SetBlendFactor(blend);

f32_t gamma = m_pTAA->GetGamma();
if (ImGui::SliderFloat("Variance Gamma", &gamma, 0.5f, 2.0f))
    m_pTAA->SetGamma(gamma);

if (ImGui::Button("Reset History")) m_pTAA->Reset();

ImGui::Separator();
ImGui::Text("Current Jitter: %.3f, %.3f (px)", m_vCurJitter.x, m_vCurJitter.y);
ImGui::End();
```

---

## 12. UI 규칙 — TAA 이후 렌더

현재 `CEngineApp::Render` 는 ImGui 를 마지막 단계에서 렌더한다 — 이미 TAA 이후. OK.
단 **게임 내 HUD (HealthBar 등)** 는 ImGui 가 아닌 메시 렌더면, **반드시 TAA 이후 패스**에 배치. Jitter 때문에 1px 흔들림 발생.

→ 현 `CUI_Manager` 의 스크린 오브젝트 렌더 순서 재확인. 필요하면 별도 `UIOverlayPass` 신설.

---

## 13. 디버깅 체크리스트 (Winters 특화)

| 증상 | 원인 | 해결 |
|------|------|------|
| 화면 전체 떨림 | MV 계산에 jitter 포함됨 | MV 생성 셰이더는 `matViewProjNoJitter` 사용 (문서 05 §4) |
| 전환 시 잔상 | `CTAAResolve::Reset` 미호출 | `Change_Scene` 직후 호출 |
| 캐릭터 뒤 고스트 | Variance gamma 낮음 | 1.0 → 1.25, Catmull-Rom 확인 |
| 텍스처 흐림 | Mip bias 미적용 | `LinearWrap_TAABias` 샘플러 바인드 |
| 빠른 동작 시 사라짐 | velocity alpha 너무 약함 | `lerp(blend, 0.2, saturate(mvLen*0.1))` 상수 튜닝 |
| 하이라이트 번쩍 | HDR 직접 블렌드 | `ToneMap`/`ToneUnMap` 쌍 확인 |
| UI 떨림 | UI 렌더가 TAA 전 | UI 순서 이동 |
| Client 빌드 실패 | EngineSDK flat include | `TAAResolve.h` 가 서브폴더 include 쓰는지 확인 |

---

## 14. Stage 7 완료 기준

- [ ] `CTAAResolve` / `JitterSequences` 빌드 통과 + EngineSDK 동기화
- [ ] `Shaders/PostFX/TAA/*` 3 파일 + Bin 복사 확인
- [ ] 정지 씬: 에일리어싱이 3프레임 내 수렴
- [ ] 카메라 회전: 화면 가장자리 얇은 기둥(탑 지붕 등) 떨림 없음
- [ ] 이렐리아 Q 스킬 후반 (빠른 이동): 잔상 < 0.2s
- [ ] ImGui 패턴 Halton 8 ↔ R2 전환 실시간 반영
- [ ] FPS 저하 < 0.5ms @ 1080p

---

## 15. DLSS/FSR 로의 확장 노트

문서 05 의 MV 를 완비하고, TAA 가 본 문서 수준이면 DLSS/FSR 스위치는 간단:

```cpp
// Engine/Public/Renderer/PostFX/Upscaler.h  (장기 Phase — 8 이후)
class WINTERS_ENGINE IUpscaler
{
public:
    virtual ID3D11ShaderResourceView* Upscale(
        ID3D11ShaderResourceView* pColor,
        ID3D11ShaderResourceView* pDepth,
        ID3D11ShaderResourceView* pMV,
        const DirectX::XMFLOAT2& vJitter) = 0;
};

// 구현은 NVSDK_NGX / FSR2 바인딩 — TAA 를 대체
```

---

## 16. 참고

- Karis 2014 — High-Quality Temporal Supersampling (UE4)
- Jimenez 2016 — Filmic SMAA
- Salvi 2016 — Excursion in Temporal Supersampling
- Pedersen 2016 — Temporal Reprojection in INSIDE
- Martin Roberts 2018 — R2 sequence
- `.md\plan\graphics\08_STAGE7_TEMPORAL.md` — 상위 계획

---

*문서 끝. TAA 는 Motion Vector (문서 05) 가 정확해야 성립. 두 문서 함께 읽을 것.*
