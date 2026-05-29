# 05. Rendering Pipeline -- UE5-Style RenderGraph / MeshDrawCommand / MaterialInstance / SceneProxy

> **UE5 대응**: `FRDGBuilder` (RDG), `FMeshDrawCommand`, `FMaterialRenderProxy`, `FPrimitiveSceneProxy`, `FMeshBatch`
> **현재 Winters**: Immediate-mode `ModelRenderer::Render(worldMat)` 수동 호출, 6개 unlit HLSL 고정, cbuffer 수동 바인딩, 재질 시스템 0%
> **목표**: RenderGraph 기반 패스 의존성 관리, MeshDrawCommand 캐시로 DrawCall 자동 수집, MaterialInstance 파라미터화, SceneProxy 로 렌더 스레드 분리 준비

---

## 1. Architecture Overview

### 1.1 UE5 Rendering Pipeline 핵심

```
AActor
  └── UMeshComponent
       └── CreateSceneProxy() → FPrimitiveSceneProxy (render thread copy)
            └── GetDynamicMeshElements() → FMeshBatch → FMeshDrawCommand

FMeshDrawCommand (cached):
  - ShaderBindings (vertex/index buffer, cbuffers, textures)
  - PipelineState (shader, rasterizer, blend, depth)
  - DrawPrimitiveArgs (indexCount, instanceCount)

FRDGBuilder (Render Dependency Graph):
  - AddPass("BasePass", ...) → dependencies tracked → auto barrier
  - AddPass("Lighting", depends: BasePass)
  - AddPass("PostProcess", depends: Lighting)

FMaterialRenderProxy:
  - Material parameters (albedo, metallic, roughness, emissive)
  - Shader permutation selection
  - Texture bindings
```

### 1.2 현재 Winters 렌더링 문제

```
Scene_InGame::OnRender() — 수동 나열:
  m_Map.Render();
  m_Irelia.Render();
  m_Yasuo.Render();
  m_Sylas.Render();
  ...

ModelRenderer::Render() 내부:
  pContext->VSSetShader(m_pVS, ...);
  pContext->PSSetShader(m_pPS, ...);
  pContext->IASetVertexBuffers(...);
  pContext->UpdateSubresource(m_pCB_PerObject, ...);
  pContext->DrawIndexed(indexCount, 0, 0);
  // 매 Render 호출마다 셰이더/파이프라인/cbuffer 반복 바인딩

문제:
  1. 셰이더 전환 최적화 0 (같은 셰이더인데 매번 SetShader)
  2. 패스 순서 하드코딩 (NormalPass → SSAOPass → Forward 순서 수동)
  3. 재질 = 텍스처 1장 (albedo/metallic/roughness/AO 파라미터 없음)
  4. 렌더 호출 = 게임 스레드 블로킹
  5. 가시성/거리 LOD 없음 (화면 밖 메시도 DrawCall)
```

### 1.3 Winters Rendering Pipeline 설계

```
WActor
  └── WMeshComponent
       └── CreateSceneProxy() → FSceneProxy (렌더 데이터 스냅샷)
            └── CollectMeshDrawCommands() → CMeshDrawCommand

CMeshDrawCommand (cached):
  - Shader handle (DX11Shader*)
  - Pipeline handle (DX11Pipeline*)
  - Vertex/Index buffer
  - Material instance (CMaterialInstance*)
  - World matrix
  - DrawArgs (indexCount, startIndex, baseVertex)

CRenderGraph:
  - AddPass("DepthPrePass", ...)
  - AddPass("NormalPass", depends: DepthPrePass)
  - AddPass("SSAOPass", depends: NormalPass + DepthPrePass)
  - AddPass("ForwardPBR", depends: SSAOPass)
  - AddPass("FX", depends: ForwardPBR)
  - AddPass("UI", depends: FX)
  - Execute() → topological sort → sequential draw

CMaterialInstance:
  - Base material (shader + pipeline)
  - PBR parameters (albedo, metallic, roughness, emissive, AO)
  - Texture bindings (albedo map, normal map, etc.)
  - cbuffer b3 PerMaterial auto-upload
```

---

## 2. 파일 구조

```
Engine/
├── Public/Renderer/
│   ├── CRenderGraph.h            -- 렌더 패스 의존성 그래프
│   ├── CMeshDrawCommand.h        -- 캐시된 드로우 커맨드
│   ├── CMaterialInstance.h       -- 파라미터화된 재질
│   ├── FSceneProxy.h             -- 액터 렌더 데이터 스냅샷
│   └── CBPerMaterial.h           -- cbuffer b3 PerMaterial 레이아웃 (이미 존재)
├── Private/Renderer/
│   ├── CRenderGraph.cpp
│   ├── CMeshDrawCommand.cpp
│   ├── CMaterialInstance.cpp
│   └── FSceneProxy.cpp
```

