#include "Renderer/NormalPass.h"

#include "RHI/IRHIDevice.h"
#include "RHI/RHITypes.h"
#include "RHI/DX11/DX11Pipeline.h"
#include "RHI/DX11/DX11Shader.h"
#include "WintersPaths.h"

#include <d3d11.h>
#include <wrl/client.h>

using namespace Engine;

namespace
{
    ID3D11Device* GetNativeDX11Device(IRHIDevice* pDevice)
    {
        if (!pDevice)
            return nullptr;
        return static_cast<ID3D11Device*>(
            pDevice->GetNativeHandle(eNativeHandleType::DX11Device));
    }
}

struct CNormalPass::Impl
{
    u32_t width = 0;
    u32_t height = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> pNormalTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pNormalRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pNormalSRV;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> pDepthTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pDepthDSV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pDepthSRV;

    std::unique_ptr<DX11Shader> pStaticShader;
    std::unique_ptr<DX11Pipeline> pStaticPipeline;
    std::unique_ptr<DX11Shader> pSkinnedShader;
    std::unique_ptr<DX11Pipeline> pSkinnedPipeline;

    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pPrevRTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> pPrevDSV;
    D3D11_VIEWPORT prevViewport{};
    UINT prevViewportCount = 1;

    D3D11_VIEWPORT passViewport{};
};

CNormalPass::CNormalPass()
    : m_pImpl(std::make_unique<Impl>())
{
}

CNormalPass::~CNormalPass() = default;

std::unique_ptr<CNormalPass> CNormalPass::Create(IRHIDevice* pDevice, u32_t width, u32_t height)
{
    auto pPass = std::unique_ptr<CNormalPass>(new CNormalPass());
    if (!pPass->Initialize(pDevice, width, height))
        return nullptr;
    return pPass;
}

bool_t CNormalPass::Initialize(IRHIDevice* pDevice, u32_t width, u32_t height)
{
    ID3D11Device* pNativeDevice = GetNativeDX11Device(pDevice);
    if (!pNativeDevice || width == 0 || height == 0)
        return false;

    m_pImpl->width = width;
    m_pImpl->height = height;
    m_pImpl->passViewport.TopLeftX = 0.f;
    m_pImpl->passViewport.TopLeftY = 0.f;
    m_pImpl->passViewport.Width = static_cast<FLOAT>(width);
    m_pImpl->passViewport.Height = static_cast<FLOAT>(height);
    m_pImpl->passViewport.MinDepth = 0.f;
    m_pImpl->passViewport.MaxDepth = 1.f;

    D3D11_TEXTURE2D_DESC normalDesc = {};
    normalDesc.Width = width;
    normalDesc.Height = height;
    normalDesc.MipLevels = 1;
    normalDesc.ArraySize = 1;
    normalDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    normalDesc.SampleDesc.Count = 1;
    normalDesc.Usage = D3D11_USAGE_DEFAULT;
    normalDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(pNativeDevice->CreateTexture2D(&normalDesc, nullptr, m_pImpl->pNormalTexture.GetAddressOf())))
        return false;
    if (FAILED(pNativeDevice->CreateRenderTargetView(
        m_pImpl->pNormalTexture.Get(), nullptr, m_pImpl->pNormalRTV.GetAddressOf())))
        return false;
    if (FAILED(pNativeDevice->CreateShaderResourceView(
        m_pImpl->pNormalTexture.Get(), nullptr, m_pImpl->pNormalSRV.GetAddressOf())))
        return false;

    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(pNativeDevice->CreateTexture2D(&depthDesc, nullptr, m_pImpl->pDepthTexture.GetAddressOf())))
        return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    if (FAILED(pNativeDevice->CreateDepthStencilView(
        m_pImpl->pDepthTexture.Get(), &dsvDesc, m_pImpl->pDepthDSV.GetAddressOf())))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
    depthSrvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    depthSrvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthSrvDesc.Texture2D.MostDetailedMip = 0;
    depthSrvDesc.Texture2D.MipLevels = 1;
    if (FAILED(pNativeDevice->CreateShaderResourceView(
        m_pImpl->pDepthTexture.Get(), &depthSrvDesc, m_pImpl->pDepthSRV.GetAddressOf())))
        return false;

    wchar_t staticShaderPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/SSAO/NormalOnly.hlsl", staticShaderPath, MAX_PATH))
        return false;

    m_pImpl->pStaticShader = std::make_unique<DX11Shader>();
    if (!m_pImpl->pStaticShader->Load(pNativeDevice, staticShaderPath))
        return false;

    m_pImpl->pStaticPipeline = std::make_unique<DX11Pipeline>();
    if (!m_pImpl->pStaticPipeline->CreateMesh(pNativeDevice, m_pImpl->pStaticShader->GetVSBlob()))
        return false;

    wchar_t skinnedShaderPath[MAX_PATH] = {};
    if (!WintersResolveContentPath(L"Shaders/SSAO/SkinnedNormalOnly.hlsl", skinnedShaderPath, MAX_PATH))
        return false;

    m_pImpl->pSkinnedShader = std::make_unique<DX11Shader>();
    if (!m_pImpl->pSkinnedShader->Load(pNativeDevice, skinnedShaderPath))
        return false;

    m_pImpl->pSkinnedPipeline = std::make_unique<DX11Pipeline>();
    if (!m_pImpl->pSkinnedPipeline->CreateSkinnedMesh(pNativeDevice, m_pImpl->pSkinnedShader->GetVSBlob()))
        return false;

    return true;
}

