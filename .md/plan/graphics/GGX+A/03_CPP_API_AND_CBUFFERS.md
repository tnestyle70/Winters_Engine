# 03. C++ API + cbuffer 매핑 — Winters 측 통합

> 본 문서는 `Engine/Public/Renderer/PBR/*.h` + `.cpp` 신규 파일 전문 + `ModelRenderer` 확장.
> 컨벤션: WINTERS_ENGINE export, flat include, FQN ComPtr, f32_t/u32_t, C 접두사 클래스.

---

## 1. 신규 디렉토리

```
Engine/Public/Renderer/PBR/
├── PBRTypes.h                # POD (CBMaterial / CBDirLight / CBPointLight 등)
├── PBRMaterial.h             # class CPBRMaterial
├── DirectionalLight.h        # class CDirectionalLight
├── PointLight.h              # class CPointLight
└── LightManager.h            # class CLightManager (다중 광원 + StructuredBuffer 업로드)

Engine/Private/Renderer/PBR/
├── PBRMaterial.cpp
├── LightManager.cpp
└── (DirectionalLight / PointLight 은 헤더 only POD wrapper)
```

> `Engine.vcxproj` `AdditionalIncludeDirectories` 에 `Engine/Public/Renderer/PBR` 추가 (flat include 보장).

---

## 2. `Engine/Public/Renderer/PBR/PBRTypes.h`

```cpp
// =========================================================
//  PBRTypes.h — HLSL cbuffer 와 C++ POD 1:1 매핑
//  16바이트 정렬 필수 (DX11 cbuffer 제약).
// =========================================================

#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include <DirectXMath.h>
#include <cstdint>

// ─── b3 슬롯: Material ────────────────────────────────────
struct CBMaterial
{
    DirectX::XMFLOAT3 vAlbedo      = {1.0f, 1.0f, 1.0f};
    f32_t             fMetallic    = 0.0f;

    DirectX::XMFLOAT3 vEmissive    = {0.0f, 0.0f, 0.0f};
    f32_t             fRoughness   = 0.5f;

    f32_t             fAO          = 1.0f;
    f32_t             fReflectance = 0.5f;          // 비금속 F0 = 0.04 * 4 * reflectance² 로 변환 가능
    u32_t             uFlags       = 0;             // bit0=DisneyDiffuse, bit1=MSCompensation
    f32_t             _pad0        = 0.0f;
};
static_assert(sizeof(CBMaterial) % 16 == 0, "CBMaterial must be 16-byte aligned");
static_assert(sizeof(CBMaterial) == 48, "CBMaterial size mismatch");

// ─── b4 슬롯: Directional Light ──────────────────────────
struct CBDirLight
{
    DirectX::XMFLOAT3 vDirection   = {0.0f, -1.0f, 0.0f};
    f32_t             fIntensity   = 1.0f;
    DirectX::XMFLOAT3 vColor       = {1.0f, 1.0f, 1.0f};
    f32_t             _pad0        = 0.0f;
};
static_assert(sizeof(CBDirLight) % 16 == 0);
static_assert(sizeof(CBDirLight) == 32);

// ─── b0 슬롯 확장: PerFrame (View/Proj + Camera Pos) ────
struct CBPerFrame
{
    DirectX::XMFLOAT4X4 matViewProj;
    DirectX::XMFLOAT3   vCameraPos = {0, 0, 0};
    f32_t               fTime      = 0.0f;
};
static_assert(sizeof(CBPerFrame) % 16 == 0);

// ─── b1 슬롯 확장: PerObject (World + WorldInvT) ────────
struct CBPerObject
{
    DirectX::XMFLOAT4X4 matWorld;
    DirectX::XMFLOAT4X4 matWorldInvT;             // (world^-1)^T — normal 변환
};
static_assert(sizeof(CBPerObject) % 16 == 0);

// ─── StructuredBuffer 슬롯: Point Light (Forward+) ──────
struct PointLightGPU
{
    DirectX::XMFLOAT3 vPosition;
    f32_t             fRadius;
    DirectX::XMFLOAT3 vColor;
    f32_t             fIntensity;
};
static_assert(sizeof(PointLightGPU) == 32);

// ─── 플래그 ──────────────────────────────────────────────
enum class ePBRFlag : u32_t
{
    UseDisneyDiffuse  = 1u << 0,
    UseMSCompensation = 1u << 1,
    DoubleSided       = 1u << 2,
};
```