---

## 3. 코드 전문

### `Engine/Public/Renderer/CMeshDrawCommand.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"

#include <cstdint>

class DX11Shader;
class DX11Pipeline;
class CMaterialInstance;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;

/// UE5 FMeshDrawCommand 대응
/// 단일 DrawCall 에 필요한 모든 GPU 상태를 캡슐화.
/// 정렬 키로 셰이더/머티리얼/깊이 순서 정렬 → 상태 변경 최소화.
struct WINTERS_API CMeshDrawCommand
{
    // ---- Shader / Pipeline ----
    DX11Shader*     pShader = nullptr;
    DX11Pipeline*   pPipeline = nullptr;

    // ---- Geometry ----
    ID3D11Buffer*   pVertexBuffer = nullptr;
    ID3D11Buffer*   pIndexBuffer = nullptr;
    u32_t           iVertexStride = 0;
    u32_t           iIndexCount = 0;
    u32_t           iStartIndexLocation = 0;
    i32_t           iBaseVertexLocation = 0;

    // ---- Material ----
    CMaterialInstance* pMaterial = nullptr;

    // ---- Transform ----
    Mat4            matWorld{};

    // ---- Skeleton (optional) ----
    ID3D11Buffer*   pBoneMatrixBuffer = nullptr;  // cbuffer b2
    bool            bSkinned = false;

    // ---- Textures ----
    ID3D11ShaderResourceView* pAlbedoSRV = nullptr;
    ID3D11ShaderResourceView* pNormalSRV = nullptr;
    ID3D11ShaderResourceView* pAOSRV = nullptr;

    // ---- Sort Key ----
    /// 정렬 우선순위: shader(16) | material(16) | depth(32)
    u64_t GetSortKey() const;

    /// 상태 적용 + DrawIndexed 실행
    void Execute(struct ID3D11DeviceContext* pContext) const;

    /// 동일 셰이더/파이프라인인지 비교 (상태 전환 skip)
    bool IsSameState(const CMeshDrawCommand& other) const;
};
```

### `Engine/Private/Renderer/CMeshDrawCommand.cpp`

```cpp
#include "Renderer/CMeshDrawCommand.h"
#include "Renderer/CMaterialInstance.h"
#include "RHI/DX11/DX11Shader.h"
#include "RHI/DX11/DX11Pipeline.h"

#include <d3d11.h>

u64_t CMeshDrawCommand::GetSortKey() const
{
    // 상위 16비트: 셰이더 해시
    u64_t shaderKey = reinterpret_cast<uintptr_t>(pShader) & 0xFFFF;
    // 중위 16비트: 머티리얼 해시
    u64_t materialKey = reinterpret_cast<uintptr_t>(pMaterial) & 0xFFFF;
    // 하위 32비트: 깊이 (뷰 Z, 정수 변환)
    // 현재: world Z 로 근사 (카메라 고정 MOBA 에서 충분)
    u32_t depthKey = static_cast<u32_t>((matWorld.m[3][2] + 1000.f) * 100.f);

    return (shaderKey << 48) | (materialKey << 32) | depthKey;
}

void CMeshDrawCommand::Execute(ID3D11DeviceContext* pContext) const
{
    if (!pContext || !pVertexBuffer || !pIndexBuffer) return;

    // 1. IA 바인딩
    UINT offset = 0;
    pContext->IASetVertexBuffers(0, 1, &pVertexBuffer, &iVertexStride, &offset);
    pContext->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. 텍스처 바인딩 (PS slot 0 = albedo, 1 = normal, 2 = AO)
    if (pAlbedoSRV)
        pContext->PSSetShaderResources(0, 1, &pAlbedoSRV);
    if (pNormalSRV)
        pContext->PSSetShaderResources(1, 1, &pNormalSRV);
    if (pAOSRV)
        pContext->PSSetShaderResources(2, 1, &pAOSRV);

    // 3. 스키닝 본 매트릭스 (b2)
    if (bSkinned && pBoneMatrixBuffer)
        pContext->VSSetConstantBuffers(2, 1, &pBoneMatrixBuffer);

    // 4. 머티리얼 cbuffer (b3) — CMaterialInstance 가 업로드
    if (pMaterial)
        pMaterial->BindToContext(pContext);

    // 5. DrawIndexed
    pContext->DrawIndexed(iIndexCount, iStartIndexLocation, iBaseVertexLocation);
}

