#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  DX11Pipeline  |  InputLayout + RasterizerState 관리
//
//  레이아웃 종류:
//    PosColor   : { POSITION(float3), COLOR(float4) }          — Triangle
//    PosNormCol : { POSITION(float3), NORMAL(float3), COLOR(float4) } — 3D 메시
//
//  사용 흐름:
//    DX11Pipeline pipe;
//    pipe.Create(device, shader.GetVSBlob());          // PosColor (기본)
//    pipe.Create3D(device, shader.GetVSBlob());        // PosNormCol
//    pipe.Bind(context);
// ─────────────────────────────────────────────────────────────────

class DX11Pipeline
{
public:
    DX11Pipeline()  = default;
    ~DX11Pipeline() { Release(); }

    // PosColor 레이아웃 (Triangle.hlsl)
    bool Create(ID3D11Device* device, ID3DBlob* vsBlob);

    // PosNormCol 레이아웃 (Default3D.hlsl)
    bool Create3D(ID3D11Device* device, ID3DBlob* vsBlob);
    //정적 메시 레이아웃 mesh3d.hlsl
    //position normal texcoord tangent
    bool CreateMesh(ID3D11Device* device, ID3DBlob* vsBlob);
    //스키닝 메시 레이아웃(skinned 3d hlsl)
    //position + normal + texcoord + tangent + blendindices + blendweight
    bool CreateSkinnedMesh(ID3D11Device* device, ID3DBlob* vsBlob);

    void Bind(ID3D11DeviceContext* context) const;
    void Release();

private:
    bool CreateInternal(ID3D11Device* device, ID3DBlob* vsBlob,
                        const D3D11_INPUT_ELEMENT_DESC* layout, UINT count,
                        D3D11_CULL_MODE cullMode);

    ID3D11InputLayout*     m_pInputLayout   = nullptr;
    ID3D11RasterizerState* m_pRasterState   = nullptr;
    ID3D11DepthStencilState* m_pDepthState  = nullptr;
};
