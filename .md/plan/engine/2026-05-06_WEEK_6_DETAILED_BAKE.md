# 2026-05-06 Week 6 상세 박제 — Track 1 IRHI 마이그 + Track 2 RH-3 (PSO/RenderPass/BindGroup) + RH-4 (64-bit handle)

**작성일**: 2026-05-06
**상태**: 검토 대기 (계획서만 작성, 코드 변경은 codex 가 진행 / 작성자 후속 검토)
**전제**: Week 5 (Public DX11 헤더 제거 + Get_RHIDevice 정식 rename) 완료
**상위 문서**: [Twin Track 계획서 §5.3](2026-05-01_TWIN_TRACK_GGX_BRDF_DX12_VULKAN_MERGE_PLAN.md), [RHI 마스터 §2 Phase Overview RH-3/RH-4](../rhi/00_RHI_MIGRATION_MASTER.md)

---

## 0. 한 줄

> **Week 6 = T1 (PBR 코드 IRHI 통과 마이그: CMaterialPBR + CTexture + DXC 컴파일) + T2 (RH-3 PSO/RenderPass/BindGroup 4 인터페이스 + RH-4 64-bit handle + CRHIResourceTable). 합격: PBR 셰이더가 IRHI 통과 동작 + PSO 1개로 새 셰이더 추가 + 64-bit handle generation check.**

---

> **Dependency note (2026-05-02):** this bake is still a valid target shape, but the current repository is expected to arrive here through a narrower Week 3 seed and a revalidated Week 4-5 transition. Treat RH-3 / RH-4 as a follow-up after the corrected Week 3 build is stable.

## 1. Week 5 결과 검증 (Week 6 진입 전)

```bash
# 1. Public 헤더 노출 0
rg "ID3D11Device|d3d11\\.h|RHI/DX11" Engine/Public/ Client/Public/ -l | wc -l   # 0

# 2. CDX11Device.h Private 위치
ls Engine/Private/RHI/DX11/DX11Device.h    # OK
[ ! -e Engine/Public/RHI/CDX11Device.h ] && echo "OK"

# 3. Get_RHIDevice() 정식
grep "IRHIDevice\* Get_RHIDevice\(\)" Engine/Include/GameInstance.h

# 4. 빌드 + 런타임 회귀 0
```

---

## 2. Week 6 작업 매트릭스

| 순서 | 작업 | 파일 | 의존 |
|---|---|---|---|
| **T1.1** | DX11 → DXC 컴파일 전환 (`Mesh3D_PBR.hlsl` / `Skinned3D_PBR.hlsl` / `LightCullCS.hlsl` / `GTAO_CS.hlsl` / `GTAO_Blur_CS.hlsl`) | 빌드 시스템 | (W5) |
| **T1.2** | CMaterialPBR 인터페이스 마이그 (`ID3D11Device*` → `IRHIDevice*`, `DX11ConstantBuffer<>` → `IRHIBuffer`) | `Engine/Public/Renderer/CMaterialPBR.h` + `.cpp` | (W5), T2.1 |
| **T1.3** | CTexture 인터페이스 마이그 (`IRHITexture` 통과) | `Engine/Public/Resource/Texture.h` + `.cpp` | (W5) |
| **T1.4** | LightCullSystem / SSAOPass 인터페이스 마이그 | `Engine/Public/Renderer/{LightCullSystem,SSAOPass}.h` + `.cpp` | (W5) |
| **T1.5** | Mesh3D_PBR / Skinned3D_PBR 셰이더 register 슬롯 명시 (DXC 호환) | `Shaders/Mesh3D_PBR.hlsl` + `Skinned3D_PBR.hlsl` | T1.1 |
| **T2.1** | `Engine/Public/RHI/IRHIPipelineState.h` 신설 | 신설 | (W5) |
| **T2.2** | `Engine/Public/RHI/IRHIRenderPass.h` 신설 | 신설 | (W5) |
| **T2.3** | `Engine/Public/RHI/IRHIBindGroup.h` + `IRHIBindGroupLayout.h` 신설 | 신설 | T2.1 |
| **T2.4** | `RHIDescriptors.h` 확장 (PipelineDesc / RenderPassDesc / BindGroupLayoutDesc) | `Engine/Public/RHI/RHIDescriptors.h` | T2.1~T2.3 |
| **T2.5** | `IRHIDevice` 확장 (CreatePipeline / CreateRenderPass / CreateBindGroup / CreateBindGroupLayout) | `Engine/Public/RHI/IRHIDevice.h` | T2.1~T2.4 |
| **T2.6** | CDX11Device 의 PSO/RenderPass/BindGroup 구현 (DX11 emulation) | `Engine/Private/RHI/DX11/DX11Device.cpp` + `DX11Pipeline.cpp` | T2.5 |
| **T2.7** | `RHIHandles.h` 의 64-bit handle 정식화 (32 idx + 32 gen, generation 0 reserved) | `Engine/Public/RHI/RHIHandles.h` | (W3) |
| **T2.8** | `Engine/Public/RHI/CRHIResourceTable.h` 신설 (handle → 실 resource 매핑) | 신설 | T2.7 |
| **T2.9** | thread-safety policy: render thread only assertion | T2.8 | T2.8 |
| **T2.10** | _Legacy 잔존 7개 (Shader/Pipeline/Cache) 제거 | `GameInstance.h` + `.cpp` | T1.4, T2.5 |

