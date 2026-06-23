#include "WintersPCH.h"
#include "RHI/DX12/DX12Device.h"

#include "RHI/CRHIResourceTable.h"
#include "RHI/IRHIBindGroup.h"
#include "RHI/IRHIPipelineState.h"
#include "RHI/IRHIRenderPass.h"

#include <cstdio>
#include <cstring>
#include <d3dcompiler.h>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

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

    Microsoft::WRL::ComPtr<IDXGIAdapter1> SelectHighPerformanceDX12Adapter(IDXGIFactory4* pFactory)
    {
        if (!pFactory)
            return nullptr;

        Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory6;

        if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&pFactory6))))
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

                if (!IsHardwareAdapter(pAdapter.Get()))
                    continue;

                if (SUCCEEDED(D3D12CreateDevice(
                    pAdapter.Get(),
                    D3D_FEATURE_LEVEL_12_0,
                    __uuidof(ID3D12Device),
                    nullptr)))
                {
                    return pAdapter;
                }
            }
        }

        for (UINT i = 0; ; ++i)
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
            if (FAILED(pFactory->EnumAdapters1(i, &pAdapter)))
                break;

            if (!IsHardwareAdapter(pAdapter.Get()))
                continue;

            if (SUCCEEDED(D3D12CreateDevice(
                pAdapter.Get(),
                D3D_FEATURE_LEVEL_12_0,
                __uuidof(ID3D12Device),
                nullptr)))
            {
                return pAdapter;
            }
        }

        return nullptr;
    }

    void LogDX12Adapter(IDXGIAdapter1* pAdapter)
    {
        if (!pAdapter)
        {
            OutputDebugStringA("[CDX12Device] DX12 adapter: default hardware adapter\n");
            return;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(pAdapter->GetDesc1(&desc)))
        {
            OutputDebugStringA("[CDX12Device] DX12 adapter: GetDesc1 failed\n");
            return;
        }

        char adapterName[256]{};
        WideCharToMultiByte(CP_ACP, 0, desc.Description, -1,
            adapterName, static_cast<int>(sizeof(adapterName)), nullptr, nullptr);

        char log[512]{};
        sprintf_s(log,
            "[CDX12Device] DX12 adapter: %s (%llu MB dedicated VRAM)\n",
            adapterName[0] ? adapterName : "unknown",
            static_cast<unsigned long long>(desc.DedicatedVideoMemory / (1024ull * 1024ull)));
        OutputDebugStringA(log);
    }

    D3D12_RESOURCE_BARRIER MakeTransitionBarrier(
        ID3D12Resource* pResource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = pResource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        return barrier;
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

    D3D12_HEAP_PROPERTIES MakeHeapProperties(D3D12_HEAP_TYPE type)
    {
        D3D12_HEAP_PROPERTIES props{};
        props.Type = type;
        props.CreationNodeMask = 1;
        props.VisibleNodeMask = 1;
        return props;
    }

    D3D12_RESOURCE_DESC MakeBufferResourceDesc(u64_t sizeBytes)
    {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = sizeBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        return desc;
    }

    D3D12_RESOURCE_STATES ToInitialBufferState(eRHIBufferUsage usage)
    {
        switch (usage)
        {
        case eRHIBufferUsage::Index:
            return D3D12_RESOURCE_STATE_INDEX_BUFFER;
        case eRHIBufferUsage::Vertex:
        case eRHIBufferUsage::Constant:
            return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        default:
            return D3D12_RESOURCE_STATE_COMMON;
        }
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12TopologyType(eRHIPrimitiveTopology topology)
    {
        switch (topology)
        {
        case eRHIPrimitiveTopology::TriangleList:
        case eRHIPrimitiveTopology::TriangleStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case eRHIPrimitiveTopology::LineList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case eRHIPrimitiveTopology::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
        }
    }

    D3D_PRIMITIVE_TOPOLOGY ToDX12PrimitiveTopology(eRHIPrimitiveTopology topology)
    {
        switch (topology)
        {
        case eRHIPrimitiveTopology::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case eRHIPrimitiveTopology::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case eRHIPrimitiveTopology::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case eRHIPrimitiveTopology::TriangleList:
        default:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }

    D3D12_CULL_MODE ToDX12CullMode(eRHICullMode mode)
    {
        switch (mode)
        {
        case eRHICullMode::None: return D3D12_CULL_MODE_NONE;
        case eRHICullMode::Front: return D3D12_CULL_MODE_FRONT;
        case eRHICullMode::Back:
        default:
            return D3D12_CULL_MODE_BACK;
        }
    }

    D3D12_COMPARISON_FUNC ToDX12ComparisonFunc(eRHIDepthOp op)
    {
        switch (op)
        {
        case eRHIDepthOp::Less: return D3D12_COMPARISON_FUNC_LESS;
        case eRHIDepthOp::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case eRHIDepthOp::Greater: return D3D12_COMPARISON_FUNC_GREATER;
        case eRHIDepthOp::Always: return D3D12_COMPARISON_FUNC_ALWAYS;
        case eRHIDepthOp::Never:
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
        }
    }

    D3D12_SHADER_VISIBILITY ToDX12ShaderVisibility(eRHIShaderVisibility visibility)
    {
        const u32_t value = static_cast<u32_t>(visibility);

        if (value == static_cast<u32_t>(eRHIShaderVisibility::Vertex))
            return D3D12_SHADER_VISIBILITY_VERTEX;
        if (value == static_cast<u32_t>(eRHIShaderVisibility::Pixel))
            return D3D12_SHADER_VISIBILITY_PIXEL;
        if (value == static_cast<u32_t>(eRHIShaderVisibility::Compute))
            return D3D12_SHADER_VISIBILITY_ALL;

        return D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_SHADER_VISIBILITY ToDX12TableVisibility(u32_t visibilityMask)
    {
        if (visibilityMask == static_cast<u32_t>(eRHIShaderVisibility::Vertex))
            return D3D12_SHADER_VISIBILITY_VERTEX;
        if (visibilityMask == static_cast<u32_t>(eRHIShaderVisibility::Pixel))
            return D3D12_SHADER_VISIBILITY_PIXEL;

        return D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_FILTER ToDX12Filter(eRHIFilter filter)
    {
        switch (filter)
        {
        case eRHIFilter::Point: return D3D12_FILTER_MIN_MAG_MIP_POINT;
        case eRHIFilter::Anisotropic: return D3D12_FILTER_ANISOTROPIC;
        case eRHIFilter::Linear:
        default:
            return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }

    D3D12_TEXTURE_ADDRESS_MODE ToDX12AddressMode(eRHIAddressMode mode)
    {
        switch (mode)
        {
        case eRHIAddressMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case eRHIAddressMode::Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case eRHIAddressMode::Wrap:
        default:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        }
    }

    D3D12_RASTERIZER_DESC MakeRasterizerDesc(eRHICullMode cullMode)
    {
        D3D12_RASTERIZER_DESC desc{};
        desc.FillMode = D3D12_FILL_MODE_SOLID;
        desc.CullMode = ToDX12CullMode(cullMode);
        desc.FrontCounterClockwise = FALSE;
        desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        desc.DepthClipEnable = TRUE;
        desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        return desc;
    }

    D3D12_BLEND_DESC MakeBlendDesc(eRHIBlendMode blendMode)
    {
        D3D12_BLEND_DESC desc{};

        D3D12_RENDER_TARGET_BLEND_DESC rt{};
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        if (blendMode == eRHIBlendMode::AlphaBlend)
        {
            rt.BlendEnable = TRUE;
            rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            rt.BlendOp = D3D12_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;
            rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
            rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }
        else if (blendMode == eRHIBlendMode::Additive)
        {
            rt.BlendEnable = TRUE;
            rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
            rt.DestBlend = D3D12_BLEND_ONE;
            rt.BlendOp = D3D12_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;
            rt.DestBlendAlpha = D3D12_BLEND_ONE;
            rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }
        else
        {
            rt.BlendEnable = FALSE;
            rt.SrcBlend = D3D12_BLEND_ONE;
            rt.DestBlend = D3D12_BLEND_ZERO;
            rt.BlendOp = D3D12_BLEND_OP_ADD;
            rt.SrcBlendAlpha = D3D12_BLEND_ONE;
            rt.DestBlendAlpha = D3D12_BLEND_ZERO;
            rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        }

        rt.LogicOp = D3D12_LOGIC_OP_NOOP;

        for (auto& target : desc.RenderTarget)
            target = rt;

        return desc;
    }

    D3D12_DEPTH_STENCIL_DESC MakeDepthStencilDesc(const RHIPipelineDesc& desc)
    {
        D3D12_DEPTH_STENCIL_DESC ds{};
        ds.DepthEnable = desc.dsvFormat != eRHIFormat::Unknown;
        ds.DepthWriteMask = desc.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc = ToDX12ComparisonFunc(desc.depthOp);
        ds.StencilEnable = FALSE;
        ds.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        ds.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        return ds;
    }

    struct CDX12Buffer
    {
        RHIBufferDesc desc{};
        Microsoft::WRL::ComPtr<ID3D12Resource> pResource;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> frameResources;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
        bool_t uploadHeap = false;
    };

    ID3D12Resource* GetDX12BufferResource(CDX12Buffer& buffer, u32_t frameIndex)
    {
        if (buffer.uploadHeap &&
            frameIndex < buffer.frameResources.size() &&
            buffer.frameResources[frameIndex])
        {
            return buffer.frameResources[frameIndex].Get();
        }

        return buffer.pResource.Get();
    }

    struct CDX12Shader
    {
        eRHIShaderStage stage = eRHIShaderStage::Vertex;
        std::vector<u8_t> bytecode;
        std::string debugName;
    };

    struct CDX12Texture
    {
        RHITextureDesc desc{};
        Microsoft::WRL::ComPtr<ID3D12Resource> pResource;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    };

    struct CDX12SamplerObject
    {
        RHISamplerDesc desc{};
    };

    u32_t CountLayoutSlots(const RHIBindGroupLayoutDesc& layoutDesc, eRHIBindingType type)
    {
        u32_t count = 0;
        for (u32_t i = 0; i < layoutDesc.slotCount; ++i)
        {
            if (layoutDesc.slots[i].type == type)
                ++count;
        }
        return count;
    }

    const RHIBindGroupResource* FindBindGroupResource(
        const RHIBindGroupDesc& groupDesc,
        u32_t slot,
        eRHIBindingType type)
    {
        for (u32_t i = 0; i < groupDesc.resourceCount; ++i)
        {
            if (groupDesc.resources[i].slot == slot && groupDesc.resources[i].type == type)
                return &groupDesc.resources[i];
        }
        return nullptr;
    }

    enum class eDX12RootBindingKind : u32_t
    {
        RootConstantBuffer,
        SrvTable,
        SamplerTable,
    };

    struct DX12RootBinding
    {
        eDX12RootBindingKind kind = eDX12RootBindingKind::RootConstantBuffer;
        u32_t bindGroupSlot = 0;
        u32_t bindingSlot = 0;
        eRHIBindingType type = eRHIBindingType::ConstantBuffer;
        u32_t rootParameterIndex = 0;
    };

    class CDX12PipelineState final : public IRHIPipelineState
    {
    public:
        CDX12PipelineState(
            const RHIPipelineDesc& desc,
            Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature,
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState,
            D3D_PRIMITIVE_TOPOLOGY primitiveTopology,
            std::vector<DX12RootBinding> rootBindings)
            : m_Desc(desc)
            , m_pRootSignature(std::move(pRootSignature))
            , m_pPipelineState(std::move(pPipelineState))
            , m_PrimitiveTopology(primitiveTopology)
            , m_RootBindings(std::move(rootBindings))
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

        void* GetNativeHandle(eNativeHandleType type) override
        {
            if (type == eNativeHandleType::DX12Resource)
                return m_pPipelineState.Get();

            return nullptr;
        }

        ID3D12RootSignature* GetRootSignature() const { return m_pRootSignature.Get(); }
        ID3D12PipelineState* GetPipelineState() const { return m_pPipelineState.Get(); }
        D3D_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const { return m_PrimitiveTopology; }
        const DX12RootBinding* FindRootBinding(
            u32_t bindGroupSlot,
            u32_t bindingSlot,
            eRHIBindingType type) const
        {
            for (const DX12RootBinding& binding : m_RootBindings)
            {
                if (binding.kind == eDX12RootBindingKind::RootConstantBuffer &&
                    binding.bindGroupSlot == bindGroupSlot &&
                    binding.bindingSlot == bindingSlot &&
                    binding.type == type)
                {
                    return &binding;
                }
            }

            return nullptr;
        }

        i32_t FindTableRootIndex(u32_t bindGroupSlot, eDX12RootBindingKind kind) const
        {
            for (const DX12RootBinding& binding : m_RootBindings)
            {
                if (binding.kind == kind && binding.bindGroupSlot == bindGroupSlot)
                    return static_cast<i32_t>(binding.rootParameterIndex);
            }

            return -1;
        }

    private:
        RHIPipelineDesc m_Desc{};
        std::vector<RHIInputElementDesc> m_InputElements;
        Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pPipelineState;
        D3D_PRIMITIVE_TOPOLOGY m_PrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        std::vector<DX12RootBinding> m_RootBindings;
    };

    class CDX12RenderPass final : public IRHIRenderPass
    {
    public:
        explicit CDX12RenderPass(const RHIRenderPassDesc& desc)
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

    class CDX12BindGroupLayout final : public IRHIBindGroupLayout
    {
    public:
        explicit CDX12BindGroupLayout(const RHIBindGroupLayoutDesc& desc)
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

    class CDX12BindGroup final : public IRHIBindGroup
    {
    public:
        explicit CDX12BindGroup(const RHIBindGroupDesc& desc)
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

        // shader-visible heap에서 이 그룹이 소유한 descriptor range.
        u32_t srvBaseIndex = 0;
        u32_t srvCount = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE srvTableGpu{};
        u32_t samplerBaseIndex = 0;
        u32_t samplerCount = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE samplerTableGpu{};

    private:
        RHIBindGroupDesc m_Desc{};
        std::vector<RHIBindGroupResource> m_Resources;
    };
}

struct CDX12Device::ResourceTables
{
    CRHIResourceTable<CDX12Buffer, RHIBufferTag> bufferTable;
    CRHIResourceTable<CDX12Shader, RHIShaderTag> shaderTable;
    CRHIResourceTable<CDX12Texture, RHITextureTag> textureTable;
    CRHIResourceTable<CDX12SamplerObject, RHISamplerTag> samplerTable;
    CRHIResourceTable<IRHIPipelineState, RHIPipelineTag> pipelineTable;
    CRHIResourceTable<IRHIRenderPass, RHIRenderPassTag> renderPassTable;
    CRHIResourceTable<IRHIBindGroupLayout, RHIBindGroupLayoutTag> bindGroupLayoutTable;
    CRHIResourceTable<IRHIBindGroup, RHIBindGroupTag> bindGroupTable;
};

// BindGroup이 descriptor-set처럼 shader-visible heap의 연속 range를 영속 소유한다.
struct CDX12Device::DescriptorHeaps
{
    struct Range
    {
        u32_t base = 0;
        u32_t count = 0;
    };

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pSamplerHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pSrvStagingHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pSamplerStagingHeap;
    u32_t srvIncrement = 0;
    u32_t samplerIncrement = 0;
    u32_t srvNext = 0;
    u32_t samplerNext = 0;
    u32_t srvRingNext[CDX12Device::kFrameCount] = {};
    u32_t samplerRingNext[CDX12Device::kFrameCount] = {};
    u32_t srvCapacity = 0;
    u32_t samplerCapacity = 0;
    u32_t srvRingCapacity = 0;
    u32_t samplerRingCapacity = 0;
    std::vector<Range> srvFreeRanges;
    std::vector<Range> samplerFreeRanges;

    static bool_t AllocFrom(
        std::vector<Range>& freeRanges,
        u32_t& next,
        u32_t capacity,
        u32_t count,
        u32_t& outBase)
    {
        for (size_t i = 0; i < freeRanges.size(); ++i)
        {
            if (freeRanges[i].count >= count)
            {
                outBase = freeRanges[i].base;
                freeRanges[i].base += count;
                freeRanges[i].count -= count;
                if (freeRanges[i].count == 0)
                    freeRanges.erase(freeRanges.begin() + i);
                return true;
            }
        }

        if (next + count > capacity)
            return false;

        outBase = next;
        next += count;
        return true;
    }

    bool_t AllocSrv(u32_t count, u32_t& outBase)
    {
        return AllocFrom(srvFreeRanges, srvNext, srvCapacity, count, outBase);
    }

    void FreeSrv(u32_t base, u32_t count)
    {
        srvFreeRanges.push_back({ base, count });
    }

    bool_t AllocSampler(u32_t count, u32_t& outBase)
    {
        return AllocFrom(samplerFreeRanges, samplerNext, samplerCapacity, count, outBase);
    }

    void FreeSampler(u32_t base, u32_t count)
    {
        samplerFreeRanges.push_back({ base, count });
    }

    void ResetFrame(u32_t frameIndex)
    {
        if (frameIndex >= CDX12Device::kFrameCount)
            return;

        srvRingNext[frameIndex] = 0;
        samplerRingNext[frameIndex] = 0;
    }

    bool_t AllocFrameSrv(u32_t frameIndex, u32_t count, u32_t& outBase)
    {
        if (frameIndex >= CDX12Device::kFrameCount || srvRingNext[frameIndex] + count > srvRingCapacity)
            return false;

        outBase = frameIndex * srvRingCapacity + srvRingNext[frameIndex];
        srvRingNext[frameIndex] += count;
        return true;
    }

    bool_t AllocFrameSampler(u32_t frameIndex, u32_t count, u32_t& outBase)
    {
        if (frameIndex >= CDX12Device::kFrameCount || samplerRingNext[frameIndex] + count > samplerRingCapacity)
            return false;

        outBase = frameIndex * samplerRingCapacity + samplerRingNext[frameIndex];
        samplerRingNext[frameIndex] += count;
        return true;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SrvStagingCpuAt(u32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = pSrvStagingHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index) * srvIncrement;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SrvRingCpuAt(u32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = pSrvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index) * srvIncrement;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE SrvRingGpuAt(u32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = pSrvHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<UINT64>(index) * srvIncrement;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SamplerStagingCpuAt(u32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = pSamplerStagingHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index) * samplerIncrement;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE SamplerRingCpuAt(u32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = pSamplerHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<SIZE_T>(index) * samplerIncrement;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE SamplerRingGpuAt(u32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = pSamplerHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<UINT64>(index) * samplerIncrement;
        return handle;
    }
};

class CDX12FrameCommandList final : public IRHICommandList
{
public:
    explicit CDX12FrameCommandList(CDX12Device& owner)
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

        if (!m_Owner.m_pTables || !m_Owner.m_pCommandList)
            return;

        IRHIPipelineState* pPipelineBase = m_Owner.m_pTables->pipelineTable.Lookup(handle);
        auto* pPipeline = dynamic_cast<CDX12PipelineState*>(pPipelineBase);
        if (!pPipeline)
            return;

        m_Owner.m_pCommandList->SetGraphicsRootSignature(pPipeline->GetRootSignature());
        m_Owner.m_pCommandList->SetPipelineState(pPipeline->GetPipelineState());
        m_Owner.m_pCommandList->IASetPrimitiveTopology(pPipeline->GetPrimitiveTopology());

        m_pCurrentPipeline = pPipeline;
    }

    void SetBindGroup(u32_t slot, RHIBindGroupHandle handle) override
    {
        if (!m_Owner.m_pTables ||
            !m_Owner.m_pHeaps ||
            !m_Owner.m_pDevice ||
            !m_Owner.m_pCommandList ||
            !m_pCurrentPipeline)
        {
            return;
        }

        IRHIBindGroup* pBindGroupBase = m_Owner.m_pTables->bindGroupTable.Lookup(handle);
        auto* pBindGroup = dynamic_cast<CDX12BindGroup*>(pBindGroupBase);
        if (!pBindGroup)
            return;

        const RHIBindGroupDesc& desc = pBindGroup->GetDesc();
        for (u32_t i = 0; i < desc.resourceCount; ++i)
        {
            const RHIBindGroupResource& resource = desc.resources[i];
            const DX12RootBinding* pBinding =
                m_pCurrentPipeline->FindRootBinding(slot, resource.slot, resource.type);

            if (!pBinding)
                continue;

            if (resource.type == eRHIBindingType::ConstantBuffer)
            {
                CDX12Buffer* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(resource.bufferHandle);
                ID3D12Resource* pResource = pBuffer
                    ? GetDX12BufferResource(*pBuffer, m_Owner.m_iFrameIndex)
                    : nullptr;
                if (!pResource)
                    continue;

                m_Owner.m_pCommandList->SetGraphicsRootConstantBufferView(
                    pBinding->rootParameterIndex,
                    pResource->GetGPUVirtualAddress());
            }
        }

        const i32_t srvTableIndex =
            m_pCurrentPipeline->FindTableRootIndex(slot, eDX12RootBindingKind::SrvTable);
        if (srvTableIndex >= 0 && pBindGroup->srvCount > 0)
        {
            u32_t ringBase = 0;
            if (!m_Owner.m_pHeaps->AllocFrameSrv(
                m_Owner.m_iFrameIndex,
                pBindGroup->srvCount,
                ringBase))
            {
                return;
            }

            m_Owner.m_pDevice->CopyDescriptorsSimple(
                pBindGroup->srvCount,
                m_Owner.m_pHeaps->SrvRingCpuAt(ringBase),
                m_Owner.m_pHeaps->SrvStagingCpuAt(pBindGroup->srvBaseIndex),
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            m_Owner.m_pCommandList->SetGraphicsRootDescriptorTable(
                static_cast<UINT>(srvTableIndex),
                m_Owner.m_pHeaps->SrvRingGpuAt(ringBase));
        }

        const i32_t samplerTableIndex =
            m_pCurrentPipeline->FindTableRootIndex(slot, eDX12RootBindingKind::SamplerTable);
        if (samplerTableIndex >= 0 && pBindGroup->samplerCount > 0)
        {
            u32_t ringBase = 0;
            if (!m_Owner.m_pHeaps->AllocFrameSampler(
                m_Owner.m_iFrameIndex,
                pBindGroup->samplerCount,
                ringBase))
            {
                return;
            }

            m_Owner.m_pDevice->CopyDescriptorsSimple(
                pBindGroup->samplerCount,
                m_Owner.m_pHeaps->SamplerRingCpuAt(ringBase),
                m_Owner.m_pHeaps->SamplerStagingCpuAt(pBindGroup->samplerBaseIndex),
                D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

            m_Owner.m_pCommandList->SetGraphicsRootDescriptorTable(
                static_cast<UINT>(samplerTableIndex),
                m_Owner.m_pHeaps->SamplerRingGpuAt(ringBase));
        }
    }

    void SetVertexBuffer(u32_t slot, RHIBufferHandle handle, u32_t stride, u32_t offset) override
    {
        if (!m_Owner.m_pTables || !m_Owner.m_pCommandList)
            return;

        CDX12Buffer* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(handle);
        ID3D12Resource* pResource = pBuffer
            ? GetDX12BufferResource(*pBuffer, m_Owner.m_iFrameIndex)
            : nullptr;
        if (!pResource)
            return;

        D3D12_VERTEX_BUFFER_VIEW view{};
        view.BufferLocation = pResource->GetGPUVirtualAddress() + offset;
        view.StrideInBytes = stride;
        view.SizeInBytes = pBuffer->desc.sizeBytes > offset ? pBuffer->desc.sizeBytes - offset : 0;

        m_Owner.m_pCommandList->IASetVertexBuffers(slot, 1, &view);
    }

    void SetIndexBuffer(RHIBufferHandle handle, u32_t offset, eRHIFormat indexFormat) override
    {
        if (!m_Owner.m_pTables || !m_Owner.m_pCommandList)
            return;

        CDX12Buffer* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(handle);
        ID3D12Resource* pResource = pBuffer
            ? GetDX12BufferResource(*pBuffer, m_Owner.m_iFrameIndex)
            : nullptr;
        if (!pResource)
            return;

        D3D12_INDEX_BUFFER_VIEW view{};
        view.BufferLocation = pResource->GetGPUVirtualAddress() + offset;
        view.Format = ToDXGIFormat(indexFormat);
        view.SizeInBytes = pBuffer->desc.sizeBytes > offset ? pBuffer->desc.sizeBytes - offset : 0;

        m_Owner.m_pCommandList->IASetIndexBuffer(&view);
    }

    void Draw(u32_t vertexCount, u32_t instanceCount, u32_t firstVertex, u32_t firstInstance) override
    {
        if (m_Owner.m_pCommandList)
            m_Owner.m_pCommandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void DrawIndexed(
        u32_t indexCount,
        u32_t instanceCount,
        u32_t firstIndex,
        i32_t baseVertex,
        u32_t firstInstance) override
    {
        if (m_Owner.m_pCommandList)
            m_Owner.m_pCommandList->DrawIndexedInstanced(
                indexCount,
                instanceCount,
                firstIndex,
                baseVertex,
                firstInstance);
    }

    void Dispatch(u32_t, u32_t, u32_t) override {}

    void UpdateBuffer(RHIBufferHandle handle, const void* pData, u32_t sizeBytes) override
    {
        if (!m_Owner.m_pTables || !pData || sizeBytes == 0)
            return;

        CDX12Buffer* pBuffer = m_Owner.m_pTables->bufferTable.Lookup(handle);
        ID3D12Resource* pResource = pBuffer
            ? GetDX12BufferResource(*pBuffer, m_Owner.m_iFrameIndex)
            : nullptr;
        if (!pBuffer || !pResource || !pBuffer->uploadHeap)
            return;

        void* pMapped = nullptr;
        D3D12_RANGE readRange{ 0, 0 };
        if (FAILED(pResource->Map(0, &readRange, &pMapped)))
            return;

        const u32_t copyBytes = sizeBytes < pBuffer->desc.sizeBytes ? sizeBytes : pBuffer->desc.sizeBytes;
        std::memcpy(pMapped, pData, copyBytes);
        pResource->Unmap(0, nullptr);
    }

    void TransitionResource(RHIBufferHandle, eRHIResourceState) override {}
    void TransitionResource(RHITextureHandle, eRHIResourceState) override {}

    void* GetNativeHandle(eNativeHandleType type) const override
    {
        if (type == eNativeHandleType::DX12CommandList)
            return m_Owner.m_pCommandList.Get();

        return nullptr;
    }

private:
    CDX12Device& m_Owner;
    CDX12PipelineState* m_pCurrentPipeline = nullptr;
};

CDX12Device::CDX12Device()
    : m_pTables(new ResourceTables())
    , m_pHeaps(new DescriptorHeaps())
    , m_pFrameCommandList(new CDX12FrameCommandList(*this))
{
}

CDX12Device::~CDX12Device()
{
    WaitForGpu();

    if (m_hFenceEvent)
    {
        CloseHandle(m_hFenceEvent);
        m_hFenceEvent = nullptr;
    }
}

std::unique_ptr<CDX12Device> CDX12Device::Create(const DX12DeviceDesc& desc)
{
    std::unique_ptr<CDX12Device> pDevice(new CDX12Device());
    if (!pDevice->Initialize(desc))
        return nullptr;
    return pDevice;
}

void* CDX12Device::GetNativeHandle(eNativeHandleType type) const
{
    switch (type)
    {
    case eNativeHandleType::DX12Device:
        return m_pDevice.Get();
    case eNativeHandleType::DX12CommandQueue:
        return m_pCommandQueue.Get();
    case eNativeHandleType::DX12SwapChain:
        return m_pSwapChain.Get();
    case eNativeHandleType::DX12CommandList:
        return m_pCommandList.Get();
    default:
        return nullptr;
    }
}

IRHICommandList* CDX12Device::GetFrameCommandList()
{
    return m_pFrameCommandList.get();
}

RHIBufferHandle CDX12Device::CreateBuffer(const RHIBufferDesc& desc, const void* pInitialData)
{
    if (!m_pDevice || !m_pTables || desc.sizeBytes == 0)
        return {};

    CDX12Buffer* pBuffer = new CDX12Buffer();
    pBuffer->desc = desc;

    const bool_t useUploadHeap =
        desc.dynamic ||
        desc.memoryUsage == eRHIMemoryUsage::Dynamic ||
        // Frame-time buffer creation must not reset the open frame command list.
        m_bFrameRecording;

    if (useUploadHeap)
    {
        D3D12_HEAP_PROPERTIES heapProps = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC resourceDesc = MakeBufferResourceDesc(desc.sizeBytes);

        pBuffer->frameResources.resize(kFrameCount);
        for (u32_t i = 0; i < kFrameCount; ++i)
        {
            if (FAILED(m_pDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&pBuffer->frameResources[i]))))
            {
                delete pBuffer;
                return {};
            }

            if (pInitialData)
            {
                void* pMapped = nullptr;
                D3D12_RANGE readRange{ 0, 0 };
                if (SUCCEEDED(pBuffer->frameResources[i]->Map(0, &readRange, &pMapped)))
                {
                    std::memcpy(pMapped, pInitialData, desc.sizeBytes);
                    pBuffer->frameResources[i]->Unmap(0, nullptr);
                }
            }
        }

        pBuffer->pResource = pBuffer->frameResources[0];
        pBuffer->state = D3D12_RESOURCE_STATE_GENERIC_READ;
        pBuffer->uploadHeap = true;

        return m_pTables->bufferTable.Insert(pBuffer);
    }

    const D3D12_RESOURCE_STATES finalState = ToInitialBufferState(desc.usage);
    const D3D12_RESOURCE_STATES initialState =
        pInitialData ? D3D12_RESOURCE_STATE_COPY_DEST : finalState;

    D3D12_HEAP_PROPERTIES heapProps = MakeHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC resourceDesc = MakeBufferResourceDesc(desc.sizeBytes);

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&pBuffer->pResource))))
    {
        delete pBuffer;
        return {};
    }

    pBuffer->state = initialState;

    if (pInitialData)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> pUpload;
        D3D12_HEAP_PROPERTIES uploadHeapProps = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

        if (FAILED(m_pDevice->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pUpload))))
        {
            delete pBuffer;
            return {};
        }

        void* pMapped = nullptr;
        D3D12_RANGE readRange{ 0, 0 };
        if (FAILED(pUpload->Map(0, &readRange, &pMapped)))
        {
            delete pBuffer;
            return {};
        }

        std::memcpy(pMapped, pInitialData, desc.sizeBytes);
        pUpload->Unmap(0, nullptr);

        WaitForFrame(m_iFrameIndex);
        m_pCommandAllocators[m_iFrameIndex]->Reset();
        m_pCommandList->Reset(m_pCommandAllocators[m_iFrameIndex].Get(), nullptr);
        m_pCommandList->CopyBufferRegion(pBuffer->pResource.Get(), 0, pUpload.Get(), 0, desc.sizeBytes);

        D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
            pBuffer->pResource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            finalState);
        m_pCommandList->ResourceBarrier(1, &barrier);

        m_pCommandList->Close();

        ID3D12CommandList* ppLists[] = { m_pCommandList.Get() };
        m_pCommandQueue->ExecuteCommandLists(1, ppLists);
        WaitForGpu();

        pBuffer->state = finalState;
    }

    return m_pTables->bufferTable.Insert(pBuffer);
}

void CDX12Device::DestroyBuffer(RHIBufferHandle handle)
{
    if (m_pTables)
        m_pTables->bufferTable.Erase(handle);
}

void* CDX12Device::GetBufferNativeHandle(RHIBufferHandle handle, eNativeHandleType type)
{
    if (!m_pTables || type != eNativeHandleType::DX12Resource)
        return nullptr;

    CDX12Buffer* pBuffer = m_pTables->bufferTable.Lookup(handle);
    return pBuffer ? GetDX12BufferResource(*pBuffer, m_iFrameIndex) : nullptr;
}

RHIShaderHandle CDX12Device::CreateShader(
    eRHIShaderStage stage,
    const void* pBytecode,
    u32_t sizeBytes,
    const char* debugName)
{
    if (!m_pTables || !pBytecode || sizeBytes == 0)
        return {};

    CDX12Shader* pShader = new CDX12Shader();
    pShader->stage = stage;
    pShader->bytecode.resize(sizeBytes);
    std::memcpy(pShader->bytecode.data(), pBytecode, sizeBytes);

    if (debugName)
        pShader->debugName = debugName;

    return m_pTables->shaderTable.Insert(pShader);
}

void CDX12Device::DestroyShader(RHIShaderHandle handle)
{
    if (m_pTables)
        m_pTables->shaderTable.Erase(handle);
}

RHITextureHandle CDX12Device::CreateTexture(
    const RHITextureDesc& desc,
    const void* pInitialData,
    u32_t rowPitchBytes)
{
    if (!m_pDevice || !m_pTables || desc.width == 0 || desc.height == 0)
        return {};

    if (m_bFrameRecording && pInitialData)
    {
        OutputDebugStringA(
            "[CDX12Device] CreateTexture with initial data is not allowed during frame recording; "
            "queue/cook the texture before BeginFrame or add a dedicated upload command path.\n");
        return {};
    }

    const DXGI_FORMAT format = ToDXGIFormat(desc.format);
    if (format == DXGI_FORMAT_UNKNOWN)
        return {};

    CDX12Texture* pTexture = new CDX12Texture();
    pTexture->desc = desc;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = desc.width;
    resourceDesc.Height = desc.height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = static_cast<UINT16>(pInitialData ? 1u : (desc.mipLevels ? desc.mipLevels : 1u));
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    const D3D12_RESOURCE_STATES finalState =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    const D3D12_RESOURCE_STATES initialState =
        pInitialData ? D3D12_RESOURCE_STATE_COPY_DEST : finalState;

    D3D12_HEAP_PROPERTIES heapProps = MakeHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&pTexture->pResource))))
    {
        delete pTexture;
        return {};
    }

    pTexture->state = initialState;

    if (pInitialData)
    {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 uploadSize = 0;
        m_pDevice->GetCopyableFootprints(
            &resourceDesc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &uploadSize);

        Microsoft::WRL::ComPtr<ID3D12Resource> pUpload;
        D3D12_HEAP_PROPERTIES uploadHeapProps = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC uploadDesc = MakeBufferResourceDesc(uploadSize);

        if (FAILED(m_pDevice->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&pUpload))))
        {
            delete pTexture;
            return {};
        }

        u8_t* pMapped = nullptr;
        D3D12_RANGE readRange{ 0, 0 };
        if (FAILED(pUpload->Map(0, &readRange, reinterpret_cast<void**>(&pMapped))))
        {
            delete pTexture;
            return {};
        }

        const u32_t srcRowPitch = rowPitchBytes ? rowPitchBytes : static_cast<u32_t>(rowSizeInBytes);
        const u64_t copyBytesPerRow =
            rowSizeInBytes < srcRowPitch ? rowSizeInBytes : static_cast<u64_t>(srcRowPitch);
        const u8_t* pSrc = static_cast<const u8_t*>(pInitialData);
        u8_t* pDst = pMapped + footprint.Offset;

        for (UINT row = 0; row < numRows; ++row)
        {
            std::memcpy(
                pDst + static_cast<u64_t>(row) * footprint.Footprint.RowPitch,
                pSrc + static_cast<u64_t>(row) * srcRowPitch,
                copyBytesPerRow);
        }
        pUpload->Unmap(0, nullptr);

        m_pCommandAllocators[m_iFrameIndex]->Reset();
        m_pCommandList->Reset(m_pCommandAllocators[m_iFrameIndex].Get(), nullptr);

        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = pTexture->pResource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = pUpload.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;

        m_pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
            pTexture->pResource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            finalState);
        m_pCommandList->ResourceBarrier(1, &barrier);

        m_pCommandList->Close();

        ID3D12CommandList* ppLists[] = { m_pCommandList.Get() };
        m_pCommandQueue->ExecuteCommandLists(1, ppLists);
        WaitForGpu();

        pTexture->state = finalState;
    }

    return m_pTables->textureTable.Insert(pTexture);
}