bool CMeshDrawCommand::IsSameState(const CMeshDrawCommand& other) const
{
    return pShader == other.pShader
        && pPipeline == other.pPipeline
        && pMaterial == other.pMaterial;
}
```

### `Engine/Public/Renderer/CMaterialInstance.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "Object/ObjectMacros.h"

#include <string>
#include <memory>

struct ID3D11Buffer;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
class CDX11Device;
class DX11Shader;
class DX11Pipeline;

/// PBR 머티리얼 cbuffer 레이아웃 (register b3)
/// 16바이트 정렬 강제 (HLSL packing rules)
struct alignas(16) CBPerMaterial
{
    Vec4  vAlbedoColor  = { 1.f, 1.f, 1.f, 1.f };  // 0~15
    f32_t fMetallic     = 0.f;                        // 16
    f32_t fRoughness    = 0.5f;                       // 20
    f32_t fAO           = 1.f;                        // 24
    f32_t fEmissive     = 0.f;                        // 28
    Vec4  vEmissiveColor = { 0.f, 0.f, 0.f, 0.f };  // 32~47
    // total: 48 bytes → pad to 48 (multiple of 16)
};
static_assert(sizeof(CBPerMaterial) == 48,
              "CBPerMaterial must be 48 bytes (3 * 16B aligned)");

/// UE5 FMaterialRenderProxy / UMaterialInstance 대응
/// 셰이더 + 파이프라인 + PBR 파라미터 + 텍스처 바인딩을 캡슐화.
/// WPROPERTY(EditAnywhere) 로 ImGui 에서 실시간 튜닝 가능.
WCLASS()
class WINTERS_API CMaterialInstance : public WObject
{
    using Super = WObject;
    WINTERS_GENERATED_BODY(CMaterialInstance)

public:
    virtual ~CMaterialInstance();

    /// 팩토리
    static std::unique_ptr<CMaterialInstance> Create(
        CDX11Device* pDevice,
        DX11Shader* pShader,
        DX11Pipeline* pPipeline);

    // ---- PBR Parameters ----

    WPROPERTY(EditAnywhere, Category = "Material")
    Vec4 m_vAlbedoColor = { 1.f, 1.f, 1.f, 1.f };

    WPROPERTY(EditAnywhere, Slider, Category = "Material")
    f32_t m_fMetallic = 0.f;

    WPROPERTY(EditAnywhere, Slider, Category = "Material")
    f32_t m_fRoughness = 0.5f;

    WPROPERTY(EditAnywhere, Slider, Category = "Material")
    f32_t m_fAO = 1.f;

    WPROPERTY(EditAnywhere, Slider, Category = "Material")
    f32_t m_fEmissive = 0.f;

    WPROPERTY(EditAnywhere, Category = "Material")
    Vec4 m_vEmissiveColor = { 0.f, 0.f, 0.f, 0.f };

    // ---- Texture Bindings ----

    void SetAlbedoTexture(ID3D11ShaderResourceView* pSRV) { m_pAlbedoSRV = pSRV; }
    void SetNormalTexture(ID3D11ShaderResourceView* pSRV) { m_pNormalSRV = pSRV; }
    void SetAOTexture(ID3D11ShaderResourceView* pSRV) { m_pAOSRV = pSRV; }
    void SetEmissiveTexture(ID3D11ShaderResourceView* pSRV) { m_pEmissiveSRV = pSRV; }

    ID3D11ShaderResourceView* GetAlbedoSRV() const { return m_pAlbedoSRV; }
    ID3D11ShaderResourceView* GetNormalSRV() const { return m_pNormalSRV; }

    // ---- Shader / Pipeline ----

    DX11Shader* GetShader() const { return m_pShader; }
    DX11Pipeline* GetPipeline() const { return m_pPipeline; }

    // ---- GPU Upload ----

    /// cbuffer 데이터를 GPU 에 업로드 (dirty 시에만)
    void UploadToGPU(ID3D11DeviceContext* pContext);

    /// 컨텍스트에 cbuffer b3 바인딩
    void BindToContext(ID3D11DeviceContext* pContext) const;

    /// dirty flag 수동 설정
    void MarkDirty() { m_bDirty = true; }

protected:
    CMaterialInstance();

private:
    DX11Shader*   m_pShader = nullptr;     // non-owning
    DX11Pipeline* m_pPipeline = nullptr;   // non-owning