---

## 3. Track 1 — PBR IRHI 마이그

### 3.1 DXC 컴파일 전환 (T1.1)

**현재**: `D3DCompileFromFile` (D3DCompiler 사용 — DX11 한정)
**Week 6**: `DXC` (DirectX Shader Compiler — DX11/12/Vulkan 모두 호환)

**파일**: `Engine/Public/RHI/CShaderCompiler.h` (신설 또는 기존 확장)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHITypes.h"
#include <vector>
#include <memory>

namespace Engine
{
    enum class eShaderTarget : u32_t
    {
        DXIL,           // DX11/12 (DXC native)
        SPIRV,          // Vulkan (DXC -spirv)
        DXBC,           // DX11 legacy (D3DCompile, deprecated)
    };

    struct WINTERS_ENGINE ShaderCompileResult
    {
        std::vector<u8_t> bytecode;
        bool_t            ok = false;
        std::string       error;
    };

    class WINTERS_ENGINE CShaderCompiler
    {
    public:
        static ShaderCompileResult Compile(const wstring_t& path,
                                           const std::string& entryPoint,
                                           eRHIShaderStage stage,
                                           eShaderTarget target);
    };
}
```

`.cpp` 구현 요약:
- `target == DXIL` → DXC `IDxcCompiler3::Compile` (`-T cs_6_0` 등)
- `target == SPIRV` → DXC + `-spirv` 플래그
- `target == DXBC` → D3DCompileFromFile (deprecated, W7+ 제거)

### 3.2 CMaterialPBR IRHI 마이그 (T1.2)

**파일**: `Engine/Public/Renderer/CMaterialPBR.h` (W2 신설 후 W6 마이그)

```cpp
// BEFORE (W2)
class WINTERS_ENGINE CMaterialPBR
{
public:
    static std::unique_ptr<CMaterialPBR> Create(ID3D11Device* pDevice);
    void Bind(ID3D11DeviceContext* pCtx);
    /* ... */
private:
    std::unique_ptr<DX11ConstantBuffer<CBPerMaterial>> m_pCBuffer;
};

// AFTER (W6 RH-3)
class WINTERS_ENGINE CMaterialPBR
{
public:
    static std::unique_ptr<CMaterialPBR> Create(IRHIDevice* pDevice);
    //   ↑ IRHIDevice* 통과

