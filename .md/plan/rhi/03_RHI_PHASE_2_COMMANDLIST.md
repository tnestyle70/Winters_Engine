# Phase RH-2 Sub-plan: CommandList Abstraction

**작성일**: 2026-04-30 (Codex 2차 검토 보정 2026-04-30)
**상위 문서**: `00_RHI_MIGRATION_MASTER.md` §2
**범위**: `IRHICommandList / IRHICommandPool / IRHIFence / IRHISemaphore` 박제 + DX11 immediate emulation + **Renderer/Resource/CEngineApp/FX 9개 leak consumer 마이그 + Public DX11 헤더 → Private 이동 + GameInstance `Get_RHIDevice()` 정식 rename**
**합격**: Engine/Public 전수 grep `ID3D11Device|d3d11.h|RHI/DX11` → 0 hit, `Engine/Public/RHI/DX11` 폴더 삭제, `CDX11Device.h` Private 이동 완료

**한 줄**: **★ Codex 2차 보정 — RH-2 가 본 plan 의 가장 무거운 phase. CommandList 추상화 + Renderer 마이그 + Public DX11 헤더 일괄 이동 + GameInstance 정식 rename 까지 한 번에. 2주.**

---

## ★ Codex 2차 검토 변경 요약

| 변경 | 이전 RH-2 (1차) | 신규 RH-2 (2차) |
|---|---|---|
| 기간 | 1주 | **2주** (P0-1, P0-2 의 file move 흡수) |
| DX11 CommandList Begin/End | "no-op" | **★ RTV/DSV/viewport 바인딩 책임 명시** (P1-13) |
| Renderer 마이그 | "30곳" 추상 | **9개 leak consumer 전수 명시** (P1-14) |
| Public DX11 헤더 이동 | RH-0 에서 | **RH-2 종료 시 일괄 이동** (P0-1, P0-2) |
| GameInstance rename | RH-1 에서 | **RH-2 종료 시 `Get_NewRHIDevice()` → `Get_RHIDevice()`** (P0-4) |

---

## 1. 신규 인터페이스 (★ 추후 본격 박제 — h/cpp 전문 별도 sprint)

### 1.1 `IRHICommandList.h` — 핵심 시그니처

```cpp
class IRHICommandList
{
public:
    virtual ~IRHICommandList() = default;

    // Lifecycle
    virtual void Begin() = 0;
    virtual void End()   = 0;

    // Pipeline / RenderPass (RH-3 박제 후 정식)
    virtual void BeginRenderPass(const RHIRenderPassBeginDesc& desc) = 0;
    virtual void EndRenderPass()                                      = 0;
    virtual void SetPipeline(IRHIPipelineState* pso)                  = 0;
    virtual void SetBindGroup(u32_t setIndex, IRHIBindGroup* bg)      = 0;

    // Vertex / Index
    virtual void SetVertexBuffer(u32_t slot, IRHIBuffer* vb, u32_t offsetBytes = 0) = 0;
    virtual void SetIndexBuffer(IRHIBuffer* ib, eRHIIndexFormat fmt)                = 0;

    // Viewport / Scissor
    virtual void SetViewport(f32_t x, f32_t y, f32_t w, f32_t h, f32_t minDepth = 0.f, f32_t maxDepth = 1.f) = 0;
    virtual void SetScissor(i32_t x, i32_t y, u32_t w, u32_t h) = 0;

    // Draw
    virtual void Draw(u32_t vertexCount, u32_t instanceCount = 1, u32_t firstVertex = 0, u32_t firstInstance = 0) = 0;
    virtual void DrawIndexed(u32_t indexCount, u32_t instanceCount = 1, u32_t firstIndex = 0, i32_t vertexOffset = 0, u32_t firstInstance = 0) = 0;

    // Compute
    virtual void Dispatch(u32_t x, u32_t y, u32_t z) = 0;

    // Resource transition (DX11 = no-op, DX12/VK = 진짜 barrier)
    virtual void ResourceBarrier(IRHIBuffer*  pBuf, eRHIResourceState before, eRHIResourceState after) = 0;
    virtual void ResourceBarrier(IRHITexture* pTex, eRHIResourceState before, eRHIResourceState after) = 0;

    // Copy
    virtual void CopyBuffer(IRHIBuffer* dst, IRHIBuffer* src, u32_t sizeBytes, u32_t srcOffset = 0, u32_t dstOffset = 0) = 0;
    virtual void CopyTexture(IRHITexture* dst, IRHITexture* src) = 0;

    // Update (Map/Unmap 의 wrapper)
    virtual void UpdateBuffer(IRHIBuffer* dst, const void* data, u32_t sizeBytes, u32_t offset = 0) = 0;

protected:
    IRHICommandList() = default;
};
```

