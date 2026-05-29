# Winters Engine — Render Graph 구현 계획서

## Context

Winters Engine의 구현 로드맵: **Fiber Job System → Render Graph → GPU Driven Pipeline**.
현재 Phase 0-1 (DX11 기본 렌더링 동작)이며, Render Graph는 Phase 2 렌더 엔진의 핵심 기반.
기존의 명령형(imperative) 렌더링 (`BeginFrame → OnRender → EndFrame`)을 선언형(declarative) DAG 기반으로 전환하여,
향후 Deferred Rendering, PostFX, GPU-Driven Pipeline의 토대를 만든다.

---

## 1. Render Graph 핵심 원리

### 1-1. 개념

Render Graph (= Frame Graph)는 **DAG(Directed Acyclic Graph)**로 프레임 렌더링을 표현하는 시스템.

- **Node** = 렌더 패스 (G-Buffer, Lighting, PostFX 등)
- **Edge** = 리소스 의존성 (패스 A가 텍스처 T에 Write → 패스 B가 텍스처 T를 Read)

매 프레임 3단계:
```
[Setup]  패스 등록 + 리소스 Read/Write 선언
    ↓
[Compile] 토폴로지 정렬 + 데드 패스 컬링 + 리소스 수명 분석 + 풀 할당
    ↓
[Execute] 정렬 순서대로 패스 실행 (DX11 호출)
```

### 1-2. DX11에서 Render Graph가 주는 이점

DX11은 DX12/Vulkan과 달리 명시적 배리어가 없지만:
1. **RTV/SRV/UAV 뷰 충돌 자동 관리** — 패스 A에서 RTV로 쓴 텍스처를 패스 B에서 SRV로 읽을 때, 그래프가 자동으로 RTV 언바인드 후 SRV 바인딩
2. **Transient 리소스 풀링** — 프레임 내 임시 RT를 풀에서 재활용 (메모리 절약)
3. **패스 컬링** — 출력을 아무도 읽지 않는 패스 자동 제거
4. **아키텍처 분리** — "이 패스는 X를 읽고 Y에 쓴다"만 선언, 실행 순서는 그래프가 결정
5. **DX12 이식 준비** — 의존성 정보가 이미 있으므로 배리어 삽입만 추가하면 됨

### 1-3. 참조 아키텍처

Frostbite FrameGraph (GDC 2017) 기반, DX11 단순화 버전.

---

## 2. 핵심 기술 스택

| 기술 | 용도 |
|------|------|
| **DAG + Kahn's Algorithm** | 토폴로지 정렬 (실행 순서 결정) |
| **Reference Counting** | 데드 패스 컬링 (역방향 전파) |
| **Resource Pool** | D3D11 텍스처/뷰 재활용 (desc 매칭) |
| **RgHandle (uint32)** | 리소스 버전 추적 (상위16=리소스ID, 하위16=버전) |
| **std::function Lambda** | 패스 실행 콜백 (Setup/Execute 분리) |
| **TYPELESS Format** | Depth SRV 겸용 (R24G8_TYPELESS → DSV+SRV) |

---

## 3. 추가할 파일 목록

### 신규 생성 (7파일)

| # | 파일 경로 | 용도 |
|---|-----------|------|
| 1 | `Engine/Header/Renderer/RenderGraph/RgTypes.h` | RgHandle, ERgResourceUsage, ERgPassType 정의 |
| 2 | `Engine/Header/Renderer/RenderGraph/RgResource.h` | RgTextureDesc, RgBufferDesc, RgResourceEntry |
| 3 | `Engine/Header/Renderer/RenderGraph/RgPassNode.h` | RgPassNode, RgPassContext, RgPassResource |
| 4 | `Engine/Header/Renderer/RenderGraph/RgResourcePool.h` | CRgResourcePool (DX11 텍스처 풀) |
| 5 | `Engine/Header/Renderer/RenderGraph/CRenderGraph.h` | CRenderGraph + CRgPassBuilder (메인 클래스) |
| 6 | `Engine/Code/Renderer/RenderGraph/RgResourcePool.cpp` | 풀 구현 |
| 7 | `Engine/Code/Renderer/RenderGraph/CRenderGraph.cpp` | 그래프 Setup/Compile/Execute 구현 |