    void Bind(IRHICommandList* pCmd);   // ← W6 RH-3 IRHICommandList
    /* ... */

private:
    RHIBufferHandle    m_hCBuffer{};      // ← handle API
    RHITextureHandle   m_hAlbedoTex{};
    RHITextureHandle   m_hNormalTex{};
    RHITextureHandle   m_hMRTex{};
    RHIBindGroupHandle m_hBindGroup{};    // ← W6 RH-3 BindGroup (b3 cbuffer + t0/t1/t2 텍스처 + s0 sampler)
};
```

`.cpp` 핵심:

```cpp
std::unique_ptr<CMaterialPBR> CMaterialPBR::Create(IRHIDevice* pDevice)
{
    auto p = std::unique_ptr<CMaterialPBR>(new CMaterialPBR());

    // 1. CBuffer (64B PerMaterial)
    RHIBufferDesc cbDesc{};
    cbDesc.sizeBytes = sizeof(CBPerMaterial);
    cbDesc.usage = eRHIBufferUsage::Constant;
    cbDesc.dynamic = true;
    cbDesc.debugName = "CBPerMaterial";
    p->m_hCBuffer = pDevice->CreateBuffer(cbDesc, &p->m_CB);

    // 2. BindGroup (b3 cbuffer + t0/t1/t2 + s0 sampler) — W6 RH-3 후 정식
    //   (W6 시점: BindGroup 생성은 §4 BindGroup 절 참조)

    return p;
}

void CMaterialPBR::Bind(IRHICommandList* pCmd)
{
    if (m_bDirty)
    {
        // CBuffer 업데이트
        // ★ W6 시점: pCmd->UpdateBuffer(m_hCBuffer, &m_CB, sizeof(m_CB));
        // 또는 pDevice->UpdateBuffer(...)
        m_bDirty = false;
    }

    // BindGroup 1개로 전체 (cbuffer + 텍스처 + 샘플러)
    pCmd->SetBindGroup(/* slot */ 0, m_hBindGroup);
}
```

### 3.3 CTexture IRHITexture 통과 (T1.3)

**파일**: `Engine/Public/Resource/Texture.h` (수정)

```cpp
// BEFORE (RH-1)
class WINTERS_ENGINE CTexture
{
public:
    static std::unique_ptr<CTexture> Create(IRHIDevice* pDevice, const wstring_t& path);
    ID3D11ShaderResourceView* GetShaderResourceView_NonOwning() const;
    /* ... */
private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSRV;   // ← still raw
};

// AFTER (W6 RH-3)
class WINTERS_ENGINE CTexture
{
public:
    static std::unique_ptr<CTexture> Create(IRHIDevice* pDevice, const wstring_t& path);

    RHITextureHandle GetHandle() const { return m_hTexture; }
    u32_t            GetWidth()  const;
    u32_t            GetHeight() const;

private:
    RHITextureHandle m_hTexture{};   // ← handle 만 보유
    IRHIDevice*      m_pDevice = nullptr;  // ← 해제 시 Destroy 위해
};
```

`.cpp`:

```cpp
std::unique_ptr<CTexture> CTexture::Create(IRHIDevice* pDevice, const wstring_t& path)
{
    auto p = std::unique_ptr<CTexture>(new CTexture());
    p->m_pDevice = pDevice;

    // 1. WIC / DDS 로드 (DirectXTK 사용 시 escape)
    //    또는 W6+ 정식: stb_image / 자체 .wtex 포맷
    auto* pNativeDevice = (ID3D11Device*)pDevice->GetNativeHandle(eRHINativeType::DX11Device);
    /* WIC 로드 → ID3D11Texture2D 획득 */

    // 2. RHITextureDesc + CreateTexture
    RHITextureDesc desc{};
    desc.width = w;
    desc.height = h;
    desc.format = eRHIFormat::R8G8B8A8_UNorm;
    desc.usage = eRHITextureUsage::ShaderResource;
    p->m_hTexture = pDevice->CreateTexture(desc, pPixelData);

    return p;
}
```

### 3.4 LightCullSystem + SSAOPass 마이그 (T1.4)

**LightCullSystem** (W3 신설 후 W6 마이그):

```cpp
// BEFORE
static std::unique_ptr<CLightCullSystem> Create(ID3D11Device* pDevice, u32_t w, u32_t h);
void Dispatch(ID3D11DeviceContext* pCtx, /*...*/);

// AFTER (W6)
static std::unique_ptr<CLightCullSystem> Create(IRHIDevice* pDevice, u32_t w, u32_t h);
void Dispatch(IRHICommandList* pCmd, /*...*/);
```

**SSAOPass** (W4 신설 후 W6 마이그) — 동일 패턴.

### 3.5 셰이더 register 슬롯 명시 (T1.5)

DXC SPIR-V 호환 위해 모든 셰이더 슬롯 명시:

```hlsl
// BEFORE (W2)
cbuffer CBPerMaterial : register(b3) { /* ... */ }
//   ↑ register 명시는 이미 OK