void CDX12Device::DestroyTexture(RHITextureHandle handle)
{
    if (m_pTables)
        m_pTables->textureTable.Erase(handle);
}

void* CDX12Device::GetTextureNativeHandle(RHITextureHandle handle, eNativeHandleType type)
{
    if (!m_pTables || type != eNativeHandleType::DX12Resource)
        return nullptr;

    CDX12Texture* pTexture = m_pTables->textureTable.Lookup(handle);
    return pTexture ? pTexture->pResource.Get() : nullptr;
}

RHISamplerHandle CDX12Device::CreateSampler(const RHISamplerDesc& desc)
{
    if (!m_pTables)
        return {};

    CDX12SamplerObject* pSampler = new CDX12SamplerObject();
    pSampler->desc = desc;
    return m_pTables->samplerTable.Insert(pSampler);
}

void CDX12Device::DestroySampler(RHISamplerHandle handle)
{
    if (m_pTables)
        m_pTables->samplerTable.Erase(handle);
}

RHIPipelineHandle CDX12Device::CreatePipeline(const RHIPipelineDesc& desc)
{
    if (!m_pDevice || !m_pTables)
        return {};

    CDX12Shader* pVS = m_pTables->shaderTable.Lookup(desc.vsHandle);
    CDX12Shader* pPS = m_pTables->shaderTable.Lookup(desc.psHandle);
    if (!pVS || !pPS || pVS->stage != eRHIShaderStage::Vertex || pPS->stage != eRHIShaderStage::Pixel)
        return {};

    std::vector<D3D12_ROOT_PARAMETER> rootParameters;
    std::vector<DX12RootBinding> rootBindings;
    std::deque<std::vector<D3D12_DESCRIPTOR_RANGE>> tableRangeStorage;

    for (u32_t groupIndex = 0; groupIndex < desc.bindGroupLayoutCount; ++groupIndex)
    {
        IRHIBindGroupLayout* pLayoutBase =
            m_pTables->bindGroupLayoutTable.Lookup(desc.bindGroupLayouts[groupIndex]);
        auto* pLayout = dynamic_cast<CDX12BindGroupLayout*>(pLayoutBase);
        if (!pLayout)
            return {};

        std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges;
        u32_t srvVisibilityMask = 0;
        u32_t samplerVisibilityMask = 0;

        const RHIBindGroupLayoutDesc& layoutDesc = pLayout->GetDesc();
        for (u32_t slotIndex = 0; slotIndex < layoutDesc.slotCount; ++slotIndex)
        {
            const RHIBindingSlot& slot = layoutDesc.slots[slotIndex];

            if (slot.type == eRHIBindingType::ConstantBuffer)
            {
                D3D12_ROOT_PARAMETER parameter{};
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                parameter.ShaderVisibility = ToDX12ShaderVisibility(slot.visibility);
                parameter.Descriptor.ShaderRegister = slot.slot;
                parameter.Descriptor.RegisterSpace = groupIndex;

                DX12RootBinding binding{};
                binding.kind = eDX12RootBindingKind::RootConstantBuffer;
                binding.bindGroupSlot = groupIndex;
                binding.bindingSlot = slot.slot;
                binding.type = slot.type;
                binding.rootParameterIndex = static_cast<u32_t>(rootParameters.size());

                rootParameters.push_back(parameter);
                rootBindings.push_back(binding);
            }
            else if (slot.type == eRHIBindingType::ShaderResource)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.NumDescriptors = 1;
                range.BaseShaderRegister = slot.slot;
                range.RegisterSpace = groupIndex;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                srvRanges.push_back(range);
                srvVisibilityMask |= static_cast<u32_t>(slot.visibility);
            }
            else if (slot.type == eRHIBindingType::Sampler)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                range.NumDescriptors = 1;
                range.BaseShaderRegister = slot.slot;
                range.RegisterSpace = groupIndex;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                samplerRanges.push_back(range);
                samplerVisibilityMask |= static_cast<u32_t>(slot.visibility);
            }
        }

        if (!srvRanges.empty())
        {
            tableRangeStorage.push_back(std::move(srvRanges));

            D3D12_ROOT_PARAMETER parameter{};
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.ShaderVisibility = ToDX12TableVisibility(srvVisibilityMask);
            parameter.DescriptorTable.NumDescriptorRanges =
                static_cast<UINT>(tableRangeStorage.back().size());
            parameter.DescriptorTable.pDescriptorRanges = tableRangeStorage.back().data();

            DX12RootBinding binding{};
            binding.kind = eDX12RootBindingKind::SrvTable;
            binding.bindGroupSlot = groupIndex;
            binding.type = eRHIBindingType::ShaderResource;
            binding.rootParameterIndex = static_cast<u32_t>(rootParameters.size());

            rootParameters.push_back(parameter);
            rootBindings.push_back(binding);
        }

        if (!samplerRanges.empty())
        {
            tableRangeStorage.push_back(std::move(samplerRanges));

            D3D12_ROOT_PARAMETER parameter{};
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.ShaderVisibility = ToDX12TableVisibility(samplerVisibilityMask);
            parameter.DescriptorTable.NumDescriptorRanges =
                static_cast<UINT>(tableRangeStorage.back().size());
            parameter.DescriptorTable.pDescriptorRanges = tableRangeStorage.back().data();

            DX12RootBinding binding{};
            binding.kind = eDX12RootBindingKind::SamplerTable;
            binding.bindGroupSlot = groupIndex;
            binding.type = eRHIBindingType::Sampler;
            binding.rootParameterIndex = static_cast<u32_t>(rootParameters.size());

            rootParameters.push_back(parameter);
            rootBindings.push_back(binding);
        }
    }

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = static_cast<UINT>(rootParameters.size());
    rootDesc.pParameters = rootParameters.empty() ? nullptr : rootParameters.data();
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> pRootBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pRootErrors;
    if (FAILED(D3D12SerializeRootSignature(
        &rootDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &pRootBlob,
        &pRootErrors)))
    {
        if (pRootErrors)
            OutputDebugStringA(static_cast<const char*>(pRootErrors->GetBufferPointer()));
        return {};
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature;
    if (FAILED(m_pDevice->CreateRootSignature(
        0,
        pRootBlob->GetBufferPointer(),
        pRootBlob->GetBufferSize(),
        IID_PPV_ARGS(&pRootSignature))))
    {
        return {};
    }

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
    inputElements.reserve(desc.inputElementCount);

    for (u32_t i = 0; i < desc.inputElementCount; ++i)
    {
        const RHIInputElementDesc& src = desc.inputElements[i];

        D3D12_INPUT_ELEMENT_DESC dst{};
        dst.SemanticName = src.semanticName;
        dst.SemanticIndex = src.semanticIndex;
        dst.Format = ToDXGIFormat(src.format);
        dst.InputSlot = src.inputSlot;
        dst.AlignedByteOffset = src.alignedByteOffset;
        dst.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        dst.InstanceDataStepRate = 0;

        inputElements.push_back(dst);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = { inputElements.data(), static_cast<UINT>(inputElements.size()) };
    psoDesc.pRootSignature = pRootSignature.Get();
    psoDesc.VS = { pVS->bytecode.data(), pVS->bytecode.size() };
    psoDesc.PS = { pPS->bytecode.data(), pPS->bytecode.size() };
    psoDesc.RasterizerState = MakeRasterizerDesc(desc.cullMode);
    psoDesc.BlendState = MakeBlendDesc(desc.blendMode);
    psoDesc.DepthStencilState = MakeDepthStencilDesc(desc);
    psoDesc.SampleMask = 0xffffffffu;
    psoDesc.PrimitiveTopologyType = ToDX12TopologyType(desc.topology);
    psoDesc.NumRenderTargets = desc.rtvCount;

    for (u32_t i = 0; i < desc.rtvCount && i < 8; ++i)
        psoDesc.RTVFormats[i] = ToDXGIFormat(desc.rtvFormats[i]);

    psoDesc.DSVFormat = ToDXGIFormat(desc.dsvFormat);
    psoDesc.SampleDesc.Count = 1;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState;
    if (FAILED(m_pDevice->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&pPipelineState))))
    {
        return {};
    }

    return m_pTables->pipelineTable.Insert(new CDX12PipelineState(
        desc,
        pRootSignature,
        pPipelineState,
        ToDX12PrimitiveTopology(desc.topology),
        std::move(rootBindings)));
}