### 1.2 `IRHICommandPool.h` — VK 의 per-thread CommandPool 추상화

```cpp
class IRHICommandPool
{
public:
    virtual ~IRHICommandPool() = default;

    virtual std::unique_ptr<IRHICommandList> AllocateCommandList() = 0;
    virtual void Reset() = 0;   // 모든 list 회수

protected:
    IRHICommandPool() = default;
};
```

### 1.3 `IRHIFence.h` — CPU↔GPU 동기화

```cpp
class IRHIFence
{
public:
    virtual ~IRHIFence() = default;

    virtual void  Signal(u64_t value) = 0;
    virtual void  Wait(u64_t value)   = 0;
    virtual u64_t GetCompletedValue() = 0;

protected:
    IRHIFence() = default;
};
```

### 1.4 `IRHISemaphore.h` — GPU↔GPU 동기화 (VK 핵심, DX12 = monolithic fence)

```cpp
class IRHISemaphore
{
public:
    virtual ~IRHISemaphore() = default;
protected:
    IRHISemaphore() = default;
};
```

---

## 2. DX11 Immediate Emulation 전략 (★ Codex P1-13 보정)

★ **Codex P1-13 보정**: Begin/End 가 단순 no-op 이면 RTV/DSV/viewport 바인딩 책임이 붕 뜬다. `CDX11CommandList::Begin()` 이 본 frame 의 RTV/DSV/viewport 바인딩 책임 명시.

```cpp
class CDX11CommandList final : public IRHICommandList
{
public:
    static std::unique_ptr<CDX11CommandList> Create(
        CDX11Device* pDevice, ID3D11DeviceContext* pCtx);

    // ─────────────────────────────────────────────────
    // Begin / End — DX11 immediate emulation
    // ─────────────────────────────────────────────────
    //
    // ★ Codex P1-13 보정 — Begin 은 단순 no-op 이 아니라 frame-default state 바인딩 책임:
    //   - Default RTV/DSV (back buffer)
    //   - Default viewport
    //   - Default rasterizer (BeginRenderPass 가 override 가능)
    //
    //   End 도 단순 no-op 이 아니라 state cache flush + RTV/DSV unbind:
    //   - DX11 driver 가 매 frame 에 상태 보존하지만, RH-2 합격 시점부터 명시 unbind
    //     으로 DX12/VK 마이그 시 hidden state leak 방지.
    void Begin() override
    {
        // 1. 본 frame 의 default RTV/DSV 바인딩 — CDX11Device 가 관리
        ID3D11RenderTargetView* pRTV = m_pDevice->GetDefaultRTV();
        ID3D11DepthStencilView* pDSV = m_pDevice->GetDefaultDSV();
        m_pContext->OMSetRenderTargets(1, &pRTV, pDSV);

        // 2. Default viewport
        D3D11_VIEWPORT vp = m_pDevice->GetDefaultViewport();
        m_pContext->RSSetViewports(1, &vp);

        // 3. State tracking 초기화
        m_CurrentPSO = nullptr;
        m_CurrentRenderPassActive = false;
    }

    void End() override
    {
        // 1. 활성 render pass 강제 종료
        if (m_CurrentRenderPassActive)
            EndRenderPass();

        // 2. RTV/DSV unbind — DX12/VK 마이그 시 hidden state leak 방지
        ID3D11RenderTargetView* pNullRTV[1] = { nullptr };
        m_pContext->OMSetRenderTargets(1, pNullRTV, nullptr);
    }

    // ─────────────────────────────────────────────────
    // SetVertexBuffer — handle 기반 (★ Codex P0-5 일관)
    // ─────────────────────────────────────────────────
    void SetVertexBuffer(u32_t slot, RHIBufferHandle vb, u32_t offsetBytes) override
    {
        if (!vb.IsValid()) return;

        IRHIBuffer* pIBuf = m_pDevice->ResolveBuffer(vb);
        if (!pIBuf) return;

        ID3D11Buffer* pBuf = static_cast<CDX11Buffer*>(pIBuf)->GetD3DBuffer();
        u32_t stride = pIBuf->GetStrideBytes();
        m_pContext->IASetVertexBuffers(slot, 1, &pBuf, &stride, &offsetBytes);
    }

    void DrawIndexed(u32_t indexCount, u32_t instanceCount,
                     u32_t firstIndex, i32_t vertexOffset, u32_t firstInstance) override
    {
        m_pContext->DrawIndexedInstanced(indexCount, instanceCount,
            firstIndex, vertexOffset, firstInstance);
    }

    // ─────────────────────────────────────────────────
    // ResourceBarrier — DX11 = state tracking 만, 실제 driver call X
    // ─────────────────────────────────────────────────
    void ResourceBarrier(RHIBufferHandle h, eRHIResourceState before, eRHIResourceState after) override
    {
        if (auto* pBuf = m_pDevice->ResolveBuffer(h))
            pBuf->SetCurrentState(after);
    }

    // ... (모든 IRHICommandList 메서드를 m_pContext 즉시 호출로 wrap)

private:
    CDX11CommandList() = default;

    CDX11Device*         m_pDevice = nullptr;        // borrowed (default RTV/DSV/viewport)
    ID3D11DeviceContext* m_pContext = nullptr;       // borrowed
    IRHIPipelineState*   m_CurrentPSO = nullptr;
    bool_t               m_CurrentRenderPassActive = false;
};
```