    // GPU cbuffer
    ID3D11Buffer* m_pCBPerMaterial = nullptr;  // 소유 (Release 필요)
    bool          m_bDirty = true;

    // Textures (non-owning SRV)
    ID3D11ShaderResourceView* m_pAlbedoSRV = nullptr;
    ID3D11ShaderResourceView* m_pNormalSRV = nullptr;
    ID3D11ShaderResourceView* m_pAOSRV = nullptr;
    ID3D11ShaderResourceView* m_pEmissiveSRV = nullptr;
};
```

### `Engine/Private/Renderer/CMaterialInstance.cpp`

```cpp
#include "Renderer/CMaterialInstance.h"
#include "RHI/CDX11Device.h"

#include <d3d11.h>

void CMaterialInstance::RegisterProperties(WClass* cls)
{
    REGISTER_PROPERTY(CMaterialInstance, m_vAlbedoColor,
                      ePropertyFlags::EditAnywhere);
    REGISTER_PROPERTY_RANGE(CMaterialInstance, m_fMetallic,
                            ePropertyFlags::EditAnywhere | ePropertyFlags::Slider,
                            0.0, 1.0);
    REGISTER_PROPERTY_RANGE(CMaterialInstance, m_fRoughness,
                            ePropertyFlags::EditAnywhere | ePropertyFlags::Slider,
                            0.01, 1.0);
    REGISTER_PROPERTY_RANGE(CMaterialInstance, m_fAO,
                            ePropertyFlags::EditAnywhere | ePropertyFlags::Slider,
                            0.0, 1.0);
    REGISTER_PROPERTY_RANGE(CMaterialInstance, m_fEmissive,
                            ePropertyFlags::EditAnywhere | ePropertyFlags::Slider,
                            0.0, 10.0);
    REGISTER_PROPERTY(CMaterialInstance, m_vEmissiveColor,
                      ePropertyFlags::EditAnywhere);
}

CMaterialInstance::CMaterialInstance()
{
}

CMaterialInstance::~CMaterialInstance()
{
    if (m_pCBPerMaterial)
    {
        m_pCBPerMaterial->Release();
        m_pCBPerMaterial = nullptr;
    }
}

std::unique_ptr<CMaterialInstance> CMaterialInstance::Create(
    CDX11Device* pDevice,
    DX11Shader* pShader,
    DX11Pipeline* pPipeline)
{
    if (!pDevice) return nullptr;

    auto pMat = std::unique_ptr<CMaterialInstance>(new CMaterialInstance());
    pMat->m_pShader = pShader;
    pMat->m_pPipeline = pPipeline;

    // cbuffer 생성
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = sizeof(CBPerMaterial);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;

    CBPerMaterial initData{};
    D3D11_SUBRESOURCE_DATA subData{};
    subData.pSysMem = &initData;

    HRESULT hr = pDevice->GetDevice()->CreateBuffer(&desc, &subData,
                                                     &pMat->m_pCBPerMaterial);
    if (FAILED(hr))
        return nullptr;

    return pMat;
}

void CMaterialInstance::UploadToGPU(ID3D11DeviceContext* pContext)
{
    if (!m_bDirty || !m_pCBPerMaterial || !pContext) return;

    CBPerMaterial data;
    data.vAlbedoColor = m_vAlbedoColor;
    data.fMetallic = m_fMetallic;
    data.fRoughness = m_fRoughness;
    data.fAO = m_fAO;
    data.fEmissive = m_fEmissive;
    data.vEmissiveColor = m_vEmissiveColor;

    pContext->UpdateSubresource(m_pCBPerMaterial, 0, nullptr, &data, 0, 0);
    m_bDirty = false;
}

void CMaterialInstance::BindToContext(ID3D11DeviceContext* pContext) const
{
    if (!pContext || !m_pCBPerMaterial) return;

    // cbuffer b3 = PerMaterial
    pContext->VSSetConstantBuffers(3, 1, &m_pCBPerMaterial);
    pContext->PSSetConstantBuffers(3, 1, &m_pCBPerMaterial);

    // 텍스처 바인딩 (PS t0~t3)
    if (m_pAlbedoSRV)
        pContext->PSSetShaderResources(0, 1, &m_pAlbedoSRV);
    if (m_pNormalSRV)
        pContext->PSSetShaderResources(1, 1, &m_pNormalSRV);
    if (m_pAOSRV)
        pContext->PSSetShaderResources(2, 1, &m_pAOSRV);
    if (m_pEmissiveSRV)
        pContext->PSSetShaderResources(3, 1, &m_pEmissiveSRV);
}
```

### `Engine/Public/Renderer/FSceneProxy.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "WintersMath.h"
#include "Renderer/CMeshDrawCommand.h"