void CDX12Device::DestroyPipeline(RHIPipelineHandle handle)
{
    if (m_pTables)
        m_pTables->pipelineTable.Erase(handle);
}

RHIRenderPassHandle CDX12Device::CreateRenderPass(const RHIRenderPassDesc& desc)
{
    if (!m_pTables)
        return {};

    return m_pTables->renderPassTable.Insert(new CDX12RenderPass(desc));
}

void CDX12Device::DestroyRenderPass(RHIRenderPassHandle handle)
{
    if (m_pTables)
        m_pTables->renderPassTable.Erase(handle);
}

RHIBindGroupLayoutHandle CDX12Device::CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc)
{
    if (!m_pTables)
        return {};

    return m_pTables->bindGroupLayoutTable.Insert(new CDX12BindGroupLayout(desc));
}

void CDX12Device::DestroyBindGroupLayout(RHIBindGroupLayoutHandle handle)
{
    if (m_pTables)
        m_pTables->bindGroupLayoutTable.Erase(handle);
}

void CDX12Device::WriteBindGroupDescriptors(
    const RHIBindGroupLayoutDesc& layoutDesc,
    const RHIBindGroupDesc& groupDesc,
    D3D12_CPU_DESCRIPTOR_HANDLE srvBaseCpu,
    D3D12_CPU_DESCRIPTOR_HANDLE samplerBaseCpu)
{
    u32_t srvOffset = 0;
    u32_t samplerOffset = 0;

    for (u32_t i = 0; i < layoutDesc.slotCount; ++i)
    {
        const RHIBindingSlot& slot = layoutDesc.slots[i];

        if (slot.type == eRHIBindingType::ShaderResource)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = srvBaseCpu;
            handle.ptr += static_cast<SIZE_T>(srvOffset) * m_pHeaps->srvIncrement;
            ++srvOffset;

            const RHIBindGroupResource* pResource =
                FindBindGroupResource(groupDesc, slot.slot, eRHIBindingType::ShaderResource);
            CDX12Texture* pTexture = pResource
                ? m_pTables->textureTable.Lookup(pResource->textureHandle)
                : nullptr;

            if (pTexture && pTexture->pResource)
            {
                m_pDevice->CreateShaderResourceView(pTexture->pResource.Get(), nullptr, handle);
            }
            else
            {
                D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc{};
                nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                nullDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                nullDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                nullDesc.Texture2D.MipLevels = 1;
                m_pDevice->CreateShaderResourceView(nullptr, &nullDesc, handle);
            }
        }
        else if (slot.type == eRHIBindingType::Sampler)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerBaseCpu;
            handle.ptr += static_cast<SIZE_T>(samplerOffset) * m_pHeaps->samplerIncrement;
            ++samplerOffset;

            const RHIBindGroupResource* pResource =
                FindBindGroupResource(groupDesc, slot.slot, eRHIBindingType::Sampler);
            CDX12SamplerObject* pSampler = pResource
                ? m_pTables->samplerTable.Lookup(pResource->samplerHandle)
                : nullptr;

            D3D12_SAMPLER_DESC samplerDesc{};
            samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.MaxAnisotropy = 1;
            samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

            if (pSampler)
            {
                samplerDesc.Filter = ToDX12Filter(pSampler->desc.filter);
                samplerDesc.AddressU = ToDX12AddressMode(pSampler->desc.addressU);
                samplerDesc.AddressV = ToDX12AddressMode(pSampler->desc.addressV);
                samplerDesc.AddressW = ToDX12AddressMode(pSampler->desc.addressW);
                samplerDesc.MaxAnisotropy =
                    pSampler->desc.maxAnisotropy > 1 ? pSampler->desc.maxAnisotropy : 1;
            }

            m_pDevice->CreateSampler(&samplerDesc, handle);
        }
    }
}