**핵심**:
- `Begin / End` 가 **state lifecycle 의 명확한 경계**. DX11 driver 가 자동 관리하던 부분을 명시화 → DX12/VK 마이그 시 hidden race 방지.
- `Submit` 은 여전히 no-op (DX11 = immediate context, 이미 GPU 도달).

---

## 3. Renderer/Resource 마이그 — 9개 leak consumer 전수 (★ Codex P1-14 보정)

★ **Codex P1-14 보정**: "30곳" 추상 → 4-folder pre-scan 결과 기반 명시 매트릭스.

### 3.1 마이그 대상 매트릭스 (RH-0 inventory + RH-1 RHI 인터페이스 기반)

| # | 파일 | 현재 시그니처 / 의존 | RH-2 마이그 후 |
|---|---|---|---|
| 1 | `Engine/Public/Framework/CEngineApp.h` | DX11 헤더 직접 include | RHI 인터페이스만 (CDX11Device 직접 의존 제거) |
| 2 | `Engine/Public/Manager/UI/UI_Manager.h` | `Render(ID3D11Device*, ID3D11DeviceContext*)` | `Render(IRHICommandList*)` |
| 3 | `Engine/Public/Renderer/PlaneRenderer.h` | `Render(ID3D11DeviceContext*, const Mat4&)` | `Render(IRHICommandList*, const Mat4&)` |
| 4 | `Engine/Public/Renderer/CubeRenderer.h` | 동일 패턴 | 동일 |
| 5 | `Engine/Public/Renderer/TriangleRenderer.h` | 동일 | 동일 |
| 6 | `Engine/Public/Renderer/ModelRenderer.h` | 동일 | 동일 |
| 7 | `Engine/Public/Renderer/FxStaticMeshRenderer.h` | 동일 | 동일 |
| 8 | `Engine/Public/Resource/Mesh.h` | `Bind(ID3D11DeviceContext*)` | `Bind(IRHICommandList*)` |
| 9 | `Engine/Public/Resource/Model.h` | 동일 | 동일 |
| 10 | `Engine/Public/Resource/Texture.h` | `Create(ID3D11Device*, ...)` | `Create(IRHIDevice*, ...)` 또는 handle 반환 |
| 11 | `Engine/Public/Resource/ResourceCache.h` | DX11 의존 | RHI 인터페이스로 마이그 |
| 12 | `Engine/Public/AssetFormat/Mesh/WMeshLoader.h` | DX11 직접 | RHIDevice 통과 |
| 13 | `Engine/Public/Editor/ImGuiLayer.h` | DX11 backend integration | `GetNativeHandle(DX11_Device)` 사용 — 단 1곳 escape |
| 14 | `Engine/Public/Engine_Defines.h` | DX11 의존 (RH-0 §2.2 결정 따라) | 의존 제거 |
| 15 | `Engine/Public/RHI/CDX11Device.h` | Public 위치 | **`Engine/Private/RHI/DX11/DX11Device.h`** 로 이동 |
| 16 | `Client/Public/GameObject/FX/FxBillboardComponent.h` | DX11 직접 | RHI 인터페이스 |
| 17 | `Client/Public/GameObject/FX/FxMeshComponent.h` | 동일 | 동일 |
| 18 | `Client/Public/GameObject/FX/FxSystem.h` | 동일 | 동일 |
| 19 | `Client/Private/Scene/Scene_InGame.cpp` | 12 hit + 1898 GetContext | `Get_FrameCommandList()` |