void CNormalPass::Begin(IRHIDevice* pDevice)
{
    ID3D11DeviceContext* pContext = static_cast<ID3D11DeviceContext*>(
        pDevice ? pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext) : nullptr);
    if (!pContext)
        return;

    m_pImpl->pPrevRTV.Reset();
    m_pImpl->pPrevDSV.Reset();
    m_pImpl->prevViewportCount = 1;
    pContext->OMGetRenderTargets(
        1,
        m_pImpl->pPrevRTV.ReleaseAndGetAddressOf(),
        m_pImpl->pPrevDSV.ReleaseAndGetAddressOf());
    pContext->RSGetViewports(&m_pImpl->prevViewportCount, &m_pImpl->prevViewport);

    const float clearNormal[4] = { 0.5f, 0.5f, 1.0f, 1.0f };
    pContext->ClearRenderTargetView(m_pImpl->pNormalRTV.Get(), clearNormal);
    pContext->ClearDepthStencilView(m_pImpl->pDepthDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

    ID3D11RenderTargetView* pRTV = m_pImpl->pNormalRTV.Get();
    pContext->OMSetRenderTargets(1, &pRTV, m_pImpl->pDepthDSV.Get());
    pContext->RSSetViewports(1, &m_pImpl->passViewport);
}

void CNormalPass::End(IRHIDevice* pDevice)
{
    ID3D11DeviceContext* pContext = static_cast<ID3D11DeviceContext*>(
        pDevice ? pDevice->GetNativeHandle(eNativeHandleType::DX11DeviceContext) : nullptr);
    if (!pContext)
        return;

    ID3D11RenderTargetView* pPrevRTV = m_pImpl->pPrevRTV.Get();
    pContext->OMSetRenderTargets(1, &pPrevRTV, m_pImpl->pPrevDSV.Get());
    if (m_pImpl->prevViewportCount > 0)
        pContext->RSSetViewports(m_pImpl->prevViewportCount, &m_pImpl->prevViewport);
}

void* CNormalPass::GetDepthSRVNative() const
{
    return m_pImpl ? m_pImpl->pDepthSRV.Get() : nullptr;
}

void* CNormalPass::GetNormalSRVNative() const
{
    return m_pImpl ? m_pImpl->pNormalSRV.Get() : nullptr;
}

DX11Shader* CNormalPass::GetStaticShader() const
{
    return m_pImpl ? m_pImpl->pStaticShader.get() : nullptr;
}

DX11Pipeline* CNormalPass::GetStaticPipeline() const
{
    return m_pImpl ? m_pImpl->pStaticPipeline.get() : nullptr;
}

DX11Shader* CNormalPass::GetSkinnedShader() const
{
    return m_pImpl ? m_pImpl->pSkinnedShader.get() : nullptr;
}

DX11Pipeline* CNormalPass::GetSkinnedPipeline() const
{
    return m_pImpl ? m_pImpl->pSkinnedPipeline.get() : nullptr;
}