### 수정할 기존 파일 (4파일)

| # | 파일 경로 | 변경 내용 |
|---|-----------|-----------|
| 8 | `Engine/Header/RHI/CDX11Device.h` L56 | `GetWidth()` / `GetHeight()` 접근자 추가 |
| 9 | `Engine/Header/Framework/CEngineApp.h` L5,40,52 | `#include`, `GetRenderGraph()`, `m_RenderGraph` 멤버 추가 |
| 10 | `Engine/Code/Framework/CEngineApp.cpp` L52,96-105 | Initialize에서 그래프 초기화, Render()를 그래프 기반으로 전환 |
| 11 | `Client/Code/CGameApp.cpp` OnRender() | 기존 직접 렌더링 → graph.AddPass() 방식으로 전환 |

---

## 4. 상세 클래스 설계

### 4-1. `Engine/Header/Renderer/RenderGraph/RgTypes.h` — 기본 타입

```cpp
#pragma once
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  RgTypes.h  |  Render Graph 기본 타입 정의
// ─────────────────────────────────────────────────────────────────

struct RgHandle
{
    uint32 id = UINT32_MAX;

    bool   IsValid()          const { return id != UINT32_MAX; }
    uint16 GetResourceIndex() const { return static_cast<uint16>(id >> 16); }
    uint16 GetVersion()       const { return static_cast<uint16>(id & 0xFFFF); }

    static RgHandle Create(uint16 resIdx, uint16 ver)
    {
        return { (static_cast<uint32>(resIdx) << 16) | ver };
    }

    bool operator==(const RgHandle& o) const { return id == o.id; }
    bool operator!=(const RgHandle& o) const { return id != o.id; }
};

static constexpr RgHandle RG_HANDLE_INVALID = { UINT32_MAX };

enum class ERgResourceUsage : uint8
{
    RenderTarget,     // OMSetRenderTargets (write)
    DepthStencil,     // OMSetRenderTargets depth (write)
    ShaderResource,   // SRV (read)
    UnorderedAccess,  // UAV (read/write, CS)
};

enum class ERgPassType : uint8
{
    Graphics,   // VS/PS draw calls
    Compute,    // CS dispatch
    Copy,       // CopyResource
};
```

### 4-2. `Engine/Header/Renderer/RenderGraph/RgResource.h` — 리소스 디스크립터

```cpp
#pragma once
#include "RgTypes.h"
#include <d3d11.h>
#include <string>

// ─────────────────────────────────────────────────────────────────
//  RgResource.h  |  리소스 디스크립터 + 그래프 내 리소스 엔트리
// ─────────────────────────────────────────────────────────────────

struct RgTextureDesc
{
    uint32      width     = 0;    // 0 = 백버퍼 크기
    uint32      height    = 0;    // 0 = 백버퍼 크기
    DXGI_FORMAT format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32      mipLevels = 1;
    uint32      arraySize = 1;
    uint32      bindFlags = 0;    // 그래프가 usage 분석 후 자동 설정
    string      debugName;

    bool operator==(const RgTextureDesc& o) const
    {
        return width == o.width && height == o.height
            && format == o.format && mipLevels == o.mipLevels
            && arraySize == o.arraySize;
    }
};

struct RgBufferDesc
{
    uint32  byteWidth       = 0;
    uint32  structureStride = 0;
    uint32  bindFlags       = 0;
    string  debugName;
};

struct RgResourceEntry
{
    uint16      resourceIndex   = 0;
    uint16      currentVersion  = 0;
    bool        bImported       = false;   // 외부 리소스 (백버퍼 등)
    bool        bTransient      = true;    // 프레임 내 임시

    RgTextureDesc textureDesc;

    // 물리 리소스 (Compile 시 할당)
    ID3D11Texture2D*            pTexture = nullptr;
    ID3D11RenderTargetView*     pRTV     = nullptr;
    ID3D11DepthStencilView*     pDSV     = nullptr;
    ID3D11ShaderResourceView*   pSRV     = nullptr;
    ID3D11UnorderedAccessView*  pUAV     = nullptr;

    // 수명 추적
    uint32  firstPassIndex = UINT32_MAX;
    uint32  lastPassIndex  = 0;
};
```

