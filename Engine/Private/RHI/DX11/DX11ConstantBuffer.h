#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>

#include "Renderer/FxShaderConstants.h"

#include <cassert>
#include <cstring>

template<typename T>
class DX11ConstantBuffer
{
    static_assert(sizeof(T) % 16 == 0, "Constant buffer size must be 16-byte aligned");

public:
    DX11ConstantBuffer() = default;
    ~DX11ConstantBuffer() { Release(); }

    DX11ConstantBuffer(const DX11ConstantBuffer&) = delete;
    DX11ConstantBuffer& operator=(const DX11ConstantBuffer&) = delete;

    DX11ConstantBuffer(DX11ConstantBuffer&& other) noexcept
        : m_pBuffer(other.m_pBuffer)
    {
        other.m_pBuffer = nullptr;
    }

    DX11ConstantBuffer& operator=(DX11ConstantBuffer&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_pBuffer = other.m_pBuffer;
            other.m_pBuffer = nullptr;
        }
        return *this;
    }

    [[nodiscard]] bool Create(ID3D11Device* device)
    {
        assert(device);

        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = sizeof(T);
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &m_pBuffer));
    }

    void Update(ID3D11DeviceContext* context, const T& data)
    {
        assert(context && m_pBuffer);

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        const HRESULT hr = context->Map(m_pBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr))
        {
            std::memcpy(mapped.pData, &data, sizeof(T));
            context->Unmap(m_pBuffer, 0);
        }
    }

    void BindVS(ID3D11DeviceContext* context, UINT slot) const
    {
        assert(context && m_pBuffer);
        context->VSSetConstantBuffers(slot, 1, &m_pBuffer);
    }

    void BindPS(ID3D11DeviceContext* context, UINT slot) const
    {
        assert(context && m_pBuffer);
        context->PSSetConstantBuffers(slot, 1, &m_pBuffer);
    }

    void Bind(ID3D11DeviceContext* context, UINT slot) const
    {
        BindVS(context, slot);
        BindPS(context, slot);
    }

    void Release()
    {
        if (m_pBuffer)
        {
            m_pBuffer->Release();
            m_pBuffer = nullptr;
        }
    }

private:
    ID3D11Buffer* m_pBuffer = nullptr;
};