// AFTER (W6 — 추가 검증)
//   D3DCompile 은 register 미명시도 OK 였지만 DXC 는 일부 케이스에서 binding 충돌.
//   모든 cbuffer / Texture / SamplerState 에 명시적 register 강제.
cbuffer CBPerMaterial : register(b3, space0) { /* ... */ }
Texture2D    g_AlbedoMap : register(t0, space0);
SamplerState g_LinearWrap : register(s0, space0);
```

### 3.6 합격 게이트 (Track 1 W6)

- ✅ 5개 셰이더 (Mesh3D_PBR / Skinned3D_PBR / LightCullCS / GTAO_CS / GTAO_Blur_CS) DXC 컴파일 통과
- ✅ CMaterialPBR / CTexture / CLightCullSystem / CSSAOPass 모두 IRHIDevice* 통과
- ✅ 이렐리아 PBR + Forward+ + SSAO 가 IRHI 인터페이스 경로로 동작
- ✅ Frame ≤20ms 회귀 0

---

## 4. Track 2 — RH-3 PSO/RenderPass/BindGroup + RH-4 64-bit handle

### 4.1 IRHIPipelineState (T2.1)

**파일**: `Engine/Public/RHI/IRHIPipelineState.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "RHIDescriptors.h"
#include "RHIHandles.h"

namespace Engine
{
    enum class eRHIPrimitiveTopology : u32_t { TriangleList, TriangleStrip, LineList, PointList };
    enum class eRHIBlendMode : u32_t { Opaque, AlphaBlend, Additive, Premultiplied };
    enum class eRHIDepthOp : u32_t { Less, LessEqual, Greater, Always, Never };
    enum class eRHICullMode : u32_t { None, Front, Back };

    struct WINTERS_ENGINE RHIInputElementDesc
    {
        const char* semanticName = nullptr;   // ★ static const char* 만 허용 (lifetime: 영구)
        u32_t       semanticIndex = 0;
        eRHIFormat  format = eRHIFormat::R32G32B32_Float;
        u32_t       alignedByteOffset = 0;
        u32_t       inputSlot = 0;
    };

    struct WINTERS_ENGINE RHIPipelineDesc
    {
        RHIShaderHandle vsHandle{};
        RHIShaderHandle psHandle{};
        RHIShaderHandle csHandle{};   // compute pipeline 용 (vs/ps null)

        const RHIInputElementDesc* inputElements = nullptr;
        u32_t inputElementCount = 0;

        eRHIPrimitiveTopology topology = eRHIPrimitiveTopology::TriangleList;
        eRHIBlendMode         blendMode = eRHIBlendMode::Opaque;
        eRHIDepthOp           depthOp = eRHIDepthOp::Less;
        eRHICullMode          cullMode = eRHICullMode::Back;
        bool_t                depthWrite = true;

        eRHIFormat            rtvFormats[8] = { eRHIFormat::R8G8B8A8_UNorm };
        u32_t                 rtvCount = 1;
        eRHIFormat            dsvFormat = eRHIFormat::D24_UNorm_S8_UInt;

        const char*           debugName = nullptr;
    };

    class WINTERS_ENGINE IRHIPipelineState
    {
    public:
        virtual ~IRHIPipelineState() = default;
        virtual const RHIPipelineDesc& GetDesc() const = 0;
        virtual void* GetNativeHandle(eRHINativeType type) = 0;
    };
}
```

### 4.2 IRHIRenderPass (T2.2)

**파일**: `Engine/Public/RHI/IRHIRenderPass.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "RHIDescriptors.h"

namespace Engine
{
    enum class eRHILoadOp : u32_t { Load, Clear, DontCare };
    enum class eRHIStoreOp : u32_t { Store, DontCare };

    struct WINTERS_ENGINE RHIAttachmentDesc
    {
        RHITextureHandle textureHandle{};
        eRHIFormat       format = eRHIFormat::R8G8B8A8_UNorm;
        eRHILoadOp       loadOp = eRHILoadOp::Clear;
        eRHIStoreOp      storeOp = eRHIStoreOp::Store;
        f32_t            clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        f32_t            clearDepth = 1.0f;
        u8_t             clearStencil = 0;
    };

