#include "RHI/DX11/DX11Buffer.h"
#include <cassert>

// ─────────────────────────────────────────────────────────────────
//  DX11Buffer 구현
//
//  D3D11_USAGE_IMMUTABLE : 생성 후 CPU에서 변경 불가. GPU 최적화.
//  현재는 정적 삼각형이라 IMMUTABLE 사용.
//  동적 버퍼(애니메이션, 파티클 등)는 DYNAMIC + MAP/UNMAP 필요.
// ─────────────────────────────────────────────────────────────────

bool DX11Buffer::CreateVertex(ID3D11Device* device,
                               const void*   data,
                               uint32_t      stride,
                               uint32_t      count)
{
    assert(device && data && stride > 0 && count > 0);

    D3D11_BUFFER_DESC desc    = {};
    desc.ByteWidth            = stride * count;
    desc.Usage                = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags            = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags       = 0;
    desc.MiscFlags            = 0;
    desc.StructureByteStride  = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem                = data;

    HRESULT hr = device->CreateBuffer(&desc, &initData, &m_pBuffer);
    if (FAILED(hr))
        return false;

    m_VertexCount = count;
    return true;
}

void DX11Buffer::BindVertex(ID3D11DeviceContext* context,
                             uint32_t             stride,
                             uint32_t             slot) const
{
    assert(context && m_pBuffer);

    UINT offset = 0;
    context->IASetVertexBuffers(slot, 1, &m_pBuffer, &stride, &offset);
}

// ── IndexBuffer ────────────────────────────────────────────────────
bool DX11Buffer::CreateIndex(ID3D11Device* device,
                              const void*   data,
                              uint32_t      count,
                              bool          use32)
{
    assert(device && data && count > 0);

    m_IndexFormat = use32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    uint32_t elementSize = use32 ? sizeof(uint32_t) : sizeof(uint16_t);

    D3D11_BUFFER_DESC desc    = {};
    desc.ByteWidth            = elementSize * count;
    desc.Usage                = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags            = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags       = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem                = data;

    HRESULT hr = device->CreateBuffer(&desc, &initData, &m_pIndexBuffer);
    if (FAILED(hr))
        return false;

    m_IndexCount = count;
    return true;
}

void DX11Buffer::BindIndex(ID3D11DeviceContext* context) const
{
    assert(context && m_pIndexBuffer);
    context->IASetIndexBuffer(m_pIndexBuffer, m_IndexFormat, 0);
}

void DX11Buffer::DrawIndexed(ID3D11DeviceContext* context) const
{
    assert(context);
    if (m_pIndexBuffer)
        context->DrawIndexed(m_IndexCount, 0, 0);
    else
        context->Draw(m_VertexCount, 0);
}

void DX11Buffer::DrawIndexedRange(
    ID3D11DeviceContext* context,
    uint32_t startIndex,
    uint32_t indexCount) const
{
    assert(context);
    if (indexCount == 0)
        return;

    if (m_pIndexBuffer)
        context->DrawIndexed(indexCount, startIndex, 0);
    else
        context->Draw(indexCount, startIndex);
}

void DX11Buffer::Release()
{
    if (m_pIndexBuffer)
    {
        m_pIndexBuffer->Release();
        m_pIndexBuffer = nullptr;
    }
    if (m_pBuffer)
    {
        m_pBuffer->Release();
        m_pBuffer = nullptr;
    }
    m_VertexCount = 0;
    m_IndexCount  = 0;
}
