#include "RHI/DX11/DX11Pipeline.h"
#include "Core/Profiler/RenderFrameStats.h"
#include <d3dcompiler.h>
#include <cassert>

// ─────────────────────────────────────────────────────────────────
//  DX11Pipeline 구현
//
//  Create()   : PosColor 레이아웃 (Triangle.hlsl — NDC 삼각형)
//  Create3D() : PosNormCol 레이아웃 (Default3D.hlsl — 3D 메시)
// ─────────────────────────────────────────────────────────────────

bool DX11Pipeline::CreateInternal(ID3D11Device* device, ID3DBlob* vsBlob,
                                   const D3D11_INPUT_ELEMENT_DESC* layout, UINT count,
                                   D3D11_CULL_MODE cullMode)
{
    assert(device && vsBlob);

    HRESULT hr = device->CreateInputLayout(
        layout, count,
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &m_pInputLayout);
    if (FAILED(hr))
        return false;

    // ── RasterizerState ───────────────────────────────────────
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode              = D3D11_FILL_SOLID;
    rsDesc.CullMode              = cullMode;
    rsDesc.FrontCounterClockwise = FALSE;
    rsDesc.DepthClipEnable       = TRUE;

    hr = device->CreateRasterizerState(&rsDesc, &m_pRasterState);
    if (FAILED(hr))
    {
        Release();
        return false;
    }

    // ── DepthStencilState ─────────────────────────────────────
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = TRUE;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc      = D3D11_COMPARISON_LESS;

    hr = device->CreateDepthStencilState(&dsDesc, &m_pDepthState);

    if (FAILED(hr))
    {
        Release();
        return false;
    }

    return true;
}

bool DX11Pipeline::Create(ID3D11Device* device, ID3DBlob* vsBlob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    return CreateInternal(device, vsBlob, layout, 2, D3D11_CULL_NONE);
}

bool DX11Pipeline::Create3D(ID3D11Device* device, ID3DBlob* vsBlob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    return CreateInternal(device, vsBlob, layout, 3, D3D11_CULL_BACK);
}

bool DX11Pipeline::CreateMesh(ID3D11Device* device, ID3DBlob* vsBlob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    return CreateInternal(device, vsBlob, layout, 4, D3D11_CULL_NONE);
}

bool DX11Pipeline::CreateSkinnedMesh(ID3D11Device* device, ID3DBlob* vsBlob)
{
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    return CreateInternal(device, vsBlob, layout, 6, D3D11_CULL_BACK);
}

void DX11Pipeline::Bind(ID3D11DeviceContext* context) const
{
    RenderFrameStats::AddBindPipeline();
    context->IASetInputLayout(m_pInputLayout);
    context->RSSetState(m_pRasterState);
    if (m_pDepthState)
        context->OMSetDepthStencilState(m_pDepthState, 0);
}

void DX11Pipeline::Release()
{
    if (m_pDepthState)  { m_pDepthState->Release();  m_pDepthState  = nullptr; }
    if (m_pRasterState) { m_pRasterState->Release(); m_pRasterState = nullptr; }
    if (m_pInputLayout) { m_pInputLayout->Release(); m_pInputLayout = nullptr; }
}