---

## 3. `Engine/Public/Renderer/PBR/PBRMaterial.h`

```cpp
// =========================================================
//  PBRMaterial.h — 재질 객체 (텍스처 5종 + cbuffer 업로드)
//  WINTERS_ENGINE export.
// =========================================================

#pragma once
#include "PBRTypes.h"
#include "WintersAPI.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <string>
#include <memory>

class WINTERS_ENGINE CPBRMaterial
{
public:
    ~CPBRMaterial() = default;
    CPBRMaterial(const CPBRMaterial&)            = delete;     // dllexport + unique_ptr 안전
    CPBRMaterial& operator=(const CPBRMaterial&) = delete;
    CPBRMaterial(CPBRMaterial&&)                 = default;
    CPBRMaterial& operator=(CPBRMaterial&&)      = default;

    static std::unique_ptr<CPBRMaterial> Create(ID3D11Device* pDevice);

    // 텍스처 바인딩 (텍스처별 별도 호출)
    bool LoadAlbedo    (ID3D11DeviceContext* pCtx, const std::wstring& strPath);
    bool LoadNormal    (ID3D11DeviceContext* pCtx, const std::wstring& strPath);
    bool LoadMetalRough(ID3D11DeviceContext* pCtx, const std::wstring& strPath);
    bool LoadAO        (ID3D11DeviceContext* pCtx, const std::wstring& strPath);
    bool LoadEmissive  (ID3D11DeviceContext* pCtx, const std::wstring& strPath);

    // cbuffer 파라미터 setter (ImGui 슬라이더에서 호출)
    void SetAlbedo     (const DirectX::XMFLOAT3& v) { m_Material.vAlbedo = v; }
    void SetMetallic   (f32_t f)                    { m_Material.fMetallic = f; }
    void SetRoughness  (f32_t f)                    { m_Material.fRoughness = f; }
    void SetAO         (f32_t f)                    { m_Material.fAO = f; }
    void SetEmissive   (const DirectX::XMFLOAT3& v) { m_Material.vEmissive = v; }
    void SetReflectance(f32_t f)                    { m_Material.fReflectance = f; }
    void SetFlag       (ePBRFlag flag, bool_t bOn);

    const CBMaterial& GetCBData() const { return m_Material; }

    // 바인딩 (b3 + t0~t4)
    void Bind(ID3D11DeviceContext* pCtx);

private:
    CPBRMaterial() = default;

    Microsoft::WRL::ComPtr<ID3D11Buffer>             m_pCBMaterial;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pAlbedoSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pNormalSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pMetalRoughSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pAOSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pEmissiveSRV;

    CBMaterial m_Material{};
};
```

---

## 4. `Engine/Private/Renderer/PBR/PBRMaterial.cpp`

```cpp
#include "PBRMaterial.h"
#include "WintersPCH.h"
#include <DirectXTex.h>
#include <directxtk/WICTextureLoader.h>

std::unique_ptr<CPBRMaterial> CPBRMaterial::Create(ID3D11Device* pDevice)
{
    auto pMat = std::unique_ptr<CPBRMaterial>(new CPBRMaterial());

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth      = sizeof(CBMaterial);
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(pDevice->CreateBuffer(&desc, nullptr, pMat->m_pCBMaterial.GetAddressOf())))
        return nullptr;

    return pMat;
}

bool CPBRMaterial::LoadAlbedo(ID3D11DeviceContext* pCtx, const std::wstring& strPath)
{
    Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
    pCtx->GetDevice(pDevice.GetAddressOf());
    HRESULT hr = DirectX::CreateWICTextureFromFile(
        pDevice.Get(), pCtx, strPath.c_str(), nullptr, m_pAlbedoSRV.GetAddressOf());
    return SUCCEEDED(hr);
}

// LoadNormal / LoadMetalRough / LoadAO / LoadEmissive 동일 패턴
// (... 생략, 실제 구현 시 5개 다 동일)

void CPBRMaterial::SetFlag(ePBRFlag flag, bool_t bOn)
{
    if (bOn) m_Material.uFlags |=  static_cast<u32_t>(flag);
    else     m_Material.uFlags &= ~static_cast<u32_t>(flag);
}

void CPBRMaterial::Bind(ID3D11DeviceContext* pCtx)
{
    // 1. cbuffer 업데이트
    D3D11_MAPPED_SUBRESOURCE map{};
    pCtx->Map(m_pCBMaterial.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &m_Material, sizeof(CBMaterial));
    pCtx->Unmap(m_pCBMaterial.Get(), 0);

    // 2. PS 에 b3 바인딩
    ID3D11Buffer* cb = m_pCBMaterial.Get();
    pCtx->PSSetConstantBuffers(3, 1, &cb);

    // 3. 텍스처 t0~t4 바인딩
    ID3D11ShaderResourceView* srvs[5] = {
        m_pAlbedoSRV.Get(),
        m_pNormalSRV.Get(),
        m_pMetalRoughSRV.Get(),
        m_pAOSRV.Get(),
        m_pEmissiveSRV.Get()
    };
    pCtx->PSSetShaderResources(0, 5, srvs);
}
```