RHIBindGroupHandle CDX12Device::CreateBindGroup(const RHIBindGroupDesc& desc)
{
    if (!m_pTables || !m_pHeaps)
        return {};

    IRHIBindGroupLayout* pLayoutBase = m_pTables->bindGroupLayoutTable.Lookup(desc.layoutHandle);
    auto* pLayout = dynamic_cast<CDX12BindGroupLayout*>(pLayoutBase);
    if (!pLayout)
        return {};

    const RHIBindGroupLayoutDesc& layoutDesc = pLayout->GetDesc();
    const u32_t srvCount = CountLayoutSlots(layoutDesc, eRHIBindingType::ShaderResource);
    const u32_t samplerCount = CountLayoutSlots(layoutDesc, eRHIBindingType::Sampler);

    u32_t srvBase = 0;
    u32_t samplerBase = 0;

    if (srvCount > 0 && !m_pHeaps->AllocSrv(srvCount, srvBase))
        return {};

    if (samplerCount > 0 && !m_pHeaps->AllocSampler(samplerCount, samplerBase))
    {
        if (srvCount > 0)
            m_pHeaps->FreeSrv(srvBase, srvCount);
        return {};
    }

    CDX12BindGroup* pGroup = new CDX12BindGroup(desc);
    pGroup->srvBaseIndex = srvBase;
    pGroup->srvCount = srvCount;
    pGroup->samplerBaseIndex = samplerBase;
    pGroup->samplerCount = samplerCount;

    WriteBindGroupDescriptors(
        layoutDesc,
        pGroup->GetDesc(),
        srvCount > 0 ? m_pHeaps->SrvStagingCpuAt(srvBase) : D3D12_CPU_DESCRIPTOR_HANDLE{},
        samplerCount > 0 ? m_pHeaps->SamplerStagingCpuAt(samplerBase) : D3D12_CPU_DESCRIPTOR_HANDLE{});

    return m_pTables->bindGroupTable.Insert(pGroup);
}