    struct WINTERS_ENGINE RHIRenderPassDesc
    {
        RHIAttachmentDesc colorAttachments[8];
        u32_t             colorCount = 0;
        RHIAttachmentDesc depthAttachment;
        bool_t            hasDepth = false;
        const char*       debugName = nullptr;
    };

    class WINTERS_ENGINE IRHIRenderPass
    {
    public:
        virtual ~IRHIRenderPass() = default;
        virtual const RHIRenderPassDesc& GetDesc() const = 0;
        virtual void* GetNativeHandle(eRHINativeType type) = 0;
    };
}
```

### 4.3 IRHIBindGroup + IRHIBindGroupLayout (T2.3)

**파일**: `Engine/Public/RHI/IRHIBindGroup.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "RHIDescriptors.h"

namespace Engine
{
    enum class eRHIBindingType : u32_t
    {
        ConstantBuffer,
        ShaderResource,        // Texture or Buffer SRV
        UnorderedAccess,       // RWTexture or RWBuffer UAV
        Sampler,
    };

    enum class eRHIShaderVisibility : u32_t
    {
        Vertex   = 1 << 0,
        Pixel    = 1 << 1,
        Compute  = 1 << 2,
        All      = 0xFF,
    };

    struct WINTERS_ENGINE RHIBindingSlot
    {
        u32_t                slot = 0;
        eRHIBindingType      type = eRHIBindingType::ShaderResource;
        eRHIShaderVisibility visibility = eRHIShaderVisibility::All;
    };

    struct WINTERS_ENGINE RHIBindGroupLayoutDesc
    {
        const RHIBindingSlot* slots = nullptr;
        u32_t                 slotCount = 0;
        const char*           debugName = nullptr;
    };

    struct WINTERS_ENGINE RHIBindGroupResource
    {
        u32_t slot = 0;
        // 다음 중 하나만 valid (handle 종류로 판별)
        RHIBufferHandle  bufferHandle{};
        RHITextureHandle textureHandle{};
        RHISamplerHandle samplerHandle{};
    };

    struct WINTERS_ENGINE RHIBindGroupDesc
    {
        RHIBindGroupHandle  layoutHandle{};   // 어떤 layout 의 binding
        const RHIBindGroupResource* resources = nullptr;
        u32_t               resourceCount = 0;
        const char*         debugName = nullptr;
    };

    // ★ BindGroup 은 immutable. 변경하려면 별도 UpdateBindGroup() API 또는 재생성.
    class WINTERS_ENGINE IRHIBindGroupLayout
    {
    public:
        virtual ~IRHIBindGroupLayout() = default;
        virtual const RHIBindGroupLayoutDesc& GetDesc() const = 0;
    };

    class WINTERS_ENGINE IRHIBindGroup
    {
    public:
        virtual ~IRHIBindGroup() = default;
        virtual const RHIBindGroupDesc& GetDesc() const = 0;
        virtual void* GetNativeHandle(eRHINativeType type) = 0;
    };
}
```

### 4.4 IRHIDevice 확장 (T2.5)

```cpp
// IRHIDevice.h 추가
public:
    /* RH-1 메서드 그대로 + */

    virtual RHIPipelineHandle    CreatePipeline(const RHIPipelineDesc& desc) = 0;
    virtual void                 DestroyPipeline(RHIPipelineHandle h) = 0;

    virtual RHIRenderPassHandle  CreateRenderPass(const RHIRenderPassDesc& desc) = 0;
    virtual void                 DestroyRenderPass(RHIRenderPassHandle h) = 0;

    virtual RHIBindGroupHandle   CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc) = 0;
    virtual RHIBindGroupHandle   CreateBindGroup(const RHIBindGroupDesc& desc) = 0;
    virtual void                 DestroyBindGroup(RHIBindGroupHandle h) = 0;

    // BindGroup 의 일부 자원만 변경 (textures swap 등)
    virtual void UpdateBindGroup(RHIBindGroupHandle h,
                                 const RHIBindGroupResource* resources,
                                 u32_t resourceCount) = 0;
