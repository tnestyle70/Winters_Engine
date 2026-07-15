#include "WintersPCH.h"
#include "RHI/DX11/CDX11Device.h"
#include "WintersCore.h"
#include "ProfilerAPI.h"

#include <cstdio>
#include <cstring>
#include <vector>

// DX11 API는 여기서만 개별 include
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ─────────────────────────────────────────────────────────────────
//  CDX11Device 구현
//
//  DX11 디바이스 초기화 순서:
//    1. D3D11CreateDeviceAndSwapChain()
//       - FeatureLevel: 11.1 우선, 11.0 폴백
//       - SwapEffect: DXGI_SWAP_EFFECT_FLIP_DISCARD (DX11.1+, Win10+)
//         → 구형 DISCARD보다 효율적. 백버퍼를 OS와 공유하지 않음.
//    2. SwapChain 백버퍼 → RenderTargetView
//    3. DepthStencil 텍스처 + View 생성
//    4. Viewport 설정
//    5. OM(Output Merger)에 RTV + DSV 바인딩
//
//  RHI 추상화 방향:
//    현재: CDX11Device가 모든 DX11 호출을 직접 수행
//    향후: IRHIDevice → CDX11Device / CDX12Device
//          Render Graph가 IRHIDevice를 통해 리소스 생성/바인딩
// ─────────────────────────────────────────────────────────────────

namespace
{
    bool_t IsHardwareAdapter(IDXGIAdapter1* pAdapter)
    {
        if (!pAdapter)
            return false;

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(pAdapter->GetDesc1(&desc)))
            return false;