### 4-3. `Engine/Header/Renderer/RenderGraph/RgPassNode.h` — 패스 노드

```cpp
#pragma once
#include "RgTypes.h"
#include <vector>
#include <functional>
#include <string>

class CRenderGraph;

// ─────────────────────────────────────────────────────────────────
//  RgPassNode.h  |  그래프 내 단일 렌더 패스
// ─────────────────────────────────────────────────────────────────

struct RgPassResource
{
    RgHandle         handle;
    ERgResourceUsage usage;
};

// 패스 실행 시 전달되는 컨텍스트
struct RgPassContext
{
    ID3D11DeviceContext* pContext = nullptr;
    ID3D11Device*        pDevice = nullptr;
    CRenderGraph*        pGraph  = nullptr;

    ID3D11ShaderResourceView*   GetSRV(RgHandle handle) const;
    ID3D11RenderTargetView*     GetRTV(RgHandle handle) const;
    ID3D11DepthStencilView*     GetDSV(RgHandle handle) const;
    ID3D11UnorderedAccessView*  GetUAV(RgHandle handle) const;
    ID3D11Texture2D*            GetTexture(RgHandle handle) const;
};

using RgPassExecuteFn = std::function<void(const RgPassContext& ctx)>;

struct RgPassNode
{
    string                   name;
    ERgPassType              type       = ERgPassType::Graphics;
    uint32                   passIndex  = 0;

    vector<RgPassResource>   reads;
    vector<RgPassResource>   writes;

    RgPassExecuteFn          executeFn;

    bool                     bCulled    = false;
    int32                    refCount   = 0;
};
```

### 4-4. `Engine/Header/Renderer/RenderGraph/RgResourcePool.h` — 텍스처 풀

```cpp
#pragma once
#include "RgResource.h"
#include <vector>

// ─────────────────────────────────────────────────────────────────
//  CRgResourcePool  |  DX11 텍스처 풀 (transient 리소스 재활용)
//
//  동일 desc+bindFlags인 미사용 텍스처를 재활용.
//  N프레임 미사용 시 자동 해제 (GarbageCollect).
// ─────────────────────────────────────────────────────────────────

class CRgResourcePool
{
public:
    CRgResourcePool() = default;
    ~CRgResourcePool();

    void Initialize(ID3D11Device* pDevice);
    void Shutdown();

    bool AcquireTexture(const RgTextureDesc& desc, uint32 bindFlags,
                        RgResourceEntry& outEntry);
    void ReleaseTexture(RgResourceEntry& entry);
    void GarbageCollect(uint32 currentFrame, uint32 maxIdleFrames = 4);

private:
    struct PooledTexture
    {
        RgTextureDesc               desc;
        uint32                      bindFlags     = 0;
        ID3D11Texture2D*            pTexture      = nullptr;
        ID3D11RenderTargetView*     pRTV          = nullptr;
        ID3D11DepthStencilView*     pDSV          = nullptr;
        ID3D11ShaderResourceView*   pSRV          = nullptr;
        ID3D11UnorderedAccessView*  pUAV          = nullptr;
        uint32                      lastUsedFrame = 0;
        bool                        bInUse        = false;
    };

    bool CreateTextureViews(const RgTextureDesc& desc, uint32 bindFlags,
                            PooledTexture& out);

    ID3D11Device*         m_pDevice = nullptr;
    vector<PooledTexture> m_vecPool;
    uint32                m_iCurrentFrame = 0;
};
```