```

### 4.5 CDX11Device 의 PSO/RenderPass/BindGroup DX11 emulation (T2.6)

**파일**: `Engine/Private/RHI/DX11/DX11Device.cpp`

```cpp
RHIPipelineHandle CDX11Device::CreatePipeline(const RHIPipelineDesc& desc)
{
    // DX11 emulation: PSO = (InputLayout + VS + PS + RS + BS + DS + Topology) 묶음
    auto* pso = new CDX11PipelineState();   // PIMPL 안에 raw DX11 state 보유
    pso->Initialize(m_pDevice.Get(), desc);

    // ResourceTable 에 등록
    RHIPipelineHandle h = m_PipelineTable.Insert(pso);
    return h;
}

RHIRenderPassHandle CDX11Device::CreateRenderPass(const RHIRenderPassDesc& desc)
{
    // DX11 emulation: RenderPass = RTV/DSV 핸들 묶음 + clear values
    auto* rp = new CDX11RenderPass();
    rp->Initialize(desc);
    RHIRenderPassHandle h = m_RenderPassTable.Insert(rp);
    return h;
}

RHIBindGroupHandle CDX11Device::CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc)
{
    // DX11 emulation: BindGroupLayout = slot table copy
    auto* bgl = new CDX11BindGroupLayout(desc);
    RHIBindGroupHandle h = m_BindGroupLayoutTable.Insert(bgl);
    return h;
}

RHIBindGroupHandle CDX11Device::CreateBindGroup(const RHIBindGroupDesc& desc)
{
    // DX11 emulation: BindGroup = (cbuffer/SRV/UAV/Sampler 슬롯별 raw pointer 배열)
    auto* bg = new CDX11BindGroup(desc);
    RHIBindGroupHandle h = m_BindGroupTable.Insert(bg);
    return h;
}
```

### 4.6 RHIHandles.h 64-bit handle 정식화 (T2.7)

```cpp
// W3 RH-1 의 임시 64-bit handle → W6 정식화
template<typename Tag>
struct WINTERS_ENGINE RHIHandle
{
    u64_t value = 0;

    bool_t IsValid() const
    {
        return value != 0 && Generation() != 0;   // ★ generation 0 reserved (invalid)
    }

    u32_t  Index()      const { return (u32_t)(value & 0xFFFFFFFF); }
    u32_t  Generation() const { return (u32_t)((value >> 32) & 0xFFFFFFFF); }

    static RHIHandle Make(u32_t idx, u32_t gen)
    {
        // generation 0 은 reserved → 자동으로 1 이상으로
        if (gen == 0) gen = 1;
        RHIHandle h;
        h.value = ((u64_t)gen << 32) | (u64_t)idx;
        return h;
    }
};
```

### 4.7 CRHIResourceTable (T2.8)

**파일**: `Engine/Public/RHI/CRHIResourceTable.h` (신설)

```cpp
#pragma once
#include "WintersAPI.h"
#include "WintersTypes.h"
#include "RHIHandles.h"
#include <vector>
#include <cassert>

namespace Engine
{
    template<typename TResource, typename TTag>
    class WINTERS_ENGINE CRHIResourceTable
    {
    public:
        using HandleType = RHIHandle<TTag>;

        // ★ thread-safety policy: render thread only.
        //   다른 thread 에서 호출 시 debug build 에서 assert.
        HandleType Insert(TResource* pResource)
        {
            AssertRenderThread();
            u32_t idx;
            if (!m_FreeList.empty())
            {
                idx = m_FreeList.back();
                m_FreeList.pop_back();
                m_Slots[idx].pResource = pResource;
                ++m_Slots[idx].generation;   // 재사용 시 gen 증가
            }
            else
            {
                idx = (u32_t)m_Slots.size();
                m_Slots.push_back({ pResource, 1u });   // gen 1 부터
            }
            return HandleType::Make(idx, m_Slots[idx].generation);
        }

        TResource* Lookup(HandleType h)
        {
            AssertRenderThread();
            if (!h.IsValid()) return nullptr;
            u32_t idx = h.Index();
            if (idx >= m_Slots.size()) return nullptr;

            // generation check (use-after-free 방지)
            if (m_Slots[idx].generation != h.Generation())
            {
                // 디버그: dangling handle
                return nullptr;
            }
            return m_Slots[idx].pResource;
        }