void CDX12Device::DestroyBindGroup(RHIBindGroupHandle handle)
{
    if (!m_pTables)
        return;

    IRHIBindGroup* pGroupBase = m_pTables->bindGroupTable.Lookup(handle);
    auto* pGroup = dynamic_cast<CDX12BindGroup*>(pGroupBase);
    if (pGroup && m_pHeaps)
    {
        if (pGroup->srvCount > 0)
            m_pHeaps->FreeSrv(pGroup->srvBaseIndex, pGroup->srvCount);
        if (pGroup->samplerCount > 0)
            m_pHeaps->FreeSampler(pGroup->samplerBaseIndex, pGroup->samplerCount);
    }

    m_pTables->bindGroupTable.Erase(handle);
}

void CDX12Device::UpdateBindGroup(
    RHIBindGroupHandle handle,
    const RHIBindGroupResource* resources,
    u32_t resourceCount)
{
    if (!m_pTables || !m_pHeaps)
        return;

    IRHIBindGroup* pGroup = m_pTables->bindGroupTable.Lookup(handle);
    auto* pDX12Group = dynamic_cast<CDX12BindGroup*>(pGroup);
    if (!pDX12Group)
        return;

    pDX12Group->UpdateResources(resources, resourceCount);

    IRHIBindGroupLayout* pLayoutBase =
        m_pTables->bindGroupLayoutTable.Lookup(pDX12Group->GetDesc().layoutHandle);
    auto* pLayout = dynamic_cast<CDX12BindGroupLayout*>(pLayoutBase);
    if (!pLayout)
        return;

    WriteBindGroupDescriptors(
        pLayout->GetDesc(),
        pDX12Group->GetDesc(),
        pDX12Group->srvCount > 0
            ? m_pHeaps->SrvStagingCpuAt(pDX12Group->srvBaseIndex)
            : D3D12_CPU_DESCRIPTOR_HANDLE{},
        pDX12Group->samplerCount > 0
            ? m_pHeaps->SamplerStagingCpuAt(pDX12Group->samplerBaseIndex)
            : D3D12_CPU_DESCRIPTOR_HANDLE{});
}