        return (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> SelectHighPerformanceDX11Adapter()
    {
        Microsoft::WRL::ComPtr<IDXGIFactory1> pFactory1;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&pFactory1))))
            return nullptr;

        Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory6;
        if (SUCCEEDED(pFactory1.As(&pFactory6)))
        {
            for (UINT i = 0; ; ++i)
            {
                Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
                if (FAILED(pFactory6->EnumAdapterByGpuPreference(
                    i,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(&pAdapter))))
                {
                    break;
                }

                if (IsHardwareAdapter(pAdapter.Get()))
                    return pAdapter;
            }
        }

        for (UINT i = 0; ; ++i)
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
            if (FAILED(pFactory1->EnumAdapters1(i, &pAdapter)))
                break;

            if (IsHardwareAdapter(pAdapter.Get()))
                return pAdapter;
        }

        return nullptr;
    }

    void LogDX11Adapter(IDXGIAdapter1* pAdapter, const wchar_t* pReason)
    {
        if (!pAdapter)
        {
            OutputDebugStringA("[CDX11Device] DX11 adapter: OS default hardware adapter\n");
            return;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(pAdapter->GetDesc1(&desc)))
        {
            OutputDebugStringA("[CDX11Device] DX11 adapter: selected adapter, GetDesc1 failed\n");
            return;
        }

        char adapterName[256]{};
        WideCharToMultiByte(CP_ACP, 0, desc.Description, -1,
            adapterName, static_cast<int>(sizeof(adapterName)), nullptr, nullptr);
        if (adapterName[0] == '\0')
            sprintf_s(adapterName, "unknown");

        char reason[128]{};
        if (pReason)
        {
            WideCharToMultiByte(CP_ACP, 0, pReason, -1,
                reason, static_cast<int>(sizeof(reason)), nullptr, nullptr);
        }
        if (reason[0] == '\0')
            sprintf_s(reason, "selected");

        char log[512]{};
        sprintf_s(log,
            "[CDX11Device] DX11 adapter: %s (%llu MB dedicated VRAM, %s)\n",
            adapterName,
            static_cast<unsigned long long>(desc.DedicatedVideoMemory / (1024ull * 1024ull)),
            reason);
        OutputDebugStringA(log);
    }

    DXGI_FORMAT ToDXGIFormat(eRHIFormat format)
    {
        switch (format)
        {
        case eRHIFormat::R8G8B8A8_UNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case eRHIFormat::R16_Float: return DXGI_FORMAT_R16_FLOAT;
        case eRHIFormat::R16G16B16A16_Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case eRHIFormat::R24G8_Typeless: return DXGI_FORMAT_R24G8_TYPELESS;
        case eRHIFormat::D24_UNorm_S8_UInt: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case eRHIFormat::R24_UNorm_X8_Typeless: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case eRHIFormat::R32_Float: return DXGI_FORMAT_R32_FLOAT;
        case eRHIFormat::R32G32_Float: return DXGI_FORMAT_R32G32_FLOAT;
        case eRHIFormat::R32G32B32_Float: return DXGI_FORMAT_R32G32B32_FLOAT;
        case eRHIFormat::R32G32B32A32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case eRHIFormat::R32_UInt: return DXGI_FORMAT_R32_UINT;
        case eRHIFormat::R32G32B32A32_UInt: return DXGI_FORMAT_R32G32B32A32_UINT;
        default: return DXGI_FORMAT_UNKNOWN;
        }
    }

    u32_t BytesPerPixelOf(eRHIFormat format)
    {
        switch (format)
        {
        case eRHIFormat::R16_Float: return 2;
        case eRHIFormat::R16G16B16A16_Float: return 8;
        case eRHIFormat::R32G32_Float: return 8;
        case eRHIFormat::R32G32B32_Float: return 12;
        case eRHIFormat::R32G32B32A32_Float: return 16;
        case eRHIFormat::R32G32B32A32_UInt: return 16;
        default: return 4;
        }
    }

    D3D11_PRIMITIVE_TOPOLOGY ToDX11PrimitiveTopology(eRHIPrimitiveTopology topology)
    {
        switch (topology)
        {
        case eRHIPrimitiveTopology::TriangleStrip:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case eRHIPrimitiveTopology::LineList:
            return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case eRHIPrimitiveTopology::PointList:
            return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        case eRHIPrimitiveTopology::TriangleList:
        default:
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }

    D3D11_CULL_MODE ToDX11CullMode(eRHICullMode mode)
    {
        switch (mode)
        {
        case eRHICullMode::None: return D3D11_CULL_NONE;
        case eRHICullMode::Front: return D3D11_CULL_FRONT;
        case eRHICullMode::Back:
        default:
            return D3D11_CULL_BACK;
        }
    }

    D3D11_COMPARISON_FUNC ToDX11ComparisonFunc(eRHIDepthOp op)
    {
        switch (op)
        {
        case eRHIDepthOp::Less: return D3D11_COMPARISON_LESS;
        case eRHIDepthOp::LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
        case eRHIDepthOp::Greater: return D3D11_COMPARISON_GREATER;
        case eRHIDepthOp::Always: return D3D11_COMPARISON_ALWAYS;
        case eRHIDepthOp::Never:
        default:
            return D3D11_COMPARISON_NEVER;
        }
    }

    D3D11_FILTER ToDX11Filter(eRHIFilter filter)
    {
        switch (filter)
        {
        case eRHIFilter::Point: return D3D11_FILTER_MIN_MAG_MIP_POINT;
        case eRHIFilter::Anisotropic: return D3D11_FILTER_ANISOTROPIC;
        case eRHIFilter::Linear:
        default:
            return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }

    D3D11_TEXTURE_ADDRESS_MODE ToDX11AddressMode(eRHIAddressMode mode)
    {
        switch (mode)
        {
        case eRHIAddressMode::Clamp: return D3D11_TEXTURE_ADDRESS_CLAMP;
        case eRHIAddressMode::Border: return D3D11_TEXTURE_ADDRESS_BORDER;
        case eRHIAddressMode::Wrap:
        default:
            return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }

    struct CDX11BufferObject
    {
        RHIBufferDesc desc{};
        Microsoft::WRL::ComPtr<ID3D11Buffer> pBuffer;
        bool_t dynamic = false;
    };

    struct CDX11ShaderObject
    {
        eRHIShaderStage stage = eRHIShaderStage::Vertex;
        std::vector<u8_t> bytecode;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> pVS;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> pPS;
    };

    struct CDX11TextureObject
    {
        RHITextureDesc desc{};
        Microsoft::WRL::ComPtr<ID3D11Texture2D> pTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pSRV;
    };

    struct CDX11SamplerObject
    {
        RHISamplerDesc desc{};
        Microsoft::WRL::ComPtr<ID3D11SamplerState> pSampler;
    };

    u32_t FindSlotVisibilityMask(
        const RHIBindGroupLayoutDesc* pLayoutDesc,
        u32_t slot,
        eRHIBindingType type)
    {
        if (pLayoutDesc)
        {
            for (u32_t i = 0; i < pLayoutDesc->slotCount; ++i)
            {
                if (pLayoutDesc->slots[i].slot == slot && pLayoutDesc->slots[i].type == type)
                    return static_cast<u32_t>(pLayoutDesc->slots[i].visibility);
            }
        }

        return static_cast<u32_t>(eRHIShaderVisibility::All);
    }

    class CDX11PipelineState final : public IRHIPipelineState
    {
    public:
        explicit CDX11PipelineState(const RHIPipelineDesc& desc)
            : m_Desc(desc)
        {
            if (desc.inputElements && desc.inputElementCount > 0)
            {
                m_InputElements.assign(
                    desc.inputElements,
                    desc.inputElements + desc.inputElementCount);
                m_Desc.inputElements = m_InputElements.data();
                m_Desc.inputElementCount = static_cast<u32_t>(m_InputElements.size());
            }
        }

        const RHIPipelineDesc& GetDesc() const override
        {
            return m_Desc;
        }

        void* GetNativeHandle(eNativeHandleType) override
        {
            return nullptr;
        }

        // RHI 셰이더 핸들이 유효할 때만 호출. 실패하면 파이프라인 생성 자체를 실패로 본다.
        bool_t InitializeNative(
            ID3D11Device* pDevice,
            const CDX11ShaderObject* pVSObject,
            const CDX11ShaderObject* pPSObject)
        {
            if (!pDevice || !pVSObject || !pPSObject || !pVSObject->pVS || !pPSObject->pPS)
                return false;

            m_pVS = pVSObject->pVS;
            m_pPS = pPSObject->pPS;
            m_Topology = ToDX11PrimitiveTopology(m_Desc.topology);

            if (!m_InputElements.empty())
            {
                std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
                elements.reserve(m_InputElements.size());

                for (const RHIInputElementDesc& src : m_InputElements)
                {
                    D3D11_INPUT_ELEMENT_DESC dst{};
                    dst.SemanticName = src.semanticName;
                    dst.SemanticIndex = src.semanticIndex;
                    dst.Format = ToDXGIFormat(src.format);
                    dst.InputSlot = src.inputSlot;
                    dst.AlignedByteOffset = src.alignedByteOffset;
                    dst.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
                    elements.push_back(dst);
                }

                if (FAILED(pDevice->CreateInputLayout(
                    elements.data(),
                    static_cast<UINT>(elements.size()),
                    pVSObject->bytecode.data(),
                    pVSObject->bytecode.size(),
                    m_pInputLayout.GetAddressOf())))
                {
                    return false;
                }
            }

            D3D11_RASTERIZER_DESC rasterDesc{};
            rasterDesc.FillMode = D3D11_FILL_SOLID;
            rasterDesc.CullMode = ToDX11CullMode(m_Desc.cullMode);
            rasterDesc.DepthClipEnable = TRUE;
            if (FAILED(pDevice->CreateRasterizerState(&rasterDesc, m_pRasterizer.GetAddressOf())))
                return false;

            D3D11_DEPTH_STENCIL_DESC depthDesc{};
            depthDesc.DepthEnable = m_Desc.dsvFormat != eRHIFormat::Unknown;
            depthDesc.DepthWriteMask =
                m_Desc.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            depthDesc.DepthFunc = ToDX11ComparisonFunc(m_Desc.depthOp);
            if (FAILED(pDevice->CreateDepthStencilState(&depthDesc, m_pDepthStencil.GetAddressOf())))
                return false;

            D3D11_RENDER_TARGET_BLEND_DESC rt{};
            rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

            if (m_Desc.blendMode == eRHIBlendMode::AlphaBlend)
            {
                rt.BlendEnable = TRUE;
                rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
                rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
                rt.BlendOp = D3D11_BLEND_OP_ADD;
                rt.SrcBlendAlpha = D3D11_BLEND_ONE;
                rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
                rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            }
            else if (m_Desc.blendMode == eRHIBlendMode::Additive)
            {
                rt.BlendEnable = TRUE;
                rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
                rt.DestBlend = D3D11_BLEND_ONE;
                rt.BlendOp = D3D11_BLEND_OP_ADD;
                rt.SrcBlendAlpha = D3D11_BLEND_ONE;
                rt.DestBlendAlpha = D3D11_BLEND_ONE;
                rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            }
            else
            {
                rt.BlendEnable = FALSE;
                rt.SrcBlend = D3D11_BLEND_ONE;
                rt.DestBlend = D3D11_BLEND_ZERO;
                rt.BlendOp = D3D11_BLEND_OP_ADD;
                rt.SrcBlendAlpha = D3D11_BLEND_ONE;
                rt.DestBlendAlpha = D3D11_BLEND_ZERO;
                rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            }

            D3D11_BLEND_DESC blendDesc{};
            blendDesc.RenderTarget[0] = rt;
            if (FAILED(pDevice->CreateBlendState(&blendDesc, m_pBlend.GetAddressOf())))
                return false;

            m_bNativeReady = true;
            return true;
        }

        bool_t IsNativeReady() const
        {
            return m_bNativeReady;
        }

        void Apply(ID3D11DeviceContext* pContext) const
        {
            pContext->IASetInputLayout(m_pInputLayout.Get());
            pContext->IASetPrimitiveTopology(m_Topology);
            pContext->VSSetShader(m_pVS.Get(), nullptr, 0);
            pContext->PSSetShader(m_pPS.Get(), nullptr, 0);
            pContext->RSSetState(m_pRasterizer.Get());
            pContext->OMSetDepthStencilState(m_pDepthStencil.Get(), 0);

            const f32_t blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
            pContext->OMSetBlendState(m_pBlend.Get(), blendFactor, 0xffffffffu);
        }

    private:
        RHIPipelineDesc m_Desc{};
        std::vector<RHIInputElementDesc> m_InputElements;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVS;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPS;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_pRasterizer;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_pDepthStencil;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_pBlend;
        D3D11_PRIMITIVE_TOPOLOGY m_Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        bool_t m_bNativeReady = false;
    };

    class CDX11RenderPass final : public IRHIRenderPass
    {
    public:
        explicit CDX11RenderPass(const RHIRenderPassDesc& desc)
            : m_Desc(desc)
        {
        }

        const RHIRenderPassDesc& GetDesc() const override
        {
            return m_Desc;
        }

        void* GetNativeHandle(eNativeHandleType) override
        {
            return nullptr;
        }

    private:
        RHIRenderPassDesc m_Desc{};
    };

    class CDX11BindGroupLayout final : public IRHIBindGroupLayout
    {
    public:
        explicit CDX11BindGroupLayout(const RHIBindGroupLayoutDesc& desc)
            : m_Desc(desc)
        {
            if (desc.slots && desc.slotCount > 0)
            {
                m_Slots.assign(desc.slots, desc.slots + desc.slotCount);
                m_Desc.slots = m_Slots.data();
                m_Desc.slotCount = static_cast<u32_t>(m_Slots.size());
            }
        }

        const RHIBindGroupLayoutDesc& GetDesc() const override
        {
            return m_Desc;
        }

    private:
        RHIBindGroupLayoutDesc m_Desc{};
        std::vector<RHIBindingSlot> m_Slots;
    };

    class CDX11BindGroup final : public IRHIBindGroup
    {
    public:
        explicit CDX11BindGroup(const RHIBindGroupDesc& desc)
            : m_Desc(desc)
        {
            UpdateResources(desc.resources, desc.resourceCount);
        }

        const RHIBindGroupDesc& GetDesc() const override
        {
            return m_Desc;
        }

        void* GetNativeHandle(eNativeHandleType) override
        {
            return nullptr;
        }

        void UpdateResources(const RHIBindGroupResource* resources, u32_t resourceCount)
        {
            if (resources && resourceCount > 0)
            {
                m_Resources.assign(resources, resources + resourceCount);
                m_Desc.resources = m_Resources.data();
                m_Desc.resourceCount = static_cast<u32_t>(m_Resources.size());
            }
            else
            {
                m_Resources.clear();
                m_Desc.resources = nullptr;
                m_Desc.resourceCount = 0;
            }
        }

    private:
        RHIBindGroupDesc m_Desc{};
        std::vector<RHIBindGroupResource> m_Resources;
    };
}

struct CDX11Device::ResourceTables
{
    CRHIResourceTable<CDX11BufferObject, RHIBufferTag> bufferTable;
    CRHIResourceTable<CDX11ShaderObject, RHIShaderTag> shaderTable;
    CRHIResourceTable<CDX11TextureObject, RHITextureTag> textureTable;
    CRHIResourceTable<CDX11SamplerObject, RHISamplerTag> samplerTable;
};

// DX11 immediate context를 IRHICommandList로 감싼다. Begin/End/RenderPass는 즉시 모드라 no-op.
class CDX11FrameCommandList final : public IRHICommandList
{
public:
    explicit CDX11FrameCommandList(CDX11Device& owner)
        : m_Owner(owner)
    {
    }

    void Begin() override {}
    void End() override {}

    void BeginRenderPass(RHIRenderPassHandle) override {}
    void EndRenderPass() override {}

    void SetPipeline(RHIPipelineHandle handle) override
    {
        m_pCurrentPipeline = nullptr;

        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (!pContext)
            return;

        IRHIPipelineState* pPipelineBase = m_Owner.m_PipelineTable.Lookup(handle);
        auto* pPipeline = dynamic_cast<CDX11PipelineState*>(pPipelineBase);
        if (!pPipeline || !pPipeline->IsNativeReady())
            return;

        pPipeline->Apply(pContext);
        m_pCurrentPipeline = pPipeline;
    }

    void SetBindGroup(u32_t, RHIBindGroupHandle handle) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (!pContext || !m_Owner.m_pTables)
            return;

        IRHIBindGroup* pGroupBase = m_Owner.m_BindGroupTable.Lookup(handle);
        auto* pGroup = dynamic_cast<CDX11BindGroup*>(pGroupBase);
        if (!pGroup)
            return;

        const RHIBindGroupDesc& groupDesc = pGroup->GetDesc();

        IRHIBindGroupLayout* pLayoutBase =
            m_Owner.m_BindGroupLayoutTable.Lookup(groupDesc.layoutHandle);
        auto* pLayout = dynamic_cast<CDX11BindGroupLayout*>(pLayoutBase);
        const RHIBindGroupLayoutDesc* pLayoutDesc = pLayout ? &pLayout->GetDesc() : nullptr;

        constexpr u32_t kVertexBit = static_cast<u32_t>(eRHIShaderVisibility::Vertex);
        constexpr u32_t kPixelBit = static_cast<u32_t>(eRHIShaderVisibility::Pixel);

        for (u32_t i = 0; i < groupDesc.resourceCount; ++i)
        {
            const RHIBindGroupResource& resource = groupDesc.resources[i];
            const u32_t visibility =
                FindSlotVisibilityMask(pLayoutDesc, resource.slot, resource.type);

            switch (resource.type)
            {
            case eRHIBindingType::ConstantBuffer:
            {
                CDX11BufferObject* pBuffer =
                    m_Owner.m_pTables->bufferTable.Lookup(resource.bufferHandle);
                ID3D11Buffer* pNative = pBuffer ? pBuffer->pBuffer.Get() : nullptr;
                if (!pNative)
                    break;

                if (visibility & kVertexBit)
                    pContext->VSSetConstantBuffers(resource.slot, 1, &pNative);
                if (visibility & kPixelBit)
                    pContext->PSSetConstantBuffers(resource.slot, 1, &pNative);
                break;
            }
            case eRHIBindingType::ShaderResource:
            {
                CDX11TextureObject* pTexture =
                    m_Owner.m_pTables->textureTable.Lookup(resource.textureHandle);
                ID3D11ShaderResourceView* pSRV = pTexture ? pTexture->pSRV.Get() : nullptr;
                if (!pSRV)
                    break;

                if (visibility & kVertexBit)
                    pContext->VSSetShaderResources(resource.slot, 1, &pSRV);
                if (visibility & kPixelBit)
                    pContext->PSSetShaderResources(resource.slot, 1, &pSRV);
                break;
            }
            case eRHIBindingType::Sampler:
            {
                CDX11SamplerObject* pSampler =
                    m_Owner.m_pTables->samplerTable.Lookup(resource.samplerHandle);
                ID3D11SamplerState* pNative = pSampler ? pSampler->pSampler.Get() : nullptr;
                if (!pNative)
                    break;

                if (visibility & kVertexBit)
                    pContext->VSSetSamplers(resource.slot, 1, &pNative);
                if (visibility & kPixelBit)
                    pContext->PSSetSamplers(resource.slot, 1, &pNative);
                break;
            }
            default:
                break;
            }
        }
    }

    void SetVertexBuffer(u32_t slot, RHIBufferHandle handle, u32_t stride, u32_t offset) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (!pContext || !m_Owner.m_pTables)
            return;

        CDX11BufferObject* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(handle);
        ID3D11Buffer* pNative = pBuffer ? pBuffer->pBuffer.Get() : nullptr;
        if (!pNative)
            return;

        const UINT strides[] = { stride };
        const UINT offsets[] = { offset };
        pContext->IASetVertexBuffers(slot, 1, &pNative, strides, offsets);
    }

    void SetIndexBuffer(RHIBufferHandle handle, u32_t offset, eRHIFormat indexFormat) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (!pContext || !m_Owner.m_pTables)
            return;

        CDX11BufferObject* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(handle);
        ID3D11Buffer* pNative = pBuffer ? pBuffer->pBuffer.Get() : nullptr;
        if (!pNative)
            return;

        pContext->IASetIndexBuffer(pNative, ToDXGIFormat(indexFormat), offset);
    }

    void Draw(u32_t vertexCount, u32_t instanceCount, u32_t firstVertex, u32_t firstInstance) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (pContext)
            pContext->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void DrawIndexed(
        u32_t indexCount,
        u32_t instanceCount,
        u32_t firstIndex,
        i32_t baseVertex,
        u32_t firstInstance) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (pContext)
            pContext->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
    }

    void Dispatch(u32_t, u32_t, u32_t) override {}

    void UpdateBuffer(RHIBufferHandle handle, const void* pData, u32_t sizeBytes) override
    {
        ID3D11DeviceContext* pContext = m_Owner.GetContext();
        if (!pContext || !m_Owner.m_pTables || !pData || sizeBytes == 0)
            return;

        CDX11BufferObject* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(handle);
        if (!pBuffer || !pBuffer->pBuffer)
            return;

        if (pBuffer->dynamic)
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(pContext->Map(pBuffer->pBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return;

            const u32_t copyBytes =
                sizeBytes < pBuffer->desc.sizeBytes ? sizeBytes : pBuffer->desc.sizeBytes;
            std::memcpy(mapped.pData, pData, copyBytes);
            pContext->Unmap(pBuffer->pBuffer.Get(), 0);
        }
        else
        {
            pContext->UpdateSubresource(pBuffer->pBuffer.Get(), 0, nullptr, pData, 0, 0);
        }
    }

    void TransitionResource(RHIBufferHandle, eRHIResourceState) override {}
    void TransitionResource(RHITextureHandle, eRHIResourceState) override {}

    void* GetNativeHandle(eNativeHandleType type) const override
    {
        if (type == eNativeHandleType::DX11DeviceContext)
            return m_Owner.GetContext();

        return nullptr;
    }

private:
    CDX11Device& m_Owner;
    CDX11PipelineState* m_pCurrentPipeline = nullptr;
};