#include <vector>

class WMeshComponent;
class CMaterialInstance;

/// UE5 FPrimitiveSceneProxy 대응
/// 게임 스레드의 WMeshComponent 데이터를 렌더 스레드용으로 스냅샷.
/// 현재 DX11 단일 스레드이므로 스냅샷 = 참조, 향후 멀티스레드 렌더 시 값 복사로 전환.
///
/// 역할:
///   1. WMeshComponent 의 메시/머티리얼/트랜스폼을 수집
///   2. CMeshDrawCommand 배열 생성 (메시 당 1개)
///   3. CRenderGraph 가 SceneProxy 에서 DrawCommand 를 수집하여 정렬/실행
struct WINTERS_API FSceneProxy
{
    /// 소유 컴포넌트 (non-owning reference)
    WMeshComponent* pOwnerComponent = nullptr;

    /// 월드 매트릭스 (스냅샷)
    Mat4 matWorld{};

    /// 가시성
    bool bVisible = true;

    /// 스키닝 여부
    bool bSkinned = false;

    /// 이 프록시가 생성하는 드로우 커맨드 목록
    std::vector<CMeshDrawCommand> DrawCommands;

    /// WMeshComponent 에서 드로우 커맨드 수집
    void CollectDrawCommands();

    /// 가시성 테스트 (향후 frustum culling)
    bool IsVisible() const { return bVisible; }

    /// 유효성 검사
    bool IsValid() const { return pOwnerComponent != nullptr && !DrawCommands.empty(); }
};
```

### `Engine/Private/Renderer/FSceneProxy.cpp`

```cpp
#include "Renderer/FSceneProxy.h"
#include "Actor/WMeshComponent.h"
#include "Renderer/ModelRenderer.h"
#include "Renderer/CMaterialInstance.h"

void FSceneProxy::CollectDrawCommands()
{
    DrawCommands.clear();

    if (!pOwnerComponent) return;

    auto* pRenderer = pOwnerComponent->GetModelRenderer();
    if (!pRenderer) return;

    matWorld = pOwnerComponent->GetWorldMatrix();
    bVisible = pOwnerComponent->IsVisible();

    if (!bVisible) return;

    // 현재 ModelRenderer 는 내부에서 직접 DrawIndexed 를 호출하므로,
    // 과도기에는 DrawCommand 1개 = "이 ModelRenderer 를 Render() 호출" 로 래핑.
    // 향후: ModelRenderer 내부의 서브메시별 VB/IB/Material 을 직접 추출하여
    //       메시당 1개의 CMeshDrawCommand 생성.

    CMeshDrawCommand cmd{};
    cmd.matWorld = matWorld;
    cmd.bSkinned = pRenderer->HasSkeleton();
    // 나머지 필드는 ModelRenderer::RenderNormalPass 에서 내부적으로 설정
    // 향후: pRenderer->GetSubMeshData(i) 로 직접 추출

    DrawCommands.push_back(cmd);
}
```

### `Engine/Public/Renderer/CRenderGraph.h`

```cpp
#pragma once

#include "WintersAPI.h"
#include "WintersTypes.h"
#include "Renderer/CMeshDrawCommand.h"
#include "Renderer/FSceneProxy.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

struct ID3D11DeviceContext;
class DX11Shader;
class DX11Pipeline;

/// 렌더 패스 콜백 시그니처
/// pContext: D3D11 컨텍스트
/// drawCommands: 이 패스에서 실행할 드로우 커맨드 (정렬 완료)
using RenderPassFn = std::function<void(
    ID3D11DeviceContext* pContext,
    const std::vector<CMeshDrawCommand>& drawCommands)>;

/// 렌더 패스 기술자
struct RenderPassDesc
{
    std::string                 name;
    RenderPassFn                executeFn;
    std::vector<std::string>    dependencies;   // 이 패스가 의존하는 패스 이름
    bool                        bEnabled = true;
};

