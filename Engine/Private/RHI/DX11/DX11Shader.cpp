#include "RHI/DX11/DX11Shader.h"
#include <cassert>

// ─────────────────────────────────────────────────────────────────
//  DX11Shader 구현
//
//  컴파일 플래그:
//    Debug 빌드  : D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION
//    Release 빌드: D3DCOMPILE_OPTIMIZATION_LEVEL3
//
//  VS 바이트코드(m_pVSBlob)는 Load 후에도 유지된다.
//  → DX11Pipeline이 InputLayout 생성 시 이 블롭이 필요하기 때문.
//  → Release() 호출 시 함께 해제.
// ─────────────────────────────────────────────────────────────────

bool DX11Shader::Load(ID3D11Device* device,
                      const wchar_t* hlslPath,
                      const char*    vsEntry,
                      const char*    psEntry)
{
    assert(device && hlslPath);

    // ── VS 컴파일 ───────────────────────────────────────────
    m_pVSBlob = CompileShader(hlslPath, vsEntry, "vs_5_0");
    if (!m_pVSBlob)
        return false;

    HRESULT hr = device->CreateVertexShader(
        m_pVSBlob->GetBufferPointer(),
        m_pVSBlob->GetBufferSize(),
        nullptr,
        &m_pVS);

    if (FAILED(hr))
    {
        m_pVSBlob->Release();
        m_pVSBlob = nullptr;
        return false;
    }

    // ── PS 컴파일 ───────────────────────────────────────────
    ID3DBlob* pPSBlob = CompileShader(hlslPath, psEntry, "ps_5_0");
    if (!pPSBlob)
    {
        Release();
        return false;
    }

    hr = device->CreatePixelShader(
        pPSBlob->GetBufferPointer(),
        pPSBlob->GetBufferSize(),
        nullptr,
        &m_pPS);
    pPSBlob->Release();

    if (FAILED(hr))
    {
        Release();
        return false;
    }

    return true;
}

void DX11Shader::Bind(ID3D11DeviceContext* context) const
{
    context->VSSetShader(m_pVS, nullptr, 0);
    context->PSSetShader(m_pPS, nullptr, 0);
}

void DX11Shader::Unbind(ID3D11DeviceContext* context) const
{
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
}

void DX11Shader::Release()
{
    if (m_pPS)     { m_pPS->Release();     m_pPS     = nullptr; }
    if (m_pVS)     { m_pVS->Release();     m_pVS     = nullptr; }
    if (m_pVSBlob) { m_pVSBlob->Release(); m_pVSBlob = nullptr; }
}

// ── private: 셰이더 컴파일 ─────────────────────────────────────
ID3DBlob* DX11Shader::CompileShader(const wchar_t* path,
                                     const char*    entry,
                                     const char*    target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob* pCode  = nullptr;
    ID3DBlob* pError = nullptr;

    HRESULT hr = D3DCompileFromFile(
        path,
        nullptr,        // defines
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry,
        target,
        flags,
        0,
        &pCode,
        &pError);

    if (FAILED(hr))
    {
        if (pError)
        {
            OutputDebugStringA("[DX11Shader] Shader compile error:\n");
            OutputDebugStringA(static_cast<const char*>(pError->GetBufferPointer()));
            pError->Release();
        }
        else
        {
            OutputDebugStringA("[DX11Shader] D3DCompileFromFile failed (no error blob — file not found?)\n");
        }
        __debugbreak();
        return nullptr;
    }

    if (pError)
        pError->Release();

    return pCode;
}
