#include "RHI/DX11/BlendStateCache.h"

std::unique_ptr<CBlendStateCache> CBlendStateCache::Create(ID3D11Device* pDevice)
{
    auto pInstance = std::unique_ptr<CBlendStateCache>(new CBlendStateCache());
    if (FAILED(pInstance->Initialize(pDevice)))
        return nullptr;
    return pInstance;
}

HRESULT CBlendStateCache::Initialize(ID3D11Device* pDevice)
{
    if (!pDevice) return E_INVALIDARG;

    auto fill = [](D3D11_BLEND_DESC& desc)
        {
            desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        };

    // Opaque
    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = FALSE;
        fill(desc);
        if (FAILED(pDevice->CreateBlendState(&desc,
            m_pStates[(size_t)eBlendPreset::Opaque].GetAddressOf()))) return E_FAIL;
    }
    // AlphaBlend (straight alpha)
    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        fill(desc);
        if (FAILED(pDevice->CreateBlendState(&desc,
            m_pStates[(size_t)eBlendPreset::AlphaBlend].GetAddressOf()))) return E_FAIL;
    }
    // PremultipliedAlpha
    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        fill(desc);
        if (FAILED(pDevice->CreateBlendState(&desc,
            m_pStates[(size_t)eBlendPreset::PremultipliedAlpha].GetAddressOf()))) return E_FAIL;
    }
    // Additive
    {
        D3D11_BLEND_DESC desc = {};
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        fill(desc);
        if (FAILED(pDevice->CreateBlendState(&desc,
            m_pStates[(size_t)eBlendPreset::Additive].GetAddressOf()))) return E_FAIL;
    }
    return S_OK;
}

void CBlendStateCache::Bind(ID3D11DeviceContext* pContext, eBlendPreset preset) const
{
    const f32_t blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
    pContext->OMSetBlendState(m_pStates[(size_t)preset].Get(), blendFactor, 0xFFFFFFFFu);
}

ID3D11BlendState* CBlendStateCache::Get(eBlendPreset preset) const
{
    return m_pStates[(size_t)preset].Get();
}