CDX11Device::CDX11Device()
    : m_pTables(new ResourceTables())
    , m_pFrameCommandList(new CDX11FrameCommandList(*this))
{
}

CDX11Device::~CDX11Device() = default;

IRHICommandList* CDX11Device::GetFrameCommandList()
{
    return m_pFrameCommandList.get();
}

bool CDX11Device::Initialize(const DeviceDesc& desc)
{
    m_bVSync = desc.vsync;
    m_Width  = desc.width;
    m_Height = desc.height;

    if (!CreateDeviceAndSwapChain(desc))
    {
        OutputDebugStringA("[CDX11Device] FAIL: CreateDeviceAndSwapChain\n");
        return false;
    }
    OutputDebugStringA("[CDX11Device] OK  : CreateDeviceAndSwapChain\n");

    if (!CreateRenderTarget())
    {
        OutputDebugStringA("[CDX11Device] FAIL: CreateRenderTarget\n");
        return false;
    }
    OutputDebugStringA("[CDX11Device] OK  : CreateRenderTarget\n");

    if (!CreateDepthStencil(desc.width, desc.height))
    {
        OutputDebugStringA("[CDX11Device] FAIL: CreateDepthStencil\n");
        return false;
    }
    OutputDebugStringA("[CDX11Device] OK  : CreateDepthStencil\n");

    // ── 뷰포트 설정 ─────────────────────────────────────────
    m_Viewport          = {};
    m_Viewport.TopLeftX = 0.f;
    m_Viewport.TopLeftY = 0.f;
    m_Viewport.Width    = static_cast<float>(desc.width);
    m_Viewport.Height   = static_cast<float>(desc.height);
    m_Viewport.MinDepth = 0.f;
    m_Viewport.MaxDepth = 1.f;
    m_pContext->RSSetViewports(1, &m_Viewport);

    // ── 기본 렌더타겟 바인딩 ─────────────────────────────────
    m_pContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());

    // GPU 타이밍 쿼리는 실패해도 디바이스 초기화를 막지 않는다.