### 4-5. `Engine/Header/Renderer/RenderGraph/CRenderGraph.h` — 메인 클래스

```cpp
#pragma once
#include "RgTypes.h"
#include "RgResource.h"
#include "RgPassNode.h"
#include "RgResourcePool.h"
#include <vector>
#include <string>
#include <functional>

class CDX11Device;

// ─────────────────────────────────────────────────────────────────
//  CRenderGraph  |  프레임 그래프
//
//  매 프레임: BeginFrame → AddPass x N → Compile → Execute
// ─────────────────────────────────────────────────────────────────

class CRgPassBuilder
{
public:
    CRgPassBuilder(CRenderGraph* pGraph, RgPassNode* pPass);

    RgHandle Read(RgHandle input,
                  ERgResourceUsage usage = ERgResourceUsage::ShaderResource);

    RgHandle Write(RgHandle output,
                   ERgResourceUsage usage = ERgResourceUsage::RenderTarget);

    RgHandle ReadWrite(RgHandle resource,
                       ERgResourceUsage usage = ERgResourceUsage::UnorderedAccess);

    void SetSideEffect();

private:
    CRenderGraph* m_pGraph;
    RgPassNode*   m_pPass;
};

class CRenderGraph
{
public:
    CRenderGraph()  = default;
    ~CRenderGraph() { Shutdown(); }

    void Initialize(CDX11Device* pDevice);
    void Shutdown();

    // ── 프레임 사이클 ───────────────────────────────────────
    void BeginFrame();

    template<typename SetupFn, typename ExecuteFn>
    void AddPass(const char* name, ERgPassType type,
                 SetupFn&& setupFn, ExecuteFn&& executeFn);

    void Compile();
    void Execute();

    // ── 리소스 관리 ─────────────────────────────────────────
    RgHandle CreateTexture(const RgTextureDesc& desc);
    RgHandle ImportBackBuffer();
    RgHandle ImportTexture(const char* debugName,
                           ID3D11Texture2D* pTex,
                           ID3D11RenderTargetView* pRTV,
                           ID3D11DepthStencilView* pDSV,
                           ID3D11ShaderResourceView* pSRV);

    RgHandle GetBackBufferHandle() const { return m_hBackBuffer; }
    const RgResourceEntry& GetResourceEntry(RgHandle handle) const;
    uint32 GetBackBufferWidth()  const;
    uint32 GetBackBufferHeight() const;

private:
    friend class CRgPassBuilder;

    RgHandle CreateNewVersion(uint16 resourceIndex);

    // Compile 내부
    void ComputeResourceLifetimes();
    void CullPasses();
    void TopologicalSort();
    void AllocateResources();

    // Execute 내부
    void UnbindConflictingViews(const RgPassNode& pass);
    void BindPassResources(const RgPassNode& pass);

    CDX11Device*             m_pDevice = nullptr;
    CRgResourcePool          m_ResourcePool;

    vector<RgPassNode>       m_vecPasses;
    vector<RgResourceEntry>  m_vecResources;
    vector<uint32>           m_vecSortedPasses;

    RgHandle                 m_hBackBuffer = RG_HANDLE_INVALID;
    uint32                   m_iFrameIndex = 0;
    bool                     m_bCompiled   = false;
};

// ── AddPass 템플릿 구현 ─────────────────────────────────────
template<typename SetupFn, typename ExecuteFn>
void CRenderGraph::AddPass(const char* name, ERgPassType type,
                           SetupFn&& setupFn, ExecuteFn&& executeFn)
{
    RgPassNode pass;
    pass.name      = name;
    pass.type      = type;
    pass.passIndex = static_cast<uint32>(m_vecPasses.size());
    pass.executeFn = std::forward<ExecuteFn>(executeFn);

    CRgPassBuilder builder(this, &pass);
    setupFn(builder);

    m_vecPasses.push_back(std::move(pass));
}
```