### 3.2 마이그 명령

```powershell
# 마이그 대상 전수 검증 (RH-2 진입 전)
Set-Location C:\Users\user\Desktop\Winters

Select-String -Path "Engine\Public\*","Client\Public\*" `
    -Pattern "ID3D11Device\*|ID3D11DeviceContext\*|d3d11.h" -List
```

### 3.3 마이그 패턴 — sample (PlaneRenderer)

### 3.2 마이그 패턴

**수정 전**:
```cpp
class CPlaneRenderer
{
public:
    void Render(ID3D11DeviceContext* pCtx, const Mat4& world);
};
```

**수정 후**:
```cpp
class IRHICommandList;

class CPlaneRenderer
{
public:
    void Render(IRHICommandList* pCmd, const Mat4& world);
};
```

caller 측 (Scene_InGame.cpp:1898):
```cpp
// 수정 후
auto* pCmd = CGameInstance::Get()->Get_FrameCommandList();
m_pAttackRangePlane->Render(pCmd, world);
```

### 3.3 Render(...) 본문 마이그

각 Renderer 의 Render 메서드 내부 DX11 호출 → IRHICommandList API 로 변환:

```cpp
// 수정 전
pCtx->IASetVertexBuffers(0, 1, &m_pVB, &stride, &offset);
pCtx->DrawIndexed(indexCount, 0, 0);

// 수정 후
pCmd->SetVertexBuffer(0, m_pVB.get());        // m_pVB 는 IRHIBuffer*
pCmd->DrawIndexed(indexCount);
```

---

## 4. Public DX11 헤더 일괄 이동 (★ Codex P0-1, P0-2 — RH-2 종료 시점)

★ §3 의 19개 마이그 완료 후 **마지막 단계**:

```powershell
Set-Location C:\Users\user\Desktop\Winters

# 9개 헤더 + CDX11Device.h 일괄 이동
git mv Engine/Public/RHI/DX11/BlendStateCache.h    Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11Buffer.h         Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11ConstantBuffer.h Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11IndexBuffer.h    Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11Pipeline.h       Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11Shader.h         Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11StructuredBuffer.h Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/DX11VertexBuffer.h   Engine/Private/RHI/DX11/
git mv Engine/Public/RHI/DX11/SamplerStateCache.h  Engine/Private/RHI/DX11/

# CDX11Device.h → Private 이동 + rename
git mv Engine/Public/RHI/CDX11Device.h Engine/Private/RHI/DX11/DX11Device.h