/// UE5 FRDGBuilder 대응 (간이 버전)
/// 렌더 패스를 의존성 순서로 정렬하여 실행.
/// 현재 DX11 단일 스레드이므로 barrier 는 불필요, 실행 순서만 보장.
///
/// 사용법:
///   CRenderGraph graph;
///   graph.AddPass("DepthPrePass", {}, [](ctx, cmds){ ... });
///   graph.AddPass("NormalPass", {"DepthPrePass"}, [](ctx, cmds){ ... });
///   graph.AddPass("SSAOPass", {"NormalPass", "DepthPrePass"}, [](ctx, cmds){ ... });
///   graph.AddPass("ForwardPBR", {"SSAOPass"}, [](ctx, cmds){ ... });
///   graph.Compile();
///   graph.Execute(pContext, sceneProxies);
class WINTERS_API CRenderGraph
{
public:
    CRenderGraph();
    ~CRenderGraph();

    /// 팩토리
    static std::unique_ptr<CRenderGraph> Create();

    /// 렌더 패스 추가
    void AddPass(const std::string& name,
                 const std::vector<std::string>& dependencies,
                 RenderPassFn executeFn);

    /// 패스 활성/비활성
    void SetPassEnabled(const std::string& name, bool bEnabled);

    /// 의존성 기반 실행 순서 컴파일 (위상 정렬)
    bool Compile();

    /// 컴파일된 순서로 모든 패스 실행
    /// sceneProxies 에서 DrawCommand 수집 → 패스별 분배 → 정렬 → 실행
    void Execute(ID3D11DeviceContext* pContext,
                 const std::vector<FSceneProxy>& sceneProxies);

    /// 패스 수
    u32_t GetPassCount() const { return static_cast<u32_t>(m_Passes.size()); }

    /// 컴파일 완료 여부
    bool IsCompiled() const { return m_bCompiled; }

    /// 패스 실행 순서 (디버그용)
    const std::vector<std::string>& GetExecutionOrder() const { return m_ExecutionOrder; }

private:
    std::vector<RenderPassDesc> m_Passes;
    std::unordered_map<std::string, u32_t> m_PassNameToIndex;
    std::vector<std::string> m_ExecutionOrder;
    bool m_bCompiled = false;
};
```

### `Engine/Private/Renderer/CRenderGraph.cpp`

```cpp
#include "Renderer/CRenderGraph.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

#ifdef _DEBUG
#include <crtdbg.h>
#define RG_LOG(fmt, ...) do {                                    \
    char _buf[512];                                              \
    snprintf(_buf, sizeof(_buf), "[RenderGraph] " fmt "\n",      \
             ##__VA_ARGS__);                                     \
    OutputDebugStringA(_buf);                                    \
} while(0)
#else
#define RG_LOG(fmt, ...) ((void)0)
#endif

CRenderGraph::CRenderGraph()
{
}

CRenderGraph::~CRenderGraph()
{
}

std::unique_ptr<CRenderGraph> CRenderGraph::Create()
{
    return std::make_unique<CRenderGraph>();
}

void CRenderGraph::AddPass(const std::string& name,
                            const std::vector<std::string>& dependencies,
                            RenderPassFn executeFn)
{
    RenderPassDesc desc;
    desc.name = name;
    desc.executeFn = std::move(executeFn);
    desc.dependencies = dependencies;

    m_PassNameToIndex[name] = static_cast<u32_t>(m_Passes.size());
    m_Passes.push_back(std::move(desc));
    m_bCompiled = false;
}

void CRenderGraph::SetPassEnabled(const std::string& name, bool bEnabled)
{
    auto it = m_PassNameToIndex.find(name);
    if (it != m_PassNameToIndex.end())
        m_Passes[it->second].bEnabled = bEnabled;
}

bool CRenderGraph::Compile()
{
    m_ExecutionOrder.clear();

    // Kahn's algorithm for topological sort
    u32_t passCount = static_cast<u32_t>(m_Passes.size());

    std::unordered_map<std::string, u32_t> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> graph;

    for (auto& pass : m_Passes)
    {
        if (inDegree.find(pass.name) == inDegree.end())
            inDegree[pass.name] = 0;

        for (auto& dep : pass.dependencies)
        {
            graph[dep].push_back(pass.name);
            inDegree[pass.name]++;
            if (inDegree.find(dep) == inDegree.end())
                inDegree[dep] = 0;
        }
    }

    std::queue<std::string> q;
    for (auto& [name, deg] : inDegree)
    {
        if (deg == 0)
            q.push(name);
    }

    while (!q.empty())
    {
        auto cur = q.front();
        q.pop();

        // 등록된 패스인 경우만 실행 순서에 추가
        if (m_PassNameToIndex.count(cur) > 0)
            m_ExecutionOrder.push_back(cur);

        for (auto& next : graph[cur])
        {
            if (--inDegree[next] == 0)
                q.push(next);
        }
    }

    // 순환 의존성 검증
    if (m_ExecutionOrder.size() < passCount)
    {
        RG_LOG("ERROR: Circular dependency detected! Compiled %u / %u passes",
               static_cast<u32_t>(m_ExecutionOrder.size()), passCount);
        m_bCompiled = false;
        return false;
    }

    m_bCompiled = true;
    RG_LOG("Compiled %u passes", passCount);
    for (auto& name : m_ExecutionOrder)
        RG_LOG("  Pass: %s", name.c_str());

    return true;
}