bool_t CDX12Device::Initialize(const DX12DeviceDesc& desc)
{
    m_iWidth = desc.width;
    m_iHeight = desc.height;
    m_bVSync = desc.vsync;

    if (!CreateDevice(desc))
        return false;
    if (!CreateCommandObjects())
        return false;
    if (!CreateSwapChain(desc))
        return false;
    if (!CreateRenderTargets())
        return false;
    if (!CreateDepthStencilTarget())
        return false;
    if (!CreateFence())
        return false;
    if (!CreateDescriptorHeaps())
        return false;

    OutputDebugStringA("[CDX12Device] DX12 device/swapchain ready\n");
    return true;
}

bool_t CDX12Device::CreateDescriptorHeaps()
{
    if (!m_pHeaps)
        return false;

    constexpr u32_t kSrvHeapCapacity = 1024;
    constexpr u32_t kSamplerHeapCapacity = 256;

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc{};
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.NumDescriptors = kSrvHeapCapacity;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_pDevice->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_pHeaps->pSrvHeap))))
        return false;

    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(m_pDevice->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&m_pHeaps->pSrvStagingHeap))))
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC samplerDesc{};
    samplerDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    samplerDesc.NumDescriptors = kSamplerHeapCapacity;
    samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_pDevice->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(&m_pHeaps->pSamplerHeap))))
        return false;

    samplerDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if (FAILED(m_pDevice->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(&m_pHeaps->pSamplerStagingHeap))))
        return false;

    m_pHeaps->srvIncrement =
        m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_pHeaps->samplerIncrement =
        m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    m_pHeaps->srvCapacity = kSrvHeapCapacity;
    m_pHeaps->samplerCapacity = kSamplerHeapCapacity;
    m_pHeaps->srvRingCapacity = kSrvHeapCapacity / kFrameCount;
    m_pHeaps->samplerRingCapacity = kSamplerHeapCapacity / kFrameCount;
    return true;
}

