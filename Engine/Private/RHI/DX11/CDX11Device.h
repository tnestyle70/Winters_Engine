#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>   // Microsoft::WRL::ComPtr — SDK 배포 시 Client 가 PCH 없이 이 헤더를 직접 파싱하므로 명시 필요
#include "RHI/CRHIResourceTable.h"
#include "RHI/IRHIBindGroup.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHIPipelineState.h"
#include "RHI/IRHIRenderPass.h"
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  CDX11Device  |  DirectX 11 디바이스 + 스왑체인 관리
//
//  내부(Engine/Header)에만 존재. Client에 노출되지 않음.
//  ID3D11Device*, IDXGISwapChain* 등 DX 타입이 Client에 노출되지 않는다.
//
//  RHI 추상화 방향:
//    현재: CDX11Device (직접 DX11 사용)
//    향후: IRHIDevice 인터페이스 → CDX11Device / CDX12Device
//          Client는 IRHIDevice*만 알고, DX11/12 구분 불필요
//
//  포함 리소스:
//    - ID3D11Device + ID3D11DeviceContext
//    - IDXGISwapChain (FLIP_DISCARD, 더블 버퍼링)
//    - Render Target View (백버퍼)
//    - Depth Stencil Buffer + View
// ─────────────────────────────────────────────────────────────────

struct DeviceDesc
{
    HWND    hwnd       = nullptr;
    uint32  width      = 1280;
    uint32  height     = 720;
    bool    vsync      = true;
    bool    fullscreen = false;
};

class CDX11Device : public IRHIDevice
{
public:
    virtual ~CDX11Device() = default;
    
    static unique_ptr<CDX11Device> Create(const DeviceDesc& desc);

    void    BeginFrame(f32_t r = 0.f, f32_t g = 1.f,
                       f32_t b = 1.f, f32_t a = 1.f) override;
    void    EndFrame() override;

    // ── 접근자 ───────────────────────────────────────────────
    // 렌더러 내부에서만 사용. Client에는 노출 안 됨.
    ID3D11Device*           GetDevice()    const { return m_pDevice.Get();          }
    ID3D11DeviceContext*    GetContext()   const { return m_pContext.Get();         }
    ID3D11RenderTargetView* GetBackRTV()   const { return m_pRenderTargetView.Get();}
    ID3D11DepthStencilView* GetDSV()       const { return m_pDepthStencilView.Get();}
    eRHIBackend             GetBackend() const override { return eRHIBackend::DX11; }
    void* GetNativeHandle(eNativeHandleType type) const override
    {
        switch (type)
        {
        case eNativeHandleType::DX11Device:
            return m_pDevice.Get();
        case eNativeHandleType::DX11DeviceContext:
            return m_pContext.Get();
        case eNativeHandleType::DX11SwapChain:
            return m_pSwapChain.Get();
        default:
            return nullptr;
        }
    }

    RHIPipelineHandle CreatePipeline(const RHIPipelineDesc& desc) override;
    void DestroyPipeline(RHIPipelineHandle handle) override;

    RHIRenderPassHandle CreateRenderPass(const RHIRenderPassDesc& desc) override;
    void DestroyRenderPass(RHIRenderPassHandle handle) override;

    RHIBindGroupLayoutHandle CreateBindGroupLayout(const RHIBindGroupLayoutDesc& desc) override;
    void DestroyBindGroupLayout(RHIBindGroupLayoutHandle handle) override;

    RHIBindGroupHandle CreateBindGroup(const RHIBindGroupDesc& desc) override;
    void DestroyBindGroup(RHIBindGroupHandle handle) override;

    void UpdateBindGroup(RHIBindGroupHandle handle,
                         const RHIBindGroupResource* resources,
                         u32_t resourceCount) override;

private:
    CDX11Device() = default;

    bool Initialize(const DeviceDesc& desc);
    bool    CreateDeviceAndSwapChain(const DeviceDesc& desc);
    bool    CreateRenderTarget();
    bool    CreateDepthStencil(uint32 width, uint32 height);

    Microsoft::WRL::ComPtr<ID3D11Device>             m_pDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>      m_pContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain>           m_pSwapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_pRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pDepthStencilBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_pDepthStencilView;
    
    D3D11_VIEWPORT  m_Viewport  = {};
    bool            m_bVSync    = true;
    uint32          m_Width     = 1280;
    uint32          m_Height    = 720;

    CRHIResourceTable<IRHIPipelineState, RHIPipelineTag> m_PipelineTable;
    CRHIResourceTable<IRHIRenderPass, RHIRenderPassTag> m_RenderPassTable;
    CRHIResourceTable<IRHIBindGroupLayout, RHIBindGroupLayoutTag> m_BindGroupLayoutTable;
    CRHIResourceTable<IRHIBindGroup, RHIBindGroupTag> m_BindGroupTable;
};