---

## 5. `Engine/Public/Renderer/PBR/DirectionalLight.h`

```cpp
// =========================================================
//  DirectionalLight.h — 단일 태양광 (Stage 4 검증용)
//  POD wrapper — 매니저 X. CLightManager 가 다수 처리.
// =========================================================

#pragma once
#include "PBRTypes.h"
#include "WintersAPI.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>

class WINTERS_ENGINE CDirectionalLight
{
public:
    ~CDirectionalLight() = default;
    CDirectionalLight(const CDirectionalLight&)            = delete;
    CDirectionalLight& operator=(const CDirectionalLight&) = delete;

    static std::unique_ptr<CDirectionalLight> Create(ID3D11Device* pDevice);

    void SetDirection(const DirectX::XMFLOAT3& v);
    void SetColor    (const DirectX::XMFLOAT3& v) { m_Data.vColor     = v; }
    void SetIntensity(f32_t f)                    { m_Data.fIntensity = f; }

    const CBDirLight& GetCBData() const { return m_Data; }

    // PS b4 슬롯 바인딩
    void Bind(ID3D11DeviceContext* pCtx);

private:
    CDirectionalLight() = default;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCB;
    CBDirLight m_Data{};
};
```

```cpp
// DirectionalLight.cpp 구현 핵심
void CDirectionalLight::SetDirection(const DirectX::XMFLOAT3& v)
{
    DirectX::XMVECTOR n = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&v));
    DirectX::XMStoreFloat3(&m_Data.vDirection, n);
}

void CDirectionalLight::Bind(ID3D11DeviceContext* pCtx)
{
    D3D11_MAPPED_SUBRESOURCE map{};
    pCtx->Map(m_pCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &m_Data, sizeof(CBDirLight));
    pCtx->Unmap(m_pCB.Get(), 0);

    ID3D11Buffer* cb = m_pCB.Get();
    pCtx->PSSetConstantBuffers(4, 1, &cb);
}
```

---

## 6. `Engine/Public/Renderer/PBR/LightManager.h`

```cpp
// =========================================================
//  LightManager.h — directional + N point lights
//  Stage 4: directional 만
//  Stage 5+: point lights → StructuredBuffer 업로드 → Forward+
//  CGameInstance Tier-2 Get_LightManager() 로 노출.
// =========================================================

#pragma once
#include "PBRTypes.h"
#include "DirectionalLight.h"
#include "PointLight.h"
#include "WintersAPI.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>
#include <vector>

class WINTERS_ENGINE CLightManager
{
public:
    static constexpr u32_t kMaxPointLights = 1024;

    ~CLightManager() = default;
    CLightManager(const CLightManager&)            = delete;
    CLightManager& operator=(const CLightManager&) = delete;

    static std::unique_ptr<CLightManager> Create(ID3D11Device* pDevice);

    CDirectionalLight* GetSun() { return m_pSun.get(); }

    // 점광원 동적 등록 (스킬 FX 같은 단발성)
    u32_t AddPointLight(const PointLightGPU& light);
    void  RemovePointLight(u32_t handle);
    void  UpdatePointLight(u32_t handle, const PointLightGPU& light);
    void  ClearPointLights();

    // 매 프레임 호출 — StructuredBuffer 업로드 + 바인딩 (t10)
    void UploadAndBind(ID3D11DeviceContext* pCtx);

    u32_t GetPointLightCount() const { return static_cast<u32_t>(m_PointLights.size()); }

    // Forward+ Light Cull CS 가 사용
    ID3D11ShaderResourceView* GetPointLightSRV() const { return m_pLightSRV.Get(); }

private:
    CLightManager() = default;

    bool InitStructuredBuffer(ID3D11Device* pDevice);

    std::unique_ptr<CDirectionalLight>                m_pSun;
    std::vector<PointLightGPU>                        m_PointLights;
    std::vector<u32_t>                                m_FreeList;     // handle 재사용

    Microsoft::WRL::ComPtr<ID3D11Buffer>              m_pLightBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_pLightSRV;
    bool_t                                            m_bDirty = true;
};
```