#ifdef WINTERS_PROFILING
    m_bGpuTimingReady = CreateGpuTimingQueries();
#endif

    return true;
}

bool CDX11Device::CreateDeviceAndSwapChain(const DeviceDesc& desc)
{
    DXGI_SWAP_CHAIN_DESC scd                        = {};
    // 초기 안정화 단계에서는 전통적인 DISCARD 스왑체인을 사용한다.
    // (FLIP 모델은 백버퍼 순환/재바인딩 정책을 더 엄격히 맞춰야 함)
    scd.BufferCount                                 = 1;
    scd.BufferDesc.Width                            = desc.width;
    scd.BufferDesc.Height                           = desc.height;
    scd.BufferDesc.Format                           = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator            = 60;
    scd.BufferDesc.RefreshRate.Denominator          = 1;
    scd.BufferUsage                                 = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                                = desc.hwnd;
    scd.SampleDesc.Count                            = 1;
    scd.SampleDesc.Quality                          = 0;
    scd.Windowed                                    = !desc.fullscreen;
    scd.SwapEffect                                  = DXGI_SWAP_EFFECT_DISCARD;
    scd.Flags                                       = 0;

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL outFeatureLevel = {};

    Microsoft::WRL::ComPtr<IDXGIAdapter1> pSelectedAdapter = SelectHighPerformanceDX11Adapter();
    LogDX11Adapter(pSelectedAdapter.Get(), L"high-performance preference");

    const auto createDevice = [&](IDXGIAdapter1* pAdapter, UINT flags) -> HRESULT
    {
        m_pSwapChain.Reset();
        m_pDevice.Reset();
        m_pContext.Reset();

        return D3D11CreateDeviceAndSwapChain(
            pAdapter,
            pAdapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            flags,
            featureLevels,
            static_cast<UINT>(_countof(featureLevels)),
            D3D11_SDK_VERSION,
            &scd,
            m_pSwapChain.GetAddressOf(),
            m_pDevice.GetAddressOf(),
            &outFeatureLevel,
            m_pContext.GetAddressOf());
    };

    HRESULT hr = createDevice(pSelectedAdapter.Get(), createFlags);

#ifdef _DEBUG
    // 디버그 레이어(Graphics Tools)가 설치되지 않은 경우 폴백
    if (FAILED(hr) && (createFlags & D3D11_CREATE_DEVICE_DEBUG))
    {
        OutputDebugStringA("[CDX11Device] Debug layer unavailable — retrying without D3D11_CREATE_DEVICE_DEBUG\n");
        createFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = createDevice(pSelectedAdapter.Get(), createFlags);
    }
#endif

    if (FAILED(hr) && pSelectedAdapter.Get())
    {
        OutputDebugStringA("[CDX11Device] High-performance adapter failed; retrying OS default hardware adapter\n");
        pSelectedAdapter.Reset();
        LogDX11Adapter(nullptr, L"fallback");
        hr = createDevice(nullptr, createFlags);
    }

    if (FAILED(hr))
    {
        OutputDebugStringA("[CDX11Device] D3D11CreateDeviceAndSwapChain failed!\n");
    }

    return SUCCEEDED(hr);
}