---

## 5. 핵심 알고리즘 상세

### 5-1. Compile — ComputeResourceLifetimes()

```
각 패스의 reads/writes를 순회:
  - 해당 리소스의 firstPassIndex = min(현재, passIndex)
  - 해당 리소스의 lastPassIndex  = max(현재, passIndex)

각 리소스의 usage를 분석하여 bindFlags 자동 계산:
  - RenderTarget usage  → D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE
  - DepthStencil usage  → D3D11_BIND_DEPTH_STENCIL
  - ShaderResource usage → D3D11_BIND_SHADER_RESOURCE
  - UnorderedAccess usage → D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE
```

### 5-2. Compile — CullPasses()

```
1. 초기 refCount = 각 패스의 출력을 읽는 다른 패스 수
2. SetSideEffect() 패스 → refCount += 1
3. 백버퍼에 Write하는 패스 → refCount += 1
4. 역순 순회: refCount == 0인 패스 → bCulled = true
   → 해당 패스가 읽던 리소스를 쓰는 패스의 refCount--
5. 변화 없을 때까지 반복
```

### 5-3. Compile — TopologicalSort()

```
Kahn's Algorithm:
1. 인접 리스트 구성: 패스 A가 리소스 R을 Write → 패스 B가 R을 Read → edge(A→B)
2. 각 패스의 in-degree 계산
3. in-degree == 0인 패스를 큐에 삽입
4. 큐에서 꺼내 m_vecSortedPasses에 추가, 후속 패스의 in-degree--
5. 새로 0이 된 패스 큐에 삽입, 반복
```

### 5-4. Execute 흐름

```
for passIndex in m_vecSortedPasses:
    pass = m_vecPasses[passIndex]
    if pass.bCulled → skip

    1) UnbindConflictingViews(pass)
       - SRV로 읽을 리소스가 현재 RTV로 바인딩 → null RTV set
       - RTV로 쓸 리소스가 현재 SRV로 바인딩 → null SRV set

    2) BindPassResources(pass)
       - Graphics: OMSetRenderTargets(모든 RTV writes + DSV write)
       - 뷰포트 설정 (RT 크기 기준)
       - SRV reads를 적절한 셰이더 스테이지에 바인딩

    3) pass.executeFn(ctx) — 사용자 콜백 실행

수명 만료된 transient 리소스 → 풀로 반환
```

---

## 6. 기존 코드 수정 사항

### 6-1. `Engine/Header/RHI/CDX11Device.h` L56에 추가

```cpp
// 수정 전 (L52-56):
    ID3D11Device*           GetDevice()    const { return m_pDevice;          }
    ID3D11DeviceContext*    GetContext()   const { return m_pContext;         }
    ID3D11RenderTargetView* GetBackRTV()   const { return m_pRenderTargetView;}
    ID3D11DepthStencilView* GetDSV()       const { return m_pDepthStencilView;}

// 수정 후 (L52-58):
    ID3D11Device*           GetDevice()    const { return m_pDevice;          }
    ID3D11DeviceContext*    GetContext()   const { return m_pContext;         }
    ID3D11RenderTargetView* GetBackRTV()   const { return m_pRenderTargetView;}
    ID3D11DepthStencilView* GetDSV()       const { return m_pDepthStencilView;}
    uint32                  GetWidth()     const { return m_Width;            }
    uint32                  GetHeight()    const { return m_Height;           }
```

### 6-2. `Engine/Header/Framework/CEngineApp.h`