```cpp
// LightManager.cpp 핵심
std::unique_ptr<CLightManager> CLightManager::Create(ID3D11Device* pDevice)
{
    auto pMgr = std::unique_ptr<CLightManager>(new CLightManager());
    pMgr->m_pSun = CDirectionalLight::Create(pDevice);

    if (!pMgr->InitStructuredBuffer(pDevice))
        return nullptr;

    pMgr->m_PointLights.reserve(kMaxPointLights);
    return pMgr;
}

bool CLightManager::InitStructuredBuffer(ID3D11Device* pDevice)
{
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth           = sizeof(PointLightGPU) * kMaxPointLights;
    bd.Usage               = D3D11_USAGE_DYNAMIC;
    bd.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(PointLightGPU);

    if (FAILED(pDevice->CreateBuffer(&bd, nullptr, m_pLightBuffer.GetAddressOf())))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format        = DXGI_FORMAT_UNKNOWN;
    sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sd.Buffer.ElementOffset = 0;
    sd.Buffer.NumElements   = kMaxPointLights;

    return SUCCEEDED(pDevice->CreateShaderResourceView(
        m_pLightBuffer.Get(), &sd, m_pLightSRV.GetAddressOf()));
}

u32_t CLightManager::AddPointLight(const PointLightGPU& light)
{
    if (!m_FreeList.empty()) {
        u32_t h = m_FreeList.back();
        m_FreeList.pop_back();
        m_PointLights[h] = light;
        m_bDirty = true;
        return h;
    }
    if (m_PointLights.size() >= kMaxPointLights) return UINT32_MAX;
    m_PointLights.push_back(light);
    m_bDirty = true;
    return static_cast<u32_t>(m_PointLights.size() - 1);
}

void CLightManager::UploadAndBind(ID3D11DeviceContext* pCtx)
{
    m_pSun->Bind(pCtx);

    if (m_bDirty && !m_PointLights.empty()) {
        D3D11_MAPPED_SUBRESOURCE map{};
        pCtx->Map(m_pLightBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
        memcpy(map.pData, m_PointLights.data(),
               sizeof(PointLightGPU) * m_PointLights.size());
        pCtx->Unmap(m_pLightBuffer.Get(), 0);
        m_bDirty = false;
    }

    ID3D11ShaderResourceView* srv = m_pLightSRV.Get();
    pCtx->PSSetShaderResources(10, 1, &srv);    // t10 = Lights
}
```

---

## 7. CGameInstance Tier-2 Getter 추가

### BEFORE — `Engine/Include/GameInstance.h` (현재 상태)

```cpp
// 클래스 내부 메서드 목록 (요약)
class WINTERS_ENGINE CGameInstance {
public:
    DECLARE_SINGLETON(CGameInstance)
    // ...
    CDX11Device*       Get_RHIDevice();
    DX11Shader*        Get_MeshShader();
    DX11Pipeline*      Get_MeshPipeline();
    CBlendStateCache*  Get_BlendStateCache();
    // ...
};
```

### AFTER — getter 2개 추가

```cpp
// Engine/Include/GameInstance.h L60 부근 (RHI 게터 묶음 내부)
class WINTERS_ENGINE CGameInstance {
public:
    DECLARE_SINGLETON(CGameInstance)
    // ...
    CDX11Device*           Get_RHIDevice();
    DX11Shader*            Get_MeshShader();
    DX11Pipeline*          Get_MeshPipeline();
    CBlendStateCache*      Get_BlendStateCache();

    // [Phase E] PBR/Forward+ Tier-2 게터 (신규)
    CLightManager*         Get_LightManager();
    CForwardPlusPipeline*  Get_ForwardPlusPipeline();
    // ...
};

// 전방선언 추가 (헤더 상단)
class CLightManager;
class CForwardPlusPipeline;
```