bool_t CDX12Device::CreateDevice(const DX12DeviceDesc& desc)
{
    (void)desc;

    UINT factoryFlags = 0;

#if defined(_DEBUG)
    Microsoft::WRL::ComPtr<ID3D12Debug> pDebug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug))))
    {
        pDebug->EnableDebugLayer();
        factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_pFactory))))
        return false;

    m_pAdapter = SelectHighPerformanceDX12Adapter(m_pFactory.Get());
    LogDX12Adapter(m_pAdapter.Get());

    if (FAILED(D3D12CreateDevice(
        m_pAdapter.Get(),
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&m_pDevice))))
    {
        OutputDebugStringA("[CDX12Device] D3D12CreateDevice failed\n");
        return false;
    }

    return true;
}

bool_t CDX12Device::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    if (FAILED(m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue))))
        return false;

    for (u32_t i = 0; i < kFrameCount; ++i)
    {
        if (FAILED(m_pDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_pCommandAllocators[i]))))
        {
            return false;
        }
    }

    if (FAILED(m_pDevice->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_pCommandAllocators[0].Get(),
        nullptr,
        IID_PPV_ARGS(&m_pCommandList))))
    {
        return false;
    }

    m_pCommandList->Close();
    return true;
}

bool_t CDX12Device::CreateSwapChain(const DX12DeviceDesc& desc)
{
    DXGI_SWAP_CHAIN_DESC1 swapDesc{};
    swapDesc.Width = desc.width;
    swapDesc.Height = desc.height;
    swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = kFrameCount;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.Stereo = FALSE;
    swapDesc.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain1;
    if (FAILED(m_pFactory->CreateSwapChainForHwnd(
        m_pCommandQueue.Get(),
        desc.hwnd,
        &swapDesc,
        nullptr,
        nullptr,
        &pSwapChain1)))
    {
        return false;
    }

    m_pFactory->MakeWindowAssociation(desc.hwnd, DXGI_MWA_NO_ALT_ENTER);

    if (FAILED(pSwapChain1.As(&m_pSwapChain)))
        return false;

    m_iFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();

    m_Viewport.TopLeftX = 0.f;
    m_Viewport.TopLeftY = 0.f;
    m_Viewport.Width = static_cast<f32_t>(desc.width);
    m_Viewport.Height = static_cast<f32_t>(desc.height);
    m_Viewport.MinDepth = 0.f;
    m_Viewport.MaxDepth = 1.f;

    m_ScissorRect.left = 0;
    m_ScissorRect.top = 0;
    m_ScissorRect.right = static_cast<LONG>(desc.width);
    m_ScissorRect.bottom = static_cast<LONG>(desc.height);

    return true;
}

