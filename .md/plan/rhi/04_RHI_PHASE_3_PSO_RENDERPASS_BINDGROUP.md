# Phase RH-3 Sub-plan: PSO + RenderPass + BindGroup

**작성일**: 2026-04-30
**상위 문서**: `00_RHI_MIGRATION_MASTER.md` §2
**범위**: `IRHIPipelineState / IRHIRenderPass / IRHIBindGroup / IRHIBindGroupLayout` + Desc 시리즈
**합격**: 새 셰이더 추가 시 PSO 1개로 끝 (state 흩어짐 X), Render pass attachment / load-store op 명시

**한 줄**: **DX12/VK 의 monolithic PSO + RenderPass + DescriptorSet 모델 채택. DX11 백엔드는 PSO 분해해서 SetRasterizer/SetBlend/SetDepth 호출.**

---

## 1. 신규 인터페이스 (★ 추후 본격 박제)

### 1.1 `IRHIPipelineState.h`

```cpp
class IRHIPipelineState
{
public:
    virtual ~IRHIPipelineState() = default;

    virtual const RHIGraphicsPipelineDesc& GetDesc() const = 0;
    virtual void* GetNativeHandle(eRHINativeHandleType t) const = 0;

protected:
    IRHIPipelineState() = default;
};
```

### 1.2 `IRHIRenderPass.h`

```cpp
class IRHIRenderPass
{
public:
    virtual ~IRHIRenderPass() = default;
    // RenderPass 객체 자체는 stateless — Desc 만 담음. Begin/End 는 CommandList.
protected:
    IRHIRenderPass() = default;
};
```

### 1.3 `IRHIBindGroup / IRHIBindGroupLayout.h` (★ Codex P1-15 보정)

★ **Codex P1-15 보정**: BindGroup 의 mutable `SetBuffer/SetTexture/SetSampler` 는 DX12/VK descriptor lifetime 관리 불명확. **생성 시 immutable** + 별도 `UpdateBindGroup()` API 분리.

```cpp
class IRHIBindGroupLayout
{
public:
    virtual ~IRHIBindGroupLayout() = default;

    IRHIBindGroupLayout(const IRHIBindGroupLayout&) = delete;
    IRHIBindGroupLayout& operator=(const IRHIBindGroupLayout&) = delete;

protected:
    IRHIBindGroupLayout() = default;
};

class IRHIBindGroup
{
public:
    virtual ~IRHIBindGroup() = default;

    IRHIBindGroup(const IRHIBindGroup&) = delete;
    IRHIBindGroup& operator=(const IRHIBindGroup&) = delete;

    // ★ Codex P1-15 — BindGroup 은 생성 시 immutable.
    //   바인딩 변경 필요하면 새 BindGroup 생성 권장 (DX12/VK descriptor set 모델).
    //   Update API 는 별도로 device->UpdateBindGroup(handle, desc) 형태 — 비싸다는 hint.
    virtual RHIBindGroupHandle GetHandle() const = 0;

protected:
    IRHIBindGroup() = default;
};
```

`IRHIDevice` 추가 메서드 (RH-3 단계):
```cpp
// 생성 시 모든 binding 명시 — immutable
virtual RHIBindGroupHandle CreateBindGroup(const RHIBindGroupDesc& desc) = 0;

// ★ Codex P1-15 — Update 는 별도 API. DX12/VK 는 descriptor copy / VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND 사용.
//   비싸다는 의미를 이름으로 노출.
virtual void UpdateBindGroupExpensive(RHIBindGroupHandle h, const RHIBindGroupDesc& desc) = 0;
```

---

## 2. Desc 시리즈 (RHIDescriptors.h 확장)