void CRenderGraph::Execute(ID3D11DeviceContext* pContext,
                            const std::vector<FSceneProxy>& sceneProxies)
{
    if (!m_bCompiled) return;
    if (!pContext) return;

    // 1. 모든 SceneProxy 에서 DrawCommand 수집
    std::vector<CMeshDrawCommand> allCommands;
    for (auto& proxy : sceneProxies)
    {
        if (!proxy.IsVisible()) continue;
        for (auto& cmd : proxy.DrawCommands)
            allCommands.push_back(cmd);
    }

    // 2. 정렬 (셰이더 → 머티리얼 → 깊이)
    std::sort(allCommands.begin(), allCommands.end(),
        [](const CMeshDrawCommand& a, const CMeshDrawCommand& b)
        {
            return a.GetSortKey() < b.GetSortKey();
        });

    // 3. 패스별 실행
    for (auto& passName : m_ExecutionOrder)
    {
        auto it = m_PassNameToIndex.find(passName);
        if (it == m_PassNameToIndex.end()) continue;

        auto& pass = m_Passes[it->second];
        if (!pass.bEnabled) continue;

        if (pass.executeFn)
            pass.executeFn(pContext, allCommands);
    }
}
```

---

## 4. 사용 예시

### 4.1 Before: 수동 렌더 호출

```cpp
// Scene_InGame::OnRender()
void CScene_InGame::OnRender()
{
    // 수동 순서 하드코딩
    if (m_pNormalPass)
    {
        m_pNormalPass->Begin();
        m_Irelia.RenderNormalPass(pShader, pPipeline, pSkinShader, pSkinPipeline);
        m_Yasuo.RenderNormalPass(pShader, pPipeline, pSkinShader, pSkinPipeline);
        // ...7 champions
        m_pNormalPass->End();
    }

    if (m_pSSAOPass)
        m_pSSAOPass->Execute(m_pNormalPass->GetNormalSRV(), m_pNormalPass->GetDepthSRV());

    // Forward 패스
    m_Map.Render();
    m_Irelia.Render();
    m_Yasuo.Render();
    m_Sylas.Render();
    m_Viego.Render();
    m_Kalista.Render();
    m_Garen.Render();
    m_Zed.Render();

    // FX
    m_pFxMeshSystem->Render(/*...*/);
    m_pFxSystem->Render(/*...*/);

    // UI
    CGameInstance::Get()->UI_Render_Overlay(matVP);
}
```

### 4.2 After: RenderGraph 기반

```cpp
// WRenderSubsystem::Initialize()
void WRenderSubsystem::Initialize(WWorld* pWorld)
{
    WWorldSubsystem::Initialize(pWorld);

    m_pRenderGraph = CRenderGraph::Create();

    // 패스 등록 (의존성 자동 정렬)
    m_pRenderGraph->AddPass("DepthPrePass", {},
        [this](auto* ctx, auto& cmds) { ExecuteDepthPrePass(ctx, cmds); });

    m_pRenderGraph->AddPass("NormalPass", {"DepthPrePass"},
        [this](auto* ctx, auto& cmds) { ExecuteNormalPass(ctx, cmds); });

    m_pRenderGraph->AddPass("SSAOPass", {"NormalPass", "DepthPrePass"},
        [this](auto* ctx, auto& cmds) { ExecuteSSAO(ctx, cmds); });

    m_pRenderGraph->AddPass("ForwardPBR", {"SSAOPass"},
        [this](auto* ctx, auto& cmds) { ExecuteForwardPBR(ctx, cmds); });

    m_pRenderGraph->AddPass("FX", {"ForwardPBR"},
        [this](auto* ctx, auto& cmds) { ExecuteFX(ctx, cmds); });

    m_pRenderGraph->AddPass("UI", {"FX"},
        [this](auto* ctx, auto& cmds) { ExecuteUI(ctx, cmds); });

    m_pRenderGraph->Compile();
}