        void Erase(HandleType h)
        {
            AssertRenderThread();
            if (!h.IsValid()) return;
            u32_t idx = h.Index();
            if (idx >= m_Slots.size()) return;
            if (m_Slots[idx].generation != h.Generation()) return;

            delete m_Slots[idx].pResource;
            m_Slots[idx].pResource = nullptr;
            m_FreeList.push_back(idx);
            // generation 은 다음 Insert 시 ++ 처리.
        }

    private:
        struct Slot
        {
            TResource* pResource = nullptr;
            u32_t      generation = 0;
        };
        std::vector<Slot>   m_Slots;
        std::vector<u32_t>  m_FreeList;

        static void AssertRenderThread()
        {
        #ifdef _DEBUG
            // CGameInstance::IsRenderThread() 같은 헬퍼로 검증
            assert(IsCurrentThreadRenderThread() && "CRHIResourceTable must be accessed from render thread only");
        #endif
        }
    };
}
```

### 4.8 _Legacy 7개 제거 (T2.10)

```cpp
// BEFORE (W5 끝 시점)
public:
    IRHIDevice* Get_RHIDevice();   // RH-2 (W5)

    // 잔존 7개 (W6 까지)
    [[deprecated(...)]] DX11Shader*       Get_MeshShader_Legacy();
    [[deprecated(...)]] DX11Pipeline*     Get_MeshPipeline_Legacy();
    [[deprecated(...)]] CBlendStateCache* Get_BlendStateCache_Legacy();
    [[deprecated(...)]] DX11Shader*       Get_FxSpriteShader_Legacy();
    [[deprecated(...)]] DX11Pipeline*     Get_FxSpritePipeline_Legacy();
    [[deprecated(...)]] DX11Shader*       Get_FxMeshShader_Legacy();
    [[deprecated(...)]] DX11Pipeline*     Get_FxMeshPipeline_Legacy();

// AFTER (W6 끝)
public:
    IRHIDevice* Get_RHIDevice();
    //   ↑ _Legacy 7개 모두 제거. 모든 caller 가 IRHIPipelineState / IRHIShader 통과.
```

**caller 마이그**: ModelRenderer, FxRenderer 등의 `pGI->Get_MeshShader_Legacy()` 등을 이제 `pDevice->CreatePipeline(desc)` 의 결과 핸들로 대체.

### 4.9 합격 게이트 (Track 2 W6)

- ✅ IRHIPipelineState / IRHIRenderPass / IRHIBindGroup / IRHIBindGroupLayout 4 인터페이스 컴파일
- ✅ CDX11Device 의 PSO/RenderPass/BindGroup CreateXxx / DestroyXxx 동작
- ✅ 64-bit handle generation check (generation 0 invalid)
- ✅ CRHIResourceTable 의 thread-safety assert
- ✅ _Legacy 7개 제거 + caller 0 hit
- ✅ LoL 빌드 통과 + 회귀 0

---

## 5. 위험 시나리오

### 5.1 R-W6-1: DXC 컴파일 후 셰이더 결과 변화
- 시나리오: D3DCompile (DXBC) → DXC (DXIL) 전환 시 동일 HLSL 의 출력 다름
- 완화: ① W6 진입 시 DXC + D3DCompile 둘 다 출력 후 PIX 캡처 비교 ② 시각 결과 동일 확인 후 D3DCompile 경로 제거

### 5.2 R-W6-2: PSO 1개 컴파일이 너무 느림 (DX12 PSO ≈ 100ms)
- 시나리오: DX11 emulation 에서는 PSO == InputLayout + VS + PS + States 묶음, 컴파일 즉시. DX12 진입 (W7+) 시 PSO 컴파일 100ms+
- 완화: ① W6 단계에서는 CreatePipeline 즉시 ② W7 RH-5 진입 시 PSO 캐시 (디스크 ID3D12PipelineLibrary) 박제

### 5.3 R-W6-3: BindGroup immutable 정책이 동적 텍스처 스왑 막음
- 시나리오: 챔프 머티리얼이 매 프레임 metallic/roughness 변경 → BindGroup 재생성 비용
- 완화: ① cbuffer 만 dynamic (UpdateBuffer) ② 텍스처는 immutable ③ BindGroup 의 cbuffer 슬롯만 별도 (UpdateBindGroup 호출로 텍스처 스왑)

### 5.4 R-W6-4: CRHIResourceTable 의 thread-safety assert 가 정상 호출에서 발화
- 시나리오: Loading thread 가 CTexture::Create 호출 → CreateTexture → ResourceTable.Insert → assert
- 완화: ① Insert 만 lock + render thread 외 호출 허용 (생성), Lookup/Erase 는 render thread only ② 또는 thread-safe queue 로 main thread 에 위임

---

## 6. Week 6 통합 합격 검증

```bash
# 1. 4 신규 인터페이스
ls Engine/Public/RHI/{IRHIPipelineState,IRHIRenderPass,IRHIBindGroup,IRHIBindGroupLayout,CRHIResourceTable}.h