```cpp
struct RHIBlendDesc
{
    bool_t  bEnabled         = false;
    eRHIBlend srcColor        = eRHIBlend::SrcAlpha;
    eRHIBlend dstColor        = eRHIBlend::InvSrcAlpha;
    eRHIBlendOp colorOp       = eRHIBlendOp::Add;
    eRHIBlend srcAlpha        = eRHIBlend::One;
    eRHIBlend dstAlpha        = eRHIBlend::Zero;
    eRHIBlendOp alphaOp       = eRHIBlendOp::Add;
    u8_t    writeMask         = 0x0F;   // RGBA
};

struct RHIDepthStencilDesc
{
    bool_t          bDepthEnable    = true;
    bool_t          bDepthWrite     = true;
    eRHIComparison  depthFunc       = eRHIComparison::Less;
    bool_t          bStencilEnable  = false;
    // ... stencil ops 생략
};

struct RHIRasterizerDesc
{
    eRHIFillMode fillMode  = eRHIFillMode::Solid;
    eRHICullMode cullMode  = eRHICullMode::Back;
    bool_t       bFrontCCW = false;
    f32_t        depthBias = 0.f;
    bool_t       bScissorEnable = false;
};

struct RHIInputElementDesc
{
    // ★ Codex P2-28 보정 — semanticName lifetime 정책:
    //   - **static const char* literal 만 허용** (예: "POSITION", "TEXCOORD")
    //   - dynamic string / std::string::c_str() 금지 — input layout 객체 lifetime
    //     동안 dangling 가능
    //   - DX11 InputLayout / DX12 InputLayout / VK VertexInputAttributeDescription
    //     모두 string 을 즉시 사용하므로 호출 시점에만 유효해도 OK 지만, 안전 마진
    //     으로 static literal 강제.
    const char* semanticName;          // static const char* literal only
    u32_t       semanticIndex;
    eRHIFormat  format;
    u32_t       inputSlot;
    u32_t       alignedByteOffset;
};

struct RHIInputLayoutDesc
{
    const RHIInputElementDesc* pElements;
    u32_t                      elementCount;
};

struct RHIGraphicsPipelineDesc
{
    IRHIShader*               pVS = nullptr;
    IRHIShader*               pPS = nullptr;
    RHIInputLayoutDesc        inputLayout;
    RHIBlendDesc              blend;
    RHIDepthStencilDesc       depthStencil;
    RHIRasterizerDesc         rasterizer;
    eRHIPrimitiveTopology     topology = eRHIPrimitiveTopology::TriangleList;

    // RenderTarget formats (RH-3 RenderPass 와 매칭)
    eRHIFormat                rtvFormats[8] = {};
    u32_t                     numRenderTargets = 1;
    eRHIFormat                dsvFormat        = eRHIFormat::D24_UNorm_S8_UInt;

    // BindGroup layouts (DX12 root signature, VK pipeline layout)
    IRHIBindGroupLayout*      pBindGroupLayouts[4] = {};
    u32_t                     numBindGroupLayouts  = 0;
};

struct RHIRenderPassColorAttachment
{
    IRHITexture*  pTarget;
    eRHILoadOp    loadOp;
    eRHIStoreOp   storeOp;
    f32_t         clearColor[4];
};

struct RHIRenderPassDepthAttachment
{
    IRHITexture*  pTarget;
    eRHILoadOp    loadOp;
    eRHIStoreOp   storeOp;
    f32_t         clearDepth;
    u8_t          clearStencil;
};

struct RHIRenderPassBeginDesc
{
    RHIRenderPassColorAttachment   colorAttachments[8];
    u32_t                          numColorAttachments = 0;
    RHIRenderPassDepthAttachment   depthAttachment;
    bool_t                         bHasDepthAttachment = false;
};

struct RHIBindGroupLayoutBinding
{
    u32_t              slot;
    eRHIShaderStage    stage;
    enum class eType : u8_t { ConstantBuffer, ShaderResource, UnorderedAccess, Sampler } type;
    u32_t              count = 1;     // array binding
};

struct RHIBindGroupLayoutDesc
{
    const RHIBindGroupLayoutBinding* pBindings;
    u32_t                            bindingCount;
};

struct RHIBindGroupDesc
{
    IRHIBindGroupLayout* pLayout;
    // 실제 자원은 bg->SetBuffer/SetTexture/SetSampler 로 채움
};
```

---

## 3. IRHIDevice 에 Create 메서드 추가

```cpp
// IRHIDevice 확장:
virtual std::unique_ptr<IRHIPipelineState>   CreateGraphicsPipeline(const RHIGraphicsPipelineDesc& desc) = 0;
virtual std::unique_ptr<IRHIBindGroupLayout> CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc)   = 0;
virtual std::unique_ptr<IRHIBindGroup>       CreateBindGroup(const RHIBindGroupDesc& desc)               = 0;
```

---

## 4. DX11 백엔드 PSO 구현 (분해 패턴)

```cpp
class CDX11PipelineState : public IRHIPipelineState
{
public:
    static std::unique_ptr<CDX11PipelineState> Create(
        ID3D11Device* pDevice,
        const RHIGraphicsPipelineDesc& desc);

    // Bind 시 분해해서 호출
    void BindToContext(ID3D11DeviceContext* pCtx) const;

private:
    Microsoft::WRL::ComPtr<ID3D11InputLayout>      m_pInputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>  m_pRasterState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pDepthState;
    Microsoft::WRL::ComPtr<ID3D11BlendState>       m_pBlendState;
    ID3D11VertexShader* m_pVS = nullptr;
    ID3D11PixelShader*  m_pPS = nullptr;
    eRHIPrimitiveTopology m_Topology;

    RHIGraphicsPipelineDesc m_Desc;
};

void CDX11PipelineState::BindToContext(ID3D11DeviceContext* pCtx) const
{
    pCtx->IASetInputLayout(m_pInputLayout.Get());
    pCtx->IASetPrimitiveTopology(/* eRHIPrimitiveTopology → D3D 변환 */);
    pCtx->RSSetState(m_pRasterState.Get());
    pCtx->OMSetDepthStencilState(m_pDepthState.Get(), 0);
    pCtx->OMSetBlendState(m_pBlendState.Get(), nullptr, 0xFFFFFFFF);
    pCtx->VSSetShader(m_pVS, nullptr, 0);
    pCtx->PSSetShader(m_pPS, nullptr, 0);
}
```

DX11CommandList::SetPipeline 구현:
```cpp
void CDX11CommandList::SetPipeline(IRHIPipelineState* pso)
{
    static_cast<CDX11PipelineState*>(pso)->BindToContext(m_pContext);
}
```

---

## 5. 합격
- ✅ 4개 인터페이스 + 8개 Desc struct 박제
- ✅ 기존 `DX11Pipeline.h` 의 `Create / Create3D / CreateMesh / CreateSkinnedMesh` 4개 변형 → `CDX11PipelineState::Create(RHIGraphicsPipelineDesc{...})` 단일 진입점
- ✅ ImGui 샘플러 / 메인 메쉬 PSO 동작 확인

---

## 6. 추후 박제

본 sub-plan 은 RH-2 합격 후 본격 박제. 현재는 outline + 핵심 시그니처.