// WWorld::Render() 내부
void WRenderSubsystem::Render()
{
    // 1. SceneProxy 수집 (모든 WMeshComponent 에서)
    std::vector<FSceneProxy> proxies;
    GetWorld()->ForEachActor([&](WActor* actor)
    {
        for (auto& comp : actor->GetComponents())
        {
            auto* mesh = dynamic_cast<WMeshComponent*>(comp.get());
            if (!mesh || !mesh->IsVisible()) continue;

            FSceneProxy proxy;
            proxy.pOwnerComponent = mesh;
            proxy.CollectDrawCommands();
            if (proxy.IsValid())
                proxies.push_back(std::move(proxy));
        }
    });

    // 2. RenderGraph 실행
    auto* pContext = /* CDX11Device::Get()->GetContext() */;
    m_pRenderGraph->Execute(pContext, proxies);
}
```

### 4.3 MaterialInstance 실시간 튜닝

```cpp
// 이렐리아 머티리얼 생성
auto pMat = CMaterialInstance::Create(pDevice, pPBRShader, pPBRPipeline);
pMat->m_vAlbedoColor = {0.9f, 0.85f, 0.8f, 1.f};
pMat->m_fMetallic = 0.7f;      // 금속 갑옷
pMat->m_fRoughness = 0.3f;     // 광택
pMat->m_fAO = 1.f;
pMat->SetAlbedoTexture(pIreliaAlbedoSRV);

// ImGui 실시간 튜닝 (WPROPERTY 기반 자동 노출)
// 에디터가 CMaterialInstance 의 프로퍼티를 자동 슬라이더로 렌더
// metallic 0→1 드래그 → m_bDirty = true → 다음 프레임 GPU 업로드
```

---

## 5. Verification Checklist

```
[ ] CMeshDrawCommand::Execute → DrawIndexed 호출
[ ] CMeshDrawCommand::GetSortKey → 셰이더/머티리얼/깊이 순서 정렬
[ ] CMaterialInstance::Create → cbuffer b3 생성 성공
[ ] CMaterialInstance::UploadToGPU → dirty 시에만 UpdateSubresource
[ ] CMaterialInstance::BindToContext → VS/PS cbuffer b3 + 텍스처 t0~t3 바인딩
[ ] CBPerMaterial sizeof == 48 (static_assert)
[ ] FSceneProxy::CollectDrawCommands → WMeshComponent 에서 DrawCommand 수집
[ ] CRenderGraph::AddPass → 의존성 등록
[ ] CRenderGraph::Compile → 위상 정렬 성공 (순환 = false 반환)
[ ] CRenderGraph::Execute → 의존성 순서대로 패스 실행
[ ] CRenderGraph::SetPassEnabled("SSAOPass", false) → SSAO 패스 스킵
[ ] 정렬 후 같은 셰이더/머티리얼의 DrawCommand 가 연속 배치
[ ] 기존 ModelRenderer::Render 와 병존 (과도기)
[ ] LoL 빌드 통과 (신규 파일 추가만, 기존 코드 무변경)
```

---

## 6. Migration Strategy

### Phase 1: 인프라 추가 (기존 코드 무변경)
- CRenderGraph, CMeshDrawCommand, CMaterialInstance, FSceneProxy 엔진에 추가
- 빌드 통과 확인

### Phase 2: RenderGraph 병존
- Scene_InGame 에 CRenderGraph 인스턴스 추가
- 기존 수동 렌더 순서를 RenderGraph 패스로 래핑 (동작 동일, 구조만 변경)
- 검증: 렌더 결과 동일

### Phase 3: CMaterialInstance 적용
- Track 1 (PBR) 에서 생성한 Mesh3D_PBR.hlsl 과 연결
- cbuffer b3 PerMaterial 자동 업로드
- 이렐리아 1체 PBR 머티리얼 적용 + ImGui 슬라이더 (metallic/roughness)

### Phase 4: FSceneProxy + MeshDrawCommand
- WMeshComponent::CreateSceneProxy 구현
- ModelRenderer 내부 서브메시 데이터를 CMeshDrawCommand 로 직접 추출
- 수동 Render() 나열 → RenderGraph 자동 수집으로 교체

### Phase 5: 셰이더 상태 정렬 최적화
- DrawCommand 정렬로 셰이더/파이프라인 전환 최소화
- 셰이더 전환 횟수 프로파일러 로깅