```cpp
// L5 추가:
#include "Renderer/RenderGraph/CRenderGraph.h"

// L40 추가 (GetWindow() 다음):
    CRenderGraph&        GetRenderGraph() { return m_RenderGraph; }

// L52 추가 (m_Timer 다음):
    CRenderGraph    m_RenderGraph;
```

### 6-3. `Engine/Code/Framework/CEngineApp.cpp` — Initialize() L52

```cpp
// 수정 전:
    m_Timer.Reset();

// 수정 후:
    m_Timer.Reset();
    m_RenderGraph.Initialize(&m_Device);
```

### 6-4. `Engine/Code/Framework/CEngineApp.cpp` — Render() L96-105

```cpp
// 수정 전:
void CEngineApp::Render()
{
    m_Device.BeginFrame(0.08f, 0.08f, 0.12f, 1.f);
    if (m_pGameApp)
        m_pGameApp->OnRender();
    m_Device.EndFrame();
}

// 수정 후:
void CEngineApp::Render()
{
    m_RenderGraph.BeginFrame();      // 그래프 초기화 + 백버퍼 Import + ClearPass 등록

    if (m_pGameApp)
        m_pGameApp->OnRender();      // 게임이 AddPass()로 패스 등록

    m_RenderGraph.Compile();         // DAG 분석 + 컬링 + 리소스 할당
    m_RenderGraph.Execute();         // 정렬 순서대로 패스 실행

    m_Device.EndFrame();             // Present
}
```

### 6-5. `Client/Code/CGameApp.cpp` — OnRender() 전환

```cpp
// 수정 전:
void CGameApp::OnRender()
{
    Mat4 vp = m_Camera.GetViewProjection();
    m_Cube.UpdateCamera(vp);
    m_Cube.UpdateTransform(m_CubeTransform.GetWorldMatrix());
    m_Cube.Render();
}

// 수정 후:
void CGameApp::OnRender()
{
    CRenderGraph& graph = CEngineApp::Get().GetRenderGraph();
    RgHandle backBuffer = graph.GetBackBufferHandle();

    Mat4 vp    = m_Camera.GetViewProjection();
    Mat4 world = m_CubeTransform.GetWorldMatrix();

    graph.AddPass("ForwardOpaque", ERgPassType::Graphics,
        [backBuffer](CRgPassBuilder& builder)
        {
            builder.Write(backBuffer, ERgResourceUsage::RenderTarget);
            builder.SetSideEffect();
        },
        [this, vp, world](const RgPassContext& ctx)
        {
            m_Cube.UpdateCamera(vp);
            m_Cube.UpdateTransform(world);
            m_Cube.Render();
        }
    );
}
```

---

## 7. BeginFrame 내부 — 백버퍼 클리어 패스 자동 등록

```cpp
void CRenderGraph::BeginFrame()
{
    m_vecPasses.clear();
    m_vecResources.clear();
    m_vecSortedPasses.clear();
    m_bCompiled = false;
    m_iFrameIndex++;

    // 백버퍼 + 뎁스버퍼를 그래프에 Import
    m_hBackBuffer = ImportBackBuffer();
    RgHandle depth = ImportTexture("DepthBuffer",
        nullptr, nullptr, m_pDevice->GetDSV(), nullptr);

    // 암시적 클리어 패스 (항상 실행)
    AddPass("ClearBackBuffer", ERgPassType::Graphics,
        [bb = m_hBackBuffer, depth](CRgPassBuilder& builder)
        {
            builder.Write(bb, ERgResourceUsage::RenderTarget);
            builder.Write(depth, ERgResourceUsage::DepthStencil);
            builder.SetSideEffect();
        },
        [this](const RgPassContext& ctx)
        {
            float clearColor[4] = { 0.08f, 0.08f, 0.12f, 1.f };
            ctx.pContext->ClearRenderTargetView(m_pDevice->GetBackRTV(), clearColor);
            ctx.pContext->ClearDepthStencilView(m_pDevice->GetDSV(),
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

            // 뷰포트 + RT 바인딩
            D3D11_VIEWPORT vp = {};
            vp.Width  = static_cast<float>(m_pDevice->GetWidth());
            vp.Height = static_cast<float>(m_pDevice->GetHeight());
            vp.MaxDepth = 1.f;
            ctx.pContext->RSSetViewports(1, &vp);
            ctx.pContext->OMSetRenderTargets(1,
                &m_pDevice->GetBackRTV(), m_pDevice->GetDSV());
        }
    );
}
```