bool CDX11Device::CreateRenderTarget()
{
    ComPtr<ID3D11Texture2D> pBackBuffer;

    HRESULT hr = m_pSwapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(pBackBuffer.GetAddressOf())
    );
    if (FAILED(hr))
        return false;

    hr = m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, m_pRenderTargetView.GetAddressOf());

    return SUCCEEDED(hr);
}

bool CDX11Device::CreateDepthStencil(uint32 width, uint32 height)
{
    D3D11_TEXTURE2D_DESC dsDesc   = {};
    dsDesc.Width                  = width;
    dsDesc.Height                 = height;
    dsDesc.MipLevels              = 1;
    dsDesc.ArraySize              = 1;
    dsDesc.Format                 = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsDesc.SampleDesc.Count       = 1;
    dsDesc.SampleDesc.Quality     = 0;
    dsDesc.Usage                  = D3D11_USAGE_DEFAULT;
    dsDesc.BindFlags              = D3D11_BIND_DEPTH_STENCIL;

    HRESULT hr = m_pDevice->CreateTexture2D(&dsDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf());
    if (FAILED(hr))
        return false;

    hr = m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr, m_pDepthStencilView.GetAddressOf());
    return SUCCEEDED(hr);
}

bool CDX11Device::CreateGpuTimingQueries()
{
    D3D11_QUERY_DESC disjointDesc{ D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };
    D3D11_QUERY_DESC stampDesc{ D3D11_QUERY_TIMESTAMP, 0 };

    for (GpuTimingSlot& slot : m_GpuTimingSlots)
    {
        if (FAILED(m_pDevice->CreateQuery(&disjointDesc, slot.pDisjoint.GetAddressOf())) ||
            FAILED(m_pDevice->CreateQuery(&stampDesc, slot.pBegin.GetAddressOf())) ||
            FAILED(m_pDevice->CreateQuery(&stampDesc, slot.pEnd.GetAddressOf())))
        {
            OutputDebugStringA("[CDX11Device] GPU timing query creation failed\n");
            return false;
        }
    }
    return true;
}

