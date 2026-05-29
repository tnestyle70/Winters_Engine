#include "WintersPCH.h"
#include "RHI/DX11/SamplerStateCache.h"
#include <cassert>

CSamplerStateCache& CSamplerStateCache::Instance()
{
    static CSamplerStateCache inst;
    return inst;
}

static D3D11_SAMPLER_DESC MakeDesc(D3D11_FILTER filter, D3D11_TEXTURE_ADDRESS_MODE addr, UINT maxAniso = 1)
{
    D3D11_SAMPLER_DESC d = {};
    d.Filter = filter;
    d.AddressU = addr;
    d.AddressV = addr;
    d.AddressW = addr;
    d.MaxAnisotropy = maxAniso;
    d.ComparisonFunc = D3D11_COMPARISON_NEVER;
    d.MinLOD = 0;
    d.MaxLOD = D3D11_FLOAT32_MAX;
    return d;
}

bool CSamplerStateCache::Initialize(ID3D11Device* device)
{
    assert(device);

    D3D11_SAMPLER_DESC descs[static_cast<size_t>(SamplerPreset::Count)] = {
        MakeDesc(D3D11_FILTER_MIN_MAG_MIP_POINT,       D3D11_TEXTURE_ADDRESS_WRAP),
        MakeDesc(D3D11_FILTER_MIN_MAG_MIP_POINT,       D3D11_TEXTURE_ADDRESS_CLAMP),
        MakeDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR,      D3D11_TEXTURE_ADDRESS_WRAP),
        MakeDesc(D3D11_FILTER_MIN_MAG_MIP_LINEAR,      D3D11_TEXTURE_ADDRESS_CLAMP),
        MakeDesc(D3D11_FILTER_ANISOTROPIC,             D3D11_TEXTURE_ADDRESS_WRAP,  16),
        MakeDesc(D3D11_FILTER_ANISOTROPIC,             D3D11_TEXTURE_ADDRESS_CLAMP, 16),
    };

    for (size_t i = 0; i < static_cast<size_t>(SamplerPreset::Count); ++i)
    {
        if (FAILED(device->CreateSamplerState(&descs[i], &m_pStates[i])))
        {
            Shutdown();
            return false;
        }
    }
    return true;
}

void CSamplerStateCache::Shutdown()
{
    for (auto& s : m_pStates)
    {
        if (s) { s->Release(); s = nullptr; }
    }
}

ID3D11SamplerState* CSamplerStateCache::Get(SamplerPreset preset) const
{
    return m_pStates[static_cast<size_t>(preset)];
}

void CSamplerStateCache::BindAllPS(ID3D11DeviceContext* context, UINT startSlot) const
{
    context->PSSetSamplers(startSlot,
        static_cast<UINT>(SamplerPreset::Count),
        const_cast<ID3D11SamplerState* const*>(m_pStates));
}