bool_t CDX12Device::CreateRenderTargets()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = kFrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pRTVHeap))))
        return false;

    m_iRTVDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();

    for (u32_t i = 0; i < kFrameCount; ++i)
    {
        if (FAILED(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pRenderTargets[i]))))
            return false;

        m_pDevice->CreateRenderTargetView(m_pRenderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_iRTVDescriptorSize;
    }

    return true;
}

bool_t CDX12Device::CreateDepthStencilTarget()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_pDSVHeap))))
        return false;

    D3D12_RESOURCE_DESC depthDesc{};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = m_iWidth;
    depthDesc.Height = m_iHeight;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heapProps = MakeHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    if (FAILED(m_pDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_pDepthStencil))))
    {
        return false;
    }

    m_pDevice->CreateDepthStencilView(
        m_pDepthStencil.Get(),
        nullptr,
        m_pDSVHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

bool_t CDX12Device::CreateFence()
{
    if (FAILED(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence))))
        return false;

    m_hFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return m_hFenceEvent != nullptr;
}

void CDX12Device::BeginFrame(f32_t r, f32_t g, f32_t b, f32_t a)
{
    if (!m_pCommandList || !m_pCommandAllocators[m_iFrameIndex])
        return;

    WaitForFrame(m_iFrameIndex);

    m_pCommandAllocators[m_iFrameIndex]->Reset();
    m_pCommandList->Reset(m_pCommandAllocators[m_iFrameIndex].Get(), nullptr);

    if (m_pHeaps && m_pHeaps->pSrvHeap && m_pHeaps->pSamplerHeap)
    {
        m_pHeaps->ResetFrame(m_iFrameIndex);

        ID3D12DescriptorHeap* ppHeaps[] = { m_pHeaps->pSrvHeap.Get(), m_pHeaps->pSamplerHeap.Get() };
        m_pCommandList->SetDescriptorHeaps(2, ppHeaps);
    }

    D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
        m_pRenderTargets[m_iFrameIndex].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_pCommandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRTVHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += static_cast<SIZE_T>(m_iFrameIndex) * m_iRTVDescriptorSize;

    const FLOAT clearColor[4] = { r, g, b, a };
    m_pCommandList->RSSetViewports(1, &m_Viewport);
    m_pCommandList->RSSetScissorRects(1, &m_ScissorRect);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_pDSVHeap->GetCPUDescriptorHandleForHeapStart();
    m_pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    m_pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_pCommandList->ClearDepthStencilView(
        dsvHandle,
        D3D12_CLEAR_FLAG_DEPTH,
        1.0f,
        0,
        0,
        nullptr);

    m_bFrameRecording = true;
}

void CDX12Device::EndFrame()
{
    if (!m_bFrameRecording)
        return;

    D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(
        m_pRenderTargets[m_iFrameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_pCommandList->ResourceBarrier(1, &barrier);

    m_pCommandList->Close();

    ID3D12CommandList* ppLists[] = { m_pCommandList.Get() };
    m_pCommandQueue->ExecuteCommandLists(1, ppLists);

    const u32_t completedFrameIndex = m_iFrameIndex;
    const u64_t fenceValue = m_uNextFenceValue++;
    if (SUCCEEDED(m_pCommandQueue->Signal(m_pFence.Get(), fenceValue)))
        m_uFrameFenceValues[completedFrameIndex] = fenceValue;

    m_pSwapChain->Present(m_bVSync ? 1u : 0u, 0u);

    m_iFrameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
    m_bFrameRecording = false;
}

void CDX12Device::WaitForFenceValue(u64_t fenceValue)
{
    if (fenceValue == 0 || !m_pFence || !m_hFenceEvent)
        return;

    if (m_pFence->GetCompletedValue() >= fenceValue)
        return;

    m_pFence->SetEventOnCompletion(fenceValue, m_hFenceEvent);
    WaitForSingleObject(m_hFenceEvent, INFINITE);
}

void CDX12Device::WaitForFrame(u32_t frameIndex)
{
    if (frameIndex >= kFrameCount)
        return;

    WaitForFenceValue(m_uFrameFenceValues[frameIndex]);
    m_uFrameFenceValues[frameIndex] = 0;
}

void CDX12Device::WaitForGpu()
{
    if (!m_pCommandQueue || !m_pFence || !m_hFenceEvent)
        return;

    const u64_t fenceValue = m_uNextFenceValue++;
    if (FAILED(m_pCommandQueue->Signal(m_pFence.Get(), fenceValue)))
        return;

    WaitForFenceValue(fenceValue);

    for (u64_t& frameFenceValue : m_uFrameFenceValues)
        frameFenceValue = 0;
}
