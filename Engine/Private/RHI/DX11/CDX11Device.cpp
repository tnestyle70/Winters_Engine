#include "WintersPCH.h"
#include "RHI/DX11/CDX11Device.h"
#include "WintersCore.h"

#include <cstdio>
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

    private:
        RHIPipelineDesc m_Desc{};
        std::vector<RHIInputElementDesc> m_InputElements;
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
}

RHIPipelineHandle CDX11Device::CreatePipeline(const RHIPipelineDesc& desc)
{
    return m_PipelineTable.Insert(new CDX11PipelineState(desc));
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