---

## 8. Deferred Rendering 파이프라인 예시 (향후 Phase D)

그래프로 표현한 Deferred 파이프라인:

```
ClearBackBuffer → GBufferPass → DeferredLighting(CS) → ToneMapping → [Backbuffer]
```

```cpp
void CDeferredRenderer::SetupPasses(CRenderGraph& graph)
{
    uint32 w = graph.GetBackBufferWidth();
    uint32 h = graph.GetBackBufferHeight();

    // G-Buffer 리소스 선언
    RgHandle gbAlbedo  = graph.CreateTexture({ w, h, DXGI_FORMAT_R8G8B8A8_UNORM,
                                               1, 1, 0, "GBuffer_Albedo" });
    RgHandle gbNormal  = graph.CreateTexture({ w, h, DXGI_FORMAT_R16G16B16A16_FLOAT,
                                               1, 1, 0, "GBuffer_Normal" });
    RgHandle gbRoughAO = graph.CreateTexture({ w, h, DXGI_FORMAT_R8G8B8A8_UNORM,
                                               1, 1, 0, "GBuffer_RoughAO" });
    RgHandle depth     = graph.CreateTexture({ w, h, DXGI_FORMAT_R24G8_TYPELESS,
                                               1, 1, 0, "SceneDepth" });

    // Pass 1: G-Buffer
    graph.AddPass("GBufferPass", ERgPassType::Graphics,
        [&](CRgPassBuilder& b) {
            gbAlbedo  = b.Write(gbAlbedo,  ERgResourceUsage::RenderTarget);
            gbNormal  = b.Write(gbNormal,  ERgResourceUsage::RenderTarget);
            gbRoughAO = b.Write(gbRoughAO, ERgResourceUsage::RenderTarget);
            depth     = b.Write(depth,     ERgResourceUsage::DepthStencil);
        },
        [this](const RgPassContext& ctx) {
            // MRT 바인딩 + 모든 Opaque 메시 드로우
        }
    );

    // HDR 라이트 결과
    RgHandle hdrTarget = graph.CreateTexture({ w, h, DXGI_FORMAT_R16G16B16A16_FLOAT,
                                               1, 1, 0, "HDR_LightResult" });

    // Pass 2: Deferred Lighting (Compute Shader)
    graph.AddPass("DeferredLighting", ERgPassType::Compute,
        [&](CRgPassBuilder& b) {
            b.Read(gbAlbedo);  b.Read(gbNormal);  b.Read(gbRoughAO);  b.Read(depth);
            hdrTarget = b.Write(hdrTarget, ERgResourceUsage::UnorderedAccess);
        },
        [this](const RgPassContext& ctx) {
            // CS dispatch (16x16 그룹)
        }
    );

    // Pass 3: Tone Mapping → Backbuffer
    RgHandle bb = graph.GetBackBufferHandle();
    graph.AddPass("ToneMapping", ERgPassType::Graphics,
        [&](CRgPassBuilder& b) {
            b.Read(hdrTarget);
            bb = b.Write(bb, ERgResourceUsage::RenderTarget);
            b.SetSideEffect();
        },
        [this](const RgPassContext& ctx) {
            // 풀스크린 삼각형 + 톤맵 셰이더
        }
    );
}
```