# 2. _Legacy 7개 미존재
rg "Get_.*_Legacy" Engine Client | wc -l   # 0

# 3. CMaterialPBR / CTexture / LightCullSystem / SSAOPass 의 IRHIDevice* 통과
rg "ID3D11Device" Engine/Public/Renderer/ Engine/Public/Resource/ | wc -l   # 0

# 4. DXC 컴파일 검증
ls Shaders/Mesh3D_PBR.cso Shaders/Skinned3D_PBR.cso
file Shaders/Mesh3D_PBR.cso   # DXIL 컨테이너 확인

# 5. 빌드 + 런타임 회귀 0
MSBuild Winters.sln /p:Configuration=Debug /p:Platform=x64 /m
# 게임 실행 + 이렐리아 PBR + Forward+ + SSAO 시각 변화 0 (Frame ≤20ms 회귀 0)
```

---

## 7. 부록 A — Week 6 진입 체크리스트

```
[ ] Week 5 결과 검증 통과 (Public 노출 0 + _Legacy 7개만 잔존)
[ ] devenv.exe 종료 + git: feature/2026-05-06-week6 branch
[ ] Engine 단독 빌드 → EngineSDK/inc 동기화

Track 1 (PBR IRHI 마이그):
[ ] §3.1 DXC 컴파일러 도입 (CShaderCompiler.h/.cpp)
[ ] §3.2 CMaterialPBR 인터페이스 마이그 (IRHIDevice* + RHIBufferHandle + RHIBindGroupHandle)
[ ] §3.3 CTexture 인터페이스 마이그 (RHITextureHandle)
[ ] §3.4 CLightCullSystem / CSSAOPass 인터페이스 마이그
[ ] §3.5 셰이더 register 슬롯 명시 (DXC 호환)

Track 2 (RH-3 + RH-4):
[ ] §4.1 IRHIPipelineState.h 신설
[ ] §4.2 IRHIRenderPass.h 신설
[ ] §4.3 IRHIBindGroup.h + IRHIBindGroupLayout.h 신설
[ ] §4.4 IRHIDevice 확장 (CreatePipeline/RenderPass/BindGroup + UpdateBindGroup)
[ ] §4.5 CDX11Device 의 PSO/RenderPass/BindGroup DX11 emulation
[ ] §4.6 RHIHandles.h 64-bit handle 정식화 (generation 0 reserved)
[ ] §4.7 CRHIResourceTable 신설 + thread-safety assert
[ ] §4.8 _Legacy 7개 (Shader/Pipeline/Cache) 제거 + caller 마이그

검증:
[ ] §6.2 _Legacy 미존재
[ ] §6.3 Renderer/Resource 에서 ID3D11Device 노출 0
[ ] §6.4 DXC 컴파일 통과 (DXIL .cso)
[ ] §6.5 회귀 0 + Frame ≤20ms
```

---

## 8. 한 줄

> **Week 6 = T1 (DXC 도입 + CMaterialPBR/CTexture/LightCullSystem/SSAOPass 인터페이스 마이그) + T2 (IRHIPipelineState + IRHIRenderPass + IRHIBindGroup + IRHIBindGroupLayout 신설 + CDX11Device DX11 emulation + 64-bit handle generation + CRHIResourceTable thread-safety + _Legacy 7개 제거). 합격: PBR IRHI 통과 + PSO 1개로 새 셰이더 + 회귀 0.**

---

## 끝.
