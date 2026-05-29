#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include "WintersTypes.h"

// ─────────────────────────────────────────────────────────────────
//  DX11Shader  |  VS / PS 컴파일 및 바인딩
//
//  런타임 컴파일 (D3DCompileFromFile) 사용.
//  오프라인 컴파일(.cso)은 Phase 3 이후에 전환.
//
//  사용 흐름:
//    DX11Shader shader;
//    shader.Load(device, L"Shaders/Triangle.hlsl", "VS", "PS");
//    shader.Bind(context);
//    // draw call
//    shader.Unbind(context);
// ─────────────────────────────────────────────────────────────────

class DX11Shader
{
public:
    DX11Shader()  = default;
    ~DX11Shader() { Release(); }

    // .hlsl 파일에서 VS + PS 컴파일 및 생성
    // vsEntry / psEntry : 셰이더 진입 함수명 (기본 "VS" / "PS")
    bool Load(ID3D11Device* device,
              const wchar_t* hlslPath,
              const char*    vsEntry = "VS",
              const char*    psEntry = "PS");

    void Bind(ID3D11DeviceContext* context) const;
    void Unbind(ID3D11DeviceContext* context) const;
    void Release();

    // InputLayout 생성 시 VS 바이트코드가 필요 — DX11Pipeline에서 접근
    ID3DBlob* GetVSBlob() const { return m_pVSBlob; }
    ID3D11VertexShader* GetVS() const { return m_pVS; }
    ID3D11PixelShader*  GetPS() const { return m_pPS; }

private:
    static ID3DBlob* CompileShader(const wchar_t* path,
                                   const char*    entry,
                                   const char*    target);

    ID3D11VertexShader* m_pVS     = nullptr;
    ID3D11PixelShader*  m_pPS     = nullptr;
    ID3DBlob*           m_pVSBlob = nullptr;
};