### 구현 — `Engine/Private/GameInstance.cpp`

```cpp
// 기존 Get_RHIDevice 등이 정의된 블록 끝에 추가:
CLightManager* CGameInstance::Get_LightManager()
{
    return CEngineApp::Get().GetLightManager();
}

CForwardPlusPipeline* CGameInstance::Get_ForwardPlusPipeline()
{
    return CEngineApp::Get().GetForwardPlusPipeline();
}
```

`CEngineApp` 도 동일 패턴으로 멤버 추가 — 디바이스 생성 직후 `CLightManager::Create(...)` 호출.

---

## 8. ModelRenderer 확장 — 셰이더 + Material 분기

### 현재 시그니처 (`Engine/Public/Renderer/ModelRenderer.h:18-19`)

```cpp
bool Init(const std::string& strFbxPath,
          const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");
```

### 확장 — Material 인자 + cbuffer 슬롯 의식

```cpp
// Engine/Public/Renderer/ModelRenderer.h
class WINTERS_ENGINE ModelRenderer
{
public:
    // ... 기존 메서드 그대로 ...

    // [Phase E] PBR 분기 — 셰이더 + Material 동시 지정
    bool InitPBR(const std::string& strFbxPath,
                 std::unique_ptr<CPBRMaterial> pMaterial);

    // 기존 unlit 호출 (그대로)
    bool Init(const std::string& strFbxPath,
              const wchar_t* pHlslPath = L"Shaders/Mesh3D.hlsl");

    // PBR 모드 여부
    bool_t IsPBR() const;

    CPBRMaterial* GetPBRMaterial();
};
```

### `ModelRenderer::Render()` 분기 추가 (`Engine/Private/Renderer/ModelRenderer.cpp`)

#### BEFORE (대략 L200 부근, Render 본체)

```cpp
void ModelRenderer::Render()
{
    // 1. 셰이더 바인드
    m_pImpl->shader.Bind(m_pImpl->ctx);
    // 2. Per-frame / Per-object cbuffer 업로드
    UpdateCBuffers();
    // 3. 텍스처 t0 바인드 (diffuse only)
    m_pImpl->ctx->PSSetShaderResources(0, 1, &m_pImpl->diffuseSRV);
    // 4. 메시 그리기
    DrawMeshes();
}
```

#### AFTER

```cpp
void ModelRenderer::Render()
{
    m_pImpl->shader.Bind(m_pImpl->ctx);
    UpdateCBuffers();

    if (m_pImpl->bPBR && m_pImpl->pMaterial) {
        // [Phase E] PBR 경로
        m_pImpl->pMaterial->Bind(m_pImpl->ctx);

        // 광원 — Tier-2 게터 직접
        CLightManager* pLights = CGameInstance::Get()->Get_LightManager();
        if (pLights) pLights->UploadAndBind(m_pImpl->ctx);
    } else {
        // 기존 unlit 경로 (변경 없음)
        m_pImpl->ctx->PSSetShaderResources(0, 1, &m_pImpl->diffuseSRV);
    }

    DrawMeshes();
}
```

---

## 9. cbuffer 슬롯 충돌 매트릭스

| 슬롯 | unlit (Mesh3D) | unlit Skinned (Skinned3D) | PBR (Mesh3D_PBR) | PBR Skinned (Skinned3D_PBR) | FX (FxSprite/FxMesh) |
|---|---|---|---|---|---|
| b0 | ViewProj | ViewProj | ViewProj + CamPos + Time | ViewProj + CamPos + Time | (b0 미사용 — VS 만 b0 ViewProj) |
| b1 | World | World | World + WorldInvT | World + WorldInvT | World |
| b2 | — | BoneMatrices | — | BoneMatrices | FxParams |
| b3 | — | — | **Material** | **Material** | — |
| b4 | — | — | **DirLight** | **DirLight** | — |
| t0 | Diffuse | Diffuse | **Albedo** | **Albedo** | Diffuse |
| t1 | — | — | **Normal** | **Normal** | (Atlas 등) |
| t2 | — | — | **MetalRough** | **MetalRough** | — |
| t3 | — | — | **AO** | **AO** | — |
| t4 | — | — | **Emissive** | **Emissive** | — |
| t10 | — | — | **PointLightSB** | **PointLightSB** | — |
| t11 | — | — | **LightGrid** | **LightGrid** | — |