void CDX11Device::ReadGpuTimingResults()
{
    for (GpuTimingSlot& slot : m_GpuTimingSlots)
    {
        if (!slot.bPending)
            continue;

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        if (m_pContext->GetData(slot.pDisjoint.Get(), &disjoint, sizeof(disjoint),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            continue;

        UINT64 beginTicks = 0;
        UINT64 endTicks = 0;
        if (m_pContext->GetData(slot.pBegin.Get(), &beginTicks, sizeof(beginTicks),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK ||
            m_pContext->GetData(slot.pEnd.Get(), &endTicks, sizeof(endTicks),
                D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK)
            continue;

        slot.bPending = false;

        if (!disjoint.Disjoint && disjoint.Frequency != 0 && endTicks > beginTicks)
        {
            const uint64_t gpuUs =
                (endTicks - beginTicks) * 1000000ull / disjoint.Frequency;
            WINTERS_PROFILE_GAUGE("GPU::FrameUs", gpuUs);
            WINTERS_PROFILE_GAUGE("GPU::SourceRHIFrame", slot.uSourceRHIFrame);
        }

        // Preserve one timing result per CPU frame. Multiple completed slots must never
        // be added together and reported as one GPU frame duration.
        return;
    }
}

unique_ptr<CDX11Device> CDX11Device::Create(const DeviceDesc& desc)
{
    unique_ptr<CDX11Device> pDevice(new CDX11Device());

    if (!pDevice->Initialize(desc))
    {
        OutputDebugStringA("[CDX11Device] Create() failed\n");
        return nullptr;
    }
    return pDevice;
}

void CDX11Device::BeginFrame(float32 r, float32 g, float32 b, float32 a)
{
    // 매 프레임 RT + 뷰포트 재설정 (DISCARD 스왑체인 안정성)
    m_pContext->OMSetRenderTargets(1, m_pRenderTargetView.GetAddressOf(), m_pDepthStencilView.Get());
    m_pContext->RSSetViewports(1, &m_Viewport);

#ifdef WINTERS_PROFILING
    ++m_uGpuTimingFrameSerial;
    if (m_bGpuTimingReady)
    {
        GpuTimingSlot& slot = m_GpuTimingSlots[m_uGpuTimingWriteIndex];
        if (!slot.bPending)
        {
            slot.uSourceRHIFrame = m_uGpuTimingFrameSerial;
            m_pContext->Begin(slot.pDisjoint.Get());
            m_pContext->End(slot.pBegin.Get());
        }
    }
#endif

    float clearColor[4] = { r, g, b, a };
    m_pContext->ClearRenderTargetView(m_pRenderTargetView.Get(), clearColor);
    m_pContext->ClearDepthStencilView(
        m_pDepthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.f, 0
    );
}

void CDX11Device::EndFrame()
{
#ifdef WINTERS_PROFILING
    if (m_bGpuTimingReady)
    {
        GpuTimingSlot& slot = m_GpuTimingSlots[m_uGpuTimingWriteIndex];
        if (!slot.bPending)
        {
            m_pContext->End(slot.pEnd.Get());
            m_pContext->End(slot.pDisjoint.Get());
            slot.bPending = true;
            m_uGpuTimingWriteIndex = (m_uGpuTimingWriteIndex + 1u) % kGpuTimingSlots;
        }
    }
#endif

    // SyncInterval: 1 = VSync, 0 = 즉시 표시
    HRESULT hr = m_pSwapChain->Present(m_bVSync ? 1 : 0, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        OutputDebugStringA("[CDX11Device] DEVICE REMOVED/RESET — 드라이버 재설치 후 재실행 필요\n");
    }
    else if (FAILED(hr))
    {
        OutputDebugStringA("[CDX11Device] Present FAILED — HRESULT 오류\n");
    }

#ifdef WINTERS_PROFILING
    if (m_bGpuTimingReady)
        ReadGpuTimingResults();
#endif
}

RHIPipelineHandle CDX11Device::CreatePipeline(const RHIPipelineDesc& desc)
{
    auto* pPipeline = new CDX11PipelineState(desc);

    // RHI 셰이더 핸들이 유효하면 네이티브 PSO까지 생성한다.
    // 핸들 없이 desc만 보관하는 기존(legacy) 호출자는 데이터 전용으로 유지된다.
    if (m_pTables)
    {
        CDX11ShaderObject* pVS = m_pTables->shaderTable.Lookup(desc.vsHandle);
        CDX11ShaderObject* pPS = m_pTables->shaderTable.Lookup(desc.psHandle);

        if (pVS && pPS &&
            pVS->stage == eRHIShaderStage::Vertex &&
            pPS->stage == eRHIShaderStage::Pixel)
        {
            if (!pPipeline->InitializeNative(m_pDevice.Get(), pVS, pPS))
            {
                delete pPipeline;
                return {};
            }
        }
    }

    return m_PipelineTable.Insert(pPipeline);
}

void CDX11Device::DestroyPipeline(RHIPipelineHandle handle)
{
    m_PipelineTable.Erase(handle);
}

RHIRenderPassHandle CDX11Device::CreateRenderPass(const RHIRenderPassDesc& desc)
{
    return m_RenderPassTable.Insert(new CDX11RenderPass(desc));
}

void CDX11Device::DestroyRenderPass(RHIRenderPassHandle handle)
{
    m_RenderPassTable.Erase(handle);
}

RHIBindGroupLayoutHandle CDX11Device::CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc)
{
    return m_BindGroupLayoutTable.Insert(new CDX11BindGroupLayout(desc));
}

void CDX11Device::DestroyBindGroupLayout(RHIBindGroupLayoutHandle handle)
{
    m_BindGroupLayoutTable.Erase(handle);
}

RHIBindGroupHandle CDX11Device::CreateBindGroup(const RHIBindGroupDesc& desc)
{
    if (!desc.layoutHandle.IsValid() || !m_BindGroupLayoutTable.Lookup(desc.layoutHandle))
        return {};

    return m_BindGroupTable.Insert(new CDX11BindGroup(desc));
}

void CDX11Device::DestroyBindGroup(RHIBindGroupHandle handle)
{
    m_BindGroupTable.Erase(handle);
}

void CDX11Device::UpdateBindGroup(RHIBindGroupHandle handle,
    const RHIBindGroupResource* resources,
    u32_t resourceCount)
{
    IRHIBindGroup* pGroup = m_BindGroupTable.Lookup(handle);
    auto* pDX11Group = dynamic_cast<CDX11BindGroup*>(pGroup);
    if (!pDX11Group)
        return;

    pDX11Group->UpdateResources(resources, resourceCount);
}

RHIBufferHandle CDX11Device::CreateBuffer(const RHIBufferDesc& desc, const void* pInitialData)
{
    if (!m_pDevice || !m_pTables || desc.sizeBytes == 0)
        return {};

    const bool_t bDynamic = desc.dynamic || desc.memoryUsage == eRHIMemoryUsage::Dynamic;

    D3D11_BUFFER_DESC bufferDesc{};
    bufferDesc.ByteWidth = desc.sizeBytes;
    if (desc.usage == eRHIBufferUsage::Constant)
        bufferDesc.ByteWidth = (bufferDesc.ByteWidth + 15u) & ~15u;
    bufferDesc.Usage = bDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    bufferDesc.CPUAccessFlags = bDynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    switch (desc.usage)
    {
    case eRHIBufferUsage::Index:
        bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        break;
    case eRHIBufferUsage::Constant:
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        break;
    case eRHIBufferUsage::Vertex:
    default:
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        break;
    }

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = pInitialData;

    CDX11BufferObject* pBuffer = new CDX11BufferObject();
    pBuffer->desc = desc;
    pBuffer->dynamic = bDynamic;

    const HRESULT hrBuffer = m_pDevice->CreateBuffer(
        &bufferDesc,
        pInitialData ? &initData : nullptr,
        pBuffer->pBuffer.GetAddressOf());
    if (FAILED(hrBuffer))
    {
        char msg[256]{};
        sprintf_s(msg, "[CDX11Device] FAIL: CreateBuffer hr=0x%08X size=%u name=%s\n",
            static_cast<unsigned>(hrBuffer),
            desc.sizeBytes,
            desc.debugName ? desc.debugName : "(unnamed)");
        OutputDebugStringA(msg);
        delete pBuffer;
        return {};
    }

    return m_pTables->bufferTable.Insert(pBuffer);
}

void CDX11Device::DestroyBuffer(RHIBufferHandle handle)
{
    if (m_pTables)
        m_pTables->bufferTable.Erase(handle);
}

void* CDX11Device::GetBufferNativeHandle(RHIBufferHandle handle, eNativeHandleType type)
{
    if (!m_pTables || type != eNativeHandleType::DX11Resource)
        return nullptr;

    CDX11BufferObject* pBuffer = m_pTables->bufferTable.Lookup(handle);
    return pBuffer ? pBuffer->pBuffer.Get() : nullptr;
}

RHIShaderHandle CDX11Device::CreateShader(
    eRHIShaderStage stage,
    const void* pBytecode,
    u32_t sizeBytes,
    const char* debugName)
{
    (void)debugName;

    if (!m_pDevice || !m_pTables || !pBytecode || sizeBytes == 0)
        return {};

    CDX11ShaderObject* pShader = new CDX11ShaderObject();
    pShader->stage = stage;
    pShader->bytecode.resize(sizeBytes);
    std::memcpy(pShader->bytecode.data(), pBytecode, sizeBytes);

    HRESULT hr = S_OK;
    if (stage == eRHIShaderStage::Vertex)
        hr = m_pDevice->CreateVertexShader(pBytecode, sizeBytes, nullptr, pShader->pVS.GetAddressOf());
    else if (stage == eRHIShaderStage::Pixel)
        hr = m_pDevice->CreatePixelShader(pBytecode, sizeBytes, nullptr, pShader->pPS.GetAddressOf());

    if (FAILED(hr))
    {
        delete pShader;
        return {};
    }

    return m_pTables->shaderTable.Insert(pShader);
}

void CDX11Device::DestroyShader(RHIShaderHandle handle)
{
    if (m_pTables)
        m_pTables->shaderTable.Erase(handle);
}

RHITextureHandle CDX11Device::CreateTexture(
    const RHITextureDesc& desc,
    const void* pInitialData,
    u32_t rowPitchBytes)
{
    if (!m_pDevice || !m_pTables || desc.width == 0 || desc.height == 0)
        return {};

    const DXGI_FORMAT format = ToDXGIFormat(desc.format);
    if (format == DXGI_FORMAT_UNKNOWN)
        return {};

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = desc.width;
    texDesc.Height = desc.height;
    texDesc.MipLevels = pInitialData ? 1u : (desc.mipLevels ? desc.mipLevels : 1u);
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = pInitialData;
    initData.SysMemPitch =
        rowPitchBytes ? rowPitchBytes : desc.width * BytesPerPixelOf(desc.format);

    CDX11TextureObject* pTexture = new CDX11TextureObject();
    pTexture->desc = desc;

    const HRESULT hrTexture = m_pDevice->CreateTexture2D(
        &texDesc,
        pInitialData ? &initData : nullptr,
        pTexture->pTexture.GetAddressOf());
    if (FAILED(hrTexture))
    {
        char msg[256]{};
        sprintf_s(msg, "[CDX11Device] FAIL: CreateTexture2D hr=0x%08X %ux%u name=%s\n",
            static_cast<unsigned>(hrTexture),
            desc.width,
            desc.height,
            desc.debugName ? desc.debugName : "(unnamed)");
        OutputDebugStringA(msg);
        delete pTexture;
        return {};
    }

    const HRESULT hrSRV = m_pDevice->CreateShaderResourceView(
        pTexture->pTexture.Get(),
        nullptr,
        pTexture->pSRV.GetAddressOf());
    if (FAILED(hrSRV))
    {
        char msg[256]{};
        sprintf_s(msg, "[CDX11Device] FAIL: CreateShaderResourceView hr=0x%08X name=%s\n",
            static_cast<unsigned>(hrSRV),
            desc.debugName ? desc.debugName : "(unnamed)");
        OutputDebugStringA(msg);
        delete pTexture;
        return {};
    }

    return m_pTables->textureTable.Insert(pTexture);
}

void CDX11Device::DestroyTexture(RHITextureHandle handle)
{
    if (m_pTables)
        m_pTables->textureTable.Erase(handle);
}

void* CDX11Device::GetTextureNativeHandle(RHITextureHandle handle, eNativeHandleType type)
{
    if (!m_pTables || type != eNativeHandleType::DX11Resource)
        return nullptr;

    CDX11TextureObject* pTexture = m_pTables->textureTable.Lookup(handle);
    return pTexture ? pTexture->pTexture.Get() : nullptr;
}

RHISamplerHandle CDX11Device::CreateSampler(const RHISamplerDesc& desc)
{
    if (!m_pDevice || !m_pTables)
        return {};

    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = ToDX11Filter(desc.filter);
    samplerDesc.AddressU = ToDX11AddressMode(desc.addressU);
    samplerDesc.AddressV = ToDX11AddressMode(desc.addressV);
    samplerDesc.AddressW = ToDX11AddressMode(desc.addressW);
    samplerDesc.MaxAnisotropy = desc.maxAnisotropy > 1 ? desc.maxAnisotropy : 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0.f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    CDX11SamplerObject* pSampler = new CDX11SamplerObject();
    pSampler->desc = desc;

    if (FAILED(m_pDevice->CreateSamplerState(&samplerDesc, pSampler->pSampler.GetAddressOf())))
    {
        delete pSampler;
        return {};
    }

    return m_pTables->samplerTable.Insert(pSampler);
}

void CDX11Device::DestroySampler(RHISamplerHandle handle)
{
    if (m_pTables)
        m_pTables->samplerTable.Erase(handle);
}