그래프가 자동 처리:
- 실행 순서: Clear → GBuffer → Lighting → ToneMapping
- GBuffer RTV → Lighting SRV 전환 시 자동 언바인드
- HDR UAV → ToneMapping SRV 전환 시 자동 언바인드
- ToneMapping 제거 시 → Lighting/GBuffer 모두 자동 컬링
- transient 텍스처 풀에서 할당/반환

---

## 9. 구현 Phase 분할

### Phase A — 최소 Render Graph (먼저 구현)

**목표**: 기존 CubeRenderer와 동일한 동작을 그래프 기반으로 전환.

생성: `RgTypes.h`, `RgResource.h`, `RgPassNode.h`, `CRenderGraph.h`, `CRenderGraph.cpp`
수정: `CDX11Device.h`, `CEngineApp.h`, `CEngineApp.cpp`, `CGameApp.cpp`
스킵: RgResourcePool (imported 리소스만 사용, transient 없음)

**검증**: 빌드 성공 + 큐브 렌더링 동작 동일.

### Phase B — 리소스 풀 + Transient 텍스처

**목표**: 중간 RT를 풀에서 할당/반환.

생성: `RgResourcePool.h`, `RgResourcePool.cpp`
수정: `CRenderGraph.cpp` (AllocateResources에 풀 연동)

**검증**: 오프스크린 RT에 렌더 → 백버퍼에 컴포짓.

### Phase C — SRV/RTV 충돌 해소

**목표**: 뷰 전환 자동화.

수정: `CRenderGraph.cpp` (UnbindConflictingViews 구현)

**검증**: Depth DSV → 다음 패스에서 SRV로 읽기.

### Phase D — Deferred Rendering 기반

**목표**: G-Buffer + Lighting + ToneMapping.

생성: `CDeferredRenderer.h/cpp`, `GBuffer.hlsl`, `DeferredLighting.hlsl`, `ToneMapping.hlsl`

### Phase E — GPU-Driven 준비

**목표**: Compute 패스 + UAV + StructuredBuffer + Indirect Draw.

```
Phase A → Phase B → Phase C → Phase D → Phase E
  (각 Phase는 독립 검증 가능)
```

---

## 10. vcxproj 필터 배치

모든 Render Graph 파일은 `03. Renderer` 필터 하위에 배치:

```
03. Renderer
├── RenderGraph
│   ├── RgTypes.h
│   ├── RgResource.h
│   ├── RgPassNode.h
│   ├── RgResourcePool.h / .cpp
│   └── CRenderGraph.h / .cpp
├── CCamera.h / .cpp
├── TriangleRenderer.h / .cpp
└── CubeRenderer.h / .cpp
```

---

## 11. 잠재적 이슈 & 대응

| 이슈 | 대응 |
|------|------|
| DX11 Deferred Context 불안정 | Immediate Context에서만 Execute. JobSystem 연동은 Setup 병렬화만. |
| Depth SRV 겸용 포맷 | TYPELESS(R24G8_TYPELESS)로 생성, DSV(D24_UNORM_S8_UINT) / SRV(R24_UNORM_X8_TYPELESS) 별도 뷰 |
| MRT 바인딩 | 한 패스의 모든 RTV write를 수집하여 OMSetRenderTargets 1회 호출 |
| Lambda 캡처 | 값 복사 캡처 (`[this, vp, world]`), 참조 캡처 금지 |
| vcxproj /utf-8 | 신규 파일 추가 시 /utf-8 옵션 포함 확인 |

---

## 12. 검증 계획

1. **빌드 검증**: Engine.vcxproj에 신규 파일 추가 후 Debug/Release 빌드 성공
2. **기능 검증**: 큐브 렌더링이 그래프 기반으로 동일하게 동작
3. **OutputDebugString**: 각 패스 실행 시 `[RenderGraph] Execute: PassName` 로그 출력
4. **패스 컬링 테스트**: 읽히지 않는 패스를 추가하고 컬링되는지 확인
5. **Phase B 검증**: 오프스크린 RT 할당/반환 동작 확인