→ **충돌 없음** — PBR 은 b3/b4/t1~t4/t10/t11 만 추가. unlit 셰이더는 b3/b4 안 읽으므로 영향 0.

---

## 10. PBR 디버그 ImGui 패널

`Engine/Public/Editor/PBRDebugPanel.h` 신규:

```cpp
class WINTERS_ENGINE CPBRDebugPanel
{
public:
    static void Draw(CPBRMaterial* pMat, CDirectionalLight* pSun);
};
```

```cpp
// 구현
void CPBRDebugPanel::Draw(CPBRMaterial* pMat, CDirectionalLight* pSun)
{
    ImGui::Begin("PBR Debug");

    if (pMat) {
        auto& m = const_cast<CBMaterial&>(pMat->GetCBData());  // 슬라이더용
        ImGui::ColorEdit3("Albedo",       &m.vAlbedo.x);
        ImGui::SliderFloat("Metallic",    &m.fMetallic,    0.f, 1.f);
        ImGui::SliderFloat("Roughness",   &m.fRoughness,   0.04f, 1.f);
        ImGui::SliderFloat("AO",          &m.fAO,          0.f, 1.f);
        ImGui::SliderFloat("Reflectance", &m.fReflectance, 0.f, 1.f);
        ImGui::ColorEdit3("Emissive",     &m.vEmissive.x);
    }

    if (pSun) {
        auto& d = const_cast<CBDirLight&>(pSun->GetCBData());
        ImGui::SliderFloat3("Sun Direction", &d.vDirection.x, -1.f, 1.f);
        ImGui::ColorEdit3("Sun Color",       &d.vColor.x);
        ImGui::SliderFloat("Sun Intensity",  &d.fIntensity, 0.f, 10.f);
    }

    ImGui::End();
}
```

→ Scene_InGame::OnImGui() 에서 호출. CLAUDE.md "튜닝 파라미터는 ImGui 슬라이더 노출" 의무 충족.

---

## 11. 빌드 후 1회 검증 — 단일 광원 메탈 구

`Scene_InGame::OnEnter()` 끝에 임시 테스트 코드:

```cpp
// 테스트 — 메탈볼 + 태양광
auto pMat = CPBRMaterial::Create(pDevice);
pMat->LoadAlbedo(pCtx, L"Client/Bin/Resource/Texture/Test/gold_albedo.png");
pMat->SetMetallic(1.0f);
pMat->SetRoughness(0.2f);

m_pTestSphere->InitPBR("Client/Bin/Resource/Mesh/Test/sphere.fbx", std::move(pMat));

auto* pSun = CGameInstance::Get()->Get_LightManager()->GetSun();
pSun->SetDirection({0.3f, -1.0f, 0.5f});
pSun->SetIntensity(3.0f);
pSun->SetColor({1.0f, 0.95f, 0.8f});
```

→ 스크린에 **금빛 메탈 구** 가 태양광 방향에서 highlights 가 움직이는 게 보이면 Stage 4 통과.

---

## 다음 문서

→ `04_FORWARD_PLUS_LIGHT_CULLING.md` — Compute Shader 라이트 컬링, Tile Grid 자료구조, depth pre-pass 통합.

## 검증 체크리스트 (이 단계 완료 조건)

- [ ] `CBMaterial`, `CBDirLight`, `PointLightGPU` 모두 `static_assert(% 16 == 0)` 통과.
- [ ] `CPBRMaterial::Create()` 가 nullptr 안 반환 (cbuffer 생성 성공).
- [ ] `LoadAlbedo` 가 sRGB 텍스처 포맷 (`DXGI_FORMAT_R8G8B8A8_UNORM_SRGB`) 으로 로드.
- [ ] `CLightManager::AddPointLight` → `RemovePointLight` 후 handle 재사용.
- [ ] CGameInstance::Get_LightManager() 가 nullptr 아닌 객체 반환.
- [ ] ImGui 슬라이더로 metallic/roughness 조작 시 셰이더 결과 변경 (정확히 metalic=1 시 검정 → 광원 켜면 색상).
- [ ] **dllexport + unique_ptr 멤버 = copy delete 명시** 컨벤션 준수 (`CPBRMaterial`, `CDirectionalLight`, `CLightManager` 모두).