# 빈 폴더 정리
git clean -fd Engine/Public/RHI/DX11
```

Engine.vcxproj + filters XML 갱신 (Phase 0 §3.2, §3.3 의 박제된 diff 그대로 적용).

---

## 5. GameInstance `Get_NewRHIDevice` → `Get_RHIDevice` 정식 rename (★ Codex P0-4)

§3 + §4 완료 후, 모든 caller 가 `_Legacy` 또는 `_NewRHIDevice` 사용 중. 이 시점에 정식 rename:

```powershell
# 1. _Legacy 8개 deprecated 처리 (이미 RH-0 에서 됨, 여기선 유지)
# 2. _NewRHIDevice → 정식 _RHIDevice 로 rename
Set-Location C:\Users\user\Desktop\Winters

(Get-Content Engine\Include\GameInstance.h -Raw) `
    -replace 'Get_NewRHIDevice\b','Get_RHIDevice' `
| Set-Content Engine\Include\GameInstance.h -NoNewline

(Get-Content Engine\Private\GameInstance.cpp -Raw) `
    -replace 'Get_NewRHIDevice\b','Get_RHIDevice' `
| Set-Content Engine\Private\GameInstance.cpp -NoNewline

# 모든 caller 도 동일 rename
Get-ChildItem -Path Client\,Engine\ -Recurse -Include *.cpp,*.h `
| ForEach-Object {
    (Get-Content $_.FullName -Raw) -replace 'Get_NewRHIDevice\b','Get_RHIDevice' `
    | Set-Content $_.FullName -NoNewline
}
```

이 시점부터 `Get_RHIDevice() -> IRHIDevice*` 정식 (RH-0 의 `Get_DX11Device_Legacy()` 와 양립 — 8개 _Legacy 는 추후 사용처 제로 시 삭제).

---

## 6. 합격 게이트 (RH-2 전체)

### 6.1 인터페이스 박제 합격
- ✅ `IRHICommandList / Pool / Fence / Semaphore` 4개 인터페이스 박제
- ✅ `CDX11CommandList` 의 `Begin/End` 가 RTV/DSV/viewport 책임 명시 (★ Codex P1-13)
- ✅ Handle 기반 API (`SetVertexBuffer(slot, RHIBufferHandle)`)

### 6.2 Renderer/Resource 마이그 합격 (★ Codex P1-14)
- ✅ §3.1 19개 대상 전수 마이그 (4-folder pre-scan 기반)
- ✅ `rg "ID3D11Device|ID3D11DeviceContext" Engine/Public/ Client/Public/` → 0 hit
- ✅ `rg "d3d11.h\|RHI/DX11" Engine/Public/ Client/Public/` → 0 hit (단 ImGuiLayer.cpp 등 escape 1곳)

### 6.3 Public DX11 헤더 제거 합격 (★ Codex P0-1, P0-2)
- ✅ `Engine/Public/RHI/DX11/` 폴더 부재
- ✅ `Engine/Public/RHI/CDX11Device.h` 부재 (`Engine/Private/RHI/DX11/DX11Device.h` 로 이동)
- ✅ Engine.vcxproj + .filters XML 갱신

### 6.4 GameInstance rename 합격 (★ Codex P0-4)
- ✅ `Get_NewRHIDevice()` → `Get_RHIDevice()` 정식 rename
- ✅ `Get_DX11Device_Legacy()` 등 8개 `_Legacy` 그대로 유지 (사용처 0 검증 후 RH-3 또는 RH-4 시점에 삭제 검토)

### 6.5 빌드/런타임 합격
- ✅ Engine.dll + Client.exe 빌드 통과
- ✅ LoL 무회귀 + 시각 결과 동일
- ✅ `Get_FrameCommandList() -> IRHICommandList*` 정식 가동 (RH-1 의 nullptr 폐기)

---

## 7. 추후 박제 항목 (RH-2 본격 sprint 진입 시 본 stub 확장)

- [ ] `IRHICommandList.h` h/cpp 전문 (200~300줄)
- [ ] `CDX11CommandList.h/.cpp` 전문 (immediate emulation, ~400줄)
- [ ] `IRHICommandPool / IRHIFence / IRHISemaphore` 전문
- [ ] §3.1 19개 caller / callee 마이그 diff 전문 명시
- [ ] LoL 통합 테스트 시나리오

본 sub-plan 은 RH-1 합격 후 본격 박제. 현재는 outline + Codex 2차 보정 반영.
