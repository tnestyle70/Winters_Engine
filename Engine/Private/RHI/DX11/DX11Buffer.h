#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <cstdint>
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  DX11Buffer  |  VertexBuffer / IndexBuffer 생성 및 바인딩
//
//  VertexBuffer + IndexBuffer 생성 및 바인딩.
//
//  사용 흐름:
//    DX11Buffer vb;
//    vb.CreateVertex(device, vertices, sizeof(Vertex), 24);
//    vb.CreateIndex(device, indices, 36);
//    vb.BindVertex(context, sizeof(Vertex));
//    vb.BindIndex(context);
//    vb.DrawIndexed(context);
// ─────────────────────────────────────────────────────────────────

class DX11Buffer
{
public:
    DX11Buffer()  = default;
    ~DX11Buffer() { Release(); }

    // ── VertexBuffer ──────────────────────────────────────────
    bool CreateVertex(ID3D11Device* device,
                      const void*   data,
                      uint32_t      stride,
                      uint32_t      count);

    void BindVertex(ID3D11DeviceContext* context,
                    uint32_t             stride,
                    uint32_t             slot = 0) const;

    // ── IndexBuffer ───────────────────────────────────────────
    // data   : CPU 쪽 인덱스 배열 (uint16 또는 uint32)
    // count  : 인덱스 개수
    // use32  : true면 R32_UINT, false면 R16_UINT (기본)
    bool CreateIndex(ID3D11Device* device,
                     const void*   data,
                     uint32_t      count,
                     bool          use32 = false);

    void BindIndex(ID3D11DeviceContext* context) const;

    // ── Draw ──────────────────────────────────────────────────
    void DrawIndexed(ID3D11DeviceContext* context) const;
    void DrawIndexedRange(ID3D11DeviceContext* context,
                          uint32_t startIndex,
                          uint32_t indexCount) const;

    void Release();

    uint32_t GetVertexCount() const { return m_VertexCount; }
    uint32_t GetIndexCount()  const { return m_IndexCount;  }
    bool     HasIndexBuffer() const { return m_pIndexBuffer != nullptr; }

private:
    ID3D11Buffer* m_pBuffer      = nullptr;
    ID3D11Buffer* m_pIndexBuffer = nullptr;
    uint32_t      m_VertexCount  = 0;
    uint32_t      m_IndexCount   = 0;
    DXGI_FORMAT   m_IndexFormat  = DXGI_FORMAT_R16_UINT;
};
